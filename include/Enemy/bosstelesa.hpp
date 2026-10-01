#ifndef ENEMY_BOSSTELESA_HPP
#define ENEMY_BOSSTELESA_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Enemy/WalkerEnemy.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjSirena.hpp>
#include <System/ParamInst.hpp>

class TSharedParts;
class TTelesaManager;

class TBossTelesaSaveLoadParams : public TSpineEnemyParams {
public:
	TBossTelesaSaveLoadParams(const char*);

	/* 0x0A8 */ TParamRT<s32> mSLDamageRadius;
	/* 0x0BC */ TParamRT<s32> mSLDamageHeight;
	/* 0x0D0 */ TParamRT<s32> mSLAttackRadius;
	/* 0x0E4 */ TParamRT<s32> mSLAttackHeight;
	/* 0x0F8 */ TParamRT<s32> mSLGenAttackerTime;
	/* 0x10C */ TParamRT<s32> mSLGenBubbleTime;
	/* 0x120 */ TParamRT<f32> mSLHitAngle;
	/* 0x134 */ TParamRT<s32> mSLNumGenBubble;
	/* 0x148 */ TParamRT<f32> mSL1stBubbleSp;
	/* 0x15C */ TParamRT<f32> mSLHideAreaRadius;
	/* 0x170 */ TParamRT<s32> mSLSlotItemNum;
	/* 0x184 */ TParamRT<s32> mSLSlotFruitNum;
	/* 0x198 */ TParamRT<f32> mSLSlotFirstHitCollectRate;
	/* 0x1AC */ TParamRT<f32> mSLSlotHitCollectRate;
	/* 0x1C0 */ TParamRT<f32> mSLTransYOffset;
	/* 0x1D4 */ TParamRT<s32> mSLStopSlotTime0;
	/* 0x1E8 */ TParamRT<s32> mSLStopSlotTime1;
	/* 0x1FC */ TParamRT<s32> mSLStopSlotTime2;
	/* 0x210 */ TParamRT<s32> mSLSpicyTime;
	/* 0x220 */ TParamRT<s32> mSLPrepareSlotTime;
};

class TTelesaSlot;

class TBossTelesa : public TSpineEnemy {
public:
	TBossTelesa(const char* name = "ボステレサ");

	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual MtxPtr getTakingMtx();
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void moveObject();
	virtual void kill();
	virtual const char** getBasNameTable() const;
	virtual void reset();

	void checkHitObject(THitActor*);
	void setSpicy(TLiveActor*);
	void damageRecover();
	bool rouletteFall();
	bool slotFall();
	void flashItem(int idx);
	void genAttacker();
	void rouletteStart();
	void generateSlotItem();
	void forceAllItemKill();
	void forceHide();
	void setBckAnm(int idx);

	static f32 mEnemyGenRate;
	static f32 mItemGenRate;
	static u8 mNormalAlpha;
	static f32 mBaseHoseiPosY;
	static f32 mRouletteUpRate;
	static s32 mTelesaGenerateInterval;
	static f32 mCameraMoveLimit;
	static f32 mCameraMoveSp;

public:
	/* 0x13C is TEnemy::mHitPoints (inherited) - do not redeclare it here. */
	/* 0x150 */ u8 unk150;
	/* 0x154 */ void* unk154;
	/* 0x158 */ void* unk158;
	/* 0x15C */ TBossTelesaSaveLoadParams* mParams;
	/* 0x160 */ s32 mCurBckIdx;
	/* 0x164 */ s32 mPrevBckIdx;
	/* 0x168 */ f32 mBlendRatio;
	/* 0x16C */ THitActor* mHitActors[3];
	/* 0x178 */ TMapObjBase* mStageSlotObjects[3];
	/* 0x184 */ TTelesaSlot* mSlot;
	/* 0x188 */ TSharedParts* unk188;
	/* 0x18C */ u8 unk18C;
	/* 0x18D */ u8 mPad18D[0x1A8 - 0x18D];
	/* 0x1A8 */ s32 unk1A8;
	/* 0x1AC */ TMapObjBase* mItems[50];
	/* 0x274 */ s32 mItemNum;
	/* 0x278 */ JGeometry::TMatrix34<JGeometry::SMatrix34C<f32> > mTakingMtx;
	// 25 wide, not 20: loadAfter runs five unrolled `newAndRegisterObj`
	// loops (6+6+2+6+5) that all store through `this + 0x2A8 + 4*n`
	// with one running n, so the last five land at 0x2F8..0x30B.  See
	// the five loops in loadAfter.
	/* 0x2A8 */ TMapObjBase* mSlotFruits[25];
	/* 0x30C */ TMapObjBase* unk30C[5];
	/* 0x320 */ TMapObjBase* mCoins[10];
	/* 0x348 */ GXColor mTevColorA;
	/* 0x34C */ GXColor mTevColorB;
	/* 0x350 */ u8 unk350;
	/* 0x354 */ TTelesaManager* unk354;
	/* 0x358 */ u16 unk358;
	/* 0x35A */ u8 unk35A;
	/* 0x35B */ u8 unk35B;
	/* 0x35C */ s32 unk35C;
	/* 0x360 */ f32 unk360;
	/* 0x364 */ f32 unk364;
	/* 0x368 */ s32 unk368;
	/* 0x36C */ s32 unk36C;
	/* 0x370 */ u8 unk370;
	/* 0x371 */ u8 mPad371[0x374 - 0x371];
	/* 0x374 */ JGeometry::TVec3<f32> unk374;
	/* 0x380 */ s32 unk380;
	/* 0x384 */ u8 unk384;
	/* 0x385 */ u8 mPad385[0x388 - 0x385];
	/* 0x388 */ s32 unk388;
};

class TBossTelesa;

class TTelesaSlot : public TSlotDrum {
public:
	TTelesaSlot(const char* name);

	virtual void calcRootMatrix();
	virtual void moveObject();
	virtual void initMapObj();
	virtual u32 touchWater(THitActor*);
	virtual void initNeonMatColor() { }

	int getResultFromAng(f32);
	int getForcastResult(int);
	int getDrumResult(int);
	int getSlotResult();
	bool isRollDrum();
	void forceStopSlot(int);
	void moveStart();
	void randomReset();

public:
	/* 0x198 */ bool mRolling[3];
	/* 0x19B */ bool unk19B;
	/* 0x19C */ bool unk19C;
	/* 0x1A0 */ TBossTelesa* mOwner;
	/* 0x1A4 */ s32 unk1A4;
	/* 0x1A8 */ bool mStopped[3];
	/* 0x1AC */ JGeometry::TVec3<f32> unk1AC[4];
	/* 0x1DC */ TMapCollisionMove* mMapCollision;
	/* 0x1E0 */ u8 unk1E0;
	// TNerveBossTelesaDie zeroes these three one at a time (stfs at
	// 0x1e4 / 0x1e8 / 0x1ec) alongside one f32 per mStageSlotObjects[i].
	// The names are unknown; only the offsets and the f32 width are known.
	// unk1E0 is a lone u8, so the padding to the next f32 is explicit.
	u8 mPad1E1[0x1E4 - 0x1E1];
	/* 0x1E4 */ f32 mRouletteRollSpeeds[3];
};

DECLARE_NERVE(TNerveBossTelesaSpit, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaDie, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaHide, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaHideWait, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaAppear, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaSlotStart, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaSpitSlotItem, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaPrepareSlot, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaFreeze, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaFallDemo, TLiveActor);

class TBossTelesaTongue : public THitActor {
public:
	virtual BOOL receiveMessage(THitActor*, u32);

public:
	/* 0x68 */ TBossTelesa* mOwner;
};

class TBossTelesaBody : public THitActor {
public:
	virtual BOOL receiveMessage(THitActor*, u32);

public:
	/* 0x68 */ TBossTelesa* mOwner;
};

class TBossTelesaKillSmallEnemy : public THitActor {
public:
	void checkHit();

public:
	/* 0x68 */ TBossTelesa* mOwner;
	/* 0x6C */ bool mHit;
};

class TBossTelesaManager : public TEnemyManager {
public:
	TBossTelesaManager(const char* name = "ボステレサマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
	virtual void clipEnemies(JDrama::TGraphics*) { }
};

class TBubbleSaveLoadParams : public TWalkerEnemyParams {
public:
	TBubbleSaveLoadParams(const char* path)
	    : TWalkerEnemyParams(path)
	    , PARAM_INIT(mSLLiveTime, 200)
	    , PARAM_INIT(mSLNumDivision, 5)
	    , PARAM_INIT(mSLMaxScale, 1.5f)
	    , PARAM_INIT(mSLAddPosBase, 50.0f)
	    , PARAM_INIT(mSLRateExpand, 1.001f)
	    , PARAM_INIT(mSLDeadHeight, 300.0f)
	{
		TParams::load(mPrmPath);
	}

	/* 0x32C */ TParamRT<s32> mSLLiveTime;
	/* 0x340 */ TParamRT<s32> mSLNumDivision;
	/* 0x354 */ TParamRT<f32> mSLMaxScale;
	/* 0x368 */ TParamRT<f32> mSLAddPosBase;
	/* 0x37C */ TParamRT<f32> mSLRateExpand;
	/* 0x390 */ TParamRT<f32> mSLDeadHeight;
};

class TBubble : public TWalkerEnemy {
public:
	TBubble(const char* name = "バブル")
	    : TWalkerEnemy(name)
	    , unk198(nullptr)
	    , unk1CC(0.0f)
	    , unk1D0(0)
	    , unk1D1(0)
	{
	}


	virtual void init(TLiveManager*);
	virtual MtxPtr getTakingMtx();
	virtual void calcRootMatrix();
	virtual void kill();
	virtual f32 getGravityY() const;
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void setDeadAnm();
	virtual void attackToMario();
	virtual void setAfterDeadEffect() { }

	void appendEnemy();
	void split();

public:
	/* 0x194 */ TBubbleSaveLoadParams* mParams;
	/* 0x198 */ TSpineEnemy* unk198;
	/* 0x19C */ JGeometry::TMatrix34<JGeometry::SMatrix34C<f32> > unk19C;
	/* 0x1CC */ f32 unk1CC;
	/* 0x1D0 */ u8 unk1D0;
	/* 0x1D1 */ u8 unk1D1;
	/* 0x1D2 */ u8 unk1D2;
	/* 0x1D3 */ u8 mPad1D3;
};

DECLARE_NERVE(TNerveBubbleLive, TLiveActor);
DECLARE_NERVE(TNerveBubbleSplit, TLiveActor);

class TBubbleManager : public TSmallEnemyManager {
public:
	TBubbleManager(const char* name = "バブルマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

#endif
