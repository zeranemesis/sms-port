#ifndef ENEMY_POPO_HPP
#define ENEMY_POPO_HPP

#include <Enemy/WalkerEnemy.hpp>

// ============= params =============

class TPopoSaveLoadParams : public TWalkerEnemyParams {
public:
	TPopoSaveLoadParams(const char* path);

	f32 getMoveDist() const { return mSLMoveDist.get(); }
	f32 getMoveGravity() const { return mSLMoveGravity.get(); }
	f32 getMoveJumpSp() const { return mSLMoveJumpSp.get(); }
	f32 getAttackDist() const { return mSLAttackDist.get(); }
	f32 getAttackGravity() const { return mSLAttackGravity.get(); }
	f32 getAttackJumpSp() const { return mSLAttackJumpSp.get(); }
	f32 getReleaseSpeed() const { return mSLReleaseSpeed.get(); }
	f32 getFlyGravity() const { return mSLFlyGravity.get(); }
	s32 getFlyLimitTime() const { return mSLFlyLimitTime.get(); }
	s32 getExplosionEmitTime() const { return mSLExplosionEmitTime.get(); }
	f32 getWaterScaleMax() const { return mSLWaterScaleMax.get(); }
	f32 getThrownGravity() const { return mSLThrownGravity.get(); }
	f32 getPumpRate() const { return mSLPumpRate.get(); }
	s32 getLevelLimit() const { return mSLLevelLimit.get(); }
	f32 getScaleRate() const { return mSLScaleRate.get(); }

	/* 0x32C */ TParamRT<f32> mSLMoveDist;
	/* 0x340 */ TParamRT<f32> mSLMoveGravity;
	/* 0x354 */ TParamRT<f32> mSLMoveJumpSp;
	/* 0x368 */ TParamRT<f32> mSLAttackDist;
	/* 0x37C */ TParamRT<f32> mSLAttackGravity;
	/* 0x390 */ TParamRT<f32> mSLAttackJumpSp;
	/* 0x3A4 */ TParamRT<f32> mSLReleaseSpeed;
	/* 0x3B8 */ TParamRT<f32> mSLFlyGravity;
	/* 0x3CC */ TParamRT<s32> mSLFlyLimitTime;
	/* 0x3E0 */ TParamRT<s32> mSLExplosionEmitTime;
	/* 0x3F4 */ TParamRT<f32> mSLWaterScaleMax;
	/* 0x408 */ TParamRT<f32> mSLThrownGravity;
	/* 0x41C */ TParamRT<f32> mSLPumpRate;
	/* 0x430 */ TParamRT<s32> mSLLevelLimit;
	/* 0x444 */ TParamRT<f32> mSLScaleRate;
};

// ============= manager =============

class TPopo;

class TPopoManager : public TSmallEnemyManager {
public:
	TPopoManager(const char* name = "ポポマネージャー");
	virtual ~TPopoManager();

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void createModelData();
	virtual void initSetEnemies();
	virtual TSpineEnemy* createEnemyInstance();
};

// ============= collision =============

class TPopoCollision : public THitActor {
public:
	TPopoCollision(const char* name = "ポポコリジョン");
	virtual ~TPopoCollision();

	virtual BOOL receiveMessage(THitActor* sender, u32 message);

	// TODO: 2 UNUSED functions found in mario.MAP that were not reconstructed
	// yet: kill__14TPopoCollisionFv (size 0x10), checkHit__14TPopoCollisionFv
	// (size 0xa4). Likely inlined into receiveMessage.
};

// ============= instance =============

class TPopo : public TWalkerEnemy {
public:
	TPopo(const char* name = "ポポ");
	virtual ~TPopo();

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual void kill();
	virtual f32 getGravityY() const;
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void attackToMario();
	virtual void forceKill();
	virtual void setMActorAndKeeper();
	virtual bool isHitValid(u32);
	virtual bool isCollidMove(THitActor*);
	virtual bool isFindMario(float);
	virtual void behaveToFindMario();
	virtual void walkBehavior(int, float);

	// fabricated
	void thrownByChorobei();
	void possessedIn();
	void explosion();
	void flyBehavior();
	void checkTrigger();

	TPopoSaveLoadParams* getSaveParam2() const
	{
		return (TPopoSaveLoadParams*)getSaveParam();
	}

	static bool mRollSw;
	static bool mTriggerSw;
	static f32 mTestAng_x;
	static f32 mTestAng_y;
	static f32 mTestAng_z;
	static f32 mNozzleOffsetZ;
	static u8 mCenterJntIndex;
	static u8 mMouthJntIndex;
	static u8 mRLegJntIndex;
	static u8 mLLegJntIndex;
	static u8 mRHandJntIndex;
	static u8 mLHandJntIndex;
	static f32 mTestBodyScale;
	static bool mBrkFlag;
	static f32 mColOffsetY;
	static f32 mColMinVal;
	static bool mExplosionSw;
	static bool mLevelShootSw;

public:
	/* 0x194 */ s32 unk194;
	/* 0x198 */ f32 unk198;
	/* 0x19C */ s32 unk19C;
	/* 0x1A0 */ f32 unk1A0;
	/* 0x1A4 */ u8 unk1A4[0x10];
	/* 0x1B4 */ u8 unk1B4[4];
	/* 0x1B8 */ f32 unk1B8[5];
	/* 0x1CC */ u8 unk1CC;
	/* 0x1CD */ u8 unk1CD[0x6F];
	/* 0x23C */ TPopoCollision* unk23C;
};

// ============= nerves =============

DECLARE_NERVE(TNervePopoThrown, TLiveActor);
DECLARE_NERVE(TNervePopoWait, TLiveActor);
DECLARE_NERVE(TNervePopoExplosion, TLiveActor);
DECLARE_NERVE(TNervePopoFly, TLiveActor);
DECLARE_NERVE(TNervePopoAttack, TLiveActor);
DECLARE_NERVE(TNervePopoPossessedNozzle, TLiveActor);

extern TPopo* gpCurPopo;

#endif
