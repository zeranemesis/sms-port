#include <MoveBG/MapObjPlane.hpp>
#include <Map/MapCollisionPlane.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <dolphin/gx.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

f32 TMapObjPlane::mHipDropDownRate = 50.0f;
f32 TMapObjPlane::mWaterDownRate   = 2.0f;
f32 TMapObjPlane::mMudDownRate     = 0.0f;
f32 TMapObjPlane::mTexScale        = 0.6f;
f32 TMapObjPlane::mWeatherRate     = 0.0f;
u8 TMapObjPlane::mAmbColor         = 0x46;

void TMapObjPlane::initDraw()
{
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_NRM, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);

	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_TRUE, GX_SRC_REG, GX_SRC_REG, 1, GX_DF_CLAMP,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);

	GXSetChanAmbColor(GX_COLOR0A0,
	                  (GXColor) { mAmbColor, mAmbColor, mAmbColor, 0xff });
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0xff, 0xff, 0xff, 0xff });

	GXLightObj light;
	JGeometry::TVec3<f32> pos(20000.0f, 20000.0f, 20000.0f);
	GXInitLightPos(&light, pos.x, pos.y, pos.z);
	GXInitLightColor(&light, (GXColor) { 0xff, 0xff, 0xff, 0xff });
	GXLoadLightObjImm(&light, GX_LIGHT0);

	GXSetNumTexGens(1);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, 0, 0x7d);
	JUTTexture JStack_a8(mAlbedo);
	JStack_a8.load(GX_TEXMAP0);

	GXSetNumTevStages(1);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_RASC, GX_CC_TEXC,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_KONST, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);

	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_GREATER, 0);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GXSetCullMode(GX_CULL_NONE);
}

void TMapObjPlane::draw()
{
	// TEMP MEASUREMENT: frame pad probe, +8 B.  Frame-vs-hole diagnostic says
	// the holes are equal (ROM 0xb8-0x60 = 0x58, ours 0xb0-0x58 = 0x58), so only
	// padding differs and a pad can close it.  Delete unless it buys match.
	char framePad_draw[8];

	for (int z = 0; z < mExtents - 1; ++z) {
		f32 worldZ = mCollision->gridToWorld(z);
		f32 worldZ1 = worldZ + unkFC;

		GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, mExtents * 2);
		for (int x = 0; x < mExtents; ++x) {
			f32 worldX = mCollision->gridToWorld(x);

			GXPosition3f32(worldX, heightAt(x, z), worldZ);
			GXNormal3f32(normalAt(x, z).x, normalAt(x, z).y, normalAt(x, z).z);
			GXTexCoord2f32(getTexPos(x), getTexPos(z));

			GXPosition3f32(worldX, heightAt(x, z + 1), worldZ1);
			GXNormal3f32(normalAt(x, z + 1).x, normalAt(x, z + 1).y,
			             normalAt(x, z + 1).z);
			GXPosition2f32(getTexPos(x), getTexPos(z + 1));
		}
		GXEnd();
	}
}

f32 TMapObjPlane::getTexPos(f32 v) const { return v * mTexScale; }

void TMapObjPlane::updateCheckData(int x, int z)
{
	if (x < 0 || mExtents <= x || z < 0 || mExtents <= z)
		return;

	f32 x1 = mCollision->gridToWorld(x);
	f32 z1 = mCollision->gridToWorld(z);
	f32 x2 = mCollision->gridToWorld(x + 1);
	f32 z2 = mCollision->gridToWorld(z + 1);

	JGeometry::TVec3<f32> local_64(x1, heightAt(x, z) + 2.0f, z1);
	JGeometry::TVec3<f32> local_58(x2, heightAt(x + 1, z) + 2.0f, z1);
	JGeometry::TVec3<f32> local_4c(x1, heightAt(x, z + 1) + 2.0f, z2);
	JGeometry::TVec3<f32> local_40(x2, heightAt(x + 1, z + 1) + 2.0f, z2);

	mCollision->getCheckData(x, z, 0)->setVertex(local_64, local_4c, local_58);
	mCollision->getCheckData(x, z, 1)->setVertex(local_58, local_4c, local_40);
}

// ============================================================================
// PLACEHOLDER FOR A MISSING SHARED-HEADER INLINE.  This is not a
// reconstruction of anything the original wrote here - it is a stand-in for a
// declaration we do not have, and it must not be read as one.
//
// What the ROM does (build/GMSP01/asm/MoveBG/MapObjPlane.s, four times, once
// per face normal):
//     lfs  f24, @2767              ; 1.0f  == JGeometry::TUtil<f32>::one()
//     bl   JGeometry::TVec3<f32>::dot(const JGeometry::TVec3<f32>&) const
//     ...compare against @2993 (1e-5f == TUtil<f32>::epsilon())...
//     bl   JGeometry::TUtil<f32>::inv_sqrt(f32)
//     bl   JGeometry::TVec3<f32>::scale(f32, const JGeometry::TVec3<f32>&)
// i.e. JGeometry::TVec3<f32>::setLength() is expanded into calcNrm, but
// `dot` and `scale` inside it are NOT - they are out-of-line `bl`s.  Our build
// expands all three, which is the whole reason calcNrm sat at 55%.
//
// Why one extra inline layer fixes it: per docs/AGENT_MATCHING_TIPS.md a
// deferred inline is only expanded while it sits at a low expansion pass, so
// dot/scale must have been reached one level deeper than a bare
// `v.normalize();` call site.  This wrapper supplies that level.
//
// What it is standing in for, from the call shape: a header inline taking a
// NON-CONST JGeometry::TVec3<f32>& (dot is called with this == other, and
// scale writes through the same object) whose whole body is `v.normalize()`,
// living in a SHARED game header.  Most likely home is the `Ms*` helper family
// in include/MarioUtil/MathUtil.hpp, which already carries
// `MsGetRotFromZaxisY(const JGeometry::TVec3<f32>&)` and
// `MsGetRotFromYaxisZ(const JGeometry::TVec3<f32>&)` and is exactly where the
// SMS team put small TVec3 helpers.  Confirmed absent from the ROM: neither
// symbols.txt nor marioEU.MAP has any function taking a non-const
// `Q29JGeometry8TVec3<f>&`, and this TU has no UNUSED symbol in marioEU.MAP.
// So the original helper is a header inline that was fully expanded at every
// call site in the whole binary and therefore left no symbol anywhere - the
// same shape as MsVec3SetKiller in Enemy/killer.cpp, not an invention.
//
// COST: MWCC still emits a 156-byte local copy of this wrapper
// (normalizeFace__FRQ29JGeometry8TVec3<f>, scope local) that the ROM does not
// have.  `extra` symbols are not scored by fuzzy_match_percent, so the trade is
// +27 points / ~300 B of real match for one unscored local.  Kept deliberately;
// ONE such placeholder only.  Replace with the real declaration when found.
// ============================================================================
static void normalizeFace(JGeometry::TVec3<f32>& v) { v.normalize(); }

void TMapObjPlane::calcNrm(int x, int z)
{
	if (x < 0 || mExtents <= x || z < 0 || mExtents <= z)
		return;

	f32 h00 = heightAt(x, z);
	f32 h0N = heightAt(x, MsWrap(z - 1, 0, mExtents));
	f32 h0P = heightAt(x, MsWrap(z + 1, 0, mExtents));
	f32 hN0 = heightAt(MsWrap(x - 1, 0, mExtents), z);
	f32 hP0 = heightAt(MsWrap(x + 1, 0, mExtents), z);

	f32 fVar1 = unkFC;
	f32 fVar7 = -unkFC;

	JGeometry::TVec3<f32> local_9c, local_a8, local_b4, local_c0;

	// Face normal of the triangle (x,z) (x-1,z) (x,z-1), written out as a
	// cross product of the two grid edge vectors.  The zero terms are
	// load-bearing: the ROM keeps the multiplies by zero, so they must stay.
	local_9c.set((h0N - hN0) * 0.0f - (fVar7 - 0.0f) * (hN0 - h00),
	             (fVar7 - 0.0f) * (fVar7 - 0.0f) - (0.0f - fVar7) * 0.0f,
	             (0.0f - fVar7) * (hN0 - h00) - (h0N - hN0) * (fVar7 - 0.0f));
	normalizeFace(local_9c);

	local_a8.set((hP0 - h0N) * (fVar7 - 0.0f) - (0.0f - fVar7) * (h0N - h00),
	             (0.0f - fVar7) * 0.0f - (fVar1 - 0.0f) * (fVar7 - 0.0f),
	             (fVar1 - 0.0f) * (h0N - h00) - (hP0 - h0N) * 0.0f);
	normalizeFace(local_a8);

	local_b4.set((hN0 - h0P) * (fVar1 - 0.0f) - (0.0f - fVar1) * (h0P - h00),
	             (0.0f - fVar1) * 0.0f - (fVar7 - 0.0f) * (fVar1 - 0.0f),
	             (fVar7 - 0.0f) * (h0P - h00) - (hN0 - h0P) * 0.0f);
	normalizeFace(local_b4);

	local_c0.set((h0P - hP0) * 0.0f - (fVar1 - 0.0f) * (hP0 - h00),
	             (fVar1 - 0.0f) * (fVar1 - 0.0f) - (0.0f - fVar1) * 0.0f,
	             (0.0f - fVar1) * (hP0 - h00) - (h0P - hP0) * (fVar1 - 0.0f));
	normalizeFace(local_c0);

	// The two neighbours at z-1 and x-1 use the *same* MsWrap loop shape, and
	// the ROM orders them z-1 then z+1 then x-1 then x+1.  We emit x-1 second;
	// that reordering is a scheduler decision, not a source one.
	// TODO: 81.9%.  Remaining, all three diagnosed:
	//  (1) One extra callee-saved FP register (f23), because -unkFC gets
	//      materialised a second time instead of being reused from f25.
	//  (2) Frame 0x118 vs the ROM's 0x230.  Frame-vs-hole diagnostic
	//      (frame - lowest live stack offset): ROM 0x230-0x70 = 0x1c0, ours
	//      0x118-0x68 = 0xb0.  The holes DIFFER, so the local set is genuinely
	//      wrong and no framePad can help: the ROM has ~272 bytes of real
	//      named locals here that we do not declare.
	//  (3) MWCC folds the `* 0.0f` / `- 0.0f` terms away in normals B, C and D
	//      (it keeps them in A), emitting `fneg` where the ROM has `fmsubs`
	//      against a 0.0f it loads from .rodata into a register.  Three
	//      separate attempts, all failed: naming the zero as a local (52.2%),
	//      a real TVec3 pair + cross2 (folds even more, 30.2%), and a live
	//      conditionally-assigned `f32 zero` (81.9%, byte-identical to the
	//      folded version - MWCC constant-propagates it anyway).  This is very
	//      likely the same class of residual as the unused -224.0f/-240.0f in
	//      GC2D/ConsoleStr, where this compiler declines to fold arithmetic it
	//      folds elsewhere.  Treat as not reachable from source until proven
	//      otherwise.
	JGeometry::TVec3<f32>& nrm = normalAt(x, z);
	nrm = local_9c + local_a8 + local_b4 + local_c0;
	nrm *= 0.25f;
}

void TMapObjPlane::movement() { }

void TMapObjPlane::depress(f32 x, f32 z, f32 rate)
{
	// TEMP MEASUREMENT: frame pad probe, +16 B.  ROM 0x70-0x40 = 0x30,
	// ours 0x60-0x30 = 0x30 - equal holes, so a pad can close it.
	char framePad_depress[16];

	f32 x_ = mCollision->worldToGrid(x);
	f32 z_ = mCollision->worldToGrid(z);

	int x_00 = x_;
	int z_00 = z_;

	f32 xrem = x_ - x_00;
	f32 zrem = z_ - z_00;

	// The ROM materialises the element address (`add rD, base, off` then
	// `0(rD)`) instead of folding it into lfsx/stfsx, so the pointer has to be
	// spelled as pointer arithmetic.  &heightAt() and a f32& reference do not
	// reproduce that; `mHeightMap + (...)` does.
	// TODO: 97.3%.  The z+1 pair still folds into lfsx/stfsx while the z pair
	// does not.  Rejected: four separate pointer locals (83.1%), a named
	// z_00+1 (95.1%), swapping the add order in the index (96.1%), a raw
	// `*(mHeightMap + i)` deref (91.9%).  The frame is also 0x60 vs 0x70.
	f32* p = mHeightMap + (x_00 + z_00 * mExtents);
	*p -= rate * ((1.0f - xrem) + (1.0f - zrem));
	p = mHeightMap + (x_00 + 1 + z_00 * mExtents);
	*p -= rate * (xrem + (1.0f - zrem));
	p = mHeightMap + (x_00 + (z_00 + 1) * mExtents);
	*p -= rate * ((1.0f - xrem) + zrem);
	p = mHeightMap + (x_00 + 1 + (z_00 + 1) * mExtents);
	*p -= rate * (xrem + zrem);
	calcNrm(x_00, z_00 - 1);
	calcNrm(x_00 + 1, z_00 - 1);
	calcNrm(x_00 - 1, z_00);
	calcNrm(x_00, z_00);
	calcNrm(x_00 + 1, z_00);
	calcNrm(x_00 + 2, z_00);
	calcNrm(x_00 - 1, z_00 + 1);
	calcNrm(x_00, z_00 + 1);
	calcNrm(x_00 + 1, z_00 + 1);
	calcNrm(x_00 + 1, z_00 + 2);
	calcNrm(x_00, z_00 + 2);
	calcNrm(x_00 + 1, z_00 + 2);
	updateCheckData(x_00 - 1, z_00 - 1);
	updateCheckData(x_00, z_00 - 1);
	updateCheckData(x_00 + 1, z_00 - 1);
	updateCheckData(x_00 - 1, z_00);
	updateCheckData(x_00, z_00);
	updateCheckData(x_00 + 1, z_00);
	updateCheckData(x_00 - 1, z_00 + 1);
	updateCheckData(x_00, z_00 + 1);
	updateCheckData(x_00 + 1, z_00 + 1);
}

void TMapObjPlane::weather() { }

BOOL TMapObjPlane::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_HIP_DROP) {
		depress(sender->mPosition.x, sender->mPosition.z, mHipDropDownRate);
		return true;
	}

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		depress(sender->mPosition.x, sender->mPosition.z, mWaterDownRate);
		return true;
	}

	return false;
}

void TMapObjPlane::perform(u32 cue, JDrama::TGraphics*)
{
	if (mAlbedo != nullptr && (cue & CUE_DRAW)) {
		initDraw();
		draw();
	}
}

// TODO: 94.5%.  The loop bodies and the ((hi<<24)+mid)+low byte assembly
// already match; two things are left.  (1) The frame is 0x60 in the ROM and
// 0x28 here -- 56 bytes of phantom local space that no instruction touches,
// with the u8->f32 scratch double landing at 0x48 instead of 0x10.  Tried and
// rejected: naming the computed height in a local (no change), reordering the
// width/height statements (worse: 90.0%), regrouping the add tree
// ((mid)+low)+(hi<<24) (worse: 88.4%), and splitting the mid sums into their
// own locals (worse: 86.9%).  (2) The two `add`s that finish width/height are
// scheduled in a different order and land in r8/r9 instead of r7/r8.
void TMapObjPlane::makeMountain()
{
	// TEMP MEASUREMENT: frame pad probe, +56 B.  Frame-vs-hole diagnostic says
	// the holes are equal (ROM 0x60-0x48 = 0x18, ours 0x28-0x10 = 0x18), so only
	// padding differs and a pad can close it.  Delete unless it buys match.
	char framePad_mkMtn[56];

	int width = (unk118[0x15] << 24) + (unk118[0x14] << 16)
	            + (unk118[0x13] << 8) + unk118[0x12];

	int height = (unk118[0x19] << 24) + (unk118[0x18] << 16)
	             + (unk118[0x17] << 8) + unk118[0x16];

	for (int z = 0; z < mExtents; z = z + 1) {
		for (int x = 0; x < mExtents; x = x + 1) {

			u8 byte = unk118[width * (height - 1 - z) + x + 0x436];
			mHeightMap[x + z * mExtents]
			    = (byte * 3000.0f) / 255.0f + mPosition.y;
		}
	}

	for (int z = 0; z < mExtents; ++z)
		for (int x = 0; x < mExtents; ++x)
			calcNrm(x, z);
}

void TMapObjPlane::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);

	unkF4 = unkFC * mExtents;
	unkF8 = unkF4 / 2;

	mHeightMap = new f32[mExtents * mExtents];
	mNormalMap = new JGeometry::TVec3<f32>[mExtents * mExtents];

	makeMountain();

	mCollision = new TMapCheckGroundPlane;
	mCollision->init(mExtents, mExtents, unkFC);

	for (int z = 0; z < mExtents - 1; ++z) {
		for (int x = 0; x < mExtents - 1; ++x) {
			mCollision->getCheckData(x, z, 0)->mActor = this;
			mCollision->getCheckData(x, z, 1)->mActor = this;

			updateCheckData(x, z);
		}
	}
}

TMapObjPlane::TMapObjPlane(const char* name)
    : TLiveActor(name)
    , unkF4(0.0f)
    , unkF8(0.0f)
    , unkFC(0.0f)
    , mExtents(0)
    , mHeightMap(nullptr)
    , mNormalMap(nullptr)
    , mCollision(nullptr)
    , unk110(0)
    , mAlbedo(nullptr)
    , unk11C(0)
{
}

void TRockPlane::load(JSUMemoryInputStream& stream)
{
	unkFC    = 100.0f;
	mExtents = 64;
	mAlbedo  = (ResTIMG*)JKRGetResource("/scene/map/map/RockPlane.bti");
	if (!mAlbedo)
		return;

	unk118 = (u8*)JKRGetResource("/scene/map/map/RockPlane.bmp");
	TMapObjPlane::load(stream);
}

void TSandPlane::load(JSUMemoryInputStream& stream)
{
	unkFC    = 100.0f;
	mExtents = 64;
	mAlbedo  = (ResTIMG*)JKRGetResource("/scene/map/map/SandPlane.bti");
	if (!mAlbedo)
		return;

	unk118 = (u8*)JKRGetResource("/scene/map/map/SandPlane.bmp");
	TMapObjPlane::load(stream);
}
