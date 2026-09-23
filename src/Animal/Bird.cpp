#include <Animal/Bird.hpp>
#include <Enemy/WireBinder.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MSound/MSoundSE.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
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
	u16 idx = getModel()->getModelData()->getMaterialName()->getIndex(cMatName);
	SMS_InitPacket_OneTevColor(getModel(), idx, GX_TEVREG1, color);
}

void TAnimalBird::initCollision()
{
	initHitActor(0x10000032, 0, 0, 50.0f, 50.0f, 70.0f, 80.0f);
	onHitFlag(HIT_FLAG_CANNOT_ATTACK);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	mScaledBodyRadius = 35.0f;
}

void TAnimalBird::initParams()
{
	mHomePosition = mPosition;
	mHomePosition.y += 90.0f;
	mHomeRotation = mRotation;
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

void TAnimalBird::load(JSUMemoryInputStream& stream)
{
	TSpineEnemy::load(stream);

	s32 eventId;
	stream.read(&eventId, 4);
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
		        gpMarDirector->getCurrentMap(), eventId))
			onLiveFlag(LIVE_FLAG_DEAD);
		break;
	}

	initTevColor(&cColorTable[mColorType]);
}

void TAnimalBird::loadAfter()
{
	JDrama::TNameRef::loadAfter();
	MSoundSESystem::MSRandPlay::registerTrans(0x3869, &mPosition);
	MSoundSESystem::MSRandPlay::registerTrans(0x3870, &mPosition);
}

BOOL TAnimalBird::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: not yet decompiled
	return false;
}

void TAnimalBird::calcRootMatrix()
{
	if (mHolder)
		MTXCopy(mHolder->getTakingMtx(), getModel()->getBaseTRMtx());
	else
		TSpineEnemy::calcRootMatrix();

	getModel()->getBaseTRMtx()[1][3] += 35.0f;
}

void TAnimalBird::moveObject()
{
	// TODO: not yet decompiled
}

void TAnimalBird::bind()
{
	if (isCheckWithWireBinder())
		mWireBinder->bind(this);
	else
		TLiveActor::bind();
}

const char** TAnimalBird::getBasNameTable() const { return bird_bastable; }

bool TAnimalBird::isOnGroundNerve() const
{
	bool result = mSpine->getLatestNerve()
	                  == &TNerveAnimalBirdWaitOnGround::theNerve()
	              || mSpine->getLatestNerve()
	                     == &TNerveAnimalBirdActionOnGround::theNerve()
	              || mSpine->getLatestNerve()
	                     == &TNerveAnimalBirdWalkOnGround::theNerve()
	              || mSpine->getLatestNerve()
	                     == &TNerveAnimalBirdPreLanding::theNerve();
	return result;
}

bool TAnimalBird::isFindMario() const
{
	if (fabs(SMS_GetMarioPos().y - mPosition.y)
	    > getBirdParams()->mSearchHeight.get())
		return false;

	return isInSight(SMS_GetMarioPos(),
	                 mRandomScale * getBirdParams()->mSearchLength.get(),
	                 mRandomScale * getBirdParams()->mSearchAngle.get(),
	                 mRandomScale * getBirdParams()->mSearchAware.get())
	       != false;
}

bool TAnimalBird::isCheckWithWireBinder() const
{
	bool result = false;
	if (mWireBinder && isOnGroundNerve())
		result = true;
	return result;
}

bool TAnimalBird::doLanding(bool param_1)
{
	// TODO: not yet decompiled
	return false;
}

void TAnimalBird::doFlyToCurPathNode()
{
	// TODO: not yet decompiled
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
	MSoundSESystem::MSRandPlay::createRandPlayVec(0x3869, mObjNum);
	MSoundSESystem::MSRandPlay::createRandPlayVec(0x3870, mObjNum);
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
	// TODO: not yet decompiled
	return false;
}

DEFINE_NERVE(TNerveAnimalBirdActionOnGround, TLiveActor)
{
	// TODO: not yet decompiled
	return false;
}

DEFINE_NERVE(TNerveAnimalBirdWalkOnGround, TLiveActor)
{
	// TODO: not yet decompiled
	return false;
}

DEFINE_NERVE(TNerveAnimalBirdTakeoff, TLiveActor)
{
	// TODO: not yet decompiled
	return false;
}

DEFINE_NERVE(TNerveAnimalBirdGraphWander, TLiveActor)
{
	// TODO: not yet decompiled
	return false;
}

DEFINE_NERVE(TNerveAnimalBirdChangeToCoin, TLiveActor)
{
	// TODO: not yet decompiled
	return false;
}

DEFINE_NERVE(TNerveAnimalBirdComeback, TLiveActor)
{
	// TODO: not yet decompiled
	return false;
}

DEFINE_NERVE(TNerveAnimalBirdPreLanding, TLiveActor)
{
	// TODO: not yet decompiled
	return false;
}

DEFINE_NERVE(TNerveAnimalBirdLanding, TLiveActor)
{
	// TODO: not yet decompiled
	return false;
}
