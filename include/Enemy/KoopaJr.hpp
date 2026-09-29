#ifndef ENEMY_KOOPA_JR_HPP
#define ENEMY_KOOPA_JR_HPP

#include <Strategic/Nerve.hpp>
#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/HitActor.hpp>
#include <M3DUtil/MActor.hpp>

class TBathtub;
class TBathtubBinder;
class TBathtubKiller;
class TKoopa;
class TKoopaJr;

// A small helper for angles in radians, used by TKoopaJrSubmarine to steer.
class TDirectionCalc {
public:
	TDirectionCalc();
	TDirectionCalc(f32);
	TDirectionCalc(JGeometry::TVec3<f32>);

	static f32 r2d(f32);
	static f32 d2r(f32);

	void normalize();
	f32 calcNearerDirection(f32);
	void sub(f32);
	f32 calcTurnDirection(f32, f32);
	void makeDirection(JGeometry::TVec3<f32>);
	void calcDirectionVector();
	f32 absDirection(f32);

public:
	/* 0x0 */ f32 mDirection;
};

class TKoopaJrParams : public TSpineEnemyParams {
public:
	TKoopaJrParams(const char* path);

	/* 0xA8 */ TParamRT<f32> mSLLaunchKillerLimit;
	/* 0xBC */ TParamRT<f32> mSLDamageRadius;
	/* 0xD0 */ TParamRT<f32> mSLDamageHeight;
	/* 0xE4 */ TParamRT<f32> mSLKoopaJrScale;
	/* 0xF8 */ TParamRT<f32> mSLFastLaunchDistance;
	/* 0x10C */ TParamRT<s32> mSLDamagePeriod;
	/* 0x120 */ TParamRT<s32> mSLLaunchKillerPeriod;
	/* 0x134 */ TParamRT<s32> mSLLaunchKillerPeriodFast;
};

class TKoopaJrSubmarineParams : public TSpineEnemyParams {
public:
	TKoopaJrSubmarineParams(const char* path);

	/* 0xA8 */ TParamRT<f32> killerTargetDistanceMin;
	/* 0xBC */ TParamRT<f32> killerTargetDistance;
	/* 0xD0 */ TParamRT<f32> bottomHeight;
	/* 0xE4 */ TParamRT<f32> centerZ;
	/* 0xF8 */ TParamRT<f32> aboidKoopaFlameAngle;
	/* 0x10C */ TParamRT<f32> traceMarioAngle;
	/* 0x120 */ TParamRT<f32> mSLWavePhaseVelocity;
	/* 0x134 */ TParamRT<f32> mSLWaveAmplitudeMin;
	/* 0x148 */ TParamRT<f32> mSLWaveAmplitudeMaxLaunch;
	/* 0x15C */ TParamRT<f32> mSLWaveAmplitudeMax;
	/* 0x170 */ TParamRT<f32> mSLSwingPhaseVelocity;
	/* 0x184 */ TParamRT<f32> mSLSwingAmplitudeMin;
	/* 0x198 */ TParamRT<f32> mSLSwingAmplitudeMax;
	/* 0x1AC */ TParamRT<f32> mSLRoundAngleVelocity;
	/* 0x1C0 */ TParamRT<f32> mSLRoundDistance;
	/* 0x1D4 */ TParamRT<f32> mSLAcceleration;
	/* 0x1E8 */ TParamRT<f32> mSLRotationSpeed;
	/* 0x1FC */ TParamRT<f32> mSLSpeedMax;
	/* 0x210 */ TParamRT<f32> mSLKoopaJrSubmarineScale;
	/* 0x224 */ TParamRT<f32> mSLDamageRadius;
	/* 0x238 */ TParamRT<f32> mSLDamageHeight;
	/* 0x24C */ TParamRT<f32> shineKillerProbability0;
	/* 0x260 */ TParamRT<f32> shineKillerProbability1;
	/* 0x274 */ TParamRT<s32> mSLKillerIntervalFast;
	/* 0x288 */ TParamRT<s32> mSLKillerInterval;
};

// Hit actor that forwards every message it receives to its owner.
class TCallbackHitActor : public THitActor {
public:
	TCallbackHitActor(const char* name, u32 actorType, f32 radius, f32 height,
	                  THitActor* owner);

	virtual BOOL receiveMessage(THitActor* sender, u32 message);

public:
	/* 0x68 */ THitActor* mOwner;
};

class TKoopaJrManager : public TEnemyManager {
public:
	TKoopaJrManager(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

class TKoopaJrSubmarineManager : public TEnemyManager {
public:
	TKoopaJrSubmarineManager(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

class TKoopaJrSubmarine : public TSpineEnemy {
public:
	TKoopaJrSubmarine(const char* name);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual const char** getBasNameTable() const;
	virtual void reset();

	void resetKoopaJrSubmarine();
	void makeCollisionPositions();
	void moveSwing();
	void getSwingAngle();
	void getWaveAngle();
	// defined here: the original inlines this at every call site, so it never
	// appears as a symbol
	void damageKoopaJrSubmarine() { unk18C = 1; }
	// defined here: the original inlines this at every call site, so it never
	// appears as a symbol
	void setAnimationIndex(int index)
	{
		mMActor->setBckFromIndex(index);
		setAnmSound(getBas(index));
	}
	void prepareKillerLaunch(int);
	void prepareKillerLaunchFast(int);
	int appearShineKiller(int);
	void checkKillerLaunch();
	void launchKiller();
	void makeKillerVelocity(TBathtubKiller*, JGeometry::TVec3<f32>);
	void emitKoopaJrSubmarineEffects();
	void updateTimers();
	void setKoopaJr(TKoopaJr*);
	void makeRelativeAngle();
	void makeRoundVelocity();
	void makeDirection();
	void checkNerve();

	// fabricated
	TKoopaJrSubmarineParams* getParams() const
	{
		return (TKoopaJrSubmarineParams*)getSaveParam();
	}

public:
	// TODO: names; offsets from the ctor/resetKoopaJrSubmarine stores
	/* 0x150 */ s32 unk150; // killer launch timer
	/* 0x154 */ f32 unk154;
	/* 0x158 */ f32 unk158;
	/* 0x15C */ f32 unk15C;
	/* 0x160 */ f32 unk160;
	/* 0x164 */ TDirectionCalc unk164;
	/* 0x168 */ f32 unk168;
	/* 0x16C */ f32 unk16C;
	/* 0x170 */ u8 unk170;
	/* 0x174 */ TBathtubBinder* unk174;
	/* 0x178 */ u8 unk178[8]; // per-killer shine flags
	/* 0x180 */ s32 unk180;   // killers launched so far
	/* 0x184 */ s32 unk184;   // killers to launch
	/* 0x188 */ f32 unk188;   // saved bck frame rate
	/* 0x18C */ u8 unk18C;    // got sprayed this frame
	/* 0x190 */ f32 unk190;   // swing amplitude
	/* 0x194 */ f32 unk194;   // swing phase
	/* 0x198 */ f32 unk198;   // wave amplitude
	/* 0x19C */ f32 unk19C;   // wave phase
	/* 0x1A0 */ TKoopaJr* unk1A0;
	/* 0x1A4 */ TCallbackHitActor* unk1A4; // rear body
	/* 0x1A8 */ TCallbackHitActor* unk1A8; // front body
};

class TKoopaJr : public TSpineEnemy {
public:
	TKoopaJr(const char* name);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual const char** getBasNameTable() const;
	virtual void reset();

	void resetKoopaJr();
	void startKoopaJrMessage(u32);
	void emitKoopaJrEffects();
	// defined here: the original inlines this at every call site, so it never
	// appears as a symbol
	void setAnimationIndex(int index)
	{
		mMActor->setBckFromIndex(index);
		setAnmSound(getBas(index));
	}
	void updateTimers();
	void damageKoopaJr();
	void checkSubmarineSwing();
	void startDamageNerve();
	void checkNerve();
	void checkNerveKillerLaunchNormal();
	void checkNerveKillerLaunchFast();
	void checkNerveKillerHit();
	void getBathtubY();

	// fabricated
	TKoopaJrParams* getParams() const
	{
		return (TKoopaJrParams*)getSaveParam();
	}

public:
	/* 0x150 */ s32 unk150; // damage timer
	/* 0x154 */ s32 unk154; // killer launch timer
	/* 0x158 */ s32 unk158; // fast killer launch timer
	/* 0x15C */ TBathtub* unk15C;
	/* 0x160 */ TKoopa* unk160; // Bowser
	/* 0x164 */ TKoopaJrSubmarine* unk164;
	/* 0x168 */ TKoopaJrSubmarineManager* unk168;
	/* 0x16C */ TEnemyManager* unk16C; // bathtub killer manager
};

DECLARE_NERVE(TNerveKoopaJrSubmarineLaunchKiller, TLiveActor);
DECLARE_NERVE(TNerveKoopaJrSubmarineCannonOpenClose, TLiveActor);
DECLARE_NERVE(TNerveKoopaJrSubmarineWait, TLiveActor);
DECLARE_NERVE(TNerveKoopaJrYahoo, TLiveActor);
DECLARE_NERVE(TNerveKoopaJrLaunch, TLiveActor);
DECLARE_NERVE(TNerveKoopaJrDemo, TLiveActor);
DECLARE_NERVE(TNerveKoopaJrDamage, TLiveActor);
DECLARE_NERVE(TNerveKoopaJrWait, TLiveActor);

#endif
