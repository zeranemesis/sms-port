#ifndef ENEMY_EFFECT_ENEMY_HPP
#define ENEMY_EFFECT_ENEMY_HPP

#include <Enemy/WalkerEnemy.hpp>
#include <Enemy/SmallEnemy.hpp>

// NOTE: this header was reconstructed from disassembly. Base classes were
// confirmed from the ctor/dtor chains: TEffectEnemy::TEffectEnemy calls
// TWalkerEnemy::TWalkerEnemy(const char*), and ~TEffectEnemyManager tears
// down through __vt__19TEffectEnemyManager before calling
// TEnemyManager::~TEnemyManager(). All of the virtuals below override slots
// that already exist in TSmallEnemy/TLiveActor, so they add no new vtable
// entries; __vt__12TEffectEnemy is 0x1B8 bytes in both our object and
// marioEU.MAP.
class TEffectEnemy : public TWalkerEnemy {
public:
	TEffectEnemy(const char* name);
	virtual void init(TLiveManager*);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void kill();
	virtual void forceKill();
	virtual void setMActorAndKeeper();
	virtual void reset();
	virtual void sendAttackMsgToMario();
	virtual void setDeadAnm();
	virtual void behaveToWater(THitActor*);

	// Dead code: no call site and no vtable slot anywhere in marioEU.dol, so
	// the linker stripped it. marioEU.MAP still records it as UNUSED with a
	// size of 0xF4, which is the only verification available and which the
	// definition in the .cpp currently reproduces exactly. Not virtual.
	void emitEffect();

public:
	/* 0x194 */ u32 unk194;
};

class TEffectEnemyManager : public TSmallEnemyManager {
public:
	// marioEU.MAP has no __ct__19TEffectEnemyManager, and MarNameRefGen_Enemy
	// constructs it by calling __ct__18TSmallEnemyManagerFPCc and patching
	// __vt__19TEffectEnemyManager (0x58 bytes, i.e. no new slot) afterwards --
	// so the constructor was inline. Same shape as TKageMarioModokiManager.
	TEffectEnemyManager(const char* name = "エフェクト敵マネージャー")
	    : TSmallEnemyManager(name)
	{
	}

	virtual void initSetEnemies();
	virtual TSpineEnemy* createEnemyInstance();
	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
};

#endif // ENEMY_EFFECT_ENEMY_HPP
