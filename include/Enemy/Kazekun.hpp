#ifndef ENEMY_KAZEKUN_HPP
#define ENEMY_KAZEKUN_HPP

#include <Enemy/SmallEnemy.hpp>
#include <Strategic/Nerve.hpp>

// TODO: field offsets below 0x194 were recovered from store-instruction
// offsets in TKazekun::TKazekun / ::reset / the nerve executes only; their
// real names/semantics (and any padding between them) are not yet verified.
class TKazekun : public TSmallEnemy {
public:
	TKazekun(const char* name = "カゼクン");

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

public:
	/* 0x194 */ JGeometry::TVec3<f32> unk194; // TODO: spawn/reset position?
	/* 0x1A0 */ JGeometry::TQuat4<f32> unk1A0; // TODO: reset to identity in ::reset
	/* 0x1B0 */ s32 unk1B0; // TODO: wait-nerve timeout, compared against spine time
};

class TKazekunParams : public TSmallEnemyParams {
public:
	TKazekunParams(const char*);

	// TODO: not yet reconstructed (size 0x300 ctor, all TParamRT fields unknown).
};

class TKazekunManager : public TSmallEnemyManager {
public:
	TKazekunManager(const char* name = "カゼクンマネージャー");

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
