#include <MoveBG/MapObjGeneral.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <Strategic/Binder.hpp>
#include <Player/MarioAccess.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapData.hpp>
#include <Map/PollutionManager.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/Map.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

u32 TMapObjGeneral::mNormalLivingTime       = 960;
u32 TMapObjGeneral::mNormalFlushTime        = 360;
int TMapObjGeneral::mNormalFlushInterval    = 10;
u32 TMapObjGeneral::mNormalWaitToAppearTime = 360;
f32 TMapObjGeneral::mNormalAppearingScaleUp = 0.01f;
f32 TMapObjGeneral::mNormalThrowSpeedRate   = 0.5f;

bool TMapObjGeneral::isPollutedGround(const JGeometry::TVec3<f32>& v) const
{
	// WTF? Did they forget to refactor this?!
	const JGeometry::TVec3<f32>& p = mInitialPosition;
	if (gpPollution->isPolluted(v.x, p.y, p.z)
	    || gpPollution->isPolluted(v.x - 32.0f, v.y, v.z - 32.0f)
	    || gpPollution->isPolluted(v.x + 32.0f, v.y, v.z - 32.0f)
	    || gpPollution->isPolluted(v.x - 32.0f, v.y, v.z + 32.0f)
	    || gpPollution->isPolluted(v.x + 32.0f, v.y, v.z + 32.0f))
		return true;

	return false;
}

inline f32 distToMario(const JGeometry::TVec3<f32>& v)
{
	f32 l = (v.x - gpMarioPos->x) * (v.x - gpMarioPos->x)
	        + (v.y - gpMarioPos->y) * (v.y - gpMarioPos->y)
	        + (v.z - gpMarioPos->z) * (v.z - gpMarioPos->z);
	return JGeometry::TUtil<f32>::sqrt(l);
}

// Sum level: gives the radius sum retail's f1 = mario + damage operand order
// (and +8 of pool) in waitingToAppear's plain branch.
static inline f32 MOGSum(f32 a, f32 b) { return a + b; }

// The comparisons test the distance against the radii (retail's
// `fcmpo dist, sum; ble`): the object appears once Mario is outside them.
// The 0x4000005a branch names its 100-unit margin: a named damage radius
// there swaps the `mario + damage` fadds operands, and reading
// getDamageRadius() in the sum without the margin local is 8 short.
void TMapObjGeneral::waitingToAppear()
{
	if (isStateTimerEngaged())
		return;

	if (isActorType(0x4000005a)) {
		f32 margin = 100.0f;
		if (distToMario(getInitialPosition())
		    > SMS_GetMarioDamageRadius() + getDamageRadius() + margin)
			appear();
	} else {
		f32 damageRadius = getDamageRadius();
		if (distToMario(getInitialPosition())
		    > MOGSum(SMS_GetMarioDamageRadius(), damageRadius))
			appear();
	}
}

void TMapObjGeneral::waitingToRecover()
{
	if (!isPollutedGround(mInitialPosition))
		recover();
}

void TMapObjGeneral::waitToAppear(s32 waitTime)
{
	if (waitTime == 0)
		mStateTimer = mNormalWaitToAppearTime;
	else
		mStateTimer = waitTime;
	mState = STATE_WAITING_TO_APPEAR;
}

void TMapObjGeneral::sink()
{
	mVelocity.x = mVelocity.y = mVelocity.z = 0.0f;
	onLiveFlag(LIVE_FLAG_UNK10);
	mState = STATE_SINKING;
	unk144 = mPosition.y;
	setUpMapCollision(1);
	startSound(6);
}

void TMapObjGeneral::put()
{
	mHolder                    = nullptr;
	mHolder                    = nullptr;
	s32 preservedTimeTilAppear = getStateTimer();
	makeObjAppeared();
	mStateTimer = preservedTimeTilAppear;
	mPosition.x = JMASSin(SMS_GetMarioAngleY())
	                  * (getDamageRadius() + SMS_GetMarioDamageRadius() + 10.0f)
	              + SMS_GetMarioPos().x;
	mPosition.y = SMS_GetMarioPos().y;
	mPosition.z = JMASCos(SMS_GetMarioAngleY())
	                  * (getDamageRadius() + SMS_GetMarioDamageRadius() + 10.0f)
	              + SMS_GetMarioPos().z;
	offLiveFlag(LIVE_FLAG_UNK10);
	mGroundHeight = SMSGetMap()->checkGround(mPosition, &mGroundPlane);
}

// TODO: 99.5%, frame exact. The accessors (Mario position, angles, one
// speed, getMapObjData, getVelocity) give retail's unshared sin/cos shifts
// and its 0x90 frame; the named throw power gives retail's r5/r6. Left:
// f1/f4/f5 on y/z/rate. Inert: component stores in either order, named
// x/y/z locals (with or without the power), a named rate, both speed
// accessors, rate operand order.
// c-k7 (debugger, regalloc.py --move 36:b37 replays retail exactly): set()'s
// z binding (@1479) must be coloured before its y binding (@1478); then z
// takes f1, y f4 and the IRO rate temp f5. Bindings are created x, y, z, so y
// needs no binding (it then becomes a named web, coloured after the rate:
// y f5, rate f4) or an IRO split temp created before the rate's. Inert or
// worse: a named y before/after power (raw, getMapObjData() or the
// TMapObjGeneralGetPhysicalData level), a named TMapObjPhysicalData*;
// c-k15: `mMapObjData->getPhysicalData()` at the three reads -0x18,
// `getMapObjData()->getPhysicalData()` -0x20.
void TMapObjGeneral::thrown()
{
	mPosition.set(SMS_GetMarioPos().x, SMS_GetMarioPos().y, SMS_GetMarioPos().z);
	mRotation.set(SMS_GetMarioAngleX(), SMS_GetMarioAngleY(), SMS_GetMarioAngleZ());

	mGroundHeight = SMSGetMapBound()->checkGround(mPosition, &mGroundPlane);
	unk138        = 0;
	mHolder       = nullptr;

	f32 power = *gpMarioThrowPower;
	mVelocity.set(JMASSin(SMS_GetMarioAngleY())
	                      * getMapObjData()->mPhysical->unk4->unk2C
	                      * power
	                  + (mNormalThrowSpeedRate * SMS_GetMarioSpeedX()),
	              getMapObjData()->mPhysical->unk4->unk30,
	              JMASCos(SMS_GetMarioAngleY())
	                      * getMapObjData()->mPhysical->unk4->unk2C
	                      * power
	                  + (mNormalThrowSpeedRate * *gpMarioSpeedZ));

	offLiveFlag(LIVE_FLAG_UNK10);
	JGeometry::TVec3<f32> vel = getVelocity();
	mPosition.add(vel);
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	removeMapCollision();
	offHitFlag(HIT_FLAG_NO_COLLISION);
	startAnim(5);
	startSound(5);
	mState = STATE_NORMAL;
}

void TMapObjGeneral::touchingWater()
{
	if (animIsFinished() && hasModelOrAnimData(4))
		startAnim(0);
}

void TMapObjGeneral::touchingPlayer()
{
	if (animIsFinished() && hasModelOrAnimData(4))
		startAnim(0);
}

void TMapObjGeneral::holding()
{
	mPosition     = getHolder()->mPosition;
	mGroundHeight = gpMap->checkGround(getPosition(), &mGroundPlane);
}

static inline const TMapObjSinkData* TMapObjGeneralGetSink(TMapObjGeneral* p)
{
	TMapObjData* data           = p->mMapObjData;
	const TMapObjSinkData* sink = data->mSink;
	return sink;
}

static inline TTakeActor* TMapObjGeneralGetHeldObject(TMapObjGeneral* p)
{
	return p->mHeldObject;
}

void TMapObjGeneral::recovering()
{
	startSound(9);
	if (hasModelOrAnimData(6)) {
		J3DModel* model = getModel();
		MtxPtr mat      = model->getAnmMtx(0);
		f32 fVar1       = mat[1][3] - unk144;
		mDamageHeight += fVar1;
		calcEntryRadius();
		if (mHeldObject)
			mHeldObject->mPosition.y += fVar1;
		unk144 = mat[1][3];
		if (!animIsFinished())
			return;
	} else if (mPosition.y < unk144) {
		mPosition.y += TMapObjGeneralGetSink(this)->unk4;
		if (TMapObjGeneralGetHeldObject(this))
			TMapObjGeneralGetHeldObject(this)->mPosition.y
			    += TMapObjGeneralGetSink(this)->unk4;
		return;
	}

	makeObjRecovered();
}

void TMapObjGeneral::sinking()
{
	mPosition.y -= mMapObjData->mSink->unk0;

	for (int i = 0; i < getColNum(); ++i) {
		if (getCollision(i)->checkActorType(0x1000000)) {
			recover();
			return;
		}
	}

	if (mPosition.y + mMapObjData->mHit->unkC[2].unk4 < unk144) {
		if (mPosition.x != getInitialPosition().x
		    || mPosition.z != mInitialPosition.z) {
			makeObjDefault();
			makeObjAppeared();
		} else {
			makeObjBuried();
		}
	}
}

void TMapObjGeneral::breaking()
{
	if (animIsFinished()) {
		makeObjDead();
		if (checkMapObjFlag(MAP_OBJ_FLAG_RESPAWNING)) {
			makeObjDefault();
			waitToAppear(0);
		}
	}
}

static inline const JGeometry::TVec3<f32>&
TMapObjGeneralGetInitialScaling(TMapObjGeneral* p)
{
	const JGeometry::TVec3<f32>& scaling = p->mInitialScaling;
	return scaling;
}

void TMapObjGeneral::appearing()
{
	// TODO: uuuuuuuh...
	if (hasAnim(1)) {
		if (animIsFinished())
			goto uuuh;
		return;
	}

	{
		mScaling.x += mNormalAppearingScaleUp;
		mScaling.y += mNormalAppearingScaleUp;
		mScaling.z += mNormalAppearingScaleUp;
		if (mScaling.x < mInitialScaling.x)
			return;

		mScaling.set(TMapObjGeneralGetInitialScaling(this));
	}

uuuh:
	if (!checkLiveFlag(LIVE_FLAG_UNK10))
		return;

	makeObjAppeared();
}

void TMapObjGeneral::appeared()
{
	if (checkMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING) && !isStateTimerEngaged())
		makeObjDead();
}

void TMapObjGeneral::makeObjRecovered()
{
	makeObjDefault();
	makeObjAppeared();
}

void TMapObjGeneral::makeObjBuried()
{
	unk144 = mPosition.y;
	mPosition.y -= mMapObjData->mHit->unkC[2].unkC;
	onHitFlag(HIT_FLAG_NO_COLLISION);
	removeMapCollision();
	mMActor = nullptr;
	mState  = STATE_BURIED;
}

void TMapObjGeneral::receiveMessageFromPlayer() { startAnim(4); }

u32 TMapObjGeneral::touchWater(THitActor* water)
{
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK400000)) {
		kill();
		return 1;
	} else {
		if (hasModelOrAnimData(3)) {
			startAnim(3);
			mState = STATE_TOUCHING_WATER;
		}
		return 1;
	}
}

void TMapObjGeneral::touchPlayer(THitActor* player)
{
	TMapObjBase::touchPlayer(player);
	if (hasModelOrAnimData(4)) {
		startAnim(4);
		mState = STATE_TOUCHING_PLAYER;
	}
}

// TODO: every instruction matches; the frame is 0x28 short (0x28 vs 0x50):
// retail's two conversion temporaries sit at 0x38/0x40 over 0x30 of dead low
// region. The named table pointer (setObjHitData's spelling) gives the radius
// retail's f5. Inert: named radius/size/divisor/coordinates.
void TMapObjGeneral::recover()
{
	const TMapObjHitDataTable* table = &mMapObjData->mHit->unkC[2];
	SMSGetPollution()->clean(mPosition.x, unk144, mPosition.z,
	                         (u16)(table->unk0 / 6.0f));

	setUpMapCollision(1);
	startAnim(6);
	mState = STATE_RECOVERING;
	setObjHitData(0);
	startSound(8);
	mDamageHeight = 0.0f;
	calcEntryRadius();
	offHitFlag(HIT_FLAG_NO_COLLISION);
	if (hasModelOrAnimData(6)) {
		f32 tmp     = mPosition.y;
		mPosition.y = unk144;
		unk144      = tmp;
		getModel();
	}
}

void TMapObjGeneral::hold(TTakeActor* actor)
{
	if (mMapCollisionManager && mMapCollisionManager->unk8)
		mMapCollisionManager->unk8->remove();
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mHolder = actor;
	mState  = STATE_HOLDING;
}

// Binding level worth +8 of low region, landing
// TMapObjGeneral::ensureTakeSituation's frame at 0x20 (batch 124).
static inline bool MapObjGeneralIsStateL0(TMapObjGeneral* p, u32 i)
{
	bool state = p->isState(i);
	return state;
}

static inline bool MapObjGeneralIsState(TMapObjGeneral* p, u32 i)
{
	bool state = MapObjGeneralIsStateL0(p, i);
	return state;
}

void TMapObjGeneral::ensureTakeSituation()
{
	TMapObjBase::ensureTakeSituation();
	if (MapObjGeneralIsState(this, STATE_HOLDING) && mHolder == nullptr) {
		mState = STATE_NORMAL;
		offLiveFlag(LIVE_FLAG_UNK10);
	}
}

void TMapObjGeneral::kill()
{
	onHitFlag(HIT_FLAG_NO_COLLISION);
	removeMapCollision();
	onLiveFlag(LIVE_FLAG_UNK10 | LIVE_FLAG_UNK8);
	mStateTimer = -1;
	startAnim(2);
	mState = STATE_BREAKING;
	startSound(2);
	breaking();
}

// Binding level worth +8 of low region, landing TMapObjGeneral::appear's
// frame at 0x28 (batch 121).
static inline u32 MapObjGeneralGetLivingTime(const TMapObjGeneral* p)
{
	u32 livingTime = p->getLivingTime();
	return livingTime;
}

void TMapObjGeneral::appear()
{
	makeObjAppeared();
	startAnim(1);
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK800000)) {
		mScaling.x = mNormalAppearingScaleUp;
		mScaling.y = mNormalAppearingScaleUp;
		mScaling.z = mNormalAppearingScaleUp;
	}

	if (!isActorType(0x20000010)
	    || !TFlagManager::smInstance->getBlueCoinFlag(
	        gpMarDirector->getCurrentMap(), mEventId))
		startSound(1);

	appearing();
	if (checkMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING))
		mStateTimer = MapObjGeneralGetLivingTime(this);

	mState = STATE_APPEARING;
}

void TMapObjGeneral::work()
{
	switch (mState) {
	case STATE_NORMAL:
		appeared();
		break;
	case STATE_APPEARING:
		appearing();
		break;
	case STATE_BREAKING:
		breaking();
		break;
	case STATE_SINKING:
		sinking();
		break;
	case STATE_RECOVERING:
		recovering();
		break;
	case STATE_TOUCHING_PLAYER:
		touchingPlayer();
		break;
	case STATE_TOUCHING_WATER:
		touchingWater();
		break;
	case STATE_HOLDING:
		holding();
		break;
	case STATE_BURIED:
		waitingToRecover();
		break;
	}
}

void TMapObjGeneral::touchWall(JGeometry::TVec3<f32>* param_1,
                               TBGWallCheckRecord* param_2)
{
	param_1->x = param_2->mCenter.x;
	param_1->z = param_2->mCenter.z;
	calcReflectingVelocity(param_2->mResultWalls[0],
	                       mMapObjData->mPhysical->unk4->unk8, &mVelocity);
}

void TMapObjGeneral::checkWallCollision(JGeometry::TVec3<f32>* param_1)
{
	param_1->y += mMapObjData->getPhysical()->unk4->unk1C;

	TBGWallCheckRecord check(*param_1, mBodyRadius, 4,
	                         mMapObjData->mPhysical->mWallCheckFlags);

	bool touched = gpMap->isTouchedWallsAndMoveXZ(&check);

	param_1->y -= mMapObjData->getPhysical()->unk4->unk1C;

	if (touched) {
		unk138 = check.mResultWalls[0];
		touchWall(param_1, &check);
	} else {
		unk138 = 0;
	}
}

void TMapObjGeneral::touchRoof(JGeometry::TVec3<f32>* param_1)
{
	param_1->y = unk140;
}

void TMapObjGeneral::checkRoofCollision(JGeometry::TVec3<f32>* param_1)
{
	unk140 = gpMap->checkRoof(param_1->x, param_1->y + mHeadHeight, param_1->z,
	                          &unk13C);
	if (param_1->y + mHeadHeight >= unk140)
		touchRoof(param_1);
}

void TMapObjGeneral::touchGround(JGeometry::TVec3<f32>* param_1)
{
	if (mMapObjData->getPhysical() ? true : false) {
		mVelocity.x *= mMapObjData->getPhysicalData()->unk10;
		mVelocity.z *= mMapObjData->getPhysicalData()->unk10;
	}

	if ((mMapObjData->mPhysical ? true : false)
	    && abs(JGeometry::TVec3<f32>(mVelocity).y)
	           > mMapObjData->getPhysicalData()->unkC) {
		param_1->y -= JGeometry::TVec3<f32>(mVelocity).y;
		mVelocity.y *= -mMapObjData->getPhysicalData()->unk4;
		if (isCoin(this)) {
			SMSGetMSound()->startSoundActorWithInfo(
			    MSD_SE_SY_COIN_BOUND, &mPosition, nullptr,
			    abs(JGeometry::TVec3<f32>(mVelocity).y), 0, 0, nullptr, 0, 4);
		} else {
			startSound(4);
		}
	} else {
		offLiveFlag(LIVE_FLAG_AIRBORNE);
		mVelocity.x = mVelocity.y = mVelocity.z = 0.0f;
		onLiveFlag(LIVE_FLAG_UNK10);
		param_1->y = mGroundHeight;
	}
}

void TMapObjGeneral::checkGroundCollision(JGeometry::TVec3<f32>* param_1)
{
	mGroundHeight = gpMap->checkGround(param_1->x, param_1->y + mHeadHeight,
	                                   param_1->z, &mGroundPlane);
	mGroundHeight += 1.0f;
	if (param_1->y <= mGroundHeight) {
		touchGround(param_1);
	} else if (!mGroundActor)
		onLiveFlag(LIVE_FLAG_AIRBORNE);
}

void TMapObjGeneral::calcVelocity()
{
	if (checkLiveFlag2(LIVE_FLAG_AIRBORNE)) {
		f32 dVar5 = getGravityY();
		mVelocity.y -= dVar5;

		mVelocity.y = MsClamp<f32>(mVelocity.y, -mBodyRadius, mBodyRadius);
	}

	const TMapObjPhysicalInfo* piVar4 = mMapObjData->mPhysical;
	if (piVar4 ? (u8)1 : (u8)0) {
		mVelocity.x *= mMapObjData->getPhysicalData()->unk18;
		mVelocity.z *= mMapObjData->getPhysicalData()->unk18;

		mVelocity.x = MsClamp<f32>(mVelocity.x, -mBodyRadius, mBodyRadius);
		mVelocity.z = MsClamp<f32>(mVelocity.z, -mBodyRadius, mBodyRadius);

		if (mGroundPlane->mNormal.y == 1.0f) {
			if (abs(mVelocity.x) < mMapObjData->getPhysicalData()->unkC)
				mVelocity.x = 0.0f;
			if (abs(mVelocity.z) < mMapObjData->getPhysicalData()->unkC)
				mVelocity.z = 0.0f;
		}
	}
}

// TODO: 99.9%, frame exact; retail's inline-object block (the roof test's
// velocity copy and the three rest-test copies) sits 0xc lower, as if one
// more TVec3 temporary were expanded before them. Inert: `vec` copy-
// initialised, `+=` for add, getVelocity() in the sum.
void TMapObjGeneral::bind()
{
	if (checkLiveFlag(LIVE_FLAG_UNK10))
		return;

	if (mBinder != nullptr) {
		mBinder->bind(this);
		return;
	}

	calcVelocity();
	JGeometry::TVec3<f32> vec;
	vec = getPosition();
	vec.add(mLinearVelocity);
	vec.add(mVelocity);
	checkGroundCollision(&vec);
	if (checkMapObjFlag(MAP_OBJ_FLAG_ENABLE_WALL_COLLISION))
		checkWallCollision(&vec);

	if (checkMapObjFlag(MAP_OBJ_FLAG_ENABLE_ROOF_COLLISION)) {
		if (JGeometry::TVec3<f32>(mVelocity).y > 0.0f)
			checkRoofCollision(&vec);
	}

	if (mGroundPlane->isIllegalData()) {
		kill();
		return;
	}

	if (!checkLiveFlag2(LIVE_FLAG_AIRBORNE)) {
		if (JGeometry::TVec3<f32>(getVelocity()).x == 0.0f
		    && JGeometry::TVec3<f32>(getVelocity()).y == 0.0f
		    && JGeometry::TVec3<f32>(getVelocity()).z == 0.0f)
			onLiveFlag(LIVE_FLAG_UNK10);
	}

	mLinearVelocity = vec - mPosition;
}

// Binding level worth +8 of low region, landing TMapObjGeneral::control's
// frame at 0x20 (batch 124).
static inline bool
MapObjGeneralCheckMapObjFlagL0(const TMapObjGeneral* p, u32 i)
{
	bool mapObjFlag = p->checkMapObjFlag(i);
	return mapObjFlag;
}

static inline bool MapObjGeneralCheckMapObjFlag(const TMapObjGeneral* p, u32 i)
{
	bool mapObjFlag = MapObjGeneralCheckMapObjFlagL0(p, i);
	return mapObjFlag;
}

void TMapObjGeneral::control()
{
	TMapObjBase::control();
	if (MapObjGeneralCheckMapObjFlag(this, MAP_OBJ_FLAG_CAN_SINK)
	    && isState(STATE_NORMAL) && !isAirborne()
	    && isPollutedGround(mPosition))
		sink();

	work();
}

void TMapObjGeneral::calcRootMatrix()
{
	J3DModel* model = getModel();

	if (isState(STATE_HOLDING) && mHolder) {
		if (mMapObjData->mHold) {
			TMapObjHoldData* hold = mMapObjData->mHold;

			MtxPtr src = mHolder->getTakingMtx();
			MTXCopy(src, hold->unkC->getBaseTRMtx());
			hold->unkC->calc();

			MtxPtr src2 = hold->unk10;
			MTXCopy(src2, model->getBaseTRMtx());
			mPosition.set(src2[0][3], src2[1][3], src2[2][3]);
		} else {
			MtxPtr src = mHolder->getTakingMtx();
			MTXCopy(src, checkMapObjFlag(MAP_OBJ_FLAG_UNK100)
			                 ? model->getAnmMtx(0)
			                 : model->getBaseTRMtx());
			mPosition.set(src[0][3], src[1][3], src[2][3]);
		}
	} else {
		MsMtxSetXYZRPH(model->getBaseTRMtx(), mPosition.x,
		               mPosition.y - mYOffset, mPosition.z, mRotation.x,
		               mRotation.y, mRotation.z);
	}
	model->setBaseScale(mScaling);
}

// Fork over the static flush interval (+8 of pool in perform).
static inline int MapObjGeneralFlushInterval()
{
	return TMapObjGeneral::mNormalFlushInterval;
}

void TMapObjGeneral::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_MOVE) {
		if (isState(STATE_WAITING_TO_APPEAR))
			waitingToAppear();
	} else {
		// The explicit upcast stops MWCC reusing the timer load from the
		// engaged test, as retail reloads it.
		if (checkMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING)
		    && ((TMapObjBase*)this)->isStateTimerEngaged()
		    && getStateTimer() < getFlushTime()
		    && ((getStateTimer() / MapObjGeneralFlushInterval()) & 1) != 0) {
			return;
		}
	}

	TMapObjBase::perform(cue, graphics);
}

static inline const JGeometry::TVec3<f32>& MapObjGeneralVelocity(TMapObjGeneral* self)
{
	const JGeometry::TVec3<f32>& r = self->getVelocity();
	return r;
}

// The two actor-type tests read the sender's type (retail's `lwz 0x4c` off
// the sender register), not this object's.
// TODO: frame 0x10 short (0x48 vs 0x58); getVelocity() is +8 of it. Inert:
// the base result unnamed or BOOL, getMapObjData(); a TTakeActor binder
// costs 1% of match.
BOOL TMapObjGeneral::receiveMessage(THitActor* sender, u32 message)
{
	int ret = TMapObjBase::receiveMessage(sender, message);
	if (ret)
		return true;

	if (message == HIT_MESSAGE_TAKE && checkMapObjFlag(MAP_OBJ_FLAG_UNK100000)
	    && JGeometry::TVec3<f32>(MapObjGeneralVelocity(this)).isZero()
	    && (isState(STATE_APPEARING) || isState(STATE_NORMAL)
	        || isState(STATE_TOUCHING_PLAYER)
	        || isState(STATE_TOUCHING_WATER))) {
		hold((TTakeActor*)sender);
		return true;
	}

	if (message == HIT_MESSAGE_TAKE && sender->isActorType(0x10000025)
	    && (isState(STATE_APPEARING) || isState(STATE_NORMAL))) {
		hold((TTakeActor*)sender);
		return 1;
	}

	if (message == HIT_MESSAGE_PUT && isState(STATE_HOLDING)) {
		put();
		return true;
	}

	if (message == HIT_MESSAGE_THROWN && isState(STATE_HOLDING)
	    && mMapObjData->mPhysical != nullptr) {
		thrown();
		return true;
	}

	if (message == HIT_MESSAGE_HIP_DROP
	    && checkMapObjFlag(MAP_OBJ_FLAG_UNK200000)) {
		kill();
		return true;
	}

	bool fromMario = sender->isActorType(0x80000001);
	if (fromMario
	    && (message == HIT_MESSAGE_TRAMPLE
	        || message == HIT_MESSAGE_HIP_DROP)) {
		receiveMessageFromPlayer();
		return true;
	}

	if (message == HIT_MESSAGE_UNKB
	    && checkMapObjFlag(MAP_OBJ_FLAG_UNK200000)) {
		kill();
	}

	return false;
}

void TMapObjGeneral::loadAfter()
{
	TMapObjBase::loadAfter();
	if (checkMapObjFlag(MAP_OBJ_FLAG_CAN_SINK) && isPollutedGround(mPosition))
		makeObjBuried();
}

TMapObjGeneral::TMapObjGeneral(const char* name)
    : TMapObjBase(name)
    , unk138(0)
    , unk13C(0)
    , unk140(0.0f)
    , unk144(0.0f)
{
}
