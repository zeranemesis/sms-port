#include <System/DummyMactorString.hpp>
#include <System/DummyStrings.hpp>
#include <Camera/LensFlare.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JMath.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Camera/Camera.hpp>
#include <Camera/CameraMarioData.hpp>
#include <Camera/SunMgr.hpp>
#include <Camera/cameralib.hpp>
#include <Camera/SunModel.hpp>
#include <stdio.h>

// Parked copy of cameralib.hpp's fovy/aspect CLBCalcNearNinePos wrapper with
// the near-plane height as a level of its own (fabricated split). Retail calls
// JMASSin/JMASCos out of line from the calc-anim block, i.e. fakeTan's two
// table lookups sit at depth 5 there, while CPolarSubCamera::calcInHouseNo_
// calls the wrapper at depth 0 and expands them; one more level inside the
// wrapper satisfies both. The call itself is written in calcAnim() with all
// eight arguments: retail evaluates the camera reads and getFinalAngleZ()
// before the two `Vec` -> `TVec3` conversions (right to left), and creates the
// conversion temporaries in calcAnim's own expansion, above its locals.
static inline f32 LensNearHeight(f32 near_dist, f32 fovy)
{
	s16 halfFovyShort = CLBDegToShortAngle(0.5f * fovy);
	return 2.0f * (near_dist * fakeTan(halfFovyShort));
}

static inline void LensCalcNearNinePos(JGeometry::TVec3<f32>* out_grid,
                                       S16Vec* out_euler,
                                       const JGeometry::TVec3<f32>& origin,
                                       const JGeometry::TVec3<f32>& lookat,
                                       s16 roll, f32 near_dist, f32 fovy,
                                       f32 aspect)
{
	JGeometry::TVec2<f32> nearSize;
	nearSize.y = LensNearHeight(near_dist, fovy);
	nearSize.x = nearSize.y * aspect;
	CLBCalcNearNinePos(out_grid, out_euler, origin, lookat, roll, near_dist,
	                   nearSize);
}

TLensFlare::TLensFlare(const char* name)
    : JDrama::TViewObj(name)
    , unk10(nullptr)
    , unk14(nullptr)
    , unk18(60.0f, 60.0f, 80.0f)
    , unk24(0.0f)
    , unk28(0.0f)
    , unk2C(0.04f)
    , unk30(0.005f)
    , unk34(0.04f)
    , unk38(0.04f)
    , unk3C(30000.0f)
    , unk40(3.0f)
    , unk44(1.75f)
    , unk48(75.0f)
{
	if (gpSunMgr->isThing())
		return;

	// Dead 4-byte local. Retail puts `buf` at 0x18 and leaves one word between
	// it and the outgoing-argument area; without a local declared after `buf`
	// ours lands at 0x14 (every other instruction is identical). The same +4
	// shows up in TLensGlow::TLensGlow and TSunModel::load, the two sibling
	// functions that build a path out of cSunVolumeName, so this is a leftover
	// declaration in that idiom rather than something the function uses.
	// TODO: the original name is unrecoverable; an assigned local is register
	// allocated and does not reserve the slot, so it was never written to.
	char buf[0x100];
	int pathLen;

	snprintf(buf, 0x100, "%s/%s", cSunVolumeName, "sun_lensfx.bmd");

	unk10 = J3DModelLoaderDataBase::load(JKRGetResource(buf),
	                                     J3DMLF_MaterialPEFull
	                                         | (2 << J3DMLF_TevStageNumShift));
	unk14 = new J3DModel(unk10, 0, 1);
}

static inline void LensSetTRS(Mtx mtx, const Vec& t,
                              const JGeometry::TVec3<f32>& r,
                              const JGeometry::TVec3<f32>& s)
{
	// The three angles are named: that is what makes retail load the 0.0f
	// and the conversion constants before `s.z`.
	s16 rx   = CLBDegToShortAngle(r.x);
	s16 ry   = CLBDegToShortAngle(r.y);
	f32 degX = rx * (360.0f / 65536.0f);
	f32 degY = ry * (360.0f / 65536.0f);
	f32 degZ = 0.0f;
	MsMtxSetTRS(mtx, t.x, t.y, t.z, degX, degY, degZ, s.x, s.y, s.z);
}

// perform's cue blocks are inline members (fabricated names): retail lays
// out the calc-anim objects in reverse source order below the ones perform
// itself creates, as an inlined callee's.
inline void TLensFlare::move()
{
	if (!gpSunModel->isInBounds(unk44)) {
		unk28 = 0.0f;
	} else {
		int hiddenCount = 0;
		const JGeometry::TVec2<s16>* zBuffer = gpSunModel->unkB4;
		const bool* visible               = gpSunModel->unk180;
		for (int i = 0; i < 17; ++i, ++zBuffer, ++visible) {
			if (zBuffer->x != -1 && zBuffer->y != -1 && !*visible)
				++hiddenCount;
		}
		f32 hiddenRatio = hiddenCount * (1.0f / 17.0f);
		f32 start       = unk48 * (1.0f - hiddenRatio);

		unk28 = CLBEaseOutInbetween<f32>(start, 255.0f,
		                                 gpSunModel->getUnk194());
	}

	f32 chase;
	if (unk24 < unk28) {
		if (gpSunModel->unk194 == 0.0f)
			chase = unk30;
		else
			chase = unk2C;
	} else {
		if (gpSunModel->unk194 == 0.0f)
			chase = unk38;
		else
			chase = unk34;
	}
	CLBChaseDecrease(&unk24, unk28, chase, 0.0f);
}

inline void TLensFlare::calcAnim()
{
	Mtx mtx;
	Vec sunWorldPos = gpSunModel->unk198;

	JGeometry::TVec3<f32> near9grid[9];
	S16Vec camEuler;
	LensCalcNearNinePos(near9grid, &camEuler, gpCamera->getUnk124Vec(),
	                    gpCamera->getUnk148Vec(),
	                    gpCamera->getFinalAngleZ(), gpCamera->getNear(),
	                    gpCamera->getFovy(), gpCamera->getAspect());

	const JGeometry::TVec2<f32>& sp = gpSunModel->unkF8[0];
	f32 tx = unk3C * -sp.x;
	f32 ty = unk3C * -sp.y;
	JGeometry::TVec3<f32> d5;
	d5.sub(near9grid[5], near9grid[4]);
	d5.scale(tx);
	JGeometry::TVec3<f32> d1;
	d1.sub(near9grid[1], near9grid[4]);
	d1.scale(ty);
	JGeometry::TVec3<f32> l;
	l.add(near9grid[4], d5);
	l.add(d1);
	// TODO: the lerp is two scaled difference vectors added to grid[4]
	// (products and sums as separate fmuls/fadds, no fmadds); the sun
	// position is a plain `Vec` copy (lwz/stw). Left: (a) in move()'s
	// isInBounds expansion the result/pointer GPRs rotate (retail r4/r5/r3,
	// ours r3/r4/r5; the f0/f1 swap closed by naming `x`; c-k17 dump: the
	// `position` pointer is an IRO CSE temporary (@634) created after the two
	// `&&` value temporaries (@588/@589), and regalloc --search fixes the
	// rotation by colouring it first, so retail's pointer was an object
	// created before them); (b) closed; (c) closed by c-k17's named angles;
	// (d) the frame is 0x80 short (0x238 vs 0x2b8). c-k17 put every mapped
	// object in retail's order (hsearch dbg: order 0, from 4): the return
	// temporary, the sun conversion, the two camera conversions, then `rot`,
	// `dir`, d5/d1/l, camEuler, the grid, the sun, the matrix. Retail still has
	// 3 more words between the two camera conversions and the pool above
	// them, 2 between the unk124 conversion and `rot`, 6 between entry()'s
	// colour copy and the 8-byte object below it, and 21 below everything we
	// map. The camera points are read through Camera.hpp's `Vec`-typed
	// accessors (research c-r26: the members themselves are TVec3, since the
	// camera's own TUs pass their addresses straight to `TVec3` parameters);
	// that is instruction-identical to the old `(const Vec&)` casts and 0x18
	// closer on frame. `SMSGetCamera()->` for the two reads is 8 closer again
	// with the same instructions, not taken as a frame-only binder.
	JGeometry::TVec3<f32> dir;
	dir.sub(l, JGeometry::TVec3<f32>(sunWorldPos));
	JGeometry::TVec3<f32> rot = MsGetRotFromZaxis(dir);
	LensSetTRS(mtx, sunWorldPos, rot, unk18);
	unk14->setBaseTRMtx(mtx);
	unk14->calc();
}

inline void TLensFlare::entry()
{
	u16 i;
	int matCount = unk10->getMaterialNum();
	for (i = 0; i < matCount; ++i) {
		unk10->getMaterialNodePointer(i)->change();
		J3DGXColorS10 c;
		c         = *unk10->getMaterialNodePointer(i)->getTevColor(0);
		c.color.a = unk24;
		unk10->getMaterialNodePointer(i)->setTevColor(0, &c);
	}
	unk14->entry();
}

void TLensFlare::perform(u32 cue, JDrama::TGraphics*)
{
	if (gpSunMgr->isThing())
		return;

	bool sunInBounds;
	if (gpCameraMario->isMarioIndoor()) {
		sunInBounds = false;
	} else {
		sunInBounds = gpSunModel->isInBounds(unk40);
	}

	if (cue & CUE_MOVE)
		move();

	if (!sunInBounds)
		return;

	if (cue & CUE_CALC_ANIM)
		calcAnim();

	if (cue & CUE_ENTRY)
		entry();

	if (cue & CUE_CALC_VIEW)
		unk14->viewCalc();
}
