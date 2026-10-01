#include <Animal/Bird.hpp>
#include <Animal/AnimalBase.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/WireBinder.hpp>
#include <M3DUtil/MActor.hpp>
#include <Map/Map.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MoveBG/Item.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <dolphin/mtx.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* bird_bastable[] = {
	nullptr,
	"/scene/bird/bas/bird_fly.bas",
	"/scene/bird/bas/bird_open.bas",
	nullptr,
	nullptr,
	"/scene/bird/bas/bird_start.bas",
	"/scene/bird/bas/bird_stop.bas",
	nullptr,
	nullptr,
};

namespace {

const int cRandomAnims[] = { 7, 4, 0, 2, 8 };
const char* const cMatName = "_mat_body1";
const GXColorS10 cColorTable[] = {
	{ 0, 100, 255, 0 },
	{ 0, 200, 0, 0 },
	{ 255, 200, 0, 0 },
	{ 255, 0, 0, 0 },
};

} // namespace

TAnimalBird::TAnimalBird(const char* name)
    : TSpineEnemy(name)
    , mItem(nullptr)
    , mWireBinder(nullptr)
{
}

void TAnimalBird::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("bird_man.bmd", 0);
	mSpine->initWith(&TNerveAnimalBirdWaitOnGround::theNerve());
	initParams();
	initCollision();
	initAnmSound();
}

void TAnimalBird::initTevColor(const GXColorS10* color)
{
	int idx = getModel()->getModelData()->getMaterialName()->getIndex(cMatName);
	SMS_InitPacket_OneTevColor(getModel(), idx, GX_TEVREG1, color);
}

void TAnimalBird::initCollision()
{
	initHitActor(0x10000032, 0, 0, 50.0f, 50.0f, 70.0f, 80.0f);
	onHitFlag(HIT_FLAG_CANNOT_ATTACK);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	mScaledBodyRadius = 35.0f;
}

// The target calls initParams() out of line from init(); without this pragma it
// gets inlined and init()'s frame grows by 0x10.
#pragma dont_inline on
void TAnimalBird::initParams()
{
	mHomePosition.set(mPosition);
	mHomePosition.y += 90.0f;
	mHomeRotation.set(mRotation);
	mHitPoints    = getMaxHitPoints();
	unk178        = 0;
	unk17C        = 0;
	unk170        = 1.0f;
	offLiveFlag(LIVE_FLAG_AIRBORNE);
	mRandomScale = 1.0f - 0.1f * (MsRandF() - 0.5f);

	if (TWireBinder::isOnWire(mPosition)) {
		mWireBinder = new TWireBinder;
		mWireBinder->init(mPosition);
	}
}
#pragma dont_inline off

void TAnimalBird::load(JSUMemoryInputStream& stream)
{
	TSpineEnemy::load(stream);

	s32 eventId;
	stream >> eventId;
	if (eventId >= 0)
		mItem = TMapObjBaseManager::newAndRegisterObjByEventID(eventId,
		                                                       "鳥用");
	else
		mItem = TMapObjBaseManager::newAndRegisterObjByEventID(100, "");

	switch (mItem->getActorType()) {
	default:
		mColorType = 1;
		break;
	case 0x20000013:
		mColorType = 2;
		break;
	case 0x2000000F:
		mColorType = 3;
		break;
	case 0x20000010:
		mColorType = 0;
		if (TFlagManager::smInstance->getBlueCoinFlag(
		        SMSGetMarDirector()->getCurrentMap(), eventId))
			onLiveFlag(LIVE_FLAG_DEAD);
		break;
	}

	initTevColor(&cColorTable[mColorType]);
}

void TAnimalBird::loadAfter()
{
	JDrama::TNameRef::loadAfter();
	MSoundSESystem::MSRandPlay::registerTrans(MSD_SE_OBJ_BIRD_DOL_FLYING1, &mPosition);
	MSoundSESystem::MSRandPlay::registerTrans(MSD_SE_OBJ_BIRD_DOL_CHUN, &mPosition);
}

BOOL TAnimalBird::receiveMessage(THitActor* sender, u32 message)
{
	if (mLiveFlag & LIVE_FLAG_DEAD)
		return FALSE;

	// Written as an if/else chain rather than a switch: the target compares
	// each message individually instead of building a jump table.
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		SMS_EasyEmitParticle(PARTICLE_MS_ENM_WATHIT, &sender->mPosition,
		                     nullptr,
		                     JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
		gpMSound->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK,
		                        (const Vec*)&sender->mPosition, 0, 0.0f, 0,
		                        0, 4);
		if (unk178 <= 0)
			unk178 = getBirdParams()->mWaterproofTimerMax.get();

		if (isAirborne() && mHitPoints)
			--mHitPoints;

		return TRUE;
	}

	if (message == HIT_MESSAGE_TAKE && mHolder == nullptr) {
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mHolder = (TTakeActor*)sender;
		SMS_EasyEmitParticle(PARTICLE_MS_ENM_WATHIT, &sender->mPosition,
		                     nullptr,
		                     JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
		return TRUE;
	}

	if ((message == HIT_MESSAGE_PUT || message == HIT_MESSAGE_THROWN)
	    && mHolder == sender) {
		mHolder = nullptr;
		offHitFlag(HIT_FLAG_NO_COLLISION);
		return TRUE;
	}

	if (message == HIT_MESSAGE_UNKB) {
		mHolder = nullptr;
		if (mSpine->getLatestNerve()
		    != &TNerveAnimalBirdChangeToCoin::theNerve()) {
			mSpine->reset();
			mSpine->setNext(&TNerveAnimalBirdChangeToCoin::theNerve());
		} else
			kill();
		return TRUE;
	}

	if (message == HIT_MESSAGE_TRAMPLE && sender->isActorType(0x1000000D)) {
		if (mSpine->getLatestNerve()
		    != &TNerveAnimalBirdChangeToCoin::theNerve()) {
			mSpine->reset();
			mSpine->setNext(&TNerveAnimalBirdChangeToCoin::theNerve());
		} else
			receiveMessage(this, HIT_MESSAGE_SPRAYED_BY_WATER);
		return TRUE;
	}

	return TSpineEnemy::receiveMessage(sender, message);
}

void TAnimalBird::calcRootMatrix()
{
	if (mHolder) {
		MtxPtr takingMtx = mHolder->getTakingMtx();
		getModel()->setBaseTRMtx(takingMtx);
	} else
		TSpineEnemy::calcRootMatrix();

	getModel()->getBaseTRMtx()[1][3] += 35.0f;
}

void TAnimalBird::moveObject()
{
	if (unk178 > 0)
		unk178--;

	TSpineBase<TLiveActor>* spine = mSpine;

	// Written as a single `||` chain over fresh getLatestNerve()/theNerve()
	// calls: the target materialises its own intermediate bools for such a
	// chain (flag registers merged with `li 1` / `addi` / `clrlwi.`) and only
	// expands the first operand's calls inline, calling the rest out of line.
	bool onGroundNerve
	    = (spine->getLatestNerve() == &TNerveAnimalBirdWaitOnGround::theNerve()
	        || spine->getLatestNerve()
	               == &TNerveAnimalBirdActionOnGround::theNerve())
	      || spine->getLatestNerve()
	             == &TNerveAnimalBirdWalkOnGround::theNerve();

	if (onGroundNerve) {
		// checkLiveFlag() rather than isAirborne(): the latter's `? 1 : 0`
		// body makes MWCC materialise a bool, the target just masks the bit.
		if (checkLiveFlag(LIVE_FLAG_AIRBORNE)) {
			// The increment lives in the condition: the target evaluates the
			// getBirdParams() call first and only then bumps unk17C.
			if (getBirdParams()->mFloatingTimerMax.get() < ++unk17C) {
				spine->reset();
				spine->setNext(&TNerveAnimalBirdTakeoff::theNerve());
			}
		} else
			unk17C = 0;
	}

	// The comparison result is kept in a named bool: the target materialises it
	// with subf/cntlzw/extrwi. instead of fusing it into the branch, and calls
	// theNerve()/getLatestNerve() out of line here.
	bool isCoinNerve = spine->isNerve(&TNerveAnimalBirdChangeToCoin::theNerve());
	if (isCoinNerve && mHitPoints == 0) {
		spine->reset();
		spine->setNext(&TNerveAnimalBirdChangeToCoin::theNerve());
	}

	bool flying = spine->getLatestNerve()
	                  == &TNerveAnimalBirdGraphWander::theNerve()
	              || spine->getLatestNerve()
	                     == &TNerveAnimalBirdComeback::theNerve();
	if (!flying) {
		gpMSound->startSeRandPlay(MSD_SE_OBJ_BIRD_DOL_FLYING1,
		                          mInstanceIndex);
	}

	onGroundNerve
	    = (spine->getLatestNerve() == &TNerveAnimalBirdWaitOnGround::theNerve()
	        || spine->getLatestNerve()
	               == &TNerveAnimalBirdActionOnGround::theNerve())
	      || spine->getLatestNerve()
	             == &TNerveAnimalBirdWalkOnGround::theNerve();
	if (!onGroundNerve) {
		gpMSound->startSeRandPlay(MSD_SE_OBJ_BIRD_DOL_CHUN, mInstanceIndex);
	}

	TLiveActor::moveObject();
}

// TODO: 56 %, and the only thing wrong is the first operand of the || chain
// that isOnGroundNerve() (inlined twice) contributes. The target calls
// theNerve()/getLatestNerve() out of line there, so it must reach that inline
// at one level deeper than we do. See the note on isOnGroundNerve().
void TAnimalBird::bind()
{
	if (!isCheckWithWireBinder())
		TLiveActor::bind();
	else
		mWireBinder->bind(this);
}

const char** TAnimalBird::getBasNameTable() const { return bird_bastable; }

// TODO: the target calls theNerve() and getLatestNerve() out of line for all
// four checks when this gets inlined into bind(), we still inline the first.
bool TAnimalBird::isOnGroundNerve() const
{
	TSpineBase<TLiveActor>* spine = mSpine;
	bool result = spine->getLatestNerve()
	                  == &TNerveAnimalBirdWaitOnGround::theNerve()
	              || spine->getLatestNerve()
	                     == &TNerveAnimalBirdActionOnGround::theNerve()
	              || spine->getLatestNerve()
	                     == &TNerveAnimalBirdWalkOnGround::theNerve()
	              || spine->getLatestNerve()
	                     == &TNerveAnimalBirdPreLanding::theNerve();
	return result;
}

// TODO: frame is 8 bytes too big, everything else matches
// The original build inlines this into the three flight nerves (Comeback,
// PreLanding, Landing) but leaves it out of line for the three ground nerves
// (WaitOnGround, ActionOnGround, WalkOnGround). MWCC inlines it everywhere, so
// BIRD_SEARCH_MARIO below mirrors the inlined form used by the flight nerves.
#pragma dont_inline on
bool TAnimalBird::isFindMario() const
{
	if (getBirdParams()->mSearchHeight.get()
	    < fabs(SMS_GetMarioPos().y - mPosition.y))
		return false;

	return isInSight(SMS_GetMarioPos(),
	                 mRandomScale * getBirdParams()->mSearchLength.get(),
	                 mRandomScale * getBirdParams()->mSearchAngle.get(),
	                 mRandomScale * getBirdParams()->mSearchAware.get());
}
#pragma dont_inline off

namespace {

// Mirrors the inlined form of TAnimalBird::isFindMario() that the original
// build emits into the flight nerves. See the note on isFindMario() above.
#define BIRD_SEARCH_MARIO(b)                                                  \
	((b)->getBirdParams()->mSearchHeight.get()                                \
	     < fabs(SMS_GetMarioPos().y - (b)->mPosition.y)                      \
	     ? false                                                              \
	     : (b)->isInSight(SMS_GetMarioPos(),                                 \
	                        (b)->mRandomScale                                \
	                            * (b)->getBirdParams()->mSearchLength.get(),  \
	                        (b)->mRandomScale                                \
	                            * (b)->getBirdParams()->mSearchAngle.get(),   \
	                        (b)->mRandomScale                                \
	                            * (b)->getBirdParams()->mSearchAware.get()))

} // namespace

bool TAnimalBird::isCheckWithWireBinder() const
{
	bool result = false;
	if (mWireBinder && isOnGroundNerve())
		result = true;
	return result;
}

void TAnimalBird::doFlyToCurPathNode()
{
	// quat/v are declared up here and filled in further down: locals run from
	// the top of the frame in declaration order and the target has the quat at
	// the highest address even though it is computed last.
	JGeometry::TQuat4<f32> quat;
	JGeometry::TVec3<f32> v;
	JGeometry::TVec3<f32> diff = unkF4.getPoint();
	diff -= mPosition;

	f32 dist = diff.length();

	if (dist < 100.0f)
		return;

	f32 marchSpeed = mRandomScale * getBirdParams()->mMarchSpeed.get();
	marchSpeed     = marchSpeed * SMSGetAnmFrameRate();
	f32 turnSpeed  = getBirdParams()->mTurnSpeed.get() * SMSGetAnmFrameRate();

	if (dist <= 2.0f * calcMinimumTurnRadius(marchSpeed, turnSpeed))
		turnSpeed = calcTurnSpeedToReach(marchSpeed, 0.5f * dist);

	TAnimalBase::getRotationFlyToDir(&mRotation, diff, marchSpeed, turnSpeed);

	quat = SMS_Eular2Quat(mRotation);
	v.set(0.0f, 0.0f, marchSpeed);
	// The rotation itself is TQuat4::rotate() -- same `w * 0` residue as the
	// other three rotate() sites in this file.
	quat.rotate(v, v);

	f32 rate  = unk178 / (f32)getBirdParams()->mWaterproofTimerMax.get();
	f32 scale = 1.0f - rate;
	v.x *= scale;
	v.y *= scale;
	v.z *= scale;

	// The target recomputes the ratio here rather than reusing `rate`.
	v.y -= getBirdParams()->mWaterPowerY.get()
	       * (unk178 / (f32)getBirdParams()->mWaterproofTimerMax.get());

	mLinearVelocity = v;
}

bool TAnimalBird::doLanding(bool param_1)
{
	if (param_1) {
		f32 speed = mRandomScale * getBirdParams()->mMarchSpeed.get();
		speed     = speed * SMSGetAnmFrameRate();

		JGeometry::TQuat4<f32> quat = SMS_Eular2Quat(mRotation);
		JGeometry::TVec3<f32> velocity;
		// The residual `w * 0` multiplies are the signature of this inline
		// (MWCC folds explicit *0 but not one that came out of a function).
		quat.rotate(JGeometry::TVec3<f32>(0.0f, 0.0f, speed), velocity);
		mVelocity = velocity;
	}

	JGeometry::TVec3<f32> fall(0.0f, 0.0f, 0.0f);
	bool grounded = false;

	JGeometry::TVec3<f32> point;

	if (mWireBinder)
		mWireBinder->getPoint(&point, mHomePosition);
	else
		gpMap->checkGround(mPosition, &mGroundPlane);

	// checkLiveFlag() rather than isAirborne(): the latter's `? 1 : 0` body
	// makes MWCC materialise a bool where the target just masks the bit.
	if (checkLiveFlag(LIVE_FLAG_AIRBORNE))
		fall.y = -getBirdParams()->mLandingGravityY.get();
	else
		grounded = true;

	mRotation.x = mHomeRotation.x;
	mRotation.z = mHomeRotation.z;

	f32 torque = getBirdParams()->mLandingTorqueY.get();
	torque      = torque * SMSGetAnmFrameRate();
	f32 diff    = MsAngleDiff(mHomeRotation.y, mRotation.y);
	// MsClamp() as it is written in MathUtil.hpp tests the upper bound first;
	// the target tests the lower bound first (and drops the redundant
	// `step = -torque` store because the value is already in the register).
	f32 step;
	if (diff < -torque)
		step = -torque;
	else if (diff > torque)
		step = torque;
	else
		step = diff;
	mRotation.y = MsWrap<f32>(mRotation.y + step, 0.0f, 360.0f);

	mLinearVelocity = fall;

	// friction is built first and only then scaled by mLandingFric -- the
	// target does not fold the two multiplications together.
	JGeometry::TVec3<f32> friction;
	JGeometry::TVec3<f32> vel = mVelocity;
	friction.set(0.0f, 0.0f, JGeometry::TUtil<f32>::sqrt(vel.squared()));
	friction *= getBirdParams()->mLandingFric.get();

	JGeometry::TQuat4<f32> quat = SMS_Eular2Quat(mRotation);
	quat.rotate(friction, friction);
	mVelocity = friction;

	return grounded && fabs(step) < 0.01f;
}

TAnimalBirdParams::TAnimalBirdParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mMarchSpeed, 5.0f)
    , PARAM_INIT(mTurnSpeed, 0.1f)
    , PARAM_INIT(mReturnTimer, 1800)
    , PARAM_INIT(mSearchLength, 800.0f)
    , PARAM_INIT(mSearchHeight, 600.0f)
    , PARAM_INIT(mSearchAware, 400.0f)
    , PARAM_INIT(mSearchAngle, 90.0f)
    , PARAM_INIT(mActionTimer, 100)
    , PARAM_INIT(mWaterproofTimerMax, 45)
    , PARAM_INIT(mFloatingTimerMax, 30)
    , PARAM_INIT(mLandingGravityY, 1.0f)
    , PARAM_INIT(mLandingTorqueY, 2.0f)
    , PARAM_INIT(mWalkingTorqueY, 0.5f)
    , PARAM_INIT(mWalkingSpeed, 2.0f)
    , PARAM_INIT(mWalkTimer, 100)
    , PARAM_INIT(mLandingFric, 0.95f)
    , PARAM_INIT(mActionTimerAdd, 300)
    , PARAM_INIT(mWaterPowerY, 15.0f)
{
	TParams::load(mPrmPath);
}

TAnimalBirdManager::TAnimalBirdManager(const char* name)
    : TEnemyManager(name)
{
}

void TAnimalBirdManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TAnimalBirdParams("/Animal/bird.prm");
	TEnemyManager::load(stream);
}

void TAnimalBirdManager::loadAfter()
{
	JDrama::TNameRef::loadAfter();
	MSoundSESystem::MSRandPlay::createRandPlayVec(MSD_SE_OBJ_BIRD_DOL_FLYING1, mObjNum);
	MSoundSESystem::MSRandPlay::createRandPlayVec(MSD_SE_OBJ_BIRD_DOL_CHUN, mObjNum);
}

void TAnimalBirdManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "bird_man.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};

	createModelDataArray(entry);
}

DEFINE_NERVE(TNerveAnimalBirdWaitOnGround, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		bird->mMActor->setBckFromIndex(7);
		bird->setCurAnmSound();
	}

	bool canFly = bird->unk178 > 0 || bird->isFindMario();
	bool wantToFly = false;

	if (canFly && MsRandF() < 0.5f)
		wantToFly = true;

	if (wantToFly) {
		spine->pushAfterCurrent(&TNerveAnimalBirdTakeoff::theNerve());
		return TRUE;
	}

	if (bird->checkCurAnmEnd(0)) {
		// The target counts *up* to mActionTimer (mActionTimer - getTime())
		// and halves MsRandF() instead of doubling the ratio; no `params`
		// local either, so getBirdParams() is called twice.
		int time = bird->getBirdParams()->mActionTimer.get() - spine->getTime();
		bool wantAction = false;

		if (time >= 0)
			wantAction = MsRandF() * 0.5f
			             < (f32)time
			                   / (f32)bird->getBirdParams()
			                          ->mActionTimerAdd.get();

		if (wantAction) {
			spine->pushAfterCurrent(&TNerveAnimalBirdActionOnGround::theNerve());
			return TRUE;
		}
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdActionOnGround, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		int anm = cRandomAnims[(int)(MsRandF() * 5.0f)];

		if (anm == 8) {
			spine->pushAfterCurrent(&TNerveAnimalBirdWalkOnGround::theNerve());
			return TRUE;
		}

		if (!bird->mMActor->checkCurBckFromIndex(anm))
			bird->mMActor->setBckFromIndex(anm);
	}

	bool canFly = bird->unk178 > 0 || bird->isFindMario();
	bool wantToFly = false;

	if (canFly && MsRandF() < 0.5f)
		wantToFly = true;

	if (wantToFly) {
		spine->pushAfterCurrent(&TNerveAnimalBirdTakeoff::theNerve());
		return TRUE;
	}

	if (bird->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdWaitOnGround::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdWalkOnGround, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		bird->unk170 = -1.0f * bird->unk170;
		bird->mMActor->setBckFromIndex(8);
		bird->setCurAnmSound();
	}

	bool canFly = bird->unk178 > 0 || bird->isFindMario();
	bool wantToFly = false;

	if (canFly && MsRandF() < 0.5f)
		wantToFly = true;

	if (wantToFly) {
		spine->pushAfterCurrent(&TNerveAnimalBirdTakeoff::theNerve());
		return TRUE;
	}

	bird->mGravity = 0.15f;

	f32 torque = bird->getBirdParams()->mWalkingTorqueY.get();
	torque      = torque * SMSGetAnmFrameRate();
	bird->mRotation.y = MsWrap<f32>(bird->mRotation.y
	                                    + bird->unk170 * torque,
	                                0.0f, 360.0f);

	// TQuat4::rotate() ends in TVec3<f32>::set<f32>(x, y, z), which is the
	// out-of-line `set<f>` call the target emits right after this nerve
	// (it is the very next symbol in the map's .text layout). `v` is
	// declared before `quat` (locals run from the top of the frame down in
	// declaration order) but filled in after, which is the order the target
	// stores them in.
	JGeometry::TVec3<f32> v;
	JGeometry::TQuat4<f32> quat = SMS_Eular2Quat(bird->mRotation);
	v.set(0.0f, 0.0f, bird->getBirdParams()->mWalkingSpeed.get());
	quat.rotate(v, v);
	bird->mLinearVelocity = v;

	if (spine->getTime() > bird->getBirdParams()->mWalkTimer.get()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdWaitOnGround::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdTakeoff, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		bird->mMActor->setBckFromIndex(5);
		bird->setCurAnmSound();
		bird->onLiveFlag(LIVE_FLAG_AIRBORNE);
		bird->mGravity = 0.0f;
		bird->unk17C   = 0;

		J3DFrameCtrl* ctrl = bird->mMActor->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setRate(3.0f * ctrl->getRate());

		if (gpMSound->gateCheck(MSD_SE_OBJ_BIRD_DOL_TO_FLY1))
			MSoundSESystem::MSoundSE::startSoundActor(
			    MSD_SE_OBJ_BIRD_DOL_TO_FLY1, (const Vec*)&bird->mPosition, 0,
			    nullptr, 0, 4);
	}

	if (bird->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdGraphWander::theNerve());
		bird->onLiveFlag(LIVE_FLAG_AIRBORNE);
		bird->mGravity = 0.0f;
		bird->unk17C   = 0;
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdGraphWander, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		JGeometry::TVec3<f32> vel(0.0f, 0.0f, 0.0f);
		bird->mVelocity = vel;
		bird->getTracer()->mPrevIdx = -1;
		bird->goToShortestNextGraphNode();
	}

	if (spine->getTime() == 0 || bird->isReachedToGoal()) {
		bird->goToRandomNextGraphNode();

		JGeometry::TVec3<f32> point = bird->unk104.getPoint();
		point.x = point.x + 200.0f * (MsRandF() - 0.5f);
		point.y = point.y + 200.0f * (MsRandF() - 0.5f);
		point.z = point.z + 200.0f * (MsRandF() - 0.5f);

		TPathNode goal(point);
		bird->setGoalPath(goal);

		if (bird->mPosition.y <= bird->unkF4.getPoint().y) {
			bird->mMActor->setBckFromIndex(1);
			bird->setCurAnmSound();
		} else {
			bird->mMActor->setBckFromIndex(3);
			bird->setCurAnmSound();
		}
	}

	bird->checkCurAnmEnd(0);

	if (spine->getTime() > bird->getBirdParams()->mReturnTimer.get()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdComeback::theNerve());
		return TRUE;
	}

	bird->doFlyToCurPathNode();
	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdChangeToCoin, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		bird->onLiveFlag(LIVE_FLAG_DEAD);

		if (bird->mItem->isActorType(0x20000013)) {
			bird->mItem->JSGSetTranslation(bird->mPosition);
			((TShine*)bird->mItem)
			    ->appearWithDemo("鳥用メッセージ");
		} else {
			TMapObjBase* obj;

			if (bird->mItem->isActorType(0x2000000E))
				obj = gpItemManager->makeObjAppear(0x2000000E);
			else
				obj = bird->mItem;

			if (obj != nullptr) {
				obj->appear();
				obj->JSGSetTranslation(bird->mPosition);
				obj->mVelocity.set(0.0f, -10.0f, 0.0f);
				obj->offLiveFlag(0x3FFFFFF);
				obj->onLiveFlag(LIVE_FLAG_AIRBORNE);
			}
		}
	}

	return TRUE;
}

DEFINE_NERVE(TNerveAnimalBirdComeback, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		TPathNode goal(bird->mHomePosition);
		bird->setGoalPath(goal);
		bird->mMActor->setBckFromIndex(3);
		bird->setCurAnmSound();
	}

	bird->doFlyToCurPathNode();

	if (BIRD_SEARCH_MARIO(bird)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdGraphWander::theNerve());
		return TRUE;
	}

	if (bird->isReachedToGoal()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdPreLanding::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdPreLanding, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		bird->mMActor->setBckFromIndex(1);
		bird->setCurAnmSound();

		J3DFrameCtrl* ctrl = bird->mMActor->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setRate(1.5f * ctrl->getRate());

		bird->doLanding(true);
	}

	if (BIRD_SEARCH_MARIO(bird)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdGraphWander::theNerve());
		return TRUE;
	}

	if (bird->doLanding(false)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdLanding::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdLanding, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();
	J3DFrameCtrl* ctrl = bird->mMActor->getFrameCtrl(ANM_TYPE_BCK);

	if (spine->getTime() == 0) {
		JGeometry::TVec3<f32> vel(0.0f, 0.0f, 0.0f);
		bird->mVelocity = vel;
		bird->mMActor->setBckFromIndex(5);
		bird->setCurAnmSound();
		ctrl->setAttribute(J3DFrameCtrl::ATTR_ONCE_AND_RESET);
		ctrl->setFrame(ctrl->getEnd());
		ctrl->setRate(-1.0f * ctrl->getRate());
	}

	if (BIRD_SEARCH_MARIO(bird)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdGraphWander::theNerve());
		return TRUE;
	}

	if (ctrl->checkState(J3DFrameCtrl::STATE_COMPLETED_ONCE)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdWaitOnGround::theNerve());
		return TRUE;
	}

	return FALSE;
}
