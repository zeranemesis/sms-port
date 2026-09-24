#include <Enemy/effectEnemy.hpp>
#include <System/Particles.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>

void TEffectEnemyManager::initSetEnemies() {}

TEffectEnemy::TEffectEnemy(const char* name)
    : TWalkerEnemy(name)
    , unk194(0)
{
}

TSpineEnemy* TEffectEnemyManager::createEnemyInstance()
{
	return new TEffectEnemy("エフェクト敵");
}

void TEffectEnemyManager::loadAfter() { JDrama::TNameRef::loadAfter(); }

void TEffectEnemy::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000005;
}

void TEffectEnemy::reset() { TWalkerEnemy::reset(); }

void TEffectEnemy::kill()
{
	setDeadAnm();
	onLiveFlag(LIVE_FLAG_DEAD);
	onHitFlag(HIT_FLAG_NO_COLLISION);
}

void TEffectEnemy::setDeadAnm()
{
	gpMarioParticleManager->emitAndBindToPosPtr(0x8B, &mPosition, 0, nullptr);
	if (gpMSound->gateCheck(0x28C5))
		MSoundSESystem::MSoundSE::startSoundActor(0x28C5, &mPosition, 0, nullptr,
		                                          0, 4);
	onLiveFlag(LIVE_FLAG_UNK20000);
}

void TEffectEnemy::behaveToWater(THitActor* actor)
{
	if (mHitPoints > 1)
		TSmallEnemy::behaveToWater(actor);
	else
		kill();
}

// TODO: the remaining functions in this unit were not attempted in the
// time budget available for this pass: ::init, ::setMActorAndKeeper,
// ::kill, ::forceKill, ::perform,
// ::reset, ::behaveToWater, ::sendAttackMsgToMario, ::setDeadAnm,
// ::~TEffectEnemy, TEffectEnemyManager::~TEffectEnemyManager, ::load,
// and __sinit_effectEnemy_cpp (a 764B
// static-initializer block that almost certainly builds a params/name
// table and would require reading many constants out of the binary).
