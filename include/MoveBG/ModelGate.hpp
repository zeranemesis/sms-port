#ifndef MOVE_BG_MODEL_GATE
#define MOVE_BG_MODEL_GATE

#include <Strategic/TakeActor.hpp>
#include <M3DUtil/MActor.hpp>

class SampleCtrlModelData;

class TModelGate : public TTakeActor {
public:
	TModelGate(const char* name = "<TModelGate>")
	    : TTakeActor(name)
	{
	}

	virtual MtxPtr getTakingMtx();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void loadAfter();

	void screenBlur(JDrama::TGraphics*);
	void startOpen();

public:
	// Size 0x118
	/* 0x70 */ u8 unk70;
	/* 0x71 */ u8 unk71;  // Destination stage
	/* 0x72 */ u16 unk72; // center bone index
	/* 0x74 */ s16 unk74; // rotation used for touch particles
	/* 0x76 */ u8 unk76[2];
	/* 0x78 */ MActor* unk78; // Gate model actor
	/* 0x7C */ Mtx unk7C;     // cached taking/touch matrix
	/* 0xAC */ JGeometry::TVec3<f32> unkAC; // Mario hold offset
	/* 0xB8 */ u8 unkB8; // animation/material-control mode
	/* 0xB9 */ u8 unkB9; // material animation step (0..7)
	/* 0xBA */ u8 unkBA; // previous material animation step
	/* 0xBB */ u8 unkBB;
	/* 0xBC */ u16 unkBC; // material animation countdown
	/* 0xBE */ u16 unkBE; // material animation countdown reload (0xF0)
	/* 0xC0 */ SampleCtrlModelData* unkC0;
	/* 0xC4 */ u8 unkC4;
	/* 0xC5 */ u8 unkC5[3];
	/* 0xC8 */ s16 unkC8;
	/* 0xCA */ s16 unkCA;
	/* 0xCC */ s16 unkCC;
	/* 0xCE */ u16 unkCE;
	/* 0xD0 */ f32 unkD0;
	/* 0xD4 */ f32 unkD4;
	/* 0xD8 */ f32 unkD8;
	/* 0xDC */ f32 unkDC;
	/* 0xE0 */ u8 unkE0;
	/* 0xE1 */ u8 unkE1[3];
	/* 0xE4 */ f32 unkE4;
	/* 0xE8 */ f32 unkE8;
	/* 0xEC */ f32 unkEC;
	/* 0xF0 */ f32 unkF0;
	/* 0xF4 */ f32 unkF4;
	/* 0xF8 */ f32 unkF8;
	/* 0xFC */ f32 unkFC;
	/* 0x100 */ f32 unk100;
	/* 0x104 */ f32 unk104;
	/* 0x108 */ f32 unk108;
	/* 0x10C */ f32 unk10C;
	/* 0x110 */ f32 unk110;
	/* 0x114 */ f32 unk114;
	/* 0x118 */ u32 unk118;
};

#endif
