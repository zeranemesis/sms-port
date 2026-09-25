#ifndef ENEMY_BOMBHEI_HPP
#define ENEMY_BOMBHEI_HPP

#include <Enemy/SmallEnemy.hpp>

// ============= manager =============

class TBombHei;

class TBombHeiManager : public TSmallEnemyManager {
public:
	TBombHeiManager(const char* name = "ボムヘイマネージャー");
	virtual ~TBombHeiManager();

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
	virtual void clipEnemies(JDrama::TGraphics*);
};

// ============= instance =============

class TBombHei : public TSmallEnemy {
public:
	TBombHei(const char* name = "ボムヘイ");
	virtual ~TBombHei();

	virtual const char** getBasNameTable() const;
	virtual void forceKill();
	virtual bool isCollidMove(THitActor*);
	virtual void moveObject();
	virtual void walkBehavior(int, float);
	virtual f32 getGravityY() const;
	virtual void reset();
	virtual void behaveToRelease();
	virtual void behaveToTaken(THitActor*);
	virtual void attackToMario();
	virtual void calcRootMatrix();
	virtual void setDeadAnm();
	virtual void setFreezeAnm();
	virtual void setWalkAnm();
	virtual void genEventCoin();
	virtual void kill();
	virtual bool isHitValid(u32);
	virtual void changeOut();
	virtual void behaveToWater(THitActor*);
	virtual void setMActorAndKeeper();
	virtual void init(TLiveManager*);
	virtual void setAfterDeadEffect();
	virtual bool doKeepDistance();

	// fabricated
	void isDamageToCannon();

	static bool mSerialBomb;

public:
	// TODO: fields between 0x194 and 0x19C, and past 0x19D, are unknown
	/* 0x194 */ char unk194[0x19C - 0x194];
	/* 0x19C */ u8 unk19C;
};

DECLARE_NERVE(TNerveBombHeiExplosion, TLiveActor);
DECLARE_NERVE(TNerveBombHeiThrown, TLiveActor);
DECLARE_NERVE(TNerveBombHeiPickUp, TLiveActor);
DECLARE_NERVE(TNerveBombHeiWaitExplosion, TLiveActor);
DECLARE_NERVE(TNerveBombHeiWalkExplosion, TLiveActor);
DECLARE_NERVE(TNerveBombHeiAttack, TLiveActor);
DECLARE_NERVE(TNerveBombHeiGenerate, TLiveActor);

#endif
