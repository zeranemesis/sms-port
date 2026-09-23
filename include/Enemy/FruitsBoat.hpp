#ifndef ENEMY_FRUITS_BOAT_HPP
#define ENEMY_FRUITS_BOAT_HPP

#include <Enemy/EnemyManager.hpp>
#include <Enemy/Enemy.hpp>
#include <Strategic/Nerve.hpp>

class J3DAnmTransform;
class J3DFrameCtrl;

// The fruit boats floating around Delfino Plaza (ShipDolpic*.bmd).
// Field layout reconstructed from the ctor stores and the uses in
// build/GMSP01/asm/Enemy/fruitsboat.s.

class TFruitsBoatParams : public TSpineEnemyParams {
public:
	TFruitsBoatParams(const char* path);

	/* 0xA8 */ TParamRT<f32> mSLMoveSpeed;
	/* 0xBC */ TParamRT<f32> mSLRotSpeed;
	/* 0xD0 */ TParamRT<f32> mSLBckMoveSpeed;
};

class TFruitsBoatManager : public TEnemyManager {
public:
	TFruitsBoatManager(int boat_type, const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();

public:
	// Selects the model/collision set: 0 = ShipDolpic, 1 = ShipDolpic2,
	// 2 = ShipDolpic3, anything else = ShipDolpic4 (see MarNameRefGen).
	/* 0x54 */ int mBoatType;
};

class TFruitsBoat : public TSpineEnemy {
public:
	TFruitsBoat(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual Mtx* getRootJointMtx() const;
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void setGroundCollision();
	virtual void moveObject();
	virtual void requestShadow();

	int setBckTrack(const char*);
	void setJumpReaction();
	void traceBckTrack();
	int getBoatType() const;
	void rowToCurPathNode(f32);

	TFruitsBoatParams* getSaveLoadParam() const
	{
		return (TFruitsBoatParams*)getSaveParam();
	}

public:
	/* 0x150 */ s16 unk150; // toggled by rail node flag 0x400
	/* 0x154 */ f32 mShadowRadiusX;
	/* 0x158 */ f32 mShadowRadiusZ;
	/* 0x15C */ J3DAnmTransform* mBckTrack;
	/* 0x160 */ J3DFrameCtrl* mBckTrackCtrl;
	/* 0x164 */ JGeometry::TVec3<f32> mRollAxis;
	/* 0x170 */ f32 mRollAngle;
	/* 0x174 */ f32 mRollSpeed;
};

DECLARE_NERVE(TNerveFruitsBoatGraphWander, TLiveActor);
DECLARE_NERVE(TNerveFruitsBoatBckTrace, TLiveActor);

#endif
