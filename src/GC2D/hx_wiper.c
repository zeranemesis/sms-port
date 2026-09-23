#include <GC2D/hx_wiper.h>
#include <JSystem/ResTIMG.hpp>
#include <dolphin/gx.h>
#include <dolphin/mtx.h>
#include <dolphin/vi.h>
#include <dolphin/dvd.h>
#include <dolphin/os.h>

// TODO: this translation unit is freshly scaffolded from marioEU.MAP. The
// big drawing routines (Hx_Test*, Hx_Logo, Hx_GameOver, the Hxs_* helpers)
// are not decompiled yet and are left as empty stubs.

void ReInitializeGX(void);

typedef struct HxMotion {
	/* 0x00 */ f32 accelEnd;
	/* 0x04 */ f32 decelStart;
	/* 0x08 */ f32 end;
	/* 0x0C */ f32 accel;
	/* 0x10 */ f32 unk10;
	/* 0x14 */ f32 decel;
	/* 0x18 */ f32 speed;
	/* 0x1C */ f32 time;
	/* 0x20 */ f32 value;
} HxMotion;

typedef struct HxWork {
	/* 0x00 */ u32 width;
	/* 0x04 */ u32 height;
	/* 0x08 */ u32 halfWidth;
	/* 0x0C */ u32 halfHeight;
	/* 0x10 */ u8 state; // 0 = idle, 1 = start requested, 2 = running, 3 = done
	/* 0x11 */ u8 type;
	/* 0x12 */ u8 handleType;
	/* 0x14 */ f32 time;
	/* 0x18 */ f32 delta;
	/* 0x1C */ int param;
	/* 0x20 */ void (*handler)(void);
	/* 0x24 */ int hasResource;
	/* 0x28 */ int hasResourceEx;
	/* 0x2C */ void* resource;
	/* 0x30 */ void* resourceEx;
	/* 0x34 */ u32 resourceSize;
	/* 0x38 */ u32 step;
	/* 0x3C */ u32 timer;
	/* 0x40 */ HxMotion motion;
	/* 0x64 */ u8 isPal;
} HxWork;

static HxWork hx;
static u8 hx_buffer[0x3300] ATTRIBUTE_ALIGN(32);

static u16 img_wx;
static u16 img_wy;

static void Hx_Circle(void);
static void Hx_Test1(void);
static void Hx_Test5(void);
static void Hx_Test4(void);
static void Hx_Test2R(void);
static void Hx_Test2(void);
static void Hx_Door(void);
static void Hx_Logo(void);
static void Hx_GameOver(void);
static void dummy_handler(void);
static void Hx_SetVFilter(f32 strength);
static void Hxs_FrBufferMorf2(f32 x);
static void Hxs_FrBufferMorf2B(f32 x);

static void Hx_CameraInit(void)
{
	static Vec camLoc = { 320.0f, 240.0f, -30.0f };
	static Vec objPt  = { 320.0f, 240.0f, 0.0f };
	static Vec up     = { 0.0f, -10.0f, 0.0f };
	Mtx44 proj;
	Mtx view;
	f32 hw = hx.width >> 1;
	f32 hh = hx.height >> 1;

	camLoc.x = hw;
	camLoc.y = hh;
	objPt.x  = hw;
	objPt.y  = hh;
	C_MTXOrtho(proj, hh, -hh, -hw, hw, 0.0f, 100.0f);
	GXSetProjection(proj, GX_ORTHOGRAPHIC);
	GXSetViewport(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f);
	C_MTXLookAt(view, &camLoc, &up, &objPt);
	GXSetCullMode(GX_CULL_NONE);
	GXSetCoPlanar(GX_FALSE);
	GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
	GXSetNumTexGens(0);
	GXSetNumTevStages(1);
	GXSetNumIndStages(0);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXSetLineWidth(6, GX_TO_ZERO);
	GXLoadPosMtxImm(view, GX_PNMTX0);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHT_NULL,
	              GX_DF_NONE, GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL,
	              GX_DF_NONE, GX_AF_NONE);
	GXSetNumChans(1);
}

static void Hx_GxInit(int texMode, int blendMode)
{
	switch (texMode) {
	case 0:
		GXSetNumTexGens(0);
		GXSetNumTevStages(1);
		GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,
		              GX_COLOR0A0);
		GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
		break;
	case 1:
		GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY,
		                  GX_FALSE, GX_PTIDENTITY);
		GXSetNumTexGens(1);
		GXSetNumTevStages(1);
		GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
		GXClearVtxDesc();
		GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
		GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
		GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
		break;
	}

	switch (blendMode) {
	case 1:
		GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
		               GX_LO_CLEAR);
		break;
	case 0:
		GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
		break;
	}
}

void Hgx_DrawCircle(void)
{
	// TODO: UNUSED in the map (size 0x1c8), contents unknown
}

static void Hgx_init_tobj_resource(GXTexObj* obj, struct ResTIMG* img)
{
	void* image    = (u8*)img + img->imageDataOffset;
	u8 format      = img->format;
	u8 wrapS       = img->wrapS;
	u8 wrapT       = img->wrapT;
	u8 minFilter   = img->minFilter;
	u8 magFilter   = img->magFilter;

	img_wx = img->width;
	img_wy = img->height;
	GXInitTexObj(obj, image, img_wx, img_wy, (GXTexFmt)format,
	             (GXTexWrapMode)wrapS, (GXTexWrapMode)wrapT, GX_FALSE);
	GXInitTexObjLOD(obj, (GXTexFilter)minFilter, (GXTexFilter)magFilter, 0.0f,
	                0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
}

static void Hgx_ReadTexture(char* path, void* buffer)
{
	DVDFileInfo info;
	s32 length;

	if (hx.hasResource != 0)
		return;

	if (DVDOpen(path, &info)) {
		length = DVDReadPrio(&info, buffer, info.length, 0, 2);
		DVDClose(&info);
		DCStoreRange(buffer, length);
	}
}

static void Hx_GetFrBuffer(void* dest, u32 left, u32 top, u32 width,
                           u32 height)
{
	GXColor clear = { 0, 0, 0, 0 };

	GXSetTexCopySrc(left, top, width, height);
	GXSetTexCopyDst(width, height, GX_TF_RGB565, GX_FALSE);
	GXGetTexBufferSize(width, height, GX_TF_RGB565, GX_FALSE, 0);
	GXSetCopyClear(clear, 0xFFFFFF);
	GXCopyTex(dest, GX_TRUE);
	GXPixModeSync();
}

static u8* fbuf = hx_buffer;
static u8 vtable_org[7] = { 0x10, 0x10, 0x00, 0x00, 0x00, 0x10, 0x10 };
static u8 dec_step[4] = { 0, 1, 5, 6 };
static u8 inc_step[3] = { 2, 3, 4 };
static u8 vtable[7];

static void Hx_SetVFilter(f32 strength)
{
	u8 steps;
	u32 i;

	vtable[0] = vtable_org[0];
	vtable[1] = vtable_org[1];
	vtable[2] = vtable_org[2];
	steps     = 64.0f * strength;
	vtable[3] = vtable_org[3];
	vtable[4] = vtable_org[4];
	vtable[5] = vtable_org[5];
	vtable[6] = vtable_org[6];

	for (i = 0; i < steps; i++) {
		vtable[dec_step[i & 3]]--;
		vtable[inc_step[i % 3]]++;
	}

	GXSetCopyFilter(GX_FALSE, NULL, GX_TRUE, vtable);
}

void Hx_SetVFilterFade(void)
{
	// TODO: UNUSED in the map (size 0x358), contents unknown
}

static void __Hx_FrBufferMorf(u16 x, u16 y)
{
	GXTexObj texObj;

	Hx_CameraInit();
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	Hx_GetFrBuffer(fbuf, x, y, 0x30, 0x30);
	GXInvalidateTexAll();
	GXSetNumTexGens(1);
	GXSetNumTevStages(1);
	GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
	GXInitTexObj(&texObj, fbuf, 0x30, 0x30, GX_TF_RGB565, GX_CLAMP, GX_CLAMP,
	             GX_FALSE);
	GXInitTexObjLOD(&texObj, GX_LINEAR, GX_LINEAR, 0.0f, 10.0f, 0.0f,
	                GX_FALSE, GX_TRUE, GX_ANISO_1);
	GXLoadTexObj(&texObj, GX_TEXMAP0);
	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition3f32(x, y, 0.0f);
	GXTexCoord2f32(0.0f, 0.0f);
	GXPosition3f32(x + 0x30, y, 0.0f);
	GXTexCoord2f32(1.0f, 0.0f);
	GXPosition3f32(x + 0x30, y + 0x30, 0.0f);
	GXTexCoord2f32(1.0f, 1.0f);
	GXPosition3f32(x, y + 0x30, 0.0f);
	GXTexCoord2f32(0.0f, 1.0f);
	GXEnd();
}

static void Hx_FrBufferMorf(f32 strength)
{
	Hx_SetVFilter(strength);
	__Hx_FrBufferMorf(hx.halfWidth - 0x18, hx.halfHeight - 0x18);
}

static u8* fbuf2 = hx_buffer;

static void Frb2_InitGx(GXTexObj* texObj)
{
	Hx_CameraInit();
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	Hx_SetVFilter(1.0f);
	GXSetNumTexGens(1);
	GXSetNumTevStages(1);
	GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
	GXInitTexObj(texObj, fbuf2, 0xA0, 0x10, GX_TF_RGB565, GX_CLAMP, GX_CLAMP,
	             GX_FALSE);
	GXInitTexObjLOD(texObj, GX_LINEAR, GX_LINEAR, 0.0f, 10.0f, 0.0f,
	                GX_FALSE, GX_TRUE, GX_ANISO_1);
	GXLoadTexObj(texObj, GX_TEXMAP0);
}

static void Frb2_InitBlackBox(void)
{
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEX_DISABLE, GX_COLOR0A0);
}

static void Frb2_RendBox(u32 color, f32 x0, f32 y0, f32 x1, f32 y1)
{
	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition3f32(x0, y0, 0.0f);
	GXColor1u32(color);
	GXPosition3f32(x1, y0, 0.0f);
	GXColor1u32(color);
	GXPosition3f32(x1, y1, 0.0f);
	GXColor1u32(color);
	GXPosition3f32(x0, y1, 0.0f);
	GXColor1u32(color);
	GXEnd();
}

static void Hx_Warning(int code) { }

void SetDisplaySize(void)
{
	// TODO: UNUSED in the map (size 0x18), contents unknown
}

void Hx_ResetWipe(u32 width, u32 height)
{
	hx.state         = 0;
	hx.width         = width;
	hx.height        = height;
	hx.halfWidth     = hx.width >> 1;
	hx.halfHeight    = hx.height >> 1;
	hx.hasResource   = 0;
	hx.hasResourceEx = 0;
}

void Hx_ProvideResource(void* resource, u32 size)
{
	if (hx.state == 2)
		Hx_Warning(1);
	if (hx.hasResource != 0)
		Hx_Warning(3);
	hx.hasResource  = 1;
	hx.resource     = resource;
	hx.resourceSize = size;
}

void Hx_ProvideResourceEx(void* resource)
{
	if (hx.state == 2)
		Hx_Warning(1);
	hx.hasResourceEx = 1;
	hx.resourceEx    = resource;
}

void Hx_RemoveResource(void)
{
	if (hx.state == 2)
		Hx_Warning(1);
	if (hx.hasResource == 0)
		Hx_Warning(2);
	hx.hasResource   = 0;
	hx.hasResourceEx = 0;
}

void Hx_StartWipe(u8 type, int param)
{
	if (VIGetTvFormat() == VI_PAL)
		hx.isPal = 1;
	else
		hx.isPal = 0;

	if (hx.hasResource == 0) {
		hx.resource     = hx_buffer;
		hx.resourceSize = sizeof(hx_buffer);
	}

	switch (hx.state) {
	case 2:
		Hx_Warning(1);
		break;
	}

	hx.state = 1;
	hx.type  = type;
	hx.time  = 0.0f;
	hx.param = param;
}

static void dummy_handler(void) { }

static void (*handle_table[])(void) = {
	dummy_handler, Hx_Circle, Hx_Circle,  Hx_Test1, Hx_Test1,
	Hx_Test5,      Hx_Test5,  Hx_Test4,   Hx_Test4, Hx_Test2R,
	Hx_Test2,      Hx_Door,   Hx_Logo,    Hx_GameOver, dummy_handler,
};

static u8 handle_type[] = {
	0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0,
};

u8 Hx_GetWipeType(int type) { return handle_type[type]; }

u8 Hx_UpdateWipe(f32 delta)
{
	ReInitializeGX();
	switch (hx.state) {
	case 0:
		break;
	case 3:
		if (hx.handleType != 1) {
			Hx_CameraInit();
			Hx_GxInit(0, 0);
			Frb2_InitBlackBox();
			Frb2_RendBox(0xFF, 0.0f, 0.0f, hx.width, hx.height);
		}
		break;
	case 1:
		hx.handler    = handle_table[hx.type];
		hx.handleType = handle_type[hx.type];
		hx.state      = 2;
		hx.step       = 0;
	case 2:
		hx.delta = delta;
		GXDrawDone();
		hx.handler();
		GXDrawDone();
		hx.time += delta;
		break;
	}
	return hx.state;
}

static u32 Hx_TimerCountDown(void)
{
	if (hx.timer != 0)
		hx.timer--;
	return hx.timer;
}

static void Hx_MotionSet(HxMotion* motion, f32 distance, f32 accelTime,
                         f32 constTime, f32 decelTime)
{
	f32 speed;

	motion->accelEnd   = accelTime;
	motion->decelStart = motion->accelEnd + constTime;
	motion->end        = motion->decelStart + decelTime;
	speed = (2.0f * distance) / (accelTime + constTime + constTime + decelTime);
	if (accelTime != 0.0f)
		motion->accel = speed / accelTime;
	if (decelTime != 0.0f)
		motion->decel = -speed / decelTime;
	motion->unk10 = 0.0f;
	motion->speed = 0.0f;
	motion->value = 0.0f;
	motion->time  = 0.0f;
}

static f32 Hx_MotionUpdate(HxMotion* motion)
{
	if (motion->accelEnd > motion->time)
		motion->speed += motion->accel;
	else if (motion->decelStart <= motion->time)
		motion->speed += motion->decel;
	motion->time += 1.0f;
	motion->value += motion->speed;
	return motion->value;
}

static void Hx_Circle(void)
{
	// TODO: not decompiled yet
}

static void Hxs1_Circle(f32 param_1)
{
	// TODO: not decompiled yet
}

static void Hxs2_Circle(u8 param_1, f32 param_2, f32 param_3)
{
	// TODO: not decompiled yet
}

static void Hxs_FrBufferMorf2(f32 x)
{
	// TODO: not decompiled yet
}

static void Hxs_FrBufferMorf2B(f32 x)
{
	// TODO: not decompiled yet
}

static void Hx_Door(void)
{
	u32 value;

	switch (hx.step) {
	case 0:
		hx.step++;
		Hx_MotionSet(&hx.motion, hx.width >> 1, 5.0f, 6.0f, 5.0f);
		break;
	case 1:
		value = Hx_MotionUpdate(&hx.motion);
		Hxs_FrBufferMorf2(value);
		if (value >= hx.width >> 1) {
			hx.step++;
			Hx_MotionSet(&hx.motion, hx.width >> 1, 5.0f, 6.0f, 5.0f);
		}
		break;
	case 2:
		Hxs_FrBufferMorf2(hx.width >> 1);
		value = Hx_MotionUpdate(&hx.motion);
		Hxs_FrBufferMorf2B(value);
		if (value >= hx.width >> 1)
			hx.step++;
		break;
	case 3:
		Hxs_FrBufferMorf2(hx.width >> 1);
		Hxs_FrBufferMorf2B(hx.width >> 1);
		hx.state = 3;
		break;
	}
}

static void Hxs_GameOver(void)
{
	// TODO: not decompiled yet
}

void InitWipe(void)
{
	// TODO: UNUSED in the map (size 0xc), contents unknown
}

static void Hx_GameOver(void)
{
	// TODO: not decompiled yet
}

static void Hxs_Logo_ExtraDraw(void)
{
	// TODO: not decompiled yet
}

static void Hxs_Logo_TexSetup(void)
{
	// TODO: not decompiled yet
}

static void Hxs_Logo_TexDraw(void)
{
	// TODO: not decompiled yet
}

static void Hxs_Logo_MagDraw(void)
{
	// TODO: not decompiled yet
}

static void Hxs_PenDraw(void)
{
	// TODO: not decompiled yet
}

static int hxs_logo_resetflag;
static int hxs_logodraw_resetflag;

static void Hx_Logo(void)
{
	// TODO: not decompiled yet
}

void Hx_MovieStartSync(void)
{
	// TODO: UNUSED in the map (size 0x70), contents unknown
}

int Hx_MovieStartSyncEx(void)
{
	if (hx.type != 12)
		return 0;

	if (hx.step >= 2 && hx.step <= 5) {
		if (hxs_logodraw_resetflag == 0)
			return 0;
		hxs_logodraw_resetflag = 0;
		return 1;
	}

	if (hx.step >= 6) {
		if (hxs_logo_resetflag == 0)
			return 0;
		if (hx.step == 6 && hx.timer > 0xC0)
			return 0;
		hxs_logo_resetflag = 0;
		return 2;
	}

	return 0;
}

static void Hx_Test1(void)
{
	// TODO: not decompiled yet
}

static void Hxs1_Test1(void)
{
	// TODO: not decompiled yet
}

static void Hx_Test2(void)
{
	// TODO: not decompiled yet
}

static void Hx_Test2R(void)
{
	// TODO: not decompiled yet
}

static void Hxs1_Test2(void)
{
	// TODO: not decompiled yet
}

static void Hx_Test4(void)
{
	// TODO: not decompiled yet
}

static void Hx_Test5(void)
{
	// TODO: not decompiled yet
}
