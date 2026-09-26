#ifndef GC2D_CARD_LOAD_HPP
#define GC2D_CARD_LOAD_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <System/CardManager.hpp>
#include <GC2D/Progress.hpp>

class J2DPane;
class J2DPicture;
class J2DTextBox;
class J2DSetScreen;
class JPABaseEmitter;
class TCardLoad;
class TExPane;
class JUTTexture;
class TFileLoadBlock;
class TOptionControl;
class TMapObjOptionWall;
class TMarioGamePad;

extern TCardLoad* gpCardLoad;

class TCardLoad : public JDrama::TViewObj {
public:
	TCardLoad(const char* name = "<TCardLoad>");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void changePattern(J2DPicture*, s16, u32);
	void setupTitleScreen();
	void setupScoreScreen();
	void resetScoreScreenObjects();
	void loadLangTexture();
	void changeLanguage(int);
	void loadAfter();
	bool titleDraw();
	void makeBuffer(J2DTextBox*, int);
	TEProgress changeMode(s32);
	void setMessage(J2DTextBox*, s32, int);
	s8 waitForChoice(TEProgress, TEProgress, int);
	s8 waitForChoiceBM(TEProgress, TEProgress, int);
	s8 waitForAnyKey(TEProgress);
	s8 waitForAnyKeyBM(TEProgress);
	s8 waitForStart(TEProgress);
	s8 drawMessage(TEProgress);
	s8 drawMessageBM(TEProgress);
	s8 selectBookmark(TEProgress, TEProgress, bool);
	s8 selectFunction();
	void setSelected(u8);
	void changeScene();

	static u32 cMessageID[];
	static const char* cToOptionFileName;
	static const char* cBMarkNewFileName;
	static const char* cWindowYesFileName;
	static const char* cWindowNoFileName;
	static const char* cScoreTotalFileName;
	static const char* cBMarkMenuFileName[4];
	static const char* cScoreStageFileName[9];

public:
	struct UnkCardLoadStruct {
		/* 0x0 */ J2DPicture* unk0;
		/* 0x4 */ J2DPicture* unk4[3];
		/* 0x10 */ J2DPane* unk10[8];
		/* 0x30 */ J2DPane* unk30[2];
		/* 0x38 */ J2DPane* unk38;
	};

	/* 0x10 */ int unk10;
	/* 0x14 */ int unk14;
	/* 0x18 */ int unk18;
	/* 0x1C */ TEProgress unk1C;
	/* 0x20 */ u32 unk20;
	/* 0x24 */ int unk24;
	/* 0x28 */ J2DSetScreen* unk28;
	/* 0x2C */ J2DSetScreen* unk2C;
	/* 0x30 */ u32 unk30;
	/* 0x34 */ J2DSetScreen* unk34;
	/* 0x38 */ TMarioGamePad* unk38;
	/* 0x3C */ char unk3C[4];
	/* 0x40 */ TCardBookmarkInfo unk40[3];
	/* 0xA0 */ void* unkA0;
	/* 0xA4 */ u32 unkA4;
	/* 0xA8 */ f32 unkA8;
	/* 0xAC */ JPABaseEmitter* unkAC;
	/* 0xB0 */ s8 unkB0;
	/* 0xB1 */ s8 unkB1;
	/* 0xB2 */ s8 unkB2;
	/* 0xB4 */ s16 unkB4;
	/* 0xB6 */ s8 unkB6;
	/* 0xB7 */ s8 unkB7;
	/* 0xB8 */ u8 unkB8;
	/* 0xBC */ int unkBC;
	/* 0xC0 */ int unkC0;
	/* 0xC4 */ int unkC4;
	/* 0xC8 */ JUTTexture* unkC8[10];
	/* 0xF0 */ TExPane* unkF0;
	/* 0xF4 */ TExPane* unkF4;
	/* 0xF8 */ TExPane* unkF8[15];
	/* 0x134 */ JUTRect unk124[15];
	/* 0x224 */ TExPane* unk1D4[18];
	/* 0x26C */ J2DPane* unk208;
	/* 0x270 */ u16 unk20C[15];
	/* 0x28E */ u8 unk222[15];
	/* 0x29E */ u16 unk22E[18];
	/* 0x2C2 */ u8 unk248[18];
	/* 0x2D4 */ char unk255[0x2D6 - 0x2D4];
	/* 0x2D6 */ u16 unk258;
	/* 0x2D8 */ J2DPane* unk25C;
	/* 0x2DC */ JUTRect unk260;
	/* 0x2EC */ J2DPane* unk270;
	/* 0x2F0 */ u8 unk274;
	/* 0x2F1 */ u8 unk275;
	/* 0x2F4 */ TFileLoadBlock* unk278[3];
	/* 0x300 */ TMapObjOptionWall* unk284;
	/* 0x304 */ TExPane* unk288;
	/* 0x308 */ JUTRect unk28C;
	/* 0x318 */ J2DTextBox* unk29C;
	/* 0x31C */ J2DTextBox* unk2A0;
	/* 0x320 */ TExPane* unk2A4[3];
	/* 0x32C */ JUTRect unk2B0;
	/* 0x33C */ J2DTextBox* unk2C0[3];
	/* 0x348 */ J2DTextBox* unk2CC[3];
	/* 0x354 */ J2DPane* unk2D8[3];
	/* 0x360 */ J2DPicture* unk2E4[3];
	/* 0x36C */ J2DPicture* unk2F0[3];
	/* 0x378 */ J2DPane* unk2FC[3];
	/* 0x384 */ J2DPicture* unk308[3];
	/* 0x390 */ J2DPicture* unk314[3];
	/* 0x39C */ J2DPicture* unk320[3];
	/* 0x3A8 */ J2DPicture* unk32C[3];
	/* 0x3B4 */ int unk338;
	/* 0x3B8 */ TExPane* unk33C[3];
	/* 0x3C4 */ JUTRect unk348[3];
	/* 0x3F4 */ TExPane* unk378[3][4];
	/* 0x424 */ JUTRect unk3A8[3][4];
	/* 0x4E4 */ TExPane* unk468;
	/* 0x4E8 */ JUTRect unk46C;
	/* 0x4F8 */ J2DTextBox* unk47C;
	/* 0x4FC */ J2DTextBox* unk480;
	/* 0x500 */ TExPane* unk484[2];
	/* 0x508 */ JUTRect unk48C[2];
	/* 0x528 */ TExPane* unk4AC;
	/* 0x52C */ JUTRect unk4B0;
	/* 0x53C */ J2DTextBox* unk4C0;
	/* 0x540 */ J2DTextBox* unk4C4;
	/* 0x544 */ J2DPane* unk4C8;
	/* 0x548 */ J2DPane* unk4CC[3];
	/* 0x554 */ TExPane* unk4D8[2];
	/* 0x55C */ JUTRect unk4E0[2];
	/* 0x57C */ J2DPane* unk500;
	/* 0x580 */ J2DPicture* unk504[3];
	/* 0x58C */ J2DPane* unk510;
	/* 0x590 */ J2DPicture* unk514[2];
	/* 0x598 */ J2DTextBox* unk51C;
	/* 0x59C */ J2DTextBox* unk520;
	/* 0x5A0 */ TExPane* unk524;
	/* 0x5A4 */ JUTRect unk528;
	/* 0x5B4 */ J2DTextBox* unk538;
	/* 0x5B8 */ J2DTextBox* unk53C;
	/* 0x5BC */ J2DPane* unk540[3];
	/* 0x5C8 */ TExPane* unk54C;
	/* 0x5CC */ JUTRect unk550;
	/* 0x5DC */ J2DTextBox* unk560;
	/* 0x5E0 */ J2DTextBox* unk564;
	/* 0x5E4 */ TExPane* unk568;
	/* 0x5E8 */ JUTRect unk56C;
	/* 0x5F8 */ J2DTextBox* unk57C;
	/* 0x5FC */ J2DTextBox* unk580;
	/* 0x600 */ UnkCardLoadStruct unk584[7];
	/* 0x7A4 */ J2DPane* unk728[3];
	/* 0x7B0 */ char unk734[0x7BC - 0x7B0];
	/* 0x7BC */ J2DPane* unk740;
	/* 0x7C0 */ J2DPane* unk744;
	/* 0x7C4 */ J2DPicture* unk748;
	/* 0x7C8 */ J2DPicture* unk74C;
	/* 0x7CC */ J2DPicture* unk750;
	/* 0x7D0 */ TOptionControl* unk754;
	/* 0x7D4 */ s32 unk7D4;
	/* 0x7D8 */ void* unk7D8[5];
	/* 0x7EC */ J2DSetScreen* unk7EC[5];
	/* 0x800 */ JUTTexture* unk800[5];
	/* 0x814 */ JUTTexture* unk814[4][5];
	/* 0x864 */ JUTTexture* unk864[5];
	/* 0x878 */ JUTTexture* unk878[5];
	/* 0x88C */ JUTTexture* unk88C[5];
	/* 0x8A0 */ JUTTexture* unk8A0[9][5];
	/* 0x954 */ JUTTexture* unk954[5];
};

#endif
