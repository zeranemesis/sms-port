#ifndef MOVE_BG_MAP_OBJ_CORONA_HPP
#define MOVE_BG_MAP_OBJ_CORONA_HPP

#include <MoveBG/MapObjBase.hpp>
#include <Map/BathWaterManager.hpp>

class MActorAnmData;
class TBathtubGrip;
class TBathtubParams;
class MActor;

class TBathtub : public TMapObjBase {
public:
	TBathtub(const char* name = "バスタブ");
	// Out of line on purpose: the map records __dt__8TBathtubFv as a *global*
	// (not a weak header inline), as the very first symbol of the TU, and it
	// is this out-of-line virtual that makes MWCC emit __vt__8TBathtub here.
	virtual ~TBathtub();

	void loadAfter();
	void hipdrop(const JGeometry::TVec3<f32>&);
	void quake(const JGeometry::TVec3<f32>&);
	int getNumGripsDead() const;
	void tumble(f32, f32);
	MtxPtr getTakingMtx();
	MtxPtr getSubmarineMtxInDemo();
	MtxPtr getPeachMtxInDemo();
	MtxPtr getKoopaJrMtxInDemo();
	BOOL receiveMessage(THitActor* sender, u32 message);
	Mtx* getRootJointMtx() const;
	void perform(u32 cue, JDrama::TGraphics* graphics);
	void control();
	void calcBathtubData();
	void setupCollisions_();
	void startDemo();
	bool allowsTumble() const;
	void calcRootMatrix();
	bool getNearGrip(const JGeometry::TVec3<f32>&, f32, f32*) const;
	u8 getNextJuncture(const JGeometry::TVec3<f32>&,
	                   const JGeometry::TVec3<f32>&) const;
	u8 getNextGrip(const JGeometry::TVec3<f32>&, const JGeometry::TVec3<f32>&,
	               f32, f32*) const;
	void updatePosture_();
	void load(JSUMemoryInputStream&);
	u8 getNumKillerLaunchable() const;
	bool isKillerAttackable() const;
	u8 getNumKillerBurstable() const;
	bool isBreaking() const;                                // Unused
	bool isKillerLaunchable() const;                        // Unused
	void showMessage(u32);                                  // Unused
	u8 getNearJuncture(const JGeometry::TVec3<f32>&) const; // Unused
	MtxPtr getKoopaMtxInDemo();                             // Unused
	// The map's mangled name is getWaterMtx__8TBathtubFi, so the parameter was
	// spelled `int` in the original (this project's s32 is `signed long`).
	MtxPtr getWaterMtx(int);                                // Unused
	MtxPtr getShineEffectMtx();                             // Unused
	MtxPtr getShineMtx();                                   // Unused
	void liftMario(const JGeometry::TVec3<f32>&);           // Unused
	void trample(const JGeometry::TVec3<f32>&);             // Unused

	const TBathtubData& getBathtubData() const { return mBathtubData; }

public:
	/* 0x138 */ MActorAnmData* unk138;
	/* 0x13C */ f32 unk13C[5];
	/* 0x150 */ f32 unk150[5];
	/* 0x164 */ TMapCollisionMove** unk164;
	/* 0x168 */ TBathtubGrip** unk168;
	/* 0x16C */ TBathtubParams* unk16C;
	/* 0x170 */ TBathtubData mBathtubData;
	/* 0x1D8 */ f32 unk1D8;
	/* 0x1DC */ f32 unk1DC;
	/* 0x1E0 */ f32 unk1E0;
	/* 0x1E4 */ f32 unk1E4;
	/* 0x1E8 */ f32 unk1E8;
	/* 0x1EC */ f32 unk1EC;
	/* 0x1F0 */ f32 unk1F0;
	/* 0x1F4 */ JGeometry::TVec3<f32> unk1F4;
	/* 0x200 */ JGeometry::TVec3<f32> unk200;
	/* 0x20C */ u8 unk20C[0x30];
	/* 0x23C */ f32 unk23C;
	/* 0x240 */ f32 unk240;
	/* 0x244 */ f32 unk244;
	/* 0x248 */ int unk248;
	/* 0x24C */ int unk24C;
	/* 0x250 */ int unk250;
	/* 0x254 */ u32 unk254;
	/* 0x258 */ u32 unk258;
	/* 0x25C */ u32 unk25C;
	/* 0x260 */ int mMarioJntIdx;
	/* 0x264 */ int mStarJntIdx;
	/* 0x268 */ int mShineBodyJntIdx;
	/* 0x26C */ int mSubmarineJntIdx;
	/* 0x270 */ int mDuckJntIdx;
	/* 0x274 */ int mJuniorJntIdx;
	/* 0x278 */ int mKoopaJntIdx;
	/* 0x27C */ int mWater4JntIdx;
	/* 0x280 */ int mWater5JntIdx;
	/* 0x284 */ int mWater1JntIdx;
	/* 0x288 */ int mWater2JntIdx;
	/* 0x28C */ int mWater3JntIdx;
	/* 0x290 */ int unk290;
	/* 0x294 */ int unk294;
	/* 0x298 */ u8 unk298;
	/* 0x299 */ u8 unk299;
	/* 0x29A */ u8 unk29A;
	/* 0x29C */ MActor* unk29C;
	/* 0x2A0 */ u32 unk2A0;
};

// Gameplay tunables read from /MapObj/bathtub.prm; recovered from the
// TBaseParam name table and the vtable/default pairs of the map's ctor.
class TBathtubParams : public TParams {
public:
	TBathtubParams();

	/* 0x08 */ TParamRT<u8> resetGrip;
	/* 0x1C */ TParamRT<s32> trampleRelease;
	/* 0x30 */ TParamRT<s32> trampleRecover;
	/* 0x44 */ TParamRT<s32> quakeRelease;
	/* 0x58 */ TParamRT<s32> quakeRecover;
	/* 0x6C */ TParamRT<s32> hipdropRelease;
	/* 0x80 */ TParamRT<s32> hipdropRecover;
	/* 0x94 */ TParamRT<s32> breakCount0;
	/* 0xA8 */ TParamRT<s32> breakCount1;
	/* 0xBC */ TParamRT<s32> breakCount2;
	/* 0xD0 */ TParamRT<s32> breakCount3;
	/* 0xE4 */ TParamRT<s32> launchStopCount;
	/* 0xF8 */ TParamRT<f32> animSpeed0;
	/* 0x10C */ TParamRT<f32> animSpeed1;
	/* 0x120 */ TParamRT<f32> animSpeed2;
	/* 0x134 */ TParamRT<f32> animSpeed3;
	/* 0x148 */ TParamRT<f32> animSpeed4;
	/* 0x15C */ TParamRT<f32> shake;
	/* 0x170 */ TParamRT<f32> watermark;
	/* 0x184 */ TParamRT<f32> maxAngle;
	/* 0x198 */ TParamRT<f32> angleVelDamp;
	/* 0x1AC */ TParamRT<f32> rebound;
	/* 0x1C0 */ TParamRT<f32> shakeDamp;
	/* 0x1D4 */ TParamRT<f32> marioWeight;
	/* 0x1E8 */ TParamRT<f32> marioDropWeight;
	/* 0x1FC */ TParamRT<f32> outerHeight;
};

class TBathtubGripParts;

// One of the foot-holds of the bathtub: its own TMapObjBase-derived actor
// carrying the model and the per-part collision data.
class TBathtubGrip : public TMapObjBase {
public:
	TBathtubGrip(TBathtub*, f32, MActorAnmData*, const char* name);

	virtual ~TBathtubGrip() { }
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual Mtx* getRootJointMtx() const;
	virtual void calcRootMatrix();
	virtual void control();
	virtual void kill();

	// Unused
	void setupCollisions_();
	bool marioIsOn() const;
	void startBreak(f32, int, f32);
	void startCrack();
	bool isCracking() const;
	void reset();

public:
	/* 0x138 */ JGeometry::TVec3<f32> unk138[2];
	/* 0x150 */ TMapCollisionMove* unk150[5];
	/* 0x164 */ TMapCollisionMove* unk164[17];
	/* 0x1A8 */ TBathtubGripParts* unk1A8[5];
	/* 0x1BC */ TBathtubGripParts* unk1BC[17];
	/* 0x200 */ s32 unk200[17];
	/* 0x244 */ TBathtub* mBathtub;
	/* 0x248 */ u8 unk248;
	/* 0x249 */ u8 unk249;
	/* 0x24A */ u8 unk24A;
	/* 0x24B */ u8 unk24B;
	/* 0x24C */ f32 unk24C;
	/* 0x250 */ f32 unk250;
	/* 0x254 */ s32 unk254;
	/* 0x258 */ s32 unk258;
	/* 0x25C */ MActor* mStandMActor;
	/* 0x260 */ u8 unk260;
};

// Base of the per-part hitbox actors that hang off a TBathtubGrip.
// The map mangles the ctors as ...FPCciP12TBathtubGrip / ...FiP12TBathtubGrip,
// so both take only (index, grip) -- the "kind" is baked into the name string.
class TBathtubGripParts : public TLiveActor {
public:
	TBathtubGripParts(const char* name, int, TBathtubGrip*);
	// Defined out of line in MapObjCorona.cpp. The map records the symbol
	// itself as *weak* (i.e. a header inline) but also emits the base class'
	// vtable here, and only an out-of-line definition makes MWCC emit it --
	// an inline body drops __vt__17TBathtubGripParts and the @32@ thunk.
	virtual ~TBathtubGripParts();

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual Mtx* getRootJointMtx() const;

public:
	/* 0xF4 */ TBathtubGrip* mGrip;
	/* 0xF8 */ s32 unkF8;
};

// The indestructible variant of TBathtubGripParts.
class TBathtubGripPartsHard : public TBathtubGripParts {
public:
	TBathtubGripPartsHard(int, TBathtubGrip*);
	virtual ~TBathtubGripPartsHard() { }

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
};

// The breakable variant of TBathtubGripParts.
class TBathtubGripPartsFragile : public TBathtubGripParts {
public:
	TBathtubGripPartsFragile(int, TBathtubGrip*);
	virtual ~TBathtubGripPartsFragile() { }

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
};

#endif
