#ifndef MOVE_BG_MAP_OBJ_MARE_HPP
#define MOVE_BG_MAP_OBJ_MARE_HPP

#include <MoveBG/MapObjBase.hpp>

// TODO: mark virtual methods as such

struct TBGWallCheckRecord;

class TCogwheelScale : public TMapObjBase {
public:
	u32 touchWater(THitActor*);
	BOOL receiveMessage(THitActor* sender, u32 message);
	void touchPlayer(THitActor*);
	void control();
	TCogwheelScale(const char*);

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ f32 unk140;
	/* 0x144 */ f32 unk144;
	/* 0x148 */ f32 unk148;
	/* 0x14C */ f32 unk14C;
	/* 0x150 */ f32 unk150;
	/* 0x154 */ u8 unk154;
	/* 0x155 */ u8 padding155[3];
	/* 0x158 */ void* unk158;
};

class TCogwheel : public TMapObjBase {
public:
	void initDraw() const;
	void draw() const;
	void rebound();
	void calc();
	void control();
	void initMapObj();
	TCogwheel(const char* name = "天秤");
};

class TMapObjElasticCode : public TMapObjBase {
public:
	void draw() const;
	void control();
	void initMapObj();
	TMapObjElasticCode(const char* name = "ゴムひも");
};

class TMapObjGrowTree : public TMapObjBase {
public:
	void getGrowHeightFromRate(float) const;
	void updateHeight();
	u32 touchWater(THitActor*);
	void control();
	void loadAfter();
	void initMapObj();
	TMapObjGrowTree(const char* name = "もやしの木");
};

class TWireBell : public TMapObjBase {
public:
	void initDraw() const;
	void draw() const;
	void control();
	void loadAfter();
	TWireBell(const char* name = "ワイヤー鈴（紫）");
};

class TMapObjPuncher : public TMapObjBase {
public:
	void touchPlayer(THitActor*);
	void control();
	void load(JSUMemoryInputStream&);
	TMapObjPuncher(const char* name = "パンチャー");
};

class TMuddyBoat : public TMapObjBase {
public:
	void moveByWater();
	void calcRootMatrix();
	void kill();
	void touchWall(JGeometry::TVec3<float>*, const TBGWallCheckRecord&);
	void bindToWall(const JGeometry::TVec3<float>&, float,
	                JGeometry::TVec3<float>*);
	void bind();
	void control();
	void calc();
	u32 getSDLModelFlag() const;
	void initMapObj();
	TMuddyBoat(const char* name = "どろの船");
};

class TMareFall : public TMapObjBase {
public:
	void calc();
	void load(JSUMemoryInputStream&);
	TMareFall(const char* name = "マーレ滝");
};

class TMareCork : public TMapObjBase {
public:
	void loadAfter();
	void moveObject();
	void calcRootMatrix();
	MtxPtr getTakingMtx();
	void drawObject(JDrama::TGraphics*);
	TMareCork(const char* name = "マーレコルク");
};

class TMareEventPoint : public THitActor {
public:
	BOOL receiveMessage(THitActor* sender, u32 message);
	void load(JSUMemoryInputStream&);
	TMareEventPoint(const char* name = "イベントポイント");
};

#endif
