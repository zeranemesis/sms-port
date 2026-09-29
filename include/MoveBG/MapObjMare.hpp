#ifndef MOVE_BG_MAP_OBJ_MARE_HPP
#define MOVE_BG_MAP_OBJ_MARE_HPP

#include <MoveBG/MapObjBase.hpp>

// TODO: mark virtual methods as such

struct TBGWallCheckRecord;
class TCannon;
class TCogwheel;

class TCogwheelScale : public TMapObjBase {
public:
	/// Total size 0x15C (from `li r3, 0x15c` in TMapObjManager's factory).
	u32 touchWater(THitActor*);
	BOOL receiveMessage(THitActor* sender, u32 message);
	void touchPlayer(THitActor*);
	void control();
	TCogwheelScale(const char*);

public:
	/* 0x138 */ f32 mRotSpeed;
	/* 0x13C */ f32 mRotPos;
	/* 0x140 */ f32 mAccel;
	/* 0x144 */ f32 mLimit;
	/* 0x148 */ f32 mWaterLeakPos;
	/* 0x14C */ f32 mWaterLeakValue;
	/* 0x150 */ f32 mWaterLeakMul;
	/* 0x154 */ u8 mCogwheelScaleIsTop;
	/* 0x155 */ u8 padding155[3];
	/* 0x158 */ TCogwheel* mCogwheel;

	// fabricated
	static f32 mWaterLeakSpeed;
};

class TCogwheel : public TMapObjBase {
public:
	/// Total size 0x178 (from `li r3, 0x178` in TMarNameRefGen_MapObj).
	/* 0x138 */ f32 mSpeed;
	/* 0x13C */ f32 mAngle;
	/* 0x140 */ f32 mAcceleration;
	/* 0x144 */ f32 mFriction;
	/* 0x148 */ f32 mReverseRate;
	/* 0x14C */ f32 mRopeLength;
	/* 0x150 */ TCogwheelScale* mPlate;
	/* 0x154 */ JGeometry::TVec3<f32> mPlatePos;
	/* 0x160 */ f32 mAngleLimitLow;
	/* 0x164 */ TCogwheelScale* mPot;
	/* 0x168 */ JGeometry::TVec3<f32> mPotPos;
	/* 0x174 */ f32 mAngleLimitHigh;

public:
	/// UNUSED 0x34 in mario.MAP; the body is the direction clamp shared with
	/// TCogwheelScale::touchPlayer.
	void rebound();
	void calc();
	void control();
	void initMapObj();
	void draw() const;
	void initDraw() const;
	TCogwheel(const char* name = "天秤");

	// fabricated names for the three .sdata floats and one sdata
	static f32 mRopeWidthX;
	static f32 mRopeWidthZ;
	static f32 mTexPosRate;
	static f32 mMinSpeed;
};

class TMapObjElasticCode : public TMapObjBase {
public:
	/// Total size 0x144.
	/* 0x138 */ f32 mSpeed;
	/* 0x13C */ f32 mSpringConst;
	/* 0x140 */ f32 mFriction;

public:
	void draw() const;
	void control();
	void initMapObj();
	TMapObjElasticCode(const char* name = "ゴムひも")
	    : TMapObjBase(name)
	{
	}
};

class TMapObjGrowTree : public TMapObjBase {
public:
	/// Total size 0x14C.
	/* 0x138 */ f32 mGrowHeight;
	/* 0x13C */ f32 mGrowSpeed;
	/* 0x140 */ f32 mGrowRate;
	/* 0x144 */ int mAppearTime;
	/* 0x148 */ f32 mMinGrowHeight;

public:
	void initMapObj();
	void loadAfter();
	void control();
	u32 touchWater(THitActor*);
	/// UNUSED 0x88 in mario.MAP.
	void getGrowHeightFromRate(float) const;
	/// UNUSED 0xCC in mario.MAP.
	void updateHeight();
	TMapObjGrowTree(const char* name = "もやしの木");
};

class TWireBell : public TMapObjBase {
public:
	/// Total size 0x158.
	/* 0x138 */ int mWireNo;
	/* 0x13C */ f32 mLength;
	/* 0x140 */ f32 mLimitRotY;
	/* 0x144 */ f32 mLimitRotX;
	/* 0x148 */ f32 mTexPosRate;
	/// Wire-space anchor; control() reads .z back out as the drop distance.
	/* 0x14C */ JGeometry::TVec3<f32> mPosOnWire;

public:
	void initDraw() const;
	void draw() const;
	void control();
	void loadAfter();
	TWireBell(const char* name = "ワイヤー鈴（紫）");
};

class TMapObjPuncher : public TMapObjBase {
public:
	/// Total size 0x13C.
	/* 0x138 */ f32 mThrowPower;

public:
	void touchPlayer(THitActor*);
	void control();
	void load(JSUMemoryInputStream&);
	TMapObjPuncher(const char* name = "パンチャー")
	    : TMapObjBase(name)
	{
	}
};

class TMuddyBoat : public TMapObjBase {
public:
	/// Total size 0x188.
	/* 0x138 */ f32 mAccelPos;
	/* 0x13C */ f32 mAccelNeg;
	/* 0x140 */ f32 mSpeed;
	/* 0x144 */ f32 mSpeedFriction;
	/* 0x148 */ f32 mWaterLeakRate;
	/* 0x14C */ f32 mWaterLeakValue;
	/* 0x150 */ f32 mWaterLeakMul;
	/* 0x154 */ f32 mWallHeight;
	/* 0x158 */ f32 mWallDepthA;
	/* 0x15C */ f32 mWallDepthB;
	/* 0x160 */ f32 mWallDepthC;
	/* 0x164 */ f32 mWallWidth;
	/* 0x168 */ int mAppearTime;
	/* 0x16C */ int mCount;
	/* 0x170 */ JGeometry::TVec3<f32> mTargetPos;
	/* 0x17C */ JGeometry::TVec3<f32> mScale;

public:
	void initMapObj();
	void kill();
	void moveByWater();
	void control();
	void bind();
	void calc();
	void calcRootMatrix();
	u32 getSDLModelFlag() const;
	/// UNUSED 0xA8 in mario.MAP.
	void touchWall(JGeometry::TVec3<f32>*, const TBGWallCheckRecord&);
	/// UNUSED 0x104 in mario.MAP.
	void bindToWall(const JGeometry::TVec3<f32>&, f32,
	                JGeometry::TVec3<f32>*);
	TMuddyBoat(const char* name = "どろの船");
};

class TMareFall : public TMapObjBase {
public:
	/// Total size 0x138: no members of its own.
	void calc();
	void load(JSUMemoryInputStream&);
	TMareFall(const char* name = "マーレ滝")
	    : TMapObjBase(name)
	{
	}
};

class TMareCork : public TMapObjBase {
public:
	/// Total size 0x158.
	/* 0x138 */ TCannon* mCannon;
	/* 0x13C */ JGeometry::TVec3<f32> mVel;
	/* 0x148 */ JGeometry::TVec3<f32> mShinePos;
	/* 0x154 */ u8 mIsMoving;
	/* 0x155 */ u8 padding155[3];

public:
	void moveObject();
	void calcRootMatrix();
	void loadAfter();
	MtxPtr getTakingMtx();
	void drawObject(JDrama::TGraphics*);
	TMareCork(const char* name = "マーレコルク")
	    : TMapObjBase(name)
	{
	}
};

class TMareEventPoint : public THitActor {
public:
	/// Total size 0x6C.
	/* 0x68 */ void* mMareEventDepressWall;

public:
	BOOL receiveMessage(THitActor* sender, u32 message);
	void load(JSUMemoryInputStream&);
	TMareEventPoint(const char* name = "イベントポイント")
	    : THitActor(name)
	    , mMareEventDepressWall(nullptr)
	{
	}
};

#endif
