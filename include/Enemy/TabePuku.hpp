#ifndef ENEMY_TABEPUKU_HPP
#define ENEMY_TABEPUKU_HPP

#include <Enemy/SmallEnemy.hpp>

// ============= collision =============

class TTPHitActor : public THitActor {
public:
	TTPHitActor(const char* name = "たべプクコリジョン");
	virtual ~TTPHitActor();

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init();

	void bind();
	void updateTerrainCollsion();
};

// ============= manager =============

class TTabePuku;

class TTabePukuManager : public TSmallEnemyManager {
public:
	TTabePukuManager(const char* name = "たべプクマネージャー");
	virtual ~TTabePukuManager();

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
};

// ============= instance =============

class TTabePuku : public TSmallEnemy {
public:
	TTabePuku(const char* name = "たべプク");
	virtual ~TTabePuku();

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual MtxPtr getTakingMtx();
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void control();
	virtual void bind();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void attackToMario();
	virtual void forceKill();
	virtual bool doKeepDistance();
	virtual bool isFindMario(float);

	// fabricated
	void swimTo(const JGeometry::TVec3<f32>&);

public:
	/* TODO: instance member layout not yet reconstructed */
	/* 0x1A0 */ TTPHitActor* unk1A0;
};

// ============= nerves =============

DECLARE_NERVE(TNerveTabePukuDrag, TLiveActor);
DECLARE_NERVE(TNerveTabePukuDive, TLiveActor);
DECLARE_NERVE(TNerveTabePukuBite, TLiveActor);
DECLARE_NERVE(TNerveTabePukuAttack, TLiveActor);
DECLARE_NERVE(TNerveTabePukuRecoverGraph, TLiveActor);
DECLARE_NERVE(TNerveTabePukuFound, TLiveActor);
DECLARE_NERVE(TNerveTabePukuGraphWander, TLiveActor);

#endif
