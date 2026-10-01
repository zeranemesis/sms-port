#include <MoveBG/MapObjFlag.hpp>

// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/JMath.hpp>
#include <dolphin/gx.h>
#include <MSound/MSound.hpp>
#include <System/MarDirector.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <stdlib.h>
#include <stdio.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// The Wii/GC SDK spells its 12-byte copy helper as a JGeometry weak inline.
namespace JGeometry {
void gekko_ps_copy12(void*, void*);
}

TMapObjFlagManager* gpMapObjFlagManager;

f32 TMapObjFlag::mFlutterSpeed = 4.0f;

void TMapObjFlag::draw()
{
	Mtx mtx;
	JGeometry::gekko_ps_copy12(&mtx, (void*)j3dSys.getViewMtx());
	PSMTXConcat(mtx, mMtx, mtx);
	GXLoadPosMtxImm(mtx, 0);

	// u runs over the vertex columns, v over the rows. The first and the
	// last column of a row carry a literal 0.0f / 1.0f.

	for (s32 i = 0; i < static_cast<s32>(unk74) - static_cast<s32>(unkBC);
	     i += static_cast<s32>(unkBC)) {
		f32 v1 = (static_cast<f32>(unk74) - 1 - i)
		       / (static_cast<f32>(unk74) - 1);
		f32 v2 = (static_cast<f32>(unk74) - 2 - i)
		       / (static_cast<f32>(unk74) - 1);

		GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0,
		        static_cast<u16>(2 * ((unk70 - 2 * static_cast<s32>(unkBC))
		                             / static_cast<s32>(unkBC)
		                             + 2)));

		JGeometry::TVec3<f32>* row = unk78[i];
		GXPosition3f32(row[0].x, row[0].y, row[0].z);
		GXTexCoord2f32(0.0f, v1);
		GXPosition3f32(row[1].x, row[1].y, row[1].z);
		GXTexCoord2f32(0.0f, v2);

		for (s32 j = 1; j < static_cast<s32>(unk70) - static_cast<s32>(unkBC);
		     j += static_cast<s32>(unkBC)) {
			f32 u = static_cast<f32>(j) / (static_cast<f32>(unk70) - 1);
			GXPosition3f32(row[j].x, row[j].y, row[j].z);
			GXTexCoord2f32(u, v1);
			GXPosition3f32(row[j + 1].x, row[j + 1].y, row[j + 1].z);
			GXTexCoord2f32(u, v2);
		}

		s32 last = static_cast<s32>(unk70) - 1;
		GXPosition3f32(row[last].x, row[last].y, row[last].z);
		GXTexCoord2f32(1.0f, v1);
		GXPosition3f32(row[last + 1].x, row[last + 1].y, row[last + 1].z);
		GXTexCoord2f32(1.0f, v2);
	}
}

void TMapObjFlag::updateVertex()
{
	for (s32 i = 0; i < static_cast<s32>(unk74); i += static_cast<s32>(unkBC)) {
		f32 rowAngle = static_cast<f32>(i) * unk80;
		for (s32 j = 0; j < static_cast<s32>(unk70);
		     j += static_cast<s32>(unkBC)) {
			f32 ratio = static_cast<f32>(j) / static_cast<f32>(unk70);
			f32 angle = unk88 + (-j * unk7C + rowAngle);
			while (angle >= 180.0f)
				angle -= 360.0f;
			while (angle < -180.0f)
				angle += 360.0f;

			s32 sinIndex = static_cast<s32>(182.04445f * angle);
			sinIndex = static_cast<u16>(sinIndex) >> jmaSinShift;
			unk78[i][j].x = unk84 * ratio * jmaSinTable[sinIndex];
		}
	}
}

void TMapObjFlag::init(const char* name)
{
	unk68 = 100.0f * mScaling.z;
	unk6C = 100.0f * mScaling.y;
	unk7C = unk7C / mScaling.z;
	unk80 = unk80 / mScaling.y;
	unk84 = unk84 * mScaling.z;

	unk70 = static_cast<s32>(unk68 / 50.0f);
	unk74 = static_cast<s32>(unk6C / 100.0f);
	if (unk70 < 2)
		unk70 = 3;
	if (unk74 < 2)
		unk74 = 3;

	MsMtxSetXYZRPH(mMtx, mPosition.x, mPosition.y, mPosition.z, mRotation.x,
	               mRotation.y, mRotation.z);

	f32 stepX = unk68 / static_cast<f32>(unk70);
	f32 stepY = unk6C / static_cast<f32>(unk74);
	JKRHeap::getCurrentHeap()->getTotalFreeSize();

	unk78 = new JGeometry::TVec3<f32>*[unk74];
	for (s32 i = 0; i < static_cast<s32>(unk74); i++) {
		unk78[i] = new JGeometry::TVec3<f32>[unk70];
		for (s32 j = 0; j < static_cast<s32>(unk70); j++) {
			unk78[i][j].x = 0.0f;
			unk78[i][j].y = i * stepY;
			unk78[i][j].z = j * stepX;
		}
	}

	// TODO: the map lists this function-local static as total_use_size$
	// 2279 with a 1-byte init$2280 guard; the body it guards is unknown.
	static u32 total_use_size = 0;
	(void)total_use_size;

	JKRHeap::getCurrentHeap()->getTotalFreeSize();

	gpMapObjFlagManager->registerObj(this, name);
	initHitActor(0x4000000D, 1, 0, 0.0f, 0.0f, 0.0f, 0.0f);
}

void TMapObjFlag::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);
	char name[0x40];
	stream.readString(name, sizeof(name));
	init(name);
}

TMapObjFlag::TMapObjFlag(const char* name)
    : THitActor(name)
{
	unk68 = 0.0f;
	unk6C = 0.0f;
	unk70 = 0;
	unk74 = 0;
	unk78 = nullptr;
	unk7C = 125.0f;
	unk80 = 130.0f;
	unk84 = 20.0f;
	// a random starting angle for the wave
	unk88 = 360.0f * (0.000030517578f * (f32)rand());
	unkBC = 1;
	mMtx[2][3] = 0.0f;
	mMtx[1][3] = 0.0f;
	mMtx[0][3] = 0.0f;
	mMtx[1][2] = 0.0f;
	mMtx[0][2] = 0.0f;
	mMtx[2][1] = 0.0f;
	mMtx[0][1] = 0.0f;
	mMtx[2][0] = 0.0f;
	mMtx[1][0] = 0.0f;
	mMtx[2][2] = 1.0f;
	mMtx[1][1] = 1.0f;
	mMtx[0][0] = 1.0f;
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

void TMapObjFlagManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & 0x4) {
		for (s32 i = 0; i < 15; i++) {
			for (s32 j = 0; j < unk10[i].unk0; j++) {
				TMapObjFlag* flag = unk10[i].unk4[j];
				MsMtxSetXYZRPH(flag->mMtx, flag->mPosition.x,
				               flag->mPosition.y, flag->mPosition.z,
				               flag->mRotation.x, flag->mRotation.y,
				               flag->mRotation.z);
				flag->updateVertex();
				flag->unk88 += TMapObjFlag::mFlutterSpeed;
				if (flag->unk88 > 360.0f)
					flag->unk88 -= 360.0f;
				if (flag->mScaling.y > 3.0f && flag->mScaling.z > 3.0f
				    && gpMarDirector->mMap != 3)
					gpMSound->startSoundActor(0x302F, &flag->mPosition, 0,
					                         nullptr, 0, 4);
			}
		}
	}

	if (cue & 0x10) {
		initDraw();
		for (s32 i = 0; i < 15; i++) {
			if (unk10[i].unk0 != 0) {
				JUTTexture texture(unk10[i].unk54);
				texture.load(GX_TEXMAP0);
				for (s32 j = 0; j < unk10[i].unk0; j++)
					unk10[i].unk4[j]->draw();
			}
		}
	}
}

void TMapObjFlagManager::registerObj(TMapObjFlag* obj, const char* name)
{
	if (strcmp(name, "flagSun") == 0) {
		char path[0x40];
		if (unk10[0].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[0].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[0].unk4[unk10[0].unk0] = obj;
		unk10[0].unk0++;
	} else if (strcmp(name, "flagWhite") == 0) {
		char path[0x40];
		if (unk10[1].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[1].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[1].unk4[unk10[1].unk0] = obj;
		unk10[1].unk0++;
	} else if (strcmp(name, "flagRedsun") == 0) {
		char path[0x40];
		if (unk10[2].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[2].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[2].unk4[unk10[2].unk0] = obj;
		unk10[2].unk0++;
	} else if (strcmp(name, "flagMonte") == 0) {
		char path[0x40];
		if (unk10[3].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[3].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[3].unk4[unk10[3].unk0] = obj;
		unk10[3].unk0++;
	} else if (strcmp(name, "flagBird") == 0) {
		char path[0x40];
		if (unk10[4].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[4].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[4].unk4[unk10[4].unk0] = obj;
		unk10[4].unk0++;
	} else if (strcmp(name, "flagHigekuri") == 0) {
		char path[0x40];
		if (unk10[5].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[5].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[5].unk4[unk10[5].unk0] = obj;
		unk10[5].unk0++;
	} else if (strcmp(name, "flagBenvenuto") == 0) {
		char path[0x40];
		if (unk10[6].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[6].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[6].unk4[unk10[6].unk0] = obj;
		unk10[6].unk0++;
	} else if (strcmp(name, "flagDolpicDolphin") == 0) {
		char path[0x40];
		if (unk10[7].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[7].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[7].unk4[unk10[7].unk0] = obj;
		unk10[7].unk0++;
	} else if (strcmp(name, "flagDolSun") == 0) {
		char path[0x40];
		if (unk10[8].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[8].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[8].unk4[unk10[8].unk0] = obj;
		unk10[8].unk0++;
	} else if (strcmp(name, "flagDolSunWelcome") == 0) {
		char path[0x40];
		if (unk10[9].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[9].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[9].unk4[unk10[9].unk0] = obj;
		unk10[9].unk0++;
	} else if (strcmp(name, "flagBianco") == 0) {
		char path[0x40];
		if (unk10[10].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[10].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[10].unk4[unk10[10].unk0] = obj;
		unk10[10].unk0++;
	} else if (strcmp(name, "flagRiccoBuoy") == 0) {
		char path[0x40];
		if (unk10[11].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[11].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[11].unk4[unk10[11].unk0] = obj;
		unk10[11].unk0++;
	} else if (strcmp(name, "flagSailMonte") == 0) {
		char path[0x40];
		if (unk10[12].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[12].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[12].unk4[unk10[12].unk0] = obj;
		unk10[12].unk0++;
	} else if (strcmp(name, "MammaYacht00") == 0) {
		char path[0x40];
		if (unk10[13].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[13].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[13].unk4[unk10[13].unk0] = obj;
		unk10[13].unk0++;
	} else if (strcmp(name, "flagMare") == 0) {
		char path[0x40];
		if (unk10[14].unk54 == nullptr) {
			snprintf(path, sizeof(path), "/scene/mapObj/%s.bti", name);
			unk10[14].unk54 = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		unk10[14].unk4[unk10[14].unk0] = obj;
		unk10[14].unk0++;
	}
}

void TMapObjFlagManager::load(JSUMemoryInputStream& stream)
{
	JDrama::TNameRef::load(stream);
	char name[0x10];
	stream.readString(name, 8);

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

TMapObjFlagManager::TMapObjFlagManager(const char* name)
    : JDrama::TViewObj(name)
{
	gpMapObjFlagManager = this;
}

TMapObjFlagManager::~TMapObjFlagManager() { }
