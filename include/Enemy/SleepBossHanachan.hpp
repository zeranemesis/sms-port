#ifndef ENEMY_SLEEP_BOSS_HANACHAN_HPP
#define ENEMY_SLEEP_BOSS_HANACHAN_HPP

#include <Enemy/DemoBossHanachan.hpp>

// NOTE: this header is INCOMPLETE. Only the base class (confirmed from the
// destructor's vtable-teardown sequence: TSleepBossHanachan -> ...
// -> TDemoBossHanachan -> TSpineEnemy::~TSpineEnemy()) and the members
// backing the two functions actually implemented in SleepBossHanachan.cpp
// are declared. The remaining ~10 functions (init, startFall, the two
// TNerveSBH_* nerve classes, TSleepBossHanachanManager, etc.) were not
// reached in this pass; see SleepBossHanachan.cpp for what is written.
class TSleepBossHanachan : public TDemoBossHanachan {
public:
	virtual void calcRootMatrix();
	virtual const char** getBasNameTable() const;
};

#endif // ENEMY_SLEEP_BOSS_HANACHAN_HPP
