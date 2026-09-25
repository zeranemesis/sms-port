#include <Enemy/popo.hpp>

// rogue include: dummy string pair, needed to match the .rodata prologue
#include <System/DummyStrings.hpp>

// TODO: this entire translation unit is freshly scaffolded from mario.MAP and
// m2c drafts. Only trivial functions have been matched so far; most bodies
// below are placeholders and are known non-matching.

TPopo* gpCurPopo;

bool TPopo::mRollSw;
bool TPopo::mTriggerSw;
f32 TPopo::mTestAng_x;
f32 TPopo::mTestAng_y;
f32 TPopo::mTestAng_z;
f32 TPopo::mNozzleOffsetZ;
u8 TPopo::mCenterJntIndex;
u8 TPopo::mMouthJntIndex;
u8 TPopo::mRLegJntIndex;
u8 TPopo::mLLegJntIndex;
u8 TPopo::mRHandJntIndex;
u8 TPopo::mLHandJntIndex;
f32 TPopo::mTestBodyScale;
bool TPopo::mBrkFlag;
f32 TPopo::mColOffsetY;
f32 TPopo::mColMinVal;
bool TPopo::mExplosionSw;
bool TPopo::mLevelShootSw;

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
	// TODO: not yet decompiled
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
	// TODO: not yet decompiled
}

void TPopo::possessedIn()
{
	// TODO: not yet decompiled
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
	// TODO: not yet decompiled
	return false;
}

bool TPopo::isFindMario(float param_1)
{
	// TODO: not yet decompiled
	return false;
}

bool TPopo::isHitValid(u32 message)
{
	// TODO: not yet decompiled
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
	// TODO: not yet decompiled
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
	// TODO: not yet decompiled
	return 0.0f;
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
	// TODO: not yet decompiled
}

void TPopo::setMActorAndKeeper()
{
	// TODO: not yet decompiled
}

void TPopo::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// TODO: not yet decompiled
}

void TPopo::init(TLiveManager* liveManager)
{
	// TODO: not yet decompiled
}

void TPopo::load(JSUMemoryInputStream& stream)
{
	// TODO: not yet decompiled
}

TPopo::TPopo(const char* name)
    : TWalkerEnemy(name)
{
	// TODO: not yet decompiled
}

// TODO: callback signatures are guessed from J3D animation frame callback
// usage elsewhere; not yet verified against this file's call sites.
static void PopoNonScaleCallback(J3DNode*, int)
{
	// TODO: not yet decompiled
}

static void PopoPossessedCallback(J3DNode*, int)
{
	// TODO: not yet decompiled
}

static void PopoRollCallback(J3DNode*, int)
{
	// TODO: not yet decompiled
}

BOOL TPopoCollision::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: not yet decompiled. UNUSED functions kill() and checkHit() are
	// likely inlined here.
	return FALSE;
}

void TPopoManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// TODO: not yet decompiled
}

void TPopoManager::createModelData()
{
	// TODO: not yet decompiled
}

void TPopoManager::initSetEnemies()
{
	// TODO: not yet decompiled
}

TSpineEnemy* TPopoManager::createEnemyInstance()
{
	// TODO: not yet decompiled
	return 0;
}

void TPopoManager::load(JSUMemoryInputStream& stream)
{
	// TODO: not yet decompiled
}

TPopoManager::TPopoManager(const char* name)
    : TSmallEnemyManager(name)
{
	// TODO: not yet decompiled
}

TPopoSaveLoadParams::TPopoSaveLoadParams(const char* path)
    : TWalkerEnemyParams(path)
    // TODO: default values not yet verified against .sdata2 float literals
    , PARAM_INIT(mSLMoveDist, 0.0f)
    , PARAM_INIT(mSLMoveGravity, 0.0f)
    , PARAM_INIT(mSLMoveJumpSp, 0.0f)
    , PARAM_INIT(mSLAttackDist, 0.0f)
    , PARAM_INIT(mSLAttackGravity, 0.0f)
    , PARAM_INIT(mSLAttackJumpSp, 0.0f)
    , PARAM_INIT(mSLReleaseSpeed, 0.0f)
    , PARAM_INIT(mSLFlyGravity, 0.0f)
    , PARAM_INIT(mSLFlyLimitTime, 0)
    , PARAM_INIT(mSLExplosionEmitTime, 0)
    , PARAM_INIT(mSLWaterScaleMax, 0.0f)
    , PARAM_INIT(mSLThrownGravity, 0.0f)
    , PARAM_INIT(mSLPumpRate, 0.0f)
    , PARAM_INIT(mSLLevelLimit, 0)
    , PARAM_INIT(mSLScaleRate, 0.0f)
{
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
