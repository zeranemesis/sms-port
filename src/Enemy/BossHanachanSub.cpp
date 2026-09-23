#include <Enemy/BossHanachan.hpp>
#include <Camera/cameralib.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <System/MarDirector.hpp>
#include <JSystem/JMath.hpp>
#include <dolphin/mtx.h>

f32 BHSCalcCentrifugalForce(const JGeometry::TVec3<f32>& param_1,
                            const JGeometry::TVec3<f32>& param_2,
                            const JGeometry::TVec3<f32>& param_3, f32 param_4)
{
	JGeometry::TVec3<f32> d1(param_1.x - param_2.x, 0.0f,
	                         param_1.z - param_2.z);
	f32 dist = d1.x * d1.x + d1.z * d1.z;
	if (dist < 0.001f)
		return 0.0f;

	JGeometry::TVec3<f32> d2(param_2.x - param_3.x, 0.0f,
	                         param_2.z - param_3.z);
	if (d2.x * d2.x + d2.z * d2.z < 0.001f)
		return 0.0f;

	s16 rot1 = CLBDegToShortAngle(MsGetRotFromZaxisY(d1));
	s16 rot2 = CLBDegToShortAngle(MsGetRotFromZaxisY(d2));
	if (rot1 == rot2)
		return 0.0f;

	int diff  = rot1 - rot2;
	f32 ratio = CLBAbs<int>((s16)diff) * (1.0f / 32768.0f);
	if (ratio >= 0.5f)
		return 0.0f;

	f32 force = dist * ratio;
	if (diff < 0)
		force = -force;

	force *= JMASCos(rot1 - CLBRoundf<s16>(param_4 * (65536.0f / 360.0f)));
	return force;
}

// TODO: sin/cos end up in swapped registers and the frame is 0x20 too small,
// this is probably a rotation inline applied to (dist, 0, 0)
void BHSCalcRevisionDistXZByRotateZ(f32 param_1, f32 param_2, f32 param_3,
                                    f32* param_4, f32* param_5)
{
	f32 dist  = param_3 * param_2;
	s16 angle = CLBRoundf<s16>(param_1 * (65536.0f / 360.0f));
	f32 sin   = JMASSin(angle);
	f32 cos   = JMASCos(angle);
	f32 zero  = 0.0f;
	*param_4  = dist * cos + zero * sin;
	*param_5  = -dist * sin + zero * cos;
}

void TWaterHitActor::onWaterHitCounter() { mWaterHitCounter = 0x3C; }

BOOL TWaterHitActor::receiveMessage(THitActor* sender, u32 message)
{
	BOOL result = false;
	if (message == 0xF) {
		if (gpMarDirector->isThing())
			mWaterHitCounter = 0;
		else
			mWaterHitCounter = 0x3C;
		result = true;
	}
	return result;
}

TSphereLink::TSphereLink(u16 point_num, const JGeometry::TVec3<f32>& pos,
                         f32 length, f32 radius, f32 param_5, f32 param_6,
                         f32 param_7, f32 rot_y)
{
	mPointNum = point_num;
	mPoints   = new TSpherePoint[point_num];
	unk8      = param_5;
	unkC      = param_6;
	unk10     = radius;
	unk14     = param_7;
	unk18     = rot_y;
	f32 dx = MsSin(unk18) * length;
	f32 dz = MsCos(unk18) * length;
	for (int i = 0; i < mPointNum; ++i) {
		TSpherePoint& point = mPoints[i];
		JGeometry::TVec3<f32> v;
		if (i == 0) {
			v = pos;
		} else {
			v = mPoints[i - 1].unkC;
			v.x -= dx;
			v.z -= dz;
		}
		point.unkC = v;
		point.unk0 = point.unkC;
		point.unk18.set(0.0f, 0.0f, 0.0f);
		point.unk24 = length;
		point.unk28 = 0.0f;
	}
}

void TSphereLink::execMapCollision_(JGeometry::TVec3<f32>* pos)
{
	gpMap->isTouchedOneWallAndMoveXZ(&pos->x, pos->y, &pos->z, unk10);
	const TBGCheckData* ground;
	f32 groundY = gpMap->checkGroundIgnoreWaterSurface(pos->x, pos->y + unk10,
	                                                   pos->z, &ground);
	if (ground != nullptr && ground->isLegal() && pos->y < groundY)
		pos->y = groundY;
}

void TSphereLink::moveHead(const JGeometry::TVec3<f32>& pos)
{
	for (int i = 0; i < mPointNum; ++i) {
		mPoints[i].unkC.y += unkC;
		mPoints[i].unkC += mPoints[i].unk18;
	}

	mPoints[0].unkC = pos;
	execMapCollision_(&mPoints[0].unkC);

	for (int i = 1; i < mPointNum; ++i) {
		TSpherePoint& cur  = mPoints[i];
		TSpherePoint& prev = mPoints[i - 1];

		JGeometry::TVec3<f32> dir = cur.unkC - prev.unkC;
		if (dir.isZero())
			dir.set(0.0f, 1.0f, 0.0f);
		else
			VECNormalize(&dir, &dir);

		dir.scale(cur.unk24);
		cur.unkC = prev.unkC + dir;
		execMapCollision_(&cur.unkC);
	}

	for (int i = 0; i < mPointNum; ++i) {
		mPoints[i].unk18 = (mPoints[i].unkC - mPoints[i].unk0) * unk8;
		mPoints[i].unk0  = mPoints[i].unkC;
	}
}

BOOL TSphereLink::setDegreeZAndRevisionPosXZ(int index, f32 degree)
{
	BOOL changed        = false;
	TSpherePoint& point = mPoints[index];
	f32 oldDegree       = point.unk28;
	if (oldDegree != degree) {
		point.unk28 = degree;
		changed     = true;

		f32 rotY;
		if (index == 0) {
			rotY = unk18;
		} else {
			JGeometry::TVec3<f32> dir = mPoints[index - 1].unkC - point.unkC;
			rotY = MsWrap(MsGetRotFromZaxisY(dir), 0.0f, 360.0f);
		}

		f32 dist  = unk14 * (degree - oldDegree);
		s16 angle = CLBRoundf<s16>(rotY * (65536.0f / 360.0f));
		f32 sin   = JMASSin(angle);
		f32 cos   = JMASCos(angle);
		f32 zero  = 0.0f;
		point.unkC.x += dist * cos + zero * sin;
		point.unkC.z += -dist * sin + zero * cos;
	}
	return changed;
}
