#include <Enemy/BossHanachan.hpp>
#include <Enemy/BossHanachanChangeSaveParams.hpp>
#include <Camera/CameraShake.hpp>
#include <Camera/SunMgr.hpp>
#include <Camera/cameralib.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/Spine.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// Frames of the walk animation at which each foot hits the ground
static const f32 sEmitSandFrameFoot[2] = { 14.0f, 34.0f };
// Frames of the snort animation at which the camera shakes
static const f32 sSnortStepFrames[3] = { 21.0f, 36.0f, 55.0f };

void TBossHanachan::staticLoadParticle()
{
	SMS_LoadParticle("ms_boha_sandsmo.jpa", 0x76);
	SMS_LoadParticle("ms_boha_sand.jpa", 0x77);
	SMS_LoadParticle("ms_boha_jouki_r_a.jpa", 0x78);
	SMS_LoadParticle("ms_boha_jouki_r_b.jpa", 0x79);
	SMS_LoadParticle("ms_boha_jouki_l_a.jpa", 0x7A);
	SMS_LoadParticle("ms_boha_jouki_l_b.jpa", 0x7B);
	SMS_LoadParticle("ms_boha_hamon_a.jpa", 0x7C);
	SMS_LoadParticle("ms_boha_hamon_b.jpa", 0x7D);
	SMS_LoadParticle("ms_boha_crash_a.jpa", COLUMSAND_JPA_MS_BOHA_CRASH_A);
	SMS_LoadParticle("ms_boha_jouki2_r_a.jpa", 0x169);
	SMS_LoadParticle("ms_boha_jouki2_r_b.jpa", 0x16A);
	SMS_LoadParticle("ms_boha_jouki2_l_a.jpa", 0x16B);
	SMS_LoadParticle("ms_boha_jouki2_l_b.jpa", 0x16C);
	SMS_LoadParticle("ms_boha_sandsmo_sl.jpa", 0x16D);
	SMS_LoadParticle("ms_boha_sand_sl.jpa", 0x16E);
	SMS_LoadParticle("ms_boha_kizetsu.jpa", 0x16F);
}

// TODO: all instructions match, but the frame is 0x30 bytes too small and
// the saved registers are allocated differently. Some inline is missing.
void TBossHanachan::emitParticle_()
{
	const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();
	if (nerve == &TNerveBossHanachanDead::theNerve())
		return;

	// TODO: what is this? It is compared against Y coordinates, sea level?
	f32 sunY = gpSunMgr->unk20;
	JGeometry::TVec3<f32> pos;

	if (nerve != &TNerveBossHanachanSnort::theNerve()
	    && mHead->mHitActor->getWaterHitCounter() > 0) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x169, mHead->mNoseHallMtxR, 1, mHead);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x16B, mHead->mNoseHallMtxL, 1, mHead);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x16A, mHead->mNoseHallMtxR, 1, mHead);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x16C, mHead->mNoseHallMtxL, 1, mHead);
	}

	if (nerve == &TNerveBossHanachanTumble::theNerve()
	    || (nerve == &TNerveBossHanachanDamage::theNerve()
	        && mMarchSpeed > 0.001f)) {
		for (int i = 0; i < 8; ++i) {
			pos = mSphereLink->mPoints[i].unkC;
			if (pos.y > sunY)
				gpMarioParticleManager->emitAndBindToPosPtr(
				    0x16E, &mBody[i]->unk154, 1, mBody[i]);
		}
		for (int i = 0; i < 8; ++i) {
			pos = mSphereLink->mPoints[i].unkC;
			if (pos.y > sunY)
				gpMarioParticleManager->emitAndBindToPosPtr(
				    0x16D, &mBody[i]->unk154, 1, mBody[i]);
		}
	}

	if (nerve == &TNerveBossHanachanGraphWander::theNerve()) {
		for (int i = 0; i < 8; ++i) {
			J3DFrameCtrl* ctrl = mBody[i]->getMActor()->getFrameCtrl(0);
			for (int j = 0; j < 2; ++j) {
				if (!ctrl->checkPass(sEmitSandFrameFoot[j]))
					continue;

				MtxPtr footMtx = mBody[i]->mFootHitActor[j]->unk6C;
				if (MsRandF()
				    < mChangeSaveParams->mSLParticleProbability.get()) {
					if (footMtx[1][3] < sunY) {
						MtxPtr legMtx = mBody[i]->mLegMtx[j];
						pos.set(legMtx[0][3], sunY, legMtx[2][3]);
						gpMarioParticleManager->emit(0x7C, &pos, 0, nullptr);
						gpMarioParticleManager->emit(0x7D, &pos, 0, nullptr);
					} else {
						pos.set(footMtx[0][3], footMtx[1][3], footMtx[2][3]);
						gpMarioParticleManager->emit(0x77, &pos, 0, nullptr);
						gpMarioParticleManager->emit(0x76, &pos, 0, nullptr);
					}
				}
			}
		}
	} else if (nerve == &TNerveBossHanachanSnort::theNerve()) {
		if (mHead->getMActor()->getFrameCtrl(0)->checkPass(134.0f)) {
			gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x78, mHead->mNoseHallMtxR, 0, nullptr);
			gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x7A, mHead->mNoseHallMtxL, 0, nullptr);
			gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x79, mHead->mNoseHallMtxR, 0, nullptr);
			gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x7B, mHead->mNoseHallMtxL, 0, nullptr);
		}
	} else if (nerve == &TNerveBossHanachanDown::theNerve()) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x16F, mHead->mMapCollisionJointMtx, 1, mHead);
	}
}

// TODO: frame is 8 bytes too small, everything else matches
void TBossHanachan::emitOneTimeSandPillar_(TBossHanachanPartsBody* body)
{
	onLiveFlag(LIVE_FLAG_UNK10000);
	MtxPtr mtx = body->mMapCollisionJointMtx;
	unk1A0.set(mtx[0][3], mPosition.y, mtx[2][3]);

	TLiveActor* sand = body->getSandActor_();
	if (sand) {
		unk1A0.x = 0.5f * (unk1A0.x + sand->mPosition.x);
		unk1A0.z = 0.5f * (unk1A0.z + sand->mPosition.z);
		unk1A0.y = sand->mPosition.y;
	}

	gpMarioParticleManager->emit(COLUMSAND_JPA_MS_BOHA_CRASH_A, &unk1A0, 0,
	                             nullptr);

	MtxPtr pillarMtx = mSandPillar->getModel()->getBaseTRMtx();
	pillarMtx[0][3]  = unk1A0.x;
	pillarMtx[1][3]  = unk1A0.y;
	pillarMtx[2][3]  = unk1A0.z;
	mSandPillar->setBckFromIndex(0x25);
	mSandPillar->setBtkFromIndex(2);
	mSandPillar->setBrkFromIndex(2);

	gpCameraShake->startShake((EnumCamShakeMode)8, 1.0f);
	SMSGetMSound()->startSoundActor(MSD_SE_BS_HANA_TUMBLE, &unk1A0, 0, nullptr,
	                                0, 4);
}

// TODO: frame is 0x10 bytes too small and the two loop counters of the
// GraphWander branch get swapped registers, everything else matches
void TBossHanachan::emitCamShake_()
{
	if (gpMarDirector->mState == TMarDirector::STATE_UNK1)
		return;

	const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();
	J3DFrameCtrl* ctrl = mBody[0]->getMActor()->getFrameCtrl(0);
	bool marioOnGround = SMS_IsMarioTouchGround4cm();

	if (nerve == &TNerveBossHanachanGraphWander::theNerve()) {
		f32 dist = MsSqrtf(mDistToMarioSquared);
		f32 power;
		if (dist <= mCommonSaveParams->mSLCamShakeMaxDist.get())
			power = 1.0f;
		else if (dist >= mCommonSaveParams->mSLCamShakeZeroDist.get())
			power = 0.0f;
		else {
			f32 ratio
			    = CLBCalcRatio(mCommonSaveParams->mSLCamShakeZeroDist.get(),
			                   mCommonSaveParams->mSLCamShakeMaxDist.get(),
			                   dist);
			power = MsClamp(ratio, 0.0f, 1.0f);
		}

		for (int i = 0; i < 2; ++i) {
			if (ctrl->checkPass(sEmitSandFrameFoot[i])) {
				gpCameraShake->startShake((EnumCamShakeMode)0xB, power);
				if (marioOnGround)
					for (int j = 0; j < 8; ++j)
						SMSRumbleMgr->start(8, &mBody[j]->unk154);
			}
		}
	} else if (nerve == &TNerveBossHanachanSnort::theNerve()) {
		for (int i = 0; i < 3; ++i) {
			if (ctrl->checkPass(sSnortStepFrames[i])) {
				gpCameraShake->startShake((EnumCamShakeMode)0xA, 1.0f);
				if (marioOnGround)
					SMSRumbleMgr->start(8, (f32*)nullptr);
			}
		}
	} else if (nerve == &TNerveBossHanachanGetUp::theNerve()) {
		switch (mBody[0]->mCurAnm) {
		case BH_ANM_KIND_UNK9:
		case BH_ANM_KIND_UNKC:
			if (ctrl->checkPass(9.0f)) {
				gpCameraShake->startShake((EnumCamShakeMode)0xC, 1.0f);
				if (marioOnGround)
					SMSRumbleMgr->start(8, (f32*)nullptr);
				if (checkLiveFlag(LIVE_FLAG_UNK20000))
					MSBgm::stopTrackBGM(1, 30);
			}
			break;
		}
	}
}
