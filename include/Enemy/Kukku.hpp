#ifndef ENEMY_KUKKU_HPP
#define ENEMY_KUKKU_HPP

#include <Enemy/SmallEnemy.hpp>
#include <Strategic/HitActor.hpp>

class MActor;
class TMActorKeeper;

// ============= params =============

// TODO: field offsets are a best-effort reconstruction from the (fully
// inlined, UNUSED) TKukkuParams::TKukkuParams(const char*) draft produced by
// m2c; the exact byte offsets have not been double checked against the
// compiled size in mario.MAP yet.
class TKukkuParams : public TSmallEnemyParams {
public:
	TKukkuParams(const char* path);

	f32 getMarchSpeed() const { return mMarchSpeed.get(); }
	f32 getTurnSpeed() const { return mTurnSpeed.get(); }
	f32 getWaterPowerY() const { return mWaterPowerY.get(); }
	s32 getShootSpeed() const { return mShootSpeed.get(); }
	s32 getShootInterval() const { return mShootInterval.get(); }
	f32 getSearchRange() const { return mSearchRange.get(); }
	f32 getHabatakiTimer() const { return mHabatakiTimer.get(); }
	f32 getAirFric() const { return mAirFric.get(); }
	f32 getUpperVelocityY() const { return mUpperVelocityY.get(); }
	f32 getDropSpeed() const { return mDropSpeed.get(); }
	f32 getDropAngleX() const { return mDropAngleX.get(); }

	/* 0x2D4 */ TParamRT<f32> mMarchSpeed;
	/* 0x2E8 */ TParamRT<f32> mTurnSpeed;
	/* 0x2FC */ TParamRT<f32> mWaterPowerY;
	/* 0x310 */ TParamRT<s32> mShootSpeed;
	/* 0x324 */ TParamRT<s32> mShootInterval;
	/* 0x338 */ TParamRT<f32> mSearchRange;
	/* 0x34C */ TParamRT<f32> mHabatakiTimer;
	/* 0x360 */ TParamRT<f32> mAirFric;
	/* 0x374 */ TParamRT<f32> mUpperVelocityY;
	/* 0x388 */ TParamRT<f32> mDropSpeed;
	/* 0x39C */ TParamRT<f32> mDropAngleX;
};

// ============= manager =============

class TKukku;

class TKukkuManager : public TSmallEnemyManager {
public:
	TKukkuManager(const char* name = "クッククマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
};

// ============= ball =============

// TODO: base class inferred from the destructor, which delegates straight to
// ~THitActor with no intermediate class; field layout is a rough guess from
// TKukkuBall::init/perform m2c drafts.
class TKukkuBall : public THitActor {
public:
	TKukkuBall(MActor* mactor = 0);

	virtual void init();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

public:
	/* 0x64 */ u32 unk64;
	/* 0x68 */ MActor* unk68;
};

// ============= coin drop helper =============

// TODO: fully inlined (UNUSED) helper class used by TKukku::dropCoins() to
// hand out coins one at a time from a small fixed pool; reconstructed purely
// from the mangled UNUSED symbol names/sizes in mario.MAP, never verified
// against real asm.
class TEnemyCoinUnit {
public:
	TEnemyCoinUnit(int num);

	void init();
	void* getUnusedItem();

public:
	int unk0;
};

// ============= instance =============

class TKukku : public TSmallEnemy {
public:
	TKukku(const char* name = "クック");

	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void control();
	virtual void bind();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void reset();
	virtual const char** getBasNameTable() const;
	virtual void behaveToWater(THitActor*);
	virtual void setDeadAnm();
	virtual void setAfterDeadEffect();

	void dropCoins();
	void calcMomentum(f32);
	void updateRotation();

	// fabricated
	TKukkuManager* getManager() { return (TKukkuManager*)mManager; }
	TKukkuParams* getSaveParams() const { return (TKukkuParams*)getSaveParam(); }

public:
	// TODO: verify against the size of TKukku's compiled fields; only the
	// offsets actually touched in dropCoins/init/control/reset were recovered
	// from m2c drafts, the exact types are guesses.
	/* 0x194 */ TMActorKeeper* mBallKeeper[3];
	/* 0x1A0 */ void* unk1A0;
	/* 0x1A4 */ s32 unk1A4;
	/* 0x1A8 */ s32 unk1A8;
	/* 0x1AC */ s32 unk1AC;
	/* 0x1B0 */ s32 unk1B0;
};

// ============= nerves =============

DECLARE_NERVE(TNerveKukkuRecoverGraph, TLiveActor);
DECLARE_NERVE(TNerveKukkuPostFall, TLiveActor);
DECLARE_NERVE(TNerveKukkuFall, TLiveActor);
DECLARE_NERVE(TNerveKukkuHit, TLiveActor);
DECLARE_NERVE(TNerveKukkuGraphWander, TLiveActor);

#endif
