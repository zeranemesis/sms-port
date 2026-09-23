#ifndef ENEMY_SEAL_HPP
#define ENEMY_SEAL_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/Nerve.hpp>

// The orange goop "seals" (gene_orange) that block paths until they are
// washed away with water.
class TSeal : public TSpineEnemy {
public:
	TSeal(const char* name);

	virtual void perform(u32, JDrama::TGraphics*);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();

public:
	/* 0x150 */ int unk150; // times hit by water this frame
};

class TSealManager : public TEnemyManager {
public:
	TSealManager(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();

	void initJParticle();
};

DECLARE_NERVE(TNerveSealSleep, TLiveActor);
DECLARE_NERVE(TNerveSealWait, TLiveActor);
DECLARE_NERVE(TNerveSealDie, TLiveActor);

#endif
