#ifndef ANIMAL_BEEHIVE_HPP
#define ANIMAL_BEEHIVE_HPP

#include <Animal/fishoid.hpp>
#include <Enemy/EnemyManager.hpp>

class TMapObjBase;
class TBeeHiveParams;
class TBeeHive;

// ============= manager =============

class TBeeHiveManager : public TEnemyManager {
public:
	TBeeHiveManager(const char* name = "ハチの巣マネージャー");
	virtual ~TBeeHiveManager();

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
};

// ============= bee =============

class TBee : public TRealoidActor {
public:
	// TODO: the second argument was recovered from the UNUSED symbol
	// __ct__4TBeeFP6MActorP8TBeeHive in marioEU.MAP; the ctor is always
	// inlined so it never appears as a real symbol.
	TBee(MActor* actor, TBeeHive* owner);
	virtual ~TBee();

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init();

	/* 0xA8 */ TBeeHive* mOwner;
};

// ============= hive =============

class TBeeHive : public TRealoid {
public:
	TBeeHive(const char* name = "ハチの巣");
	virtual ~TBeeHive();

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void control();
	virtual void bind();
	virtual void reset();
	virtual TRealoidActor* createRealoidActor(MActor*);

	// fabricated
	// NOTE: the return type is not part of the mangled name, which is why the
	// map only shows "...CFv" even though the caller passes a return slot.
	JGeometry::TVec3<f32> getCenterOfGravity() const;
	void appearBee(int);
	BOOL doWait();
	void controlSound();
	void controlCollision();
	void receiveMessageFromChild(TBee*);

	TBee* getBee(int idx) { return (TBee*)getRealoid(idx); }
	TBeeHiveParams* getParams() const
	{
		return (TBeeHiveParams*)getSaveParam();
	}

public:
	/* 0x158 */ JGeometry::TQuat4<f32> mQuat;
	/* 0x168 */ JGeometry::TQuat4<f32> mTargetQuat;
	// 0x178: a real quaternion, not a TVec4 -- calcRootMatrix() copy-constructs
	// a TQuat4 out of it in one 16-byte move, and reset() fills it through
	// TRotation3::getQuat().
	JGeometry::TQuat4<f32> mVec178;
	/* 0x188 */ JGeometry::TVec3<f32> mVec188;
	/* 0x194 */ JGeometry::TVec3<f32> mHomePos;
	/* 0x1A0 */ JGeometry::TVec3<f32> mSoundPos;
	/* 0x1AC */ TMapObjBase* unk1AC;
	/* 0x1B0 */ int mCollisionIdx;
	/* 0x1B4 */ int mBeeNum;
	/* 0x1B8 */ TMapObjBase** unk1B8; // assigned the coin array in load()
	/* 0x1BC */ int unk1BC; // counts the coins that fell out
};

// ============= nerves =============

DECLARE_NERVE(TNerveBeeHiveReset, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveWait, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveMarioWaterIn, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveAttack, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveBreak, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveFall, TLiveActor);

#endif
