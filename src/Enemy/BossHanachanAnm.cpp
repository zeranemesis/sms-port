#include <Camera/cameralib.hpp>
#include <Enemy/BossHanachan.hpp>
#include <Enemy/BossHanachanChangeSaveParams.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Strategic/Spine.hpp>
#include <math.h>

template <> f32 CLBCalcRatio<f32>(f32, f32, f32);

// The target build walks mHead first and then mBody[0..7] with a rolled loop,
// so the parts are never copied into a local array.
#define BH_HEAD_AND_BODY(body)                                                  \
	do {                                                                         \
		mHead->mNonstopMotionBlend->unk28 = (body);                              \
		for (int i = 0; i < 8; i++)                                             \
			mBody[i]->mNonstopMotionBlend->unk28 = (body);                        \
	} while (0)

void TBossHanachan::changeAnmRateAndFrameUpdate_()
{
	bool setFrameRate = true;
	f32 rate         = SMSGetAnmFrameRate();
	const JDrama::TNerveBase<TLiveActor>* currentNerve
	    = mSpine->getLatestNerve();

	if (currentNerve == &TNerveBossHanachanTumble::theNerve()) {
		BH_HEAD_AND_BODY(0.0f);
		mHead->changeTumbleAnmRate_();
		for (int i = 0; i < 8; i++)
			mBody[i]->changeTumbleAnmRate_();
		setFrameRate = false;
	} else {
		// Note: the target re-loads mHead->mCurAnm from memory at every
		// decision point instead of caching it, so no local copy is made.
		if (mHead->mCurAnm < BH_ANM_KIND_UNK2
		    && mHead->mCurAnm >= BH_ANM_KIND_UNK0) {
			f32 walkSpeed = mChangeSaveParams->mSLWalkAnmMarchSpeed.get();
			if (mMarchSpeed <= walkSpeed) {
				BH_HEAD_AND_BODY(0.0f);
				switch (mHead->mCurAnm) {
				case BH_ANM_KIND_UNK1:
					setHeadAndBodyAnm(BH_ANM_KIND_UNK0,
					                  BH_STOP_MOTION_BLEND_OFF);
					mHead->copyFrameFromOldAnmToNewAnm_();
					for (int i = 0; i < 8; i++)
						mBody[i]->copyFrameFromOldAnmToNewAnm_();
					break;
				case BH_ANM_KIND_UNK0:
					break;
				default:
					setHeadAndBodyAnm(BH_ANM_KIND_UNK0,
					                  BH_STOP_MOTION_BLEND_ON);
					break;
				}
			} else {
				f32 runSpeed = mChangeSaveParams->mSLRunAnmMarchSpeed.get();
				if (mMarchSpeed < runSpeed) {
					BH_HEAD_AND_BODY(0.0f);
					switch (mHead->mCurAnm) {
					case BH_ANM_KIND_UNK0:
						setHeadAndBodyAnm(BH_ANM_KIND_UNK1,
						                  BH_STOP_MOTION_BLEND_OFF);
						mHead->copyFrameFromOldAnmToNewAnm_();
						for (int i = 0; i < 8; i++)
							mBody[i]->copyFrameFromOldAnmToNewAnm_();
						break;
					case BH_ANM_KIND_UNK1:
						break;
					default:
						setHeadAndBodyAnm(BH_ANM_KIND_UNK1,
						                  BH_STOP_MOTION_BLEND_ON);
						break;
					}
				} else {
					rate = CLBCalcRatio(walkSpeed, runSpeed,
					                    mMarchSpeed);
					switch (mHead->mCurAnm) {
					case BH_ANM_KIND_UNK1:
						if (mHead->mOldAnm != BH_ANM_KIND_UNK0) {
							setHeadAndBodyAnm(BH_ANM_KIND_UNK0,
							                  BH_STOP_MOTION_BLEND_OFF);
							mHead->copyFrameFromOldAnmToNewAnm_();
							for (int i = 0; i < 8; i++)
								mBody[i]->copyFrameFromOldAnmToNewAnm_();
						} else {
							rate = 1.0f - rate;
						}
						BH_HEAD_AND_BODY(rate);
						break;
					case BH_ANM_KIND_UNK0:
						if (mHead->mOldAnm != BH_ANM_KIND_UNK1) {
							setHeadAndBodyAnm(BH_ANM_KIND_UNK1,
							                  BH_STOP_MOTION_BLEND_OFF);
							mHead->copyFrameFromOldAnmToNewAnm_();
							for (int i = 0; i < 8; i++)
								mBody[i]->copyFrameFromOldAnmToNewAnm_();
							rate = 1.0f - rate;
						}
						BH_HEAD_AND_BODY(rate);
						break;
					default:
						BH_HEAD_AND_BODY(0.0f);
						setHeadAndBodyAnm(BH_ANM_KIND_UNK0,
						                  BH_STOP_MOTION_BLEND_ON);
						break;
					}
				}
			}

			rate = mMarchSpeed * SMSGetAnmFrameRate()
			        * mChangeSaveParams->mSLWalkBckRateMagnif.get();
			f32 minRate = mChangeSaveParams->mSLWalkBckRateMin.get();
			if (rate < minRate)
				rate = minRate;
		} else {
			BH_HEAD_AND_BODY(0.0f);
			rate = SMSGetAnmFrameRate();
		}
	}

	MActor* headActor = mHead->getMActor();
	if (setFrameRate)
		headActor->getFrameCtrl(0)->setRate(rate);
	mHead->updateAnmSound();
	headActor->frameUpdate();
	for (int i = 0; i < 8; i++) {
		MActor* actor = mBody[i]->getMActor();
		if (setFrameRate)
			actor->getFrameCtrl(0)->setRate(rate);
		mBody[i]->updateAnmSound();
		actor->frameUpdate();
	}
}

#undef BH_HEAD_AND_BODY

bool TBossHanachan::isAllBckAlreadyEnd(EnumBossHanachanAnmKind anm) const
{
	bool result = true;
	bool ok     = false;
	if (mHead->mCurAnm == anm && mHead->isCurBckAlreadyEnd_())
		ok = true;
	if (!ok) {
		result = false;
	} else {
		for (int i = 0; i < 8; i++) {
			ok = false;
			if (mBody[i]->mCurAnm == anm && mBody[i]->isCurBckAlreadyEnd_())
				ok = true;
			if (!ok) {
				result = false;
				break;
			}
		}
	}
	return result;
}

bool TBossHanachan::isFinishedGetUp() const
{
	bool result = false;
	switch (mHead->mCurAnm) {
	case BH_ANM_KIND_UNKC:
	case BH_ANM_KIND_UNK9:
		if (mHead->isCurBckAlreadyEnd_())
			result = true;
		break;
	}
	return result;
}

void TBossHanachan::considerSetAnm(EnumBossHanachanNerveAnm anm)
{
	mHead->considerSetAnm_(anm);
	for (int i = 0; i < 8; i++)
		mBody[i]->considerSetAnm_(anm);
}

void TBossHanachan::setAnmTimerWhenDead()
{
	u8 diff = mChangeSaveParams->mSLDeadFrameDiff.get();
	for (int i = 0; i < 8; i++) {
		s32 d = mWeakBodyIndex - i;
		d = (d >= 0) ? d : -d;
		mBody[i]->unk10C = diff * d;
	}
	s32 d = mWeakBodyIndex + 1;
	d = (d >= 0) ? d : -d;
	mHead->unk10C = diff * d;
}

void TBossHanachan::setAnmTimerWhenDamage()
{
	u8 diff = mChangeSaveParams->mSLDamageFrameDiff.get();
	for (int i = 0; i < 8; i++) {
		s32 d = mWeakBodyIndex - i;
		d = (d >= 0) ? d : -d;
		mBody[i]->unk10C = diff * d;
	}
	s32 d = mWeakBodyIndex + 1;
	d = (d >= 0) ? d : -d;
	mHead->unk10C = diff * d;
}

void TBossHanachan::setAnmTimerWhenSnort()
{
	u8 diff = mChangeSaveParams->mSLSnortFrameDiff.get();
	mHead->unk10C  = 0;
	for (int i = 0; i < 8; i++)
		mBody[i]->unk10C = diff * (i + 1);
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

// The target build inlines this into setTumbleAnm() (marioEU.MAP lists the
// symbol as UNUSED), which is why it is declared inline in the header.
void TBossHanachan::setTumbleBckRate_(TBossHanachanPartsBase* part)
{
	J3DFrameCtrl* frameCtrl = part->getMActor()->getFrameCtrl(0);
	f32 diff                 = unk194 - part->getRotation().z;
	diff = (diff >= 0.0f) ? diff : -diff;
	f32 t = (1.0f / unk198) * diff;
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

	for (int i = 0; i < 8; i++) {
		TBossHanachanPartsBody* body = mBody[i];
		if (body->setAnm_(anm, onOff)) {
			J3DFrameCtrl* ctrl = body->getMActor()->getFrameCtrl(0);
			u8 frameDiff = mChangeSaveParams->mSLNormalBckFrameDiff.get();
			s32 frame = (i * frameDiff) % ctrl->getEnd();
			ctrl->setFrame((f32)frame);

			f32 f = (f32)frame;
			J3DFrameCtrl* ctrl3 = body->getMActor()->getFrameCtrl(3);
			if (ctrl3)
				ctrl3->setFrame(f);

			J3DFrameCtrl* ctrl4 = body->getMActor()->getFrameCtrl(4);
			if (ctrl4)
				ctrl4->setFrame(f);
		}
	}
}
