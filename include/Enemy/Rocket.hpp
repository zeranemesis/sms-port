#ifndef ROCKET_H
#define ROCKET_H

#include <Enemy/EnemyManager.hpp>
#include <Enemy/Enemy.hpp>
#include <Enemy/SmallEnemy.hpp>
#include <Strategic/Strategy.hpp>
#include <Map/MapData.hpp>

class TWaterEmitInfo;

class TRocketSaveLoadParams : public TSmallEnemyParams {
public:
	// UNUSED, fully inlined into TRocketManager::load
	TRocketSaveLoadParams(const char* path);

	/* 0x2D4 */ TParamRT<f32> mSLReleaseSpeed;
	/* 0x2E8 */ TParamRT<f32> mSLFlyGravity;
	/* 0x2FC */ TParamRT<s32> mSLFlyLimitTime;
};

class TRocket : public TSmallEnemy {
public:
	TRocket(const char* name = "ロケット");
	virtual ~TRocket() { }

	virtual void load(JSUMemoryInputStream&);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual f32 getGravityY() const;
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void setDeadAnm();
	virtual void attackToMario();
	virtual void setMActorAndKeeper();
	virtual bool isCollidMove(THitActor*);

	bool isAttack();
	void flyBehavior();
	bool checkTrigger();
	void releaseNozzle();
	void possessedNozzle();

	static f32 mTestAng_x;
	static f32 mTestAng_y;
	static f32 mTestAng_z;
	static f32 mNozzleOffsetZ;
	static f32 mColOffsetY;

public:
	/* 0x194 */ JGeometry::TVec3<f32> mInitPos;
	/* 0x1A0 */ bool mIsPossessed; // held by Mario's nozzle
	/* 0x1A1 */ bool mHasInitPos;
	/* 0x1A4 */ TRocketSaveLoadParams* mParams;
};

class TRocketManager : public TSmallEnemyManager {
public:
	TRocketManager(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void perform(u32, JDrama::TGraphics*);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
	virtual void clipEnemies(JDrama::TGraphics*) { }
	virtual void initSetEnemies();

public:
	/* 0x60 */ bool mCanPossess; // no rocket is currently in the nozzle
	/* 0x64 */ u32 unk64;
	/* 0x68 */ TWaterEmitInfo* mExpWaterEmitInfo;
};

DECLARE_NERVE(TNerveRocketWait, TLiveActor);
DECLARE_NERVE(TNerveRocketFly, TLiveActor);
DECLARE_NERVE(TNerveRocketPossessedNozzle, TLiveActor);

#endif
