#ifndef MOVE_BG_MAP_OBJ_MONTE_HPP
#define MOVE_BG_MAP_OBJ_MONTE_HPP

#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjBlock.hpp>

class JAISound;

// TODO: mark virtual methods as such

class TMapObjMonteRoot : public TMapObjBase {
public:
	void initMapObj();
	TMapObjMonteRoot(const char* name = "根っこ")
	    : TMapObjBase(name)
	{
	}
};

class TJumpMushroom : public TMapObjBase {
public:
	BOOL receiveMessage(THitActor*, u32);
	void load(JSUMemoryInputStream&);
	TJumpMushroom(const char* name = "ジャンプきのこ")
	    : TMapObjBase(name)
	{
	}
};

class THangingBridgeBoard;

// unk38 is a table of per-vertex vertical offsets, indexed by the loop counter
// of drawUpper()/drawLowerMinus().
class THangingBridge : public JDrama::TViewObj {
public:
	void drawLowerMinus(const JGeometry::TVec3<f32>&,
	                    const JGeometry::TVec3<f32>&,
	                    const JGeometry::TVec2<f32>&, int) const;
	void drawLowerPlus(const JGeometry::TVec3<f32>&,
	                   const JGeometry::TVec3<f32>&,
	                   const JGeometry::TVec2<f32>&, int) const;
	void drawUpper(const JGeometry::TVec3<f32>&, const JGeometry::TVec3<f32>&,
	               const JGeometry::TVec2<f32>&, int) const;
	void setDrawPos(int, f32, JGeometry::TVec3<f32>*) const;
	void drawRopeBetweenBoards(f32, int) const;
	void initDraw() const;
	void perform(u32, JDrama::TGraphics*);
	void initMonte();
	void loadAfter();
	THangingBridge(const char* name = "つり橋");

public:
	/* 0x10 */ u32 unk10;
	/* 0x14 */ THangingBridgeBoard** unk14;
	// 0x18 and 0x24 are the rope attachment points of the first and the last
	// board; loadAfter() interpolates the board positions between them.
	/* 0x18 */ JGeometry::TVec3<f32> unk18;
	/* 0x24 */ JGeometry::TVec3<f32> unk24;
	// (unk24 - unk18).x/.z, normalised and rotated by a quarter turn, then
	// multiplied by unk3C to get the rope's cross-section half-extent.
	/* 0x30 */ f32 unk30;
	/* 0x34 */ f32 unk34;
	/* 0x38 */ const f32* unk38;
	/* 0x3C */ f32 unk3C;
	/* 0x40 */ f32 unk40;
	/* 0x44 */ f32 unk44;

	static f32 mRopeWidthBetweenBoards;
	static f32 mRopeWidthBetweenBoardsY;
	static int mPointNumBetweenBoards;
	static f32 mBetweenBoardsTexPosRate;
	static f32 mRopeHeight;
};

// The four neighbour pointers at 0x194..0x1A0 and the owner at 0x1BC are all
// dereferenced in control(); they are pointers, not opaque words.
class THangingBridgeBoard : public TLeanBlock {
public:
	void drawOneRope(const JGeometry::TVec3<f32>&) const;
	void drawRopes() const;
	void push(f32);
	void pushNeighbor(f32);
	void control();
	void calcDefaultMtx();
	void setGroundCollision();
	void initMapObj();
	THangingBridgeBoard(const char*);

public:
	/* 0x194 */ THangingBridgeBoard* unk194;
	/* 0x198 */ THangingBridgeBoard* unk198;
	/* 0x19C */ THangingBridgeBoard* unk19C;
	/* 0x1A0 */ THangingBridgeBoard* unk1A0;
	// Two rope attachment points, written by control() from the board's own
	// rotation and read by drawRopeBetweenBoards() as the two columns of the
	// rope's strands.
	/* 0x1A4 */ JGeometry::TVec3<f32> unk1A4[2];
	/* 0x1BC */ THangingBridge* unk1BC;

	static f32 mMarioAccelY;
	static f32 mMarioHipDropAccelY;
	static f32 mReturnAccelRate;
	static f32 mSpeedDownRate;
	static f32 mRopeWidthX;
	static f32 mRopeWidthZ;
	static f32 mTexPosRate;
};

class TSwingBoard : public TMapObjBase {
public:
	void drawOneRope(const JGeometry::TVec3<f32>&,
	                 const JGeometry::TVec3<f32>&) const;
	void initDraw() const;
	void draw() const;
	void swing();
	void control();
	void load(JSUMemoryInputStream&);
	TSwingBoard(const char* name = "つり橋");

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ f32 unk140;
	/* 0x144 */ f32 unk144;
	/* 0x148 */ f32 unk148;
	// 0x14C..0x17B is one 48-byte matrix: load() fills it with a RotY built
	// from MsSin/MsCos, control() feeds it to PSMTXConcat.
	/* 0x14C */ Mtx mMatrix;
	/* 0x17C */ JGeometry::TVec3<f32> unk17C;
	/* 0x188 */ JAISound* unk188;

	static f32 mBoardWidth;
	static f32 mRopeWidthX;
	static f32 mRopeWidthZ;
	static f32 mTexPosRate;
	static f32 mReturnAccelRate;
	static f32 mSpeedDownRate;
};

class TGoalFlag : public TMapObjBase {
public:
	f32 getRadiusAtY(f32) const { return 20.0f; }
	void touchActor(THitActor*);
	void initMapObj();
	TGoalFlag(const char* name = "ゴールフラグ")
	    : TMapObjBase(name)
	{
	}
};

class TFluffManager;

class TFluff : public TMapObjBase {
public:
	f32 getRadiusAtY(f32) const { return 20.0f; }
	u32 touchWater(THitActor*);
	void move();
	void kill();
	void control();
	void appear();
	void initMapObj();
	TFluff(const char*);

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ f32 unk140;
	/* 0x144 */ f32 unk144;
	/* 0x148 */ f32 unk148;
	/* 0x14C */ f32 unk14C;
	/* 0x150 */ f32 unk150;
	/* 0x154 */ f32 unk154;
	/* 0x158 */ f32 unk158;
	/* 0x15C */ f32 unk15C;
	/* 0x160 */ f32 unk160;
	/* 0x164 */ f32 unk164;
	/* 0x168 */ TFluffManager* unk168;
	/* 0x16C */ u8 unk16C;

	static f32 mScaleUpSpeed;
	static f32 mScaleDownSpeed;
};

class TFluffManager : public TMapObjBase {
public:
	void findNextFluff();
	void control();
	void registerNextFluff(TFluff*);
	void setUpNextFluff();
	void newFluff(const char*);
	f32 getRandomX() const;
	f32 getRandomZ() const;
	void loadAfter();
	void load(JSUMemoryInputStream&);
	TFluffManager(const char* name = "特別な綿毛");

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ f32 unk140;
	/* 0x144 */ u32 unk144;
	// 0x148..0x153 is a direction vector: load() transforms (0,0,1) by a
	// RPH matrix and scales it, control() re-reads it.
	/* 0x148 */ JGeometry::TVec3<f32> unk148;
	/* 0x154 */ f32 unk154;
	/* 0x158 */ TFluff* unk158;
	/* 0x15C */ TFluff* unk15C;
	/* 0x160 */ u32 unk160;
	/* 0x164 */ u32 unk164;
	/* 0x168 */ TFluff** unk168;

	static f32 mWindMin;
};

#endif
