
#include <Enemy/Koopa.hpp>
#include <Player/MarioAccess.hpp>
#include <Camera/CameraShake.hpp>
#include <MoveBG/MapObjCorona.hpp>
#include <Strategic/Strategy.hpp>
#include <MSound/MSound.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MAnmSound.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <Strategic/Spine.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp followed by the four MActorMtxCalcType
// names; without them every string offset in this object is shifted.
#include <System/DummyStrings.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// The retail build calls the JGeometry header template out of line
// (`bl set<f>__Q29JGeometry8TVec3<f>Ffff`) - 14 times in this TU, 8 of them in
// TKoopa::perform, 4 in TNerveKoopaFlame::execute and 2 in
// TNerveKoopaWait::execute - while MWCC always expands it at the call site,
// which costs a materialised copy of the vector in the frame. MWCC ignores
// `#pragma dont_inline` on a *header* body, so this TU-local wrapper exists
// purely to give the call sites the right shape. Its name deliberately does NOT
// match the retail symbol (it stays "extra" in objdiff until the shared
// JGeometry header is fixed) - see the libs/ JGeometry template out-of-lining
// bug noted in src/Enemy/Kazekun.cpp.
#pragma dont_inline on
static void origVec3Set(JGeometry::TVec3<f32>* v, f32 x, f32 y, f32 z)
{
	v->set(x, y, z);
}
#pragma dont_inline off

// The ROM reaches TKoopa::getTargetDir through a real `bl` from the off-course
// flip test in TNerveKoopaFlame::execute, but MWCC's inline budget expands the
// whole body there (TNameRefGen::search + a root-joint matrix + two dots +
// matan, ~130 instructions). Same shape as origVec3Set above: the name stays
// "extra" in objdiff until the shared header is fixed.
#pragma dont_inline on
static f32 origTargetDir(const TKoopa* self, const JGeometry::TVec3<f32>& pos)
{
	return self->getTargetDir(pos);
}
#pragma dont_inline off

// The ROM calls std::fmodf out of line (`bl std::fmodf(float, float)`) at
// seven sites in this TU, but libs/PowerPC_EABI_Support's math.h models it as
// an inline wrapper around the double-precision ::fmod, so every call site
// grows an `lfd` + `frsp` pair and loses the call. Same shape-only wrapper
// trick; the name stays "extra" until that header is fixed.
#pragma dont_inline on
static f32 origFmodf(f32 x, f32 y)
{
	return std::fmodf(x, y);
}
#pragma dont_inline off

static const char* koopa_bastable[] = {
	"/scene/koopa/bas/koopa_down.bas",
	nullptr,
	"/scene/koopa/bas/koopa_fall.bas",
	"/scene/koopa/bas/koopa_fire_end.bas",
	"/scene/koopa/bas/koopa_fire_loop.bas",
	"/scene/koopa/bas/koopa_fire_start.bas",
	"/scene/koopa/bas/koopa_first.bas",
	"/scene/koopa/bas/koopa_getup.bas",
	"/scene/koopa/bas/koopa_hipdrop.bas",
	"/scene/koopa/bas/koopa_stagger.bas",
	"/scene/koopa/bas/koopa_turn_l.bas",
	"/scene/koopa/bas/koopa_turn_r.bas",
	"/scene/koopa/bas/koopa_wait.bas",
	nullptr,
	"/scene/koopa/bas/koopa_waterhit.bas",
};

// NOTE: this TU is -inline deferred, so the out-of-line functions below are
// defined in the *reverse* order of the map's .text layout.

// ============= nerves =============

// Turn towards the target angle stored in unk150. `angle` is the remaining
// delta, wrapped into [-180, 180) by std::fmodf. The two branches are the same
// body in the ROM, so they are spelled out twice here rather than sharing a
// helper (which would change the inlining shape).
//
// TODO: nonmatching (75%). The remaining differences are all library/budget
// issues, documented in detail below:
//  - the ROM calls std::fmodf and JGeometry::TUtil<f32>::mod out of line;
//    libs/JSystem's JGUtil.hpp declares `mod` on the primary TUtil template only
//    (the f32 specialisation has no such member) and
//    libs/PowerPC_EABI_Support/.../math.h models std::fmodf as an inline
//    wrapper around the double-precision ::fmod, so both sites compile to
//    `lfd <1.0>; bl fmod; frsp` instead (3 wrong instructions each).
//  - the ROM expands getSaveParam() once and calls it out of line at the other
//    four sites; we expand all five (5 wrong instructions).
//  - the ROM's epilogue is `li r0,1/0; clrlwi. r0,r0,24; beq; li r3,0/1`, a
//    bool->int conversion of a value the compiler does *not* know is 0/1. None
//    of `return TRUE/FALSE`, `bool done; return done`, `return !done` or
//    `return done == false` reproduces it (tried all four).
//  - the frame is 0xd0 vs the ROM's 0x1a8.
BOOL TNerveKoopaTurnL::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	f32 angle   = -180.0f
	           + origFmodf(360.0f + (self->unk150 - self->mRotation.y
	                                  - (-180.0f)),
	                        360.0f);
	f32 speed = self->getParams()->turnSpeed.get();

	if (angle < -speed) {
		angle = -self->getParams()->turnSpeed.get();
		if (angle > 0.0f)
			self->changeAnm(
			    0xb, 0, angle * self->getParams()->turnAnim.get());
		else
			self->changeAnm(
			    0xa, 0, -angle * self->getParams()->turnAnim.get());
		self->mRotation.y
		    = -180.0f
		      + origFmodf(360.0f
		                     + (self->mRotation.y + angle - (-180.0f)),
		                   360.0f);
		return FALSE;
	} else if (angle < 0.0f) {
		if (angle > 0.0f)
			self->changeAnm(
			    0xb, 0, angle * self->getParams()->turnAnim.get());
		else
			self->changeAnm(
			    0xa, 0, -angle * self->getParams()->turnAnim.get());
		self->mRotation.y
		    = -180.0f
		      + origFmodf(360.0f
		                     + (self->mRotation.y + angle - (-180.0f)),
		                   360.0f);
		return FALSE;
	}
	return TRUE;
}

// Mirrored version of TNerveKoopaTurnL::execute: the clamp is against
// +turnSpeed and the guard tests `angle > 0.0f` instead of `angle < 0.0f`.
// TODO: see the long note above TNerveKoopaTurnL::execute.
BOOL TNerveKoopaTurnR::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	f32 angle   = -180.0f
	           + origFmodf(360.0f + (self->unk150 - self->mRotation.y
	                                  - (-180.0f)),
	                        360.0f);
	f32 speed = self->getParams()->turnSpeed.get();

	if (angle > speed) {
		angle = self->getParams()->turnSpeed.get();
		if (angle > 0.0f)
			self->changeAnm(
			    0xb, 0, angle * self->getParams()->turnAnim.get());
		else
			self->changeAnm(
			    0xa, 0, -angle * self->getParams()->turnAnim.get());
		self->mRotation.y
		    = -180.0f
		      + origFmodf(360.0f
		                     + (self->mRotation.y + angle - (-180.0f)),
		                   360.0f);
		return FALSE;
	} else if (angle > 0.0f) {
		if (angle > 0.0f)
			self->changeAnm(
			    0xb, 0, angle * self->getParams()->turnAnim.get());
		else
			self->changeAnm(
			    0xa, 0, -angle * self->getParams()->turnAnim.get());
		self->mRotation.y
		    = -180.0f
		      + origFmodf(360.0f
		                     + (self->mRotation.y + angle - (-180.0f)),
		                   360.0f);
		return FALSE;
	}
	return TRUE;
}

// Reconstructed from the ROM at 0x801212E4. unk19C is the getup timer: while it
// is positive the koopa just holds the wait pose and returns; once it expires the
// nerve asks the bathtub where Mario is heading and picks a turn nerve from the
// sign of the remaining heading delta.
BOOL TNerveKoopaWait::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();

	if (self->unk19C > 0) {
		self->changeAnmInline(0xc, 1, self->getParams()->waitSpeed.get());
		return FALSE;
	}

	TBathtub* bathtub = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");

	// Mario's velocity, extrapolated by the "estimation" param. The ROM
	// materialises the raw speed and the scaled copy as two separate
	// temporaries, so this is spelled as two statements too, and reaches
	// TVec3::set through the TU-local out-of-line wrapper.
	JGeometry::TVec3<f32> marioSpeed;
	origVec3Set(&marioSpeed, *gpMarioSpeedX, *gpMarioSpeedY, *gpMarioSpeedZ);

	f32 waitTime = self->getParams()->marioEstimationWait.get();
	JGeometry::TVec3<f32> marioVel;
	marioVel.set(marioSpeed.x * waitTime, marioSpeed.y * waitTime,
	             marioSpeed.z * waitTime);

	u8 onCourse = bathtub->getNextGrip(*gpMarioPos, marioVel,
	                                   self->getParams()->waitRange.get(),
	                                   &self->unk150);
	if (!onCourse) {
		// Re-reads the raw speed pointers rather than reusing marioSpeed.
		JGeometry::TVec3<f32> raw;
		origVec3Set(&raw, *gpMarioSpeedX, *gpMarioSpeedY, *gpMarioSpeedZ);
		f32 fireTime = self->getParams()->marioEstimationFire.get();
		marioVel.set(raw.x * fireTime, raw.y * fireTime,
		             raw.z * fireTime);
		bathtub->getNextJuncture(*gpMarioPos, marioVel);
	}

	// Wrap the heading delta into [-180, 180) and compare against focusRange
	// to pick left / right / straight ahead.
	f32 delta = -180.0f
	            + origFmodf(360.0f + (self->unk150 - self->mRotation.y
	                                  - (-180.0f)),
	                        360.0f);
	f32 focusRange = self->getParams()->focusRange.get();
	int dir        = 0;
	if (delta < -focusRange)
		dir = -1;
	else if (delta > focusRange)
		dir = 1;

	if (onCourse) {
		switch (dir) {
		case -1:
			spine->pushNerve(&TNerveKoopaTurnL::theNerve());
			return FALSE;
		case 1:
			spine->pushNerve(&TNerveKoopaTurnR::theNerve());
			return FALSE;
		case 0: break;
		default: break;
		}
	} else {
		switch (dir) {
		case -1:
			spine->pushNerve(&TNerveKoopaTurnL::theNerve());
			return FALSE;
		case 1:
			spine->pushNerve(&TNerveKoopaTurnR::theNerve());
			return FALSE;
		case 0:
		default:
			// Off-course the flip flag is measured against the direction the
			// koopa is being asked to face, not against its own heading
			// field. The ROM captures the compare's LT bit straight out of
			// cr0 (mfcr / srwi), so this is a plain `delta < 0.0f`.
			self->unk154 = -180.0f
			               + origFmodf(360.0f
			                           + (origTargetDir(self, *gpMarioPos)
			                              - self->unk150
			                              - (-180.0f)),
			                           360.0f)
			                   < 0.0f;
			spine->setNext(&TNerveKoopaFlame::theNerve());
			return FALSE;
		}
	}

	// Staying in the wait pose: hold bck 12 / btp 1. The nerve never reports
	// TRUE: reaching one of the three trigger frames (or the animation
	// ending on bck 12) only arms the tumble check below.
	self->changeAnmInline(0xc, 1, self->getParams()->waitSpeed.get());

	// Note the direction of BOTH comparisons: the ROM spells the window test
	// `frame <= N && N <= 0.005f + frame + rate`, which lowers to
	// `fcmpo cr0,a,b` + `cror eq,lt,eq` + `bne` (skip when a > b). The three
	// arms are spelled out rather than folded into one `||` chain because the
	// ROM re-fetches the frame controller once per arm, and each one branches
	// straight to the shared tail rather than setting a flag.
	TBathtub* tub;
	if (self->mMActor->getCurAnmIdx(ANM_TYPE_BCK) != 0xc)
		return FALSE;
	if (self->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr))
		goto tumble;
	{
		J3DFrameCtrl* fc = self->mMActor->getFrameCtrl(ANM_TYPE_BCK);
		if (fc->getFrame() <= 2.0f
		    && 2.0f <= 0.005f + fc->getFrame() + fc->getRate())
			goto tumble;
	}
	{
		J3DFrameCtrl* fc = self->mMActor->getFrameCtrl(ANM_TYPE_BCK);
		if (fc->getFrame() <= 400.0f
		    && 400.0f <= 0.005f + fc->getFrame() + fc->getRate())
			goto tumble;
	}
	{
		J3DFrameCtrl* fc = self->mMActor->getFrameCtrl(ANM_TYPE_BCK);
		if (fc->getFrame() <= 700.0f
		    && 700.0f <= 0.005f + fc->getFrame() + fc->getRate())
			goto tumble;
	}
	return FALSE;

tumble:
	tub = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
	if (!tub->allowsTumble())
		return FALSE;
	spine->pushNerve(&TNerveKoopaTumble::theNerve());
	return FALSE;
}

BOOL TNerveKoopaTumble::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	self->changeAnmInline(8, 0, self->getParams()->tumbleSpeed.get());
	self->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
	if (spine->getTime() == 190) {
		gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x27), 1.0f);
		TLiveActor* bathtub
		    = (TLiveActor*)JDrama::TNameRefGen::search("バスタブ");
		gpMarioParticleManager->emitAndBindToMtx(
		    0xf5, *bathtub->getRootJointMtx(), 0, this);
		if (SMS_IsMarioTouchGround4cm())
			SMSRumbleMgr->start(1, static_cast<f32*>(nullptr));
	}
	if (self->getMActor()->curAnmEndsNext())
		return TRUE;
	return FALSE;
}

BOOL TNerveKoopaFall::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	f32 rate     = self->getParams()->fallSpeed.get();
	self->changeAnmInline(2, 0, rate);
	return FALSE;
}

// Reconstructed from the ROM at 0x801204B8. A switch on the current BCK index
// with four arms: 4 = the flame-loop pose, 3 = the flame-start pose, 5 = the
// flame-finish pose, anything else = the flame-finish pose as well. Note every
// changeAnm() below passes btp 0 - the ROM's changeBtp guard is
// `if (getCurAnmIdx(ANM_TYPE_BTP) != 0)`, i.e. `cmpwi r3,0` / `beq` followed by
// `setBtpFromIndex(0)` at all five sites.
BOOL TNerveKoopaFlame::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();

	switch (self->mMActor->getCurAnmIdx(ANM_TYPE_BCK)) {
	case 4:
		// Hold until the flame has burnt long enough, then either run the
		// heading query (the ordinary case, but only on frames where the low
		// 29 bits of the nerve timer are clear - the ROM's
		// `clrlwi r0, r4, 29` / `bne`) or take the launch jump. The named
		// local keeps MWCC from rewriting the test as `clrrwi` + `beq`.
		u32 nerveTime = (u32)spine->getTime();
		if ((s32)nerveTime < self->getParams()->flameCount.get())
			return FALSE;
		if (nerveTime & 0x1fffffff) {
			if (!self->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr))
				return FALSE;
			if ((s32)nerveTime
			    < self->getParams()->flameFocusEndStep.get())
				return FALSE;
			self->changeAnmInline(3, 0, self->getParams()->fireSpeed.get());
			return FALSE;
		}
		// The same bathtub heading query the wait nerve runs; with a nonzero
		// grip result the flame nerve is never entered.
		TBathtub* bathtub
		    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");

	JGeometry::TVec3<f32> marioSpeed;
	origVec3Set(&marioSpeed, *gpMarioSpeedX, *gpMarioSpeedY, *gpMarioSpeedZ);

	f32 waitTime = self->getParams()->marioEstimationWait.get();
	JGeometry::TVec3<f32> marioVel;
	marioVel.set(marioSpeed.x * waitTime, marioSpeed.y * waitTime,
	             marioSpeed.z * waitTime);

	u8 onCourse = bathtub->getNextGrip(*gpMarioPos, marioVel,
	                                   self->getParams()->waitRange.get(),
	                                   &self->unk150);
	if (!onCourse) {
		JGeometry::TVec3<f32> raw;
		origVec3Set(&raw, *gpMarioSpeedX, *gpMarioSpeedY, *gpMarioSpeedZ);
		f32 fireTime = self->getParams()->marioEstimationFire.get();
		marioVel.set(raw.x * fireTime, raw.y * fireTime,
		             raw.z * fireTime);
		bathtub->getNextJuncture(*gpMarioPos, marioVel);
	}

	f32 delta = -180.0f
	            + origFmodf(360.0f + (self->unk150 - self->mRotation.y
	                                  - (-180.0f)),
	                        360.0f);
	f32 focusRange = self->getParams()->focusRange.get();
	int dir        = 0;
	if (delta < -focusRange)
		dir = -1;
	else if (delta > focusRange)
		dir = 1;

		if (!onCourse || dir != 0) {
			// No nerve is pushed here: the ROM falls straight out to
			// the common `li r3, 0` after the rate is written.
			self->changeAnmInline(3, 0, self->getParams()->fireSpeed.get());
		}
		return FALSE;

	case 3:
		if (!self->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
			// One-shot fire sound, gated so it only plays once per loop.
			if (self->unk155) {
				if (gpMSound->gateCheck(0x89AD)) {
					gpMSound->startSoundSet(0x89AD, &self->unk158, 0, 0.0f,
					                        0, 0, 4);
				}
				self->unk155 = 0;
			}
			if (self->unk19C > 0) {
				// setNext, not pushNerve: the ROM's cheaper nerve swap (no
				// mVertebrae push) is what the Wait and Flame nerves use. Only
				// TurnL / TurnR / Tumble take the long pushNerve form.
				spine->setNext(&TNerveKoopaWait::theNerve());
				return FALSE;
			}
			// The same bathtub heading query again, spelled out a second time
			// because the ROM does not share it between the two arms.
			TBathtub* bathtub
			    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");

			JGeometry::TVec3<f32> marioSpeed;
			origVec3Set(&marioSpeed, *gpMarioSpeedX, *gpMarioSpeedY,
			            *gpMarioSpeedZ);

			f32 waitTime = self->getParams()->marioEstimationWait.get();
			JGeometry::TVec3<f32> marioVel;
			marioVel.set(marioSpeed.x * waitTime, marioSpeed.y * waitTime,
			             marioSpeed.z * waitTime);

			u8 onCourse = bathtub->getNextGrip(*gpMarioPos, marioVel,
			                                   self->getParams()
			                                       ->waitRange.get(),
			                                   &self->unk150);
			if (!onCourse) {
				JGeometry::TVec3<f32> raw;
				origVec3Set(&raw, *gpMarioSpeedX, *gpMarioSpeedY,
				            *gpMarioSpeedZ);
				f32 fireTime = self->getParams()->marioEstimationFire.get();
				marioVel.set(raw.x * fireTime, raw.y * fireTime,
				             raw.z * fireTime);
				bathtub->getNextJuncture(*gpMarioPos, marioVel);
			}

			f32 delta = -180.0f
			            + origFmodf(360.0f + (self->unk150 - self->mRotation.y
			                                  - (-180.0f)),
			                        360.0f);
			f32 focusRange = self->getParams()->focusRange.get();
			int dir        = 0;
			if (delta < -focusRange)
				dir = -1;
			else if (delta > focusRange)
				dir = 1;

			if (!onCourse) {
				spine->setNext(&TNerveKoopaWait::theNerve());
				return FALSE;
			}

			switch (dir) {
			case -1:
				spine->pushNerve(&TNerveKoopaTurnL::theNerve());
				return FALSE;
			case 1:
				spine->pushNerve(&TNerveKoopaTurnR::theNerve());
				return FALSE;
			case 0:
			default: break;
			}

			// The ROM measures the flip flag against the direction the koopa
			// is being asked to face and captures the compare's LT bit
			// straight out of cr0 (mfcr / srwi).
			self->unk154 = -180.0f
			               + origFmodf(360.0f
			                               + (origTargetDir(self, *gpMarioPos)
			                                  - self->unk150
			                                  - (-180.0f)),
			                           360.0f)
			                   < 0.0f;
			self->changeAnmInline(5, 0, self->getParams()->fireSpeed.get());
			spine->setNext(&TNerveKoopaFlame::theNerve());
		}
		return FALSE;

	case 5:
		// The flame-finish pose: hand back to the loop pose at a flat rate.
		if (!self->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
			self->changeAnmInline(4, 0, 2.0f);
			spine->setNext(&TNerveKoopaFlame::theNerve());
		} else {
			self->unk155 = 0;
		}
		return FALSE;

	default: break;
	}

	self->changeAnmInline(5, 0, self->getParams()->fireSpeed.get());
	return FALSE;
}

BOOL TNerveKoopaProvoke::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	self->changeAnmInline(6, 0, 2.0f);
	if (self->getMActor()->curAnmEndsNext()) {
		spine->setNext(&TNerveKoopaWait::theNerve());
		return FALSE;
	}
	return FALSE;
}

BOOL TNerveKoopaStagger::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	self->changeAnmInline(9, 0, self->getParams()->staggerSpeed.get());
	if (self->getMActor()->curAnmEndsNext())
		return TRUE;
	return FALSE;
}

BOOL TNerveKoopaGetShowered::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	self->changeAnmInline(0xe, 0, self->getParams()->waterhitSpeed.get());
	if (self->getMActor()->curAnmEndsNext())
		return TRUE;
	return FALSE;
}

BOOL TNerveKoopaGetDown::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	switch (self->getMActor()->getCurAnmIdx(ANM_TYPE_BCK)) {
	case 0:
		if (self->getMActor()->curAnmEndsNext())
			self->changeAnmInline(1, 0, self->getParams()->downSpeed.get());
		break;
	case 1: {
		TBathtub* bathtub
		    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
		f32 step = self->getParams()->downStep.get();
		if (!((f32)(spine->getTime() * (bathtub->getNumGripsDead() + 2)) < step)
		    && self->getMActor()->curAnmEndsNext())
			self->changeAnmInline(7, 0, self->getParams()->downSpeed.get());
		break;
	}
	case 7:
		if (self->getMActor()->curAnmEndsNext())
			return TRUE;
		break;
	default:
		self->changeAnmInline(0, 0, self->getParams()->downSpeed.get());
		TLiveActor* bathtub
		    = (TLiveActor*)JDrama::TNameRefGen::search("バスタブ");
		gpMarioParticleManager->emitAndBindToMtx(
		    0xf5, *bathtub->getRootJointMtx(), 0, this);
		break;
	}
	return FALSE;
}

// ============= TKoopaParts =============

TKoopaParts::TKoopaParts(const char* name, u32 actorType, TKoopa* owner,
                         f32 radius)
    : THitActor(name)
    , mOwner(owner)
{
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(this);
	initHitActor(actorType, 5, 0x88000000, radius, radius, radius, radius);
	onHitFlag(HIT_FLAG_CANNOT_ATTACK);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_UNK10000000);
	onHitFlag(HIT_FLAG_UNK8000000);
}

void TKoopaParts::perform(u32 cue, JDrama::TGraphics* graphics)
{
	THitActor::perform(cue, graphics);
	if (cue & 1) {
		control();
		for (int i = 0; i < mColCount; ++i)
			attack_(mCollisions[i]);
	}
}

void TKoopaFlame::control()
{
	if (!(unk8C < unk88)) {
		onHitFlag(HIT_FLAG_CANNOT_ATTACK);
		onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
		onHitFlag(HIT_FLAG_NO_COLLISION);
		return;
	}
	unk8C = unk8C + unk84;
	f32 x      = unk6C.x + unk78.x * unk8C;
	f32 y      = unk6C.y + unk78.y * unk8C;
	f32 z      = unk6C.z + unk78.z * unk8C;
	f32 height = unk94;
	f32 radius = unk90;
	if (height <= 0.0f)
		height = 2.0f * radius;
	mPosition.x = x;
	mPosition.y = y;
	mPosition.z = z;
	offHitFlag(HIT_FLAG_CANNOT_ATTACK);
	offHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	mAttackRadius = radius;
	mAttackHeight = height;
	mDamageRadius = radius;
	mDamageHeight = height;
	calcEntryRadius();
}

BOOL TKoopaFlame::receiveMessage(THitActor*, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER:
		return FALSE;
	default:
		return TRUE;
	}
}

void TKoopaFlame::attack_(THitActor* sender)
{
	if (sender->receiveMessage(this, HIT_MESSAGE_UNKA)
	    && sender == gpMarioAddress) {
		SMS_ThrowMario(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f),
		               mOwner->getParams()->flameJump.get());
		mOwner->unk155 = 1;
		mOwner->changeAnm(3, 0, mOwner->getParams()->fireSpeed.get());
		mOwner->unk19C = 240;
	}
}

BOOL TKoopaHand::receiveMessage(THitActor*, u32) { return TRUE; }

void TKoopaHand::attack_(THitActor* sender)
{
	sender->receiveMessage(this, HIT_MESSAGE_ATTACK);
}

BOOL TKoopaHead::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER:
		if (mOwner->getShowered()) {
			gpMarioParticleManager->emit(0xe7, &sender->mPosition, 0, nullptr);
			gpMSound->startSoundSet(0x6802, &mOwner->mPosition, 0, 0.0f, 0, 0,
			                        4);
		}
		break;
	case HIT_MESSAGE_ATTACK:
		if (sender->getActorType() == 0x8000024) {
			mOwner->stagger(false);
		}
		break;
	}
	return TRUE;
}

void TKoopaHead::attack_(THitActor* sender)
{
	if (sender->receiveMessage(this, HIT_MESSAGE_ATTACK)
	    && sender == SMS_GetMarioHitActor()) {
		SMS_ThrowMario(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f), 60.0f);
	}
}

BOOL TKoopaBody::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER:
		break;
	case HIT_MESSAGE_ATTACK:
		if (sender->getActorType() == 0x8000024) {
			mOwner->stagger(false);
		}
		break;
	}
	return TRUE;
}

void TKoopaBody::attack_(THitActor* sender)
{
	if (sender->receiveMessage(this, HIT_MESSAGE_ATTACK)
	    && sender == SMS_GetMarioHitActor()) {
		SMS_ThrowMario(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f), 60.0f);
	}
}

f32 TKoopa::getFlameDirDegree() const
{
	f32 rate = getFlameDirRate() * getParams()->flameNeckRange.get();
	return mRotation.y + (unk154 ? -rate : rate);
}

namespace {
// TODO: not yet reconstructed. Full shape recovered from the ROM at
// 0x8011E8DC (2516 B, 629 instructions), recorded here so the next attempt
// does not have to re-derive it. The two cheap hypotheses have both been
// checked and eliminated:
//
//  - Struct-return-pointer calling convention: FALSIFIED. The prologue is
//    `cmpwi r4,0` (the second argument tested against zero) and the body
//    dereferences r3+4, r3+0x18 and r3+0x38 directly, storing through r3+0x150
//    etc. There is no hidden return pointer: r3 is the J3DNode* and r4 the int,
//    and every exit is `li r3, 1` or `li r3, 0` (i.e. TRUE/FALSE).
//  - Named locals to stop fmuls+fadds contraction: not applicable until there
//    is a source shape to name; the ROM is ~100% `fmadds` chains that MWCC
//    contracts from the same expressions anyway.
//
// Structure, in order:
//   0x338C  if (param != 0) return TRUE;                 (single early out)
//   0x33F0  r31 = (TKoopa*)node->mObject;  r30 = &j3dSys->0x38->0x58[node->0x18]
//           (0x18 is a u16 index, stride 0x30 = a 3x4 matrix row, so r30 is
//           J3DSys's per-node transform table entry - 12 floats, 0x00..0x2c)
//   0x3414  a local JGeometry::TVec3 (0x1fc/0x200/0x204) is loaded from
//           *gpMarioPos, y += 85.0f, then x -= r30[0x0c], y -= r30[0x1c],
//           z -= r30[0x2c]
//   0x3474  MActor::getCurAnmIdx(0); the 3 <= idx <= 5 window decides between
//           two angle sources for the first rotation matrix
//   0x34A4  TKoopa::getFlameDirRate() * 2pi * params[0x2c0/0x2d4], negated when
//           the value is < 0.0f and again when node[0x154] != 0
//   0x3500  sinf/cosf of that angle -> two JGeometry::SMatrix34C<f32>::set
//           calls (out of line, 12 float args; the last four go on the stack -
//           see the stfs f3,8(r1) / stfs f2,0xc(r1) / stfs f1,0x10(r1) /
//           stfs f0,0x14(r1) outgoing-arg stores)
//   0x36DC  TKoopa::getNeckFocus(); three cross products of the delta with the
//           matrix axes, each normalised by an out-of-line
//           JGeometry::TUtil<f32>::inv_sqrt guarded by
//           `len2 > 3.8146973e-06f` (else the vector is zeroed), then a second
//           guarded normalisation of the delta itself
//   0x37D0  an axis-angle about r30[0x20]: atan2f, scaled by 0.5f; the
//           magnitude uses a hand-rolled `frsqrte` + 3x `fnmsubs`/`fmuls`
//           Newton iteration (this is what JGeometry::TUtil<f32>::sqrt expands
//           to out of line in the ROM - 52 B, currently `missing` here)
//   0x38A4  sinf/cosf of the half angle, normalised by inv_sqrt, into a
//           second rotation matrix
//   0x3964  the same 3..5 window again, then a cross-product + sqrt-based
//           look-rotation matrix
//   0x3B1C  a third SMatrix34C::set built from the neck/head joint rows
//   0x3CEC  PSMTXCopy(r30, J3DSys::mCurrentMtx); return TRUE;
//
// The blocker is register allocation, not the maths: the ROM keeps 17
// callee-saved FPRs live across the whole function and never re-reads a spilled
// value, which MWCC will not reproduce for a ~630-instruction straight-line
// float expression tree.
int KoopaNeckCallBack(J3DNode* node, int param) { return TRUE; }
}

void TKoopa::setUpHitActors()
{
	if (isFlameStart()) {
		int slot      = -1;
		bool tooClose = false;
		for (int i = 0; i < 10; ++i) {
			TKoopaFlame* flame = unk164[i];
			if (!(flame->unk8C < flame->unk88))
				slot = i;
			else if (flame->unk8C < 2.0f * getParams()->flameRadius.get())
				tooClose = true;
		}
		if (!tooClose && slot >= 0) {
			MtxPtr mtx = mMActor->getModel()->getAnmMtx(unk1A0);
			JGeometry::TVec3<f32> dir(mtx[0][0], 0.0f, mtx[2][0]);
			JGeometry::TVec3<f32> pos;
			pos.x = mtx[0][3];
			pos.y = mtx[1][3] - 500.0f;
			pos.z = mtx[2][3];
			dir.normalize();
			unk164[slot]->launch(pos, dir, getParams()->flameHeight.get(),
			                     getParams()->flameRadius.get(),
			                     getParams()->flameVelocity.get());
		}
	} else {
		for (int i = 0; i < 10; ++i) {
			unk164[i]->unk88 = 0.0f;
			unk164[i]->unk8C = 1.0f;
		}
	}

	MtxPtr mtx = mMActor->getModel()->getAnmMtx(unk1A8);
	f32 radius = getParams()->headRadius.get();
	f32 height = 2.0f * radius;
	unk194->mPosition.x = mtx[0][3];
	unk194->mPosition.y = mtx[1][3] - 200.0f;
	unk194->mPosition.z = mtx[2][3];
	unk194->offHitFlag(HIT_FLAG_CANNOT_ATTACK);
	unk194->offHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	unk194->offHitFlag(HIT_FLAG_NO_COLLISION);
	unk194->mAttackRadius = radius;
	unk194->mAttackHeight = height;
	unk194->mDamageRadius = radius;
	unk194->mDamageHeight = height;
	unk194->calcEntryRadius();

	unk198->mPosition.x = mPosition.x;
	unk198->mPosition.y = mPosition.y;
	unk198->mPosition.z = mPosition.z;
	unk198->offHitFlag(HIT_FLAG_CANNOT_ATTACK);
	unk198->offHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	unk198->offHitFlag(HIT_FLAG_NO_COLLISION);
	unk198->mAttackRadius = 800.0f;
	unk198->mAttackHeight = 2000.0f;
	unk198->mDamageRadius = 800.0f;
	unk198->mDamageHeight = 2000.0f;
	unk198->calcEntryRadius();
}

// The ROM reaches this out of line from TNerveKoopaTurnL/TurnR and
// TKoopaFlame::attack_, so it must not be inlined; the other call sites use the
// identical-body changeAnmInline() from the header instead.
#pragma dont_inline on
void TKoopa::changeAnm(int bck, int btp, f32 rate)
{
	changeBck(bck);
	changeBtp(btp);
	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
	ctrl->setRate(0.5f * (rate * SMSGetAnmFrameRate()));
}
#pragma dont_inline off

f32 TKoopa::getFlameDirRate() const
{
	f32 frame  = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	f32 end    = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getEnd();
	f32 range  = getParams()->flameOverStart.get();
	int start  = getParams()->flameFocusStartStep.get();
	int finish = getParams()->flameFocusEndStep.get();
	int time   = mSpine->getTime();
	int idx    = mMActor->getCurAnmIdx(ANM_TYPE_BCK);
	switch (idx) {
	case 5:
		return -(frame * range / end);
	case 3:
	case 4: {
		f32 t;
		if (mSpine->getTime() <= start)
			t = -range;
		else if (time > finish)
			t = 1.0f;
		else
			t = (1.0f + range) * (f32)(time - start) / (f32)(finish - start)
			    - range;
		if (idx == 3)
			t *= 1.0f - frame / end;
		return t;
	}
	default:
		return 0.0f;
	}
}

BOOL TKoopa::isFlaming() const
{
	switch (mMActor->getCurAnmIdx(0)) {
	case 3:
	case 4:
	case 5:
		return TRUE;
	default:
		return FALSE;
	}
}

f32 TKoopa::getNeckFocus() const
{
	int idx            = mMActor->getCurAnmIdx(ANM_TYPE_BCK);
	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
	f32 end            = ctrl->getEnd();
	f32 frame          = ctrl->getFrame();
	f32 focus          = 1.0f;
	switch (idx) {
	case 6:
		focus = 0.0f;
		if (frame >= 164.0f)
			focus = (frame - 164.0f) / (end - 164.0f);
		break;
	case 2:
		focus = 0.0f;
		break;
	case 0:
		if (frame <= 40.0f)
			focus = 1.0f - frame / 40.0f;
		else
			focus = 0.0f;
		break;
	case 1:
		focus = 0.0f;
		break;
	case 7:
		if (frame <= 125.0f)
			focus = 0.0f;
		else
			focus = (frame - 125.0f) / (end - 125.0f);
		break;
	case 9:
		if (frame <= 30.0f)
			focus = 1.0f - frame / 30.0f;
		else if (frame <= 65.0f)
			focus = 0.0f;
		else
			focus = (frame - 65.0f) / (end - 65.0f);
		break;
	case 8:
		if (frame <= 30.0f)
			focus = 1.0f - frame / 30.0f;
		else if (frame <= 170.0f)
			focus = 0.0f;
		else
			focus = (frame - 170.0f) / (end - 170.0f);
		break;
	case 14:
		if (frame <= 20.0f)
			focus = 1.0f - frame / 20.0f;
		else if (frame <= 40.0f)
			focus = 0.0f;
		else
			focus = (frame - 40.0f) / (end - 40.0f);
		break;
	case 4:
		focus = 0.0f;
		break;
	case 5:
		if (frame <= 103.0f)
			focus = 1.0f - frame / 103.0f;
		else
			focus = 0.0f;
		break;
	case 3:
		focus = frame / end;
		break;
	case 12:
		if (frame <= 200.0f) {
		} else if (frame <= 255.0f)
			focus = 1.0f - (frame - 200.0f) / 55.0f;
		else if (frame <= 330.0f)
			focus = 0.0f;
		else if (frame <= 390.0f)
			focus = (frame - 330.0f) / 60.0f;
		else if (frame <= 440.0f) {
		} else if (frame <= 480.0f)
			focus = 1.0f - (frame - 440.0f) / 40.0f;
		else if (frame <= 555.0f)
			focus = 0.0f;
		else if (frame <= 615.0f)
			focus = (frame - 555.0f) / 60.0f;
		break;
	}
	return focus;
}

BOOL TKoopa::allowsLaunch() const
{
	return &TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve() ? FALSE : TRUE;
}

void TKoopa::getDown()
{
	if (&TNerveKoopaFall::theNerve() == mSpine->getCurrentNerve())
		return;
	if (&TNerveKoopaProvoke::theNerve() == mSpine->getCurrentNerve())
		return;
	if (&TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve())
		return;
	if (&TNerveKoopaStagger::theNerve() == mSpine->getCurrentNerve())
		mSpine->setNext(&TNerveKoopaGetDown::theNerve());
	if (&TNerveKoopaGetShowered::theNerve() == mSpine->getCurrentNerve())
		mSpine->setNext(&TNerveKoopaGetDown::theNerve());
	mSpine->pushNerve(&TNerveKoopaGetDown::theNerve());
}

BOOL TKoopa::effectsTumble() const
{
	if (&TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve()) {
		s32 time = mSpine->getTime();
		if (time < 900 && time > 190)
			return TRUE;
	}
	return FALSE;
}

bool TKoopa::getShowered()
{
	if (&TNerveKoopaFall::theNerve() == mSpine->getCurrentNerve())
		return false;
	if (&TNerveKoopaProvoke::theNerve() == mSpine->getCurrentNerve())
		return false;
	if (&TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve())
		return false;
	if (&TNerveKoopaGetDown::theNerve() == mSpine->getCurrentNerve())
		return false;
	if (&TNerveKoopaGetShowered::theNerve() == mSpine->getCurrentNerve())
		return true;
	if (&TNerveKoopaStagger::theNerve() == mSpine->getCurrentNerve()) {
		mSpine->setNext(&TNerveKoopaGetShowered::theNerve());
		return true;
	}
	if (&TNerveKoopaFlame::theNerve() == mSpine->getCurrentNerve()) {
		mSpine->setNext(&TNerveKoopaWait::theNerve());
		return false;
	}
	mSpine->pushNerve(&TNerveKoopaGetShowered::theNerve());
	return true;
}

void TKoopa::stagger(bool ignoreFlame)
{
	if (&TNerveKoopaFall::theNerve() != mSpine->getCurrentNerve()
	    && &TNerveKoopaProvoke::theNerve() != mSpine->getCurrentNerve()
	    && (ignoreFlame
	        || &TNerveKoopaFlame::theNerve() != mSpine->getCurrentNerve())
	    && &TNerveKoopaTumble::theNerve() != mSpine->getCurrentNerve()
	    && &TNerveKoopaGetDown::theNerve() != mSpine->getCurrentNerve()
	    && &TNerveKoopaGetShowered::theNerve() != mSpine->getCurrentNerve())
		mSpine->pushNerve(&TNerveKoopaStagger::theNerve());
}

f32 TKoopa::getTargetDir(const JGeometry::TVec3<f32>& pos) const
{
	TLiveActor* bathtub = (TLiveActor*)JDrama::TNameRefGen::search("バスタブ");
	Mtx* m              = bathtub->getRootJointMtx();
	JGeometry::TVec3<f32> origin((*m)[0][3], (*m)[1][3], (*m)[2][3]);
	JGeometry::TVec3<f32> d;
	JGeometry::TVec3<f32> axisX((*m)[0][0], (*m)[1][0], (*m)[2][0]);
	JGeometry::TVec3<f32> axisZ((*m)[0][2], (*m)[1][2], (*m)[2][2]);
	JGeometry::TVec3<f32> axisY((*m)[0][1], (*m)[1][1], (*m)[2][1]);
	d.sub(pos, origin);
	f32 z = axisZ.dot(d);
	f32 x = axisX.dot(d);
	return 0.005493164f * matan(z, x);
}

void TKoopa::fall()
{
	mRotation.y = 180.0f;
	mSpine->setNext(&TNerveKoopaFall::theNerve());
}

void TKoopa::updateAnmSound()
{
	if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) == 8) {
		unk158.x = mPosition.x;
		unk158.y = mPosition.y;
		unk158.z = mPosition.z;
	} else {
		MtxPtr mtx = mMActor->getModel()->getAnmMtx(unk1A0);
		JGeometry::TVec3<f32> pos;
		pos.z = mtx[2][3];
		pos.y = mtx[1][3];
		pos.x = mtx[0][3];
		unk158.set(pos);
	}
	if (mAnmSound && mAnmSoundPath) {
		J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
		mAnmSound->animeLoop(&unk158, ctrl->getFrame(), ctrl->getRate(), 0, 4);
	}
}

// ============= TKoopa =============

TKoopa::TKoopa(const char* name)
    : TSpineEnemy(name)
{
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	offLiveFlag(LIVE_FLAG_UNK100);
	onLiveFlag(LIVE_FLAG_UNK10);
}

// The ROM looks up a TNameRef and pulls an out-of-line base matrix out of it
// (the same search()+getRootJointMtx() pair getTargetDir uses), builds a
// rotation-only matrix from mRotation.y, `concat`s the two and takes the
// result's translation row, offset along the joint's Y axis by -1500.
void TKoopa::calcRootMatrix()
{
	TLiveActor* joint = (TLiveActor*)JDrama::TNameRefGen::search("neck");
	Mtx* jointMtx      = joint->getRootJointMtx();
	JGeometry::SMatrix34C<f32>& jm = *(JGeometry::SMatrix34C<f32>*)jointMtx;

	// The three -1500-scaled joint-Y values are computed before the matrix
	// work and kept in callee-saved f31/f30/f29 across the MsMtxSetRotRPH and
	// set() calls, so they must be named before the rotation is built.
	f32 neckX = (*jointMtx)[0][1] * -1500.0f;
	f32 neckY = (*jointMtx)[1][1] * -1500.0f;
	f32 neckZ = (*jointMtx)[2][1] * -1500.0f;

	JGeometry::TMatrix34<JGeometry::SMatrix34C<f32> > mtx;
	MsMtxSetRotRPH((MtxPtr)&mtx, 0.0f, mRotation.y, 0.0f);
	mtx.ref(0, 3) = mtx.ref(1, 3) = mtx.ref(2, 3) = 0.0f;

	// joint * mtx, written back over mtx. Spelled out rather than using
	// TMatrix34::concat, and that is NOT fixable from include/: concat lives
	// in libs/JSystem/include/JSystem/JGeometry/JGMatrix34.hpp, and its 2-arg
	// form computes sum_k a(k,i)*b(j,k) - i.e. a-transpose * b - with a 4th
	// column of a(0,0)*b(0,3) + a(1,0)*b(3,1) + a(2,0)*b(3,2) + a(3,0). The
	// ROM here computes the plain row-by-row product with the 4th column
	// sum_k a(i,k)*b(k,3) + a(i,3), so no overload or respelling of the
	// existing helper can produce it: the operand order is transposed and the
	// 4th-column terms index different elements. An overload in our own layer
	// would need a new name anyway (the member is already found), and the
	// remaining ~1.3pp gap here is register allocation, not the expression
	// tree. (The retail row-2 bug question raised alongside this is still
	// undecided; it does not affect this function, whose 4th column is
	// correct.) The twelve joint
	// values are named so the multiply keeps them in callee-saved FPRs, as in
	// the ROM; letting them be re-read from the joint matrix shrinks the live
	// set instead and costs ~1.3pp here.
	f32 j0 = jm.at(0, 0), j1 = jm.at(0, 1), j2 = jm.at(0, 2), j3 = jm.at(0, 3);
	f32 j4 = jm.at(1, 0), j5 = jm.at(1, 1), j6 = jm.at(1, 2), j7 = jm.at(1, 3);
	f32 j8 = jm.at(2, 0), j9 = jm.at(2, 1), ja = jm.at(2, 2), jb = jm.at(2, 3);
	mtx.set(j0 * mtx.at(0, 0) + j1 * mtx.at(1, 0) + j2 * mtx.at(2, 0),
	        j0 * mtx.at(0, 1) + j1 * mtx.at(1, 1) + j2 * mtx.at(2, 1),
	        j0 * mtx.at(0, 2) + j1 * mtx.at(1, 2) + j2 * mtx.at(2, 2),
	        j0 * mtx.at(0, 3) + j1 * mtx.at(1, 3) + j2 * mtx.at(2, 3) + j3,
	        j4 * mtx.at(0, 0) + j5 * mtx.at(1, 0) + j6 * mtx.at(2, 0),
	        j4 * mtx.at(0, 1) + j5 * mtx.at(1, 1) + j6 * mtx.at(2, 1),
	        j4 * mtx.at(0, 2) + j5 * mtx.at(1, 2) + j6 * mtx.at(2, 2),
	        j4 * mtx.at(0, 3) + j5 * mtx.at(1, 3) + j6 * mtx.at(2, 3) + j7,
	        j8 * mtx.at(0, 0) + j9 * mtx.at(1, 0) + ja * mtx.at(2, 0),
	        j8 * mtx.at(0, 1) + j9 * mtx.at(1, 1) + ja * mtx.at(2, 1),
	        j8 * mtx.at(0, 2) + j9 * mtx.at(1, 2) + ja * mtx.at(2, 2),
	        j8 * mtx.at(0, 3) + j9 * mtx.at(1, 3) + ja * mtx.at(2, 3) + jb);

	mPosition.set(mtx.ref(0, 3), mtx.ref(1, 3), mtx.ref(2, 3));
	mPosition.x += neckX;
	mPosition.y += neckY;
	mPosition.z += neckZ;

	mtx.ref(0, 3) = mPosition.x;
	mtx.ref(1, 3) = mPosition.y;
	mtx.ref(2, 3) = mPosition.z;
	PSMTXCopy((MtxPtr)&mtx, mMActor->getModel()->getBaseTRMtx());

	JGeometry::TVec3<f32> unitScale(1.0f, 1.0f, 1.0f);
	mMActor->getModel()->setBaseScale(unitScale);
	mScaling.setAll(1.0f);
}

void TKoopa::load(JSUMemoryInputStream& stream) { TSpineEnemy::load(stream); }

void TKoopa::loadAfter()
{
	JDrama::TNameRef::loadAfter();
	for (int i = 0; i < 10; ++i)
		unk164[i] = new TKoopaFlame("クッパの吐く炎", 0x8000029, this, 100.0f);
	for (int i = 0; i < 2; ++i)
		unk18C[i] = new TKoopaHand("クッパの手", 0x800002b, this, 100.0f);
	unk194 = new TKoopaHead("クッパの頭", 0x800002a, this, 100.0f);
	unk198 = new TKoopaBody("クッパの体", 0x800002a, this, 100.0f);
}

void TKoopa::init(TLiveManager* manager)
{
	mBodyRadius = 800.0f;
	mHeadHeight = 2000.0f;
	TSpineEnemy::init(manager);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	offHitFlag(HIT_FLAG_CANNOT_ATTACK);
	mSpine->initWith(&TNerveKoopaProvoke::theNerve());
	changeAnmInline(12, 1, 2.0f);
	changeAnmInline(6, 0, 2.0f);
	mMActor->initSimpleMotionBlend(16);
	unk150 = getTargetDir(*gpMarioPos);
	initAnmSound();
	reset();
	JUTNameTab* names = getModel()->getModelData()->getJointName();
	unk1A8            = names->getIndex("ago");
	unk1A0            = names->getIndex("head");
	unk1A4            = names->getIndex("neck");
	J3DJoint* head    = getModel()->getModelData()->getJointNodePointer(unk1A0);
	head->setCallBack(KoopaNeckCallBack);
	head->setCallBackUserData(this);
	unk1B8 = 1.0f;
	unk155 = 0;
}

const char** TKoopa::getBasNameTable() const { return koopa_bastable; }

void TKoopa::reset()
{
	TSpineEnemy::reset();
	changeAnmInline(0xc, 1, getParams()->waitSpeed.get());
	unk19C = 600;
}

void TKoopa::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & 1) {
		unk1B8 = getNeckFocus();
		if (unk19C > 0)
			unk19C = unk19C - 1;
	}
	TSpineEnemy::perform(cue, graphics);
	for (int i = 0; i < 10; ++i)
		unk164[i]->perform(cue, graphics);
	unk194->perform(cue, graphics);
	unk18C[0]->perform(cue, graphics);
	unk18C[1]->perform(cue, graphics);
	unk198->perform(cue, graphics);
	if (cue & 1) {
		f32 frame = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
		bool afterStart = false;
		bool beforeEnd  = false;
		if (&TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve()
		    && frame >= getParams()->tumbleStartFrame.get())
			afterStart = true;
		if (afterStart && frame <= getParams()->tumbleEndFrame.get())
			beforeEnd = true;
		if (beforeEnd) {
			TBathtub* bathtub
			    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
			bathtub->tumble(mRotation.y, getParams()->tumbleWeight.get());
		}
		setUpHitActors();
	}
	if (cue & 2) {
		if (!isFlameStart() && !isFlameEnd())
			return;
		mMActor->calc();
		f32 scale = getParams()->flameScale.get();
		JGeometry::TVec3<f32> scaleVec;
		scaleVec.set(scale, scale, scale);
		JPABaseEmitter* emitter;
		emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    KOOPA_JPA_MS_KP_FIRE_E, mMActor->getModel()->getAnmMtx(unk1A0), 3,
		    this);
		if (emitter)
			emitter->setGlobalScale(scaleVec);
		emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    KOOPA_JPA_MS_KP_FIRE_D, mMActor->getModel()->getAnmMtx(unk1A0), 1,
		    this);
		if (emitter)
			emitter->setGlobalScale(scaleVec);
		emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    KOOPA_JPA_MS_KP_FIRE_C, mMActor->getModel()->getAnmMtx(unk1A0), 1,
		    this);
		if (emitter)
			emitter->setGlobalScale(scaleVec);
		emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    KOOPA_JPA_MS_KP_FIRE_B, mMActor->getModel()->getAnmMtx(unk1A0), 1,
		    this);
		if (emitter)
			emitter->setGlobalScale(scaleVec);
		emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    KOOPA_JPA_MS_KP_FIRE_A, mMActor->getModel()->getAnmMtx(unk1A0), 1,
		    this);
		if (emitter)
			emitter->setGlobalScale(scaleVec);
	}
}

BOOL TKoopa::receiveMessage(THitActor* sender, u32 message)
{
	return TSpineEnemy::receiveMessage(sender, message);
}

TKoopaManager::TKoopaManager(const char* name)
    : TEnemyManager(name)
{
}

TSpineEnemy* TKoopaManager::createEnemyInstance() { return nullptr; }

void TKoopaManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "koopa_model.bmd", 0x14240000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TKoopaManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
	unk38 = new TKoopaParams("/enemy/koopa.prm");
}

void TKoopaManager::loadAfter()
{
	TEnemyManager::loadAfter();
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_a.jpa", 0x1c0);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_b.jpa", 0x1c1);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_c.jpa", 0x1c2);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_d.jpa", 0x1c3);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_hipdrop.jpa", 0xf5);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_e.jpa", 0x1f3);
}
