#include <Camera/Camera.hpp>
#include <Camera/CameraKindParam.hpp>
#include <Camera/CameraMarioData.hpp>
#include <Camera/camerasave.hpp>
#include <Camera/cameralib.hpp>
#include <Enemy/Conductor.hpp>
#include <Strategic/LiveManager.hpp>
#include <Player/Mario.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/WaterGun.hpp>
#include <System/MarioGamePad.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JMath.hpp>
#include <Enemy/Enemy.hpp>

static const char* dummyMactorStringValue1 = "\0\0\0\0\0\0\0\0\0\0\0";
static const char* SMS_NO_MEMORY_MESSAGE   = "メモリが足りません\n";

static const char* sNoticeActorManagerName[] = {
	"ヒノクリマネージャー",
	"ヒノクリ２マネージャー",
	"ボスパックンマネージャー",
	nullptr,
};

const char* bossGesoViewObjName = "ボスゲッソー";

// Both notice-distance sites read MathUtil.hpp's MsSquaredDist, whose three
// products stay apart (`fmuls` x3 + `fadds` x2) where
// JGeometry::TVec3<f32>::squared(const TVec3&) contracts them into `fmadds`;
// the helper has to be its own inline level. Earlier trials on
// getNoticeActor_: three named squares spelled out at both sites, no helper,
// 77.2%; a helper body using CLBSquared<f32> for each square, 71.3%; three
// named squares in TVec3::squared(const TVec3&) itself cost TShine::calc
// (99.9 -> 95.5), the only user of that overload.

// Both notice sites run the same clip test; retail inlines it at each.
static inline bool CameraNoticeIsInClip(const JGeometry::TVec2<f32>& clipPos,
                                        f32 clipMax)
{
	f32 clipMin = -clipMax;
	bool inClipX = false, overClipMinY = false, inClipY = false;
	if (clipMin <= clipPos.x && clipPos.x <= clipMax)
		inClipX = true;

	if (inClipX && clipMin <= clipPos.y)
		overClipMinY = true;

	if (overClipMinY && clipPos.y <= clipMax)
		inClipY = true;

	return inClipY ? true : false;
}

void CPolarSubCamera::setNoticeInfo()
{
	unk2A0       = new TLiveActor*[0x10];
	mNoticeActor = nullptr;
	unk29C       = 0;

	for (int i = 0; sNoticeActorManagerName[i] != nullptr; ++i) {
		TLiveManager* mgr
		    = gpConductor->getManagerByName(sNoticeActorManagerName[i]);

		if (mgr != nullptr) {
			s32 e = mgr->getObjNum();
			for (int j = 0; j < e; ++unk29C, ++j)
				unk2A0[unk29C] = mgr->getObj(j);
		}
	}

	unk2A8 = JDrama::TNameRefGen::search<TLiveActor>(bossGesoViewObjName);
}

// TODO: 97.1% (was 92.7%); every instruction is right, frame 0xe8 vs 0xf0.
// Restored: the clip range is [-ratio, +ratio] and both clip tests are one
// TU-local inline (CameraNoticeIsInClip: the three-bool ladder whose result
// is normalised again); the loop's distance guard is `!(dist2 <
// closestDist2)`; the identity test is `mNoticeActor == unk2A0[i]`; the
// loop's matrices go through `getUnk16C()`/`getUnk1EC()` (hoisted base temps);
// the squared-distance sum is two statements, so it is computed before the
// `bl CLBSquared` as in retail.
// What is left, registers and slots only:
//   - r31/r30 hold 0x1ec/0x16c where retail has 0x16c/0x1ec; the distance sum
//     and clipMax take each other's callee-saved FPR (retail sum f28/f27,
//     clipMax f29/f28). Un-naming noticeDist2 is inert.
//   - retail places clipPos at 0x84 and 0x6c with gaps above each, ours
//     0x8c and 0x84; the ladder's zero copies are `addi rX,r3,0` in retail,
//     `mr` off r0 here. A by-value TVec2 or (x, y) parameter helper and a
//     helper that also calls CLBCalc2DFPos are all worse (89.8-96.7%).
TLiveActor* CPolarSubCamera::getNoticeActor_()
{
	if (mNoticeActor != nullptr && !mNoticeActor->checkLiveFlag(LIVE_FLAG_DEAD)
	    && !mNoticeActor->checkLiveFlag(LIVE_FLAG_HIDDEN)) {

		f32 noticeDist2
		    = MsSquaredDist(mNoticeActor->mPosition, *gpMarioPos);
		if (noticeDist2 < CLBSquared<f32>(mSaveNotice->mOffDist.get())) {
			f32 clipMax = mSaveNotice->mOffClipRatio.get();

			JGeometry::TVec2<f32> clipPos;
			CLBCalc2DFPos(&clipPos, unk16C, unk1EC, mNoticeActor->mPosition,
			              nullptr, false);

			if (CameraNoticeIsInClip(clipPos, clipMax))
				return mNoticeActor;
		}
	}

	TLiveActor* noticeActor = nullptr;
	f32 closestDist2        = CLBSquared<f32>(mSaveNotice->mOnDist.get());

	for (int i = 0; i < unk29C; i++) {
		if (unk2A0[i]->checkLiveFlag(LIVE_FLAG_DEAD)
		    || unk2A0[i]->checkLiveFlag(LIVE_FLAG_HIDDEN))
			continue;

		if (mNoticeActor != nullptr && mNoticeActor == unk2A0[i])
			continue;

		f32 dist2
		    = MsSquaredDist(unk2A0[i]->mPosition, *gpMarioPos);
		if (!(dist2 < closestDist2))
			continue;

		f32 clipMax = mSaveNotice->mOnClipRatio.get();

		JGeometry::TVec2<f32> clipPos;
		CLBCalc2DFPos(&clipPos, getUnk16C(), getUnk1EC(),
		              unk2A0[i]->getPosition(),
		              nullptr, false);

		if (!CameraNoticeIsInClip(clipPos, clipMax))
			continue;

		if (!MsIsInSight(*gpMarioPos, SHORTANGLE2DEG(*gpMarioAngleY),
		                 unk2A0[i]->mPosition, dist2,
		                 mSaveNotice->mOnDegree.get(), -1.0f))
			continue;

		closestDist2 = dist2;
		noticeActor  = unk2A0[i];
	}

	return noticeActor;
}

void CPolarSubCamera::execNoticeOnOffProc_(EnumNoticeOnOffMode mode)
{
	switch (mode) {
	case NOTICE_MODE_UNK0:
		mNoticeActor = nullptr;
		unk64 &= ~CAMERA_FLAG_NOTICE_ACTIVE;
		break;
	case NOTICE_MODE_UNK1: {
		TLiveActor* actor = getNoticeActor_();
		if (actor != mNoticeActor && actor == nullptr) {
			mNoticeActor = nullptr;
			unk64 &= ~CAMERA_FLAG_NOTICE_ACTIVE;
		}
		break;
	}
	case NOTICE_MODE_UNK2: {
		TLiveActor* actor = getNoticeActor_();
		if (actor != mNoticeActor && actor != nullptr) {
			mNoticeActor = actor;
			unk64 |= CAMERA_FLAG_NOTICE_ACTIVE;
		}
		break;
	}
	}
}

// TODO: 97.2%. The two squared distances were swapped (retail tests the
// minimum first, then the fast minimum inside), the speed product is
// base * (chase * (ratio * speed)), dist2 is one unnamed sum, and the
// absolute angle is CLBAbs<int> of the truncated difference.
// Left: retail sign-extends matan's result (`extsh r4, r3`) and redoes the
// `extsh` in each abs arm (3 instructions); `(s16)ang`, `(int)ang`, an int
// ang and CLBAbs<s16> are inert or worse. Frame 0xa8 vs 0x98.
// The named 500.0f distance is what keeps the fmadds operands exact.
void CPolarSubCamera::calcNoticeTargetYrot_(const Vec& target)
{
	Vec mPos     = gpCameraMario->unk0;
	f32 dist2 = CLBSquared<f32>(mPos.x - target.x)
	            + CLBSquared<f32>(mPos.z - target.z);
	f32 minDist2 = CLBSquared<f32>(mSaveNotice->mRotateMinDistXZ.get());
	f32 fastDist2
	    = CLBSquared<f32>(mSaveNotice->mRotateFastMinDistXZ.get());

	if (dist2 > minDist2) {
		JGeometry::TVec3<f32> diff(mPos.x - target.x, mPos.y - target.y,
		                           mPos.z - target.z);
		MsVECNormalize(&diff, &diff);
		// TODO: many inlines from cameralib maybe?
		f32 distance = 500.0f;
		f32 dx       = diff.x * distance + mPos.x;
		f32 dz       = diff.z * distance + mPos.z;
		s16 ang      = matan(dz - mCurrentTarget.mTarget.z,
		                     dx - mCurrentTarget.mTarget.x);
		int absAngle = CLBAbs<int>((s16)(mCurrentTarget.mYaw - ang));
		f32 ratio    = (f32)absAngle * (2.0f / 65536.0f);

		f32 chase = dist2 > fastDist2
		                ? 1.0f
		                : CLBCalcRatio<f32>(minDist2, fastDist2, dist2);
		f32 base = CLBLinearInbetween<f32>(
		    1.0f, mSaveNotice->mRotateMagnifXmax.get(), mCurrentTarget.unk28);
		f32 speed
		    = unk288
		      * (base
		         * (chase * (ratio * (f32)mSaveNotice->mRotateYSpeed.get())));
		if (speed > 32766.998f)
			speed = 32766.998f;
		s16 delta = CLBRoundf<s16>(speed);
		CLBChaseGeneralConstantSpecifySpeed<s16>(&mCurrentTarget.mYaw, ang,
		                                         delta);
	}
}

// Two binding levels for getNozzleTopPos_'s frame (+0x18 together, cc37):
// the water-gun test and the nozzle matrix are each named and returned.
static inline TWaterGun* CameraNoticeWaterGun()
{
	TWaterGun* gun = SMS_GetMarioWaterGun();
	return gun;
}

static inline MtxPtr CameraNoticeNozzleMtx(TWaterGun* gun)
{
	MtxPtr mtx = gun->getNozzleMtx();
	return mtx;
}

void CPolarSubCamera::getNozzleTopPos_(JGeometry::TVec3<f32>* out) const
{
	if (CameraNoticeWaterGun() == nullptr) {
		out->set(gpCameraMario->unk0);
	} else {
		MtxPtr mtx = CameraNoticeNozzleMtx(SMS_GetMarioWaterGun());
		out->x     = mtx[0][3];
		out->y     = mtx[1][3];
		out->z     = mtx[2][3];

		JGeometry::TVec3<f32> dir(mtx[0][1], mtx[1][1], mtx[2][1]);
		dir.normalize();
		dir *= 30.0f;
		*out += dir;
	}
}

// TODO (closure batch 87): pure frame gap, 0x70 vs 0x40. The one named local
// (a scratch Vec) sits at 0x34 in retail and 0x10 here, so 36 of the 48 bytes
// are low region (inline-expansion temporaries) and 12 sit above the vector.
// getNozzleTopPos_ was the same shape at 0x58 vs 0x40 and closed with two
// binders (cc37); here that lifted the frame to 0x50 with the vector at 0x24.
// Best measured (cc37, not committed): a name-and-return reference binder
// over gpCameraMario->unk0 at the set() plus mNoticeActor->getPosition()
// lands the vector at 0x34 with the frame 0x68, 8 short *above* it; bool/u32
// flag wrappers, a TVec3 copy temporary, target-address forks/binders and a
// named notice actor or target reference all overshoot or move the vector.
// c-hs6 (hsearch): byte-exact with four named single-use values stacked, each
// +8: u32 copies of both freeze counters for their == 0 tests, a bool for
// SMS_CheckMarioFlag(MARIO_FLAG_HAS_FLUDD), and a const reference to
// mNoticeActor->getPosition(). Any three leave it 8 short; not taken as a
// stack of temporaries, kept here as the lead.
void CPolarSubCamera::ctrlLButtonCamera_()
{
	f32 stickX = -unk120->mCompSPos[4];
	f32 stickY = -unk120->mCompSPos[5];

	if (mTargetFreezeFrames == 0) {
		if (!SMS_CheckMarioFlag(MARIO_FLAG_HAS_FLUDD))
			mCurrentTarget.mTarget.set(gpCameraMario->unk0);
		else
			getNozzleTopPos_(&mCurrentTarget.mTarget);
	}

	if (mPosFreezeFrames == 0) {
		if (unk64 & CAMERA_FLAG_NOTICE_ACTIVE) {
			if (stickX != 0.0f) {
				rotateY_ByStickX_(stickX);
			} else {
				calcNoticeTargetYrot_(mNoticeActor->mPosition);
			}
		} else {
			rotateY_ByStickX_(stickX);
		}
		rotateX_ByStickY_(stickY);
	}

	calcPosAndAt_();
}
