#include <Enemy/popo.hpp>
#include <Enemy/Graph.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/ObjModel.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

// rogue include: mtx calc type names, needed to match the .rodata prologue
// (it drags in System/DummyStrings.hpp, which is needed too)
#include <M3DUtil/InfectiousStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// TODO: this translation unit started out freshly scaffolded from mario.MAP.
// The manager, constructor, parameter, init, reset, kill and receiveMessage
// paths are now matched; the nerve bodies, the model callbacks and the
// remaining behaviour hooks are still placeholders.

TPopo* gpCurPopo;

bool TPopo::mRollSw = true;
bool TPopo::mTriggerSw = true;
f32 TPopo::mTestAng_x = 90.0f;
f32 TPopo::mTestAng_y = 90.0f;
f32 TPopo::mTestAng_z;
f32 TPopo::mNozzleOffsetZ = -15.0f;
u8 TPopo::mCenterJntIndex = 1;
u8 TPopo::mMouthJntIndex = 2;
u8 TPopo::mRLegJntIndex = 5;
u8 TPopo::mLLegJntIndex = 11;
u8 TPopo::mRHandJntIndex = 7;
u8 TPopo::mLHandJntIndex = 9;
f32 TPopo::mTestBodyScale = 35.0f;
bool TPopo::mBrkFlag = true;
f32 TPopo::mColOffsetY = 20.0f;
f32 TPopo::mColMinVal = 0.6f;
bool TPopo::mExplosionSw;
bool TPopo::mLevelShootSw = true;

static const char* popo_bastable[] = {
	"/scene/popo/bas/popo_chase.bas",
	0,
	0,
	0,
	0,
	"/scene/popo/bas/popo_jump.bas",
	"/scene/popo/bas/popo_wait.bas",
};

DEFINE_NERVE(TNervePopoThrown, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNervePopoWait, TLiveActor)
{
	TPopo* self = (TPopo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->onLiveFlag(LIVE_FLAG_UNK10);
		self->receiveMessage(self, HIT_MESSAGE_PUT);
	}

	// both the current path node and its copy point at Mario, and the pending
	// path is reset
	self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));

	self->walkToCurPathNode(0.0f, 0.0f, self->mTurnSpeed);
	return FALSE;
}

DEFINE_NERVE(TNervePopoExplosion, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNervePopoFly, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNervePopoAttack, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNervePopoPossessedNozzle, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

const char** TPopo::getBasNameTable() const
{
	return (const char**)popo_bastable;
}

void TPopo::thrownByChorobei()
{
	// The vertebrae stack is cleared and the nerve set directly (no push),
	// which is TSpineBase::initWith().
	mSpine->initWith(&TNervePopoThrown::theNerve());
}

void TPopo::possessedIn()
{
	// The retail frame is 0x10 bytes larger than anything this body needs.
	char framePad_16_possessedIn[16];
	(void)framePad_16_possessedIn;
	mMActor = mMActorKeeper->getMActor("popoH.bmd");
	setBckAnm(3);
	mMActor->setBtpFromIndex(0);
	mMActor->setFrameRate(0.0f, ANM_TYPE_BTP);
	if (!mExplosionSw)
		onHitFlag(HIT_FLAG_NO_COLLISION);
	mMActor->setBrkFromIndex(0);
	mMActor->getFrameCtrl(ANM_TYPE_BRK)->setFrame(0.0f);
	unk1A0 = 30.0f;
	mMActor->setFrameRate(0.0f, ANM_TYPE_BRK);
	offLiveFlag(LIVE_FLAG_UNK10);
	unk1B8 = 90.0f;
	unk1B4 = true;
	gpMSound->startSoundActor(0x2861, &mPosition, 0, nullptr, 0, 4);
	unk1CC = 0;
	unk1CD = false;
}

void TPopo::explosion()
{
	// TODO: not yet decompiled
}

void TPopo::flyBehavior()
{
	// TODO: not yet decompiled
}

bool TPopo::isCollidMove(THitActor* hitActor)
{
	if (mSpine->getCurrentNerve() == &TNervePopoFly::theNerve()) {
		if (hitActor->receiveMessage(this, 0))
			mSpine->pushNerve(&TNervePopoExplosion::theNerve());
	}
	return false;
}

bool TPopo::isFindMario(float param_1)
{
	// TODO: not yet decompiled
	return false;
}

bool TPopo::isHitValid(u32 message)
{
	if (message == HIT_MESSAGE_UNKB)
		return true;

	if (message <= HIT_MESSAGE_HIP_DROP)
		mSpine->pushNerve(&TNervePopoExplosion::theNerve());

	return false;
}

void TPopo::bind()
{
	// TODO: not yet decompiled
}

void TPopo::forceKill()
{
	// TODO: not yet decompiled
}

void TPopo::kill()
{
	// The retail frame is 8 bytes larger than anything this body needs; the
	// slack is most likely a leftover temporary from the original source.
	char framePad_8_kill[8];
	(void)framePad_8_kill;
	if (unk1B4) {
		((TPopoManager*)mManager)->unk60 = 1;
		unk1B4 = 0;
	}
	TSmallEnemy::kill();
	unk23C->onHitFlag(HIT_FLAG_NO_COLLISION);
}

void TPopo::calcRootMatrix()
{
	// TODO: not yet decompiled
}

void TPopo::attackToMario()
{
	// TODO: not yet decompiled
}

void TPopo::walkBehavior(int param_1, float param_2)
{
	// TODO: not yet decompiled
}

void TPopo::behaveToFindMario()
{
	// TODO: not yet decompiled
}

f32 TPopo::getGravityY() const
{
	f32 gravity = mGravity;

	if (mSpine->getCurrentNerve() == &TNerveWalkerGraphWander::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveWalkerEscape::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveWalkerAttack::theNerve())
		return unk194->getMoveGravity();

	if (mSpine->getCurrentNerve() == &TNervePopoAttack::theNerve())
		gravity = unk194->getAttackGravity();
	else if (mSpine->getCurrentNerve() == &TNervePopoFly::theNerve())
		gravity = unk194->getFlyGravity();
	else if (mSpine->getCurrentNerve() == &TNervePopoThrown::theNerve())
		gravity = unk194->getThrownGravity();

	return gravity;
}

void TPopo::behaveToWater(THitActor* hitActor)
{
	// TODO: not yet decompiled
}

void TPopo::checkTrigger()
{
	// TODO: not yet decompiled
}

void TPopo::reset()
{
	gpCurPopo = this;
	TWalkerEnemy::reset();
	unk165 = false;
	unk1B4 = false;
	unk198 = 1.0f;
	unk1B8 = 0.0f;
	unk19C = 0;
	mScaledBodyRadius = mBodyScale * mBodyRadius * 15.0f;
	unk190 = 0.2f;
	expandCollision();
	mMActor = mMActorKeeper->getMActor("popoL.bmd");
	if (unk1A4) {
		onLiveFlag(LIVE_FLAG_UNK10);
		mSpine->initWith(&TNervePopoWait::theNerve());
		mPosition = unk1A8;
		offLiveFlag(LIVE_FLAG_UNK800);
	}
	unk23C->onHitFlag(HIT_FLAG_NO_COLLISION);
	unk18C = 0;
}

void TPopo::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 2);
	mMActor       = mMActorKeeper->createMActor("popoH.bmd", 3);
	mMActorKeeper->createMActor("popoL.bmd", 3);
}

void TPopo::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);
	unk23C->THitActor::perform(cue, graphics);
}

// TODO: callback signatures are guessed from J3D animation frame callback
// usage elsewhere; not yet verified against this file's call sites.
static int PopoNonScaleCallback(J3DNode*, int);
static int PopoPossessedCallback(J3DNode*, int);
static int PopoRollCallback(J3DNode*, int);

void TPopo::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	unk1A8 = mPosition;
	unk1A4 = 1;
	reset();
}

TPopo::TPopo(const char* name)
    : TWalkerEnemy(name)
    , unk194(0)
    , unk198(1.0f)
    , unk19C(0)
    , unk1A0(30.0f)
    , unk1A4(0)
    , unk1B4(0)
    , unk1B8(0.0f)
    , unk1CC(0)
    , unk1CD(false)
    , unk23C(nullptr)
{
}

// TODO: callback signatures are guessed from J3D animation frame callback
// usage elsewhere; not yet verified against this file's call sites.
// TODO: bodies not yet decompiled; the retail versions return 1.
static int PopoNonScaleCallback(J3DNode*, int)
{
	// TODO: not yet decompiled
	return 0;
}

static int PopoPossessedCallback(J3DNode*, int)
{
	// TODO: not yet decompiled
	return 0;
}

static int PopoRollCallback(J3DNode*, int)
{
	// TODO: not yet decompiled
	return 0;
}

BOOL TPopoCollision::receiveMessage(THitActor* sender, u32 message)
{
	// While the owner is flying, the collision body is inert; otherwise the
	// message goes straight to the owner's own hit-actor handling.
	TLiveActor* owner = (TLiveActor*)mOwner;
	if (owner->mSpine->getCurrentNerve() != &TNervePopoFly::theNerve())
		return mOwner->receiveMessage(sender, message);
	return FALSE;
}

void TPopoManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// As in TPopo::kill, the retail frame carries 8 bytes of slack that this
	// body never touches.
	char framePad_8_perform[8];
	(void)framePad_8_perform;
	// TODO: 0x1A4 is TPopo::unk1A4[0]; the flag is unnamed so far.
	if (cue & 1) {
		for (int i = 0; i < getActiveObjNum(); ++i) {
			TPopo* popo = (TPopo*)unk18[i];
			if (popo->unk1A4 && popo->checkLiveFlag(LIVE_FLAG_DEAD))
				popo->reset();
		}
	}
	TEnemyManager::perform(cue, graphics);
}

void TPopoManager::createModelData()
{
	// TODO: 0x210 is a raw J3DMLF_* combination; the two relevant bits
	// have not been identified yet.
	static TModelDataLoadEntry entry[] = {
		{ "popoH.bmd", 0x210, 0 },
		{ "popoL.bmd", 0x210, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TPopoManager::initSetEnemies()
{
	// The original computes isDummy() and compares it against FALSE, but the
	// outcome is discarded: this looks like a debug check whose body was never
	// written (or was stripped).
	TGraphWeb* graph = getObj(0)->getTracer()->getGraph();
	bool usable = graph != nullptr && graph->isDummy() == FALSE;
	(void)usable;
}

TSpineEnemy* TPopoManager::createEnemyInstance()
{
	return new TPopo;
}

TPopoSaveLoadParams::TPopoSaveLoadParams(const char* path)
    : TWalkerEnemyParams(path)
    , PARAM_INIT(mSLMoveDist, 100.0f)
    , PARAM_INIT(mSLMoveGravity, 0.1f)
    , PARAM_INIT(mSLMoveJumpSp, 10.0f)
    , PARAM_INIT(mSLAttackDist, 100.0f)
    , PARAM_INIT(mSLAttackGravity, 0.1f)
    , PARAM_INIT(mSLAttackJumpSp, 10.0f)
    , PARAM_INIT(mSLReleaseSpeed, 10.0f)
    , PARAM_INIT(mSLFlyGravity, 0.0f)
    , PARAM_INIT(mSLFlyLimitTime, 300)
    , PARAM_INIT(mSLExplosionEmitTime, 60)
    , PARAM_INIT(mSLWaterScaleMax, 2.0f)
    , PARAM_INIT(mSLThrownGravity, 0.5f)
    , PARAM_INIT(mSLPumpRate, 0.0001f)
    , PARAM_INIT(mSLLevelLimit, 1.2f)
    , PARAM_INIT(mSLScaleRate, 0.99f)
{
	TParams::load(mPrmPath);
}

void TPopoManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TPopoSaveLoadParams("/enemy/popo.prm");
	unk64 = new TWaterEmitInfo("/enemy/popowater.prm");
	unk68 = new TWaterEmitInfo("/enemy/popoexpwater.prm");
}

void TPopo::init(TLiveManager* liveManager)
{
	TWalkerEnemy::init(liveManager);

	mActorType = 0x100D;

	if (mInstanceIndex == 0) {
		// The loop body is empty in the retail build; getModel() is an
		// out-of-line call, so the comparison survives optimisation.
		J3DModelData* tables = getModel()->getModelData();
		for (u8 i = 0; i < tables->getJointNum(); ++i) {
		}
	}

	unk150 = 0x11;
	unk194 = (TPopoSaveLoadParams*)getSaveParam2();
	mSpine->initWith(&TNerveWalkerGraphWander::theNerve());
	onLiveFlag(LIVE_FLAG_UNK4000);

	mMActor->setJointCallback(mCenterJntIndex, PopoRollCallback);
	mMActorKeeper->getMActor("popoL.bmd")
	    ->setJointCallback(mCenterJntIndex, PopoRollCallback);
	mMActor->setJointCallback(mMouthJntIndex, PopoPossessedCallback);
	mMActor->setJointCallback(mRLegJntIndex, PopoNonScaleCallback);
	mMActor->setJointCallback(mLLegJntIndex, PopoNonScaleCallback);
	mMActor->setJointCallback(mRHandJntIndex, PopoNonScaleCallback);
	mMActor->setJointCallback(mLHandJntIndex, PopoNonScaleCallback);

	unk188 = 0.0f;
	unk23C = new TPopoCollision("ポポコリジョン");

	TEnemyNameRefGroup* group = (TEnemyNameRefGroup*)
	    JDrama::TNameRef::search("敵グループ");
	group->mObjects.insert(group->mObjects.end(), unk23C);

	unk23C->initHitActor(0x100D, 2, 0x9800, 80.0f, 80.0f, 80.0f, 80.0f);
	unk23C->onHitFlag(HIT_FLAG_NO_COLLISION);
	unk23C->setOwner(this);
}

TPopoManager::TPopoManager(const char* name)
    : TSmallEnemyManager(name)
    , unk60(1)
    , unk64(nullptr)
    , unk68(nullptr)
{
	gpCurPopo = nullptr;
	unk5C = 0;
}

TPopo::~TPopo()
{
	// TODO: not yet decompiled
}

TPopoCollision::~TPopoCollision()
{
	// TODO: not yet decompiled
}

TPopoManager::~TPopoManager()
{
	// TODO: not yet decompiled
}