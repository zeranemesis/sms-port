#ifndef MOVE_BG_MAP_OBJ_MAMMA_HPP
#define MOVE_BG_MAP_OBJ_MAMMA_HPP

#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjEx.hpp>
#include <Strategic/MirrorActor.hpp>

class J3DJoint;
class MActor;
class TMapCollisionMove;
class TSleepBossHanachan;
class TShiningStone;
class TSandBomb;
class TSandBase;
class TMapObjFlag;
class JPABaseEmitter;

class TSandLeaf : public TMapObjBase {
public:
	u32 touchWater(THitActor*);
	void control();
	TSandLeaf(const char* name = "すなやまの芽")
	    : TMapObjBase(name)
	    , unk138(nullptr)
	{
	}

public:
	/* 0x138 */ TSandBase* unk138; // the sand base that spawned this leaf
};

class TSandBase : public TMapObjBase {
public:
	void isDown() const;
	virtual void grow() = 0;
	virtual bool withering();
	TSandBase(const char*);
	static s32 mWitherTime;
	static f32 mScaleMin;

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ s32 unk140;
	/* 0x144 */ TSandBomb* unk144;
};

class TSandLeafBase : public TSandBase {
public:
	virtual void grow();
	virtual void control();
	virtual void initMapObj();
	TSandLeafBase(const char* name = "すなやまの芽の土台")
	    : TSandBase(name)
	{
	}
};

class TSandBomb : public TSandLeaf {
public:
	u32 touchWater(THitActor*);
	u32 getSDLModelFlag() const;
	void initMapObj();
	void makeObjAppeared();

	TSandBomb()
	    : TSandLeaf("すなやま爆弾")
	    , unk13C(0)
	    , unk140(0)
	{
	}

public:
	/* 0x13C */ u32 unk13C;
	/* 0x140 */ u8 unk140;
};

class TSandBombBase : public TSandBase {
public:
	virtual void loadAfter();
	virtual void control();
	virtual void initMapObj();
	virtual void grow();
	virtual void waitBeforeExplode();
	virtual void explode();
	virtual void exploding();
	virtual void expanded();
	virtual void withered();
	virtual TMapObjBase* findTriggerActor();
	TSandBombBase(const char* name = "すなやま爆弾の土台");

	static f32 mFiringFrameSpeed;
	static f32 mFiringFrameDownSpeed;
	static f32 mExplodeFrameSpeed;
	static f32 mMarioJumpRate;
	static f32 mExlodingRumbleTime;

public:
	/* 0x148 */ s32 unk148;
	/* 0x14C */ f32 unk14C;
	/* 0x150 */ f32 unk150;
	/* 0x154 */ f32 unk154;
};

class TSandCastle : public TSandBombBase {
public:
	enum {
		STATE_SHRINKING = 2,
	};

	bool withering();
	void expanded();
	void explode();
	void waitBeforeExplode();
	void calcRootMatrix();
	TMapObjBase* findTriggerActor();
	void loadAfter();
	void initMapObj();
	TSandCastle(const char* name = "砂の城");

	static f32 mCollisionRate;

public:
	/* 0x158 */ TMapObjBase* unk158;
	/* 0x15C */ u8 unk15C;
};

class TLeanMirror : public TMapObjBase {
public:
	enum {
		STATE_IDLE    = 0,
		STATE_SHAKE   = 1,
		STATE_GO      = 2,
		STATE_WAIT    = 3,
		STATE_FINISH  = 4,
		STATE_STANDBY = 5,
	};

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void control();
	virtual u32 getSDLModelFlag() const;
	virtual void initMapObj();
	virtual void draw() const;
	virtual void touchPlayer(THitActor*);
	virtual void touchEnemy(THitActor*);

	void enemyIsOn() const;
	void updateSpeedVec(const JGeometry::TVec3<f32>&, f32);
	void calcCurrentMtx(MtxPtr);
	void release();
	void controlGoTarget();
	void controlShake();

	TLeanMirror(const char* name = "ぐらぐら鏡");

	static s32 mGoTargetTime;
	static s32 mDemoWaitTime;
	static s32 mDemoLightTime;

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ JGeometry::TVec3<f32> unk140;
	/* 0x14C */ JGeometry::TVec3<f32> unk14C;
	/* 0x158 */ f32 unk158;
	/* 0x15C */ f32 unk15C;
	/* 0x160 */ f32 unk160;
	/* 0x164 */ f32 unk164;
	/* 0x168 */ f32 unk168;
	/* 0x16C */ f32 unk16C;
	/* 0x170 */ f32 unk170;
	/* 0x174 */ f32 unk174;
	/* 0x178 */ f32 unk178;
	/* 0x17C */ TShiningStone* unk17C;
	/* 0x180 */ JGeometry::TVec3<f32> unk180;
	/* 0x18C */ JGeometry::TVec3<f32> unk18C;
	/* 0x198 */ f32 unk198;
	/* 0x19C */ s32 unk19C;
	/* 0x1A0 */ JGeometry::TVec3<f32> unk1A0;
	/* 0x1AC */ u8 unk1AC;
	/* 0x1AE */ u16 unk1AE;
};

class TShiningStone : public THitActor {
public:
	void endDemo();
	void putOnLight(TLiveActor*);
	void perform(u32 cue, JDrama::TGraphics* graphics);
	void load(JSUMemoryInputStream&);
	TShiningStone(const char* name = "太陽石");

public:
	/* 0x68 */ MActor** mMirror;
	/* 0x6C */ MActor* mStone;
	/* 0x70 */ u8 mGreen;
	/* 0x71 */ u8 mBlue;
	/* 0x72 */ u8 mRed;
	/* 0x73 */ u8 mWhite;
	/* 0x74 */ s32 mLightCount;
	/* 0x78 */ JPABaseEmitter* mEmitter;
	/* 0x7C */ f32 unk7C;
};

class TJointModel;
class TJointObj;

class TMammaBlockRotate : public TMapObjBase {
public:
	enum {
		STATE_ROTATING = 1,
		STATE_GO       = 2,
		STATE_WAIT     = 3,
		STATE_BACK     = 4,
	};

	u32 touchWater(THitActor*);
	void control();
	void initMapObj();
	void load(JSUMemoryInputStream&);
	TMammaBlockRotate(const char* name = "太陽の塔ブロック");

	static f32 mRotSpeed;
	static f32 mRotReturnSpeed;
	static f32 mRotEnd;
	static f32 mMapGoSpeed;
	static f32 mMapBackSpeed;
	static s32 mWaitTime;

public:
	/* 0x138 */ TJointModel* unk138;
	/* 0x13C */ TJointObj* unk13C;
	/* 0x140 */ TJointObj* unk140;
	/* 0x144 */ TMapCollisionMove* unk144;
	/* 0x148 */ TMapCollisionMove* unk148;
};

class TMammaYacht : public TMapObjBase {
public:
	void control();
	void initMapObj();
	TMammaYacht(const char* name = "砂の城")
	    : TMapObjBase(name)
	{
	}

public:
	/* 0x138 */ TMapObjFlag* unk138;
};

class TSandBird : public TJointCoin {
public:
	virtual void control();
	virtual void initMapObj();
	virtual TMapObjBase* makeObjFromJointName(const char*, unsigned short);
	virtual bool nameIsObj(const char*);

	TSandBird(const char* name = "おおすな鳥");

public:
	/* 0x148 */ u32 unk148;
	/* 0x14C */ u32 unk14C;
	/* 0x150 */ u8 unk150;
	/* 0x151 */ u8 unk151;
};

class TGoalWatermelon : public TMapObjBase {
public:
	enum {
		STATE_HIDDEN = 1,
	};

	~TGoalWatermelon() { }
	void touchActor(THitActor*);
	void control();
	void loadAfter();
	void load(JSUMemoryInputStream&);
	TGoalWatermelon(const char* name = "スイカゴール");

public:
	/* 0x138 */ TLiveActor* unk138;
	/* 0x13C */ TMapObjBase* unk13C;
	/* 0x140 */ f32 unk140;
	/* 0x144 */ f32 unk144;
	/* 0x148 */ f32 unk148;
};

class TWatermelonStatic : public TMapObjBase {
public:
	// left inline on purpose: defining it out-of-line makes CodeWarrior emit
	// the dtor strong and reorders it, which costs more than the vtable
	// relocation it would fix
	~TWatermelonStatic() { }
	u32 touchWater(THitActor*);
	void control();

	TWatermelonStatic()
	    : TMapObjBase("固定スイカ")
	{
	}
};

class TMammaMirrorMapOperator : public JDrama::TViewObj {
public:
	void show(int);
	void hide(int);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void loadAfter();
	TMammaMirrorMapOperator(const char* name = "鏡内地形");

public:
	/* 0x10 */ void* unk10[8];
	/* 0x30 */ JGeometry::TVec3<f32> unk30[8];
	/* 0x90 */ f32 unk90[8];
	/* 0xB0 */ u8 unkB0[8];
	/* 0xB8 */ JGeometry::TVec3<f32> unkB8[3];
};

class TSandEgg : public TMapObjBase {
public:
	u32 getSDLModelFlag() const;
	TSandEgg(const char* name = "すなのたまご")
	    : TMapObjBase(name)
	{
	}
};

#endif
