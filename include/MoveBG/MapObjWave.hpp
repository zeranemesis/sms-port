#ifndef MOVE_BG_MAP_OBJ_WAVE_HPP
#define MOVE_BG_MAP_OBJ_WAVE_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>

class TMapObjWave;

extern TMapObjWave* gpMapObjWave;

class TMapObjWave : public JDrama::TViewObj {
public:
	TMapObjWave(const char* name = "波の表現");

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
	/* 0x10 */ u8 unk10[0x14];
	/* 0x24 */ f32 mAngleSpeed0;
	/* 0x28 */ f32 mAngleSpeed1;
	/* 0x2C */ f32 unk2C;
	/* 0x30 */ f32 unk30;
	/* 0x34 */ f32 unk34;
	/* 0x38 */ f32 unk38;
	/* 0x3C */ f32 mAmplitude0;
	/* 0x40 */ f32 mAmplitude1;
	/* 0x44 */ u8 unk44[0x1C];
	/* 0x60 */ f32 mTexSpeed;
	/* 0x64 */ f32 mAngle0;
	/* 0x68 */ f32 mAngle1;
	/* 0x6C */ f32 mTexPos0;
	/* 0x70 */ f32 mTexPos1;
	/* 0x74 */ u8 unk74[0x20];
	/* 0x94 */ u32 unk94;
	/* 0x98 */ u8 unk98[0x4];
};

#endif
