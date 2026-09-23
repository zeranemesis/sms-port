#ifndef ENEMY_YUMBO_HPP
#define ENEMY_YUMBO_HPP

#include <Enemy/SmallEnemy.hpp>
#include <Strategic/HitActor.hpp>

class MActor;
class J3DMaterialTable;
class TYumbo;

// Yumbo (the dancing Pianta-village flower head, sambohead in the files).
// It turns into a flower and shoots TYumboSeeds at Mario.
// Layout reconstructed from build/GMSP01/asm/Enemy/yunbo.s.

class TYumboParams : public TSmallEnemyParams {
public:
	TYumboParams(const char* path);

	/* 0x2D4 */ TParamRT<s32> mRecoverTimer;
	/* 0x2E8 */ TParamRT<f32> mShootSpeed;
	/* 0x2FC */ TParamRT<f32> mShootAngleX;
	/* 0x310 */ TParamRT<s32> mSeedLife;
	/* 0x324 */ TParamRT<f32> mSeedAirFric;
	/* 0x338 */ TParamRT<f32> mSeedGravityY;
};

class TYumboManager : public TSmallEnemyManager {
public:
	TYumboManager(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();

	void loadMaterialTable(J3DMaterialTable**, const char*);

public:
	/* 0x60 */ J3DMaterialTable* mFlowerMaterialTable;
};

class TYumboSeed : public THitActor {
public:
	TYumboSeed(MActor* actor, const TYumbo& owner);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void init();

	void startToMove(const JGeometry::TVec3<f32>& pos,
	                 const JGeometry::TVec3<f32>& velocity, int life);
	void checkHitActors();

	enum {
		SEED_FLAG_UNUSED = 0x1,
		SEED_FLAG_UNK4   = 0x4,
	};

public:
	/* 0x68 */ const TYumbo* mOwner;
	/* 0x6C */ MActor* mActor;
	/* 0x70 */ u32 mSeedFlags;
	/* 0x74 */ int mLife;
	/* 0x78 */ JGeometry::TVec3<f32> mVelocity;
};

class TYumbo : public TSmallEnemy {
public:
	TYumbo(const char* name);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void moveObject();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void setDeadAnm();
	virtual void attackToMario();
	virtual bool doKeepDistance();

	TYumboSeed* getUnusedSeed();
	bool isDead() const;
	bool isFreeze() const;
	bool isWaterproof() const;
	void changeToYumbo();
	void changeToFlower();
	void lookatMario();
	void shotSeeds();
	bool isChangedBlock() const;
	bool isAllSeedBroken() const;
	bool isWantToAppear() const;
	bool isFindOutMario() const;
	void updateEffect();
	void updateCollision();
	void behaveHitAttack();
	void initCollision();
	void setMaterialToMActor(MActor*, J3DMaterialTable*);
	void initMActorAndKeeper();

	TYumboParams* getSaveLoadParam() const
	{
		return (TYumboParams*)getSaveParam();
	}

public:
	/* 0x194 */ TYumboSeed* mSeeds[16];
	/* 0x1D4 */ int mCenterJointIndex;
	/* 0x1D8 */ bool mHideEffectDone;
};

DECLARE_NERVE(TNerveYumboDancing, TLiveActor);
DECLARE_NERVE(TNerveYumboHiding, TLiveActor);
DECLARE_NERVE(TNerveYumboAppearing, TLiveActor);
DECLARE_NERVE(TNerveYumboAttack, TLiveActor);
DECLARE_NERVE(TNerveYumboFreeze, TLiveActor);

#endif
