#ifndef ANIMAL_BEEHIVE_HPP
#define ANIMAL_BEEHIVE_HPP

#include <Animal/fishoid.hpp>
#include <Enemy/EnemyManager.hpp>

// ============= manager =============

class TBeeHive;

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
	TBee(MActor* actor);
	virtual ~TBee();

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init();
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
	void getCenterOfGravity() const;
	void appearBee(int);
	void doWait();
	void controlSound();
	void controlCollision();
	void receiveMessageFromChild(TBee*);

public:
	// TODO: fields between 0x158 and 0x1A0, 0x1AC and past 0x1B8 are unknown
	/* 0x158 */ char unk158[0x1A0 - 0x158];
	/* 0x1A0 */ JGeometry::TVec3<f32> mSoundPos;
	/* 0x1AC */ char unk1AC[4];
	/* 0x1B0 */ int mCollisionIdx;
	/* 0x1B4 */ int mBeeNum;
};

// ============= nerves =============

DECLARE_NERVE(TNerveBeeHiveReset, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveWait, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveMarioWaterIn, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveAttack, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveBreak, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveFall, TLiveActor);

#endif
