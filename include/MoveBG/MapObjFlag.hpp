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
	void updateVertex();
	void draw();

	static f32 mFlutterSpeed;

public:
	// TODO: exact layout unverified past this point; offsets derived from
	// disassembly of init()/updateVertex() only (0x68..0xbc), fields
	// preceding 0x68 (0x28,0x2c) not yet placed relative to THitActor base.
	/* 0x68 */ f32 unk68;
	/* 0x6C */ f32 unk6C;
	/* 0x70 */ u32 unk70;
	/* 0x74 */ u32 unk74;
	/* 0x78 */ u32 unk78;
	/* 0x7C */ f32 unk7C;
	/* 0x80 */ f32 unk80;
	/* 0x84 */ f32 unk84;
	/* 0x88 */ f32 unk88;
	/* 0x8C */ f32 unk8C;
	/* 0x90 */ f32 unk90;
	/* 0x94 */ f32 unk94;
	/* 0x98 */ f32 unk98;
	/* 0x9C */ f32 unk9C;
	/* 0xA0 */ f32 unkA0;
	/* 0xA4 */ f32 unkA4;
	/* 0xA8 */ f32 unkA8;
	/* 0xAC */ f32 unkAC;
	/* 0xB0 */ f32 unkB0;
	/* 0xB4 */ f32 unkB4;
	/* 0xB8 */ f32 unkB8;
	/* 0xBC */ u32 unkBC;
};

class TMapObjFlagManager;

extern TMapObjFlagManager* gpMapObjFlagManager;

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

public:
	/* 0x10 */ TMapObjFlagInfo unk10[15];
};

#endif
