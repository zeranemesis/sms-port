#ifndef MOVE_BG_MAP_OBJ_PINNA_HPP
#define MOVE_BG_MAP_OBJ_PINNA_HPP

#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjGeneral.hpp>
#include <MoveBG/MapObjTown.hpp>

// TODO: mark virtual methods as such

class TCoin;

class TFerrisWheel : public TMapObjBase {
public:
	u32 becomeCalmlyCallback(u32, u32);
	virtual void control();
	virtual void initMapObj();
	TFerrisWheel(const char* name = "観覧車");

public:
	/* 0x138 */ int unk138;
	/* 0x13C */ TMapObjBase** unk13C;
	/* 0x140 */ f32 unk140;
};

class THorizontalViking : public TMapObjBase {
public:
	void updateTrans();
	void moveNormal();
	virtual void control();
	virtual void reset();
	virtual void initMapObj();
	THorizontalViking(const char* name)
	    : TMapObjBase(name)
	{
		unk138.set(0.0f, 0.0f, 0.0f);
		unk144 = 0.0f;
		unk148 = 0.0f;
	}

public:
	/* 0x138 */ JGeometry::TVec3<f32> unk138;
	/* 0x144 */ f32 unk144;
	/* 0x148 */ f32 unk148;
};

class TViking : public THorizontalViking {
public:
	void roll();
	virtual void control();
	virtual void reset();
	virtual void loadAfter();
	virtual void initMapObj();
	TViking(const char* name = "バイキング");

public:
	/* 0x14C */ s32 unk14C;
	/* 0x150 */ JGeometry::TVec3<f32> unk150;
};

class TPinnaShell : public THitActor {
public:
	void opened();
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	void control();
	TPinnaShell(const char*);
	TPinnaShell()
	    : THitActor("ピアホ")
	{
		unk68 = 0;
		unk6C = 0.0f;
		unk70 = 0.0f;
		unk74 = 0;
		unk78 = 0;
		unk7C = 0;
		unk80 = 0;
		unk84 = 0;
		unk88 = 0;
		unk8C = 0;
		initHitActor(0x4000013A, 1, 0x80000000, 250.0f, 400.0f, 250.0f,
		             200.0f);
	}

public:
	/* 0x68 */ s32 unk68;
	/* 0x6C */ f32 unk6C;
	/* 0x70 */ f32 unk70;
	// The ROM does `lwz r3, 0x74(r31)` and immediately calls the
	// TMapObjBase member concatOnlyRotFromRight, so unk74 is a TMapObjBase*
	// rather than a bare MtxPtr. 0x8C is loaded as a float source only.
	/* 0x74 */ TMapObjBase* unk74;
	/* 0x78 */ u32 unk78;
	/* 0x7C */ u32 unk7C;
	/* 0x80 */ TLiveActor* unk80;
	/* 0x84 */ TMapObjBase* unk84;
	/* 0x88 */ TLiveActor* unk88;
	/* 0x8C */ MtxPtr unk8C;
	/* 0x48 */ u16 unk48;
};

class TShellCup : public TMapObjBase {
public:
	virtual void control();
	void attachCoin(TCoin*, int);
	void calcAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void loadAfter();
	virtual void initMapObj();
	TShellCup(const char* name = "シェルカップ");

	static f32 mWaterOpenAccel;
	static f32 mOpenRotMax;
	static f32 mCloseAccel;
	static f32 mShellDamageRot;

public:
	/* 0x138 */ TPinnaShell unk138[6];
	/* 0x498 */ TLiveActor* unk498;
	/* 0x49C */ TLiveActor* unk49C;
	/* 0x4A0 */ TLiveActor* unk4A0;
};

class TMerrygoround : public TMapObjBase {
public:
	virtual void control();
	virtual void draw() const;
	virtual void initMapObj();
	TMerrygoround(const char* name = "メリーゴーランド");

	static f32 mRotSpeed;

public:
	/* 0x138 */ u32 unk138;
	/* 0x13C */ u32 unk13C;
	/* 0x140 */ u16 unk140;
	/* 0x142 */ u16 unk142;
	/* 0x144 */ u32 unk144[9];
	/* 0x168 */ u32 unk168[9];
	/* 0x18C */ u16 unk18C[9];
	/* 0x1A0 */ u32 unk1A0;
	/* 0x1A4 */ u16 unk1A4;
};

class TChangeStageMerrygoround : public TMapObjChangeStage {
public:
	void touchPlayer(THitActor*);
	void calc();

	TChangeStageMerrygoround()
	    : TMapObjChangeStage("ステージ切り替え（メリーゴーランド用）")
	    , unk13C(0)
	{
	}

public:
	/* 0x13C */ u8 unk13C;
};

class TBalloonKoopaJr : public TMapObjGeneral {
public:
	void touchActor(THitActor*);
	void kill();
	void load(JSUMemoryInputStream&);
	TBalloonKoopaJr(const char* name = "風船（クッパＪｒ）")
	    : TMapObjGeneral(name)
	{
	}

public:
	/* 0x148 */ JGeometry::TVec3<f32> unk148;
};

class TPinnaEntrance : public TMapObjBase {
public:
	void loadAfter();
	TPinnaEntrance(const char* name = "ピンナ入り口")
	    : TMapObjBase(name)
	{
	}
};

class TWaterRecoverObj : public TMapObjBase {
public:
	void touchPlayer(THitActor*);
	TWaterRecoverObj(const char* name = "水回復オブジェ")
	    : TMapObjBase(name)
	{
	}
};

class TAmiKing : public TMapObjBase {
public:
	u32 touchWater(THitActor*) { return 1; }
	void loadAfter();
	void initMapObj();
	void calc();
	void moveObject();
	void calcRootMatrix();
	void bind();
	void touchPlayer(THitActor*);
	TAmiKing(const char* name = "アミキング")
	    : TMapObjBase(name)
	{
	}

public:
	/* 0x138 */ u8 unk138;
	/* 0x13C */ JGeometry::TVec3<f32> unk13C;
	/* 0x148 */ u32 unk148;
};

class TPinnaCoaster : public TMapObjBase {
public:
	virtual void control();
	virtual void initMapObj();
	TPinnaCoaster(const char* name = "コースター");

public:
	/* 0x138 */ MActor* unk138;
	/* 0x13C */ u32 unk13C; // padding / unknown
	/* 0x140 */ f32 unk140;
	/* 0x144 */ f32 unk144;
	/* 0x148 */ f32 unk148;
};

class TMerryPole : public TMapObjBase {
public:
	virtual Mtx* getRootJointMtx() const { return (Mtx*)unk138.mMtx; }

	TMerryPole()
	    : TMapObjBase("メリーゴーランド用ポール")
	{
		unk138.identity();
	}

public:
	/* 0x138 */ TMtx34f unk138;
};

#endif
