#ifndef ENEMY_AMI_NOKO_HPP
#define ENEMY_AMI_NOKO_HPP

#include <Enemy/WalkerEnemy.hpp>

class TAmiNoko : public TWalkerEnemy {
public:
	TAmiNoko(const char* name = "アミノコ");

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor*, u32);
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

	// fabricated
	// TODO: verify param type name

public:
	/* 0x194 */ u32 unk194;
	/* 0x198 */ u32 unk198;
	/* 0x19C */ JGeometry::TVec3<f32> unk19C;
	/* 0x1A8 */ JGeometry::TVec3<f32> unk1A8;
	/* 0x1B4 */ JGeometry::TVec3<f32> unk1B4;
	/* 0x1C0 */ JGeometry::TVec3<f32> unk1C0;
	/* 0x1CC */ char unk1CC[0x20C - 0x1CC];
	/* 0x20C */ s8 unk20C;
};

class TAmiHit : public THitActor {
public:
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor*, u32);
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
