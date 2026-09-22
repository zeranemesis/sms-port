#ifndef ENEMY_EFFECT_ENEMY_HPP
#define ENEMY_EFFECT_ENEMY_HPP

#include <Enemy/WalkerEnemy.hpp>
#include <Enemy/SmallEnemy.hpp>

// NOTE: this header is INCOMPLETE. Base classes were confirmed from
// disassembly: TEffectEnemy's ctor calls TWalkerEnemy::TWalkerEnemy(const
// char*); TEffectEnemyManager's dtor tears down through
// TSmallEnemyManager::__vtable then calls TEnemyManager::~TEnemyManager().
// Only the one member function actually implemented in this pass
// (TEffectEnemyManager::initSetEnemies) is declared below; the other 16
// functions in effectEnemy.cpp (ctor/dtor, init, perform, forceKill, kill,
// reset, behaveToWater, sendAttackMsgToMario, setDeadAnm,
// setMActorAndKeeper, load, loadAfter, createEnemyInstance, __sinit) were
// not reached in this pass.
class TEffectEnemy : public TWalkerEnemy {
public:
	TEffectEnemy(const char* name);
};

class TEffectEnemyManager : public TSmallEnemyManager {
public:
	virtual void initSetEnemies();
};

#endif // ENEMY_EFFECT_ENEMY_HPP
