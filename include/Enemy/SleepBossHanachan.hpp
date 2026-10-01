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
	// No __ct__18TSleepBossHanachanF* in mario.MAP either, so the original
	// constructor was inline; MarNameRefGen_BossEnemy.cpp expands it and the
	// whole chain is visible there: TSpineEnemy(name), the TDemoBossHanachan
	// vtable, then this class's, then the two initialisers below. NB the
	// position is +0x150 and the constant really is 0.0f (the ROM's @3238 in
	// .sdata2 is four zero bytes), and it has to be an initialiser rather than
	// a body statement -- that is what keeps TVec3::set<f32> out of line,
	// which is the whole reason this TU carries a 16-byte copy of it.
	TSleepBossHanachan(const char* name = "?")
	    : TDemoBossHanachan(name)
	    , unk150(0.0f, 0.0f, 0.0f)
	    , unk15C(nullptr)
	{
	}

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
// inline as well.
class TSleepBossHanachanManager : public TDemoBossHanachanManager {
public:
	TSleepBossHanachanManager(const char* name = "?")
	    : TDemoBossHanachanManager(name)
	{
	}

	virtual void createModelData();
};

#endif // ENEMY_SLEEP_BOSS_HANACHAN_HPP
