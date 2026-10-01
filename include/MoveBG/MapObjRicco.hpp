#ifndef MOVE_BG_MAP_OBJ_RICCO_HPP
#define MOVE_BG_MAP_OBJ_RICCO_HPP

#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjBlock.hpp>
#include <MoveBG/Item.hpp>
#include <dolphin/gx/GXStruct.h>

class JAISound;
class TFruitLauncher;

// TODO: mark virtual methods as such

class TCraneRotY : public TMapObjBase {
public:
	void calc();
	void control();
	void load(JSUMemoryInputStream&);
	TCraneRotY(const char* name = "Ｙ軸回転クレーン")
	    : TMapObjBase(name)
	    , unk138(0.0f)
	    , unk13C(0.0f)
	    , unk140(0.0f)
	    , unk144(0.0f)
	    , unk148(0)
	{
	}

public:
	enum {
		STATE_ROTATE_UP   = 0x0,
		STATE_WAIT_UP     = 0x1,
		STATE_ROTATE_DOWN = 0x2,
		STATE_WAIT_DOWN   = 0x3,
	};

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ f32 unk140;
	/* 0x144 */ f32 unk144;
	/* 0x148 */ u32 unk148;

	static u32 mWaitTime;
};

class TCraneUpDown : public TMapObjBase {
public:
	~TCraneUpDown();
	void control();
	void initMapObj();
	TCraneUpDown(const char* name = "上下クレーン")
	    : TMapObjBase(name)
	    , unk138(0)
	    , unk13C(0)
	{
	}

public:
	enum {
		STATE_ROTATE_UP   = 0x0,
		STATE_WAIT_UP     = 0x1,
		STATE_ROTATE_DOWN = 0x2,
		STATE_WAIT_DOWN   = 0x3,
	};

public:
	/* 0x138 */ TMapObjBase* unk138;
	/* 0x13C */ u32 unk13C;
	/* 0x140 */ f32 unk140;
	/* 0x144 */ f32 unk144;

	static f32 mRotSpeed;
	static u32 mWaitTime;
};

class TCraneCargo : public TLeanBlock {
public:
	void control();
	void calc();
	TCraneCargo()
	    : TLeanBlock("クレーン積み荷")
	{
	}
};

class TRiccoWatermill : public TMapObjBase {
public:
	u32 touchWater(THitActor*);
	void control();
	void calc();
	void loadAfter();
	TRiccoWatermill(const char* name = "リコ水車");

public:
	// TODO: the meaning of each state is inferred from control()/touchWater();
	// nothing in the binary names them. 0/1 and 5 run the "mill spins, partner
	// rises" block, 2/3/4 skip it and run the switch below instead.
	enum {
		STATE_ROTATE   = 0x0,
		STATE_RISE     = 0x1,
		STATE_TOP_WAIT = 0x2,
		STATE_TOP      = 0x3,
		STATE_BOTTOM   = 0x4,
		STATE_STOPPED  = 0x5,
	};

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ TMapObjBase* mPartner;
	/* 0x140 */ s32 unk140;
	/* 0x144 */ u8 unk144;
	/* 0x148 */ TMapObjBase* unk148;
	/* 0x14C */ JAISound* unk14C;
	/* 0x150 */ JAISound* unk150;
	/* 0x154 */ JAISound* unk154;

	static f32 mRotAccel;
	static f32 mRotSpeedMaxUp;
	static f32 mRotSpeedMaxDown;
	static f32 mRotDown;
	static f32 mSubmarineMoveRate;
	static f32 mSubmarineMaxTransY;
	static f32 mSubmarineBottomTransY;
	static u32 mWaitTime;
	static f32 mSubmarineSurfaceTransY;
};

class TSurfGesoObj : public TItem {
public:
	~TSurfGesoObj();
	void initMapObj();
	TSurfGesoObj(const char* name = "イカサーフィン")
	    : TItem(name)
	{
	}

public:
	/* 0x154 */ GXColorS10 mTevColor;
};

class TFruitSwitch : public TMapObjBase {
public:
	void pullUp();
	void pushDown();
	BOOL receiveMessage(THitActor* sender, u32 message);
	TFruitSwitch(const char* name = "フルーツスイッチ")
	    : TMapObjBase(name)
	{
	}

public:
	/* 0x138 */ TFruitLauncher* mLauncher;
};

class TFruitLauncher : public TMapObjBase {
public:
	~TFruitLauncher();
	void appearFruit() const;
	void fireObj();
	void loadAfter();
	TFruitLauncher(const char* name = "フルーツ発射口")
	    : TMapObjBase(name)
	{
	}

	// TODO: unk138/unk13C are the two tank-switch map objects found by name in
	// loadAfter(); their real type is unknown, they are only used to
	// back-reference the launcher and to receive the switch animation.
	// fireObj() indexes this pair with unk140 (slwi r0, r0, 2 / add r3, r31, r0 /
	// lwz r30, 0x138(r3)), so it is really a two-element array and the two
	// separate fields below are its elements.
	/* 0x138 */ TMapObjBase* unk138[2];
	/* 0x140 */ u32 unk140;

	static f32 mObjSpeedXZ;
	static f32 mObjSpeedY;
	static u32 mFruitLiveTime;
};

#endif
