#ifndef GC2D_GUIDE_HPP
#define GC2D_GUIDE_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JUtility/JUTRect.hpp>

class JKRMemArchive;
class J2DPane;
class J2DPicture;
class J2DSetScreen;
class J2DTextBox;
class JUTTexture;
class TBoundPane;
class TExPane;
class TMarioGamePad;

// fabricated
// Per-stage totals displayed on the guide (map) screen.
struct TGuideStageInfo {
	/* 0x0 */ u8 unk0;
	/* 0x1 */ u8 mShineNum;
	/* 0x2 */ u8 mEtcShineNum;
	/* 0x4 */ u16 mCoinNum;
	/* 0x6 */ u8 mHundredCoinShine;
	/* 0x7 */ u8 mBlueCoinNum;
};

class TGuide : public JDrama::TViewObj {
public:
	TGuide(const char* name = "<Guide>");
	virtual ~TGuide() { }
	virtual void load(JSUMemoryInputStream& stream);
	virtual void perform(u32, JDrama::TGraphics*);

	void resetObjects();
	void resetScore();
	JKRMemArchive* setup(JKRMemArchive*);
	void setup2(JKRMemArchive*);
	void startMoveCursor();
	void startMoveCursor2();
	void linkSelect();
	void changePattern(J2DPicture*, s16, u32);
	void mirrorPattern(J2DPicture*, s16, u32);
	void rotatePattern(J2DPicture*, s16, u32, s16);
	void shinePattern(TBoundPane*, s16, u32);
	void mmarkPattern(TExPane*, s16, u32);
	void searchNearPoint(s16*, s16*, s16, s16);
	int checkPoint(int, int);
	void changeBotStatus(int);
	void placeMario();
	void appearGuidePane(int);
	void disappearGuidePane(int);
	void control(); // fabricated

public:
	/* 0x10 */ int unk10;
	/* 0x14 */ TGuideStageInfo unk14[10];
	/* 0x64 */ char unk64[0xBC - 0x64];
	/* 0xBC */ J2DSetScreen* unkBC;
	/* 0xC0 */ TMarioGamePad* unkC0;
	/* 0xC4 */ u8 unkC4;
	/* 0xC5 */ u8 unkC5;
	/* 0xC8 */ JUTTexture* unkC8[10];
	/* 0xF0 */ u16 unkF0;
	/* 0xF4 */ J2DPane* unkF4;
	/* 0xF8 */ J2DPicture* unkF8[2];
	/* 0x100 */ J2DPane* unk100;
	/* 0x104 */ J2DPane* unk104[2];
	/* 0x10C */ J2DPicture* unk10C[3];
	/* 0x118 */ J2DPane* unk118;
	/* 0x11C */ J2DPicture* unk11C[2];
	/* 0x124 */ J2DTextBox* unk124;
	/* 0x128 */ TExPane* unk128[2];
	/* 0x130 */ char unk130[4];
	/* 0x134 */ J2DPicture* unk134;
	/* 0x138 */ J2DPicture* unk138;
	/* 0x13C */ J2DPicture* unk13C;
	/* 0x140 */ J2DPicture* unk140;
	/* 0x144 */ J2DPicture* unk144;
	/* 0x148 */ J2DPicture* unk148;
	/* 0x14C */ J2DPicture* unk14C;
	/* 0x150 */ J2DPicture* unk150;
	/* 0x154 */ J2DPane* unk154;
	/* 0x158 */ J2DPane* unk158;
	/* 0x15C */ u8 unk15C;
	/* 0x160 */ int unk160;
	/* 0x164 */ u8 unk164;
	/* 0x168 */ J2DPane* unk168[22];
	/* 0x1C0 */ TExPane* unk1C0[22];
	/* 0x218 */ JUTRect unk218[22];
	/* 0x378 */ TExPane* unk378[22];
	/* 0x3D0 */ J2DPane* unk3D0[10];
	/* 0x3F8 */ char unk3F8[0x424 - 0x3F8];
	/* 0x424 */ TExPane* unk424;
	/* 0x428 */ TExPane* unk428;
	/* 0x42C */ s16 unk42C;
	/* 0x430 */ J2DPane* unk430;
	/* 0x434 */ JUTRect unk434;
	/* 0x444 */ TBoundPane* unk444;
	/* 0x448 */ J2DPane* unk448;
	/* 0x44C */ J2DPane* unk44C[10];
	/* 0x474 */ void* unk474;
	/* 0x478 */ TExPane* unk478;
	/* 0x47C */ u8 unk47C;
	/* 0x480 */ int unk480;
	/* 0x484 */ char unk484[0x48C - 0x484];
	/* 0x48C */ JUTRect unk48C;
	/* 0x49C */ char unk49C[0x6F8 - 0x49C];
};

#endif
