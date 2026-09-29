#ifndef ENEMY_KAZEKUN_HPP
#define ENEMY_KAZEKUN_HPP

#include <Enemy/SmallEnemy.hpp>
#include <Strategic/Nerve.hpp>

class TKazekunParams;

// TODO: field offsets below 0x194 were recovered from store-instruction
// offsets in TKazekun::TKazekun / ::reset / the nerve executes only; their
// real names/semantics (and any padding between them) are not yet verified.
class TKazekun : public TSmallEnemy {
public:
	TKazekun(const char* name = "カゼクン");
	virtual ~TKazekun() { }

	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void setDeadAnm();
	virtual void attackToMario();
	virtual bool isCollidMove(THitActor*);

	void flyAroundMario();
	void doAttackPose(bool);

	// fabricated (same pattern as TAmenbo::getSaveParam2)
	TKazekunParams* getSaveParam2() const
	{
		return (TKazekunParams*)getSaveParam();
	}

public:
	/* 0x194 */ JGeometry::TVec3<f32> unk194; // TODO: spawn/reset position?
	/* 0x1A0 */ JGeometry::TQuat4<f32> unk1A0; // TODO: reset to identity in ::reset
	/* 0x1B0 */ s32 unk1B0; // TODO: wait-nerve timeout, compared against spine time
};

class TKazekunParams : public TSmallEnemyParams {
public:
	TKazekunParams(const char*);

	/* 0x2D4 */ TParamRT<f32> mAppearDist;      // value @ 0x2E4
	/* 0x2E8 */ TParamRT<f32> mAroundDist;      // value @ 0x2F8
	/* 0x2FC */ TParamRT<f32> mAroundSpeed;     // value @ 0x30C
	/* 0x310 */ TParamRT<s32> mAroundTime;      // value @ 0x320
	/* 0x324 */ TParamRT<f32> mAttackSpeed;     // value @ 0x334
	/* 0x338 */ TParamRT<f32> mAirFric;         // value @ 0x348 (default 0.97)
	/* 0x34C */ TParamRT<s32> mResetTime;       // value @ 0x35C (default 300)
	/* 0x360 */ TParamRT<s32> mResetTimeHitting;// value @ 0x370 (default 1500)
	/* 0x374 */ TParamRT<s32> mPoseTime;        // value @ 0x384 (default 120)
	/* 0x388 */ TParamRT<f32> mDicideTiming;    // value @ 0x398
	/* 0x39C */ TParamRT<f32> mTurnOffsetY;     // value @ 0x3AC
	/* 0x3B0 */ TParamRT<f32> mLostOffsetYUp;   // value @ 0x3C0
	/* 0x3C4 */ TParamRT<f32> mLostOffsetYDown; // value @ 0x3D4
	/* 0x3D8 */ TParamRT<f32> mPoseSpeed;       // value @ 0x3E8
	/* 0x3EC */ TParamRT<f32> mPoseOmegaRate;   // value @ 0x3FC
};

class TKazekunManager : public TSmallEnemyManager {
public:
	TKazekunManager(const char* name = "カゼクンマネージャー");
	virtual ~TKazekunManager() { }

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
};

DECLARE_NERVE(TNerveKazekunSearch, TLiveActor);
DECLARE_NERVE(TNerveKazekunTurn, TLiveActor);
DECLARE_NERVE(TNerveKazekunAppear, TLiveActor);
DECLARE_NERVE(TNerveKazekunWait, TLiveActor);
DECLARE_NERVE(TNerveKazekunPreAttack, TLiveActor);
DECLARE_NERVE(TNerveKazekunAttack, TLiveActor);
DECLARE_NERVE(TNerveKazekunDisappear, TLiveActor);
DECLARE_NERVE(TNerveKazekunHitWater, TLiveActor);

#endif
