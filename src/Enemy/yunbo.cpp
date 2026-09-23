#include <Enemy/Yumbo.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Strategy.hpp>
#include <Map/Map.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Player/MarioAccess.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JGeometry/JGQuat4.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <stdlib.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

extern f32 SMSGetAnmFrameRate();

// This TU is -inline deferred: the definition order below is the reverse of
// the .text layout in marioEU.MAP.

static const char* sambohead_bastable[] = {
	"/scene/sambohead/bas/flower_shoot.bas",
	"/scene/sambohead/bas/samboHead_crash.bas",
	"/scene/sambohead/bas/samboHead_dance.bas",
	"/scene/sambohead/bas/samboHead_down.bas",
	"/scene/sambohead/bas/samboHead_Fhide.bas",
	"/scene/sambohead/bas/samboHead_hit.bas",
	"/scene/sambohead/bas/samboHead_hit_end.bas",
	"/scene/sambohead/bas/samboHead_jump_end.bas",
	"/scene/sambohead/bas/samboHead_jump_start.bas",
	nullptr,
	"/scene/sambohead/bas/samboHead_set.bas",
	"/scene/sambohead/bas/samboHead_turn.bas",
	nullptr,
};

TYumboSeed::TYumboSeed(MActor* actor, const TYumbo& owner)
    : THitActor("ユンボ種")
    , mOwner(&owner)
    , mActor(actor)
    , mSeedFlags(SEED_FLAG_UNUSED)
{
}

void TYumboSeed::init()
{
	initHitActor(0x1000002A, 1, 0x80000000, 30.0f, 30.0f, 0.0f, 0.0f);
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("弾グループ"))
	    ->getChildren()
	    .push_back(this);
}

void TYumboSeed::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (mSeedFlags & SEED_FLAG_UNUSED)
		return;

	if (cue & CUE_CALC_ANIM) {
		TPosition3f mtx;
		mtx.translation(mPosition);
		mActor->getModel()->setBaseScale(mScaling);
		MTXCopy(mtx, mActor->getModel()->getBaseTRMtx());
		mActor->getModel()->calc();
	}

	if (cue & CUE_MOVE) {
		mVelocity.y -= mOwner->getSaveLoadParam()->mSeedGravityY.get();
		mPosition += mVelocity;
		mVelocity.scale(mOwner->getSaveLoadParam()->mSeedAirFric.get());
		checkHitActors();
		if (--mLife == 0) {
			mSeedFlags |= SEED_FLAG_UNUSED;
			onHitFlag(HIT_FLAG_NO_COLLISION);
		}
	}

	if (!(mSeedFlags & SEED_FLAG_UNK4))
		mActor->perform(cue, graphics);
}

void TYumboSeed::checkHitActors()
{
	for (int i = 0; i < mColCount; ++i) {
		if (mCollisions[i]->mActorType == 0x80000001) {
			SMS_SendMessageToMario(this, 0xE);
			mSeedFlags |= SEED_FLAG_UNUSED;
		}
	}
}

void TYumboSeed::startToMove(const JGeometry::TVec3<f32>& pos,
                             const JGeometry::TVec3<f32>& velocity, int life)
{
	mSeedFlags &= ~SEED_FLAG_UNUSED;
	mPosition = pos;
	mVelocity = velocity;
	mLife     = life;
	mScaling.set(2.0f, 2.0f, 2.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
}

TYumbo::TYumbo(const char* name)
    : TSmallEnemy(name)
{
	onLiveFlag(LIVE_FLAG_UNK10);
}

void TYumbo::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	initMActorAndKeeper();
	mSpine->initWith(&TNerveYumboDancing::theNerve());

	for (TYumboSeed** it = mSeeds; it != mSeeds + 16; ++it) {
		*it = new TYumboSeed(mMActorKeeper->createMActor("samboSeed.bmd", 3),
		                     *this);
		(*it)->init();
	}

	initCollision();
	mScaledBodyRadius = 75.0f;
	mScaling.x = mScaling.y = mScaling.z = 1.5f;
	initAnmSound();
	mCenterJointIndex
	    = getModel()->getModelData()->getJointName()->getIndex("center");
}

void TYumbo::reset() { }

void TYumbo::initMActorAndKeeper()
{
	mMActorKeeper  = new TMActorKeeper(mManager, 0x12);
	MActor* yumbo  = mMActorKeeper->createMActor("yumbo.bmd", 0);
	MActor* flower = mMActorKeeper->createMActor("flower.bmd", 0);
	setMaterialToMActor(flower,
	                    ((TYumboManager*)mManager)->mFlowerMaterialTable);
	mMActor = yumbo;
}

void TYumbo::setMaterialToMActor(MActor* actor, J3DMaterialTable* table)
{
	actor->getModel()->getModelData()->setMaterialTable(
	    table, (J3DMaterialCopyFlag)3);
	actor->initDL();
	actor->getModel()->lock();
}

void TYumbo::initCollision()
{
	initHitActor(0x1000002A, 1, 0x80000000, 97.5f, 225.0f, 90.0f, 225.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	mGroundHeight = gpMap->checkGround(
	    mPosition.x, mPosition.y + mHeadHeight, mPosition.z, &mGroundPlane);
}

BOOL TYumbo::receiveMessage(THitActor* sender, u32 message)
{
	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return FALSE;

	switch (message) {
	case HIT_MESSAGE_TRAMPLE:
	case HIT_MESSAGE_HIP_DROP:
		if (!isFreeze())
			return FALSE;
		behaveHitAttack();
		return TRUE;
	}

	return TSmallEnemy::receiveMessage(sender, message);
}

void TYumbo::moveObject()
{
	if (!isChangedBlock())
		updateEffect();
	updateCollision();
	updateSquareToMario();
	TSmallEnemy::moveObject();
}

void TYumbo::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);
	for (TYumboSeed** it = mSeeds; it != mSeeds + 16; ++it)
		(*it)->perform(cue, graphics);
}

void TYumbo::behaveToWater(THitActor*)
{
	if (!isWaterproof()) {
		mSpine->reset();
		mSpine->setNext(&TNerveYumboFreeze::theNerve());
	}
}

void TYumbo::behaveHitAttack()
{
	mSpine->reset();
	mSpine->setNext(&TNerveSmallEnemyDie::theNerve());
}

void TYumbo::updateCollision()
{
	if (isFreeze() || isDead())
		onHitFlag(HIT_FLAG_CANNOT_ATTACK);
	else
		offHitFlag(HIT_FLAG_CANNOT_ATTACK);
}

void TYumbo::updateEffect()
{
	if (isFreeze()) {
		JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToPosPtr(
		    PARTICLE_MS_POI_KIZETSU, &mPosition, 1, this);
		if (emitter)
			emitter->setGlobalScale(mScaling);
	}

	if (mMActor->checkCurAnm("sambohead_dance", 0))
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    PARTICLE_MS_YNB_ONPU, getModel()->getAnmMtx(mCenterJointIndex), 1,
		    this);
}

bool TYumbo::isFindOutMario() const
{
	if (fabsf(gpMarioPos->y - mPosition.y)
	    < getSaveLoadParam()->mSLSearchHeight.get()) {
		JGeometry::TVec3<f32> target(gpMarioPos->x, mPosition.y,
		                             gpMarioPos->z);
		if (isInSight(target, getSaveLoadParam()->mSLSearchLength.get(),
		              getSaveLoadParam()->mSLSearchAngle.get(),
		              getSaveLoadParam()->mSLSearchAware.get()))
			return true;
		return false;
	}
	return false;
}

bool TYumbo::isWantToAppear() const
{
	if (getSaveLoadParam()->mSLGiveUpHeight.get()
	    <= fabsf(gpMarioPos->y - mPosition.y))
		return true;

	JGeometry::TVec3<f32> diff = *gpMarioPos;
	diff -= mPosition;
	diff.y      = 0.0f;
	f32 giveUp = getSaveLoadParam()->mSLGiveUpLength.get();
	return giveUp * giveUp < diff.squared();
}

bool TYumbo::isAllSeedBroken() const
{
	for (TYumboSeed* const* it = mSeeds; it != mSeeds + 16; ++it)
		if (!((*it)->mSeedFlags & TYumboSeed::SEED_FLAG_UNUSED))
			return false;
	return true;
}

bool TYumbo::isChangedBlock() const
{
	return mSpine->getLatestNerve() == &TNerveSmallEnemyChange::theNerve();
}

// TODO: the quaternion part is a rough guess and does not match yet
void TYumbo::shotSeeds()
{
	TYumboSeed* seed = getUnusedSeed();
	if (seed == nullptr)
		return;

	MActor* actor = mMActor;
	actor->setBck(0);
	setCurAnmSound();

	JGeometry::TVec3<f32> velocity = *gpMarioPos;
	velocity -= mPosition;
	velocity.y += 200.0f * (0.5f + rand() * (1.0f / 32768.0f));
	velocity.setLength(getSaveLoadParam()->mShootSpeed.get());

	JGeometry::TQuat4<f32> yaw;
	yaw.setRotate(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f),
	              -(MsGetRotFromZaxisY(velocity) * (3.1415927f / 180.0f)));
	yaw.rotate(velocity);

	JGeometry::TQuat4<f32> spin;
	spin.setRotate(JGeometry::TVec3<f32>(0.0f, 0.0f, 1.0f),
	               6.2831855f * (rand() * (1.0f / 32768.0f)));
	JGeometry::TQuat4<f32> tilt;
	tilt.setRotate(JGeometry::TVec3<f32>(1.0f, 0.0f, 0.0f),
	               -3.1415927f * getSaveLoadParam()->mShootAngleX.get());
	JGeometry::TQuat4<f32> rot;
	rot.mul(spin, tilt);
	rot.rotate(velocity);

	seed->startToMove(mPosition, velocity,
	                  getSaveLoadParam()->mSeedLife.get());
}

void TYumbo::lookatMario()
{
	JGeometry::TVec3<f32> diff = *gpMarioPos;
	diff -= mPosition;
	mRotation.y = MsGetRotFromZaxisY(diff);
}

void TYumbo::changeToFlower()
{
	mMActor = mMActorKeeper->getMActor("flower.bmd");
}

void TYumbo::changeToYumbo()
{
	mMActor = mMActorKeeper->getMActor("yumbo.bmd");
}

bool TYumbo::isWaterproof() const
{
	const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();
	return nerve == &TNerveYumboFreeze::theNerve()
	       || nerve == &TNerveSmallEnemyDie::theNerve()
	       || nerve == &TNerveYumboHiding::theNerve()
	       || nerve == &TNerveYumboAttack::theNerve();
}

bool TYumbo::isFreeze() const
{
	return mSpine->getLatestNerve() == &TNerveYumboFreeze::theNerve();
}

bool TYumbo::isDead() const
{
	return mSpine->getLatestNerve() == &TNerveSmallEnemyDie::theNerve();
}

TYumboSeed* TYumbo::getUnusedSeed()
{
	for (TYumboSeed** it = mSeeds; it != mSeeds + 16; ++it)
		if ((*it)->mSeedFlags & TYumboSeed::SEED_FLAG_UNUSED)
			return *it;
	return nullptr;
}

void TYumbo::setDeadAnm() { setBckAnm(3); }

bool TYumbo::doKeepDistance() { return isFreeze(); }

void TYumbo::attackToMario()
{
	if (!isFreeze())
		sendAttackMsgToMario();
}

TYumboParams::TYumboParams(const char* path)
    : TSmallEnemyParams(path)
    , PARAM_INIT(mRecoverTimer, 600)
    , PARAM_INIT(mShootSpeed, 55.0f)
    , PARAM_INIT(mShootAngleX, 0.2f)
    , PARAM_INIT(mSeedLife, 80)
    , PARAM_INIT(mSeedAirFric, 0.94f)
    , PARAM_INIT(mSeedGravityY, 0.9f)
{
	TParams::load(mPrmPath);
}

TYumboManager::TYumboManager(const char* name)
    : TSmallEnemyManager(name)
    , mFlowerMaterialTable(nullptr)
{
}

void TYumboManager::load(JSUMemoryInputStream& stream)
{
	TYumboParams* params = new TYumboParams("/enemy/Yumbo.prm");
	unk38                = params;
	TSmallEnemyManager::load(stream);
	params->mSLAttackRadius.set(97);
	params->mSLAttackHeight.set(225);
	params->mSLDamageRadius.set(90);
	params->mSLDamageHeight.set(225);
	loadMaterialTable(&mFlowerMaterialTable,
	                  "/scene/samboHead/flower_blue.bmt");
}

void TYumboManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "yumbo.bmd", 0x10210000, 0 },
		{ "flower.bmd", 0x10210000, 0 },
		{ 0 },
	};
	createModelDataArray(entry);
}

void TYumboManager::loadMaterialTable(J3DMaterialTable** table,
                                      const char* path)
{
	*table = J3DModelLoaderDataBase::loadMaterialTable(
	    JKRFileLoader::getGlbResource(path));
}

const char** TYumbo::getBasNameTable() const { return sambohead_bastable; }

DEFINE_NERVE(TNerveYumboDancing, TLiveActor)
{
	TYumbo* self = (TYumbo*)spine->getBody();
	if (spine->getTime() == 0)
		self->setBckAnm(2);

	self->lookatMario();
	if (self->isFindOutMario()) {
		spine->pushAfterCurrent(&TNerveYumboHiding::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveYumboHiding, TLiveActor)
{
	TYumbo* self = (TYumbo*)spine->getBody();
	if (spine->getTime() == 0) {
		self->setBckAnm(4);
		self->getMActor()->getFrameCtrl(0)->setRate(3.0f
		                                            * SMSGetAnmFrameRate());
		self->mHideEffectDone = false;
		SMSGetMSound()->startSoundActor(MSD_SE_EN_YUMBO_SINK, &self->mPosition,
		                                0, nullptr, 0, 4);
	}

	if (!self->mHideEffectDone
	    && self->getMActor()->getFrameCtrl(0)->checkPass(34.0f)) {
		gpMarioParticleManager->emit(PARTICLE_MS_SMB_AP_ROCK, &self->mPosition,
		                             0, nullptr);
		gpMarioParticleManager->emit(PARTICLE_MS_SMB_AP_SMOKE,
		                             &self->mPosition, 0, nullptr);
		self->mHideEffectDone = true;
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveYumboAttack::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveYumboAppearing, TLiveActor)
{
	TYumbo* self = (TYumbo*)spine->getBody();
	if (spine->getTime() == 0) {
		self->changeToYumbo();
		self->setBckAnm(10);
		gpMarioParticleManager->emit(PARTICLE_MS_SMB_AP_ROCK, &self->mPosition,
		                             0, nullptr);
		gpMarioParticleManager->emit(PARTICLE_MS_SMB_AP_SMOKE,
		                             &self->mPosition, 0, nullptr);
	}

	self->lookatMario();
	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveYumboDancing::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveYumboAttack, TLiveActor)
{
	TYumbo* self = (TYumbo*)spine->getBody();
	if (spine->getTime() == 0) {
		self->changeToFlower();
		self->shotSeeds();
	}

	if (self->isWantToAppear()) {
		spine->pushAfterCurrent(&TNerveYumboAppearing::theNerve());
		return TRUE;
	}

	if (self->getSaveLoadParam()->mSeedLife.get() / 16 < spine->getTime()) {
		spine->pushAfterCurrent(this);
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveYumboFreeze, TLiveActor)
{
	TYumbo* self = (TYumbo*)spine->getBody();
	if (spine->getTime() == 0) {
		self->setBckAnm(11);
		SMSGetMSound()->startSoundActor(MSD_SE_EN_COMMON_TWINKLE,
		                                &self->mPosition, 0, nullptr, 0, 4);
	}

	if (self->getSaveLoadParam()->mRecoverTimer.get() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveYumboDancing::theNerve());
		return TRUE;
	}
	return FALSE;
}
