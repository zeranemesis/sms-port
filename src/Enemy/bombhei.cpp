#include <Enemy/bombhei.hpp>

// TODO: this entire translation unit is freshly scaffolded from mario.MAP
// and m2c drafts. Only trivial destructors and getBasNameTable() are
// matched so far; the rest are placeholder stubs.

bool TBombHei::mSerialBomb;

static const char* bombhei_bastable[] = {
	// TODO: recover actual .bas paths from .rodata
	0,
};

DEFINE_NERVE(TNerveBombHeiExplosion, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBombHeiThrown, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBombHeiPickUp, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBombHeiWaitExplosion, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBombHeiWalkExplosion, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBombHeiAttack, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBombHeiGenerate, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

const char** TBombHei::getBasNameTable() const
{
	return (const char**)bombhei_bastable;
}

void TBombHei::isDamageToCannon()
{
	// TODO: not yet decompiled
}

void TBombHei::forceKill()
{
	// TODO: not yet decompiled
}

bool TBombHei::isCollidMove(THitActor* hitActor)
{
	// TODO: not yet decompiled
	return false;
}

void TBombHei::moveObject()
{
	// TODO: not yet decompiled
}

void TBombHei::walkBehavior(int param_1, float param_2)
{
	// TODO: not yet decompiled
}

f32 TBombHei::getGravityY() const
{
	// TODO: not yet decompiled
	return 0.0f;
}

void TBombHei::reset()
{
	// TODO: not yet decompiled
}

void TBombHei::behaveToRelease()
{
	// TODO: not yet decompiled
}

void TBombHei::behaveToTaken(THitActor* hitActor)
{
	// TODO: not yet decompiled
}

void TBombHei::attackToMario()
{
	// TODO: not yet decompiled
}

void TBombHei::calcRootMatrix()
{
	// TODO: not yet decompiled
}

void TBombHei::setDeadAnm()
{
	// TODO: not yet decompiled
}

void TBombHei::setFreezeAnm()
{
	// TODO: not yet decompiled
}

void TBombHei::setWalkAnm()
{
	// TODO: not yet decompiled
}

void TBombHei::genEventCoin()
{
	// TODO: not yet decompiled
}

void TBombHei::kill()
{
	// TODO: not yet decompiled
}

bool TBombHei::isHitValid(u32 message)
{
	// TODO: not yet decompiled
	return false;
}

void TBombHei::changeOut()
{
	// TODO: not yet decompiled
}

void TBombHei::behaveToWater(THitActor* hitActor)
{
	// TODO: not yet decompiled
}

void TBombHei::setMActorAndKeeper()
{
	// TODO: not yet decompiled
}

void TBombHei::init(TLiveManager* liveManager)
{
	// TODO: not yet decompiled
}

TBombHei::TBombHei(const char* name)
    : TSmallEnemy(name)
{
	// TODO: not yet decompiled
}

TSpineEnemy* TBombHeiManager::createEnemyInstance()
{
	// TODO: not yet decompiled
	return 0;
}

void TBombHeiManager::createModelData()
{
	// TODO: not yet decompiled
}

void TBombHeiManager::load(JSUMemoryInputStream& stream)
{
	// TODO: not yet decompiled
}

TBombHeiManager::TBombHeiManager(const char* name)
    : TSmallEnemyManager(name)
{
	// TODO: not yet decompiled
}

void TBombHei::setAfterDeadEffect()
{
	// TODO: not yet decompiled
}

bool TBombHei::doKeepDistance()
{
	// TODO: not yet decompiled
	return false;
}

TBombHei::~TBombHei()
{
	// TODO: not yet decompiled
}

TBombHeiManager::~TBombHeiManager()
{
	// TODO: not yet decompiled
}

void TBombHeiManager::clipEnemies(JDrama::TGraphics* graphics)
{
	// TODO: not yet decompiled
}
