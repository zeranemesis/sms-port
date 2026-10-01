#ifndef ENEMY_AMI_NOKO_HPP
#define ENEMY_AMI_NOKO_HPP

#include <Enemy/WalkerEnemy.hpp>

class TAmiHit;

class TAmiNokoParams : public TWalkerEnemyParams {
public:
	TAmiNokoParams(const char* path)
	    : TWalkerEnemyParams(path)
	    , PARAM_INIT(mSLElecRange, 200.0f)
	    , PARAM_INIT(mSLMtxRotSpeed, 0.05f)
	{
		TParams::load(mPrmPath);
	}

public:
	/* 0x32C */ TParamRT<f32> mSLElecRange;
	/* 0x340 */ TParamRT<f32> mSLMtxRotSpeed;
};

class TAmiNoko : public TWalkerEnemy {
public:
	TAmiNoko(const char* name = "アミノコ");

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void load(JSUMemoryInputStream&);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual f32 getGravityY() const;
	virtual const char** getBasNameTable() const;
	virtual bool isCollidMove(THitActor*) { return false; }
	virtual bool isHitValid(u32);
	virtual void attackToMario();
	virtual void setWalkAnm();
	virtual void behaveToWater(THitActor*);
	virtual void reset();
	virtual void setMActorAndKeeper();

	void calcDirection();
	void emitEffects();
	// UNUSED in the map at 0x1EC; inlined at both call sites. The first
	// argument is the per-step distance cap (3.0f from WalkOnFence, 0.0f
	// from Turn) and the second the normalise scale.
	void creepToCurPathNode(f32 maxStep, f32 scale);
	bool isDeadByWall();

public:
	/* 0x194 */ const TBGCheckData* unk194;
	/* 0x198 */ u32 unk198;
	/* 0x19C */ JGeometry::TVec3<f32> unk19C;
	/* 0x1A8 */ JGeometry::TVec3<f32> unk1A8;
	/* 0x1B4 */ JGeometry::TVec3<f32> unk1B4;
	/* 0x1C0 */ JGeometry::TVec3<f32> unk1C0;
	/* 0x1CC */ Mtx unk1CC;
	/* 0x1FC */ JGeometry::TVec3<f32> unk1FC;
	/* 0x208 */ TSpineEnemyParams* unk208;
	/* 0x20C */ bool unk20C;
	/* 0x210 */ TAmiHit* mAmiHit;
	// Nothing past 0x214: the largest offset any code touches on `this` in
	// build/GMSP01/asm/Enemy/amiNoko.s is 0x210, and MarNameRefGen_Enemy's
	// "AmiNoko" branch does `li r3, 0x214` for `new TAmiNoko`. (There used
	// to be a fabricated `char unk214[0x428 - 0x214]` + `unk428` here, which
	// made the class 0x434 and was never referenced by anything.)
};

class TAmiHit : public THitActor {
public:
	TAmiHit(const char* name) : THitActor(name) { }

	// Defined in-class so MWCC gives __dt__7TAmiHitFv the weak linkage the
	// map records (a .cpp definition comes out global).
	virtual ~TAmiHit() { }
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor*, u32);

public:
	/* 0x68 */ TAmiNoko* mParent;
};

class TAmiNokoManager : public TSmallEnemyManager {
public:
	TAmiNokoManager(const char* name = "アミノコマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSmallEnemy* createEnemyInstance();
};

class TLiveActor;

DECLARE_NERVE(TNerveAmiNokoAttack, TLiveActor);
DECLARE_NERVE(TNerveAmiNokoDie, TLiveActor);
DECLARE_NERVE(TNerveAmiNokoFreeze, TLiveActor);
DECLARE_NERVE(TNerveAmiNokoWalkOnFence, TLiveActor);
DECLARE_NERVE(TNerveAmiNokoTurn, TLiveActor);

#endif
