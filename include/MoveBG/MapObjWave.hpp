#ifndef MOVE_BG_MAP_OBJ_WAVE_HPP
#define MOVE_BG_MAP_OBJ_WAVE_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <dolphin/gx/GXStruct.h>

class TMapObjWave;

extern TMapObjWave* gpMapObjWave;

class TMapObjWave : public JDrama::TViewObj {
public:
	TMapObjWave(const char* name = "波の表現");
	virtual ~TMapObjWave() { }

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	// Unused
	void movement();
	void updateTime();
	void updateHeightAndAlpha();
	void draw();
	s32 getAlpha(f32, f32) const;
	void noWave();
	f32 getHeight(f32, f32, f32) const;
	f32 getWaveHeight(f32, f32) const;
	// Unused
	f32 getStaticTexPos0(f32) const;
	f32 getStaticTexPos1(f32) const;
	f32 getMoveTexPos0(f32) const;
	f32 getMoveTexPos1(f32) const;
	void initDraw();

public:
	// Total size is 0x9C, from the `li r3, 0x9c` ahead of the constructor
	// call in TMarNameRefGen::getNameRef_MapObj.
	/* 0x10 */ f32 mWaveSpan;
	/* 0x14 */ f32 mHalfWaveSpan;
	/* 0x18 */ f32 mInvHalfWaveSpan;
	/* 0x1C */ f32 mWaveHeight;
	/* 0x20 */ s32 mWaveCount;
	/* 0x24 */ f32 mAngleSpeed0;
	/* 0x28 */ f32 mAngleSpeed1;
	/* 0x2C */ f32 unk2C;
	/* 0x30 */ f32 unk30;
	/* 0x34 */ f32 unk34;
	/* 0x38 */ f32 unk38;
	/* 0x3C */ f32 mAmplitude0;
	/* 0x40 */ f32 mAmplitude1;
	/* 0x44 */ f32 mAlphaAcc;
	/* 0x48 */ f32 mAlphaStep;
	/* 0x4C */ f32 unk4C;
	/* 0x50 */ f32 unk50;
	/* 0x54 */ f32 mAlpha;
	/* 0x58 */ f32 mAlphaMax;
	/* 0x5C */ f32 mAlphaMin;
	/* 0x60 */ f32 mTexSpeed;
	/* 0x64 */ f32 mAngle0;
	/* 0x68 */ f32 mAngle1;
	/* 0x6C */ f32 mTexPos0;
	/* 0x70 */ f32 mTexPos1;
	/* 0x74 */ f32 mWaveTexScale;
	/* 0x78 */ f32 mWaveTexScale2;
	// Three GXColorS10 (8 bytes each), set up in initDraw().
	/* 0x7C */ GXColorS10 mTevColor0;
	/* 0x84 */ GXColorS10 mTevColor1;
	/* 0x8C */ GXColorS10 mTevColor2;
	/* 0x94 */ u32 unk94;
	/* 0x98 */ s16 unk98H;
	/* 0x9A */ u8 unk9A[0x2];
};

#endif
