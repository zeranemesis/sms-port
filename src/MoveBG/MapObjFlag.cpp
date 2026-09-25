#include <MoveBG/MapObjFlag.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/JMath.hpp>
#include <dolphin/gx.h>
#include <System/MarDirector.hpp>
#include <stdlib.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

TMapObjFlagManager* gpMapObjFlagManager;

TMapObjFlagManager::~TMapObjFlagManager() { }

f32 TMapObjFlag::mFlutterSpeed = 4.0f;

void TMapObjFlag::updateVertex()
{
	JGeometry::TVec3<f32>** vertices =
	    reinterpret_cast<JGeometry::TVec3<f32>**>(unk78);

	for (s32 i = 0; i < static_cast<s32>(unk74); i += static_cast<s32>(unkBC)) {
		f32 rowAngle = static_cast<f32>(i) * unk80;
		for (s32 j = 0; j < static_cast<s32>(unk70);
		     j += static_cast<s32>(unkBC)) {
			f32 ratio = static_cast<f32>(j) / static_cast<f32>(unk70);
			f32 angle = -static_cast<f32>(j) * unk7C + rowAngle + unk88;
			while (angle >= 180.0f)
				angle -= 360.0f;
			while (angle < -180.0f)
				angle += 360.0f;

			s32 sinIndex = static_cast<s32>(182.04445f * angle);
			sinIndex = static_cast<u16>(sinIndex) >> jmaSinShift;
			vertices[i][j].x = unk84 * ratio * jmaSinTable[sinIndex];
		}
	}
}

TMapObjFlag::TMapObjFlag(const char* name)
    : THitActor(name)
{
	unk68 = 0.0f;
	unk6C = 0.0f;
	unk70 = 0;
	unk74 = 0;
	unk78 = 0;
	unk7C = 125.0f;
	unk80 = 130.0f;
	unk84 = 20.0f;
	unk88 = 4.0f * ((f32)rand() * 0.000030517578f);
	unkBC = 1;
	unkB8 = 0.0f;
	unkA8 = 0.0f;
	unk98 = 0.0f;
	unkA4 = 0.0f;
	unk94 = 0.0f;
	unkB0 = 0.0f;
	unk90 = 0.0f;
	unkAC = 0.0f;
	unk9C = 0.0f;
	unkB4 = 1.0f;
	unkA0 = 1.0f;
	unk8C = 1.0f;
}

void TMapObjFlag::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);
	char name[0x40];
	stream.readString(name, sizeof(name));
	init(name);
}

TMapObjFlagManager::TMapObjFlagManager(const char* name)
    : JDrama::TViewObj(name)
{
	gpMapObjFlagManager = this;
}

void TMapObjFlagManager::load(JSUMemoryInputStream& stream)
{
	JDrama::TNameRef::load(stream);
	char buffer[8];
	stream.readString(buffer, sizeof(buffer));

	switch (gpMarDirector->mMap) {
	case 0:
		TMapObjFlag::mFlutterSpeed = 16.0f;
		break;
	case 2:
		TMapObjFlag::mFlutterSpeed = 16.0f;
		break;
	case 4:
		TMapObjFlag::mFlutterSpeed = 12.0f;
		break;
	default:
		TMapObjFlag::mFlutterSpeed = 8.0f;
		break;
	}
}

void TMapObjFlagManager::initDraw()
{
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetCurrentMtx(GX_PNMTX0);
	GXSetNumChans(0);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0,
	              GX_DF_NONE, GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0,
	              GX_DF_NONE, GX_AF_NONE);
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0xff, 0xff, 0xff, 0xff });
	GXSetNumTexGens(1);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY,
	                  GX_FALSE, GX_PTIDENTITY);
	GXSetNumTevStages(1);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_GREATER, 0);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GXSetZCompLoc(GX_FALSE);
	GXSetCullMode(GX_CULL_NONE);
}
