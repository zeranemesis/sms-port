#include "MoveBG/MapObjCorona.hpp"
#include "MoveBG/MapObjBase.hpp"
#include <math.h>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JMath.hpp>
#include <M3DUtil/MActor.hpp>
#include <Camera/CameraShake.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Player/MarioAccess.hpp>
#include <System/Particles.hpp>

extern "C" u8 allowsLaunch__6TKoopaCFv(void*);
extern "C" void getDown__6TKoopaFv(void*);
extern "C" void stagger__6TKoopaFb(void*, bool);
extern "C" void* __ct__14TBathtubParamsFv(void*);


static bool bathtubKoopaAllowsLaunch()
{
	return allowsLaunch__6TKoopaCFv(JDrama::TNameRefGen::search("クッパ")) != 0;
}

void TBathtub::loadAfter()
{
	SMS_LoadParticle("/scene/map/map/ms_lkp_yuge1.jpa", 0x1BE);
	SMS_LoadParticle("/scene/map/map/ms_kp_funsui.jpa", 0x1BF);
	SMS_LoadParticle("/scene/map/map/ms_kp_break_a.jpa", 0x0F6);
	SMS_LoadParticle("/scene/map/map/ms_kp_break_b.jpa", 0x0F7);
}

void TBathtub::hipdrop(const JGeometry::TVec3<f32>& position)
{
	if (reinterpret_cast<const u8*>(this)[0x299] != 0)
		return;

	u8* params = reinterpret_cast<u8*>(unk16C);
	if (unk250 > *reinterpret_cast<int*>(params + 0x7C))
		return;

	f32 dx = position.x - *reinterpret_cast<f32*>(reinterpret_cast<u8*>(this) + 0x10C);
	f32 dz = position.z - *reinterpret_cast<f32*>(reinterpret_cast<u8*>(this) + 0x114);
	f32 distance = dz * dz + (dx * dx + 0.0f);
	if (distance > 0.0000038146973f)
		JGeometry::TUtil<f32>::inv_sqrt(distance);

	unk250 = *reinterpret_cast<int*>(params + 0x7C);
	unk258 = *reinterpret_cast<int*>(params + 0x90);
	unk25C = *reinterpret_cast<int*>(params + 0x90);
	unk254 = *reinterpret_cast<u32*>(params + 0x7C);
	stagger__6TKoopaFb(JDrama::TNameRefGen::search("クッパ"), false);
}

void TBathtub::quake(const JGeometry::TVec3<f32>& position)
{
	if (reinterpret_cast<const u8*>(this)[0x299] != 0)
		return;

	f32 dx = position.x - *reinterpret_cast<f32*>(reinterpret_cast<u8*>(this) + 0x10C);
	f32 dz = position.z - *reinterpret_cast<f32*>(reinterpret_cast<u8*>(this) + 0x114);
	f32 distance = dz * dz + (dx * dx + 0.0f);
	if (distance > 0.0000038146973f) {
		JGeometry::TUtil<f32>::inv_sqrt(distance);
	}

	unk24C = 300;
	u8* params = reinterpret_cast<u8*>(unk16C);
	unk250 = *reinterpret_cast<int*>(params + 0x54);
	unk258 = *reinterpret_cast<int*>(params + 0x68);
	unk25C = *reinterpret_cast<int*>(params + 0x68);
	unk254 = *reinterpret_cast<u32*>(params + 0x7C);
	unk248 = *reinterpret_cast<int*>(params + 0xF4);

	gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x25), 1.0f);
	gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x26), 1.0f);
	SMSRumbleMgr->start(4, static_cast<f32*>(nullptr));
	JGeometry::TVec3<f32> velocity(0.0f, 1.0f, 0.0f);
	SMS_ThrowMario(velocity, 10.0f);
	getDown__6TKoopaFv(JDrama::TNameRefGen::search("クッパ"));
}

int TBathtub::getNumGripsDead() const
{
	int count = 0;
	if (reinterpret_cast<const u8*>(unk168[0])[0x249] == 0)
		count = 1;
	if (reinterpret_cast<const u8*>(unk168[1])[0x249] == 0)
		count += 1;
	if (reinterpret_cast<const u8*>(unk168[2])[0x249] == 0)
		count += 1;
	if (reinterpret_cast<const u8*>(unk168[3])[0x249] == 0)
		count += 1;
	if (reinterpret_cast<const u8*>(unk168[4])[0x249] == 0)
		count += 1;
	return count;
}

void TBathtub::tumble(f32 angle, f32 force)
{
	if (unk29A == 0) {
		f32 amount = force * 0.0001f;
		s32 index = static_cast<u16>(static_cast<s32>(182.04445f * angle)) >> jmaSinShift;
		unk1E8 += amount * jmaCosTable[index];
		*reinterpret_cast<volatile f32*>(&unk1EC) += 0.0f;
		unk1F0 += amount * -jmaSinTable[index];
	}
}

MtxPtr TBathtub::getTakingMtx()
{
	return mMActor->getModel()->getAnmMtx(mMarioJntIdx);
}

MtxPtr TBathtub::getSubmarineMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mSubmarineJntIdx);
}

MtxPtr TBathtub::getPeachMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mDuckJntIdx);
}

MtxPtr TBathtub::getKoopaJrMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mJuniorJntIdx);
}

BOOL TBathtub::receiveMessage(THitActor*, u32 message)
{
	switch (message) {
	case 1:
		hipdrop(*gpMarioPos);
		return true;
	case 3:
		hipdrop(*gpMarioPos);
		return true;
	case 0: {
		if (reinterpret_cast<const u8*>(this)[0x29A] == 0) {
			const u8* params = reinterpret_cast<const u8*>(unk16C);
			int timer = *reinterpret_cast<const int*>(params + 0x2C);
			if (unk250 <= timer) {
				unk250 = timer;
				unk258 = *reinterpret_cast<const int*>(params + 0x40);
				unk25C = *reinterpret_cast<const int*>(params + 0x40);
				unk254 = *reinterpret_cast<const u32*>(params + 0x7C);
			}
		}
		return true;
	}
	case 2:
	default:
		return false;
	}
}

Mtx* TBathtub::getRootJointMtx() const
{
    if (reinterpret_cast<const u8*>(this)[0x29A] != 0)
        return *reinterpret_cast<Mtx**>(reinterpret_cast<u8*>(getModel()) + 0x58);
    return reinterpret_cast<Mtx*>(reinterpret_cast<u8*>(getModel()) + 0x20);
}

void TBathtub::perform(u32 cue, JDrama::TGraphics* graphics) { }

void TBathtub::control() { }

void TBathtub::calcBathtubData() { }

void TBathtub::setupCollisions_() { }

void TBathtub::startDemo() { }

bool TBathtub::allowsTumble() const
{
	f32 gripY = 0.0f;
	if (gpMarioPos == nullptr || !getNearGrip(*gpMarioPos, 18.0f, &gripY))
		return false;

	const u8* self = reinterpret_cast<const u8*>(this);
	f32 dx = gpMarioPos->x - *reinterpret_cast<const f32*>(self + 0x170);
	f32 dy = gpMarioPos->y - *reinterpret_cast<const f32*>(self + 0x174);
	f32 dz = gpMarioPos->z - *reinterpret_cast<const f32*>(self + 0x178);
	f32 x = *reinterpret_cast<const f32*>(self + 0x188) * dx
	       + *reinterpret_cast<const f32*>(self + 0x18C) * dy
	       + *reinterpret_cast<const f32*>(self + 0x190) * dz;
	f32 y = *reinterpret_cast<const f32*>(self + 0x194) * dx
	       + *reinterpret_cast<const f32*>(self + 0x198) * dy
	       + *reinterpret_cast<const f32*>(self + 0x19C) * dz;
	f32 z = *reinterpret_cast<const f32*>(self + 0x1A0) * dx
	       + *reinterpret_cast<const f32*>(self + 0x1A4) * dy
	       + *reinterpret_cast<const f32*>(self + 0x1A8) * dz;
	f32 magnitude = JGeometry::TUtil<f32>::sqrt(x * x + y * y + z * z);
	if (magnitude < 4200.0f || magnitude > 4700.0f)
		return false;
	return true;
}

void TBathtub::calcRootMatrix()
{
	MtxPtr matrix = getModel()->getBaseTRMtx();
	if (reinterpret_cast<const u8*>(this)[0x299] != 0) {
		MsMtxSetRotRPH(matrix, 0.0f, mRotation.y, 0.0f);
		matrix[0][3] = mPosition.x;
		matrix[1][3] = mPosition.y;
		matrix[2][3] = mPosition.z;
		return;
	}

	const f32 x = unk1D8;
	const f32 y = unk1DC;
	const f32 z = unk1E0;
	const f32 w = unk1E4;
	const f32 xx = x + x;
	const f32 yy = y + y;
	const f32 zz = z + z;
	const f32 wx = w * xx;
	const f32 wy = w * yy;
	const f32 wz = w * zz;
	const f32 xx2 = x * xx;
	const f32 xy = x * yy;
	const f32 xz = x * zz;
	const f32 yy2 = y * yy;
	const f32 yz = y * zz;
	const f32 zz2 = z * zz;
	matrix[0][0] = 1.0f - (yy2 + zz2);
	matrix[0][1] = xy - wz;
	matrix[0][2] = xz + wy;
	matrix[1][0] = xy + wz;
	matrix[1][1] = 1.0f - (xx2 + zz2);
	matrix[1][2] = yz - wx;
	matrix[2][0] = xz - wy;
	matrix[2][1] = yz + wx;
	matrix[2][2] = 1.0f - (xx2 + yy2);
	matrix[0][3] = mPosition.x;
	matrix[1][3] = mPosition.y;
	matrix[2][3] = mPosition.z;
}

#pragma dont_inline on
bool TBathtub::getNearGrip(const JGeometry::TVec3<f32>& position, f32 radius,
                           f32* gripAngle) const
{
	Mtx* matrix = getRootJointMtx();
	f32 dx = position.x - (*matrix)[0][3];
	f32 dy = position.y - (*matrix)[1][3];
	f32 dz = position.z - (*matrix)[2][3];
	f32 localX = (*matrix)[0][0] * dx + (*matrix)[1][0] * dy + (*matrix)[2][0] * dz;
	f32 localZ = (*matrix)[0][2] * dx + (*matrix)[1][2] * dy + (*matrix)[2][2] * dz;
	f32 angle = 0.005493164f * static_cast<f32>(matan(localZ, localX));
	f32 nearest = 180.0f;
	int nearestIndex = 0;
	for (int i = 0; i < 5; ++i) {
		f32 wrapped = static_cast<f32>(fmod(360.0f + (unk150[i] - angle + 180.0f), 360.0f));
		f32 distance = static_cast<f32>(fabs(-180.0f + wrapped));
		if (distance < nearest) {
			nearest = distance;
			nearestIndex = i;
		}
	}
	if (nearest < radius) {
		*gripAngle = unk150[nearestIndex];
		return true;
	}
	return false;
}
#pragma dont_inline off

u8 TBathtub::getNextJuncture(const JGeometry::TVec3<f32>& position,
                             const JGeometry::TVec3<f32>& direction) const
{
	Mtx* matrix = getRootJointMtx();
	JGeometry::TVec3<f32> delta(position.x - (*matrix)[0][3],
	                            position.y - (*matrix)[1][3],
	                            position.z - (*matrix)[2][3]);
	JGeometry::TVec3<f32> normal;
	normal.setLength(delta, 1.0f);

	const f32 dot = normal.x * direction.x + normal.y * direction.y
	              + normal.z * direction.z;
	JGeometry::TVec3<f32> projected(
	    delta.x + direction.x - normal.x * dot,
	    delta.y + direction.y - normal.y * dot,
	    delta.z + direction.z - normal.z * dot);
	f32 localX = (*matrix)[0][0] * projected.x
	            + (*matrix)[1][0] * projected.y
	            + (*matrix)[2][0] * projected.z;
	f32 localZ = (*matrix)[0][2] * projected.x
	            + (*matrix)[1][2] * projected.y
	            + (*matrix)[2][2] * projected.z;
	f32 angle = 0.005493164f * static_cast<f32>(matan(localZ, localX));

	f32 nearest = 180.0f;
	u8 nearestIndex = 0;
	for (u8 i = 0; i < 5; ++i) {
		f32 wrapped = static_cast<f32>(fmod(360.0f + (unk13C[i] - angle + 180.0f),
		                                    360.0f));
		f32 distance = static_cast<f32>(fabs(-180.0f + wrapped));
		if (distance < nearest) {
			nearestIndex = i;
			nearest = distance;
		}
	}
	return nearestIndex;
}

u8 TBathtub::getNextGrip(const JGeometry::TVec3<f32>& position,
                         const JGeometry::TVec3<f32>& direction, f32 radius,
                         f32* gripAngle) const
{
	Mtx* matrix = getRootJointMtx();
	JGeometry::TVec3<f32> delta(position.x - (*matrix)[0][3],
	                            position.y - (*matrix)[1][3],
	                            position.z - (*matrix)[2][3]);
	JGeometry::TVec3<f32> normal;
	normal.setLength(delta, 1.0f);
	const f32 dot = normal.x * direction.x + normal.y * direction.y
	              + normal.z * direction.z;
	JGeometry::TVec3<f32> projected(
	    delta.x + direction.x - normal.x * dot,
	    delta.y + direction.y - normal.y * dot,
	    delta.z + direction.z - normal.z * dot);
	f32 localX = (*matrix)[0][0] * projected.x
	            + (*matrix)[1][0] * projected.y
	            + (*matrix)[2][0] * projected.z;
	f32 localZ = (*matrix)[0][2] * projected.x
	            + (*matrix)[1][2] * projected.y
	            + (*matrix)[2][2] * projected.z;
	f32 angle = 0.005493164f * static_cast<f32>(matan(localZ, localX));

	f32 nearest = 180.0f;
	u8 nearestIndex = 0;
	for (u8 i = 0; i < 5; ++i) {
		f32 wrapped = static_cast<f32>(fmod(360.0f + (unk150[i] - angle + 180.0f),
		                                    360.0f));
		f32 distance = static_cast<f32>(fabs(-180.0f + wrapped));
		if (distance < nearest) {
			nearestIndex = i;
			nearest = distance;
		}
	}
	if (nearest >= radius)
		return 0;
	*gripAngle = unk150[nearestIndex];
	return 1;
}

void TBathtub::updatePosture_() { }

TBathtub::TBathtub(const char* name)
    : TMapObjBase(name)
{
	unk164 = nullptr;
	unk290 = 0;
	unk16C = reinterpret_cast<TBathtubParams*>(
	    __ct__14TBathtubParamsFv(new u8[0x210]));
	unk1D8 = 0.0f;
	unk1DC = 0.0f;
	unk1E0 = 0.0f;
	unk1E4 = 1.0f;
	mPosition.x = 0.0f;
	mPosition.y = 0.0f;
	mPosition.z = 0.0f;
	unk1E8 = 0.0f;
	unk1EC = 0.0f;
	unk1F0 = 0.0f;
	unk250 = 0;
	unk254 = 1;
	unk258 = 0;
	unk25C = 1;
	unk248 = 0;
	unk298 = 0;
	unk244 = 0.0f;
	unk240 = 0.0f;
	unk23C = 0.0f;
	unk29A = 0;
	unk2A0 = 0;
	unk294 = 0;
}

void TBathtub::load(JSUMemoryInputStream&) { }

u8 TBathtub::getNumKillerLaunchable() const
{
	if (reinterpret_cast<const u8*>(this)[0x299] != 0
	    || !bathtubKoopaAllowsLaunch() || unk248 > 0)
		return 0;

	int count = getNumGripsDead() + 1;
	if (count < 2)
		count = 2;
	if (count > 4)
		count = 4;
	return static_cast<u8>(count);
}

bool TBathtub::isKillerAttackable() const { return unk248 <= 0; }

u8 TBathtub::getNumKillerBurstable() const
{
    if (reinterpret_cast<const u8*>(this)[0x299] != 0
        || !bathtubKoopaAllowsLaunch() || unk248 > 0)
        return 0;

	int count = getNumGripsDead();
	if (count >= 4)
		return 8;
	if (!allowsTumble() && unk250 == 0 && unk258 == 0) {
        switch (count) {
        case 1:
            return 4;
        case 2:
            return 6;
		case 3:
		case 4:
			return 8;
		default:
			return 0;
		}
	}
	return 0;
}

// Unused
bool TBathtub::isBreaking() const { return false; }

// Unused
bool TBathtub::isKillerLaunchable() const { return false; }

// Unused
void TBathtub::showMessage(u32) { }

// Unused
u8 TBathtub::getNearJuncture(const JGeometry::TVec3<f32>&) const { return 0; }

// Unused
MtxPtr TBathtub::getKoopaMtxInDemo() { return nullptr; }

// Unused
MtxPtr TBathtub::getWaterMtx(s32) { return nullptr; }

// Unused
MtxPtr TBathtub::getShineEffectMtx() { return nullptr; }

// Unused
MtxPtr TBathtub::getShineMtx() { return nullptr; }

// Unused
void TBathtub::liftMario(const JGeometry::TVec3<f32>&) { }

// Unused
void TBathtub::trample(const JGeometry::TVec3<f32>&) { }
