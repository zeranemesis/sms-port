#ifndef ENEMY_CANNON_HPP
#define ENEMY_CANNON_HPP

#include <Strategic/Nerve.hpp>
#include <Enemy/SmallEnemy.hpp>
#include <Strategic/SharedParts.hpp>

class TCannon;
class TBombHei;
class MAnmSound;
class TMapCollisionMove;
class TMapCollisionBase;

class TCannonSaveLoadParams : public TSmallEnemyParams {
public:
	TCannonSaveLoadParams(const char* path);

	/* 0x2D4 */ TParamRT<f32> mSLHideDist;
	/* 0x2E8 */ TParamRT<f32> mSLBombDist;
	/* 0x2FC */ TParamRT<f32> mSLKillerDist;
	/* 0x310 */ TParamRT<s32> mSLBombInterval;
	/* 0x324 */ TParamRT<s32> mSLKillerInterval;
	/* 0x338 */ TParamRT<s32> mSLShootInterval;
	/* 0x34C */ TParamRT<f32> mSLChorobeiAttackRadius;
	/* 0x360 */ TParamRT<f32> mSLChorobeiAttackHeight;
	/* 0x374 */ TParamRT<f32> mSLChorobeiDamageRadius;
	/* 0x388 */ TParamRT<f32> mSLChorobeiDamageHeight;
	/* 0x39C */ TParamRT<f32> mSLKillerTransYOffset;
	/* 0x3B0 */ TParamRT<f32> mSLBombHeiGenerateRate;
	/* 0x3C4 */ TParamRT<f32> mSLThrowXZSpeed;
};

class TCannonManager : public TSmallEnemyManager {
public:
	TCannonManager(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual TSmallEnemy* createEnemyInstance();
	virtual void clipEnemies(JDrama::TGraphics*) { }
};

// The Chorobei (Monty Mole-like gunner) sitting inside the cannon.
class TChorobei : public THitActor {
public:
	TChorobei(TCannon* cannon, int jointIndex, const char* name);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);

	void setBckAnm(int);
	void checkHit();
	bool isUpEnd();
	bool isDownEnd();
	MActor* getMActor() const { return mParts->getMActor(); }

public:
	/* 0x68 */ TCannon* mCannon;
	/* 0x6C */ TSharedParts* mParts;
	/* 0x70 */ f32 unk70;
	/* 0x74 */ MAnmSound* mAnmSound;
	/* 0x78 */ const char* unk78;
	/* 0x7C */ f32 unk7C;
};

class TCannonDom : public TSharedParts {
public:
	TCannonDom(TLiveActor* owner, int jointIndex, SDLModelData* modelData,
	           u32 flags, const char* name);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void setBckAnm(int);

public:
	/* 0x1C */ MAnmSound* mAnmSound;
	/* 0x20 */ const char* unk20;
	/* 0x24 */ u8 unk24;
	/* 0x28 */ f32 unk28;
	/* 0x2C */ f32 unk2C;
	/* 0x30 */ f32 unk30;
};

class TCannon : public TSmallEnemy {
public:
	TCannon(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual MtxPtr getTakingMtx();
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void moveObject();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual bool isCollidMove(THitActor*) { return false; }
	virtual BOOL isInhibitedForceMove() { return TRUE; }
	virtual bool isHitVallid(u32) { return false; }

	void calcObjCollision();
	void entryObjCollision();
	void bombSet();
	void bombShoot();
	void bombScaleUp();
	void hitHead(TBombHei*);
	void updateAttachPos();
	void killerShoot();
	void endKillerShoot();
	void damage();
	void setKillerGoalPoint();
	void deadCannon();
	void startDemo();
	void startMarioDemo();
	void turnToGoal();
	bool isObject();
	void killShootAct();
	void gateOpen();
	void startChorobeiShout();

	TChorobei* getChorobei() const { return unk1A8; }
	u8 getShootKind() const { return unk290; }
	TSpineEnemy* getMareGate() const { return unk254; }
	TCannonSaveLoadParams* getSLParams() const { return unk28C; }

	// fabricated
	TCannonSaveLoadParams* getParams() const
	{
		return (TCannonSaveLoadParams*)getSaveParam();
	}

	static u8 mChorobeiJntIdx;
	static u8 mChorobeiHandJntIdx;
	static f32 mVelocityRate;
	static f32 mSearchRate;

public:
	// TODO: most fields are only known by offset (ctor/load/reset stores)
	/* 0x194 */ JGeometry::TVec3<f32> unk194;
	/* 0x1A0 */ TSmallEnemy* unk1A0; // bomb held by the Chorobei
	/* 0x1A4 */ TLiveActor* unk1A4;
	/* 0x1A8 */ TChorobei* unk1A8;
	/* 0x1AC */ TCannonDom* unk1AC[3];
	/* 0x1B8 */ TCannonDom* unk1B8;
	/* 0x1BC */ TSharedParts* unk1BC;
	/* 0x1C0 */ TMapCollisionMove* unk1C0[3];
	/* 0x1CC */ u8 unk1CC[0x1E0 - 0x1CC];
	/* 0x1E0 */ MtxPtr unk1E0;
	/* 0x1E4 */ TPosition3f unk1E4;
	/* 0x214 */ s32 unk214;
	/* 0x218 */ s32 unk218;
	/* 0x21C */ u8 unk21C;
	/* 0x220 */ f32 unk220;
	/* 0x224 */ JGeometry::TVec3<f32> unk224;
	/* 0x230 */ u8 unk230; // area id
	/* 0x234 */ f32 unk234;
	/* 0x238 */ u8 unk238;
	/* 0x239 */ u8 unk239;
	/* 0x23C */ JGeometry::TVec3<f32> unk23C; // position at load
	/* 0x248 */ JGeometry::TVec3<f32> unk248;
	/* 0x254 */ TSpineEnemy* unk254; // mare gate effect
	/* 0x258 */ TMapCollisionMove* unk258;
	/* 0x25C */ JGeometry::TVec3<f32> unk25C[4];
	/* 0x28C */ TCannonSaveLoadParams* unk28C;
	/* 0x290 */ u8 unk290;
	/* 0x294 */ JGeometry::TVec3<f32> unk294;
	/* 0x2A0 */ JGeometry::TVec3<f32> unk2A0;
	/* 0x2AC */ f32 unk2AC;
	/* 0x2B0 */ TMapCollisionMove* unk2B0;
};

DECLARE_NERVE(TNerveCannonObject, TLiveActor);
DECLARE_NERVE(TNerveCannonDamageDemo, TLiveActor);
DECLARE_NERVE(TNerveCannonDamage, TLiveActor);
DECLARE_NERVE(TNerveCannonClose, TLiveActor);
DECLARE_NERVE(TNerveCannonForceBombShoot, TLiveActor);
DECLARE_NERVE(TNerveCannonShoot, TLiveActor);
DECLARE_NERVE(TNerveCannonSearch, TLiveActor);
DECLARE_NERVE(TNerveCannonOpen, TLiveActor);

#endif
