#ifndef ENEMY_BOSS_HANACHAN_HPP
#define ENEMY_BOSS_HANACHAN_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <JSystem/JGeometry/JGVec3.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Strategic/Nerve.hpp>

class TLiveActor;
class MActor;
class TIdxGroupObj;
class TMapCollisionMove;
class TBossHanachan;
class TBossHanachanCommonSaveParams;
class TBossHanachanChangeSaveParams;

DECLARE_NERVE(TNerveSBH_Fall, TLiveActor);
DECLARE_NERVE(TNerveSBH_SleepContinue, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanDead, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanSnort, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanDamage, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanGetUp, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanDown, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanTumble, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanGraphWander, TLiveActor);

// TODO: enumerand names are unknown
enum EnumBossHanachanAnmKind {
	BH_ANM_KIND_UNK0,
	BH_ANM_KIND_UNK1,
	BH_ANM_KIND_UNK2,
	BH_ANM_KIND_UNK3,
	BH_ANM_KIND_UNK4,
	BH_ANM_KIND_UNK5,
	BH_ANM_KIND_UNK6,
	BH_ANM_KIND_UNK7,
	BH_ANM_KIND_UNK8,
	BH_ANM_KIND_UNK9,
	BH_ANM_KIND_UNKA,
	BH_ANM_KIND_UNKB,
	BH_ANM_KIND_UNKC,
	BH_ANM_KIND_UNKD,
	BH_ANM_KIND_UNKE,
	BH_ANM_KIND_UNKF,
	BH_ANM_KIND_UNK10,
	BH_ANM_KIND_UNK11,
	BH_ANM_KIND_UNK12,
};

// TODO: enumerand names are guesses based on the nerve that uses them
enum EnumBossHanachanNerveAnm {
	BH_NERVE_ANM_TUMBLE,
	BH_NERVE_ANM_DOWN,
	BH_NERVE_ANM_GET_UP,
	BH_NERVE_ANM_DAMAGE,
	BH_NERVE_ANM_SNORT,
	BH_NERVE_ANM_DEAD,
};

enum EnumBossHanachanStopMotionBlendOnOff {
	BH_STOP_MOTION_BLEND_OFF,
	BH_STOP_MOTION_BLEND_ON,
};

class TSpherePoint {
public:
	TSpherePoint() { }

public:
	/* 0x0 */ JGeometry::TVec3<f32> unk0;
	/* 0xC */ JGeometry::TVec3<f32> unkC;
	/* 0x18 */ JGeometry::TVec3<f32> unk18;
	/* 0x24 */ f32 unk24;
	/* 0x28 */ f32 unk28;
};

class TSphereLink {
public:
	TSphereLink(u16, const JGeometry::TVec3<f32>&, f32, f32, f32, f32, f32,
	            f32);

	BOOL setDegreeZAndRevisionPosXZ(int, f32);
	void moveHead(const JGeometry::TVec3<f32>&);
	void execMapCollision_(JGeometry::TVec3<f32>*);

public:
	/* 0x0 */ u16 mPointNum;
	/* 0x4 */ TSpherePoint* mPoints;
	/* 0x8 */ f32 unk8;
	/* 0xC */ f32 unkC;
	/* 0x10 */ f32 unk10;
	/* 0x14 */ f32 unk14;
	/* 0x18 */ f32 unk18;
};

void BHSCalcRevisionDistXZByRotateZ(f32, f32, f32, f32*, f32*);
f32 BHSCalcCentrifugalForce(const JGeometry::TVec3<f32>&,
                            const JGeometry::TVec3<f32>&,
                            const JGeometry::TVec3<f32>&, f32);

class TFootHitActor : public TWaterHitActor {
public:
	TFootHitActor(const char* name)
	    : TWaterHitActor(name)
	{
	}
	virtual ~TFootHitActor() { }

public:
	/* 0x6C */ MtxPtr unk6C;
};

// fabricated
// TODO: this 0x2C sized object hanging off of every part has no symbols of
// its own, the name is made up
struct TBHNonstopMotionBlend {
	TBHNonstopMotionBlend(int frames)
	    : unk0(1)
	    , unk4(frames)
	    , unk8(0)
	    , unkC(0.0f)
	    , unk10(0.0f)
	    , unk14(0.0f)
	    , unk18(0.0f)
	    , unk1C(0.0f)
	    , unk20(0.0f)
	    , unk24(0)
	    , unk28(0.0f)
	{
	}

	/* 0x0 */ int unk0;
	/* 0x4 */ int unk4;
	/* 0x8 */ int unk8;
	/* 0xC */ f32 unkC;
	/* 0x10 */ f32 unk10;
	/* 0x14 */ f32 unk14;
	/* 0x18 */ f32 unk18;
	/* 0x1C */ f32 unk1C;
	/* 0x20 */ f32 unk20;
	/* 0x24 */ int unk24;
	/* 0x28 */ f32 unk28;
};

class TBossHanachanPartsBase : public TLiveActor {
public:
	TBossHanachanPartsBase(TBossHanachan*, u32, int, const char*);
	virtual ~TBossHanachanPartsBase() { }

	virtual const char** getBasNameTable() const;
	virtual BOOL setAnm_(EnumBossHanachanAnmKind,
	                     EnumBossHanachanStopMotionBlendOnOff)
	    = 0;

	void considerSetAnm_(EnumBossHanachanNerveAnm);
	bool isReactToTrampleOrHipDrop_() const;
	void calcRotateZWhenGetUp_();
	bool isMarioOn_() const;
	TLiveActor* getSandActor_() const;
	void copyFrameFromOldAnmToNewAnm_();
	bool isCurBckAlreadyEnd_() const;
	void setDamageFog_(JDrama::TGraphics*);
	void entryCircleShadow_();
	void moveMapCollision_();
	void changeTumbleAnmRate_();
	void restartBck_();
	void setNonstopMotionBlendRatio_(f32);
	void offNonstopMotionBlend_();
	void initMapCollisionAndHitActor_(TIdxGroupObj*);

public:
	/* 0xF4 */ EnumBossHanachanAnmKind mCurAnm;
	/* 0xF8 */ EnumBossHanachanAnmKind mOldAnm;
	/* 0xFC */ TBossHanachan* mOwner;
	/* 0x100 */ TWaterHitActor* mHitActor;
	/* 0x104 */ TMapCollisionMove* mMapCollision;
	/* 0x108 */ MtxPtr mMapCollisionJointMtx;
	/* 0x10C */ int unk10C;
	/* 0x110 */ TBHNonstopMotionBlend* mNonstopMotionBlend;
};

class TBossHanachanPartsHead : public TBossHanachanPartsBase {
public:
	TBossHanachanPartsHead(TBossHanachan*, const char*);
	virtual ~TBossHanachanPartsHead() { }

	virtual BOOL receiveMessage(THitActor*, u32);
	virtual BOOL setAnm_(EnumBossHanachanAnmKind,
	                     EnumBossHanachanStopMotionBlendOnOff);

public:
	/* 0x114 */ MtxPtr mNoseHallMtxL;
	/* 0x118 */ MtxPtr mNoseHallMtxR;
};

class TBossHanachanPartsBody : public TBossHanachanPartsBase {
public:
	TBossHanachanPartsBody(TBossHanachan*, const char*);
	virtual ~TBossHanachanPartsBody() { }

	virtual BOOL receiveMessage(THitActor*, u32);
	virtual BOOL setAnm_(EnumBossHanachanAnmKind,
	                     EnumBossHanachanStopMotionBlendOnOff);

	void initFootHitActor_(TIdxGroupObj*);

public:
	/* 0x114 */ int mBodyIndex;
	/* 0x118 */ TFootHitActor* mFootHitActor[2];
	/* 0x120 */ JGeometry::TVec3<f32> unk120;
	/* 0x12C */ JGeometry::TVec3<f32> unk12C;
	/* 0x138 */ JGeometry::TVec3<f32> unk138;
	/* 0x144 */ f32 unk144;
	/* 0x148 */ f32 unk148;
	// indexed by foot in TBossHanachan::emitParticle_, so this is an array:
	// [0] is the L3 leg joint, [1] the R3 one (same order as mFootHitActor)
	/* 0x14C */ MtxPtr mLegMtx[2];
	/* 0x154 */ JGeometry::TVec3<f32> unk154;
};

class TBossHanachan : public TSpineEnemy {
public:
	TBossHanachan(const char* name = "ボスハナチャン");
	virtual ~TBossHanachan() { }

	virtual void perform(u32, JDrama::TGraphics*);
	virtual void init(TLiveManager*);
	virtual void bind();
	virtual void moveObject();
	virtual void kill();
	virtual BOOL hasMapCollision() const;

	void removeAllMapCollision();
	void execDamage();
	void goToInitialRecoverGraphNode();
	void execSlip();
	void execWalk(bool);
	bool isCanWalk() const;
	f32 getBodyMaxRotateZ() const;
	bool checkFallDecideAndSetup();
	bool isTumbleCompletelyAllBody() const;
	void execBodyCalcAnim_();
	void execHeadCalcAnim_();
	void throwMario_(THitActor*);
	void setRandomWeakBodyIndex();

	void changeAnmRateAndFrameUpdate_();
	void copyFrameFromOldAnmToNewAnm_();
	void setHeadAndBodyNonstopMotionBlendRatio_(f32);
	void offHeadAndBodyNonstopMotionBlend_();
	bool isAllBckAlreadyEnd(EnumBossHanachanAnmKind) const;
	bool isFinishedGetUp() const;
	void considerSetAnm(EnumBossHanachanNerveAnm);
	void setAnmTimerWhenDead();
	void setAnmTimerWhenDamage();
	void setAnmTimerWhenSnort();
	void setAnmTimerWhenGetUp();
	void setTumbleAnm(EnumBossHanachanStopMotionBlendOnOff);
	void setTumbleBckRate_(TBossHanachanPartsBase*);
	void setHeadAndBodyAnm(EnumBossHanachanAnmKind,
	                       EnumBossHanachanStopMotionBlendOnOff);

	void emitCamShake_();
	void emitOneTimeSandPillar_(TBossHanachanPartsBody*);
	void emitParticle_();
	static void staticLoadParticle();

public:
	/* 0x150 */ TBossHanachanPartsBody* mBody[8];
	/* 0x170 */ TBossHanachanPartsHead* mHead;
	/* 0x174 */ int mWeakBodyIndex;
	/* 0x178 */ TSphereLink* mSphereLink;
	/* 0x17C */ JGeometry::TVec3<f32> unk17C;
	/* 0x188 */ JGeometry::TVec3<f32> unk188;
	/* 0x194 */ f32 unk194;
	/* 0x198 */ f32 unk198;
	/* 0x19C */ MActor* mSandPillar;
	/* 0x1A0 */ JGeometry::TVec3<f32> unk1A0;
	/* 0x1AC */ JGeometry::TVec3<f32> unk1AC;
	/* 0x1B8 */ int unk1B8;
	/* 0x1BC */ TBossHanachanCommonSaveParams* mCommonSaveParams;
	/* 0x1C0 */ TBossHanachanChangeSaveParams* mChangeSaveParams;
};

class TBossHanachanManager : public TEnemyManager {
public:
	TBossHanachanManager(const char*);
	virtual ~TBossHanachanManager() { }

	virtual void loadAfter();
	virtual void createModelData();
	virtual BOOL hasMapCollision() const;
	virtual void clipEnemies(JDrama::TGraphics*);

public:
	/* 0x54 */ TBossHanachanCommonSaveParams* mCommonSaveParams;
	/* 0x58 */ TBossHanachanChangeSaveParams* mChangeSaveParams[3];
};

#endif
