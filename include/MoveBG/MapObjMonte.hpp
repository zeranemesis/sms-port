#ifndef MOVE_BG_MAP_OBJ_MONTE_HPP
#define MOVE_BG_MAP_OBJ_MONTE_HPP

#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjBlock.hpp>

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
	/* 0x194 */ u32 unk194;
	/* 0x198 */ u32 unk198;
	/* 0x19C */ u32 unk19C;
	/* 0x1A0 */ u32 unk1A0;
	/* 0x1A4 */ JGeometry::TVec3<f32> unk1A4[2];
	/* 0x1BC */ TMapObjBase* unk1BC;
};

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
	/* 0x18 */ u8 unk18[0x20];
	/* 0x38 */ u32 unk38;
	/* 0x3C */ f32 unk3C;
	/* 0x40 */ f32 unk40;
	/* 0x44 */ f32 unk44;

	static f32 mRopeWidthBetweenBoards;
	static f32 mRopeWidthBetweenBoardsY;
	static int mPointNumBetweenBoards;
	static f32 mBetweenBoardsTexPosRate;
	static f32 mRopeHeight;
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
	/* 0x14C */ f32 unk14C;
	/* 0x150 */ f32 unk150;
	/* 0x154 */ f32 unk154;
	/* 0x158 */ f32 unk158;
	/* 0x15C */ f32 unk15C;
	/* 0x160 */ f32 unk160;
	/* 0x164 */ f32 unk164;
	/* 0x168 */ f32 unk168;
	/* 0x16C */ f32 unk16C;
	/* 0x170 */ f32 unk170;
	/* 0x174 */ f32 unk174;
	/* 0x178 */ f32 unk178;
	/* 0x17C */ f32 unk17C;
	/* 0x180 */ f32 unk180;
	/* 0x184 */ f32 unk184;
	/* 0x188 */ u32 unk188;
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
	/* 0x148 */ f32 unk148;
	/* 0x14C */ f32 unk14C;
	/* 0x150 */ f32 unk150;
	/* 0x154 */ f32 unk154;
	/* 0x158 */ u32 unk158;
	/* 0x15C */ u32 unk15C;
	/* 0x160 */ u32 unk160;
	/* 0x164 */ u32 unk164;
};

#endif
