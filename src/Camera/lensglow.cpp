#include <Camera/LensGlow.hpp>
// Rogue include: retail's .rodata opens with this header's zero object and
// no-memory message, ahead of SunModel.hpp's volume names.
#include <System/DummyMactorString.hpp>
#include <System/DummyStrings.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DMaterialAnm.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DAnmLoader.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JMath.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Camera/Camera.hpp>
#include <Camera/CameraMarioData.hpp>
#include <Camera/SunMgr.hpp>
#include <Camera/cameralib.hpp>
#include <Camera/SunModel.hpp>
#include <System/Resolution.hpp>
#include <stdio.h>

TLensGlow::TLensGlow(bool param_1, const char* name)
    : JDrama::TViewObj(name)
    , unk10(nullptr)
    , unk14(nullptr)
    , unk18(nullptr)
    , unk1C()
    , unk30(nullptr)
    , unk34()
    , unk48(0.0f)
    , unk4C(0.0f)
    , unk50(0.5f)
    , unk54(0.5f)
    , unk58(0.5f)
    , unk5C(-1)
    , unk5D(2)
    , unk68(10.0f)
    , unk6C(10.0f)
    , unk70(0.1f)
    , unk80(0.0f)
    , unk84(0.0f)
    , unk88(0.0f)
    , unk8C(0.0f)
    , unk90(0.1f)
    , unk94(1.5f)
{
	const char* base = cSunVolumeName;
	if (param_1) {
		base  = cSunsetVolumeName;
		unk68 = 15.0f;
		unk6C = 10.0f;
	}

	// Dead 4-byte local, the same leftover declaration as in
	// TLensFlare::TLensFlare and TSunModel::load (the three sibling functions
	// that build a path out of cSunVolumeName): retail puts `buf` at 0x48 and
	// leaves one word between it and the outgoing-argument area, ours lands at
	// 0x44 without it. An assigned local is register-allocated and reserves
	// nothing, so it was never written to; the original name is unrecoverable.
	char buf[0x100];
	int pathLen;

	snprintf(buf, sizeof(buf), "%s/%s", base, "glow.bmd");
	unk10 = J3DModelLoaderDataBase::load(JKRGetResource(buf),
	                                     J3DMLF_MaterialPEFull
	                                         | (2 << J3DMLF_TevStageNumShift));
	unk14 = new J3DModel(unk10, 0, 1);

	snprintf(buf, sizeof(buf), "%s/%s", base, "glow.btk");
	unk18
	    = (J3DAnmTextureSRTKey*)J3DAnmLoaderDataBase::load(JKRGetResource(buf));
	unk18->searchUpdateMaterialID(unk10);

	snprintf(buf, sizeof(buf), "%s/%s", base, "glow.brk");
	unk30 = (J3DAnmTevRegKey*)J3DAnmLoaderDataBase::load(JKRGetResource(buf));
	unk30->searchUpdateMaterialID(unk10);

	u16 i;
	int matCount = unk10->getMaterialNum();
	for (i = 0; i < matCount; ++i) {
		J3DMaterialAnm* anm = new J3DMaterialAnm;
		unk10->getMaterialNodePointer(i)->change();
		unk10->getMaterialNodePointer(i)->setMaterialAnm(anm);
	}

	unk10->entryTexMtxAnimator(unk18);
	unk10->entryTevRegAnimator(unk30);

	unk1C.init(unk18->getFrameMax());
	unk1C.setRate(SMSGetAnmFrameRate());
	unk1C.setAttribute(J3DFrameCtrl::ATTR_LOOP);

	unk34.init(unk30->getFrameMax());
	unk34.setRate(SMSGetAnmFrameRate());
	unk34.setAttribute(J3DFrameCtrl::ATTR_LOOP);

	unk74.x = 500.0f;
	unk74.y = 500.0f;
	unk74.z = 0.0f;
	unk60 = unk64 = unk6C;
}

// TODO: 99.8%, frame exact (0x178), all instructions right. The 88 missing
// bytes of pool were the `gpSunModel` reads: a TU-local accessor that names
// and returns the pointer is ~+8 of pool per site (a 2187-way per-site sweep
// found retail's frame with every read through it except `tx`'s; with the
// screen-centre level below, `ty` is read raw too).
// Closed (c-m27): the f27-f31 rotation (cx/cy have to colour before
// dispRatio's IRO temp, which they do as locals of an inlined callee: the
// screen-centre block below), the `t` conversion pair (`t *= ...`), the avg
// f5/f6 pair (then reopened by the level), the colour-loop counter (`u16 i`
// declared ahead of matCount).
// Left: avg's zero lands in f5 and is copied to f6 (retail loads f6; ctor,
// zero(), set(), both chained orders are inert); the named block (c, mtx,
// scaleV) sits 8 high; matCount r26 vs r27 against the `i * 4` web.
static inline TSunModel* lgSunN() { TSunModel* m = gpSunModel; return m; }
// The screen-centre and target-offset block of the move cue. Its cx/cy have
// to be callee locals: as the perform's own named locals they colour after
// dispRatio's IRO temporary (regalloc), where retail has them f31/f30.
static inline void LGCalcCenter(TLensGlow* lg, u8 thing, f32 dispRatio)
{
	f32 cy, cx, b, a;
	a  = lgSunN()->unkF8[0].x;
	cx = (f32)(SMSGetGameRenderWidth() >> 1) * a;
	b  = lgSunN()->unkF8[0].y;
	cy = (f32)(SMSGetGameRenderHeight() >> 1) * b;

	if (thing == 0) {
		lg->unk8C = 0.0f;
		lg->unk88 = 0.0f;
	} else {
		if (dispRatio >= 0.5f) {
			lg->unk8C = 0.0f;
			lg->unk88 = 0.0f;
		} else {
			JGeometry::TVec2<f32> avg;
			avg.zero();

			const JGeometry::TVec2<f32>* it2 = lgSunN()->unkF8;
			const bool* it1                  = lgSunN()->unk180;
			for (int i = 0; i < 17; ++i, ++it2, ++it1)
				if (*it1)
					avg += *it2;

			f32 avgx = avg.x * (1.0f / (f32)thing);
			f32 avgy = avg.y * (1.0f / (f32)thing);

			f32 tx = CLBLinearInbetween(avgx, gpSunModel->unkF8[0].x,
			                            dispRatio * 2.0f);
			f32 ty = CLBLinearInbetween(avgy, gpSunModel->unkF8[0].y,
			                            dispRatio * 2.0f);

			u16 w  = SMSGetGameRenderWidth();
			u16 h  = SMSGetGameRenderHeight();
			f32 ax = tx * (w >> 1);
			f32 ay = ty * (h >> 1);
			lg->unk88  = ax - cx;
			lg->unk8C  = ay - cy;
		}
	}

	CLBChaseDecrease(&lg->unk80, lg->unk88, lg->unk90, 0.0f);
	CLBChaseDecrease(&lg->unk84, lg->unk8C, lg->unk90, 0.0f);

	lg->unk74.x = cx + lg->unk80;
	lg->unk74.y = cy + lg->unk84;
}

void TLensGlow::perform(u32 cue, JDrama::TGraphics* graphics)
{
	bool inBounds = false;
	if (gpCameraMario->isMarioIndoor()) {
		inBounds = false;
	} else {
		inBounds = lgSunN()->isInBounds(unk94);
	}

	if (cue & CUE_MOVE) {
		f32 dispRatio = lgSunN()->getUnk194();
		u8 thing      = lgSunN()->getUnk191();

		if (thing <= unk5D) {
			unk4C = 0.0f;
		} else {
			f32 t   = (f32)(thing - unk5D);
			t *= 1.0f / (f32)(17 - unk5D);
			f32 lin = CLBLinearInbetween(0.0f, 1.0f, t);
			unk4C   = CLBEaseOutInbetween(0.0f, (f32)unk5C, lin);
		}

		f32 ratio;
		if (unk48 < unk4C) {
			ratio = unk50;
		} else if (dispRatio == 0.0f)
			ratio = unk58;
		else
			ratio = unk54;

		CLBChaseDecrease(&unk48, unk4C, ratio, 0.0f);

		// target scale based on sun model's dispersion
		unk64 = CLBLinearInbetween(unk6C * 0.002f, unk68 * 0.002f, dispRatio);
		CLBChaseDecrease(&unk60, unk64, unk70, 0.0f);

		// Compute base screen-space center

		LGCalcCenter(this, thing, dispRatio);
	}

	if (cue & CUE_CALC_ANIM) {
		unk1C.update();
		unk34.update();

		if (inBounds) {
			JGeometry::TVec3<f32> scaleV(unk60, unk60, 1.0f);

			Mtx mtx;
			CLBCalcScaleTranslateMatrix(mtx, scaleV, unk74);
			unk14->setBaseTRMtx(mtx);
			unk14->calc();
		}
	}

	if ((cue & CUE_ENTRY) != 0 && inBounds) {
		u16 i;
		int matCount = unk10->getMaterialNum();
		for (i = 0; i < matCount; ++i) {
			J3DGXColorS10 c;
			c         = *unk10->getMaterialNodePointer(i)->getTevColor(0);
			c.color.a = (s16)unk48;
			unk10->getMaterialNodePointer(i)->setTevColor(0, &c);
		}

		unk18->setFrame(unk1C.getFrame());
		unk30->setFrame(unk34.getFrame());

		unk14->entry();
	}

	if ((cue & CUE_CALC_VIEW) && inBounds) {
		unk14->viewCalc();
	}
}
