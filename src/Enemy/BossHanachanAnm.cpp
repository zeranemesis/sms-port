#include <Camera/cameralib.hpp>
#include <Enemy/BossHanachan.hpp>
#include <Enemy/BossHanachanChangeSaveParams.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Strategic/Spine.hpp>
#include <math.h>

void TBossHanachan::changeAnmRateAndFrameUpdate_()
{
	TBossHanachanPartsBase* parts[9];
	parts[0] = mHead;
	for (int i = 0; i < 8; i++)
		parts[i + 1] = mBody[i];

	bool setFrameRate = true;
	f32 frameRate = SMSGetAnmFrameRate();
	f32 rate = frameRate;
	const JDrama::TNerveBase<TLiveActor>* currentNerve
	    = mSpine->getCurrentNerve();

	if (currentNerve == &TNerveBossHanachanTumble::theNerve()) {
		for (int i = 0; i < 9; i++)
			parts[i]->mNonstopMotionBlend->unk28 = 0.0f;
		for (int i = 0; i < 9; i++)
			parts[i]->changeTumbleAnmRate_();
		setFrameRate = false;
	} else {
		int currentAnm = mHead->mCurAnm;
		f32 walkSpeed = mChangeSaveParams->mSLWalkAnmMarchSpeed.get();
		f32 runSpeed = mChangeSaveParams->mSLRunAnmMarchSpeed.get();

		if (currentAnm < BH_ANM_KIND_UNK0
		    || currentAnm > BH_ANM_KIND_UNK1) {
			for (int i = 0; i < 9; i++)
				parts[i]->mNonstopMotionBlend->unk28 = 0.0f;
			rate = SMSGetAnmFrameRate();
		} else if (mMarchSpeed <= walkSpeed) {
			for (int i = 0; i < 9; i++)
				parts[i]->mNonstopMotionBlend->unk28 = 0.0f;
			if (currentAnm == BH_ANM_KIND_UNK1) {
				setHeadAndBodyAnm(BH_ANM_KIND_UNK0,
				                  BH_STOP_MOTION_BLEND_OFF);
				mHead->copyFrameFromOldAnmToNewAnm_();
				for (int i = 0; i < 8; i++)
					mBody[i]->copyFrameFromOldAnmToNewAnm_();
			} else if (currentAnm != BH_ANM_KIND_UNK0) {
				setHeadAndBodyAnm(BH_ANM_KIND_UNK0,
				                  BH_STOP_MOTION_BLEND_ON);
			}
		} else if (mMarchSpeed < runSpeed) {
			for (int i = 0; i < 9; i++)
				parts[i]->mNonstopMotionBlend->unk28 = 0.0f;
			if (currentAnm == BH_ANM_KIND_UNK0) {
				setHeadAndBodyAnm(BH_ANM_KIND_UNK1,
				                  BH_STOP_MOTION_BLEND_OFF);
				mHead->copyFrameFromOldAnmToNewAnm_();
				for (int i = 0; i < 8; i++)
					mBody[i]->copyFrameFromOldAnmToNewAnm_();
			} else if (currentAnm != BH_ANM_KIND_UNK1) {
				setHeadAndBodyAnm(BH_ANM_KIND_UNK1,
				                  BH_STOP_MOTION_BLEND_ON);
			}
		} else {
			f32 ratio = CLBCalcRatio(walkSpeed, runSpeed, mMarchSpeed);
			if (currentAnm == BH_ANM_KIND_UNK1) {
				if (mHead->mOldAnm == BH_ANM_KIND_UNK0) {
					rate = 1.0f - ratio;
				} else {
					setHeadAndBodyAnm(BH_ANM_KIND_UNK0,
					                  BH_STOP_MOTION_BLEND_OFF);
					mHead->copyFrameFromOldAnmToNewAnm_();
					for (int i = 0; i < 8; i++)
						mBody[i]->copyFrameFromOldAnmToNewAnm_();
					rate = ratio;
				}
			} else if (currentAnm == BH_ANM_KIND_UNK0) {
				if (mHead->mOldAnm == BH_ANM_KIND_UNK1) {
					rate = ratio;
				} else {
					setHeadAndBodyAnm(BH_ANM_KIND_UNK1,
					                  BH_STOP_MOTION_BLEND_OFF);
					mHead->copyFrameFromOldAnmToNewAnm_();
					for (int i = 0; i < 8; i++)
						mBody[i]->copyFrameFromOldAnmToNewAnm_();
					rate = 1.0f - ratio;
				}
			} else {
				for (int i = 0; i < 9; i++)
					parts[i]->mNonstopMotionBlend->unk28 = 0.0f;
				setHeadAndBodyAnm(BH_ANM_KIND_UNK0,
				                  BH_STOP_MOTION_BLEND_ON);
			}
			if (currentAnm == BH_ANM_KIND_UNK0
			    || currentAnm == BH_ANM_KIND_UNK1) {
				for (int i = 0; i < 9; i++)
					parts[i]->mNonstopMotionBlend->unk28 = rate;
			}
		}

		if (currentAnm >= BH_ANM_KIND_UNK0
		    && currentAnm <= BH_ANM_KIND_UNK1) {
			f32 minRate = mChangeSaveParams->mSLWalkBckRateMin.get();
			rate = mMarchSpeed * SMSGetAnmFrameRate()
			        * mChangeSaveParams->mSLWalkBckRateMagnif.get();
			if (rate < minRate)
				rate = minRate;
		}
	}

	if (setFrameRate) {
		for (int i = 0; i < 9; i++)
			parts[i]->getMActor()->getFrameCtrl(0)->setRate(rate);
	}
	for (int i = 0; i < 9; i++) {
		parts[i]->updateAnmSound();
		parts[i]->getMActor()->frameUpdate();
	}
}

bool TBossHanachan::isAllBckAlreadyEnd(EnumBossHanachanAnmKind anm) const
{
	bool ok = false;
	if (mHead->mCurAnm == anm && mHead->isCurBckAlreadyEnd_())
		ok = true;
	if (!ok)
		return false;

	for (int i = 0; i < 8; i++) {
		ok = false;
		if (mBody[i]->mCurAnm == anm && mBody[i]->isCurBckAlreadyEnd_())
			ok = true;
		if (!ok)
			return false;
	}
	return true;
}

bool TBossHanachan::isFinishedGetUp() const
{
	bool result = false;
	if (mHead->mCurAnm == BH_ANM_KIND_UNKC || mHead->mCurAnm == BH_ANM_KIND_UNK9) {
		if (mHead->isCurBckAlreadyEnd_())
			result = true;
	}
	return result;
}

void TBossHanachan::considerSetAnm(EnumBossHanachanNerveAnm anm)
{
	mHead->considerSetAnm_(anm);
	for (int i = 0; i < 8; i++)
		mBody[i]->considerSetAnm_(anm);
}

// fabricated
// TODO: name is a guess; MWCC does not inline MSL's abs(), but the target
// clearly inlines a branch-based abs here, so we replicate that shape.
static inline s32 BHAbs(s32 v) { return v < 0 ? -v : v; }

void TBossHanachan::setAnmTimerWhenDead()
{
	u8 diff = mChangeSaveParams->mSLDeadFrameDiff.get();
	for (int i = 0; i < 8; i++)
		mBody[i]->unk10C = diff * BHAbs(mWeakBodyIndex - i);
	mHead->unk10C = diff * BHAbs(mWeakBodyIndex - -1);
}

void TBossHanachan::setAnmTimerWhenDamage()
{
	u8 diff = mChangeSaveParams->mSLDamageFrameDiff.get();
	for (int i = 0; i < 8; i++)
		mBody[i]->unk10C = diff * BHAbs(mWeakBodyIndex - i);
	mHead->unk10C = diff * BHAbs(mWeakBodyIndex - -1);
}

void TBossHanachan::setAnmTimerWhenSnort()
{
	u8 diff = mChangeSaveParams->mSLSnortFrameDiff.get();
	mHead->unk10C  = 0;
	mBody[0]->unk10C = diff;
	mBody[1]->unk10C = diff * 2;
	mBody[2]->unk10C = diff * 3;
	mBody[3]->unk10C = diff * 4;
	mBody[4]->unk10C = diff * 5;
	mBody[5]->unk10C = diff * 6;
	mBody[6]->unk10C = diff * 7;
	mBody[7]->unk10C = diff * 8;
}

void TBossHanachan::setAnmTimerWhenGetUp()
{
	u8 diff = mChangeSaveParams->mSLGetUpFrameDiff.get();
	mBody[7]->unk10C = 0;
	mBody[6]->unk10C = diff;
	mBody[5]->unk10C = diff * 2;
	mBody[4]->unk10C = diff * 3;
	mBody[3]->unk10C = diff * 4;
	mBody[2]->unk10C = diff * 5;
	mBody[1]->unk10C = diff * 6;
	mBody[0]->unk10C = diff * 7;
	mHead->unk10C    = diff * 8;
}

void TBossHanachan::setTumbleBckRate_(TBossHanachanPartsBase* part)
{
	J3DFrameCtrl* frameCtrl = part->getMActor()->getFrameCtrl(0);
	f32 diff = fabsf(unk194 - part->getRotation().z);
	f32 t    = (1.0f / unk198) * diff;
	frameCtrl->setRate((1.0f / t) * (2.0f * (40.0f * SMSGetAnmFrameRate())));
}

void TBossHanachan::setTumbleAnm(EnumBossHanachanStopMotionBlendOnOff onOff)
{
	EnumBossHanachanAnmKind anm;
	if (unk194 == 179.0f)
		anm = BH_ANM_KIND_UNK11;
	else if (unk194 == -179.0f)
		anm = BH_ANM_KIND_UNK10;
	else
		return;

	mHead->setAnm_(anm, onOff);
	setTumbleBckRate_(mHead);
	for (int i = 0; i < 8; i++) {
		mBody[i]->setAnm_(anm, onOff);
		setTumbleBckRate_(mBody[i]);
	}
}

void TBossHanachan::setHeadAndBodyAnm(EnumBossHanachanAnmKind anm,
                                       EnumBossHanachanStopMotionBlendOnOff onOff)
{
	mHead->setAnm_(anm, onOff);

	u8 frameDiff = mChangeSaveParams->mSLNormalBckFrameDiff.get();
	for (int i = 0; i < 8; i++) {
		if (mBody[i]->setAnm_(anm, onOff)) {
			J3DFrameCtrl* ctrl = mBody[i]->getMActor()->getFrameCtrl(0);
			f32 frame          = (f32)((i * frameDiff) % ctrl->getEnd());
			ctrl->setFrame(frame);

			J3DFrameCtrl* ctrl3 = mBody[i]->getMActor()->getFrameCtrl(3);
			if (ctrl3)
				ctrl3->setFrame(frame);

			J3DFrameCtrl* ctrl4 = mBody[i]->getMActor()->getFrameCtrl(4);
			if (ctrl4)
				ctrl4->setFrame(frame);
		}
	}
}
