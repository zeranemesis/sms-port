#include <MarioUtil/PacketUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DShape.hpp>
#include <JSystem/J3D/J3DGraphBase/Blocks/J3DPEBlocks.hpp>
#include <dolphin/gx/GXDispList.h>
#include <dolphin/gd.h>

// The five helpers below are the SDK's GX command-FIFO writers, spelled out so
// they can be inlined into ShapePacketCallBackFunc (which is where the ROM has
// them). The FIFO is the write-gather pipe at 0xCC008000 - see
// include/port/recomp_gx_fifo.h - so every store below goes there.
// The ROM emits NO out-of-line copy of this: it inlines the body at every
// call site. Without `inline`, MWCC sees a multi-call `static` and emits it
// out of line, which costs 72 B of `extra` text in our object and ~1.5 pp of
// the unit, because those bytes sit in the denominator with no ROM counterpart.
inline static void FifoSetChanMatColor(GXChannelID chan, GXColor color)
{
	GXCmd1u8(GX_CMD_LOAD_XF_REG);
	GXCmd1u16(0);
	GXCmd1u16(XF_REG_MATERIAL0_ID + (chan & 1));
	GXCmd1u32(color.r << 24 | color.g << 16 | color.b << 8 | color.a);
}

// TODO: the ROM still emits one extra rlwimi per colour half (it merges the
// second shifted channel with rlwimi where we keep a second slwi), so the
// shape differs even though the value is right.
// No out-of-line copy exists in the ROM; see FifoSetChanMatColor above.
inline static void FifoSetTevColorS10(GXTevRegID id, GXColorS10 color)
{
	// The & 0x7FF masks are load-bearing: they let MWCC fold each channel shift
	// into clrlslwi/rlwimi. Written out longhand (no mask) it emits slwi/or
	// triples instead and the unit drops from 82.6% to 77.8%.
	u32 ra = BP_TEV_COLOR_REG_RA(color.r & 0x7FF, color.a & 0x7FF, 0,
	                            0xE0 + id * 2);
	u32 bg = BP_TEV_COLOR_REG_BG(color.b & 0x7FF, color.g & 0x7FF, 0,
	                            0xE1 + id * 2);

	// The redundant BG writes are for hardware timing.
	*(volatile u8*)0xCC008000  = 0x61;
	*(volatile u32*)0xCC008000 = ra;
	*(volatile u8*)0xCC008000  = 0x61;
	*(volatile u32*)0xCC008000 = bg;
	*(volatile u8*)0xCC008000  = 0x61;
	*(volatile u32*)0xCC008000 = bg;
	*(volatile u8*)0xCC008000  = 0x61;
	*(volatile u32*)0xCC008000 = bg;
}

// No out-of-line copy exists in the ROM; see FifoSetChanMatColor above.
inline static void FifoSetTevKColor(GXTevKColorID id, GXColor color)
{
	*(volatile u8*)0xCC008000  = 0x61;
	*(volatile u32*)0xCC008000
	    = BP_TEV_COLOR_REG_RA(color.r, color.a, 1, 0xE0 + id * 2);
	*(volatile u8*)0xCC008000  = 0x61;
	*(volatile u32*)0xCC008000
	    = BP_TEV_COLOR_REG_BG(color.b, color.g, 1, 0xE1 + id * 2);
}

// ROM keeps this out of line: ShapePacketCallBackFunc calls it for the packet
// types 5 and 8, so it must not be inlined into the callback.
//
// TODO: instruction-for-instruction identical to ours, but the ROM re-derives
// table->r[i] through a fresh address every unrolled step - "addi r7,r5,4;
// lhz r7,0x0(r7)" - where we reach the same halfword with displacement
// addressing ("lhz r0,4(r5)"). That costs the ROM four extra addi (312 bytes vs
// our 296) and interleaves the stb/lis 0xcc01 differently, so it reads like
// MWCC scheduling rather than anything the source can say. Left as-is.
#pragma dont_inline on
static void FifoSetFogRangeAdj(u8 enable, u16 center, GXFogAdjTable* table)
{
	if (enable != 0) {
		s32 i;
		for (i = 0; i < 10; i += 2) {
			*(volatile u8*)0xCC008000  = 0x61;
			*(volatile u32*)0xCC008000 = (0xE9 + i / 2) << 24
			                            | (u32)table->r[i + 1] << 12
			                            | table->r[i];
		}
	}

	*(volatile u8*)0xCC008000  = 0x61;
	*(volatile u32*)0xCC008000 = 0xE8 << 24 | (center + 342) | enable << 10;
}
#pragma dont_inline off

static void FifoSetFog(GXFogType type, f32 startZ, f32 endZ, f32 nearZ, f32 farZ,
                       GXColor color)
{
	// TODO(fdivs): the ROM divides in single precision straight off the double
	// register - "lfd f0,0x30(r1); fsubs f0,f0,f2; fdivs f0,f3,f0" - so both the
	// subtraction and the division are single-precision ops over registers that
	// hold doubles (a Gekko single-precision op rounds whatever double it finds
	// in the FPR, which is why the ROM still gets the right answer). Getting
	// fdivs out of MWCC 1.2.5e needs the denominator in a named f32 local; that
	// is the `denom` below, which buys fdivs and moves scale/a_f onto the ROM's
	// 0x30/0x24 slots. `fsub` vs `fsubs` and the extra frsp are the two
	// instructions still in the wrong shape. Measured on a probe TU:
	//   a / (scale - K)                -> fsub; fdiv; frsp
	//   d = scale - K; a / d           -> fsub; frsp; fdivs      <- used here
	//   a / (f32)(scale - K)           -> fsub; frsp; fdivs
	//   d = (f32)scale - (f32)K        -> frsp; lfs; fsubs; fdivs  (!!)
	//   d = (f32)scale - (f32)kConstF64-> lfd; frsp; frsp; fsubs; fdivs
	// The last two are the only forms that emit fsubs, and both need the
	// constant cast to f32, which folds @2284 from the ROM's 8-byte double down
	// to a 4-byte .sdata2 entry and costs an extra frsp.
	double scale;
	f32 a, b, c;

	if (farZ == nearZ || endZ == startZ) {
		a = 0.0f;
		b = 0.5f;
		c = 0.0f;
	} else {
		a = (farZ * nearZ) / ((farZ - nearZ) * (endZ - startZ));
		b = farZ / (farZ - nearZ);
		c = startZ / (endZ - startZ);
	}

	u32 expn = 1;
	while (b > 1.0) {
		b *= 0.5f;
		++expn;
	}
	while (b > 0.0f && b < 0.5) {
		b *= 2.0f;
		--expn;
	}

	f32 a_f;

	// Naming these two is load-bearing as well, not just for fdivs above: c20
	// has to stay live from here to the 0xF1000000 store, which is what gives
	// MWCC a sixth non-volatile register (r27) and makes the whole tail use the
	// ROM's r26..r31 set instead of ours r26..r30 plus a late reload.
	u32 c20 = *(volatile u32*)&c << 20;
	{
		f32 denom;

		((u32*)&scale)[0] = 0x43300000;
		((u32*)&scale)[1] = (1 << expn) ^ 0x80000000;
		denom = scale - 4503601774854144.0;
		a_f = a / denom;
	}

	*(volatile u8*)0xCC008000 = 0x61;
	*(volatile u32*)0xCC008000
	    = 0xEE000000 | ((u32)*(volatile u32*)&a_f >> 12);
	*(volatile u8*)0xCC008000 = 0x61;
	*(volatile u32*)0xCC008000 = 0xEF000000 | (u32)(8388638.0f * b);
	*(volatile u8*)0xCC008000  = 0x61;
	*(volatile u32*)0xCC008000 = 0xF0000000 | (u32)expn;
	*(volatile u8*)0xCC008000 = 0x61;
	*(volatile u32*)0xCC008000 = ((u32)type << 21) | c20 | 0xF1000000;
	*(volatile u8*)0xCC008000 = 0x61;
	*(volatile u32*)0xCC008000
	    = BP_FOG_COLOR(color.r, color.g, color.b, 0xF2);
}

// TODO: name and meaning unknown. The ROM keeps this as one 4-byte constant in
// .sdata2 and copies it to the stack to pass by value; it is all zeroes.
static const GXColor sFogOffColor = { 0, 0, 0, 0 };

// fabricated: the user-area blob handed to the shape packets. The first word is
// a type tag; the meaning of the remaining words depends on that tag.
struct PacketUserData {
	/* 0x00 */ u32 unk0;
	/* 0x04 */ union {
		GXChannelID chan;
		GXTevRegID tevReg;
		GXTevKColorID tevKReg;
		J3DFog* fog;
		u8* displayList;
	} unk4;
	/* 0x08 */ union {
		const GXColor* color;
		const GXColorS10* colorS10;
		GXTevRegID tevReg;
		GXTevKColorID tevKReg;
		u32 displayListSize;
	} unk8;
	/* 0x0C */ union {
		const GXColor* color;
		const GXColorS10* colorS10;
		GXTevRegID tevReg;
		J3DFog* fog;
	} unkC;
	/* 0x10 */ union {
		const GXColor* color;
		const GXColorS10* colorS10;
		J3DFog* fog;
	} unk10;
	/* 0x14 */ union {
		const GXColor* color;
		const GXColorS10* colorS10;
	} unk14;
	/* 0x18 */ const GXColorS10* unk18;
};

// The 0xF1000000 word: the ROM builds it as
//   slwi r0,type,21 ; rlwimi r0,c,20,12,31 ; oris r0,r0,0xf100
// i.e. it merges (c<<20) into (type<<21) with a single rlwimi and folds the
// constant in last with oris, which needs MWCC to believe bits 16-31 of the
// shifted pair are clear. Spelling it `type<<21 | c20 | 0xF1000000` gets the
// instruction order and every register right but yields slwi + or + oris;
// hoisting the raw bits of c instead of the shifted value so MWCC can fold the
// shift into the merge costs 3.8% on this function (79.4 vs 83.1).
//
// TODO(offsets): 213 of the remaining diffs in this function are pure stack
// offsets and the instruction sets are otherwise identical. The frame is 0x178
// in the ROM and 0xB8 here. The block of locals is contiguous in both and sits
// at a fixed distance below the new SP - MWCC allocates it top-down from just
// under the register save area - so the only thing that moves it is the reserve
// MWCC puts at the bottom of the frame (0x10..reserve_end):
//   ROM  reserve 0x10..0xE7 = 0xD8 (216B), block 0xE8..0x167 = 32 slots
//   ours reserve 0x10..0x3B = 0x2C ( 44B), block 0x3C..0xAB = 28 slots
// and the reserve is content-driven but saturates: measured 0x08 with only
// case 0, 0x20 with cases 0-3, 0x2C with all eleven. Adding a dead
// "char framePad[N]" only grows the frame above the block (verified: pad[172]
// took the frame to 0x168 and left every local at 0x3C..0xAB), dropping the
// by-value GXColor arguments of the three FifoSetFog call sites left it at
// 0x2C, and removing a named local did not move it either. There is no
// source-expressible way found to grow the reserve by the 172 bytes needed.
//
// Per-site slot map (target - ours) once a reserve of 0xD8 exists, i.e. the
// exact deltas still to be produced:
//   case 0/1/2/2/3/3/3 by-value GXColorS10 copies   +0xBC
//   case 6/7/7/8 by-value GXColor copies           +0xC0
//   case 9/9/10/10/10 copies and sFogOffColor      +0xC4
//   case 5 and case 8 fog->mColor (FifoSetFog arg) +0x88
// Note the last group is not a uniform shift either: the ROM hoists those two
// copies to the very bottom of the block with 16 bytes (0xEC..0xFB) of dead
// space between them and sFogOffColor, which ours interleaves at 0x74/0x60.
static void ShapePacketCallBackFunc(J3DCallBackPacket* packet, int data)
{
	PacketUserData* ud = (PacketUserData*)packet->getUserArea();

	if (data == 0) {
		switch (ud->unk0) {
		case 0:
			FifoSetChanMatColor(ud->unk4.chan, *ud->unk8.color);
			break;
		case 1:
			FifoSetTevColorS10(ud->unk4.tevReg, *ud->unk8.colorS10);
			break;
		case 2:
			FifoSetTevColorS10(ud->unk4.tevReg, *ud->unkC.colorS10);
			FifoSetTevColorS10(ud->unk8.tevReg, *ud->unk10.colorS10);
			break;
		case 3:
			FifoSetTevColorS10(ud->unk4.tevReg, *ud->unk10.colorS10);
			FifoSetTevColorS10(ud->unk8.tevReg, *ud->unk14.colorS10);
			FifoSetTevColorS10(ud->unkC.tevReg, *ud->unk18);
			break;
		case 4:
			GXCallDisplayList(ud->unk4.displayList, ud->unk8.displayListSize);
			break;
		case 5: {
			J3DFog* fog = ud->unk4.fog;
			FifoSetFog((GXFogType)fog->mType, fog->mStartZ, fog->mEndZ,
			           fog->mNearZ, fog->mFarZ, fog->mColor);
			FifoSetFogRangeAdj(fog->mAdjEnable, fog->mCenter,
			                   (GXFogAdjTable*)fog->mFogAdjTable);
			break;
		}
		case 6:
			FifoSetTevKColor(ud->unk4.tevKReg, *ud->unk8.color);
			break;
		case 7:
			FifoSetTevKColor(ud->unk4.tevKReg, *ud->unkC.color);
			FifoSetTevKColor(ud->unk8.tevKReg, *ud->unk10.color);
			break;
		case 8: {
			J3DFog* fog = ud->unk10.fog;
			FifoSetTevKColor(ud->unk8.tevKReg, *ud->unkC.color);
			FifoSetFog((GXFogType)fog->mType, fog->mStartZ, fog->mEndZ,
			           fog->mNearZ, fog->mFarZ, fog->mColor);
			FifoSetFogRangeAdj(fog->mAdjEnable, fog->mCenter,
			                   (GXFogAdjTable*)fog->mFogAdjTable);
			break;
		}
		case 9:
			FifoSetTevColorS10(ud->unk4.tevReg, *ud->unk8.colorS10);
			FifoSetTevKColor(GX_KCOLOR0, *ud->unkC.color);
			break;
		case 10:
			FifoSetTevColorS10(ud->unk4.tevReg, *ud->unkC.colorS10);
			FifoSetTevColorS10(ud->unk8.tevReg, *ud->unk10.colorS10);
			FifoSetTevKColor(GX_KCOLOR0, *ud->unk14.color);
			break;
		}
	} else if (data == 1) {
		// Packets that carry fog data turn the fog back off once drawn.
		if (ud->unk0 == 8 || ud->unk0 == 5) {
			FifoSetFog(GX_FOG_NONE, 0.0f, 0.0f, 0.0f, 0.0f,
			           sFogOffColor);
		}
	}
}

static J3DShapePacket* InitPacket_Sub(J3DModel* model, u16 mat_idx)
{
	J3DMaterial* mat = model->getModelData()->getMaterialNodePointer(mat_idx);
	return model->getShapePacket(mat->getShape()->getIndex());
}

// fabricated
struct PacketUserData_MatColor {
	u32 unk0;
	GXChannelID unk4;
	const GXColor* unk8;
};

void SMS_InitPacket_MatColor(J3DModel* param_1, u16 param_2,
                             GXChannelID param_3, const GXColor* param_4)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_MatColor* userData = new PacketUserData_MatColor;

	userData->unk0 = 0;
	userData->unk4 = param_3;
	userData->unk8 = param_4;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_OneTevColor {
	u32 unk0;
	GXTevRegID unk4;
	const GXColorS10* unk8;
};

void SMS_InitPacket_OneTevColor(J3DModel* param_1, u16 param_2,
                                GXTevRegID param_3, const GXColorS10* param_4)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_OneTevColor* userData = new PacketUserData_OneTevColor;

	userData->unk0 = 1;
	userData->unk4 = param_3;
	userData->unk8 = param_4;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_TwoTevColor {
	u32 unk0;
	GXTevRegID unk4;
	GXTevRegID unk8;
	const GXColorS10* unkC;
	const GXColorS10* unk10;
};

void SMS_InitPacket_TwoTevColor(J3DModel* param_1, u16 param_2,
                                GXTevRegID param_3, const GXColorS10* param_4,
                                GXTevRegID param_5, const GXColorS10* param_6)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_TwoTevColor* userData = new PacketUserData_TwoTevColor;

	userData->unk0  = 2;
	userData->unk4  = param_3;
	userData->unkC  = param_4;
	userData->unk8  = param_5;
	userData->unk10 = param_6;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_ThreeTevColor {
	u32 unk0;
	GXTevRegID unk4;
	GXTevRegID unk8;
	GXTevRegID unkC;
	const GXColorS10* unk10;
	const GXColorS10* unk14;
	const GXColorS10* unk18;
};

void SMS_InitPacket_ThreeTevColor(J3DModel* param_1, u16 param_2,
                                  GXTevRegID param_3, const GXColorS10* param_4,
                                  GXTevRegID param_5, const GXColorS10* param_6,
                                  GXTevRegID param_7, const GXColorS10* param_8)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_ThreeTevColor* userData = new PacketUserData_ThreeTevColor;

	userData->unk0  = 3;
	userData->unk4  = param_3;
	userData->unk10 = param_4;
	userData->unk8  = param_5;
	userData->unk14 = param_6;
	userData->unkC  = param_7;
	userData->unk18 = param_8;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_Fog {
	u32 unk0;
	J3DFog* unk4;
};

void SMS_InitPacket_Fog(J3DModel* param_1, u16 param_2)
{
	J3DPEBlock* peBlock = param_1->getModelData()
	                          ->getMaterialNodePointer(param_2)
	                          ->getPEBlock();
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	J3DFog* fog = peBlock->getFog();

	PacketUserData_Fog* userData = new PacketUserData_Fog;
	userData->unk0               = 5;
	userData->unk4               = fog;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_OneTevKColor {
	u32 unk0;
	GXTevKColorID unk4;
	const GXColor* unk8;
};

void SMS_InitPacket_OneTevKColor(J3DModel* param_1, u16 param_2,
                                 GXTevKColorID param_3, const GXColor* param_4)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_OneTevKColor* userData = new PacketUserData_OneTevKColor;

	userData->unk0 = 6;
	userData->unk4 = param_3;
	userData->unk8 = param_4;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_TwoTevKColor {
	u32 unk0;
	GXTevKColorID unk4;
	GXTevKColorID unk8;
	const GXColor* unkC;
	const GXColor* unk10;
};

void SMS_InitPacket_TwoTevKColor(J3DModel* param_1, u16 param_2,
                                 GXTevKColorID param_3, const GXColor* param_4,
                                 GXTevKColorID param_5, const GXColor* param_6)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_TwoTevKColor* userData = new PacketUserData_TwoTevKColor;

	userData->unk0  = 7;
	userData->unk4  = param_3;
	userData->unkC  = param_4;
	userData->unk8  = param_5;
	userData->unk10 = param_6;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_OneTevKColorAndFog {
	u32 unk0;
	u32 unk4;
	GXTevKColorID unk8;
	const GXColor* unkC;
	u32 unk10;
	J3DFog* unk14;
};

void SMS_InitPacket_OneTevKColorAndFog(J3DModel* param_1, u16 param_2,
                                       GXTevKColorID param_3,
                                       const GXColor* param_4)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_OneTevKColorAndFog* userData
	    = new PacketUserData_OneTevKColorAndFog;

	userData->unk0 = 8;
	userData->unk4 = 6;
	userData->unk8 = param_3;

	if (param_4 != nullptr) {
		userData->unkC = param_4;
	} else {
		userData->unkC = &param_1->getModelData()
		                      ->getMaterialNodePointer(param_2)
		                      ->getTevBlock()
		                      ->getTevKColor(param_3)
		                      ->color;
	}

	J3DFog* fog = param_1->getModelData()
	                  ->getMaterialNodePointer(param_2)
	                  ->getPEBlock()
	                  ->getFog();

	userData->unk10 = 5;
	userData->unk14 = fog;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_OneTevColorAndOneTevKColor {
	u32 unk0;
	GXTevRegID unk4;
	const GXColorS10* unk8;
	const GXColor* unkC;
};

void SMS_InitPacket_OneTevColorAndOneTevKColor(J3DModel* param_1, u16 param_2,
                                               GXTevRegID param_3,
                                               const GXColorS10* param_4,
                                               const GXColor* param_5)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_OneTevColorAndOneTevKColor* userData
	    = new PacketUserData_OneTevColorAndOneTevKColor;

	userData->unk0 = 9;
	userData->unk4 = param_3;
	userData->unk8 = param_4;
	userData->unkC = param_5;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_TwoTevColorAndOneTevKColor {
	u32 unk0;
	GXTevRegID unk4;
	GXTevRegID unk8;
	const GXColorS10* unkC;
	const GXColorS10* unk10;
	const GXColor* unk14;
};

void SMS_InitPacket_TwoTevColorAndOneTevKColor(J3DModel* param_1, u16 param_2,
                                               GXTevRegID param_3,
                                               const GXColorS10* param_4,
                                               GXTevRegID param_5,
                                               const GXColorS10* param_6,
                                               const GXColor* param_7)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_TwoTevColorAndOneTevKColor* userData
	    = new PacketUserData_TwoTevColorAndOneTevKColor;

	userData->unk0  = 10;
	userData->unk4  = param_3;
	userData->unkC  = param_4;
	userData->unk8  = param_5;
	userData->unk10 = param_6;
	userData->unk14 = param_7;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

void SMS_HideAllShapePacket(J3DModel* model)
{
	u16 mats = model->getModelData()->getMaterialNum();
	for (u16 i = 0; i < mats; ++i)
		model->getShapePacket(i)->hide();
}

void SMS_ShowAllShapePacket(J3DModel* model)
{
	u16 mats = model->getModelData()->getMaterialNum();
	for (u16 i = 0; i < mats; ++i)
		model->getShapePacket(i)->show();
}
