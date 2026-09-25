#ifndef MOVE_BG_MAP_OBJ_MAMMA_HPP
#define MOVE_BG_MAP_OBJ_MAMMA_HPP

#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjEx.hpp>

// TODO: mark virtual methods as such

class TSandLeaf : public TMapObjBase {
public:
	u32 touchWater(THitActor*);
	void control();
	TSandLeaf(const char* name = "すなやまの芽")
	    : TMapObjBase(name)
	    , unk138(0)
	{
	}

public:
	/* 0x138 */ u32 unk138;
};

class TSandBase : public TMapObjBase {
public:
	void isDown() const;
	virtual bool withering();
	TSandBase(const char*);
	static f32 mScaleMin;

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ u32 unk140;
	/* 0x144 */ TMapObjBase* unk144;
};

class TSandLeafBase : public TSandBase {
public:
	void grow();
	void control();
	void initMapObj();
	TSandLeafBase(const char* name = "すなやまの芽の土台")
	    : TSandBase(name)
	{
	}
};

class TSandBomb : public TSandLeaf {
public:
	void makeObjAppeared();
	u32 touchWater(THitActor*);
	u32 getSDLModelFlag() const;
	void initMapObj();

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
	void withered();
	void expanded();
	void exploding();
	void explode();
	void waitBeforeExplode();
	void grow();
	void control();
	void findTriggerActor();
	void loadAfter();
	void initMapObj();
	TSandBombBase(const char* name = "すなやま爆弾の土台");

public:
	/* 0x148 */ u32 unk148;
	/* 0x14C */ f32 unk14C;
	/* 0x150 */ f32 unk150;
	/* 0x154 */ f32 unk154;
};

class TSandCastle : public TSandBombBase {
public:
	bool withering();
	void expanded();
	void explode();
	void waitBeforeExplode();
	void calcRootMatrix();
	void findTriggerActor();
	void loadAfter();
	void initMapObj();
	TSandCastle(const char* name = "砂の城");
};

class TLeanMirror : public TMapObjBase {
public:
	void enemyIsOn() const;
	void draw() const;
	void updateSpeedVec(const JGeometry::TVec3<f32>&, f32);
	BOOL receiveMessage(THitActor* sender, u32 message);
	void touchPlayer(THitActor*);
	void touchEnemy(THitActor*);
	void calcCurrentMtx(MtxPtr);
	void release();
	void controlGoTarget();
	void controlShake();
	void control();
	void loadAfter();
	u32 getSDLModelFlag() const;
	void initMapObj();
	void load(JSUMemoryInputStream&);
	TLeanMirror(const char* name = "ぐらぐら鏡");
};

class TShiningStone : public THitActor {
public:
	void endDemo();
	void putOnLight(TLiveActor*);
	void perform(u32 cue, JDrama::TGraphics* graphics);
	void load(JSUMemoryInputStream&);
	TShiningStone(const char* name = "太陽石")
	    : THitActor(name)
	{
	}
};

class TMammaBlockRotate : public TMapObjBase {
public:
	u32 touchWater(THitActor*);
	void control();
	void initMapObj();
	void load(JSUMemoryInputStream&);
	TMammaBlockRotate(const char* name = "太陽の塔ブロック");
};

class TMammaYacht : public TMapObjBase {
public:
	void control();
	void initMapObj();
	TMammaYacht(const char* name = "砂の城")
	    : TMapObjBase(name)
	{
	}
};

class TSandBird : public TJointCoin {
public:
	~TSandBird();
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
	~TGoalWatermelon();
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

class TMammaMirrorMapOperator : public JDrama::TViewObj {
public:
	void show(int);
	void hide(int);
	void perform(u32 cue, JDrama::TGraphics* graphics);
	void loadAfter();
	TMammaMirrorMapOperator(const char* name = "鏡内地形操作");
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
