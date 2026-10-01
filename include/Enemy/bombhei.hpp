#ifndef ENEMY_BOMBHEI_HPP
#define ENEMY_BOMBHEI_HPP

#include <Enemy/WalkerEnemy.hpp>

class TBombHei;

// ============= save/load params =============

// The parameter block is derived from TWalkerEnemyParams -- every offset used
// by the binary (0x32C..0x3A3) follows mSLZigzagCycle/mSLZigzagAngle/
// mSLMarchSpeedLow/mSLMarchSpeedHigh/unk324, and the object is 0x3A4 bytes.
class TBombHeiSaveLoadParams : public TWalkerEnemyParams {
public:
	TBombHeiSaveLoadParams(const char* name);

	s32 getSLBombTime() const { return mSLBombTime.get(); }
	f32 getSLBombRange() const { return mSLBombRange.get(); }
	f32 getSLThrownVY() const { return mSLThrownVY.get(); }
	f32 getSLThrownRateXZ() const { return mSLThrownRateXZ.get(); }
	f32 getSLThrownGravityY() const { return mSLThrownGravityY.get(); }
	f32 getSLShootVelocity() const { return mSLShootVelocity.get(); }

	/* 0x32C */ TParamRT<s32> mSLBombTime;
	/* 0x340 */ TParamRT<f32> mSLBombRange;
	/* 0x354 */ TParamRT<f32> mSLThrownVY;
	/* 0x368 */ TParamRT<f32> mSLThrownRateXZ;
	/* 0x37C */ TParamRT<f32> mSLThrownGravityY;
	/* 0x390 */ TParamRT<f32> mSLShootVelocity;
};

// ============= manager =============

class TBombHeiManager : public TSmallEnemyManager {
public:
	TBombHeiManager(const char* name = "ボムヘイマネージャー");
	virtual ~TBombHeiManager();

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
	virtual void clipEnemies(JDrama::TGraphics*);

	// UNUSED in the map (0x24 bytes); every call site inlines it.
	bool canMakeDeadCoin();

public:
	/* 0x60 */ int mDeadCoinCount;
};

// ============= instance =============

class TBombHei : public TWalkerEnemy {
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

	// UNUSED in the map (0xC4 / 0x8C bytes) and never emitted, so the bodies
	// below cannot be verified against the binary.
	void bombIn();
	bool isExplosion();

	// fabricated
	TBombHeiSaveLoadParams* getBombParam() const
	{
		return (TBombHeiSaveLoadParams*)getSaveParam();
	}

	// fabricated
	bool isDamageToCannon();

	static bool mSerialBomb;

public:
	// 0x19D..0x19F is padding between mIsBomb and mBombRadius
	/* 0x194 */ TBombHeiSaveLoadParams* mBombParam;
	/* 0x198 */ int mBombTimer;
	/* 0x19C */ u8 mIsBomb;
	/* 0x1A0 */ f32 mBombRadius;
	/* 0x1A4 */ bool mMadeDeadCoin;
};

DECLARE_NERVE(TNerveBombHeiExplosion, TLiveActor);
DECLARE_NERVE(TNerveBombHeiThrown, TLiveActor);
DECLARE_NERVE(TNerveBombHeiPickUp, TLiveActor);
DECLARE_NERVE(TNerveBombHeiWaitExplosion, TLiveActor);
DECLARE_NERVE(TNerveBombHeiWalkExplosion, TLiveActor);
DECLARE_NERVE(TNerveBombHeiAttack, TLiveActor);
DECLARE_NERVE(TNerveBombHeiGenerate, TLiveActor);

#endif
