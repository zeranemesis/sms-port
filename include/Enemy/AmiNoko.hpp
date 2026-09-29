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
	virtual bool isCollidMove(THitActor*);
	virtual bool isHitValid(u32);
	virtual void attackToMario();
	virtual void setWalkAnm();
	virtual void behaveToWater(THitActor*);
	virtual void reset();
	virtual void setMActorAndKeeper();

	void calcDirection();
	void emitEffects();

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
	/* 0x214 */ char unk214[0x428 - 0x214];
	/* 0x428 */ JGeometry::TVec3<f32> unk428;
};

class TAmiHit : public THitActor {
public:
	TAmiHit(const char* name) : THitActor(name) { }

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

DECLARE_NERVE(TNerveAmiNokoDie, TLiveActor);
DECLARE_NERVE(TNerveAmiNokoFreeze, TLiveActor);
DECLARE_NERVE(TNerveAmiNokoWalkOnFence, TLiveActor);
DECLARE_NERVE(TNerveAmiNokoTurn, TLiveActor);

#endif
