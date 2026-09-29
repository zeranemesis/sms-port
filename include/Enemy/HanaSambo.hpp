#ifndef ENEMY_HANA_SAMBO_HPP
#define ENEMY_HANA_SAMBO_HPP

#include <Enemy/WalkerEnemy.hpp>
#include <Enemy/SmallEnemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <JSystem/JDrama/JDRViewObj.hpp>
#include <M3DUtil/MActor.hpp>
#include <Strategic/ObjModel.hpp>
#include <JSystem/JGeometry.hpp>

// Sambo (Pianta-village flower-head enemy), split in the retail binary as
// hanasambo.cpp. Layout from build/GMSP01/asm/Enemy/hanasambo.s.
// TODO: only the members touched by constructors are named; the rest of
// the classes is still to be reconstructed.

class TMBindShadowBody;
class THanaSambo;
class TSamboFlowerCoinUnit {
public:
	void checkGenCoin();
};

class TSamboLeaf : public JDrama::TViewObj {
public:
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
};
class TMapObjBase;
class J3DMaterialTable;
class MActor;

class TSamboFlowerSaveLoadParams : public TSpineEnemyParams {
public:
	TSamboFlowerSaveLoadParams(const char* path);

	/* 0x0A8 */ TParamRT<f32> mSLLeafVelocityXZ;
	/* 0x0BC */ TParamRT<f32> mSLLeafVelocityY;
	/* 0x0D0 */ TParamRT<f32> mSLLeafGravity;
	/* 0x0E4 */ TParamRT<f32> mSLBudDist;
	/* 0x0F8 */ TParamRT<s32> mSLBloomTimer;
	/* 0x10C */ TParamRT<f32> mSLCoinCircleR;
	/* 0x120 */ TParamRT<f32> mSLCoinVelocityXZ;
	/* 0x134 */ TParamRT<f32> mSLCoinVelocityY;
	/* 0x148 */ TParamRT<f32> mSLSeedShootRange;
	/* 0x15C */ TParamRT<s32> mSLSeedShootInterval;
	/* 0x170 */ TParamRT<f32> mSLSeedGravity;
	/* 0x184 */ TParamRT<f32> mSLSeedSpeedXZ;
	/* 0x198 */ TParamRT<f32> mSLSeedSpeedY;
};

// The flower of the Pianta-village sambo plant; the bloom hides a coin unit.
class TSamboFlower : public TSpineEnemy {
public:
	TSamboFlower(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void control() { }
	virtual void moveObject();
	virtual void drawObject(JDrama::TGraphics*);
	virtual void reset();
	virtual void setMActorAndKeeper();

	void setMaterialToMActor(MActor* actor, J3DMaterialTable* table)
	{
		actor->getModel()->getModelData()->setMaterialTable(
		    table, (J3DMaterialCopyFlag)3);
		actor->initDL();
		actor->getModel()->lock();
	}

public:
	/* 0x150 */ u8 unk150;
	/* 0x154 */ s32 unk154;
	/* 0x158 */ s32 unk158;
	/* 0x15C */ s32 unk15C;
	/* 0x160 */ u8 unk160;
	/* 0x164 */ int* unk164; // remaining-coin counter of the owning coin unit
	/* 0x168 */ TMapObjBase* unk168;
	/* 0x16C */ TSamboFlowerSaveLoadParams* unk16C;
};

class TSamboFlowerManager : public TEnemyManager {
public:
	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
	virtual void dropLeaf(JGeometry::TVec3<f32>&, JGeometry::TVec3<f32>&);

public:
	/* 0x54 */ TSamboFlowerCoinUnit** unk54;
	/* 0x58 */ int unk58;
	/* 0x5C */ int unk5C;
	/* 0x60 */ TSamboLeaf** unk60;
	/* 0x64 */ J3DMaterialTable* unk64;
};

// The flower-head hit volume that follows THanaSambo's head joint.
class THanaSamboHead : public THitActor {
public:
	virtual BOOL receiveMessage(THitActor* sender, u32 message);

public:
	/* 0x68 */ THanaSambo* mOwner;
};

#include <System/ParamInst.hpp>

class TSamboHeadSaveLoadParams : public TWalkerEnemyParams {
public:
	TSamboHeadSaveLoadParams(const char* path);

	/* 0x32C */ TParamRT<f32> mSLAppearDist;
	/* 0x340 */ TParamRT<f32> mSLHideDist;
	/* 0x354 */ TParamRT<f32> mSLMoveDist;
	/* 0x368 */ TParamRT<f32> mSLMoveGravity;
	/* 0x37C */ TParamRT<f32> mSLJumpSp;
	/* 0x390 */ TParamRT<s32> mSLJumpPrepareTime;
	/* 0x3A4 */ TParamRT<f32> mSLHitJumpSpXZ;
	/* 0x3B8 */ TParamRT<f32> mSLHitJumpSpY;
	/* 0x3CC */ TParamRT<f32> mSLHitJumpGravity;
	/* 0x3E0 */ TParamRT<f32> mSLHitJumpSpRateXZ;
	/* 0x3F4 */ TParamRT<f32> mSLHitJumpSpRateY;
	/* 0x408 */ TParamRT<f32> mSLJumpAngY;
};

class THanaSamboSaveLoadParams : public TSmallEnemyParams {
public:
	THanaSamboSaveLoadParams(const char* path);

	/* 0x2D4 */ TParamRT<f32> mSLAttackDist;
	/* 0x2E8 */ TParamRT<s32> mSLAttackInterval;
	/* 0x2FC */ TParamRT<f32> mSLAppearDist;
	/* 0x310 */ TParamRT<f32> mSLHideDist;
	/* 0x324 */ TParamRT<s32> mSLAttackingTime;
	/* 0x338 */ TParamRT<f32> mSLHeadAttackRadius;
	/* 0x34C */ TParamRT<f32> mSLHeadAttackHeight;
	/* 0x360 */ TParamRT<f32> mSLHeadDamageRadius;
	/* 0x374 */ TParamRT<f32> mSLHeadDamageHeight;
};

class TSamboHead : public TWalkerEnemy {
public:
	TSamboHead(const char* name);

	virtual void calcRootMatrix();
	virtual void reset();
	virtual const char** getBasNameTable() const;
	virtual void setDeadAnm();
	virtual void setMActorAndKeeper();
	virtual void kill();
	virtual f32 getGravityY() const;
	virtual void load(JSUMemoryInputStream&);
	virtual void setAfterDeadEffect();

public:
	/* 0x194 */ TSamboHeadSaveLoadParams* unk194;
	/* 0x198 */ TSamboFlower* unk198;
	/* 0x19C */ u32 unk19C;
	/* 0x1A0 */ char unk1A0[0x1AC - 0x1A0];
	/* 0x1AC */ f32 unk1AC;
	/* 0x1B0 */ u8 unk1B0;
};

class TSamboHeadManager : public TSmallEnemyManager {
public:
	TSamboHeadManager(const char* name);

	virtual TSpineEnemy* createEnemyInstance();
	virtual void createModelData();
	virtual void load(JSUMemoryInputStream&);
};

class THanaSambo : public TSmallEnemy {
public:
	THanaSambo(const char* name);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual const char** getBasNameTable() const;
	virtual void setWaitAnm();
	virtual void setDeadAnm();
	virtual void setMActorAndKeeper();
	virtual void kill();
	virtual void reset();
	virtual void drawObject(JDrama::TGraphics*);
	virtual void behaveToWater(THitActor*);
	virtual bool isCollidMove(THitActor*);
	virtual BOOL isHitValid(u32 message);
	virtual void load(JSUMemoryInputStream&);
	virtual void moveObject();

	void createPollen();

	static u8 mHeadJntIndex;
	static u8 mPollenJntIndex;

public:
	/* 0x194 */ THanaSamboHead* unk194;
	/* 0x198 */ THanaSamboSaveLoadParams* unk198;
	/* 0x19C */ JGeometry::TVec3<f32> unk19C; // position at last reset
	/* 0x1A8 */ TSamboFlower* unk1A8;
	/* 0x1AC */ TMBindShadowBody* unk1AC;
	/* 0x1B0 */ u8 unk1B0;
	/* 0x1B4 */ JGeometry::TVec3<f32> unk1B4[4];
};

class THanaSamboManager : public TSmallEnemyManager {
public:
	THanaSamboManager(const char* name);

	virtual TSpineEnemy* createEnemyInstance();
	virtual void createModelData();
	virtual void load(JSUMemoryInputStream&);
};

// In the retail binary theNerve() is inlined in every user (the function-local
// static and its __register_global_object call appear in the callers), so the
// accessor is defined in the class here.
#define DECLARE_INLINE_NERVE(Name, T)                                         \
	class Name : public TNerveBase<T> {                                          \
	public:                                                                      \
		virtual BOOL execute(TSpineBase<T>*) const;                                 \
		static const Name& theNerve()                                               \
		{                                                                           \
			static Name instance;                                                      \
			return instance;                                                           \
		}                                                                           \
	};

DECLARE_INLINE_NERVE(TNerveHanaSamboFreeze, TLiveActor);
DECLARE_INLINE_NERVE(TNerveHanaSamboDie, TLiveActor);
DECLARE_INLINE_NERVE(TNerveHanaSamboHide, TLiveActor);
DECLARE_INLINE_NERVE(TNerveHanaSamboWait, TLiveActor);
DECLARE_INLINE_NERVE(TNerveHanaSamboAttack, TLiveActor);
DECLARE_INLINE_NERVE(TNerveHanaSamboAppear, TLiveActor);
DECLARE_INLINE_NERVE(TNerveSamboHeadRecoverWater, TLiveActor);
DECLARE_INLINE_NERVE(TNerveSamboHeadAttack, TLiveActor);
DECLARE_INLINE_NERVE(TNerveSamboHeadHitWater, TLiveActor);
DECLARE_INLINE_NERVE(TNerveSamboHeadAppear, TLiveActor);
DECLARE_INLINE_NERVE(TNerveSamboHeadHide, TLiveActor);

#endif
