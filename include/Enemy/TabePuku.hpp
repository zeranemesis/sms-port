#ifndef ENEMY_TABEPUKU_HPP
#define ENEMY_TABEPUKU_HPP

#include <Enemy/SmallEnemy.hpp>
#include <JSystem/JGeometry/JGQuat4.hpp>
#include <JSystem/JGeometry/JGRotation3.hpp>

class TBGCheckData;
class TTabePuku;
class TTabePukuParams;

// ============= collision =============

class TTPHitActor : public THitActor {
public:
	// NB: the constructor is inline in the original - marioEU.dol has no
	// __ct__11TTPHitActor symbol, only the base THitActor call followed by a
	// vtable store at the `new` site.
	TTPHitActor(const char* name = "えだぶくろ")
	    : THitActor(name)
	{
	}
	virtual ~TTPHitActor();

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init();

	void bind();
	void updateTerrainCollsion();

	// fabricated
	f32 getMouthYOffset() const { return mMouthYOffset; }
	f32 getMouthRadius() const { return mMouthRadius; }
	f32 getGroundY() const { return mGroundY; }
	bool isOnGround() const { return mAirborne == 0 ? true : false; }
	bool isTouchedWall() const { return mTouchedWall != 0 ? true : false; }
	const TBGCheckData* getGroundPlane() const { return mGroundPlane; }
	const JGeometry::TVec3<f32>& getVel() const { return mVel; }

public:
	/* 0x68 */ TTabePuku* mOwner;
	/* 0x6C */ JGeometry::TVec3<f32> mVel;
	/* 0x78 */ f32 mMouthYOffset;
	/* 0x7C */ f32 mMouthRadius;
	/* 0x80 */ f32 mGroundY;
	/* 0x84 */ const TBGCheckData* mGroundPlane;
	/* 0x88 */ u8 mAirborne;
	/* 0x89 */ u8 mTouchedWall;
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
	void setMomentumFromQuat();
	void calcYawFromVelocity();
	bool isTouchedPlane() const { return mTouchedWall != 0 ? true : false; }
	TTabePukuParams* getParams() const
	{
		return (TTabePukuParams*)getSaveParam();
	}
	u32 getMouthIndex() const { return mMouthIndex; }

public:
	/* 0x194 */ TTPHitActor* mHit;
	/* 0x198 */ JGeometry::TQuat4<f32> mQuat;
	/* 0x1A8 */ JGeometry::TRotation3<JGeometry::TMatrix34<
	    JGeometry::SMatrix34C<f32> > > mTakingMtx;
	/* 0x1D8 */ u32 mMouthIndex;
	/* 0x1DC */ u8 mTouchedWall;
	/* 0x1E0 */ f32 mDiveStartY;
	/* 0x1E4 */ JGeometry::TVec3<f32> mDragVec;
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
