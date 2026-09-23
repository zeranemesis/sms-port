#include <Enemy/TinKoopa.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/CoasterKiller.hpp>
#include <Enemy/Graph.hpp>
#include <Camera/CameraShake.hpp>
#include <GC2D/GCConsole2.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Player/Mario.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Strategy.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// TODO: this translation unit is freshly scaffolded from marioEU.MAP. Class
// layouts only contain the fields verified so far; the effect emitters, the
// break nerve, TTinKoopa::init and the parts setup are not decompiled yet.

static const char* tinkoopa_bastable[] = {
	"/scene/tinkoopa/bas/tinkoopa_break1.bas",
	"/scene/tinkoopa/bas/tinkoopa_break2.bas",
	"/scene/tinkoopa/bas/tinkoopa_break3.bas",
	"/scene/tinkoopa/bas/tinkoopa_break4.bas",
	nullptr,
	"/scene/tinkoopa/bas/tinkoopa_damage1.bas",
	"/scene/tinkoopa/bas/tinkoopa_damage2.bas",
	"/scene/tinkoopa/bas/tinkoopa_damage3.bas",
	"/scene/tinkoopa/bas/tinkoopa_damage4.bas",
	nullptr,
	nullptr,
	nullptr,
	"/scene/tinkoopa/bas/tinkoopa_wait1.bas",
	"/scene/tinkoopa/bas/tinkoopa_wait2.bas",
	"/scene/tinkoopa/bas/tinkoopa_wait3.bas",
	"/scene/tinkoopa/bas/tinkoopa_wait4.bas",
	"/scene/tinkoopa/bas/tinkoopa_wait5.bas",
};

static const char* TTinKoopa_jointNameTable[] = {
	"jnt_head",     "jnt_breast",   "jnt_stomach",   "jnt_rarm",
	"jnt_larm",     "jnt_leg",      "jnt_leye",      "jnt_reye",
	"fire_null",    "fire_col_null", "jnt_femur",    "killer_null1",
	"killer_null2", "killer_null3", "killer_null4",
};

static int TTinKoopa_jointIndexTable[15];

TTinKoopaParams::TTinKoopaParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLPartsHP, 2)
    , PARAM_INIT(mSLFlameHP, 10)
    , PARAM_INIT(mSLFlameRevivalTime, 10)
    , PARAM_INIT(mSLFlameDamageRadius0, 100.0f)
    , PARAM_INIT(mSLFlameDamageHeight0, 400.0f)
    , PARAM_INIT(mSLFlameDamageRadius1, 100.0f)
    , PARAM_INIT(mSLFlameDamageHeight1, 400.0f)
    , PARAM_INIT(mSLDamageRadius, 1000.0f)
    , PARAM_INIT(mSLDamageHeight0, 4000.0f)
    , PARAM_INIT(mSLDamageHeight1, 4000.0f)
    , PARAM_INIT(mSLKillerInterval, 30)
    , PARAM_INIT(mSLDefeatWaitTime, 600)
    , PARAM_INIT(mSLKillerApproachingDistance, 6000.0f)
{
	TParams::load(mPrmPath);

	// NOTE: the values from the .prm file are overridden right away
	mSLPartsHP.set(1);
	mSLFlameHP.set(50);
	mSLFlameRevivalTime.set(1200);
	mSLFlameDamageRadius0.set(500.0f);
	mSLFlameDamageHeight0.set(300.0f);
	mSLFlameDamageRadius1.set(700.0f);
	mSLFlameDamageHeight1.set(400.0f);
	mSLKillerInterval.set(60);
	mSLDamageRadius.set(800.0f);
	mSLDamageHeight0.set(4000.0f);
	mSLDamageHeight1.set(3400.0f);
	mSLDefeatWaitTime.set(160);
	mSLKillerApproachingDistance.set(2000.0f);
}

int TTinKoopa_getJointIndex(int index)
{
	return TTinKoopa_jointIndexTable[index];
}

const char* TTinKoopa_getCollisionFileName(int index)
{
	static const char* table[] = {
		"/scene/tinkoopa/head_col.col",  "/scene/tinkoopa/breast_col.col",
		"/scene/tinkoopa/stomach_col.col", "/scene/tinkoopa/rarm_col.col",
		"/scene/tinkoopa/larm_col.col",  "/scene/tinkoopa/leg_col.col",
	};
	return table[index];
}

const char* TTinKoopa_getPartsFileName(int index)
{
	static const char* table[] = {
		nullptr,           "tinkoopa_breast.bmd", "tinkoopa_stomach.bmd",
		"tinkoopa_rarm.bmd", "tinkoopa_larm.bmd", nullptr,
	};
	return table[index];
}

int TTinKoopa_getBreakingAnimationIndex(int index)
{
	static int table[] = { 0, 4, 11, 9, 10, 0 };
	return table[index];
}

u32 TTinKoopa_getActorType(int index)
{
	static u32 table[] = {
		0x08000019, 0x0800001A, 0x0800001B,
		0x0800001D, 0x0800001C, 0x0800001E,
	};
	return table[index];
}

int TTinKoopa_getWaitAnimationIndex(int phase)
{
	static int table[] = { 12, 13, 14, 15, 16 };
	return table[phase];
}

int TTinKoopa_getBreakAnimationIndex(int phase)
{
	static int table[] = { 0, 1, 2, 3, 16 };
	return table[phase];
}

int TTinKoopa_getDamageAnimationIndex(int phase)
{
	static int table[] = { 5, 6, 7, 8, 16 };
	return table[phase];
}

int TTinKoopa_getPartsVisibleFrame(int phase)
{
	static int table[] = { 100, 134, 134, 100, 0 };
	return table[phase];
}

int TTinKoopa_getBreakingPartsIndex(int phase)
{
	static int table[] = { 2, 3, 4, 1, 2 };
	return table[phase];
}

static const char* breastTrackJointNameTable[] = {
	"breast_1", "breast_2", "breast_3", "breast_4", "breast_5", "breast_6",
};

static const char* bellyTrackJointNameTable[] = {
	"stomach_1", "stomach_2", "stomach_3",
	"stomach_4", "stomach_5", "stomach_6",
};

static const char* rightArmTrackJointNameTable[] = {
	"rarm_1",
	"rarm_2",
	"rarm_3",
	"rarm_4",
};

static const char* leftArmTrackJointNameTable[] = {
	"larm_1",
	"larm_2",
	"larm_3",
	"larm_4",
};

void TTinKoopa::init(TLiveManager* manager)
{
	// TODO: not decompiled yet
}

void TTinKoopa::makeLaunchSchedule()
{
	int i = 0;
	mLaunchSchedule->mOrders[i++]->makeOrder(0, 600, -1, 0);
	mLaunchSchedule->mOrders[i++]->makeOrder(0, 1230, -1, 1);
	mLaunchSchedule->mOrders[i++]->makeOrder(0, 2450, -1, 1);
	mLaunchSchedule->mOrders[i++]->makeOrder(1, 240, -1, 0);
	mLaunchSchedule->mOrders[i++]->makeOrder(1, 280, -1, 1);
	mLaunchSchedule->mOrders[i++]->makeOrder(1, 630, -1, 0);
	mLaunchSchedule->mOrders[i++]->makeOrder(1, 900, -1, 0);
	mLaunchSchedule->mOrders[i++]->makeOrder(1, 1200, -1, 1);
	mLaunchSchedule->mOrders[i++]->makeOrder(2, 565, -1, 0);
	mLaunchSchedule->mOrders[i++]->makeOrder(2, 700, -1, 0);
	mLaunchSchedule->mOrders[i++]->makeOrder(2, 2220, -1, 1);
}

TTinKoopaFlame::TTinKoopaFlame(const char* name, TTinKoopa* owner)
    : THitActor(name)
    , mOwner(owner)
{
	// TODO: not decompiled yet, only seen inlined into TTinKoopa::init
}

void TTinKoopaFlame::makeHitCollision()
{
	if (mOwner->mPhase == 0)
		setHitParams(0.0f, 0.0f,
		             mOwner->getParams()->mSLFlameDamageRadius0.get(),
		             mOwner->getParams()->mSLFlameDamageHeight0.get());
	else if (mOwner->mPhase == 1)
		setHitParams(0.0f, 0.0f,
		             mOwner->getParams()->mSLFlameDamageRadius1.get(),
		             mOwner->getParams()->mSLFlameDamageHeight1.get());
}

void TTinKoopaFlame::resetTinKoopaFlame()
{
	mHitPoints = mOwner->getParams()->mSLFlameHP.get();
	unk6C      = 1.0f;
	mIsHit     = 0;
}

BOOL TTinKoopaFlame::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		if (mOwner->mFlameTimer <= 0) {
			if (mHitPoints > 0)
				mHitPoints--;

			if (mHitPoints <= 0) {
				mHitPoints = mOwner->getParams()->mSLFlameHP.get();
				mOwner->mFlameTimer
				    = (s16)mOwner->getParams()->mSLFlameRevivalTime.get();
				onHitFlag(HIT_FLAG_NO_COLLISION);
			}

			if (!mIsHit) {
				mIsHit = 1;
				gpMarioParticleManager->emitAndBindToPosPtr(0xF3, &mPosition,
				                                            0, this);
			}
		}
		return TRUE;
	}
	return FALSE;
}

void TTinKoopaFlame::hitWater()
{
	// TODO: UNUSED in the map (size 0xd4), contents unknown
}

void TTinKoopaFlame::perform(u32 cue, JDrama::TGraphics* graphics)
{
	THitActor::perform(cue, graphics);

	if (cue & CUE_MOVE) {
		if (mOwner->mFlameTimer <= 0 && mOwner->mPhase != 4) {
			int frame;
			if (mOwner->mPhase == 0)
				frame = 2750;
			else
				frame = 3400;

			if (mOwner->checkTruckAnimationPass(frame))
				SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
		}
	}

	if (cue & CUE_CALC_ANIM) {
		MtxPtr mtx
		    = mOwner->getModel()->getAnmMtx(TTinKoopa_jointIndexTable[9]);
		mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);
		emitFlameEffects();
		if (mOwner->mFlameTimer <= 0)
			offHitFlag(HIT_FLAG_NO_COLLISION);
		mIsHit = 0;
	}
}

void TTinKoopaFlame::checkMario()
{
	// TODO: UNUSED in the map (size 0xc4), contents unknown
}

bool TTinKoopaFlame::isHighPosition()
{
	// TODO: UNUSED in the map (size 0x18), contents unknown
	return false;
}

void TTinKoopaFlame::emitFlameEffects()
{
	// TODO: not decompiled yet
}

TTinKoopaLaunchOrder::TTinKoopaLaunchOrder(TTinKoopa* owner)
    : mOwner(owner)
    , mLap(0)
    , mFrame(0)
    , mCount(0)
    , mSide(0)
{
}

void TTinKoopaLaunchOrder::makeOrder(s8 lap, long frame, s8 count, s8 side)
{
	mLap   = lap;
	mFrame = frame;
	mCount = count;
	mSide  = side;
}

void TTinKoopaLaunchOrder::checkOrder()
{
	if (mOwner->mLap == mLap && mOwner->checkTruckAnimationPass(mFrame)) {
		int count = 1;
		if (mCount == -1) {
			if (mOwner->mPhase == 0)
				count = 1;
			else if (mOwner->mPhase == 1)
				count = 1;
			else if (mOwner->mPhase == 2)
				count = 2;
			else if (mOwner->mPhase == 3)
				count = 3;
		} else {
			count = mCount;
		}

		if (mSide == 1)
			count = count <= 2 ? count : 2;

		mOwner->makeKillerQueue(count, mSide);
	}
}

TTinKoopaLaunchSchedule::TTinKoopaLaunchSchedule(u8 num, TTinKoopa* owner)
    : mOrderNum(num)
    , mOwner(owner)
{
	mOrders = new TTinKoopaLaunchOrder*[mOrderNum];
	for (int i = 0; i < mOrderNum; ++i)
		mOrders[i] = new TTinKoopaLaunchOrder(mOwner);
}

void TTinKoopaLaunchSchedule::checkOrder()
{
	for (int i = 0; i < mOrderNum; ++i)
		mOrders[i]->checkOrder();
}

TTinKoopaPartsBase::TTinKoopaPartsBase(const char* name, int index,
                                       TTinKoopa* owner)
    : TLiveActor(name)
    , mIsBreaking(0)
    , mIndex(index)
    , mOwner(owner)
    , mBreakActor(nullptr)
{
}

void TTinKoopaPartsBase::initTinKoopaPartsBase()
{
	// TODO: not decompiled yet
}

void TTinKoopaPartsBase::reset()
{
	mIsBreaking = 0;
	mCollision->setUpTrans(JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f));
}

void TTinKoopaPartsBase::resetTinKoopaPartsBase()
{
	// TODO: UNUSED in the map (size 0x4c), contents unknown
}

void TTinKoopaPartsBase::startBreaking()
{
	// TODO: not decompiled yet
}

void TTinKoopaPartsBase::emitPartsTrackEffects()
{
	// TODO: UNUSED in the map (size 0x2fc), contents unknown
}

void TTinKoopaPartsBase::emitPartsTrackEffects(const char** joints, int num)
{
	// TODO: not decompiled yet
}

void TTinKoopaPartsBase::emitPartsDisappearEffects()
{
	// TODO: not decompiled yet
}

void TTinKoopaPartsBase::emitPartsDisappearEffects(const char** joints,
                                                   int num, f32 param_3)
{
	// TODO: not decompiled yet
}

BOOL TTinKoopaPartsBase::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->getActorType() == 0x1000002B) {
		mOwner->hitParts();
		return TRUE;
	}
	return FALSE;
}

void TTinKoopaPartsBase::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TLiveActor::perform(cue, graphics);

	if (cue & CUE_MOVE)
		mCollision->moveMtx(mOwner->getModel()->getAnmMtx(
		    TTinKoopa_jointIndexTable[mIndex]));

	if (cue & CUE_CALC_ANIM && mIsBreaking) {
		if (mBreakActor != nullptr && mBreakActor->curAnmEndsNext(0, nullptr))
			mIsBreaking = 0;
	}

	if (mIsBreaking) {
		if (mBreakActor != nullptr)
			mBreakActor->perform(cue, graphics);
	}
}

TTinKoopaMtxCalc::TTinKoopaMtxCalc(TTinKoopa* owner)
    : mOwner(owner)
{
	// TODO: UNUSED in the map (size 0x98), not verified
}

void TTinKoopaMtxCalc::joinAnm(int)
{
	// TODO: UNUSED in the map (size 0x38), contents unknown
}

void TTinKoopaMtxCalc::calc(u16 index) { M3UMtxCalcSIAnmBlendQuat::calc(index); }

TTinKoopa::TTinKoopa(const char* name)
    : TSpineEnemy(name)
    , mKillerManager(nullptr)
    , unk1F8(0)
{
	onLiveFlag(LIVE_FLAG_UNK10);
	offLiveFlag(LIVE_FLAG_UNK100);
	mScaledBodyRadius = 2000.0f;
}

void TTinKoopa::makeCoasterDistanceTable()
{
	// TODO: UNUSED in the map (size 0x16c), inlined into init
}

f32 TTinKoopa::calcCoasterDistance(int from, int to)
{
	f32 distance = 0.0f;
	for (int i = from; i < to; ++i)
		distance += mCoasterDistanceTable[i];
	return distance;
}

f32 TTinKoopa::calcCoasterDistanceInOrder(int, int)
{
	// TODO: UNUSED in the map (size 0x220), contents unknown
	return 0.0f;
}

bool TTinKoopa::checkKillerApproachingFromBack(TCoasterKiller*,
                                               JGeometry::TVec3<f32>, f32)
{
	// TODO: UNUSED in the map (size 0xdc), contents unknown
	return false;
}

void TTinKoopa::reset()
{
	TSpineEnemy::reset();
	mFlame->resetTinKoopaFlame();
	for (int i = 0; i < 6; ++i)
		mParts[i]->reset();
	makeHitCollision();
	mFlame->makeHitCollision();
	resetTinKoopa();
	changeBck(TTinKoopa_getWaitAnimationIndex(mPhase));
}

void TTinKoopa::resetTinKoopa()
{
	if (mKillerManager == nullptr)
		mKillerManager = (TEnemyManager*)JDrama::TNameRefGen::search(
		    "コースターキラーマネージャー");

	mTruck           = gpMarioOriginal->mKoopaRail;
	mPhase           = 0;
	unk154           = 0;
	unk158           = 0;
	mLap             = 0;
	unk168           = 0;
	mPartsHP         = getParams()->mSLPartsHP.get();
	mBreakingParts   = nullptr;
	mKillerQueueNum  = 0;
	mKillerQueueIdx  = 0;
	mKillerQueue[0]  = 0;
	mKillerQueue[1]  = 0;
	mKillerQueue[2]  = 0;
	mKillerQueue[3]  = 0;
	mKillerTimer     = 0;
	mFlameTimer      = 0;
	mDefeatTimer     = 0;
	mKillerTimer     = 0;
	unk1B4           = 0.0f;
	unk1B8           = 30.0f;
	unk1BC           = 15.0f;
	unk1C0           = 45.0f;
}

void TTinKoopa::makeHitCollision()
{
	// TODO: the target also compares mPhase against 2 afterwards, with no
	// code for it; the third branch is unknown
	if (mPhase == 0) {
		setHitParams(0.0f, 0.0f, getParams()->mSLDamageRadius.get(),
		             getParams()->mSLDamageHeight0.get());
	} else if (mPhase == 1) {
		setHitParams(0.0f, 0.0f, getParams()->mSLDamageRadius.get(),
		             getParams()->mSLDamageHeight1.get());
	}
}

void TTinKoopa::makeKillerQueue(int num, s8 side)
{
	if (num > 4)
		num = 4;
	mKillerQueueIdx = 0;
	mKillerQueueNum = num;
	for (int i = 0; i < mKillerQueueNum; ++i)
		mKillerQueue[i] = side;
	mKillerTimer = 0;
}

void TTinKoopa::checkLap()
{
	// TODO: UNUSED in the map (size 0x8c), contents unknown
}

bool TTinKoopa::checkTruckAnimationPass(int frame)
{
	if (mTruck == nullptr)
		return false;
	return mTruck->getFrameCtrl(0)->checkPass(frame) ? true : false;
}

void printTinKoopaDebugInfo(TTinKoopa*)
{
	// TODO: UNUSED in the map (size 0x24c), contents unknown
}

void TTinKoopa::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// TODO: not decompiled yet
	TSpineEnemy::perform(cue, graphics);
}

void TTinKoopa::makeEyeBeamEffect()
{
	// TODO: UNUSED in the map (size 0x3b4), contents unknown
}

void TTinKoopa::updateTimers()
{
	// TODO: UNUSED in the map (size 0x4c), contents unknown
}

const char** TTinKoopa::getBasNameTable() const { return tinkoopa_bastable; }

void TTinKoopa::changeBck(int index)
{
	mMActor->setBckFromIndex(index);
	setAnmSound(getBas(index));
}

BOOL TTinKoopa::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->getActorType() == 0x1000002B) {
		hitParts();
		return TRUE;
	}
	return FALSE;
}

void TTinKoopa::hitParts()
{
	if (mSpine->getCurrentNerve() != &TNerveTinKoopaBreak::theNerve()
	    && mSpine->getCurrentNerve() != &TNerveTinKoopaDamage::theNerve()
	    && mPhase != 4) {
		gpMarDirector->mConsole->startAppearBalloon(0x24, true);
		mPartsHP--;
		if (mPartsHP <= 0)
			mSpine->pushNerve(&TNerveTinKoopaBreak::theNerve());
		else
			mSpine->pushNerve(&TNerveTinKoopaDamage::theNerve());
	}
}

void TTinKoopa::startBreakingParts()
{
	// TODO: UNUSED in the map (size 0x4c), contents unknown
}

void TTinKoopa::launchKiller(int side)
{
	TSpineEnemy* killer = mKillerManager->getDeadEnemy();
	if (killer != nullptr) {
		killer->reset();
		int joint;
		if (side == 1)
			joint = mKillerQueueIdx + 11;
		else
			joint = 14 - mKillerQueueIdx;
		joint = TTinKoopa_jointIndexTable[joint];
		getJointTransByIndex(joint, &killer->mPosition);
		((TCoasterKiller*)killer)->mPathDir = side;
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0xEF, getModel()->getAnmMtx(joint), 0, this);
		SMSGetMSound()->startSoundActor(0x285D, &killer->mPosition, 0,
		                                nullptr, 0, 4);
	}
}

void TTinKoopa::checkKillerLaunch()
{
	// TODO: UNUSED in the map (size 0xd8), contents unknown
}

void TTinKoopa::checkTinKoopaMessage()
{
	// TODO: UNUSED in the map (size 0x1e0), contents unknown
}

void TTinKoopa::checkTinKoopaKillerApproachingMessage()
{
	// TODO: not decompiled yet
}

void TTinKoopa::checkTinKoopaFirstRocketMessage()
{
	// TODO: UNUSED in the map (size 0x6c), contents unknown
}

void TTinKoopa::checkTinKoopaFirstFlameMessage()
{
	if (mTruck != nullptr && unk168 == 0
	    && mSpine->getCurrentNerve() == &TNerveTinKoopaWait::theNerve()) {
		J3DFrameCtrl* ctrl = mTruck->getFrameCtrl(0);
		if (mPhase == 0) {
			if (ctrl->checkPass(2600.0f)) {
				gpMarDirector->mConsole->startAppearBalloon(0xB, true);
				unk168 = 1;
			}
		} else if (ctrl->checkPass(3100.0f)) {
			gpMarDirector->mConsole->startAppearBalloon(0xB, true);
			unk168 = 1;
		}
	}
}

void TTinKoopa::startTinKoopaMessage(u32)
{
	// TODO: UNUSED in the map (size 0x2c), contents unknown
}

void TTinKoopa::emitTinKoopaEffects()
{
	// TODO: not decompiled yet
}

TTinKoopaManager::TTinKoopaManager(const char* name)
    : TEnemyManager(name)
{
}

void TTinKoopaManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "tinkoopa_body.bmd", 0x10240000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TTinKoopaManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
	unk38 = new TTinKoopaParams("/enemy/tinkoopa.prm");
}

void TTinKoopaManager::loadAfter()
{
	static const char* onetimeFilenames[] = {
		"/scene/tinkoopa/jpa/ms_mkp_hibana_d1he.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_killer.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_smoke1.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_parge_b14.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_parge_b23.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_flame_yuge.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_parts.jpa",
	};
	for (int i = 0; i < 7; ++i)
		SMS_LoadParticle(onetimeFilenames[i], i + 0xEE);

	static const char* loopFilenames[] = {
		"/scene/tinkoopa/jpa/ms_mkp_hibana_w1br.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_hibana_w3ar.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_hibana_w4ar.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_biri_w1st.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_biri_w1ar.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_biri_w1fe.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_biri_w1he.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_biri_d1br_a.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_biri_d1br_b.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_b1ar.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_w2br_a.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_w2br_b.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_b1he.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_b1fe_l.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_b1fe_r.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_fire_a.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_fire_b.jpa",
	};
	for (int i = 0; i < 17; ++i)
		SMS_LoadParticle(loopFilenames[i], i + 0x1AC);

	static const char* loopIndirectFilenames[] = {
		"/scene/tinkoopa/jpa/ms_mkp_fire_c.jpa",
	};
	SMS_LoadParticle(loopIndirectFilenames[0], 0x1F2);
}

TSpineEnemy* TTinKoopaManager::createEnemyInstance() { return nullptr; }

DEFINE_NERVE(TNerveTinKoopaWait, TLiveActor)
{
	TTinKoopa* self = (TTinKoopa*)spine->getBody();
	if (spine->getTime() == 0) {
		self->changeBck(TTinKoopa_getWaitAnimationIndex(self->mPhase));
		self->mDefeatTimer = self->getParams()->mSLDefeatWaitTime.get();
	}

	if (self->mPhase == 4 && self->mDefeatTimer <= 0)
		TFlagManager::smInstance->setBool(true, 0x5000A);

	return FALSE;
}

DEFINE_NERVE(TNerveTinKoopaDamage, TLiveActor)
{
	TTinKoopa* self = (TTinKoopa*)spine->getBody();
	if (spine->getTime() == 0) {
		self->changeBck(TTinKoopa_getDamageAnimationIndex(self->mPhase));
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0xEE, self->getModel()->getAnmMtx(TTinKoopa_jointIndexTable[0]),
		    0, this);
		gpCameraShake->startShake(CAM_SHAKE_MODE_UNK6, 1.0f);
	}

	if (self->mMActor->checkCurBckFromIndex(
	        TTinKoopa_getDamageAnimationIndex(self->mPhase))
	    && self->mMActor->curAnmEndsNext(0, nullptr)) {
		self->changeBck(TTinKoopa_getWaitAnimationIndex(self->mPhase));
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveTinKoopaBreak, TLiveActor)
{
	TTinKoopa* self = (TTinKoopa*)spine->getBody();
	if (spine->getTime() == 0) {
		self->changeBck(TTinKoopa_getBreakAnimationIndex(self->mPhase));
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0xEE, self->getModel()->getAnmMtx(TTinKoopa_jointIndexTable[0]),
		    0, this);
		gpCameraShake->startShake(CAM_SHAKE_MODE_UNK6, 1.0f);
		if (self->mPhase == 3) {
			((TCoasterKillerManager*)self->mKillerManager)->unk60 = 1;
			MSBgm::stopTrackBGMs(7, 10);
		}
	}

	if (self->mMActor->checkCurBckFromIndex(
	        TTinKoopa_getBreakAnimationIndex(self->mPhase))) {
		TTinKoopaPartsBase* parts
		    = self->mParts[TTinKoopa_getBreakingPartsIndex(self->mPhase)];
		if (self->mMActor->curAnmEndsNext(0, nullptr)) {
			parts->mCollision->remove();
			self->mPhase++;
			self->makeHitCollision();
			self->mFlame->makeHitCollision();
			self->changeBck(TTinKoopa_getWaitAnimationIndex(self->mPhase));
			self->mPartsHP = self->getParams()->mSLPartsHP.get();
			return TRUE;
		}

		if (!parts->mIsBreaking
		    && self->mMActor->getFrameCtrl(0)->checkPass(
		        TTinKoopa_getPartsVisibleFrame(self->mPhase))) {
			self->mBreakingParts
			    = self->mParts[TTinKoopa_getBreakingPartsIndex(self->mPhase)];
			self->mBreakingParts->startBreaking();
		}
	}

	return FALSE;
}
