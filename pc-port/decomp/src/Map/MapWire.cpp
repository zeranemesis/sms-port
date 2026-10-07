#include <Map/MapWire.hpp>

#include <dolphin/mtx.h>
#include <dolphin/gx.h>
#include <fake_tgmath.h>
#include <dolphin/types.h>

#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>
#include <JSystem/JMath.hpp>
#include <Camera/CubeMapTool.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/ModelUtil.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <Player/MarioAccess.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <System/DummyMactorString.hpp>
#include <System/DummyStrings.hpp>
// After the no-memory message: retail's .rodata has setUpTrans's zero and one
// literals between it and this unit's own data (c-r35).
#include <Map/MapCollisionEntry.hpp>

TMapWirePoint::TMapWirePoint()
{
	mPosOnWire     = 0.0f;
	mPosReturnRate = 0.0f;
	mPosition.zero();
	mDefaultPosition.zero();
}

f32 TMapWire::mMoveTimerSpeed = 0.03f;
f32 TMapWire::mDownRateMax    = 0.003f;
f32 TMapWire::mEndRate        = 0.001f;
f32 TMapWire::mStretchRate    = 2.0f;
f32 TMapWire::mHeightRate     = 1.3f;
f32 TMapWire::mReleaseHeight  = 100.0f;
f32 TMapWire::mFootLength     = 26.0f;
f32 TMapWire::mDrawWidth      = 5.0f;
f32 TMapWire::mDrawHeight     = 6.0f;

// Retail adds the point before the offset inside the strip loops, which only
// happens when the vertex goes through a helper taking the point by reference.
static inline void addPoint(const JGeometry::TVec3<f32>& p, f32 dx, f32 dz)
{
	GXPosition3f32(p.x + dx, p.y, p.z + dz);
}

static inline void subPoint(const JGeometry::TVec3<f32>& p, f32 dx, f32 dz)
{
	GXPosition3f32(p.x - dx, p.y, p.z - dz);
}

static inline void downPoint(const JGeometry::TVec3<f32>& p, f32 h)
{
	GXPosition3f32(p.x, p.y - h, p.z);
}

// TODO: frame is 0x70, retail 0x78; helper placement on the start/end points
// moves it in 8-byte steps but no tried split reaches 0x78.
void TMapWire::drawLower() const
{
	f32 xOffset = mDrawAxes.x;
	xOffset *= mDrawWidth;
	f32 zOffset = mDrawAxes.y;
	zOffset *= mDrawWidth;

	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, (mNumActiveMapWirePoints + 2) * 2);

	subPoint(mStartPoint, xOffset, zOffset);
	downPoint(getStartPoint(), mDrawHeight);

	for (int i = 0; i < mNumActiveMapWirePoints; i++) {
		subPoint(mMapWirePoints[i].mPosition, xOffset, zOffset);
		downPoint(mMapWirePoints[i].mPosition, mDrawHeight);
	}

	subPoint(mEndPoint, xOffset, zOffset);
	downPoint(mEndPoint, mDrawHeight);

	GXEnd();

	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, (mNumActiveMapWirePoints + 2) * 2);

	downPoint(mStartPoint, mDrawHeight);
	addPoint(mStartPoint, xOffset, zOffset);

	for (int i = 0; i < mNumActiveMapWirePoints; i++) {
		downPoint(mMapWirePoints[i].mPosition, mDrawHeight);
		addPoint(mMapWirePoints[i].mPosition, xOffset, zOffset);
	}

	downPoint(mEndPoint, mDrawHeight);
	addPoint(mEndPoint, xOffset, zOffset);

	GXEnd();
}

// TODO: frame is 0x40, retail 0x58; helpers on the start/end points, a
// const getPoint(), and TVec2/TVec3 offset locals were inert or wrong.
// lever-search closes it only with a mixed spelling (a named
// `startPoint` ref for the first vertex's x/z, getStartPoint() elsewhere):
// docs/progress/lever-search/mapwire_drawupper.patch. Not applied as
// implausible; addPoint/subPoint on either spelling stays at 99.9.
// Each getX() passed to addPoint/subPoint is one dead word: all four start
// and end vertices through the helpers with getStartPoint() on both start
// ones is instruction-exact at 0x50; getEndPoint() in the end helpers lands
// 0x58 but schedules the r31 restore after mtlr (and a raw end with the
// accessor in only one helper breaks the body).
// c-hs4: getStartPoint() at all six inline start reads (end raw, inline or
// through the helpers) lands 0x58 with every slot; the only residue is the
// first vertex's two fadds, where retail adds point + offset (addPoint's
// order). addPoint(getStartPoint()) there is 0x50, and `xOffset + x` is inert.
void TMapWire::drawUpper() const
{
	f32 xOffset = mDrawAxes.x;
	xOffset *= mDrawWidth;
	f32 zOffset = mDrawAxes.y;
	zOffset *= mDrawWidth;

	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, (mNumActiveMapWirePoints + 2) * 2);

	GXPosition3f32(mStartPoint.x + xOffset, mStartPoint.y,
	               mStartPoint.z + zOffset);
	GXPosition3f32(mStartPoint.x - xOffset, mStartPoint.y,
	               mStartPoint.z - zOffset);

	for (int index = 0; index < mNumActiveMapWirePoints; index++) {
		addPoint(mMapWirePoints[index].mPosition, xOffset, zOffset);
		subPoint(mMapWirePoints[index].mPosition, xOffset, zOffset);
	}

	GXPosition3f32(mEndPoint.x + xOffset, mEndPoint.y, mEndPoint.z + zOffset);
	GXPosition3f32(mEndPoint.x - xOffset, mEndPoint.y, mEndPoint.z - zOffset);

	GXEnd();
}

f32 TMapWire::getPointPowerAtReleased(f32 pos) const
{
	// 1 = default height, 0 = stretched all the way down
	f32 relativeHeightAtPos;
	if (pos >= mHangPos) {
		relativeHeightAtPos = (pos - mHangPos) / (1.0f - mHangPos);
	} else {
		relativeHeightAtPos = 1.0f - pos / mHangPos;
	}

	f32 power = 1.0f - relativeHeightAtPos * relativeHeightAtPos;
	return power;
}

void TMapWire::getPointPosAtReleased(f32 pos, JGeometry::TVec3<f32>* out) const
{
	JGeometry::TVec3<f32> linePoint;
	getPointPosOnLine(pos, &linePoint);

	JGeometry::TVec3<f32> defaultPoint;
	getPointPosDefault(pos, &defaultPoint);

	f32 power = getPointPowerAtReleased(pos);

	// Component stores: retail stores x before the y blend is finished.
	out->x = linePoint.x;
	out->y = linePoint.y
	         + (1.0f - mBounceRemainingPower) * (defaultPoint.y - linePoint.y)
	         + power * mHangOrBouncePoint.y;
	out->z = linePoint.z;
}

void TMapWire::updatePointAtReleased(int index)
{
	TMapWirePoint* mapWirePoint = &mMapWirePoints[index];

	f32 pos = mapWirePoint->mDefaultPosOnWire;
	if (fabsf(pos - mapWirePoint->mPosOnWire)
	    > fabsf(mapWirePoint->mPosReturnRate)) {
		pos = mapWirePoint->mPosOnWire + mapWirePoint->mPosReturnRate;
	}

	// The point is computed into a local and then copied: writing straight
	// into mPosition puts move()/release() instructions off retail.
	JGeometry::TVec3<f32> newPos;
	getPointPosAtReleased(pos, &newPos);
	mapWirePoint->mPosition.set(newPos.x, newPos.y, newPos.z);
}

bool TMapWire::updateMovePointAtReleased()
{
	mBounceRemainingPower -= mBounceDecayRate;

	if (mBounceRemainingPower < TMapWire::mEndRate)
		return true;

	mMoveTimer += TMapWire::mMoveTimerSpeed;
	if (mMoveTimer >= 2.0f) {
		mMoveTimer -= 2.0f;
	}

	f32 bounceCos = JMASCos(mMoveTimer * 32768.0f);
	mHangOrBouncePoint.y = bounceCos * mBounceAmplitude * mBounceRemainingPower;
	return false;
}

void TMapWire::initPointAtJustReleased(f32 pos, TMapWirePoint* point)
{
	point->mPosOnWire = pos;
	JGeometry::TVec3<f32> newPos;
	getPointPosAtReleased(pos, &newPos);
	point->mPosition.set(newPos.x, newPos.y, newPos.z);
	point->mPosReturnRate = (point->mDefaultPosOnWire - pos) / 1000.0f;
}

// TODO: 99.8%: frame 0x138 vs ours 0xf8, every instruction and register
// right. Same missing slots as move(). Assigning Mario's velocity
// component-wise (upstream's spelling) moves the register choice closer
// than the three-argument constructor did; SMS_GetMarioSpeed*() at all
// four reads instead of the raw pointers adds 0x10 of frame (c-hs6).
void TMapWire::release()
{
	if (mState == TMapWire::RELEASED)
		return;

	mState = TMapWire::RELEASED;

	mHangOrBouncePoint.zero();

	mNumActiveMapWirePoints = mNumMapWirePoints;

	int halfNumPoints = mNumActiveMapWirePoints / 2;

	if (halfNumPoints != 0) {
		f32 posAdvancePerPoint = mHangPos / halfNumPoints;

		for (int i = 0; i < halfNumPoints; i++) {
			mMapWirePoints[i].reset();

			initPointAtJustReleased(posAdvancePerPoint * (i + 1),
			                        &mMapWirePoints[i]);
		}
	}

	if (mNumActiveMapWirePoints - halfNumPoints != 0) {
		f32 posAdvancePerPoint
		    = (1.0f - mHangPos) / (mNumActiveMapWirePoints - halfNumPoints);

		for (int i = halfNumPoints; i < mNumActiveMapWirePoints; i++) {
			mMapWirePoints[i].reset();

			initPointAtJustReleased(posAdvancePerPoint * (i - halfNumPoints + 1)
			                            + mHangPos,
			                        &mMapWirePoints[i]);
		}
	}

	f32 stretchRatio = mStretchRate * abs(mHangPos - 0.5f);

	if (SMS_GetMarioSpeedY() > 0) {
		JGeometry::TVec3<f32> marioVel;
		marioVel.x = SMS_GetMarioSpeedX();
		marioVel.y = SMS_GetMarioSpeedY();
		marioVel.z = SMS_GetMarioSpeedZ();
		mBounceAmplitude = mHeightRate * marioVel.length();
	} else {
		mBounceAmplitude = mReleaseHeight;
	}

	mBounceDecayRate      = mDownRateMax * stretchRatio;
	mBounceRemainingPower = 1.0f;
	mMoveTimer            = 1.0f;
}

void TMapWire::getPointPosAtHanged(f32 pos, JGeometry::TVec3<f32>* out) const
{
	f32 posOffset = pos - mHangPos;

	out->x = mWireSpan.x * posOffset + mHangOrBouncePoint.x;
	out->z = mWireSpan.z * posOffset + mHangOrBouncePoint.z;

	if (pos <= mHangReferencePos1) {
		out->y = mHangOrBouncePoint.y
		         + ((mStartPoint.y - mHangOrBouncePoint.y)
		            * (mHangReferencePos1 - pos))
		               / mHangReferencePos1;
	} else if (pos >= mHangReferencePos2) {
		out->y = mHangOrBouncePoint.y
		         + ((mEndPoint.y - mHangOrBouncePoint.y)
		            * (pos - mHangReferencePos2))
		               / (1.0f - mHangReferencePos2);
	} else {
		out->y = mHangOrBouncePoint.y;
	}
}

void TMapWire::getPointInfoAtHanged(f32 pos, TMapWirePoint* point)
{
	point->mPosOnWire = pos;

	JGeometry::TVec3<f32> outPoint;
	getPointPosAtHanged(pos, &outPoint);
	point->mPosition.set(outPoint.x, outPoint.y, outPoint.z);
}

void TMapWire::setFootPointsAtHanged(MtxPtr mtx)
{
	mState = TMapWire::HANGING;

	mHangOrBouncePoint.set(mtx[0][3], mtx[1][3],
	                       mtx[2][3]); // translate portion of matrix
	mHangPos = getPosInWire(mHangOrBouncePoint);

	mHangReferencePos1 = mHangPos - mFootLength / mWireLength;
	mHangReferencePos2 = mHangPos + mFootLength / mWireLength;

	mNumActiveMapWirePoints = 2;

	TMapWirePoint* refPoints = &mMapWirePoints[0];
	if (mFootLength < mHangPos * mWireLength) {
		getPointInfoAtHanged(mHangReferencePos1, &refPoints[0]);
	} else {
		refPoints[0].mPosOnWire = mHangPos;
		refPoints[0].mPosition.set(mHangOrBouncePoint.x, mHangOrBouncePoint.y,
		                           mHangOrBouncePoint.z);
	}

	if (mFootLength < (1.0f - mHangPos) * mWireLength) {
		getPointInfoAtHanged(mHangReferencePos2, &refPoints[1]);
	} else {
		refPoints[1].mPosOnWire = mHangPos;
		refPoints[1].mPosition.set(mHangOrBouncePoint.x, mHangOrBouncePoint.y,
		                           mHangOrBouncePoint.z);
	}
}

void TMapWire::calcViewAndDBEntry()
{
	mStartFittingModel->viewCalc();
	mEndFittingModel->viewCalc();
}

// TODO: 99.8%: frame 0xd8 vs ours 0x90, every instruction right: retail's
// linePoint/defaultPoint pair sits 0x34 higher and the JMASCos fctiwz slot
// 0x1c further above it. getPointPosDefault spelled as component stores,
// a named sag or scaleAdd changes code (move 90-92%); declaring newPos,
// linePoint/defaultPoint or a named y earlier is frame-inert or +8, and
// computing power first changes code. Needs a structural lead. The JMASCos
// product's register came from updateMovePointAtReleased naming the cosine
// and multiplying it first (either alone is inert).
void TMapWire::move()
{
	switch (mState) {
	case IDLE:
		break;

	case HANGING:
		break;

	case RELEASED:
		if (updateMovePointAtReleased()) {
			TMapWirePoint* mapWirePoint;

			for (int i = 0; i < mNumActiveMapWirePoints; i++) {
				mapWirePoint = &mMapWirePoints[i];
				mapWirePoint->reset();
			}

			mState = TMapWire::IDLE;
		} else {
			for (int i = 0; i < mNumActiveMapWirePoints; i++) {
				updatePointAtReleased(i);
			}
		}
	}
}

f32 TMapWire::getPosInWire(const JGeometry::TVec3<f32>& point) const
{
	// TODO: frame, instructions and named block match: retail's perpPoint is
	// the top named slot (0x9c), so it is declared first and assigned later
	// (c-t6; named getStartPoint/getEndPoint references are worse). Left:
	// the two length temporaries of the return swap places (retail's first
	// at 0x54, its second at 0x60); a named denominator moves the frame.

	// Position here is only considered in the horizontal plane
	JGeometry::TVec3<f32> perpPoint;
	JGeometry::TVec3<f32> flatStart = getStartPoint();
	JGeometry::TVec3<f32> flatEnd   = getEndPoint();
	flatStart.y                     = 0.0f;
	flatEnd.y                       = 0.0f;

	perpPoint = MsPerpendicFootToLineR(flatStart, flatEnd, point);

	return JGeometry::TVec3<f32>(perpPoint - flatStart).length()
	     / JGeometry::TVec3<f32>(flatEnd - flatStart).length();
}

/**
 * @brief Gets a position on the straight line connecting the wire's endpoints.
 *
 * @param pos the relative position on the wire (0 to 1)
 * @param out the output vector
 */
// TODO: retail spells the products span-first (`fmadds span, pos, start`).
// `out->set(...)` here is refuted: it lands move()'s out-of-line `bl set<f>`
// (84.3 -> 85.4) but costs getPointPosOnWire 95.8 -> 48.9, so the
// out-of-line `set` in move()/release() must come from a deeper expansion.
void TMapWire::getPointPosOnLine(f32 pos, JGeometry::TVec3<f32>* out) const
{
	out->set(mWireSpan.x * pos + mStartPoint.x, mWireSpan.y * pos + mStartPoint.y,
	         mWireSpan.z * pos + mStartPoint.z);
}

// TODO: 99.7%: the line point's x/z loads are ordered x before z in retail,
// z before x here (getPointPosDefault's set arguments); spelling it as
// component stores or a scaleAdd shrinks the frame to 0x58 and costs move.
void TMapWire::getPointPosOnWire(f32 pos, JGeometry::TVec3<f32>* out) const
{
	if (pos < 0.0f) {
		pos = 0.0f;
	}
	if (pos > 1.0f) {
		pos = 1.0f;
	}

	if (mState == TMapWire::HANGING) {
		getPointPosAtHanged(pos, out);
	} else {
		getPointPosAtReleased(pos, out);
	}
}

/**
 * @brief The "default" position of a point on this wire after accounting for
 * its sag factor.
 *
 * @param pos the relative position on the wire (0 to 1)
 * @param out the output vector
 */
void TMapWire::getPointPosDefault(f32 pos, JGeometry::TVec3<f32>* out) const
{
	out->set(mWireSpan.x * pos + mStartPoint.x,
	         mWireSpan.y * pos + mStartPoint.y
	             - mWireSag * JMASSin(pos * 32768.0f),
	         mWireSpan.z * pos + mStartPoint.z);
}

void TMapWire::initTipPoints(const TCubeGeneralInfo* cubeInfo)
{
	JGeometry::TVec3<f32> halfWire(0.0f, 0.0f, mWireLength * 0.5f);

	JGeometry::TRotation3<TMtx33f> wireTransform;
	wireTransform.identity();
	wireTransform.setEular((s16)(cubeInfo->getUnk18().x / 180.0f * 32768.0f),
	                       (s16)(cubeInfo->getUnk18().y / 180.0f * 32768.0f),
	                       (s16)(cubeInfo->getUnk18().z / 180.0f * 32768.0f));

	wireTransform.mult33(halfWire);

	mStartPoint.set(cubeInfo->getUnkC().x - halfWire.x,
	                cubeInfo->getUnkC().y - halfWire.y + cubeInfo->getUnk24().y,
	                cubeInfo->getUnkC().z - halfWire.z);

	mEndPoint.set(halfWire.x + cubeInfo->getUnkC().x,
	              halfWire.y + cubeInfo->getUnkC().y + cubeInfo->getUnk24().y,
	              halfWire.z + cubeInfo->getUnkC().z);

	mWireSpan = mEndPoint - mStartPoint;
}

// TODO: two instructions differ and the frame is 0x30 short (0x180 vs
// 0x1b0); getStartPoint() at the start reads (end raw, as in drawUpper) took
// it from 0x168.
void TMapWire::init(const TCubeGeneralInfo* cubeInfo)
{
	mNumMapWirePoints = (s32)((cubeInfo->getUnk24().z / 50.0f + 1.0f) - 2.0f);
	mNumActiveMapWirePoints = mNumMapWirePoints;

	mMapWirePoints = new TMapWirePoint[mNumMapWirePoints];

	mWireLength = cubeInfo->getUnk24().z;

	initTipPoints(cubeInfo);

	mWireSag = cubeInfo->getUnk24().y * 0.5f;

	TMapWirePoint* point2;
	for (int i = 0; i < mNumMapWirePoints; i++) {
		{
			TMapWirePoint* point = &mMapWirePoints[i];
			f32 pos              = (f32)(i + 1) / (f32)(mNumMapWirePoints);
			point->mPosOnWire = point->mDefaultPosOnWire = pos;
		}

		point2 = &mMapWirePoints[i];
		getPointPosDefault(point2->mPosOnWire, &point2->mDefaultPosition);

		point2->reset();
	}

	if (mEndPoint.x != getStartPoint().x) {
		f32 angle   = atanf((mEndPoint.z - getStartPoint().z)
		                    / (mEndPoint.x - getStartPoint().x));
		mWireHAngle = -angle * 180.0f / M_PI + 90.0f;
	} else {
		mWireHAngle = 0.0f;
	}

	mDrawAxes.set(mEndPoint.x - getStartPoint().x,
	              mEndPoint.z - getStartPoint().z);
	mDrawAxes.normalize();
	mDrawAxes.rotate(M_PI / 2);

	mStartFittingModel
	    = SMS_CreatePartsModel("/common/map/WireFitting.bmd",
	                           J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
	                               | (1 << J3DMLF_TevStageNumShift));
	mEndFittingModel
	    = new J3DModel(mStartFittingModel->getModelData(),
	                   J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
	                       | (1 << J3DMLF_TevStageNumShift),
	                   1);

	Mtx mtx;

	MsMtxSetXYZRPH(mtx, getStartPoint().x, getStartPoint().y,
	               getStartPoint().z, cubeInfo->getUnk18().x,
	               cubeInfo->getUnk18().y, cubeInfo->getUnk18().z);
	mStartFittingModel->setBaseTRMtx(mtx);
	mStartFittingModel->calc();

	MsMtxSetXYZRPH(mtx, mEndPoint.x, mEndPoint.y, mEndPoint.z,
	               cubeInfo->getUnk18().x, cubeInfo->getUnk18().y + 180.0f,
	               cubeInfo->getUnk18().z);
	mEndFittingModel->setBaseTRMtx(mtx);
	mEndFittingModel->calc();

	gpMapObjManager->entryStaticDrawBufferSun(mStartFittingModel);
	gpMapObjManager->entryStaticDrawBufferSun(mEndFittingModel);

	TMapCollisionStatic* collision1 = new TMapCollisionStatic;
	collision1->init("/common/map/WireFitting.col", 2, nullptr);
	collision1->setUpMtx(mStartFittingModel->getAnmMtx(0));

	TMapCollisionStatic* collision2 = new TMapCollisionStatic;
	collision2->init("/common/map/WireFitting.col", 2, nullptr);
	collision2->setUpMtx(mEndFittingModel->getAnmMtx(0));
}

TMapWire::TMapWire()
{
	mWireHAngle             = 0.0f;
	mWireSag                = 0.0f;
	mNumActiveMapWirePoints = 0;
	mNumMapWirePoints       = 0;
	mMapWirePoints          = nullptr;
	mHangPos                = 0.0f;
	mMoveTimer              = 0.0f;
	mBounceRemainingPower   = 0.0f;
	mBounceAmplitude        = 0.0f;
	mHangReferencePos1      = 0.0f;
	mHangReferencePos2      = 0.0f;
	mState                  = TMapWire::IDLE;
	mStartPoint.zero();
	mEndPoint.zero();
	mWireSpan.zero();
	mDrawAxes.zero();
	mHangOrBouncePoint.zero();
	mStartFittingModel = nullptr;
	mEndFittingModel   = nullptr;
}
