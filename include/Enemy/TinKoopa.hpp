#ifndef ENEMY_TIN_KOOPA_HPP
#define ENEMY_TIN_KOOPA_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <M3DUtil/M3UJoint.hpp>
#include <Strategic/Spine.hpp>

class TTinKoopa;
class TCoasterKiller;
class TGraphWeb;
class TMapCollisionMove;
class MActor;

// Mecha-Bowser (the Pinna Park boss).

class TTinKoopaParams : public TSpineEnemyParams {
public:
	TTinKoopaParams(const char* path);

	/* 0xA8 */ TParamRT<s32> mSLPartsHP;
	/* 0xBC */ TParamRT<s32> mSLFlameHP;
	/* 0xD0 */ TParamRT<s32> mSLFlameRevivalTime;
	/* 0xE4 */ TParamRT<f32> mSLFlameDamageRadius0;
	/* 0xF8 */ TParamRT<f32> mSLFlameDamageHeight0;
	/* 0x10C */ TParamRT<f32> mSLFlameDamageRadius1;
	/* 0x120 */ TParamRT<f32> mSLFlameDamageHeight1;
	/* 0x134 */ TParamRT<f32> mSLDamageRadius;
	/* 0x148 */ TParamRT<f32> mSLDamageHeight0;
	/* 0x15C */ TParamRT<f32> mSLDamageHeight1;
	/* 0x170 */ TParamRT<s32> mSLKillerInterval;
	/* 0x184 */ TParamRT<s32> mSLDefeatWaitTime;
	/* 0x198 */ TParamRT<f32> mSLKillerApproachingDistance;
};

class TTinKoopaManager : public TEnemyManager {
public:
	TTinKoopaManager(const char* name = "メカクッパマネージャ");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void loadAfter();
	virtual void createModelData();
	virtual BOOL hasMapCollision() const { return TRUE; }
	virtual TSpineEnemy* createEnemyInstance();
};

class TTinKoopaMtxCalc : public M3UMtxCalcSIAnmBlendQuat {
public:
	TTinKoopaMtxCalc(TTinKoopa* owner);

	virtual void calc(u16);

	void joinAnm(int);

public:
	/* 0x64 */ TTinKoopa* mOwner;
};

// One of the breakable body parts, each one also has its own map collision.
class TTinKoopaPartsBase : public TLiveActor {
public:
	TTinKoopaPartsBase(const char* name, int index, TTinKoopa* owner);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void reset();

	void emitPartsDisappearEffects(const char**, int, f32);
	void emitPartsDisappearEffects();
	void emitPartsTrackEffects(const char**, int);
	void emitPartsTrackEffects();
	void startBreaking();
	void resetTinKoopaPartsBase();
	void initTinKoopaPartsBase();

public:
	/* 0xF4 */ TMapCollisionMove* mCollision;
	/* 0xF8 */ u8 mIsBreaking;
	/* 0xFC */ int mIndex;
	/* 0x100 */ TTinKoopa* mOwner;
	/* 0x104 */ MActor* mBreakActor;
	/* 0x108 */ JGeometry::TVec3<f32> unk108[6];
};

class TTinKoopaLaunchOrder {
public:
	TTinKoopaLaunchOrder(TTinKoopa* owner);

	void makeOrder(s8 lap, long frame, s8 count, s8 side);
	void checkOrder();

public:
	/* 0x0 */ TTinKoopa* mOwner;
	/* 0x4 */ s8 mLap;
	/* 0x8 */ long mFrame;
	/* 0xC */ s8 mCount;
	/* 0xD */ s8 mSide;
};

class TTinKoopaLaunchSchedule {
public:
	TTinKoopaLaunchSchedule(u8 num, TTinKoopa* owner);

	void checkOrder();

public:
	/* 0x0 */ u8 mOrderNum;
	/* 0x4 */ TTinKoopa* mOwner;
	/* 0x8 */ TTinKoopaLaunchOrder** mOrders;
};

// The flame breath, a hit actor that follows the mouth joint.
class TTinKoopaFlame : public THitActor {
public:
	TTinKoopaFlame(const char* name, TTinKoopa* owner);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);

	void emitFlameEffects();
	bool isHighPosition();
	void checkMario();
	void hitWater();
	void resetTinKoopaFlame();
	void makeHitCollision();

public:
	/* 0x68 */ TTinKoopa* mOwner;
	/* 0x6C */ f32 unk6C;
	/* 0x70 */ s16 mHitPoints;
	/* 0x72 */ u8 mIsHit;
};

class TTinKoopa : public TSpineEnemy {
public:
	TTinKoopa(const char* name = "メカクッパ");

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager* manager);
	virtual BOOL hasMapCollision() const { return TRUE; }
	virtual const char** getBasNameTable() const;
	virtual void reset();

	void emitTinKoopaEffects();
	void startTinKoopaMessage(u32);
	void checkTinKoopaFirstFlameMessage();
	void checkTinKoopaFirstRocketMessage();
	void checkTinKoopaKillerApproachingMessage();
	void checkTinKoopaMessage();
	void checkKillerLaunch();
	void launchKiller(int side);
	void startBreakingParts();
	void hitParts();
	void changeBck(int);
	void updateTimers();
	void makeEyeBeamEffect();
	bool checkTruckAnimationPass(int);
	void checkLap();
	void makeKillerQueue(int, s8);
	void makeHitCollision();
	void resetTinKoopa();
	bool checkKillerApproachingFromBack(TCoasterKiller*,
	                                    JGeometry::TVec3<f32>, f32);
	f32 calcCoasterDistanceInOrder(int, int);
	f32 calcCoasterDistance(int, int);
	void makeCoasterDistanceTable();
	void makeLaunchSchedule();

	TTinKoopaParams* getParams() const
	{
		return (TTinKoopaParams*)getSaveParam();
	}

public:
	/* 0x150 */ int mPhase;
	/* 0x154 */ int unk154;
	/* 0x158 */ int unk158;
	/* 0x15C */ int mLap;
	/* 0x160 */ TTinKoopaFlame* mFlame;
	/* 0x164 */ MActor* mTruck; // mario's koopa rail
	/* 0x168 */ u8 unk168;
	/* 0x169 */ s8 mKillerQueue[4];
	/* 0x170 */ int mKillerQueueNum;
	/* 0x174 */ int mKillerQueueIdx;
	/* 0x178 */ int mKillerTimer;
	/* 0x17C */ int mFlameTimer;
	/* 0x180 */ int mDefeatTimer;
	/* 0x184 */ char unk184[0x1B4 - 0x184];
	/* 0x1B4 */ f32 unk1B4;
	/* 0x1B8 */ f32 unk1B8;
	/* 0x1BC */ f32 unk1BC;
	/* 0x1C0 */ f32 unk1C0;
	/* 0x1C4 */ char unk1C4[4];
	/* 0x1C8 */ int mPartsHP;
	/* 0x1CC */ TTinKoopaPartsBase* mParts[6];
	/* 0x1E4 */ TTinKoopaPartsBase* mBreakingParts;
	/* 0x1E8 */ f32* mCoasterDistanceTable;
	/* 0x1EC */ TGraphWeb* mGraph;
	/* 0x1F0 */ TEnemyManager* mKillerManager;
	/* 0x1F4 */ TTinKoopaLaunchSchedule* mLaunchSchedule;
	/* 0x1F8 */ int unk1F8;
};

void printTinKoopaDebugInfo(TTinKoopa*);

DECLARE_NERVE(TNerveTinKoopaBreak, TLiveActor);
DECLARE_NERVE(TNerveTinKoopaDamage, TLiveActor);
DECLARE_NERVE(TNerveTinKoopaWait, TLiveActor);

#endif
