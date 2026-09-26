#ifndef ENEMY_SLEEP_BOSS_HANACHAN_HPP
#define ENEMY_SLEEP_BOSS_HANACHAN_HPP

#include <Enemy/DemoBossHanachan.hpp>

class TMirrorActor;

// Sleeping boss "Hanachan". The class itself carries only two members on top
// of TDemoBossHanachan (which adds none of its own on top of TSpineEnemy, whose
// last member ends at 0x14C, so the layout below is what the ROM's destructor
// and startFall()/init() read):
//   0x150 world position the shine spawns at when Hanachan falls,
//   0x15C the reflection copy drawn in the mirror.
class TSleepBossHanachan : public TDemoBossHanachan {
public:
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual const char** getBasNameTable() const;

	void startFall(f32, f32, f32);

public:
	/* 0x150 */ JGeometry::TVec3<f32> unk150;
	/* 0x15C */ TMirrorActor* unk15C;
};

// Only createModelData() is defined out of line (it is the key function, so
// the vtable is emitted from SleepBossHanachan.cpp). There is no __ct__ symbol
// anywhere in mario.MAP for this class, so the original constructor was
// inline -- but nothing in the tree can construct it yet, see the TODO in
// MarNameRefGen_BossEnemy.cpp: TDemoBossHanachanManager's constructor takes no
// name argument, which is a cross-TU header change left for a human.
class TSleepBossHanachanManager : public TDemoBossHanachanManager {
public:
	virtual void createModelData();
};

#endif // ENEMY_SLEEP_BOSS_HANACHAN_HPP
