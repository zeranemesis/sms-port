#include <MoveBG/MapObjWave.hpp>
#include <System/MarDirector.hpp>
#include <math.h>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JUtility/JUTColor.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <Camera/CubeManagerBase.hpp>
#include <Player/MarioAccess.hpp>
#include <dolphin/gx.h>
#include <JSystem/JGeometry/JGMatrix33.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <stdlib.h>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// This unit is reverse_fn_order: with -inline deferred MWCC emits functions in
// the reverse of their source order, so the source runs backwards relative to
// the addresses in the map. Check with:
//   python tools/validate-symbol-order.py -u mario/MoveBG/MapObjWave
// The linker's .text layout for this TU is, in address order:
//   __ct__, load, perform, movement, updateTime, updateHeightAndAlpha, draw,
//   getAlpha, noWave, getHeight, getWaveHeight, getStaticTexPos0,
//   getStaticTexPos1, getMoveTexPos0, getMoveTexPos1, initDraw, __sinit
// so this file defines them in exactly the reverse of that.

// Alpha-compare thresholds for the wave quads, used by initDraw(). These are
// mutable statics: retail loads them at run time rather than folding the
// literals into the call.
static u8 sAlphaCompLarge = 0x55;
static u8 sAlphaCompSmall = 0x23;

// The flat water tint the wave quads are drawn with; its alpha channel is
// replaced per-vertex by the distance fade computed in draw(). TColor's
// default constructor stores 0xffffffff, which is why retail runs it from
// __sinit_MapObjWave_cpp rather than leaving it zeroed in .bss.
static JUtility::TColor sColor;

// 0.700 is the background type of the riverbed collider under the wave.
static inline bool isUnk700(const TBGCheckData* data)
{
	if (data->mBGType == 0x700)
		return true;
	else
		return false;
}

void TMapObjWave::initDraw()
{
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);

	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX1, GX_DIRECT);

	GXLoadPosMtxImm(j3dSys.mViewMtx, GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);

	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);

	GXSetNumTexGens(2);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, GX_FALSE,
	                  0x7d);
	GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, 0x3c, GX_FALSE,
	                  0x7d);

	JUTTexture texture(reinterpret_cast<const ResTIMG*>(unk94));
	texture.load(GX_TEXMAP0);

	// The register colours are passed by value, so each is copied through the
	// stack first. The retail indices are 1, 2 and 3, i.e. the colours go
	// into the three first-named registers.
	GXSetTevColorS10(static_cast<GXTevRegID>(1), mTevColor0);
	GXSetTevColorS10(static_cast<GXTevRegID>(2), mTevColor1);
	GXSetTevColorS10(static_cast<GXTevRegID>(3), mTevColor2);

	GXSetNumTevStages(2);

	// Stage 0: the distance fade written by draw() as vertex alpha.
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA, GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);

	// Stage 1: add the scrolling texture on top.
	GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP0, GX_COLOR0A0);
	GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_RASC, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_TEXA, GX_CA_APREV,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2,
	                GX_TRUE, GX_TEVPREV);

	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GXSetAlphaCompare(GX_GEQUAL, sAlphaCompLarge, GX_AOP_AND, GX_LEQUAL,
	                  sAlphaCompSmall);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);
	GXSetCullMode(GX_CULL_NONE);
}

// The four texture-coordinate helpers. The static pair share a scale, the
// moving pair share another (and the x half of the moving pair is squashed to
// 0.8 so the two layers scroll at slightly different rates).
f32 TMapObjWave::getMoveTexPos1(f32 z) const
{
	f32 scaled = z * mWaveTexScale2;
	return mTexPos1 + scaled;
}

f32 TMapObjWave::getMoveTexPos0(f32 x) const
{
	return x * mWaveTexScale2 * 0.8f;
}

f32 TMapObjWave::getStaticTexPos1(f32 z) const
{
	return z * mWaveTexScale;
}

f32 TMapObjWave::getStaticTexPos0(f32 x) const
{
	return x * mWaveTexScale;
}

// 0.15915507f is 1/2pi: it turns a world distance into a fraction of a
// wavelength before the phase is added.
f32 TMapObjWave::getWaveHeight(f32 x, f32 z) const
{
	if (!unk94)
		return 0.0f;

	f32 waveX = mAmplitude0 * sinf(mAngleSpeed0 * (0.15915507f * x) + mAngle0);
	f32 waveZ = mAmplitude1 * sinf(mAngleSpeed1 * (0.15915507f * z) + mAngle1);

	return waveX + waveZ;
}

f32 TMapObjWave::getHeight(f32 x, f32 y, f32 z) const
{
	const TBGCheckData* data;
	f32 result = gpMap->checkGroundExactY(x, 50.0f + y, z, &data);

	// The inner `else result = result;` is a no-op that keeps the ground
	// height: over a non-sea water surface the wave surface is still the
	// collision height. It is written out because retail emits an extra
	// unconditional branch for the empty else block ahead of the "not water"
	// arm, and dropping the else costs that instruction.
	if (data->isWaterSurface()) {
		if (data->isSea())
			result = getWaveHeight(x, z);
		else
			result = result;
	} else {
		result = y;
	}

	return result;
}

void TMapObjWave::noWave()
{
	unk34       = 0.0f;
	unk38       = 0.0f;
	unk2C       = 0.0f;
	unk30       = 0.0f;
	mAmplitude0 = 0.0f;
	mAmplitude1 = 0.0f;
}

// Per-vertex alpha of the wave surface: full strength along the centre line,
// fading linearly to zero at the edge of the wave span. Both coordinates are
// measured from Mario, so the result is the darker of the two edges.
s32 TMapObjWave::getAlpha(f32 x, f32 z) const
{
	if (fabsf(x) > fabsf(z))
		return mAlpha * (1.0f - mInvHalfWaveSpan * fabsf(x));
	return mAlpha * (1.0f - mInvHalfWaveSpan * fabsf(z));
}

void TMapObjWave::draw()
{
	// The surface is a triangle strip of (mWaveCount x mWaveCount) quads,
	// centred on Mario: `z` walks the rows, `x` the columns.
	for (f32 zOffset = -mHalfWaveSpan;
	     zOffset <= mHalfWaveSpan - mWaveHeight; zOffset += mWaveHeight) {
		f32 z0 = zOffset + gpMarioPos->z;
		f32 z1 = z0 + mWaveHeight;
		
		
		GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, mWaveCount * 2);

		for (f32 xOffset = -mHalfWaveSpan;
		     xOffset <= mHalfWaveSpan - mWaveHeight; xOffset += mWaveHeight) {
			f32 worldX = xOffset + gpMarioPos->x;
			s32 alpha0 = getAlpha(xOffset, zOffset);
			s32 alpha1 = getAlpha(xOffset, zOffset + mWaveHeight);

			GXPosition3f32(worldX, getWaveHeight(worldX, z0), z0);
			GXColor4u8(sColor.r, sColor.g, sColor.b, alpha0);
			GXTexCoord2f32(getStaticTexPos0(worldX) + mTexPos0,
			               getStaticTexPos1(z0));
			GXTexCoord2f32(getMoveTexPos0(worldX), getMoveTexPos1(z0));

			GXPosition3f32(worldX, getWaveHeight(worldX, z1), z1);
			GXColor4u8(sColor.r, sColor.g, sColor.b, alpha1);
			GXTexCoord2f32(getStaticTexPos0(worldX) + mTexPos0,
			               getStaticTexPos1(z1));
			GXTexCoord2f32(getMoveTexPos0(worldX), getMoveTexPos1(z1));
		}
	}
}

#pragma dont_inline on
void TMapObjWave::updateHeightAndAlpha()
{
	// Two ground probes: one at Mario's exact position (used to see whether he
	// is over water at all), and one at y = 10 which is what the shallow-water
	// height ramp is measured against.
	const TBGCheckData* ground;
	const TBGCheckData* ground2;
	
	
	gpMap->checkGround(SMS_GetMarioPos(), &ground);
	gpMap->checkGroundExactY(SMS_GetMarioPos().x, 10.0f, SMS_GetMarioPos().z,
	                         &ground2);

	if (SMS_CheckMarioFlag(MARIO_FLAG_IN_SHALLOW_WATER)
	    || ground2->isWaterSurface() || ground->isWaterSurface()) {
		f32 height = gpMap->checkGroundIgnoreWaterSurface(
		    SMS_GetMarioPos().x, 0.0f, SMS_GetMarioPos().z, &ground2);

		// Amplitude ramps from the "at the surface" pair (unk2C/unk30) towards
		// the deep-water pair (unk34/unk38) over unk4C units of depth.
		f32 height2 = unk4C + height;
		if (height2 < 0.0f || isUnk700(ground2)) {
			mAmplitude0 = unk2C;
			mAmplitude1 = unk30;
		} else {
			f32 ratio = 1.0f - height2 / unk4C;
			mAmplitude0 = ratio * (unk2C - unk34) + unk34;
			mAmplitude1 = ratio * (unk30 - unk38) + unk38;
		}

		// Same ramp for the vertex alpha, over unk50 units.
		f32 height3 = unk50 + height;
		if (height3 < 0.0f || isUnk700(ground2)) {
			mAlpha = mAlphaMax;
		} else {
			mAlpha = (1.0f - height3 / unk50) * (mAlphaMax - mAlphaMin)
			         + mAlphaMin;
		}
	} else {
		mAmplitude0 = unk34;
		mAmplitude1 = unk38;
		mAlpha      = mAlphaMin;
	}

	// One corner of Delfino Plaza has a hard-coded flat patch.
	if (gpMarDirector->mMap == 4 && -4950.0f < SMS_GetMarioPos().x
	    && -4340.0f > SMS_GetMarioPos().x && 7660.0f < SMS_GetMarioPos().z
	    && 8040.0f > SMS_GetMarioPos().z) {
		mAmplitude0 = unk34;
		mAmplitude1 = unk38;
		mAlpha      = mAlphaMin;
	}

	// Standing in a current cube slowly lifts the wave, both to get Mario's
	// attention and so he can see he is in moving water.
	int cubeNo = gpCubeStream->getInCubeNo(SMS_GetMarioPos());
	if (cubeNo != -1) {
		if (mAlphaAcc < ((TCubeStreamInfo&)(*gpCubeStream->unk14)[cubeNo]).unk3C)
			mAlphaAcc += mAlphaStep;
	} else if (mAlphaAcc > 0.0f) {
		mAlphaAcc -= mAlphaStep;
	} else {
		mAlphaAcc = 0.0f;
	}

	if (mAlphaAcc > 0.0f) {
		mAmplitude0 = unk2C + mAlphaAcc;
		mAmplitude1 = unk30 + mAlphaAcc;
	}
}
#pragma dont_inline off

#pragma dont_inline on
void TMapObjWave::updateTime()
{
	mAngle0 += mAngleSpeed0;
	if (mAngle0 > 6.2831802f)
		mAngle0 -= 6.2831802f;

	mAngle1 += mAngleSpeed1;
	if (mAngle1 > 6.2831802f)
		mAngle1 -= 6.2831802f;

	mTexPos0 += mTexSpeed;
	if (mTexPos0 > 1.0f)
		mTexPos0 -= 1.0f;

	mTexPos1 += mTexSpeed;
	if (mTexPos1 > 1.0f)
		mTexPos1 -= 1.0f;
}
#pragma dont_inline off

void TMapObjWave::movement()
{
	updateTime();
	if (gpMarDirector->mMap == 4 || gpMarDirector->mMap == 6)
		updateHeightAndAlpha();
}

void TMapObjWave::perform(u32 cue, JDrama::TGraphics* graphics)
{
	
	

	if (!unk94)
		return;

	if (cue & CUE_MOVE) {
		updateTime();
		if (gpMarDirector->mMap == 4 || gpMarDirector->mMap == 6)
			updateHeightAndAlpha();
	}

	if (cue & CUE_DRAW) {
		initDraw();
		draw();
	}
}

void TMapObjWave::load(JSUMemoryInputStream& stream)
{
	JDrama::TNameRef::load(stream);

	
	

	mWaveSpan        = 5200.0f;
	mWaveHeight      = 200.0f;
	mHalfWaveSpan    = mWaveSpan * 0.5f;
	mInvHalfWaveSpan = 1.0f / mHalfWaveSpan;
	mWaveCount       = mWaveSpan / mWaveHeight;
	unk94            = (u32)JKRFileLoader::getGlbResource("/scene/map/map/wave.bti");
	mTexSpeed        = 0.0015f;
	mWaveTexScale    = 0.0012f;
	mWaveTexScale2   = 0.0015f;
	unk4C            = 400.0f;
	unk50            = 150.0f;
	mAngleSpeed0     = 0.02f;
	mAngleSpeed1     = 0.03f;

	switch (gpMarDirector->mMap) {
	case 3:
	case 30:
		unk2C = 25.0f;
		unk30 = 20.0f;
		unk34 = 0.0f;
		unk38 = 0.0f;
		// Case 3/30 sets the amplitudes as well as the ramps; the write
		// after the switch then stores the same pair a second time.
		mAmplitude0 = unk2C;
		mAmplitude1 = unk30;
		break;
	case 4:
		unk2C = 40.0f;
		unk30 = 30.0f;
		unk34 = 5.0f;
		unk38 = 0.0f;
		break;
	case 13:
		unk2C = 30.0f;
		unk30 = 25.0f;
		unk34 = 5.0f;
		unk38 = 0.0f;
		break;
	case 9:
	case 52:
		unk2C = 10.0f;
		unk30 = 15.0f;
		unk34 = 0.0f;
		unk38 = 0.0f;
		break;
	default:
		unk2C = 30.0f;
		unk30 = 25.0f;
		unk34 = 0.0f;
		unk38 = 0.0f;
		break;
	}

	mAmplitude0 = unk2C;
	mAmplitude1 = unk30;
}

TMapObjWave* gpMapObjWave;

TMapObjWave::TMapObjWave(const char* name)
    : JDrama::TViewObj(name)
{
	mWaveSpan     = 0.0f;
	mHalfWaveSpan = 0.0f;
	mInvHalfWaveSpan = 0.0f;
	mWaveCount    = 0;
	mAngleSpeed0  = 0.0f;
	mAngleSpeed1  = 0.0f;
	unk2C         = 0.0f;
	unk30         = 0.0f;
	unk34         = 0.0f;
	unk38         = 0.0f;
	mAmplitude0   = 0.0f;
	mAmplitude1   = 0.0f;
	mAlphaAcc     = 0.0f;
	mAlphaStep    = 0.1f;
	unk4C         = 0.0f;
	unk50         = 0.0f;
	mAlpha        = 255.0f;
	mAlphaMax     = 255.0f;
	mAlphaMin     = 0.0f;
	mTexSpeed     = 0.0f;

	mAngle0 = 360.0f * MsRandF();
	mAngle1 = 360.0f * MsRandF();
	mTexPos0 = MsRandF();
	mTexPos1 = MsRandF();

	mWaveTexScale  = 0.0f;
	mWaveTexScale2 = 0.0f;
	unk94          = 0;
	unk98H         = 0;

	sColor.r = 0xc8;
	sColor.g = 0xc8;
	sColor.b = 0xff;
	sColor.a = 0;

	// 0xc2, 0xf2, 0xbe for the first Tev color register; the alpha channels
	// of the second and third hold the near/far fade endpoints.
	mTevColor0.r = 0xc2;
	mTevColor0.g = 0xf2;
	mTevColor0.b = 0xbe;
	mTevColor0.a = 0;

	mTevColor1.r = 0;
	mTevColor1.g = 0;
	mTevColor1.b = 0;
	mTevColor1.a = 0x48;

	mTevColor2.r = 0;
	mTevColor2.g = 0;
	mTevColor2.b = 0;
	mTevColor2.a = 0x90;

	gpMapObjWave = this;
}

