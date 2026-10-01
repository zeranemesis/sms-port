
#include <MoveBG/MapObjMonte.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// rand() only: TSwingBoard::load() converts its result straight to f32,
// which is the double lowering, not MsRandF()'s single fmuls.
#include <stdlib.h>

#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/WaterGun.hpp>
#include <Player/Yoshi.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>

void TMapObjMonteRoot::initMapObj()
{
	TMapObjBase::initMapObj();
	f32 damageHeight = 1400.0f * mScaling.y;
	mDamageHeight = damageHeight;
	calcEntryRadius();
	f32 y = mInitialPosition.y + mYOffset;
	mPosition.y = y;
}

BOOL TJumpMushroom::receiveMessage(THitActor*, unsigned long)
{
	startAnim(1);
	return TRUE;
}

void TJumpMushroom::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	int value;
	stream.read(&value, 4);
	if (mMapCollisionManager) {
		mMapCollisionManager->getUnk8()->setAllData(value);
	}
}

void THangingBridgeBoard::calcDefaultMtx()
{
	Mtx rotX;
	Mtx rotY;
	makeRootMtxRotX(rotX);
	makeRootMtxRotY(rotY);
	PSMTXConcat(rotY, rotX, rotY);
	mDefaultMtx.set(rotY);
	mVelocity.y = 0.0f;
	mPosition.y = mInitialPosition.y;
}

void THangingBridgeBoard::setGroundCollision()
{
	if (SMS_GetYoshi()->isHatched()
	    && mPosition.x - mBodyRadius < SMS_GetYoshi()->getTranslation().x
	    && mPosition.x + mBodyRadius > SMS_GetYoshi()->getTranslation().x
	    && mPosition.z - mBodyRadius < SMS_GetYoshi()->getTranslation().z
	    && mPosition.z + mBodyRadius > SMS_GetYoshi()->getTranslation().z) {
		// TODO: 99.9% - frame 0x48 vs target 0x40. Naming `col` shrinks the frame
		// to 0x40 but allocates it to r0 (extra mr r3,r0); leaving it unnamed
		// puts it in r3 (exact instructions) but adds an 8-byte temp slot.
		J3DModel* model = getModel();
		MtxPtr anmMtx = model->getAnmMtx(0);
		if (mMapCollisionManager->getUnk8()) {
			mMapCollisionManager->getUnk8()->moveMtx(anmMtx);
		}
	} else {
		TMapObjBase::setGroundCollision();
	}
}

void THangingBridgeBoard::initMapObj()
{
	TLeanBlock::initMapObj();
	unk140 = 0.01f;
	unk144 = 0.02f;
	unk148 = 0.08f;
}

THangingBridgeBoard::THangingBridgeBoard(const char* name)
    : TLeanBlock(name)
{
	unk1BC = nullptr;
	unk194 = 0;
	unk198 = 0;
	unk19C = 0;
	unk1A0 = 0;
	unk1A4[0].zero();
	unk1A4[1].zero();
}

// Same reason as the THangingBridge helpers below: perform() calls this twice
// per board and the ROM keeps it out of line.
#pragma dont_inline on
// One rope segment: two GX quads spanning the board's mRopeWidthX either side
// in x and mRopeWidthZ in z, mRopeHeight tall, textured along y by
// mTexPosRate. In scenario 0xD the lower edge is dropped by 60 units.
void THangingBridgeBoard::drawOneRope(const JGeometry::TVec3<f32>& pos) const
{
	f32 top = pos.y + THangingBridge::mRopeHeight;
	f32 bot = pos.y;
	f32 right = pos.x + THangingBridgeBoard::mRopeWidthX;
	f32 left = pos.x - THangingBridgeBoard::mRopeWidthX;
	f32 front = pos.z + THangingBridgeBoard::mRopeWidthZ;
	f32 back = pos.z - THangingBridgeBoard::mRopeWidthZ;
	if (gpMarDirector->mMap == 0xD) {
		bot -= 60.0f;
	}
	f32 vTop = THangingBridgeBoard::mTexPosRate * (top - pos.y);
	f32 vBot = THangingBridgeBoard::mTexPosRate * (bot - pos.y);
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 8);
	GXPosition3f32(pos.x, top, front);
	GXTexCoord2f32(0.0f, vTop);
	GXPosition3f32(pos.x, bot, front);
	GXTexCoord2f32(0.0f, vBot);
	GXPosition3f32(left, top, back);
	GXTexCoord2f32(1.0f, vTop);
	GXPosition3f32(left, bot, back);
	GXTexCoord2f32(1.0f, vBot);
	GXPosition3f32(right, top, back);
	GXTexCoord2f32(2.0f, vTop);
	GXPosition3f32(right, bot, back);
	GXTexCoord2f32(2.0f, vBot);
	GXPosition3f32(pos.x, top, front);
	GXTexCoord2f32(3.0f, vTop);
	GXPosition3f32(pos.x, bot, front);
	GXTexCoord2f32(3.0f, vBot);
}
#pragma dont_inline off

void THangingBridgeBoard::drawRopes() const {}

void THangingBridgeBoard::push(f32) {}

void THangingBridgeBoard::pushNeighbor(f32) {}

// Physics for one plank: Mario's weight and hip attack both accelerate the
// board (and, through the owner's accel rates, its two side neighbours), then
// the swing velocity is integrated into the board's y offset and damped.
void THangingBridgeBoard::control()
{
	TLeanBlock::control();
	// Each neighbour is nudged by accel * the owner's own accel rate. Naming
	// the product keeps MWCC from contracting the pair into one fnmsubs.
	f32 accelY = THangingBridgeBoard::mMarioAccelY;
	f32 hipY = THangingBridgeBoard::mMarioHipDropAccelY;
	if (!marioIsOn()) {
		goto neighbourPass;
	}
	mVelocity.y -= THangingBridgeBoard::mMarioAccelY;
	if (unk194) {
		f32 d = accelY * unk1BC->unk40;
		unk194->mVelocity.y -= d;
	}
	if (unk19C) {
		f32 d = accelY * unk1BC->unk44;
		unk19C->mVelocity.y -= d;
	}
neighbourPass:
	if (unk198) {
		f32 d = accelY * unk1BC->unk40;
		unk198->mVelocity.y -= d;
	}
	if (unk1A0) {
		f32 d = accelY * unk1BC->unk44;
		unk1A0->mVelocity.y -= d;
	}
	if (marioHipAttack()) {
		mVelocity.y -= THangingBridgeBoard::mMarioHipDropAccelY;
		if (unk194) {
			f32 d = hipY * unk1BC->unk40;
			unk194->mVelocity.y -= d;
		}
		if (unk19C) {
			f32 d = hipY * unk1BC->unk44;
			unk19C->mVelocity.y -= d;
		}
		if (unk198) {
			f32 d = hipY * unk1BC->unk40;
			unk198->mVelocity.y -= d;
		}
		if (unk1A0) {
			f32 d = hipY * unk1BC->unk44;
			unk1A0->mVelocity.y -= d;
		}
	}
	// Integrate and damp.
	mPosition.y += mVelocity.y;
	// Spring back towards the rope's rest length, then damp.
	f32 pull = mInitialPosition.y - mPosition.y;
	mVelocity.y = mVelocity.y + THangingBridgeBoard::mReturnAccelRate * pull;
	mVelocity.y = mVelocity.y * THangingBridgeBoard::mSpeedDownRate;
	// The four rope points follow the board's own rotation, so they sit on a
	// circle of radius unk1BC->unk3C about the plank's centre.
	MtxPtr mtx = getModel()->getAnmMtx(0);
	unk1A4[0].x = mPosition.x - mtx[0][0] * unk1BC->unk3C;
	unk1A4[0].y = mPosition.y + 70.0f - mtx[1][0] * unk1BC->unk3C;
	unk1A4[0].z = mPosition.z - mtx[2][0] * unk1BC->unk3C;
	unk1A4[1].x = mPosition.x + mtx[0][0] * unk1BC->unk3C;
	unk1A4[1].y = mPosition.y + 70.0f + mtx[0][0] * unk1BC->unk3C;
	unk1A4[1].z = mPosition.z + mtx[2][0] * unk1BC->unk3C;
}

f32 THangingBridge::mRopeWidthBetweenBoards = 0.0f;
f32 THangingBridge::mRopeWidthBetweenBoardsY = 0.0f;
int THangingBridge::mPointNumBetweenBoards = 0;
f32 THangingBridge::mBetweenBoardsTexPosRate = 0.0f;
f32 THangingBridge::mRopeHeight = 0.0f;

// The drawing helpers below are still empty stubs. MWCC happily inlines an
// empty body, which erases every `bl` the ROM actually emits from perform().
// dont_inline keeps the call sites; it goes away once the bodies are real.
#pragma dont_inline on

// One sub-segment of the rope between boards, "minus" strand: n sub-segments,
// each stepping from `a` towards `b` by 1/n, and each emitting two vertices
// of a triangle strip. unk38[i] is the per-index vertical sag.
void THangingBridge::drawLowerMinus(const JGeometry::TVec3<f32>& a,
                                    const JGeometry::TVec3<f32>& b,
                                    const JGeometry::TVec2<f32>& uv,
                                    int n) const
{
	f32 ax = a.x, ay = a.y, az = a.z;
	f32 step = 1.0f / (f32)n;
	f32 dx = step * (b.x - a.x);
	f32 dy = step * (b.y - a.y);
	f32 dz = step * (b.z - a.z);
	f32 rate = THangingBridge::mBetweenBoardsTexPosRate;
	f32 widthY = THangingBridge::mRopeWidthBetweenBoardsY;
	for (int i = 0; i < n; ++i) {
		f32 y = ay - unk38[i];
		f32 v = rate * (ax + az);
		GXPosition3f32(ax - uv.x, y, az - uv.y);
		GXTexCoord2f32(0.0f, v);
		GXPosition3f32(ax, y - widthY, az);
		GXTexCoord2f32(1.0f, v);
		ax += dx;
		ay += dy;
		az += dz;
	}
}

// "plus" strand: same stepping as drawLowerMinus(), but the second vertex of
// each pair goes off the uv offset instead of down widthY.
void THangingBridge::drawLowerPlus(const JGeometry::TVec3<f32>& a,
                                   const JGeometry::TVec3<f32>& b,
                                   const JGeometry::TVec2<f32>& uv,
                                   int n) const
{
	f32 ax = a.x, ay = a.y, az = a.z;
	f32 step = 1.0f / (f32)n;
	f32 dx = step * (b.x - a.x);
	f32 dy = step * (b.y - a.y);
	f32 dz = step * (b.z - a.z);
	f32 rate = THangingBridge::mBetweenBoardsTexPosRate;
	f32 widthY = THangingBridge::mRopeWidthBetweenBoardsY;
	for (int i = 0; i < n; ++i) {
		f32 y = ay - unk38[i];
		f32 v = rate * (ax + az);
		GXPosition3f32(ax, y - widthY, az);
		GXTexCoord2f32(0.0f, v);
		GXPosition3f32(ax + uv.x, y, az + uv.y);
		GXTexCoord2f32(1.0f, v);
		ax += dx;
		ay += dy;
		az += dz;
	}
}

// Upper strand: both vertices sit at the same height (no widthY drop), and the
// second one flips the uv offset's sign.
void THangingBridge::drawUpper(const JGeometry::TVec3<f32>& a,
                               const JGeometry::TVec3<f32>& b,
                               const JGeometry::TVec2<f32>& uv,
                               int n) const
{
	f32 ax = a.x, ay = a.y, az = a.z;
	f32 step = 1.0f / (f32)n;
	f32 dx = step * (b.x - a.x);
	f32 dy = step * (b.y - a.y);
	f32 dz = step * (b.z - a.z);
	f32 rate = THangingBridge::mBetweenBoardsTexPosRate;
	for (int i = 0; i < n; ++i) {
		f32 y = ay - unk38[i];
		f32 v = rate * (ax + az);
		GXPosition3f32(ax + uv.x, y, az + uv.y);
		GXTexCoord2f32(0.0f, v);
		GXPosition3f32(ax - uv.x, y, az - uv.y);
		GXTexCoord2f32(1.0f, v);
		ax += dx;
		ay += dy;
		az += dz;
	}
}

void THangingBridge::setDrawPos(int, f32, JGeometry::TVec3<f32>*) const {}

// Six triangular strips make up the rope that spans the gap between the first
// and the last board: the three strands (lower -z, lower +z, upper) for the
// +offset column of rope points, then the same three for the -offset column.
//
// Each strip walks the board list, drawing a quad between the running point
// and each board's rope point in turn, then closes with two more quads at the
// far end so the rope reaches the bridge's own anchor point.
void THangingBridge::drawRopeBetweenBoards(f32 dy, int n) const
{
	// Cross-section of the rope, in board-local units scaled by unk3C.
	f32 ox = unk30 * unk3C;
	f32 oz = unk34 * unk3C;
	// The ROM's frame is 0x108 with its local block ending at 0xcc, so it has
	// 8 bytes of (unreferenced) local above `uv` that we have to reproduce to
	// get both the frame size and every slot offset right. Unnamed, so it
	// costs no instructions.
	char localPad_top[8];
	(void)localPad_top;
	// The rope's texture coordinate pair, used by all six strips.
	JGeometry::TVec2<f32> uv;
	uv.x = unk30;
	uv.y = unk34;
	uv.x *= mRopeWidthBetweenBoards;
	uv.y *= mRopeWidthBetweenBoards;
	// unk10 boards plus the two closing quads, repeated n times over.
	u16 count = (u16)(((int)unk10 + 2) * n);
	JGeometry::TVec3<f32> v0;
	JGeometry::TVec3<f32> v1;
	// Local-slot padding: the ROM's v1/v0/uv sit at 0xac/0xb8/0xc4, ours sit
	// 0x80 lower. Unnamed, so it costs no instructions.
	char localPad_80[0x80];
	(void)localPad_80;

	// --- strip 1: lower rope, -z side, +offset ---
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, count);
	v0.x = unk18.x + ox;
	v0.y = unk18.y + dy;
	v0.z = unk18.z + oz;
	for (int i = 0; i < (int)unk10; ++i) {
		v1 = unk14[i]->unk1A4[0];
		v1.y += dy;
		drawLowerMinus(v0, v1, uv, n);
		v0 = v1;
	}
	v1.x = unk24.x + ox;
	v1.y = unk24.y + dy;
	v1.z = unk24.z + oz;
	drawLowerMinus(v0, v1, uv, n);
	v0 = v1;
	drawLowerMinus(v0, v1, uv, n);

	// --- strip 2: lower rope, +z side, +offset ---
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, count);
	v0.x = unk18.x + ox;
	v0.y = unk18.y + dy;
	v0.z = unk18.z + oz;
	for (int i = 0; i < (int)unk10; ++i) {
		v1 = unk14[i]->unk1A4[0];
		v1.y += dy;
		drawLowerPlus(v0, v1, uv, n);
		v0 = v1;
	}
	v1.x = unk24.x + ox;
	v1.y = unk24.y + dy;
	v1.z = unk24.z + oz;
	drawLowerPlus(v0, v1, uv, n);
	v0 = v1;
	drawLowerPlus(v0, v1, uv, n);

	// --- strip 3: upper rope, +offset ---
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, count);
	v0.x = unk18.x + ox;
	v0.y = unk18.y + dy;
	v0.z = unk18.z + oz;
	for (int i = 0; i < (int)unk10; ++i) {
		v1 = unk14[i]->unk1A4[0];
		v1.y += dy;
		drawUpper(v0, v1, uv, n);
		v0 = v1;
	}
	v1.x = unk24.x + ox;
	v1.y = unk24.y + dy;
	v1.z = unk24.z + oz;
	drawUpper(v0, v1, uv, n);
	v0 = v1;
	drawUpper(v0, v1, uv, n);

	// --- strip 4: lower rope, -z side, -offset ---
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, count);
	v0.x = unk18.x - ox;
	v0.y = unk18.y + dy;
	v0.z = unk18.z - oz;
	for (int i = 0; i < (int)unk10; ++i) {
		v1 = unk14[i]->unk1A4[1];
		v1.y += dy;
		drawLowerMinus(v0, v1, uv, n);
		v0 = v1;
	}
	v1.x = unk24.x - ox;
	v1.y = unk24.y + dy;
	v1.z = unk24.z - oz;
	drawLowerMinus(v0, v1, uv, n);
	v0 = v1;
	drawLowerMinus(v0, v1, uv, n);

	// --- strip 5: lower rope, +z side, -offset ---
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, count);
	v0.x = unk18.x - ox;
	v0.y = unk18.y + dy;
	v0.z = unk18.z - oz;
	for (int i = 0; i < (int)unk10; ++i) {
		v1 = unk14[i]->unk1A4[1];
		v1.y += dy;
		drawLowerPlus(v0, v1, uv, n);
		v0 = v1;
	}
	v1.x = unk24.x - ox;
	v1.y = unk24.y + dy;
	v1.z = unk24.z - oz;
	drawLowerPlus(v0, v1, uv, n);
	v0 = v1;
	drawLowerPlus(v0, v1, uv, n);

	// --- strip 6: upper rope, -offset ---
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, count);
	v0.x = unk18.x - ox;
	v0.y = unk18.y + dy;
	v0.z = unk18.z - oz;
	for (int i = 0; i < (int)unk10; ++i) {
		v1 = unk14[i]->unk1A4[1];
		v1.y += dy;
		drawUpper(v0, v1, uv, n);
		v0 = v1;
	}
	v1.x = unk24.x - ox;
	v1.y = unk24.y + dy;
	v1.z = unk24.z - oz;
	drawUpper(v0, v1, uv, n);
	v0 = v1;
	drawUpper(v0, v1, uv, n);
}

void THangingBridge::initDraw() const {}

void THangingBridge::perform(u32 cue, JDrama::TGraphics*)
{
	// The ROM's `beq` after the rlwinm. goes straight to the epilogue, so the
	// bit test guards the whole body, not just initDraw().
	//
	// MWCC 1.2.5e encodes a test of bit b as `rlwinm rD, rS, 0, 31-b, 31-b`
	// -- the mask is the *complement* of the bit index. The ROM's single
	// `rlwinm. r0, r4, 0, 28, 28` therefore tests bit 31-28 = 3, and
	// `CUE_DRAW = 0x8` (libs/JSystem/include/JSystem/JDrama/JDRViewObj.hpp:16)
	// is that bit. This was previously written as `cue & 0x10000000`, which
	// was a misreading of the mask as if it were the bit index: it emitted
	// mb=me=3 and gated drawing on an unrelated cue bit.
	if (!(cue & CUE_DRAW)) {
		return;
	}
	initDraw();
	// One local, reused: the ROM reloads the same sp+0x34 slot for both calls.
	JGeometry::TVec3<f32> vec;
	// Frame padding: the ROM's frame is 0x50, ours 0x30, and every local slot
	// is 0x20 below where the ROM puts it.
	char framePad_20_perform[0x20];
	(void)framePad_20_perform;
	for (int i = 0; i < (int)unk10; ++i) {
		THangingBridgeBoard* board = unk14[i];
		vec = board->unk1A4[0];
		board->drawOneRope(vec);
		vec = board->unk1A4[1];
		board->drawOneRope(vec);
	}
	// ROM reads 0x7c(gpMarDirector) here, i.e. mMap, not mState (0x64).
	if (gpMarDirector->mMap == 0xD) {
		drawRopeBetweenBoards(-60.0f, mPointNumBetweenBoards);
	} else {
		drawRopeBetweenBoards(0.0f, mPointNumBetweenBoards);
	}
	drawRopeBetweenBoards(mRopeHeight, 1);
}

void THangingBridge::loadAfter() {}

void THangingBridge::initMonte() {}

THangingBridge::THangingBridge(const char* name)
    : TViewObj(name)
{
	unk10 = 0;
	unk14 = 0;
	unk38 = 0;
	unk3C = 0.0f;
	unk40 = 0.0f;
	unk44 = 0.0f;
}

// One rope segment running from `a` to `b`, drawn as two quads. The row on
// the +z side is textured with unk138 * mTexPosRate, the -z row gets a
// constant zero v coordinate.
void TSwingBoard::drawOneRope(const JGeometry::TVec3<f32>& a,
                              const JGeometry::TVec3<f32>& b) const
{
	f32 v = unk138 * TSwingBoard::mTexPosRate;
	f32 w = TSwingBoard::mRopeWidthX;
	f32 bx = b.x, ax = a.x, bz = b.z, az = a.z;
	f32 br = bx + w, ar = ax + w;
	f32 d = TSwingBoard::mRopeWidthZ;
	f32 bf = bz + d, bb = bz - d, af = az + d, ab = az - d;
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 8);
	GXPosition3f32(ax, a.y, af);
	GXTexCoord2f32(0.0f, v);
	GXPosition3f32(bx, b.y, bf);
	GXTexCoord2f32(0.0f, 0.0f);
	GXPosition3f32(ar, a.y, ab);
	GXTexCoord2f32(1.0f, v);
	GXPosition3f32(br, b.y, bb);
	GXTexCoord2f32(1.0f, 0.0f);
	GXPosition3f32(ax - w, a.y, ab);
	GXTexCoord2f32(2.0f, v);
	GXPosition3f32(bx - w, b.y, bb);
	GXTexCoord2f32(2.0f, 0.0f);
	GXPosition3f32(ax, a.y, af);
	GXTexCoord2f32(3.0f, v);
	GXPosition3f32(bx, b.y, bf);
	GXTexCoord2f32(3.0f, 0.0f);
}

void TSwingBoard::initDraw() const {}

// Two drawOneRope() calls, one per side of the board: the first rope runs
// from the board's current position to its saved one, the second mirrors it
// about the board's local origin so the pair straddles the plank.
void TSwingBoard::draw() const
{
	initDraw();
	MtxPtr mtx = getModel()->getAnmMtx(0);
	f32 w = TSwingBoard::mBoardWidth;
	// Saved end of the rope: the initial position, offset by the board's
	// world-space x/z axis and lifted by unk138.
	JGeometry::TVec3<f32> a;
	a.x = mInitialPosition.x + w * mtx[0][0];
	a.y = mInitialPosition.y + unk138;
	a.z = mInitialPosition.z + w * mtx[2][0];
	// Live end: same construction from the current position.
	JGeometry::TVec3<f32> b;
	b.x = mPosition.x + w * mtx[0][0];
	b.y = mPosition.y + 60.0f;
	b.z = mPosition.z + w * mtx[2][0];
	drawOneRope(a, b);
	a.x = mInitialPosition.x - w * mtx[0][0];
	a.z = mInitialPosition.z - w * mtx[2][0];
	b.x = mPosition.x - w * mtx[0][0];
	b.z = mPosition.z - w * mtx[2][0];
	drawOneRope(a, b);
}

void TSwingBoard::swing() {}

// Swing physics. Mario standing on the board accelerates it away from the
// water gun's nozzle axis; then a spring/damper pair keeps it swinging, and
// the plank's position is recomputed from the swing matrix each frame.
void TSwingBoard::control()
{
	TMapObjBase::control();
	if (marioIsOn() && marioIsOn()
	    && SMS_GetMarioWaterGun()->mIsEmitWater != 0) {
		// The water jet pushes the board along the nozzle's negated x/z.
		MtxPtr emit = SMS_GetMarioWaterGun()->getEmitMtx(0);
		f32 nx = -emit[0][0];
		f32 nz = -emit[0][2];
		MtxPtr mtx = getModel()->getAnmMtx(0);
		unk144 = unk140 + (mtx[0][3] * nx + mtx[1][3] * nz) * unk144;
	}
	// Spring towards the rest angle.
	unk13C = unk13C + unk144;
	f32 vel = unk144;
	f32 rest = unk13C;
	unk144 = rest - TSwingBoard::mReturnAccelRate * vel;
	// Hard limit: past mSpeedDownRate the swing velocity is clamped to that
	// fraction of the current angle.
	if (fabs(unk144) > TSwingBoard::mSpeedDownRate) {
		unk144 = unk144 * TSwingBoard::mSpeedDownRate;
	}
	// The swing has come back to vertical: stop the creak sound if it is
	// playing, otherwise start the rising/falling variant by sign.
	if (!(vel * unk144 != 0.0f)) {
		if (unk188) {
			unk188->stop(1);
		}
	} else if (unk144 > 0.0f) {
		f32 pitch = fabs(unk13C);
		if (gpMSound->gateCheck(0x3867)) {
			MSoundSESystem::MSoundSE::startSoundActorWithInfo(
			    0x3867, &mPosition, nullptr, pitch, 0, 0, &unk188, 0, 4);
		}
	} else {
		f32 pitch = fabs(unk13C);
		if (gpMSound->gateCheck(0x3868)) {
			MSoundSESystem::MSoundSE::startSoundActorWithInfo(
			    0x3868, &mPosition, nullptr, pitch, 0, 0, &unk188, 0, 4);
		}
	}
	// Recompute the plank's world position from its swing matrix, then copy
	// it into the model's node matrices for rendering.
	unk140 = -unk13C;
	f32 s = sinf(3.14f * (unk140 / 180.0f));
	f32 c = cosf(3.14f * (unk140 / 180.0f));
	// A RotZ by the current swing angle, composed onto the swing matrix.
	Mtx axis;
	axis[0][0] = 1.0f;
	axis[0][1] = 0.0f;
	axis[0][2] = 0.0f;
	axis[0][3] = 0.0f;
	axis[1][0] = 0.0f;
	axis[1][1] = c;
	axis[1][2] = s;
	axis[1][3] = 0.0f;
	axis[2][0] = 0.0f;
	axis[2][1] = -s;
	axis[2][2] = c;
	axis[2][3] = 0.0f;
	axis[3][0] = 0.0f;
	axis[3][1] = 0.0f;
	axis[3][2] = 0.0f;
	axis[3][3] = 1.0f;
	MtxPtr mtx = getModel()->getAnmMtx(0);
	PSMTXConcat(mMatrix, axis, mtx);
	f32 c2 = cosf(3.14f * (unk13C / 180.0f));
	f32 s2 = sinf(3.14f * (unk13C / 180.0f));
	mPosition.x = mInitialPosition.x - mtx[0][1] * unk138;
	mPosition.y = mInitialPosition.y + unk138 - mtx[1][1] * unk138;
	mPosition.z = mInitialPosition.z - mtx[2][1] * unk138;
	mtx[0][3] = mPosition.x;
	mtx[1][3] = mPosition.y;
	mtx[2][3] = mPosition.z;
}

void TSwingBoard::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	stream.read(&unk138, 4);
	if (unk138 == -1.0f) {
		unk138 = 5000.0f;
	}
	stream.read(&unk140, 4);
	if (unk140 > 10.0f || unk140 == 0.0f) {
		unk140 = 0.003f;
	}
	// unk17C is the board's rest orientation; its y picks up the rope's
	// current height so the board hangs at the right level.
	unk17C.x = mPosition.x;
	unk17C.y = mPosition.y + unk138;
	unk17C.z = mPosition.z;
	// rand() as a float goes through a double (MWCC's int->float lowering),
	// so this is NOT MsRandF(), which would be a single fmuls.
	const f32 kInv = 0.000030517578f;
	const f32 r1 = (f32)rand();
	f32 t148 = kInv * r1;
	t148 = t148 + 1.0f;
	unk148 = (t148 * 0.5f) * 0.05f;
	const f32 r2 = (f32)rand();
	f32 t13C = kInv * r2;
	unk13C = (t13C - 0.5f) * 20.0f;
	if (unk13C > 0.0f) {
		unk144 = unk148 * -(kInv * (f32)rand());
	} else {
		unk144 = unk148 * (kInv * (f32)rand());
	}
	// The swing matrix is built in place: MsMtxSetRotY() inlined with a
	// destination of mMatrix. The trailing stores are MsMtxSetRotY's own
	// constant writes, not separate statements.
	MsMtxSetRotY(mMatrix, 182.04445f * mRotation.y);
}

#pragma dont_inline off

TSwingBoard::TSwingBoard(const char* name)
    : TMapObjBase(name)
{
	unk138 = 5000.0f;
	unk13C = 0.0f;
	unk140 = 0.0f;
	unk144 = 0.0f;
	unk148 = 0.0f;
	unk188 = 0;
	mMatrix[2][3] = 0.0f;
	mMatrix[1][3] = 0.0f;
	mMatrix[0][3] = 0.0f;
	mMatrix[1][2] = 0.0f;
	mMatrix[0][2] = 0.0f;
	mMatrix[2][1] = 0.0f;
	mMatrix[0][1] = 0.0f;
	mMatrix[2][0] = 0.0f;
	mMatrix[1][0] = 0.0f;
	mMatrix[2][2] = 1.0f;
	mMatrix[1][1] = 1.0f;
	mMatrix[0][0] = 1.0f;
	unk17C.z = 0.0f;
	unk17C.y = 0.0f;
	unk17C.x = 0.0f;
}

void TGoalFlag::touchActor(THitActor* actor)
{
	if (actor->isActorType(0x80000001)) {
		if (!TFlagManager::getInstance()->getBool(0x00050005)) {
			TFlagManager::getInstance()->setBool(true, 0x00050005);
		}
		actor->receiveMessage(this, HIT_MESSAGE_ATTACK);
	} else if (actor->isActorType(0x08000002)) {
		actor->receiveMessage(this, HIT_MESSAGE_ATTACK);
	}
}

void TGoalFlag::initMapObj() { TMapObjBase::initMapObj(); }

u32 TFluff::touchWater(THitActor* actor)
{
	const JGeometry::TVec3<f32>& waterPos = getWaterPos(actor);
	JGeometry::TVec3<f32> normal;
	getNormalVecFromTarget(waterPos.x, waterPos.y, waterPos.z, &normal);
	mVelocity.x -= normal.x * unk160;
	mVelocity.y -= normal.y * unk160;
	mVelocity.z -= normal.z * unk160;
	return 1;
}

void TFluff::move() {}

void TFluff::kill()
{
	if (mHeldObject) {
		mHeldObject->receiveMessage(this, HIT_MESSAGE_UNK8);
		mHeldObject->mHolder = nullptr;
		mHeldObject = nullptr;
	}
	setState(3);
}

void TFluff::control() {}

void TFluff::appear() {}

void TFluff::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = 300.0f;
	unk13C = 0.5f;
}

TFluff::TFluff(const char* name)
    : TMapObjBase(name)
{
	unk138 = 0.0f;
	unk13C = 0.0f;
	unk140 = 0.0f;
	unk144 = 0.0f;
	unk148 = 0.0f;
	unk14C = 0.0f;
	unk150 = 0.0f;
	unk160 = 1.0f;
	unk164 = 0.95f;
	unk168 = nullptr;
	unk16C = 0;
	unk15C = 0.0f;
	unk158 = 0.0f;
	unk154 = 0.0f;
}

void TFluffManager::findNextFluff() {}

void TFluffManager::control() {}

void TFluffManager::registerNextFluff(TFluff*) {}

void TFluffManager::setUpNextFluff() {}

void TFluffManager::newFluff(const char*) {}

f32 TFluffManager::getRandomX() const { return 0.0f; }

f32 TFluffManager::getRandomZ() const { return 0.0f; }

void TFluffManager::loadAfter() {}

void TFluffManager::load(JSUMemoryInputStream&) {}

TFluffManager::TFluffManager(const char* name)
    : TMapObjBase(name)
{
	unk138 = 0.0f;
	unk13C = 0.0f;
	unk140 = 0.0f;
	unk144 = 0;
	unk154 = 0.0f;
	unk158 = 0;
	unk15C = 0;
	unk160 = 0;
	unk164 = 0;
	unk148.x = 0.0f;
	unk148.y = 0.0f;
	unk148.z = 0.0f;
}
