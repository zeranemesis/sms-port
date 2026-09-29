
#include <MoveBG/MapObjBall.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
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

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

u32 TResetFruit::mFruitLivingTime       = 0x3840;
f32 TResetFruit::mScaleUpSpeed           = 1.05f;
f32 TResetFruit::mBreakingScaleSpeed     = 0.96f;
u32 TResetFruit::mFruitWaitTimeToAppear  = 0x168;

// States that TMapObjGeneral does not name yet.
enum {
	FRUIT_STATE_ROTTING = 0xB,
	FRUIT_STATE_WAITING = 0xC,
	FRUIT_STATE_WAITTOSEE = 0xD,
};

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
	f->offMapObjFlag(0xC000);
	f->setState(FRUIT_STATE_WAITTOSEE);

	if (SMSGetMarDirector()->mMap == 3 && f->unk1A4)
		f->makeObjDead();
}

TRandomFruit::~TRandomFruit() { }

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
	// TODO: the per-wall loop needs TBGWallCheckRecord's wall array,
	// which is not modelled yet. Structure only.
}

void TMapObjBall::touchPollution() { kill(); }

void TMapObjBall::touchWaterSurface() { kill(); }

void TMapObjBall::rebound(JGeometry::TVec3<f32>* position)
{
	calcReflectingVelocity(
	    mGroundPlane, mMapObjData->mPhysical->unk4->unk4, &mVelocity);
	position->y = mGroundHeight;
	onLiveFlag(LIVE_FLAG_AIRBORNE);

	if (isActorType(0x400000D0)) {
		if (mScaling.x <= 5.0f) {
			f32 speed = fabsf(mGroundPlane->unk38);
			if (gpMSound->gateCheck(0x3889))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x3889, &mPosition, nullptr, speed, 0, 0, nullptr, 4);
		} else {
			f32 speed = fabsf(mGroundPlane->unk38);
			if (gpMSound->gateCheck(0x388C))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x388C, &mPosition, nullptr, speed, 0, 0, nullptr, 4);
		}
	} else {
		u32 sound = mMapObjData->mSound->unk4.unk0[4];
		if (gpMSound->gateCheck(sound))
			MSoundSESystem::MSoundSE::startSoundActorWithInfo(
			    sound, nullptr, &mPosition, &mVelocity, 0, 0, 0, nullptr, 4);
	}
}

void TMapObjBall::touchGround(JGeometry::TVec3<f32>* position)
{
	f32 speed = fabsf(JGeometry::TUtil<f32>::sqrt(
	    mVelocity.x * mVelocity.x + mVelocity.y * mVelocity.y
	    + mVelocity.z * mVelocity.z));

	if (speed > 0.05f && isActorType(0x400000D0)) {
		if (mScaling.x <= 5.0f) {
			if (gpMSound->gateCheck(0x308A))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x308A, &mPosition, nullptr, speed, 0, 0, nullptr, 4);
		} else {
			if (gpMSound->gateCheck(0x308B))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x308B, &mPosition, nullptr, speed, 0, 0, nullptr, 4);
		}
	}

	// TODO: 0x100/0x101/0x102..0x105/0x4104 are TBGCheckData attribute
	// values that are not named anywhere in the tree yet.
	u16 attr = mGroundPlane->unk0;
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
		mVelocity.x += unk180 * mGroundPlane->unk34;
		mVelocity.z += unk180 * mGroundPlane->unk3C;
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
	f32 speed = JGeometry::TUtil<f32>::sqrt(velocity.x * velocity.x
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
	onLiveFlag(LIVE_FLAG_UNK10);

	SMS_GetMarioHitActor()->moveRequest(this, 0xE);

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
		onLiveFlag(LIVE_FLAG_UNK10);
	}

	return 1;
}

void TMapObjBall::boundByActor(THitActor* actor)
{
	// TODO: 1364 bytes of roll-over / fling handling; not reconstructed.
}

void TMapObjBall::touchActor(THitActor* actor)
{
	if (unk194 == 0 && !isState(STATE_HOLDING) && !isHideObj(actor)
	    && actor->isActorType(0x08000083) && !actor->isActorType(0x400000CA)
	    && !actor->isActorType(0x400000CC)) {
		if (actor->isActorType(0x80000001) && isActorType(0x400000D0)
		    && *gpMarioSpeedY != 0.0f) {
			kicked();
			return;
		}
		boundByActor(actor);
	}
}

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
		unk138      = record.unk50;
		position->x = record.mCenter.x;
		position->z = record.mCenter.z;
		touchWall(position, &record);
	} else {
		unk138 = nullptr;
	}
}

void TMapObjBall::makeObjDefault()
{
	TMapObjBase::makeObjDefault();
	MtxPtr mtx  = getModel()->getAnmMtx(0);
	mtx[0][3]   = mPosition.x;
	mtx[1][3]   = mPosition.y + mBodyRadius;
	mtx[2][3]   = mPosition.z;
}

void TMapObjBall::makeObjAppeared()
{
	TMapObjBase::makeObjAppeared();
	calcCurrentMtx();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	mtx[0][3]  = mPosition.x;
	mtx[1][3]  = mPosition.y + mBodyRadius;
	mtx[2][3]  = mPosition.z;

	if (isActorType(0x40000394) && mtx[1][1] > 0.0f)
		mtx[1][3] -= 50.0f * mtx[1][1];

	if (isActorType(0x40000392))
		mtx[1][3] -= 10.0f * (0.25f - mtx[1][1]);

	unkE8 = 0;
}

void TMapObjBall::control()
{
	TMapObjGeneral::control();

	if (unk194 != 0)
		unk194--;

	if (isState(STATE_HOLDING)) {
		// TODO: the taking-matrix fix-up is not reconstructed.
	} else {
		f32 speed = JGeometry::TUtil<f32>::sqrt(
		    mVelocity.x * mVelocity.x + mVelocity.y * mVelocity.y
		    + mVelocity.z * mVelocity.z);
		if (!(speed <= 3.814697265625e-06f) || mGroundPlane->unk44)
			kill();
	}
}

BOOL TMapObjBall::receiveMessage(THitActor* sender, u32 message)
{
	if (TMapObjGeneral::receiveMessage(sender, message))
		return true;

	if (message == 4 && checkMapObjFlag(MAP_OBJ_FLAG_UNK80)) {
		hold(sender);
		return true;
	}

	if (sender->isActorType(0x80000001) && isActorType(0x400000D0)
	    && message != 4) {
		kicked();
		return true;
	}

	return false;
}

// TODO: only the 0x400000D0 case is recovered; the other four parameter
// tables in the .s are still unidentified.
void TMapObjBall::initMapObj()
{
	TMapObjGeneral::initMapObj();

	mInitialScaling = mScaling;

	if (mActorType == 0x400000D0) {
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
		unk184 = 0.0f;
		unk188 = 0.0f;
		mBodyRadius = 50.0f * mScaling.x;
		unk18C     = mBodyRadius / 3.0f;
	}

	if (mActorType == 0x40000393) {
		mBodyRadius = 45.0f * mScaling.x;
		unk190      = mBodyRadius;
	}
	if (mActorType == 0x40000390 || mActorType == 0x40000391) {
		mBodyRadius = 40.0f * mScaling.x;
		unk190      = 20.0f;
	}
	if (mActorType == 0x40000392)
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
	if (SMSGetMarDirector()->mMap == 4) {
		mGroundHeight = gpMap->checkGround(position->x,
		                                   position->y + 200.0f,
		                                   position->z, &mGroundPlane);
		mGroundHeight += 1.0f;
		if (position->y >= mGroundHeight) {
			touchGround(position);
		} else {
			onLiveFlag(LIVE_FLAG_AIRBORNE);
		}
	} else if (SMSGetMarDirector()->mMap == 7) {
		mGroundHeight = gpMap->checkGround(position->x,
		                                   position->y + mVelocity.y,
		                                   position->z, &mGroundPlane);
		if (mGroundPlane->unk0 == 0x801 || mGroundPlane->unk0 == 0x203) {
			mGroundHeight = gpMap->checkGroundExactY(
			    position->x, mGroundHeight - 200.0f, position->z,
			    &mGroundPlane);
		}
		mGroundHeight += 1.0f;
		if (position->y >= mGroundHeight) {
			touchGround(position);
		} else {
			onLiveFlag(LIVE_FLAG_AIRBORNE);
		}
	} else {
		TMapObjGeneral::checkGroundCollision(position);
	}
}

void TResetFruit::waitingToAppear()
{
	if (SMSGetMarDirector()->mMap == 3 && unk1A4)
		makeObjDead();

	if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK20) && !isStateTimerEngaged()
	    && getHitObjNumMax() == 0) {
		onMapObjFlag(MAP_OBJ_FLAG_UNK4);
		makeObjAppeared();

		Mtx mtx;
		PSMTXScale(&mtx, 0.2f, 0.2f, 0.2f);
		concatOnlyRotFromLeft(&mtx, getModel()->getAnmMtx(0),
		                      getModel()->getAnmMtx(0));

		mScaling.x = 0.2f;
		onHitFlag(HIT_FLAG_NO_TAKING);
		mState = STATE_APPEARING;

		if (gpMSound->gateCheck(0x3802))
			MSoundSESystem::MSoundSE::startSoundActor(0x3802, &mPosition, 0,
			                                         nullptr, 0, 4);
	}
}

void TResetFruit::makeObjWaitingToAppear()
{
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
	if (mVelocity.length() <= 10.0f) {
		TMapObjGeneral::hold(holder);
	}
	mVelocity.zero();

	onLiveFlag(LIVE_FLAG_UNK10);

	if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK20) && !isStateTimerEngaged()) {
		onMapObjFlag(MAP_OBJ_FLAG_UNK4);
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
		onLiveFlag(LIVE_FLAG_UNK10);
	}

	if (!isStateTimerEngaged()) {
		onMapObjFlag(MAP_OBJ_FLAG_UNK4);
		startStateTimer(getLivingTime());
	}

	onLiveFlag(LIVE_FLAG_UNK10);
	mState = FRUIT_STATE_ROTTING;

	return 1;
}

void TResetFruit::touchActor(THitActor* actor)
{
	if (!isState(STATE_APPEARING) && !isState(STATE_BREAKING)
	    && !isState(FRUIT_STATE_WAITING)
	    && !isState(STATE_WAITING_TO_APPEAR)) {
		TMapObjBall::touchActor(actor);

		if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK20) && !isState(STATE_NORMAL)
		    && !checkLiveFlag(LIVE_FLAG_UNK20) && !isStateTimerEngaged()) {
			onMapObjFlag(MAP_OBJ_FLAG_UNK4);
			startStateTimer(getLivingTime());
		}

		onLiveFlag(LIVE_FLAG_UNK10);
		mState = FRUIT_STATE_ROTTING;
	}
}

void TResetFruit::touchGround(JGeometry::TVec3<f32>* position)
{
	if (mGroundPlane->unk0 == 0x800) {
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
		onMapObjFlag(MAP_OBJ_FLAG_UNK4);
		startStateTimer(getLivingTime());
	}

	onLiveFlag(LIVE_FLAG_UNK10);
	mState = FRUIT_STATE_ROTTING;
}

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
		onLiveFlag(LIVE_FLAG_UNK10);

		SMS_GetMarioHitActor()->moveRequest(this, 0xE);

		if (gpMSound->gateCheck(0x194F))
			MSoundSESystem::MSoundSE::startSoundActor(0x194F, &mPosition, 0,
			                                         nullptr, 0, 4);
	}
}

void TResetFruit::breaking()
{
	Mtx mtx;
	PSMTXScale(&mtx, 1.0f, mBreakingScaleSpeed, 1.0f);
	concatOnlyRotFromLeft(&mtx, getModel()->getAnmMtx(0),
	                      getModel()->getAnmMtx(0));

	mScaling.x *= mBreakingScaleSpeed;
	getModel()->getAnmMtx(0)[1][3] = mPosition.y + mBodyRadius * mScaling.x;

	if (mScaling.x < 0.2f) {
		mPosition.y = mPosition.y + mBodyRadius * 0.5f;
		mScaling    = mInitialScaling;
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
	PSMTXScale(&mtx, mScaleUpSpeed, mScaleUpSpeed, mScaleUpSpeed);
	concatOnlyRotFromLeft(&mtx, getModel()->getAnmMtx(0),
	                      getModel()->getAnmMtx(0));

	mScaling.x  *= mScaleUpSpeed;
	mAttackHeight = mBodyRadius * mScaling.x;
	getModel()->getAnmMtx(0)[1][3] = mPosition.y + mBodyRadius * mScaling.x;

	if (mScaling.x >= mInitialScaling.x) {
		mScaling = mInitialScaling;
		getModel()->update();
		offHitFlag(HIT_FLAG_NO_TAKING);
		appear();
		mState = STATE_NORMAL;
	}
}

// TODO: only the 1/2/5/0xA/0xB/0xC cases are identified; the state
// 0 body (the collision-list walk) is missing.
void TResetFruit::control()
{
	switch (mState) {
	case 0:
		break;
	case 1:
	case 2: {
		TMapObjGeneral::control();
		if (unk194 != 0)
			unk194--;
		if (isState(STATE_HOLDING)) {
			// TODO: taking-matrix fix-up.
		} else {
			f32 speed = JGeometry::TUtil<f32>::sqrt(
			    mVelocity.x * mVelocity.x + mVelocity.y * mVelocity.y
			    + mVelocity.z * mVelocity.z);
			if (!(speed <= 3.814697265625e-06f)
			    || mGroundPlane->unk44)
				kill();
		}
		if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK20) && !isStateTimerEngaged()) {
			if (mHolder != nullptr) {
				mHolder->moveRequest(this, 8);
				mHolder->unk6C = 0;
				mHolder        = nullptr;
			}
			mVelocity.z = 0.0f;
			mVelocity.y = 0.0f;
			mVelocity.x = 0.0f;
			mState      = FRUIT_STATE_WAITING;
		}
		break;
	}
	case 5: {
		TMapObjBall::control();
		if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK20) && !isStateTimerEngaged()) {
			if (mHolder != nullptr) {
				mHolder->moveRequest(this, 8);
				mHolder->unk6C = 0;
				mHolder        = nullptr;
			}
			mVelocity.z = 0.0f;
			mVelocity.y = 0.0f;
			mVelocity.x = 0.0f;
			mState      = FRUIT_STATE_WAITING;
		}
		break;
	}
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
	case FRUIT_STATE_ROTTING:
		if (isStateTimerEngaged())
			break;
		TResetFruit_hideAndWait(this);
		break;
	default:
		break;
	}
}

void TResetFruit::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (SMSGetMarDirector()->mMap == 7) {
		f32 speed = JGeometry::TUtil<f32>::sqrt(
		    mVelocity.x * mVelocity.x + mVelocity.y * mVelocity.y
		    + mVelocity.z * mVelocity.z);
		if (isState(STATE_HOLDING) || speed > 3.814697265625e-06f) {
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
				offMapObjFlag(0xC000);
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
	onMapObjFlag(MAP_OBJ_FLAG_UNK4);
	mState = FRUIT_STATE_ROTTING;
}

void TResetFruit::makeObjAppeared()
{
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK20))
		rotting();

	TMapObjBase::makeObjAppeared();
	calcCurrentMtx();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	mtx[0][3]  = mPosition.x;
	mtx[1][3]  = mPosition.y + mBodyRadius;
	mtx[2][3]  = mPosition.z;

	if (isActorType(0x40000394) && mtx[1][1] > 0.0f)
		mtx[1][3] -= 50.0f * mtx[1][1];

	if (isActorType(0x40000392))
		mtx[1][3] -= 10.0f * (0.25f - mtx[1][1]);

	unkE8 = 0;

	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK20))
		mState = FRUIT_STATE_ROTTING;
}

u32 TResetFruit::getLivingTime() const { return mFruitLivingTime; }

// TODO: the first branch's call sequence is recovered but its guard
// conditions are not fully understood.
BOOL TResetFruit::receiveMessage(THitActor* sender, u32 message)
{
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

			if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK20)
			    && !isState(STATE_NORMAL)
			    && !checkLiveFlag(LIVE_FLAG_UNK20)
			    && !isStateTimerEngaged()) {
				onMapObjFlag(MAP_OBJ_FLAG_UNK4);
				startStateTimer(getLivingTime());
			}

			onLiveFlag(LIVE_FLAG_UNK10);
			mState = FRUIT_STATE_ROTTING;
		}
	}

	if (TMapObjGeneral::receiveMessage(sender, message))
		return true;

	if (message == 4 && checkMapObjFlag(MAP_OBJ_FLAG_UNK80)) {
		hold(sender);
		return true;
	}

	if (sender->isActorType(0x80000001) && isActorType(0x400000D0)
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

TResetFruit::~TResetFruit() { }

// TODO: the four-way name switch and the snprintf argument order are
// reconstructed from the .s; the strings themselves are recovered.
void TRandomFruit::initMapObj()
{
	f32 r = mBreakingScaleSpeed * (rand() * (1.0f / 32768.0f)) * 5.0f;
	u32 sel = (u32)r;

	switch (sel) {
	case 0:
		snprintf(unk1A8, 0x20, "FruitCoconut");
		break;
	case 1:
		snprintf(unk1A8, 0x20, "FruitDurian");
		break;
	case 2:
		snprintf(unk1A8, 0x20, "FruitPapaya");
		break;
	case 4:
		snprintf(unk1A8, 0x20, "FruitPine");
		break;
	default:
		snprintf(unk1A8, 0x20, "FruitPine");
		break;
	}

	unkF4 = unk1A8;

	TMapObjBall::initMapObj();
	SMS_InitPacket_OneTevColor(getModel(), 0, GX_TEVREG1, &mRottenColor);
}

TRandomFruit::TRandomFruit(const char* name)
	: TResetFruit(name)
{
	memset(unk1A8, 0, sizeof(unk1A8));
}

TRandomFruit::~TRandomFruit() { }

void TCoverFruit::calcRootMatrix()
{
	if (mHolder != nullptr) {
		MtxPtr mtx = mHolder->getTakingMtx();
		PSMTXCopy(mtx, getModel()->getBaseTRMtx());
		mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);
	} else {
		f32 rotZ = mRotation.z;
		f32 y    = mPosition.y - mYOffset;
		f32 rotY = mRotation.y;
		f32 rotX = mRotation.x;
		f32 z    = mPosition.z;
		f32 x    = mPosition.x;
		J3DModel* model = getModel();
		MtxPtr mtx      = model->getBaseTRMtx();
		s16 rotZ16      = rotZ * (65536.0f / 360.0f);
		s16 rotY16      = rotY * (65536.0f / 360.0f);
		s16 rotX16      = rotX * (65536.0f / 360.0f);
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
		if (mScaling.x <= 5.0f) {
			f32 speed = fabsf(mGroundPlane->unk38);
			if (gpMSound->gateCheck(0x3889))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x3889, &mPosition, nullptr, speed, 0, 0, nullptr, 4);
		} else {
			f32 speed = fabsf(mGroundPlane->unk38);
			if (gpMSound->gateCheck(0x388C))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x388C, &mPosition, nullptr, speed, 0, 0, nullptr, 4);
		}
	} else {
		u32 sound = mMapObjData->mSound->unk4.unk0[4];
		if (gpMSound->gateCheck(sound))
			MSoundSESystem::MSoundSE::startSoundActorWithInfo(
			    sound, nullptr, &mPosition, &mVelocity, 0, 0, 0, nullptr, 4);
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
		if (actor->isActorType(0x80000001) && isActorType(0x400000D0)
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

	mWaterEmitInfo->mPos.x = mPosition.x;
	mWaterEmitInfo->mPos.y = mPosition.y;
	mWaterEmitInfo->mPos.z = mPosition.z;
	gpModelWaterManager->emitRequest(*mWaterEmitInfo);

	if (gpMSound->gateCheck(0x38A3))
		MSoundSESystem::MSoundSE::startSoundActor(0x38A3, &mPosition, 0,
		                                         nullptr, 0, 4);

	if (unk19C < 10) {
		TMapObjBase* obj = gpItemManager->makeObjAppear(
		    0x200E, mPosition.x, mPosition.y, mPosition.z, 1, true);
		if (obj != nullptr) {
			obj->mVelocity.x = 0.0f;
			obj->mVelocity.y = 25.0f;
			obj->mVelocity.z = 0.0f;
			obj->onLiveFlag(LIVE_FLAG_UNK10);
			unk19C++;
		}
	}

	TMapObjGeneral::kill();
}

void TBigWatermelon::appearing()
{
	TMapObjGeneral::appearing();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	calcRootMatrix();
	getModel()->update();

	mtx[1][3] = mPosition.y + mBodyRadius * (mEntryRadius
	                                         / mInitialScaling.x);
	mAttackHeight = 50.0f * mScaling.x;
	mAttackRadius = 50.0f * mScaling.x;
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
		f32 speed = JGeometry::TUtil<f32>::sqrt(
		    mVelocity.x * mVelocity.x + mVelocity.y * mVelocity.y
		    + mVelocity.z * mVelocity.z);
		if (!(speed <= 3.814697265625e-06f) || mGroundPlane->unk44)
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

	if (message == 4 && checkMapObjFlag(MAP_OBJ_FLAG_UNK80)) {
		hold(sender);
		return true;
	}

	if (sender->isActorType(0x80000001) && isActorType(0x400000D0)
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

TMapObjBall::~TMapObjBall() { }

// UNUSED in the map: these four were always inlined, so the linker never
// emitted a copy of them.
void TResetFruit::rotting() { }
void TResetFruit::waitEffect() { }
void TResetFruit::living() { }
void TResetFruit::pick(THitActor*) { }
