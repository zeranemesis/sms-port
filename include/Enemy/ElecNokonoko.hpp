#ifndef ENEMY_ELEC_NOKONOKO_HPP
#define ENEMY_ELEC_NOKONOKO_HPP

#include <Enemy/WalkerEnemy.hpp>
#include <Enemy/EnemyAttachment.hpp>

class J3DMaterialTable;
class TElecNokonoko;

class TElecNokonokoSaveLoadParams : public TWalkerEnemyParams {
public:
	TElecNokonokoSaveLoadParams(const char* path);

	/* 0x32C */ TParamRT<s32> mSLReadyTime;
	/* 0x340 */ TParamRT<f32> mSLCarapaceGravity;
	/* 0x354 */ TParamRT<f32> mSLCarapaceSpeed;
	/* 0x368 */ TParamRT<f32> mSLCarapaceTurnSpeed;
	/* 0x37C */ TParamRT<f32> mSLCarapaceSpinSpeed;
	/* 0x390 */ TParamRT<f32> mSLCarapaceShootRange;
	/* 0x3A4 */ TParamRT<f32> mSLCarapaceFlyDist;
};

// The electric shell an elec nokonoko shoots at mario and then collects back.
class TElecCarapace : public TEnemyAttachment {
public:
	TElecCarapace(const char* name);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual void kill();
	virtual f32 getPhaseShift() const { return unk174 ? 0.0f : 180.0f; }
	virtual void loadInit(TSpineEnemy*, const char*);
	virtual void appear();
	virtual void rebirth() { }
	virtual void sendMessage();
	virtual void behaveToHitGround();
	virtual void behaveToHitWall(const TBGCheckData*);
	virtual void setBehavior();
	virtual void recoverScale() { }
	virtual f32 getNowGravity();
	virtual void shoot();

	bool isMove();
	void move();
	void reflect(THitActor*);
	void setZigParameter();

public:
	/* 0x16C */ TElecNokonoko* mOwner;
	/* 0x170 */ THitActor* unk170; // last actor we got reflected by
	/* 0x174 */ bool unk174;
	/* 0x175 */ u8 unk175;
	/* 0x176 */ u8 unk176;
	/* 0x178 */ f32 unk178; // zigzag cycle
	/* 0x17C */ f32 unk17C; // zigzag angle
	/* 0x180 */ int unk180; // wall reflection cooldown
	/* 0x184 */ u8 unk184;
	/* 0x188 */ f32 unk188; // spin angle
	/* 0x18C */ JGeometry::TVec3<f32> unk18C; // return velocity
	/* 0x198 */ f32 unk198;
};

class TElecNokonoko : public TWalkerEnemy {
public:
	TElecNokonoko(const char* name = "電気ノコノコ");

	virtual void load(JSUMemoryInputStream& stream);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager* manager);
	virtual void calcRootMatrix();
	virtual void moveObject();
	virtual const char** getBasNameTable() const;
	virtual void genRandomItem();
	virtual void behaveToWater(THitActor*);
	virtual void setWalkAnm();
	virtual void setDeadAnm();
	virtual void setMeltAnm();
	virtual void setWaitAnm();
	virtual void setRunAnm();
	virtual void attackToMario();
	virtual void setMActorAndKeeper();
	virtual void sendAttackMsgToMario();
	virtual void behaveToFindMario();
	virtual bool isResignationAttack();
	virtual void rest();

	void recoverCarapace();
	bool isDeadByThunder();
	void forceCatchReady();
	bool isCatchReady();
	bool isShootReady();
	void shootIn();
	void catchIn();

	static bool mReflectSw;
	static u8 mCarapaceJntIndex;

public:
	/* 0x194 */ TElecCarapace* mCarapace;
	/* 0x198 */ int unk198;
	/* 0x19C */ int unk19C;
	/* 0x1A0 */ TElecNokonokoSaveLoadParams* mParams;
	/* 0x1A4 */ int unk1A4; // 0 = shell is on the back, 1 = shell was shot
	/* 0x1A8 */ JGeometry::TVec3<f32> unk1A8;
};

class TElecNokonokoManager : public TSmallEnemyManager {
public:
	TElecNokonokoManager(const char* name = "電気ノコノコマネージャー");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
	virtual void clipEnemies(JDrama::TGraphics* graphics);
	virtual void initSetEnemies();

public:
	/* 0x60 */ J3DMaterialTable* unk60;
};

void createNokonokoThunder(JGeometry::TVec3<f32>);

DECLARE_NERVE(TNerveElecCarapaceReturn, TLiveActor);
DECLARE_NERVE(TNerveElecCarapaceWait, TLiveActor);
DECLARE_NERVE(TNerveElecCarapaceMove, TLiveActor);
DECLARE_NERVE(TNerveElecNokonokoAttack, TLiveActor);
DECLARE_NERVE(TNerveElecNokonokoRebirth, TLiveActor);
DECLARE_NERVE(TNerveElecNokonokoFreeze, TLiveActor);
DECLARE_NERVE(TNerveElecNokonokoTurn, TLiveActor);
DECLARE_NERVE(TNerveElecNokonokoCollect, TLiveActor);
DECLARE_NERVE(TNerveElecNokonokoShoot, TLiveActor);

#endif
