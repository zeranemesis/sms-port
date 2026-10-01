#ifndef MOVE_BG_MAP_OBJ_FLAG_HPP
#define MOVE_BG_MAP_OBJ_FLAG_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <Strategic/HitActor.hpp>

struct ResTIMG;

class TMapObjFlag : public THitActor {
public:
	TMapObjFlag(const char*);
	virtual ~TMapObjFlag() { }

	void load(JSUMemoryInputStream&);
	void init(const char*);
	virtual void updateVertex();
	void draw();

	// TODO: never called from anywhere in the retail binary; the linker map
	// lists it as UNUSED (0x114 bytes), so its body is not recoverable.
	void update();

	static f32 mFlutterSpeed;

public:
	/* 0x68 */ f32 unk68;
	/* 0x6C */ f32 unk6C;
	/* 0x70 */ u32 unk70;
	/* 0x74 */ u32 unk74;
	/* 0x78 */ JGeometry::TVec3<f32>** unk78;
	/* 0x7C */ f32 unk7C;
	/* 0x80 */ f32 unk80;
	/* 0x84 */ f32 unk84;
	/* 0x88 */ f32 unk88;
	/* 0x8C */ Mtx mMtx;
	/* 0xBC */ u32 unkBC;
};

// TODO: the linker map also lists TMapObjFlagLower / TMapObjFlagSail with
// their own updateVertex() and destructors, all of them UNUSED. Nothing in the
// emitted code references them, so no reconstruction is possible.
class TMapObjFlagManager : public JDrama::TViewObj {
public:
	class TMapObjFlagInfo {
	public:
		TMapObjFlagInfo()
		    : unk0(0)
		    , unk54(0)
		{
		}

	public:
		/* 0x00 */ int unk0;
		/* 0x04 */ TMapObjFlag* unk4[20];
		/* 0x54 */ ResTIMG* unk54;
	};

	TMapObjFlagManager(const char* name = "旗管理");
	virtual ~TMapObjFlagManager();

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void initDraw();
	void registerObj(TMapObjFlag*, const char*);

	// TODO: UNUSED in the map (0x80 bytes). It is the body that every branch
	// of registerObj() repeats; reconstructing it out of line is only
	// worthwhile if MWCC refuses to inline it everywhere.
	void loadFlag(TMapObjFlagInfo*, TMapObjFlag*, const char*);

public:
	/* 0x10 */ TMapObjFlagInfo unk10[15];
};

extern TMapObjFlagManager* gpMapObjFlagManager;

#endif
