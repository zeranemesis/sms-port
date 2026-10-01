#ifndef ENEMY_POPO_HPP
#define ENEMY_POPO_HPP

#include <Enemy/WalkerEnemy.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JGadget/std-list.hpp>

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
	// NOTE: the binary stores a float here (lfs @1.2 / stfs), not an int.
	f32 getLevelLimit() const { return mSLLevelLimit.get(); }
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
	/* 0x430 */ TParamRT<f32> mSLLevelLimit;
	/* 0x444 */ TParamRT<f32> mSLScaleRate;
};

// ============= manager =============

class TPopo;

class TWaterEmitInfo;

class TPopoManager : public TSmallEnemyManager {
public:
	TPopoManager(const char* name = "ポポマネージャー");
	virtual ~TPopoManager() { }

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void createModelData();
	virtual void initSetEnemies();
	virtual TSpineEnemy* createEnemyInstance();

	// fabricated
	TPopoSaveLoadParams* getSaveParam2() const
	{
		return (TPopoSaveLoadParams*)unk38;
	}
	TWaterEmitInfo* getWaterEmitInfo1() const { return unk64; }
	TWaterEmitInfo* getWaterEmitInfo2() const { return unk68; }

public:
	/* 0x60 */ u8 unk60;
	/* 0x64 */ TWaterEmitInfo* unk64;
	/* 0x68 */ TWaterEmitInfo* unk68;
};

// ============= collision =============

// fabricated: the "敵グループ" name-ref that TPopo::init registers the
// collision actor with resolves to a TNameRef subclass carrying a JGadget
// list of the registered objects at +0x10.
class TEnemyNameRefGroup : public JDrama::TNameRef {
public:
	TEnemyNameRefGroup(const char* name)
	    : TNameRef(name)
	{
	}
	/* 0x10 */ JGadget::TList_pointer_void mObjects;
};

class TPopoCollision : public THitActor {
public:
	TPopoCollision(const char* name = "ポポコリジョン");
	virtual ~TPopoCollision() { }

	virtual BOOL receiveMessage(THitActor* sender, u32 message);

	// TODO: 2 UNUSED functions found in mario.MAP that were not reconstructed
	// yet: kill__14TPopoCollisionFv (size 0x10), checkHit__14TPopoCollisionFv
	// (size 0xa4). Likely inlined into receiveMessage.

	// fabricated
	THitActor* getOwner() const { return mOwner; }
	void setOwner(THitActor* owner) { mOwner = owner; }

	/* 0x68 */ THitActor* mOwner;
};

// ============= instance =============

class TPopo : public TWalkerEnemy {
public:
	TPopo(const char* name = "ポポ");
	virtual ~TPopo() { }

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
	bool checkTrigger();
	TPopoSaveLoadParams* getSaveParam2() const
	{
		return (TPopoSaveLoadParams*)getSaveParam();
	}
	f32 getUnk1B8() const { return unk1B8; }

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
	/* 0x194 */ TPopoSaveLoadParams* unk194;
	/* 0x198 */ f32 unk198;
	/* 0x19C */ s32 unk19C;
	/* 0x1A0 */ f32 unk1A0;
	/* 0x1A4 */ bool unk1A4;
	/* 0x1A8 */ JGeometry::TVec3<f32> unk1A8;
	/* 0x1B4 */ bool unk1B4;
	/* 0x1B8 */ f32 unk1B8;
	/* 0x1BC */ u8 unk1BC;
	/* 0x1C0 */ JGeometry::TVec3<f32> unk1C0;
	/* 0x1CC */ u8 unk1CC;
	/* 0x1CD */ bool unk1CD;
	// The 0x1D0 area holds two full 3x4 matrices: the first is the nozzle
	// joint transform saved by PopoPossessedCallback, the second is the
	// centre joint transform handed to the water-jet emitter. The three
	// floats after them are the lengths of that joint's basis vectors,
	// stored rotated by one.
	/* 0x1CE */ u8 unk1CE[2];
	/* 0x1D0 */ Mtx unk1D0;
	/* 0x200 */ Mtx unk200;
	/* 0x230 */ JGeometry::TVec3<f32> unk230;
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
