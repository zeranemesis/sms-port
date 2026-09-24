#ifndef MOVE_BG_MAP_OBJ_WAVE_HPP
#define MOVE_BG_MAP_OBJ_WAVE_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>

class TMapObjWave;

extern TMapObjWave* gpMapObjWave;

class TMapObjWave : public JDrama::TViewObj {
public:
	TMapObjWave(const char* name = "波の表現");
	virtual ~TMapObjWave();

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void movement();
	void updateTime();
	void updateHeightAndAlpha();
	void draw();
	void getAlpha(float, float) const;
	void noWave();
	f32 getHeight(float, float, float) const;
	f32 getWaveHeight(float, float) const;
	void getStaticTexPos0(float) const;
	void getStaticTexPos1(float) const;
	void getMoveTexPos0(float) const;
	void getMoveTexPos1(float) const;
	void initDraw();

public:
	// Total size is 0x9C, from the `li r3, 0x9c` ahead of the constructor
	// call in TMarNameRefGen::getNameRef_MapObj. Offsets named below are the
	// ones updateTime() and noWave() reach; the gaps are still unread.
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
	/* 0x44 */ u8 unk44[0x8];
	/* 0x4C */ f32 unk4C;
	/* 0x50 */ f32 unk50;
	/* 0x54 */ u8 unk54[0xC];
	/* 0x60 */ f32 mTexSpeed;
	/* 0x64 */ f32 mAngle0;
	/* 0x68 */ f32 mAngle1;
	/* 0x6C */ f32 mTexPos0;
	/* 0x70 */ f32 mTexPos1;
	/* 0x74 */ f32 mWaveTexScale;
	/* 0x78 */ f32 mWaveTexScale2;
	/* 0x7C */ u8 unk7C[0x18];
	/* 0x94 */ u32 unk94;
	/* 0x98 */ u8 unk98[0x4];
};

#endif
