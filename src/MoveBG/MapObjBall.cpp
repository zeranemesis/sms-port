
#include <MoveBG/MapObjBall.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
// rogue include: the retail object also carries the four
// MActorMtxCalcType_* names from M3DUtil/InfectiousStrings.hpp in .rodata
// (they are unreferenced, but the compiler still emits the literals), and
// they sit between the dummy pair and the /scene/mapObj/*.jpa strings.
#include <M3DUtil/InfectiousStrings.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionData.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MoveBG/Item.hpp>
#include <System/FlagManager.hpp>
#include <Player/ModelWaterManager.hpp>
#include <JSystem/JParticle/JPAResourceManager.hpp>
#include <JSystem/JGeometry.hpp>
#include <Player/MarioAccess.hpp>
#include <System/MarDirector.hpp>
#include <Map/PollutionManager.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <System/Particles.hpp>
#include <Camera/CubeManagerBase.hpp>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// The original calls JGeometry::TUtil<f32>::sqrt(v) out-of-line here
// (bl sqrt__Q29JGeometry8TUtil<f>Ff), with the range guard inside the
// callee. JGUtil.hpp only offers the inline spelling, so MWCC always
// expands these sites and the call never appears.
// FABRICATED: the callee is orig_sqrt, so the `bl` itself still shows as
// one mismatched instruction. Making JGUtil.hpp out-of-line instead was
// measured repo-wide at -32.2 points - see docs/AGENT_MATCHING_TIPS.md.
#pragma dont_inline on
static f32 orig_sqrt(f32 v) {
	return JGeometry::TUtil<f32>::sqrt(v);
}
#pragma dont_inline off


u32 TResetFruit::mFruitLivingTime       = 0x3840;
f32 TResetFruit::mScaleUpSpeed           = 1.05f;
f32 TResetFruit::mBreakingScaleSpeed     = 0.96f;
u32 TResetFruit::mFruitWaitTimeToAppear  = 0x168;

// TBigWatermelon::startEvent's four names, as the raw SJIS bytes the retail
// object carries at .rodata @1490+0x1AC/0x1C8/0x1D8/0x1EC. They are passed
// through verbatim, so the literals must stay byte-identical. Declaration
// order is also emission order, which is what fixes their .rodata offsets.
static const char kShineName[] = "\x83\x56\x83\x83\x83\x43\x93\x93\x81\x69"
                                 "\x82\xA8\x89\xBB\x82\xAF\x83\x58\x83\x43"
                                 "\x83\x4A\x97\x70\x81\x6A";
static const char kStageName[] = "\x83\x58\x83\x43\x83\x4A\x81\x69\x91\xE5"
                                 "\x81\x6A";
static const char kCamName[]   = "\x83\x58\x83\x43\x83\x53\x81\x5B\x83\x8B"
                                 "\x83\x4A\x83\x81\x83\x89\x83\x89";
static const char kDemoName[]  = "\x83\x58\x83\x43\x83\x56\x83\x83\x83\x83"
                                 "\x83\x43\x93\x93\x83\x4A\x83\x81\x83\x89";

// States that TMapObjGeneral does not name yet.
enum {
	FRUIT_STATE_ROTTING = 0xB,
	FRUIT_STATE_WAITING = 0xC,
	FRUIT_STATE_WAITTOSEE = 0xD,
};

// TMapObjBase::MAP_OBJ_FLAG_* has no enumerator for 0x40000; the fruit
// code always raises it together with startStateTimer(), so the name used
// here is a guess. TODO: needs a real name (and a header enumerator).
enum { MAP_OBJ_FLAG_HAS_STATE_TIMER = 0x40000 };

// Recovered from the five identical inline copies in
// receiveMessage / touchWaterSurface / touchPollution / touchGround /
// makeObjWaitingToAppear. The map lists no symbol for it.
static void TResetFruit_hideAndWait(TResetFruit* f)
{
	f->makeObjDefault();
	f->makeObjDead();
	f->calcRootMatrix();
	f->getModel()->calc();

	f->startStateTimer(TResetFruit::mFruitWaitTimeToAppear);
	f->offMapObjFlag(MAP_OBJ_FLAG_HAS_STATE_TIMER);
	f->setState(TMapObjGeneral::STATE_WAITING_TO_APPEAR);

	if (SMSGetMarDirector()->mMap == 3 && f->unk1A4)
		f->makeObjDead();
}

void TMapObjBall::touchRoof(JGeometry::TVec3<f32>* velocity)
{
	if (velocity->y > unk140)
		velocity->y = unk140;

	calcReflectingVelocity(unk13C, mMapObjData->mPhysical->unk4->unk4,
	                       &mVelocity);
}

void TMapObjBall::touchWall(JGeometry::TVec3<f32>* position,
	                        TBGWallCheckRecord* record)
{
	if (!checkLiveFlag(LIVE_FLAG_AIRBORNE) && isActorType(0x400000D0))
		mVelocity.y += unk184 * orig_sqrt(mVelocity.squared());

	for (int i = 0; i < record->mResultWallsNum; ++i) {
		const TBGCheckData* wall = record->mResultWalls[i];

		f32 dot = mVelocity.x * wall->mNormal.x
		          + mVelocity.y * wall->mNormal.y
		          + mVelocity.z * wall->mNormal.z;
		if (dot >= 0.0f)
			continue;

		f32 d = position->x * wall->mNormal.x
		        + position->y * wall->mNormal.y
		        + position->z * wall->mNormal.z;

		position->x += (mBodyRadius - (d + wall->mPlaneDistance))
		               * wall->mNormal.x;
		position->z += (mBodyRadius - d) * wall->mNormal.z;

		f32 k = dot * -(1.0f + mMapObjData->mPhysical->unk4->unk8);
		mVelocity.x += k * wall->mNormal.x;
		mVelocity.z += k * wall->mNormal.z;

		if (isActorType(0x400000D0)) {
			if (mScaling.y >= 5.0f) {
				f32 speed = orig_sqrt(mVelocity.squared());
				if (gpMSound->gateCheck(0x308A))
					MSoundSESystem::MSoundSE::startSoundActorWithInfo(
					    0x308A, &mPosition, nullptr, speed, 0, 0, nullptr, 4,
					    0);
			} else {
				f32 speed = orig_sqrt(mVelocity.squared());
				if (gpMSound->gateCheck(0x308B))
					MSoundSESystem::MSoundSE::startSoundActorWithInfo(
					    0x308B, &mPosition, nullptr, speed, 0, 0, nullptr, 4,
					    0);
			}
		} else {
			u32 sound = mMapObjData->mSound->unk4->unk0[2];
			if (gpMSound->gateCheck(sound))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    sound, &mPosition, &mVelocity, 0.0f, 0, 0, nullptr, 0, 4);
		}
	}
}

void TMapObjBall::touchPollution() { kill(); }

void TMapObjBall::touchWaterSurface() { kill(); }

void TMapObjBall::rebound(JGeometry::TVec3<f32>* position)
{
	// frame-size pad: the retail frame is 0x38 bytes larger than the code
	// needs; the extra locals the original declared here were all optimised
	// away.
	char framePad_56_rebound[0x38];
	(void)framePad_56_rebound;
	calcReflectingVelocity(
	    mGroundPlane, mMapObjData->mPhysical->unk4->unk4, &mVelocity);
	position->y = mGroundHeight;
	onLiveFlag(LIVE_FLAG_AIRBORNE);

	if (isActorType(0x400000D0)) {
		if (mScaling.y >= 5.0f) {
			f32 speed = fabsf(mGroundPlane->mNormal.y);
			if (gpMSound->gateCheck(0x3889))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x3889, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
		} else {
			f32 speed = fabsf(mGroundPlane->mNormal.y);
			if (gpMSound->gateCheck(0x388C))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x388C, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
		}
	} else {
		u32 sound = mMapObjData->mSound->unk4->unk0[4];
		if (gpMSound->gateCheck(sound))
			MSoundSESystem::MSoundSE::startSoundActorWithInfo(
			    sound, &mPosition, &mVelocity, 0.0f, 0, 0, nullptr, 0, 4);
	}
}

void TMapObjBall::touchGround(JGeometry::TVec3<f32>* position)
{
	f32 speed = fabsf(orig_sqrt(
	    mVelocity.x * mVelocity.x + mVelocity.y * mVelocity.y
	    + mVelocity.z * mVelocity.z));

	if (speed > 0.05f && isActorType(0x400000D0)) {
		if (mScaling.y >= 5.0f) {
			if (gpMSound->gateCheck(0x308A))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x308A, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
		} else {
			if (gpMSound->gateCheck(0x308B))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x308B, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
		}
	}

	// TODO: 0x100/0x101/0x102..0x105/0x4104 are TBGCheckData attribute
	// values that are not named anywhere in the tree yet.
	u16 attr = mGroundPlane->mBGType;
	if (attr == 0x100 || attr == 0x101
	    || (attr >= 0x102 && attr <= 0x105) || attr == 0x4104) {
		touchWaterSurface();
		position->x = mPosition.x;
		position->y = mPosition.y;
		position->z = mPosition.z;
		return;
	}

	if (gpPollution->isPolluted(position->x, position->y, position->z)) {
		touchPollution();
		position->x = mPosition.x;
		position->y = mPosition.y;
		position->z = mPosition.z;
		return;
	}

	if (mVelocity.y > -unk188) {
		onLiveFlag(LIVE_FLAG_UNK40);
		mVelocity.y = 0.0f;
		position->y = mGroundHeight;
	} else {
		rebound(position);
	}

	if (!checkLiveFlag(LIVE_FLAG_AIRBORNE)) {
		mVelocity.x += unk180 * mGroundPlane->mNormal.x;
		mVelocity.z += unk180 * mGroundPlane->mNormal.z;
	}

	f32 fric = mMapObjData->mPhysical->unk4->unk10;
	mVelocity.x *= fric;
	mVelocity.z *= fric;
}

void TMapObjBall::put()
{
	TMapObjGeneral::put();
	calcCurrentMtx();
}

void TMapObjBall::hold(TTakeActor* holder)
{
	JGeometry::TVec3<f32> velocity = mVelocity;
	f32 speed = orig_sqrt(velocity.x * velocity.x
	                                       + velocity.y * velocity.y
	                                       + velocity.z * velocity.z);
	if (speed <= 10.0f) {
		TMapObjGeneral::hold(holder);
		mVelocity.z = 0.0f;
		mVelocity.y = 0.0f;
		mVelocity.x = 0.0f;
	}
}

void TMapObjBall::kicked()
{
	JGeometry::TVec3<f32> velocity = mVelocity;

	if (velocity.y > 0.0f) {
		if (velocity.y == 0.0f) {
			mVelocity.y = unk178;
		} else {
			mVelocity.y = unk160 * velocity.y - unk174 * (*gpMarioSpeedY);
		}
	}

	mVelocity.x += unk170 * (*gpMarioSpeedX);
	mVelocity.z += unk170 * (*gpMarioSpeedZ);

	f32 min = mMapObjData->mPhysical->unk4->unkC;
	if (fabsf(mVelocity.x) < min && fabsf(mVelocity.z) < min) {
		mVelocity.x = 2.0f * (rand() * (1.0f / 32768.0f)) - 1.0f;
		mVelocity.z = 2.0f * (rand() * (1.0f / 32768.0f)) - 1.0f;
	}

	unk194 = 10;
	offLiveFlag(LIVE_FLAG_UNK10);

	SMS_GetMarioHitActor()->receiveMessage(this, 0xE);

	if (!isActorType(0x400000D0) && gpMSound->gateCheck(0x194F))
		MSoundSESystem::MSoundSE::startSoundActor(0x194F, &mPosition, 0,
		                                         nullptr, 0, 4);
}

u32 TMapObjBall::touchWater(THitActor* actor)
{
	if (!isState(STATE_HOLDING) && !isState(STATE_APPEARING)) {
		JGeometry::TVec3<f32> velocity = mVelocity;
		const JGeometry::TVec3<f32>& speed = getWaterSpeed(actor);
		velocity.x += speed.x * unk17C;
		velocity.y += speed.y * unk17C;
		velocity.z += speed.z * unk17C;
		mVelocity.x = velocity.x;
		mVelocity.y = velocity.y;
		mVelocity.z = velocity.z;
		offLiveFlag(LIVE_FLAG_UNK10);
	}

	return 1;
}

// The retail object calls this out-of-line from touchActor() and
// receiveMessage(), so keep it out of line.
#pragma dont_inline on
void TMapObjBall::boundByActor(THitActor* actor)
{
	// Direction to the actor, flattened onto the XZ plane. The zero Y is
	// live: it is the middle term of the dot product below.
	JGeometry::TVec3<f32> dir(actor->mPosition.x - mPosition.x, 0.0f,
	                         actor->mPosition.z - mPosition.z);

	// Mario's hit sphere grows with the fruit's own attack radius; every
	// other ball just uses its damage radius.
	f32 radius = isActorType(0x400000D0)
	                 ? mAttackRadius + actor->mDamageRadius
	                 : mDamageRadius;

	if (radius * radius < dir.x * dir.x + dir.z * dir.z)
		return;

	if (dir.x != 0.0f && dir.z != 0.0f)
		MsVECNormalize(&dir, &dir);

	if (actor->isActorType(0x80000001)) {
		// Mario kicked us rather than merely touching us.
		if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK2000000)) {
			f32 min = mMapObjData->mPhysical->unk4->unkC;
			if (fabsf(*gpMarioSpeedX) > min || fabsf(*gpMarioSpeedZ) > min) {
				mVelocity.y += unk150;
				if (!isActorType(0x400000D0)
				    && gpMSound->gateCheck(0x194F))
					MSoundSESystem::MSoundSE::startSoundActor(
					    0x194F, &mPosition, 0, nullptr, 0, 4);
			} else {
				mVelocity.y += unk154;
			}
			mVelocity.x += unk148 * *gpMarioSpeedX - dir.x * unk14C;
			mVelocity.z += unk148 * *gpMarioSpeedZ - dir.z * unk14C;
			actor->receiveMessage(this, 0xE);
		}
	} else {
		// Something else: bounce off `dir` if we are still travelling
		// towards it and are moving fast enough to be worth reflecting.
		JGeometry::TVec3<f32> d = mVelocity;
		f32 dot = d.x * dir.x + d.y * dir.y + d.z * dir.z;

		JGeometry::TVec3<f32> x = mVelocity;
		JGeometry::TVec3<f32> z = mVelocity;
		if (dot >= 0.0f
		    && fabsf(x.x) > mMapObjData->mPhysical->unk4->unkC
		    && fabsf(z.z) > mMapObjData->mPhysical->unk4->unkC) {
			f32 k = unk16C + 1.0f;
			mVelocity.x -= k * dir.x * dot;
			mVelocity.y += unk168;
			mVelocity.z -= k * dir.z * dot;
			actor->receiveMessage(this, 0x10);
			if (!isActorType(0x400000D0)
			    && gpMSound->gateCheck(0x3862))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x3862, &mPosition, 0, nullptr, 0, 4);
		} else {
			mVelocity.x -= dir.x * unk164;
			mVelocity.y += unk168;
			mVelocity.z -= dir.z * unk164;
		}

		if (actor->isActorType(0x80000001)
		    && !checkMapObjFlag(MAP_OBJ_FLAG_UNK2000000)) {
			JGeometry::TVec3<f32> f = mVelocity;
			if (f.y < 0.0f
			    && (130.0f + gpMarioPos->y)
			           < mPosition.y + mBodyRadius) {
				JGeometry::TVec3<f32> g = f;
				mVelocity.y = unk160 * -g.y;
				mVelocity.x += unk158 * *gpMarioSpeedX;
				mVelocity.y += unk15C * *gpMarioSpeedY;
				mVelocity.z += unk158 * *gpMarioSpeedZ;
				if (!isActorType(0x400000D0)
				    && gpMSound->gateCheck(0x194F))
					MSoundSESystem::MSoundSE::startSoundActor(
					    0x194F, &mPosition, 0, nullptr, 0, 4);
			}
		}
	}

	unk194 = 10;
	offLiveFlag(LIVE_FLAG_UNK10);
	onLiveFlag(LIVE_FLAG_AIRBORNE);
}
#pragma dont_inline off

// The retail object calls this out-of-line from TResetFruit::touchActor()
// and TResetFruit::receiveMessage(), so keep it out of line.
#pragma dont_inline on
void TMapObjBall::touchActor(THitActor* actor)
{
	// frame-size pad: the retail frame is 8 bytes larger than the code needs
	char framePad_8_touchActor[8];
	(void)framePad_8_touchActor;
	if (unk194 == 0 && !isState(STATE_HOLDING) && !isHideObj(actor)
	    && actor->isActorType(0x08000083) && !actor->isActorType(0x400000CA)
	    && !actor->isActorType(0x400000CC)) {
		if (actor->isActorType(0x80000001) && !isActorType(0x400000D0)
		    && *gpMarioSpeedY != 0.0f) {
			kicked();
			return;
		}
		boundByActor(actor);
	}
}
#pragma dont_inline off

void TMapObjBall::calcCurrentMtx()
{
	// TODO: 1072 bytes rebuilding the model matrix from the rolling
	// parameter table. Not reconstructed.
}

void TMapObjBall::checkWallCollision(JGeometry::TVec3<f32>* position)
{
	JGeometry::TVec3<f32> center(position->x, position->y + mBodyRadius,
	                            position->z);

	TBGWallCheckRecord record(center, mBodyRadius, 4,
	                          mMapObjData->mPhysical->mWallCheckFlags);

	if (gpMap->isTouchedWallsAndMoveXZ(&record)) {
		unk138      = record.mResultWalls[0];
		position->x = record.mCenter.x;
		position->z = record.mCenter.z;
		touchWall(position, &record);
	} else {
		unk138 = nullptr;
	}
}

void TMapObjBall::makeObjDefault()
{
	// The retail frame is 8 bytes larger than the code needs; the extra
	// locals the original declared here were all optimised away.
	char framePad_8_makeObjDefault[8];
	(void)framePad_8_makeObjDefault;
	TMapObjBase::makeObjDefault();
	MtxPtr mtx = getModel()->getAnmMtx(0);
	mtx[0][3]  = mPosition.x;
	mtx[1][3]  = mPosition.y + mBodyRadius;
	mtx[2][3]  = mPosition.z;
}

void TMapObjBall::makeObjAppeared()
{
	// frame-size pad: the retail frame is 8 bytes larger than the code needs
	char framePad_8_makeObjAppeared[8];
	(void)framePad_8_makeObjAppeared;
	TMapObjBase::makeObjAppeared();
	calcCurrentMtx();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	mtx[0][3]  = mPosition.x;
	mtx[1][3]  = mPosition.y + mBodyRadius;
	mtx[2][3]  = mPosition.z;

	if (isActorType(0x40000394) && mtx[1][1] > 0.0f)
		mtx[1][3] -= 50.0f * mtx[1][1];

	if (isActorType(0x40000392))
		mtx[1][3] -= 10.0f * (1.0f - mtx[1][1]);

	unkE8 = 0;
}

// TResetFruit::control() calls this twice with a real `bl`
// (801DA53C, 801DA5C0); MWCC inlines it away by default.
#pragma dont_inline on
void TMapObjBall::control()
{
	TMapObjGeneral::control();

	if (unk194 != 0)
		unk194--;

	if (isState(STATE_HOLDING)) {
		Mtx mtx;
		PSMTXCopy(mHolder->getTakingMtx(), mtx);
		mtx[1][3] += unk190;
		PSMTXCopy(mtx, getModel()->getAnmMtx(0));
	} else {
		JGeometry::TVec3<f32> velocity = mVelocity;
		// frame-size pad: the retail frame is 0x18 bytes larger than the
		// code needs; the extra locals the original declared here were all
		// optimised away.
		char framePad_24_control[0x18];
		(void)framePad_24_control;
		if (!(velocity.squared() <= JGeometry::TUtil<f32>::epsilon())
		    && mGroundPlane->mActor)
			calcCurrentMtx();
	}
}
#pragma dont_inline off

BOOL TMapObjBall::receiveMessage(THitActor* sender, u32 message)
{
	if (TMapObjGeneral::receiveMessage(sender, message))
		return true;

	if (message == 4 && checkMapObjFlag(MAP_OBJ_FLAG_UNK100000)) {
		hold((TTakeActor*)sender);
		return true;
	}

	if (sender->isActorType(0x80000001) && !isActorType(0x400000D0)
	    && message != 4) {
		kicked();
		return true;
	}

	return false;
}

// The six parameter tables below were transcribed field-by-field out of
// the .s. The field names are placeholders; the *assignment order* inside
// each table is what the original wrote and must not be re-sorted --
// 0x4000064 and the 0x4000039x tables store 0x170/0x174/0x178 before
// 0x164/0x168/0x16C.
void TMapObjBall::initMapObj()
{
	TMapObjGeneral::initMapObj();

	mInitialScaling.set(mScaling);

	switch (mActorType) {
	case 0x400000D0:
		unk14C = 4.0f;
		unk150 = 0.0f;
		unk154 = 0.0f;
		unk158 = 0.15f;
		unk15C = 0.0f;
		unk160 = 0.9f;
		unk164 = 0.06f;
		unk168 = 1.5f;
		unk16C = 0.5f;
		unk170 = 0.5f;
		unk174 = 0.2f;
		unk178 = 2.5f;
		unk17C = 0.001f;
		unk180 = 0.3f;
		unk184 = 1.5f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		unk18C     = mBodyRadius / 3.0f;
		break;
	case 0x40000064:
		unk148 = 0.6f;
		unk14C = 2.0f;
		unk150 = 0.02f;
		unk154 = 0.0f;
		unk158 = 0.055f;
		unk15C = 0.02f;
		unk160 = 0.83f;
		unk170 = 0.9f;
		unk174 = 0.13f;
		unk178 = 20.0f;
		unk164 = 0.5f;
		unk168 = 0.02f;
		unk16C = 0.5f;
		unk17C = 1.2f;
		unk180 = 0.8f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		unk18C     = mBodyRadius / 3.0f;
		break;
	case 0x40000393:
		unk148 = 0.6f;
		unk14C = 0.2f;
		unk150 = 1.3f;
		unk154 = 15.0f;
		unk158 = 0.5f;
		unk15C = 1.3f;
		unk160 = 1.0f;
		unk170 = 0.9f;
		unk174 = 0.13f;
		unk178 = 20.0f;
		unk164 = 2.0f;
		unk168 = 0.02f;
		unk16C = 0.3f;
		unk17C = 0.05f;
		unk180 = 0.5f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		unk18C     = 50.0f;
		break;
	case 0x40000390:
	case 0x40000391:
	case 0x40000392:
		unk148 = 0.4f;
		unk14C = 0.2f;
		unk150 = 1.3f;
		unk154 = 0.0f;
		unk158 = 1.2f;
		unk15C = 0.8f;
		unk160 = 0.5f;
		unk170 = 0.9f;
		unk174 = 0.13f;
		unk178 = 20.0f;
		unk164 = 2.0f;
		unk168 = 0.02f;
		unk16C = 0.3f;
		unk17C = 0.05f;
		unk180 = 0.5f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		unk18C     = 50.0f;
		break;
	case 0x40000394:
		unk148 = 0.2f;
		unk14C = 0.0f;
		unk150 = 0.0f;
		unk154 = 0.0f;
		unk158 = 0.0f;
		unk15C = 0.0f;
		unk160 = 0.0f;
		unk170 = 0.0f;
		unk174 = 0.0f;
		unk178 = 0.0f;
		unk164 = 0.0f;
		unk168 = 0.0f;
		unk16C = 0.0f;
		unk17C = 0.05f;
		unk180 = 0.5f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		unk18C     = 50.0f;
		break;
	case 0x40000395:
		unk148 = 0.4f;
		unk14C = 0.2f;
		unk150 = 1.3f;
		unk154 = 0.0f;
		unk158 = 1.2f;
		unk15C = 0.8f;
		unk160 = 0.5f;
		unk170 = 0.9f;
		unk174 = 0.13f;
		unk178 = 20.0f;
		unk164 = 2.0f;
		unk168 = 0.02f;
		unk16C = 0.3f;
		unk17C = 0.05f;
		unk180 = 0.5f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		unk18C     = 50.0f;
		break;
	}

	if (isActorType(0x40000393)) {
		mBodyRadius = 45.0f * mScaling.y;
		unk190      = mBodyRadius;
	}
	if (isActorType(0x40000390)) {
		mBodyRadius = 40.0f * mScaling.y;
		unk190      = 20.0f;
	}
	if (isActorType(0x40000391)) {
		mBodyRadius = 40.0f * mScaling.y;
		unk190      = 20.0f;
	}
	if (isActorType(0x40000392))
		unk190 = 10.0f;
}

TMapObjBall::TMapObjBall(const char* name)
	: TMapObjGeneral(name)
{
	unk148 = 0.0f;
	unk14C = 0.0f;
	unk150 = 0.0f;
	unk154 = 0.0f;
	unk158 = 0.0f;
	unk15C = 0.0f;
	unk160 = 0.0f;
	unk164 = 0.0f;
	unk168 = 0.0f;
	unk16C = 0.0f;
	unk170 = 0.0f;
	unk174 = 0.0f;
	unk178 = 0.0f;
	unk17C = 0.0f;
	unk180 = 0.0f;
	unk184 = 0.0f;
	unk188 = 0.0f;
	unk18C = 0.0f;
	unk190 = 0.0f;
	unk194 = 0;
	mInitialScaling.z = 0.0f;
	mInitialScaling.y = 0.0f;
	mInitialScaling.x = 0.0f;
}

void TResetFruit::checkGroundCollision(JGeometry::TVec3<f32>* position)
{
	// frame-size pad: the retail frame is 0x28 bytes larger than the code needs
	char framePad_40_checkGroundCollision[0x24];
	(void)framePad_40_checkGroundCollision;
	if (SMSGetMarDirector()->mMap == 7 || SMSGetMarDirector()->mMap == 4) {
		if (SMSGetMarDirector()->mMap == 4) {
			mGroundHeight = gpMap->checkGround(position->x,
			                                   position->y + 200.0f,
			                                   position->z, &mGroundPlane);
			mGroundHeight += 1.0f;
			if (!(position->y <= mGroundHeight)) {
				touchGround(position);
			} else {
				onLiveFlag(LIVE_FLAG_AIRBORNE);
			}
		} else {
			mGroundHeight = gpMap->checkGround(position->x,
			                                   position->y + mHeadHeight,
			                                   position->z, &mGroundPlane);
			if (mGroundPlane->mBGType == 0x801
			    || mGroundPlane->mBGType == 0x203) {
				mGroundHeight = gpMap->checkGroundExactY(
				    position->x, mGroundHeight - 200.0f, position->z,
				    &mGroundPlane);
			}
			mGroundHeight += 1.0f;
			if (!(position->y <= mGroundHeight)) {
				touchGround(position);
			} else {
				onLiveFlag(LIVE_FLAG_AIRBORNE);
			}
		}
	} else {
		TMapObjGeneral::checkGroundCollision(position);
	}
}

void TResetFruit::waitingToAppear()
{
	if (SMSGetMarDirector()->mMap == 3 && unk1A4)
		makeObjDead();

	if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000) && !isStateTimerEngaged()
	    && getHitObjNumMax() == 0) {
		onMapObjFlag(MAP_OBJ_FLAG_HAS_STATE_TIMER);
		makeObjAppeared();

		Mtx mtx;
		// frame-size pad: the retail frame is 0x28 bytes larger than the
		// code needs; the extra locals were all optimised away.
		char framePad_36_waitingToAppear[0x24];
		(void)framePad_36_waitingToAppear;
		PSMTXScale(&mtx[0], 0.2f, 0.2f, 0.2f);
		concatOnlyRotFromLeft(&mtx[0], getModel()->getAnmMtx(0),
		                      getModel()->getAnmMtx(0));

		mScaling.y = 0.2f;
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mState = STATE_APPEARING;

		if (gpMSound->gateCheck(0x3802))
			MSoundSESystem::MSoundSE::startSoundActor(0x3802, &mPosition, 0,
			                                         nullptr, 0, 4);
	}
}

void TResetFruit::makeObjWaitingToAppear()
{
	char framePad_8_makeObjWaitingToAppear[8];
	(void)framePad_8_makeObjWaitingToAppear;
	mState = FRUIT_STATE_ROTTING;
	TResetFruit_hideAndWait(this);
}

void TResetFruit::thrown()
{
	TMapObjGeneral::thrown();
	mState = FRUIT_STATE_ROTTING;
}

void TResetFruit::hold(TTakeActor* holder)
{
	if (orig_sqrt(mVelocity.squared()) <= 10.0f) {
		TMapObjGeneral::hold(holder);
	}
	mVelocity.zero();

	offLiveFlag(LIVE_FLAG_UNK10);

	if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000) && !isStateTimerEngaged()) {
		onMapObjFlag(MAP_OBJ_FLAG_HAS_STATE_TIMER);
		startStateTimer(getLivingTime());
	}
}

void TResetFruit::touchPollution()
{
	// TODO: the leading particle emission (index 0x8B) needs
	// TMarioParticleManager's wrapper, which is not in the tree.
	if (gpMSound->gateCheck(0x3881))
		MSoundSESystem::MSoundSE::startSoundActor(0x3881, &mPosition, 0,
		                                         nullptr, 0, 4);

	mState = FRUIT_STATE_ROTTING;
	TResetFruit_hideAndWait(this);
	mState = FRUIT_STATE_ROTTING;
	TResetFruit_hideAndWait(this);
}

void TResetFruit::touchWaterSurface()
{
	char framePad_16_touchWaterSurface[0x10];
	(void)framePad_16_touchWaterSurface;
	emitColumnWater();

	if (gpMSound->gateCheck(0x3875))
		MSoundSESystem::MSoundSE::startSoundActor(0x3875, &mPosition, 0,
		                                         nullptr, 0, 4);

	mState = FRUIT_STATE_ROTTING;
	TResetFruit_hideAndWait(this);
}

u32 TResetFruit::touchWater(THitActor* actor)
{
	if (!isState(STATE_HOLDING) && !isState(STATE_APPEARING)) {
		JGeometry::TVec3<f32> velocity = mVelocity;
		const JGeometry::TVec3<f32>& speed = getWaterSpeed(actor);
		velocity.x += speed.x * unk17C;
		velocity.y += speed.y * unk17C;
		velocity.z += speed.z * unk17C;
		mVelocity.x = velocity.x;
		mVelocity.y = velocity.y;
		mVelocity.z = velocity.z;
		offLiveFlag(LIVE_FLAG_UNK10);
	}

	if (!isStateTimerEngaged()) {
		onMapObjFlag(MAP_OBJ_FLAG_HAS_STATE_TIMER);
		startStateTimer(getLivingTime());
	}

	offLiveFlag(LIVE_FLAG_UNK10);
	mState = FRUIT_STATE_ROTTING;

	return 1;
}

void TResetFruit::touchActor(THitActor* actor)
{
	// frame-size pad: the retail frame is 0x10 bytes larger than the code needs
	char framePad_16_touchActor[0x10];
	(void)framePad_16_touchActor;
	if (!isState(STATE_APPEARING) && !isState(STATE_BREAKING)
	    && !isState(FRUIT_STATE_WAITING)
	    && !isState(STATE_WAITING_TO_APPEAR)) {
		TMapObjBall::touchActor(actor);

		if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000) && !isState(STATE_NORMAL)
		    && !checkLiveFlag(LIVE_FLAG_UNK10) && !isStateTimerEngaged()) {
			onMapObjFlag(MAP_OBJ_FLAG_HAS_STATE_TIMER);
			startStateTimer(getLivingTime());
		}

		offLiveFlag(LIVE_FLAG_UNK10);
		mState = FRUIT_STATE_ROTTING;
	}
}

void TResetFruit::touchGround(JGeometry::TVec3<f32>* position)
{
	// frame-size pad: the retail frame is 0x10 bytes larger than the code needs
	char framePad_16_touchGround[0x10];
	(void)framePad_16_touchGround;
	// The `? true : false` is what makes MWCC materialise the bool in r0.
	if (mGroundPlane->mBGType == 0x800 ? true : false) {
		mState = FRUIT_STATE_ROTTING;
		TResetFruit_hideAndWait(this);
		position->x = mPosition.x;
		position->y = mPosition.y;
		position->z = mPosition.z;
	} else {
		TMapObjBall::touchGround(position);
	}
}

void TResetFruit::makeObjLiving()
{
	if (!isStateTimerEngaged()) {
		onMapObjFlag(MAP_OBJ_FLAG_HAS_STATE_TIMER);
		startStateTimer(getLivingTime());
	}

	offLiveFlag(LIVE_FLAG_UNK10);
	mState = FRUIT_STATE_ROTTING;
}

void TResetFruit::pick(THitActor*) { }

void TResetFruit::kicked()
{
	if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK40) && !isState(STATE_HOLDING)
	    && (*gpMarioSpeedY) >= 0.0f) {
		JGeometry::TVec3<f32> velocity = mVelocity;
		f32 min = mMapObjData->mPhysical->unk4->unkC;

		if (fabsf(velocity.x) < min && fabsf(velocity.z) < min) {
			if (!checkLiveFlag(LIVE_FLAG_AIRBORNE) || mScaling.x > 130.0f
			    || mScaling.x > 0.0f) {
				// TODO: the two-way bounce test is not recovered.
			} else {
				velocity.zero();
			}
		}

		if (velocity.y == 0.0f) {
			mVelocity.y = unk178;
		} else {
			mVelocity.y = unk160 * velocity.y - unk174 * (*gpMarioSpeedY);
		}

		mVelocity.x += unk170 * (*gpMarioSpeedX);
		mVelocity.z += unk170 * (*gpMarioSpeedZ);

		if (fabsf(mVelocity.x) < min && fabsf(mVelocity.z) < min) {
			mVelocity.x = 2.0f * (rand() * (1.0f / 32768.0f)) - 1.0f;
			mVelocity.z = 2.0f * (rand() * (1.0f / 32768.0f)) - 1.0f;
		}

		unk194 = 10;
		offLiveFlag(LIVE_FLAG_UNK10);

		SMS_GetMarioHitActor()->receiveMessage(this, 0xE);

		if (gpMSound->gateCheck(0x194F))
			MSoundSESystem::MSoundSE::startSoundActor(0x194F, &mPosition, 0,
			                                         nullptr, 0, 4);
	}
}

// UNUSED in the map: these three were always inlined, so the linker never
// emitted a copy of them. Placed where the map's symbol order puts them.
void TResetFruit::living() { }
void TResetFruit::waitEffect() { }
void TResetFruit::rotting() { }


void TResetFruit::breaking()
{
	Mtx mtx;
	char framePad_24_breaking[0x18];
	(void)framePad_24_breaking;
	PSMTXScale(&mtx[0], 1.0f, mBreakingScaleSpeed, 1.0f);
	J3DModel* model = getModel();
	MtxPtr anmMtx   = model->getAnmMtx(0);
	concatOnlyRotFromLeft(&mtx[0], anmMtx, anmMtx);

	mScaling.y *= mBreakingScaleSpeed;
	anmMtx[1][3] = mPosition.y + mBodyRadius * mScaling.y;

	if (mScaling.y < 0.2f) {
		mPosition.y = mPosition.y + mBodyRadius * 0.5f;
		mScaling.set(mInitialScaling);
		emitAndScale(0xE5, 0, &mPosition);

		if (gpMSound->gateCheck(0x387D))
			MSoundSESystem::MSoundSE::startSoundActor(0x387D, &mPosition, 0,
			                                         nullptr, 0, 4);

		startStateTimer(0xF0);
		sleep();
		mState = FRUIT_STATE_WAITTOSEE;
	}
}

void TResetFruit::appearing()
{
	Mtx mtx;
	char framePad_20_appearing[0x14];
	(void)framePad_20_appearing;
	PSMTXScale(&mtx[0], mScaleUpSpeed, mScaleUpSpeed, mScaleUpSpeed);
	J3DModel* model = getModel();
	MtxPtr anmMtx   = model->getAnmMtx(0);
	concatOnlyRotFromLeft(&mtx[0], anmMtx, anmMtx);

	mScaling.y *= mScaleUpSpeed;
	mScaledBodyRadius = mBodyRadius * mScaling.y;
	anmMtx[1][3] = mPosition.y + mBodyRadius * mScaling.y;

	if (mScaling.y >= mInitialScaling.y) {
		mScaling.set(mInitialScaling);
		getModel()->calc();
		offHitFlag(HIT_FLAG_NO_COLLISION);
		makeObjAppeared();
		mState = STATE_NORMAL;
	}
}

void TResetFruit::control()
{
	// The case numbers below are read off the retail jump table at
	// @4191 (.data:0x1F0), which maps state -> body as
	//   0 -> break          1 -> .L_801DA2D0 (collision walk)
	//   2,3 -> .L_801DA640  6 -> .L_801DA5BC
	//  11 -> .L_801DA438   12 -> .L_801DA730  13 -> .L_801DA7BC
	switch (mState) {
	case 0:
		break;
	case 1: {
		// Walk our own collision list and re-run the normal-state logic on
		// anything touching us, skipping the states that own themselves.
		offHitFlag(HIT_FLAG_NO_COLLISION);
		for (int i = 0; i < (int) mColCount; i++) {
			if (isState(STATE_APPEARING) || isState(STATE_BREAKING)
			    || isState(FRUIT_STATE_WAITING)
			    || isState(STATE_WAITING_TO_APPEAR)) {
				continue;
			}
			TMapObjBall::touchActor(mCollisions[i]);
			if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000)
			    && !isState(STATE_NORMAL) && !checkLiveFlag(LIVE_FLAG_UNK10)
			    && !isStateTimerEngaged()) {
				onMapObjFlag(MAP_OBJ_FLAG_HAS_STATE_TIMER);
				// Virtual slot 0x164 in __vt__11TResetFruit is
				// getLivingTime() const.
				startStateTimer(getLivingTime());
			}
			// rlwinm r3,r3,0,28,26 clears LIVE_FLAG_UNK10.
			mLiveFlag &= ~(LIVE_FLAG_UNK8 | LIVE_FLAG_UNK10
			               | LIVE_FLAG_UNK20);
			mState = FRUIT_STATE_ROTTING;
		}
		if (mGroundPlane->mActor) {
			// Virtual slot 0x1EC is calcCurrentMtx().
			calcCurrentMtx();
		}
		break;
	}
	case 2:
	case 3: {
		TMapObjGeneral::control();
		if (unk194 != 0)
			unk194--;
		if (isState(STATE_HOLDING)) {
			// TODO: taking-matrix fix-up.
		} else {
			if (mVelocity.squared() > 3.814697265625e-06f
			    && mGroundPlane->mActor)
				kill();
		}
		if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000) && !isStateTimerEngaged()) {
			if (mHolder != nullptr) {
				mHolder->receiveMessage(this, 8);
				mHolder->mHeldObject = 0;
				mHolder        = nullptr;
			}
			mVelocity.z = 0.0f;
			mVelocity.y = 0.0f;
			mVelocity.x = 0.0f;
			mState      = FRUIT_STATE_WAITING;
		}
		break;
	}
	case 6: {
		TMapObjBall::control();
		if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000) && !isStateTimerEngaged()) {
			if (mHolder != nullptr) {
				mHolder->receiveMessage(this, 8);
				mHolder->mHeldObject = 0;
				mHolder        = nullptr;
			}
			mVelocity.z = 0.0f;
			mVelocity.y = 0.0f;
			mVelocity.x = 0.0f;
			mState      = FRUIT_STATE_WAITING;
		}
		break;
	}
	// Source order here must stay WAITTOSEE (0xD) before ROTTING (0xB):
	// MWCC emits switch bodies in source order, which is what puts the
	// .L_801DA730 body before .L_801DA7BC in the jump table.
	case FRUIT_STATE_ROTTING:
		if (isStateTimerEngaged())
			break;
		TResetFruit_hideAndWait(this);
		break;
	case FRUIT_STATE_WAITING:
		// Drop whatever is holding us, stop dead, and go back to
		// FRUIT_STATE_WAITING's own "wait then respawn" timer.
		TMapObjBall::control();
		if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000)
		    && !isStateTimerEngaged()) {
			if (mHolder != nullptr) {
				mHolder->receiveMessage(this, 8);
				mHolder->mHeldObject = 0;
				mHolder        = nullptr;
			}
			mVelocity.z = 0.0f;
			mVelocity.y = 0.0f;
			mVelocity.x = 0.0f;
			mState      = FRUIT_STATE_WAITING;
		}
		break;
	case FRUIT_STATE_WAITTOSEE:
		if (isStateTimerEngaged())
			break;
		mRottenColor.r = 0xFF;
		mRottenColor.g = 0xFF;
		mRottenColor.b = 0xFF;
		awake();
		mState = FRUIT_STATE_ROTTING;
		TResetFruit_hideAndWait(this);
		break;
	default:
		break;
	}
}

void TResetFruit::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (SMSGetMarDirector()->mMap == 7) {
		if (isState(STATE_HOLDING)
		    || mVelocity.squared() > 3.814697265625e-06f) {
			onLiveFlag(LIVE_FLAG_UNK2000);
		} else if (gpCubeArea->isInAreaCube(mPosition)) {
			if (isState(FRUIT_STATE_ROTTING)
			    && (mPosition.x != mInitialPosition.x
			        || mPosition.z != mInitialPosition.z)) {
				mState = FRUIT_STATE_ROTTING;
				makeObjDead();
				calcRootMatrix();
				getModel()->calc();
				startStateTimer(mFruitWaitTimeToAppear);
				offMapObjFlag(MAP_OBJ_FLAG_HAS_STATE_TIMER);
				mState = STATE_WAITING_TO_APPEAR;

				if (SMSGetMarDirector()->mMap == 3 && unk1A4)
					makeObjDead();
				return;
			}
		}
	}

	TMapObjGeneral::perform(cue, graphics);
}

void TResetFruit::killByTimer(int timer)
{
	startStateTimer(timer);
	onMapObjFlag(MAP_OBJ_FLAG_HAS_STATE_TIMER);
	mState = FRUIT_STATE_ROTTING;
}

void TResetFruit::makeObjAppeared()
{
	// frame-size pad: the retail frame is 8 bytes larger than the code needs
	char framePad_8_makeObjAppeared[8];
	(void)framePad_8_makeObjAppeared;
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
		makeObjDefault();

	TMapObjBase::makeObjAppeared();
	calcCurrentMtx();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	mtx[0][3]  = mPosition.x;
	mtx[1][3]  = mPosition.y + mBodyRadius;
	mtx[2][3]  = mPosition.z;

	if (isActorType(0x40000394) && mtx[1][1] > 0.0f)
		mtx[1][3] -= 50.0f * mtx[1][1];

	if (isActorType(0x40000392))
		mtx[1][3] -= 10.0f * (1.0f - mtx[1][1]);

	unkE8 = 0;

	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
		mState = FRUIT_STATE_ROTTING;
}

// TODO: the first branch's call sequence is recovered but its guard
// conditions are not fully understood.
BOOL TResetFruit::receiveMessage(THitActor* sender, u32 message)
{
	// frame-size pad: the retail frame is 0x28 bytes larger than the code needs
	char framePad_44_receiveMessage[0x2C];
	(void)framePad_44_receiveMessage;
	if (message == 0xB) {
		if (!isState(STATE_NORMAL) && !isState(STATE_HOLDING)
		    && !isState(FRUIT_STATE_ROTTING)) {
			mState = FRUIT_STATE_ROTTING;
			TResetFruit_hideAndWait(this);
			return true;
		}
		return false;
	}

	if (message == 0xD) {
		kill();
		return true;
	}

	if (!isState(STATE_NORMAL) && !isState(STATE_HOLDING)
	    && !isState(FRUIT_STATE_ROTTING)) {
		if (!isState(STATE_APPEARING) && !isState(STATE_BREAKING)
		    && !isState(FRUIT_STATE_WAITING)
		    && !isState(STATE_WAITING_TO_APPEAR)) {
			TMapObjBall::touchActor(sender);

			if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000)
			    && !isState(STATE_NORMAL)
			    && !checkLiveFlag(LIVE_FLAG_UNK10)
			    && !isStateTimerEngaged()) {
				onMapObjFlag(MAP_OBJ_FLAG_HAS_STATE_TIMER);
				startStateTimer(getLivingTime());
			}

			offLiveFlag(LIVE_FLAG_UNK10);
			mState = FRUIT_STATE_ROTTING;
		}
	}

	if (TMapObjGeneral::receiveMessage(sender, message))
		return true;

	if (message == 4 && checkMapObjFlag(MAP_OBJ_FLAG_UNK100000)) {
		hold((TTakeActor*)sender);
		return true;
	}

	if (sender->isActorType(0x80000001) && !isActorType(0x400000D0)
	    && message != 4) {
		kicked();
		return true;
	}

	if (message == 6 && isState(STATE_NORMAL))
		mState = FRUIT_STATE_ROTTING;

	return false;
}

void TResetFruit::initMapObj()
{
	TMapObjBall::initMapObj();
	SMS_InitPacket_OneTevColor(getModel(), 0, GX_TEVREG0, &mRottenColor);
}

TResetFruit::TResetFruit(const char* name)
    : TMapObjBall(name)
{
	mRottingScaleSpeed = 0.0f;
	unk1A4             = 0;
	mRottenColor.r     = 0xFF;
	mRottenColor.g     = 0xFF;
	mRottenColor.b     = 0xFF;
	mRottenColor.a     = 0xFF;
}

// TODO: the four-way name switch and the snprintf argument order are
// reconstructed from the .s; the strings themselves are recovered.
void TRandomFruit::initMapObj()
{
	s32 sel = (s32)((rand() * (1.0f / 32768.0f)) * 5.0f);

	switch (sel) {
	case 0:
		snprintf((char*)unk1A8, 0x20, "FruitCoconut");
		break;
	case 1:
		snprintf((char*)unk1A8, 0x20, "FruitDurian");
		break;
	case 2:
		snprintf((char*)unk1A8, 0x20, "FruitPapaya");
		break;
	case 3:
		snprintf((char*)unk1A8, 0x20, "FruitPine");
		break;
	case 4:
	case 5:
	default:
		snprintf((char*)unk1A8, 0x20, "FruitPine");
		break;
	}

	unkF4 = (const char*)unk1A8;

	TMapObjBall::initMapObj();
	// The retail object loads 1 here, i.e. GX_TEVREG0 -- not GX_TEVREG1.
	SMS_InitPacket_OneTevColor(getModel(), 0, GX_TEVREG0, &mRottenColor);
}

TRandomFruit::TRandomFruit(const char* name)
	: TResetFruit(name)
{
	memset(unk1A8, 0, sizeof(unk1A8));
}

void TCoverFruit::calcRootMatrix()
{
	// frame-size pad: the retail frame is 8 bytes larger than the code needs
	char framePad_8_calcRootMatrix[8];
	(void)framePad_8_calcRootMatrix;
	if (mHolder != nullptr) {
		MtxPtr mtx = mHolder->getTakingMtx();
		PSMTXCopy(mtx, getModel()->getBaseTRMtx());
		mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);
	} else {
		// The declaration order matters: MWCC hands out f31..f26 to these
		// locals in declaration order.
		f32 x    = mPosition.x;
		f32 y    = mPosition.y - mYOffset;
		f32 z    = mPosition.z;
		f32 rotX = mRotation.x;
		f32 rotY = mRotation.y;
		f32 rotZ = mRotation.z;
		J3DModel* model = getModel();
		MtxPtr mtx      = model->getBaseTRMtx();
		s16 rotX16      = rotX * (65536.0f / 360.0f);
		s16 rotY16      = rotY * (65536.0f / 360.0f);
		s16 rotZ16      = rotZ * (65536.0f / 360.0f);
		MsMtxSetXYZRPH(mtx, x, y, z, rotX16, rotY16, rotZ16);
	}
	getModel()->setBaseScale(mScaling);
}

BOOL TCoverFruit::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->isActorType(0x08000083) && message == HIT_MESSAGE_TAKE) {
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mHolder = (TTakeActor*)sender;
		return true;
	}

	if (message == HIT_MESSAGE_UNKB) {
		kill();
		TFlagManager::smInstance->setBool(true, 0x1038B);
		return true;
	}

	return false;
}

void TCoverFruit::loadAfter()
{
    TMapObjBase::loadAfter();
    if (TFlagManager::smInstance->getBool(0x1038B))
        makeObjDead();
}

void TBigWatermelon::touchWaterSurface()
{
	// frame-size pad: the retail frame is 8 bytes larger than the code needs
	char framePad_8_touchWaterSurface[8];
	(void)framePad_8_touchWaterSurface;
	emitColumnWater();
	if (gpMSound->gateCheck(0x3875))
		MSoundSESystem::MSoundSE::startSoundActor(0x3875, &mPosition, 0,
		                                         nullptr, 0, 4);
	kill();
}

void TBigWatermelon::touchWall(JGeometry::TVec3<f32>* position,
	                           TBGWallCheckRecord* record)
{
	TMapObjBall::touchWall(position, record);
}

void TBigWatermelon::rebound(JGeometry::TVec3<f32>* position)
{
	if (isState(FRUIT_STATE_WAITING)) {
		kill();
		position->x = mPosition.x;
		position->y = mPosition.y;
		position->z = mPosition.z;
		return;
	}

	calcReflectingVelocity(
	    mGroundPlane, mMapObjData->mPhysical->unk4->unk4, &mVelocity);
	position->y = mGroundHeight;
	onLiveFlag(LIVE_FLAG_AIRBORNE);

	if (isActorType(0x400000D0)) {
		if (mScaling.y >= 5.0f) {
			f32 speed = fabsf(mGroundPlane->mNormal.y);
			if (gpMSound->gateCheck(0x3889))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x3889, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
		} else {
			f32 speed = fabsf(mGroundPlane->mNormal.y);
			if (gpMSound->gateCheck(0x388C))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x388C, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
		}
	} else {
		u32 sound = mMapObjData->mSound->unk4->unk0[4];
		if (gpMSound->gateCheck(sound))
			MSoundSESystem::MSoundSE::startSoundActorWithInfo(
			    sound, &mPosition, &mVelocity, 0.0f, 0, 0, nullptr, 0, 4);
	}

	if (isState(FRUIT_STATE_ROTTING))
		mState = FRUIT_STATE_WAITING;
}

void TBigWatermelon::touchGround(JGeometry::TVec3<f32>* position)
{
	TMapObjBall::touchGround(position);
}

void TBigWatermelon::touchActor(THitActor* actor)
{
	// TODO: the 0x0800083 distance test and the TPoiHana branch need
	// helpers that are not in the tree yet.
	if (unk194 == 0 && !isState(STATE_HOLDING) && !isHideObj(actor)
	    && actor->isActorType(0x08000083) && !actor->isActorType(0x400000CA)
	    && !actor->isActorType(0x400000CC)) {
		if (actor->isActorType(0x80000001) && !isActorType(0x400000D0)
		    && *gpMarioSpeedY != 0.0f) {
			kill();
			return;
		}
		boundByActor(actor);
	}
}

void TBigWatermelon::kill()
{
	emitAndScale(0x5D, 0, &mPosition);
	emitAndScale(0x5E, 0, &mPosition);
	emitAndScale(0x5F, 0, &mPosition);
	emitAndScale(0x6B, 0, &mPosition, JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
	emitAndScale(0x6C, 0, &mPosition, JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));

	mWaterEmitInfo->mPos.value.x = mPosition.x;
	mWaterEmitInfo->mPos.value.y = mPosition.y;
	mWaterEmitInfo->mPos.value.z = mPosition.z;
	gpModelWaterManager->emitRequest(*mWaterEmitInfo);

	if (gpMSound->gateCheck(0x38A3))
		MSoundSESystem::MSoundSE::startSoundActor(0x38A3, &mPosition, 0,
		                                         nullptr, 0, 4);

	if (unk19C < 10) {
		TMapObjBase* obj = ((TMapObjBaseManager*)gpItemManager)->makeObjAppear(
		    mPosition.x, mPosition.y, mPosition.z, 0x200E, true);
		if (obj != nullptr) {
			obj->mVelocity.x = 0.0f;
			obj->mVelocity.y = 25.0f;
			obj->mVelocity.z = 0.0f;
			obj->offLiveFlag(LIVE_FLAG_UNK10);
			unk19C++;
		}
	}

	TMapObjGeneral::kill();
}

void TBigWatermelon::appearing()
{
	char framePad_24_appearing[0x18];
	(void)framePad_24_appearing;
	TMapObjGeneral::appearing();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	calcRootMatrix();
	getModel()->calc();

	mtx[1][3] = mPosition.y + mBodyRadius * (mScaling.y / mInitialScaling.y);
	mScaledBodyRadius = 50.0f * mScaling.x;
	mDamageRadius     = 50.0f * mScaling.x;
	calcEntryRadius();

	if (isState(STATE_NORMAL)) {
		mActorType   = 0x400000D0;
		mAttackRadius = 50.0f * mScaling.x;
		calcEntryRadius();
	} else {
		mActorType   = 0x400000DB;
		mAttackRadius = 0.0f;
		calcEntryRadius();
	}
}

void TBigWatermelon::control()
{
	TMapObjGeneral::control();

	if (unk194 != 0)
		unk194--;

	if (isState(STATE_HOLDING)) {
		// TODO: taking-matrix fix-up.
	} else {
		if (mVelocity.squared() > 3.814697265625e-06f && mGroundPlane->mActor)
			kill();
	}

	switch (mState) {
	case FRUIT_STATE_WAITTOSEE:
		if (!isStateTimerEngaged()) {
			JGeometry::TVec3<f32> one(1.0f, 1.0f, 1.0f);
			emitAndScale(0x6B, 0, &mPosition, one);
			emitAndScale(0x6C, 0, &mPosition, one);
			startStateTimer(0x1E);
		}
		if (animIsFinished())
			kill();
		break;
	default:
		break;
	}
}

// TODO: the object-name comparison and the demo-camera call need field
// and parameter names that are not in the tree yet.
void TBigWatermelon::startEvent()
{
	// The stage-event name takes the scripted-camera path; everything else
	// scatters ten watermelon items around Mario.
	if (strcmp(getName(), kStageName) == 0) {
		mPosition.x = -4660.0f;
		mPosition.y = 1300.0f;
		mPosition.z = 13600.0f;

		// rlwinm r0,r0,0,24,22 clears MAP_OBJ_FLAG_UNK100.
		unkF8 &= ~MAP_OBJ_FLAG_UNK100;
		onLiveFlag(LIVE_FLAG_UNK10);
		mVelocity.z = 0.0f;
		mVelocity.y = 0.0f;
		mVelocity.x = 0.0f;
		onLiveFlag(LIVE_FLAG_UNK10);

		startAnim(7);

		JDrama::TFlagT<u16> flag;
		gpMarDirector->fireStartDemoCamera(kCamName, nullptr, -1, 0.0f,
		                                   true, nullptr, 0, nullptr, flag);
		gpItemManager->makeShineAppearWithDemoOffset(kShineName, kDemoName,
		                                             0.0f, 0.0f, 0.0f);

		mStateTimer = 0x17C;
		mState       = 0xD;
		return;
	}

	for (int i = 0; i < 10; i++) {
		// The spawned item is a watermelon, so its post-0x138 fields are
		// reachable; makeObjAppear() only hands back the base pointer.
		TMapObjBall* item = (TMapObjBall*) gpItemManager->makeObjAppear(
		    gpMarioPos->x, gpMarioPos->y, gpMarioPos->z, 0x200000E, true);
		if (item != nullptr) {
			// Three independent rand() draws, each folded into
			// [-0.5,0.5] before the launch-speed scaling.
			f32 a = 3.8146973e-05f * (f32) rand();
			f32 b = 3.8146973e-05f * (f32) rand();
			f32 c = 3.8146973e-05f * (f32) rand();

			item->mVelocity.x = 20.0f * (c - 0.5f);
			item->mVelocity.y = 20.0f * b + 20.0f;
			item->mVelocity.z = 20.0f * (a - 0.5f);

			item->mLiveFlag &= ~LIVE_FLAG_UNK10;
			item->unk14C = 0x3C0;
		}
	}

	// Virtual slot 0x104 in __vt__14TBigWatermelon is makeObjDead().
	makeObjDead();
}

void TBigWatermelon::checkWallCollision(JGeometry::TVec3<f32>* position)
{
	TMapObjGeneral::checkWallCollision(position);
}

BOOL TBigWatermelon::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->isActorType(0x80000001)) {
		boundByActor(sender);
		return true;
	}

	if (TMapObjGeneral::receiveMessage(sender, message))
		return true;

	if (message == 4 && checkMapObjFlag(MAP_OBJ_FLAG_UNK100000)) {
		hold((TTakeActor*)sender);
		return true;
	}

	if (sender->isActorType(0x80000001) && !isActorType(0x400000D0)
	    && message != 4) {
		kicked();
		return true;
	}

	return false;
}

void TBigWatermelon::loadAfter()
{
	TMapObjGeneral::loadAfter();
	TShine* shine = static_cast<TShine*>(
	    JDrama::TNameRefGen::search("シャイン（お化けスイカ用）"));
	shine->mPosition.set(-4659.0f, 460.0f, 13620.0f);
}

void TBigWatermelon::initMapObj()
{
	TMapObjBall::initMapObj();

	if (!gParticleFlagLoaded[0x5D]) {
		gpResourceManager->load(
		    "/scene/mapObj/watermelon_bomb.jpa", 0x5D);
		gParticleFlagLoaded[0x5D] = true;
	}
	if (!gParticleFlagLoaded[0x5E]) {
		gpResourceManager->load(
		    "/scene/mapObj/watermelon_bomb_a.jpa", 0x5E);
		gParticleFlagLoaded[0x5E] = true;
	}
	if (!gParticleFlagLoaded[0x5F]) {
		gpResourceManager->load(
		    "/scene/mapObj/watermelon_bomb_b.jpa", 0x5F);
		gParticleFlagLoaded[0x5F] = true;
	}
	if (!gParticleFlagLoaded[0x6B]) {
		gpResourceManager->load(
		    "/scene/mapObj/watermelon_shrink_a.jpa", 0x6B);
		gParticleFlagLoaded[0x6B] = true;
	}
	if (!gParticleFlagLoaded[0x6C]) {
		gpResourceManager->load(
		    "/scene/mapObj/watermelon_shrink_b.jpa", 0x6C);
		gParticleFlagLoaded[0x6C] = true;
	}

	mWaterEmitInfo = new TWaterEmitInfo("/watermelon.prm");
}

TBigWatermelon::TBigWatermelon(const char* name)
	: TMapObjBall(name)
{
	mWaterEmitInfo = nullptr;
	unk19C         = 0;
	unk1A0         = 0.0f;
}

