#ifndef GC2D_GUIDE_HPP
#define GC2D_GUIDE_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JUtility/JUTRect.hpp>

class JKRMemArchive;
class J2DPicture;
class TBoundPane;
class TExPane;
class TMarioGamePad;

class TGuide : public JDrama::TViewObj {
public:
	TGuide(const char* name = "<Guide>");
	virtual ~TGuide();
	void load(JSUMemoryInputStream& stream);
	void resetObjects();
	void resetScore();
	JKRMemArchive* setup(JKRMemArchive*);
	void setup2(JKRMemArchive*);
	void startMoveCursor();
	void startMoveCursor2();
	void linkSelect();
	void changePattern(J2DPicture*, short, unsigned long);
	void mirrorPattern(J2DPicture*, short, unsigned long);
	void rotatePattern(J2DPicture*, short, unsigned long, short);
	void shinePattern(TBoundPane*, short, unsigned long);
	void mmarkPattern(TExPane*, short, unsigned long);
	void searchNearPoint(short*, short*, short, short);
	void checkPoint(int, int);
	void changeBotStatus(int);
	void placeMario();
	void appearGuidePane(int);
	void disappearGuidePane(int);
	void perform(unsigned long, JDrama::TGraphics*);

public:
	/* 0x10 */ s32 unk10;
	/* 0x14 */ char unk14[0xBC - 0x14];
	/* 0xBC */ s32 unkBC;
	/* 0xC0 */ TMarioGamePad* unkC0;
	/* 0xC4 */ u8 unkC4;
	/* 0xC5 */ u8 unkC5;
	/* 0xC6 */ char unkC6[0x160 - 0xC6];
	/* 0x160 */ s32 unk160;
	/* 0x164 */ u8 unk164;
	/* 0x165 */ char unk165[0x218 - 0x165];
	/* 0x218 */ JUTRect unk218[22];
	/* 0x378 */ char unk378[0x434 - 0x378];
	/* 0x434 */ JUTRect unk434;
	/* 0x444 */ char unk444[0x480 - 0x444];
	/* 0x480 */ s32 unk480;
	/* 0x484 */ char unk484[8];
	/* 0x48C */ JUTRect unk48C;
	/* 0x49C */ char unk49C[0x6F8 - 0x49C];
};

#endif
