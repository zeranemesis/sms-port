#ifndef ENEMY_HAUNT_LEG_HPP
#define ENEMY_HAUNT_LEG_HPP

#include <Enemy/WalkerEnemy.hpp>
#include <Strategic/HitActor.hpp>

class THauntLeg;

// Hit actor riding along with the leg, registered in the enemy group so that
// other objects can collide with the leg.
class THauntedObject : public THitActor {
public:
	THauntedObject(const char* name)
	    : THitActor(name)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);

	void kill();
	void checkHit();

public:
	/* 0x68 */ THauntLeg* unk68;
};

class THauntLegManager : public TSmallEnemyManager {
public:
	THauntLegManager(const char* name);

	virtual void load(JSUMemoryInputStream& stream);
	virtual void createModelData();
	virtual TSmallEnemy* createEnemyInstance();
	virtual void initSetEnemies();
};

class THauntLeg : public TWalkerEnemy {
public:
	THauntLeg(const char* name = "ハントレッグ");

	virtual MtxPtr getTakingMtx();
	virtual void init(TLiveManager* manager);
	virtual void calcRootMatrix();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void setGenerateAnm();
	virtual void setWalkAnm();
	virtual void setDeadAnm();
	virtual void setWaitAnm();
	virtual void setRunAnm();
	virtual void attackToMario();
	virtual void setMActorAndKeeper();
	virtual bool isCollidMove(THitActor*);

	bool isUseCallBack();

public:
	/* 0x194 */ THauntedObject* unk194;
	/* 0x198 */ u8 unk198;
	/* 0x199 */ u8 unk199;
	/* 0x19C */ TTakeActor* unk19C;
	/* 0x1A0 */ JGeometry::TVec3<f32> unk1A0;
	/* 0x1AC */ f32 unk1AC;
};

DECLARE_NERVE(TNerveHauntLegHaunt, TLiveActor);

#endif
