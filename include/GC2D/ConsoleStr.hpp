#ifndef GC2D_CONSOLE_STR_HPP
#define GC2D_CONSOLE_STR_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JUtility/JUTPoint.hpp>

class J2DSetScreen;
class TExPane;
class J2DTextBox;
class TBoundPane;

class TConsoleStr : public JDrama::TViewObj {
public:
	TConsoleStr(const char* name = "<ConsoleStr>");
	f32 getWipeCloseTime();
	void load(JSUMemoryInputStream&);
	void loadAfter();
	void perform(u32 cue, JDrama::TGraphics* graphics);
	void startAppearReady();
	void startAppearGo();
	void startAppearShineGet();
	void startAppearMiss();
	void startAppearScenario();
	bool processReady(int);
	bool processGo(float);
	bool processShineGet(int);
	bool processMiss(int);
	bool processScenario(int);
	void startCloseWipe(bool);
	void startOpenWipe();

	// TODO: wrong types
	static JUTPoint cShineGetRight1;
	static JUTPoint cShineGetLeft1;
	static JUTPoint cShineGetRight2;
	static JUTPoint cShineGetLeft2;
	static JUTPoint cShineGetRight3;
	static JUTPoint cShineGetLeft3;

public:
	/* 0x10 */ J2DSetScreen* unk10;
	/* 0x14 */ J2DSetScreen* unk14;
	u8 unk18Pad[0x3C];
	/* 0x54 */ f32 unk18;
	/* 0x58 */ int unk1C;
	/* 0x5C */ u32 unk20;
	/* 0x60 */ u32 unk24;
	/* 0x64 */ TBoundPane* unk28[3];
	/* 0x70 */ JUTPoint unk34[66];
	/* Retail contains additional pane storage before these members. */
	u8 unk244Pad[0x5C4];
	/* 0x844 */ TBoundPane* unk244[9];
	/* 0x868 */ TBoundPane* unk268[5];
	/* 0x87C */ TExPane* unk27C[5];
	/* 0x890 */ TExPane* unk290[2];
	/* 0x898 */ TExPane* unk298;
	/* 0x89C */ TExPane* unk29C;
	/* 0x8A0 */ J2DTextBox* unk2A0[2];
	/* 0x8A8 */ u8 unk2A8;
	/* 0x8A9 */ u8 unk2A9;
	/* 0x8AC */ void* unk2AC;
	/* 0x8B0 */ void* unk2B0;
	/* 0x8B4 */ void* unk2B4;
	/* Retail pane/state storage omitted from the current source model. */
	u8 unk2C0[0x20];
	/* 0x8D8 */ int unk2B8;
	/* 0x8DC */ int unk2BC;
};

#endif
