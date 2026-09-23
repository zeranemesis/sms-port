#include <Enemy/killer.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/EffectObj.hpp>
#include <Camera/CameraShake.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjBlock.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DCluster.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <dolphin/mtx.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* killer_bastable[] = {
	"/scene/killer/bas/downkiller_down1.bas", nullptr, nullptr,
	"/scene/killer/bas/killer_search1.bas",   nullptr,
};

bool TKiller::mSerialBomb        = true;
bool TKiller::mTrampleDie        = true;
f32 TFlyEnemy::mTestSp           = 2.5f;
int TFlyEnemy::mInvalidTime      = 200;
f32 TFlyEnemy::mTestMarioSpMax   = 12.0f;
static TKiller* gpCurKiller;
bool TKiller::mRollSw;

TFlyEnemyParams::TFlyEnemyParams(const char* path)
    : TWalkerEnemyParams(path)
    , PARAM_INIT(mSLNormalFlyGravityY, 0.2f)
    , PARAM_INIT(mSLNormalFlySpeed, 10.0f)
    , PARAM_INIT(mSLChaseFlyGravityY, 0.1f)
    , PARAM_INIT(mSLChaseDist, 2000.0f)
    , PARAM_INIT(mSLForceGravityY, 0.1f)
{
	TParams::load(mPrmPath);
}

void TFlyEnemy::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	unk19C = (TFlyEnemyParams*)getSaveParam();
}

f32 TFlyEnemy::getGravityY() const
{
	if (mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve())
		return unk194;
	return unk19C->mSLNormalFlyGravityY.get();
}

void TFlyEnemy::reset()
{
	TWalkerEnemy::reset();
	unk1A0 = 0;
	unk198 = 1;
	unk1A4 = 0;
	unk194 = unk19C->mSLNormalFlyGravityY.get();
	unk1A5 = 0;
}

void TFlyEnemy::fly()
{
	// TODO: not yet decompiled
}

void TFlyEnemy::calcChaseParam()
{
	// TODO: not yet decompiled
}

void TFlyEnemy::bind()
{
	if (mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve()
	    || unk1A0 < mInvalidTime) {
		fly();
		return;
	}

	TLiveActor::bind();
}

void TFlyEnemy::flyMove()
{
	// TODO: UNUSED in the map, size 0x2BC. Probably inlined into the nerves.
}

DEFINE_NERVE(TNerveFlyEnemyNormalFly, TLiveActor)
{
	// TODO: not yet decompiled
	return false;
}

DEFINE_NERVE(TNerveFlyEnemyChaseFly, TLiveActor)
{
	// TODO: not yet decompiled
	return false;
}

TKillerSaveLoadParams::TKillerSaveLoadParams(const char* path)
    : TFlyEnemyParams(path)
    , PARAM_INIT(mSLWaterAddGravityY, 1.0f)
    , PARAM_INIT(mSLChaseTimer, 1000)
    , PARAM_INIT(mSLBombRange, 300.0f)
{
	TParams::load(mPrmPath);
}

TKillerManager::TKillerManager(const char* name)
    : TSmallEnemyManager(name)
{
	gpCurKiller = nullptr;
}

void TKillerManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TKillerSaveLoadParams("/enemy/killer.prm");
}

void TKillerManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "killer_model1.bmd", 0x10220000, 0 },
		{ "downkiller_model1.bmd", 0x10220000, 0 },
		{ nullptr, 0, 0 },
	};

	createModelDataArray(entry);
}

TSpineEnemy* TKillerManager::createEnemyInstance() { return new TKiller; }

static BOOL KillerBodyCallback(J3DNode* node, BOOL param_2)
{
	// TODO: not yet decompiled
	return TRUE;
}

TKiller::TKiller(const char* name)
    : TFlyEnemy(name)
{
}

void TKiller::init(TLiveManager* manager)
{
	TFlyEnemy::init(manager);
	mActorType    = 0x1000001F;
	unk150        = 0x11;
	mKillerParams = (TKillerSaveLoadParams*)getSaveParam();
	mSpine->initWith(&TNerveFlyEnemyNormalFly::theNerve());
	onLiveFlag(LIVE_FLAG_UNK400);
	offLiveFlag(LIVE_FLAG_UNK800);
	onHitFlag(HIT_FLAG_UNK40000000);

	J3DModel* model = mMActor->getModel();
	if (!model->getSkinDeform())
		model->setSkinDeform(new J3DSkinDeform, J3D_DEFORM_ATTACH_FLAG_UNK_1);
	mMActor->resetDL();

	if (mInstanceIndex == 0) {
		for (u8 i = 0; i < getModel()->getModelData()->getJointNum(); ++i)
			;
	}

	mMActor->setJointCallback(1, KillerBodyCallback);
	unk188 = 0.0f;
}

void TKiller::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 2);
	mMActor       = mMActorKeeper->createMActor("killer_model1.bmd", 3);
	mMActorKeeper->createMActor("downkiller_model1.bmd", 3);

	u16 noseMatIdx = getActorKeeper()
	                     ->getMActor("killer_model1.bmd")
	                     ->getModel()
	                     ->getModelData()
	                     ->getMaterialName()
	                     ->getIndex("_nosemat1");
	u16 eyesMatIdx = getActorKeeper()
	                     ->getMActor("killer_model1.bmd")
	                     ->getModel()
	                     ->getModelData()
	                     ->getMaterialName()
	                     ->getIndex("_eyesmat1");
	u16 bodyMatIdx = getActorKeeper()
	                     ->getMActor("killer_model1.bmd")
	                     ->getModel()
	                     ->getModelData()
	                     ->getMaterialName()
	                     ->getIndex("_body1");

	SMS_InitPacket_OneTevColor(mMActor->getModel(), noseMatIdx, GX_TEVREG0,
	                           &mNoseColor);
	SMS_InitPacket_OneTevColor(mMActor->getModel(), eyesMatIdx, GX_TEVREG0,
	                           &mEyesColor);
	SMS_InitPacket_OneTevColor(mMActor->getModel(), bodyMatIdx, GX_TEVREG0,
	                           &mBodyColor);
	SMS_InitPacket_OneTevColor(
	    getActorKeeper()->getMActor("downkiller_model1.bmd")->getModel(),
	    bodyMatIdx, GX_TEVREG0, &mBaseColor);
}

void TKiller::behaveToWater(THitActor* water)
{
	if (mSpine->getCurrentNerve() == &TNerveFlyEnemyNormalFly::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve())
		genEventCoin();

	if (mSpine->getCurrentNerve() != &TNerveKillerExplosion::theNerve()) {
		mSpine->pushNerve(&TNerveKillerExplosion::theNerve());
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
	}
}

void TKiller::genEventCoin()
{
	int num = 2;
	if (unk1A6)
		num = 8;

	for (int i = 0; i < num; ++i) {
		JGeometry::TVec3<f32> offset(0.0f, 0.0f, 30.0f);
		Mtx mtx;
		MsMtxSetRotY(mtx, 360.0f * (1.0f / num) * (i + 1));
		MTXMultVec(mtx, &offset, &offset);
		TMapObjBase* coin = gpItemManager->makeObjAppear(
		    mPosition.x + offset.x, mPosition.y, mPosition.z + offset.z,
		    0x2000000E, true);
		if (coin) {
			coin->mPosition.y = mPosition.y;
			MsVECNormalize(&offset, &offset);
			coin->mVelocity.set(3.0f * offset.x, 20.0f, 3.0f * offset.z);
			coin->offLiveFlag(LIVE_FLAG_UNK10);
		}
	}
}

bool TKiller::isHitValid(u32 message)
{
	if (message == HIT_MESSAGE_UNKB) {
		onLiveFlag(LIVE_FLAG_DEAD);
		onHitFlag(HIT_FLAG_NO_COLLISION);
		genEventCoin();
		return false;
	}

	if (mTrampleDie)
		unk194 -= 12.0f;
	return false;
}

void TKiller::setDeadAnm()
{
	mMActor = getActorKeeper()->getMActor("downkiller_model1.bmd");
	setBckAnm(0);
	TEffectExplosion* effect
	    = (TEffectExplosion*)gpConductor->makeOneEnemyAppear(
	        mPosition, "エフェクト爆発マネージャー", 1);
	if (effect != nullptr)
		effect->generate(mPosition, mScaling);
	gpCameraShake->startShake(CAM_SHAKE_MODE_UNK6, 1.0f);
	SMSRumbleMgr->start(0x15, 5, (f32*)nullptr);
}

void TKiller::attackToMario()
{
	if (SMS_GetMarioPos().y < mPosition.y) {
		if (mSpine->getCurrentNerve() != &TNerveKillerExplosion::theNerve()) {
			mSpine->pushNerve(&TNerveKillerExplosion::theNerve());
			sendAttackMsgToMario();
			return;
		}
		SMS_SendMessageToMario(this, HIT_MESSAGE_UNKA);
	}
}

const char** TKiller::getBasNameTable() const { return killer_bastable; }

void TKiller::setNormalFlyAnm()
{
	mMActor = getActorKeeper()->getMActor("killer_model1.bmd");
	setBckAnm(2);
	unk1B8 = 0.0f;
	unk1A0 = 0;
}

void TKiller::setChaseFlyAnm() { setBckAnm(3); }

bool TKiller::isRollFly()
{
	if (mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve()
	    && isBckAnm(1))
		return true;
	return false;
}

bool TKiller::isCollidMove(THitActor* other)
{
	if (other->isActorType(0x4000000A))
		((TLiveActor*)other)->kill();

	if (mSerialBomb
	    && mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve())
		mSpine->pushNerve(&TNerveKillerExplosion::theNerve());

	return true;
}

void TKiller::flyBehavior()
{
	mTurnSpeed = mKillerParams->mSLTurnSpeedLow.get();
	if (mSpine->getTime() > mKillerParams->mSLChaseTimer.get())
		unk194 -= mKillerParams->mSLWaterAddGravityY.get();

	if (checkCurAnmEnd(0) && isBckAnm(3))
		setBckAnm(1);

	unk1B8 += 2.5f;
}

void TKiller::changeOut()
{
	SMSGetMSound()->startSoundActor(MSD_SE_EN_TELSA_RECOVER, &mPosition, 0,
	                                nullptr, 0, 4);
	onLiveFlag(LIVE_FLAG_DEAD);
	genEventCoin();
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mPosition = mJuiceBlock->mPosition;
	gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_TLS_CHANGE,
	                                            &mPosition, 0, nullptr);
	mMActor->setFrameRate(SMSGetAnmFrameRate(), 0);
	mJuiceBlock->kill();
	mJuiceBlock = nullptr;
}

void TKiller::reset()
{
	gpCurKiller = this;
	TFlyEnemy::reset();

	TMsRange<f32> range(0.0f, 1.0f);
	mBodyColor.r = mBodyColor.g = mBodyColor.b = mBaseColor.r = mBaseColor.g
	    = mBaseColor.b                         = 0;
	unk1A6                                     = 0;

	if (range.rand() < 0.05f) {
		unk1A6       = 1;
		mBodyColor.r = 200;
		mBodyColor.g = 185;
		mBodyColor.b = 0;
		mBaseColor.r = 255;
		mBaseColor.g = 225;
		mBaseColor.b = 70;
	}
}

void TKiller::setColorType()
{
	if (unk1A6)
		unk1A5 = 0;

	if (unk1A5) {
		mBodyColor.r = 70;
		mBodyColor.g = 20;
		mBodyColor.b = 70;
		mBaseColor.r = 70;
		mBaseColor.g = 20;
		mBaseColor.b = 70;
	}
}

void TKiller::bind()
{
	// TODO: not yet decompiled
}

void TKiller::calcRootMatrix()
{
	// TODO: not yet decompiled
}

bool TKiller::isFindMario(f32 param_1)
{
	TSmallEnemyParams* prms = getSaveParams();

	f32 searchHeight = prms->mSLSearchHeight.get();

	if (abs(SMS_GetMarioPos().y - mPosition.y) < searchHeight) {

		JGeometry::TVec3<f32> marioPos = SMS_GetMarioPos();

		f32 searchLength = prms->mSLSearchLength.get();
		f32 searchAngle  = prms->mSLSearchAngle.get();
		f32 searchAware  = prms->mSLSearchAware.get();

		if (isInSight(marioPos, searchLength * param_1, searchAngle * param_1,
		              searchAware * param_1))
			return true;
		else
			return false;
	}

	return false;
}

DEFINE_NERVE(TNerveKillerExplosion, TLiveActor)
{
	TKiller* self = (TKiller*)spine->getBody();

	if (spine->getTime() == 0) {
		self->unk20C = ((TKillerSaveLoadParams*)self->getSaveParam())
		                   ->mSLBombRange.get()
		               * self->getBodyScale() / self->getAttackRadius();
		self->mRotation.x = 0.0f;
		self->setDeadAnm();
		if (!self->isAirborne()) {
			if (self->getGroundPlane()->isWaterSurface()) {
				TEffectBombColumWater* water
				    = (TEffectBombColumWater*)gpConductor->makeOneEnemyAppear(
				        self->mPosition, "エフェクト爆発水柱マネージャー", 1);
				if (water) {
					JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
					water->generate(self->mPosition, scale);
				}
			}
			if (self->getGroundPlane()->isSand()) {
				TEffectColumSand* sand
				    = (TEffectColumSand*)gpConductor->makeOneEnemyAppear(
				        self->mPosition, "エフェクト砂柱マネージャー", 1);
				if (sand) {
					JGeometry::TVec3<f32> scale(0.6f, 0.9f, 0.6f);
					sand->generate(self->mPosition, scale);
				}
			}
		}
		SMSRumbleMgr->start(0x13, &self->mPosition);
	}

	if (self->unk190 < self->unk20C) {
		self->unk190 *= 1.3f;
	} else {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		if (self->checkCurAnmEnd(0)) {
			self->onLiveFlag(LIVE_FLAG_DEAD);
			self->onLiveFlag(LIVE_FLAG_UNK8);
			self->offLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
			self->mHolder = nullptr;
			self->stopAnmSound();
			spine->reset();
			spine->setNext(&TNerveSmallEnemyDie::theNerve());
			spine->pushAfterCurrent(spine->getDefault());
			self->mPosition.y -= 200.0f;
			return true;
		}
	}

	self->expandCollision();
	return false;
}
