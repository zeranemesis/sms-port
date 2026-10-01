#include <Camera/Camera.hpp>
#include <Camera/CameraKindParam.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionData.hpp>
#include <Camera/cameralib.hpp>
#include <Camera/camerasave.hpp>
#include <MarioUtil/MapUtil.hpp>
#include <JSystem/JMath.hpp>

void CPolarSubCamera::calcInHouseNoSub_()
{
	if (unk2CA != -1) {
		unk2C8 = unk2CA;
		if ((f32)mSaveEx->mInHouseMinFrame.get() < (f32)unk2CC)
			unk2CC += 1;
	} else if (unk2C8 != -1) {
		if ((f32)unk2CC < (f32)mSaveEx->mInHouseMinFrame.get()) {
			unk2CC += 1;
		} else {
			unk2C8 = -1;
			unk2CC = 0;
		}
	}
}

void CPolarSubCamera::calcInHouseNo_(bool param_1)
{
	// TODO: two known differences from the ROM (both unresolved):
	//  - the scaleAdd loop below is unrolled x9 here but only x3 in the ROM
	//    (ROM: `li r0,3 / mtctr` with 3 copies per iteration).
	//  - our frame is 0x1f8 vs the ROM's 0x1c0, and the ROM's out_euler
	//    slot (r4 = 0x8c(r1)) is only 8 bytes and sits *below* local_12C,
	//    whereas ours is 54 bytes (S16Vec[9]) above it. The 3 unused ROM
	//    slots at 0x64..0x87 plus the smaller euler slot account for the
	//    frame difference; we also save r26..r31 where the ROM only saves
	//    r24..r28.
	bool b = true;
	if (!param_1 && unk13C == unk124 && unk160 == unk148)
		b = false;

	if (b) {
		if (isThing3()) {
			unk2CA = -1;
			calcInHouseNoSub_();
			return;
		}

		JGeometry::TVec3<f32> local_120[18];
		S16Vec SStack_134[9];

		CLBCalcNearNinePos(local_120, SStack_134, unk124, unk148,
		                   getFinalAngleZ(), mNear, mFovy, mAspect);

		f32 fVar1 = unk2C4;
		for (int i = 0; i < 9; ++i) {
			local_120[9 + i].scaleAdd(fVar1, local_120[i], unk25C);
		}

		f32 tmp = unk2C0;
		for (int i = 0; i < 9; ++i) {
			for (int j = 0; j < 2; ++j) {
				f32 fVar2 = 0.0f;
				for (int k = 0; k < 2; ++k) {
					JGeometry::TVec3<f32> local_12C(local_120[j * 9 + i].x,
					                                local_120[j * 9 + i].y
					                                    - fVar2 + -78.0f,
					                                local_120[j * 9 + i].z);
					const TBGCheckData* local_138;
					gpMap->checkGroundIgnoreWaterSurface(local_12C, &local_138);
					if (local_138 && local_138->isOob()) {
						unk2CA = local_138->getData();
						calcInHouseNoSub_();
						return;
					}

					fVar2 += tmp;
				}
			}
		}
	}

	unk2CA = -1;
	calcInHouseNoSub_();
}

bool CPolarSubCamera::isNeedGroundCheck_()
{
	// TODO: the whole tail is instruction-for-instruction identical to the
	// ROM; only f1/f2 are swapped (ROM keeps mDistMax/JMASSin(maxAngle) in
	// f1 and mDistMin/JMASSin(minAngle) in f3, we keep them the other way
	// round). Swapping the declaration order of a/b or hoisting distY does
	// not fix it (both were tried, both are worse).
	bool result = true;
	if (mMode == CAMERA_MODE_REPRODUCE_DEMO
	    || (isLButtonCameraSpecifyMode(mMode) && !isNowInbetween() ? true
	                                                               : false)
	    || isRailCameraSpecifyMode(mMode) || mMode == CAMERA_MODE_MULTI_PLAYER
	    || unk278 != 0) {
		result = false;
	} else if (mMode != CAMERA_MODE_SLIDER
	           && (isNormalCameraSpecifyMode(mMode)
	               || isTowerCameraSpecifyMode(mMode))) {
		f32 a = mCurrentParams->mDistMin * JMASSin(mCurrentParams->mXAngleMin);
		f32 b = mCurrentParams->mDistMax * JMASSin(mCurrentParams->mXAngleMax);
		f32 distY = mPosition.y - mTarget.y;
		if (a > b)
			b = a;
		if (distY > 1.25f * b) {
			result = false;
			if (unk278 < 120)
				unk278 = 120;
		}
	}
	return result;
}

bool CPolarSubCamera::isNeedRoofCheck_() const
{
	bool result = true;
	// TODO: inline for the ternary thing
	if (mMode == CAMERA_MODE_REPRODUCE_DEMO
	    || (isLButtonCameraSpecifyMode(mMode) && !isNowInbetween() ? true
	                                                               : false)
	    || isRailCameraSpecifyMode(mMode) || mMode == CAMERA_MODE_MULTI_PLAYER
	    || unk27A != 0)
		result = false;

	return result;
}

bool CPolarSubCamera::isNeedWallCheck_() const
{
	bool result = true;
	if (mMode == CAMERA_MODE_REPRODUCE_DEMO || isLButtonCameraSpecifyMode(mMode)
	    || isLButtonCameraInbetween() || isTalkCameraSpecifyMode(mMode)
	    || isTalkCameraInbetween() || isRailCameraSpecifyMode(mMode)
	    || mMode == CAMERA_MODE_MULTI_PLAYER
	    || mMode == CAMERA_MODE_UNDER_GROUND || (unk64 & CAMERA_FLAG_UNK4)) {
		result = false;
	}
	return result;
}

static bool should_clip_fabricated(const TBGCheckData* data)
{
	bool result = false;

	if (data != nullptr && data->isLegal() && data->isCameraWontClip())
		result = true;

	return result;
}

bool CPolarSubCamera::execWallCheck_(Vec* param_1)
{
	// TODO: instruction sequence matches the ROM exactly; only register
	// numbering differs (wall in r6 vs r5, the 0.0f constant in f3 vs f2,
	// and the dot-product temporaries f1/f2/f3 vs f3/f4/f5). Swapping the
	// declaration order of posArg/posCam was tried and is slightly worse.
	bool moved = false;
	f32 radius = mSaveEx->mSLWallCheckRadius.get();
	if (radius > 0.0f) {
		TBGWallCheckRecord record(mCurrentTarget.mPosition.x,
		                          mPreviousTarget.mPosition.y + 10.0f,
		                          mCurrentTarget.mPosition.z, radius, 4, 0);

		if (gpMap->isTouchedWallsAndMoveXZ(&record)) {
			int n = record.mResultWallsNum;
			for (int i = 0; i < n; ++i) {
				TBGCheckData* wall = record.mResultWalls[i];
				if (should_clip_fabricated(wall)) {
					JGeometry::TVec3<f32> posArg = mCurrentTarget.mPosition;
					JGeometry::TVec3<f32> posCam = posArg;

					f32 sd = posCam.dot(wall->getNormal())
					         + wall->getPlaneDistance();
					f32 absSd = sd >= 0.0f ? sd : -sd;
					if (absSd < radius) {
						moved      = true;
						f32 pushSd = (radius - sd)
						             * mSaveEx->mSLWallRevisionRatio.get();
						posCam.x += pushSd * wall->getNormal().x;
						posCam.z += pushSd * wall->getNormal().z;
						mCurrentTarget.mPosition.x = posCam.x;
						mCurrentTarget.mPosition.z = posCam.z;
						f32 pushArg                = radius - sd;
						posArg.x += pushArg * wall->getNormal().x;
						posArg.z += pushArg * wall->getNormal().z;
						param_1->x = posArg.x;
						param_1->z = posArg.z;
					}
				}
			}
		}
	}
	return moved;
}

bool CPolarSubCamera::execRoofCheck_(Vec param_1)
{
	bool moved               = false;
	bool skipCheck           = false;
	const TBGCheckData* roof = nullptr;
	f32 roofHeight           = 0.0f;
	if (SMS_GetMonteVillageAreaInMario() == 0
	    && gpCamera->mMode != CAMERA_MODE_MONTE_HANG) {
		roofHeight = -512.5f;
		skipCheck  = true;
	} else {
		roofHeight = gpMap->checkRoof(param_1.x,
		                              mPreviousTarget.mPosition.y
		                                  - mSaveEx->mSLRoofChangeY.get(),
		                              param_1.z, &roof);
	}

	if (skipCheck || should_clip_fabricated(roof)) {
		if (mCurrentTarget.mPosition.y
		    > roofHeight - mSaveEx->mSLRoofHeight.get()) {
			mCurrentTarget.mPosition.y
			    = roofHeight - mSaveEx->mSLRoofHeight.get();
			moved = true;
		}
	}
	return moved;
}

bool CPolarSubCamera::execGroundCheck_(Vec param_1)
{
	bool moved    = false;
	f32 groundChg = mSaveEx->mGroundChangeY.get();
	f32 groundOff = CLBLinearInbetween<f32>(
	    mSaveEx->mSLGroundHeightNormal.get(),
	    mSaveEx->mSLGroundHeightReadyGun.get(), unk2AC->unk8);

	if (mMode == CAMERA_MODE_SLIDER) {
		groundChg = groundChg > 200.0f ? groundChg : 200.0f;
		groundOff = groundOff > 400.0f ? groundOff : 400.0f;
	}

	const TBGCheckData* ground;
	f32 groundY = gpMap->checkGroundIgnoreWaterSurface(
	    param_1.x, mPreviousTarget.mPosition.y + groundChg, param_1.z, &ground);

	if (should_clip_fabricated(ground)) {
		if (mCurrentTarget.mPosition.y < groundY + groundOff) {
			mCurrentTarget.mPosition.y = groundY + groundOff;
			moved                      = true;
		}
	}
	return moved;
}
