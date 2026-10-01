
#include <Enemy/bosstelesa.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <System/ParamInst.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <math.h>
#include <Enemy/Telesa.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Enemy/HamuKuri.hpp>
#include <System/MarDirector.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <Enemy/Conductor.hpp>
#include <MoveBG/Item.hpp>
#include <MoveBG/ItemManager.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <MarioUtil/ScreenUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <Camera/CameraShake.hpp>
#include <MarioUtil/TexUtil.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <Strategic/Spine.hpp>
#include <Player/MarioAccess.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjManager.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

f32 TBossTelesa::mEnemyGenRate           = 0.5f;
f32 TBossTelesa::mItemGenRate            = 0.1f;
u8 TBossTelesa::mNormalAlpha             = 150;
f32 TBossTelesa::mBaseHoseiPosY          = -300.0f;
f32 TBossTelesa::mRouletteUpRate         = 0.03f;
s32 TBossTelesa::mTelesaGenerateInterval = 400;
f32 TBossTelesa::mCameraMoveLimit        = 1000.0f;
f32 TBossTelesa::mCameraMoveSp           = 0.02f;

void TBossTelesa::setBckAnm(int idx)
{
	mPrevBckIdx = mMActor->getCurAnmIdx(0);
	mCurBckIdx  = idx;
	f32 ratio   = 1.0f;
	mBlendRatio = ratio;
	MActor* actor           = mMActor;
	J3DAnmTransform* oldAnm = actor->getBckAnm();
	MActorAnmBck* bck       = mMActor->getAnmBck();
	if (bck)
		bck->setOldMotionBlendAnmPtr(oldAnm);
	mMActor->setBckFromIndex(idx);
	mMActor->setMotionBlendRatioForBck(mBlendRatio);
	setAnmSound(getBas(idx));
}

TBubbleManager::TBubbleManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TBubbleManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TBubbleSaveLoadParams("/enemy/bubble.prm");
	TSmallEnemyManager::load(stream);
}

TSpineEnemy* TBubbleManager::createEnemyInstance() { return new TBubble; }

void TBubbleManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "btelesa_osenbubbles_ind.bmd", 0x11020000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TBubble::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000020;
	unk150     = 0x11;
	mParams    = (TBubbleSaveLoadParams*)getSaveParam();
	mSpine->initWith(&TNerveBubbleLive::theNerve());
	mMActor->setLightType(3);
	TScreenTexture* screenTexture = static_cast<TScreenTexture*>(
	    JDrama::TNameRefGen::search("スクリーンテクスチャ"));
	SMS_ChangeTextureAll(mMActor->getModel()->getModelData(), "H_ma_rak_dummy",
	                     *screenTexture->getTexture()->getTexInfo());
}

void TBubble::reset()
{
	TWalkerEnemy::reset();
	onLiveFlag(LIVE_FLAG_UNK8);
	TMsRange<f32> range(50.0f, 150.0f);
	unk1CC = range.rand();
	unk1D0 = 0;
	unk1D1 = 1;
	unk1D2 = 0;
	unk198 = nullptr;
	mSpine->initWith(&TNerveBubbleLive::theNerve());
}

void TBubble::split()
{
	int num = mParams->mSLNumDivision.get();
	for (int i = 0; i < num; ++i) {
		TBubble* child = (TBubble*)gpConductor->makeOneEnemyAppear(
		    mPosition, "バブルマネージャー", 1);
		if (!child)
			break;
		child->reset();
		child->mPosition = mPosition;
		child->mPosition.y += unk1CC;
		child->unk1CC = 0.0f;
		child->unk1D0 = 1;
		TMsRange<f32> range(-2.0f, 2.0f);
		JGeometry::TVec3<f32> vel;
		vel.x = range.rand();
		vel.y = range.rand();
		vel.z = range.rand();
		child->mVelocity = vel;
	}
}

f32 TBubble::getGravityY() const
{
	if (unk1D0) {
		if (unk1D1)
			return 0.001f;
		return 0.0f;
	}
	return mGravity;
}

void TBubble::kill()
{
	if (!checkLiveFlag(LIVE_FLAG_DEAD)) {
		if (unk198) {
			if (unk1D2)
				unk198->receiveMessage(this, 7);
			else
				unk198->kill();
			unk198 = nullptr;
		}
		mHitPoints = 1;
		if (mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()) {
			mSpine->reset();
			mSpine->setNext(&TNerveSmallEnemyDie::theNerve());
			mSpine->pushAfterCurrent(mSpine->getDefault());
			onLiveFlag(LIVE_FLAG_UNK20000);
		}
		onLiveFlag(LIVE_FLAG_UNK40);
	}
}

void TBubble::behaveToWater(THitActor*)
{
	if (mSpine->getCurrentNerve() == &TNerveBubbleLive::theNerve()
	    && mMActor->checkCurBckFromIndex(10)) {
		kill();
		TItem* item = (TItem*)gpItemManager->makeObjAppear(
		    mPosition.x, 20.0f + mPosition.y, mPosition.z, 0x20000002, true);
		if (item)
			item->killByTimer(1200);
	}
}

void TBubble::attackToMario()
{
	sendAttackMsgToMario();
	kill();
}

void TBubble::calcRootMatrix()
{
	if (!isEaten()) {
		mPosition.y = 150.0f + (mGroundHeight + unk1CC);
		MsMtxSetXYZRPH(mMActor->getModel()->getBaseTRMtx(), mPosition.x,
		               mPosition.y, mPosition.z, mRotation.x, mRotation.y, mRotation.z);
		mMActor->getModel()->setBaseScale(mScaling);
	}
}

void TBubble::setDeadAnm() { setBckAnm(9); }

MtxPtr TBubble::getTakingMtx()
{
	return mMActor->getModel()->getBaseTRMtx();
}

static const char* btelesa_bastable[] = {
	"/scene/btelesa/bas/btelesa_appear.bas",
	"/scene/btelesa/bas/btelesa_bero_hit.bas",
	"/scene/btelesa/bas/btelesa_damage.bas",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/btelesa/bas/btelesa_lick.bas",
	nullptr,
	nullptr,
	nullptr,
	"/scene/btelesa/bas/btelesa_roll.bas",
	"/scene/btelesa/bas/btelesa_spicy.bas",
	nullptr,
	nullptr,
	"/scene/btelesa/bas/btelesa_wait.bas",
	"/scene/btelesa/bas/btelesa_wet.bas",
};

const char** TBubble::getBasNameTable() const { return btelesa_bastable; }

void TBubble::appendEnemy()
{
	unk198 = nullptr;
	TMsRange<f32> range(0.0f, 100.0f);
	f32 rate = range.rand();
	TSmallEnemy* enemy;
	if (rate < 50.0f) {
		enemy = (TSmallEnemy*)gpConductor->makeOneEnemyAppear(
		    mPosition, "ポポマネージャー", 1);
		enemy->unk154 = 0.6f;
		enemy->reset();
	} else if (rate < 100.0f) {
		enemy = (TSmallEnemy*)gpConductor->makeOneEnemyAppear(
		    mPosition, "ボム兵マネージャー", 1);
		enemy->unk154 = 0.3f;
		enemy->reset();
	} else if (rate < 150.0f) {
		enemy = (TSmallEnemy*)gpConductor->makeOneEnemyAppear(
		    mPosition, "テレサマネージャー", 1);
		if (!enemy)
			return;
		enemy->unk154 = 0.6f;
		enemy->reset();
		((TTelesa*)enemy)->setAttacker();
	} else {
		enemy = (TSmallEnemy*)gpConductor->makeOneEnemyAppear(
		    mPosition, "パックンマネージャー", 1);
		enemy->unk154 = 0.6f;
		enemy->reset();
	}
	if (enemy) {
		if (enemy->receiveMessage(this, 4)) {
			enemy->onHitFlag(HIT_FLAG_UNK8000000);
			mHeldObject = enemy;
			enemy->mVelocity = JGeometry::TVec3<f32>(0.0f, 2.0f, 10.0f);
			enemy->onLiveFlag(LIVE_FLAG_AIRBORNE);
			unk198 = enemy;
		}
	}
}

DEFINE_NERVE(TNerveBubbleLive, TLiveActor)
{
	TBubble* self = (TBubble*)spine->getBody();

	if (spine->getTime() == 0) {
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
		if (!self->unk1D0) {
			self->setBckAnm(8);
		} else {
			self->setBckAnm(10);
			self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));
		}
		J3DFrameCtrl* frameCtrl = self->getMActor()->getFrameCtrl(0);
		TMsRange<f32> range(0.0f, 20.0f);
		frameCtrl->setFrame(range.rand());
		self->onLiveFlag(LIVE_FLAG_UNK8);
	} else if (self->checkCurAnmEnd(0)) {
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
		self->setBckAnm(10);
	}

	f32 addPos = self->mParams->mSLAddPosBase.get();
	if (!self->unk1D0) {
		if (self->unk1CC < addPos)
			self->unk1CC += 2.0f;
	} else {
		if (spine->getTime() > 40 && self->unk1D1) {
			JGeometry::TVec3<f32> tmp = self->mVelocity;
			JGeometry::TVec3<f32> v(tmp.x, tmp.y, tmp.z);
			v.scale(0.98f);
			self->mVelocity = v;
		} else {
			self->walkBehavior(0, 1.0f);
		}
		if (spine->getTime() == 80) {
			self->unk1D1 = 0;
			self->mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
		}
	}

	self->unk1CC += 0.001f;
	if (self->unk1CC > self->mPosition.y + self->mParams->mSLDeadHeight.get()) {
		self->unk1D2 = 0;
		self->kill();
	}
	if (self->mScaling.x < self->mParams->mSLMaxScale.get()) {
		self->mScaling.x = self->mScaling.y = self->mScaling.z
		    = self->mScaling.z * self->mParams->mSLRateExpand.get();
	}
	if (spine->getTime() > self->mParams->mSLLiveTime.get()) {
		spine->pushAfterCurrent(&TNerveBubbleSplit::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBubbleSplit, TLiveActor)
{
	TBubble* self = (TBubble*)spine->getBody();

	if (spine->getTime() == 0) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->split();
	}
	if (spine->getTime() == 10)
		self->setBckAnm(9);
	if (self->checkCurAnmEnd(0) && self->getMActor()->checkCurBckFromIndex(9)) {
		self->unk1D2 = 0;
		self->kill();
	}
	return FALSE;
}

TBossTelesaSaveLoadParams::TBossTelesaSaveLoadParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLDamageRadius, 200)
    , PARAM_INIT(mSLDamageHeight, 100)
    , PARAM_INIT(mSLAttackRadius, 220)
    , PARAM_INIT(mSLAttackHeight, 120)
    , PARAM_INIT(mSLGenAttackerTime, 500)
    , PARAM_INIT(mSLGenBubbleTime, 600)
    , PARAM_INIT(mSLHitAngle, 20.0f)
    , PARAM_INIT(mSLNumGenBubble, 5)
    , PARAM_INIT(mSL1stBubbleSp, 10.0f)
    , PARAM_INIT(mSLHideAreaRadius, 500.0f)
    , PARAM_INIT(mSLSlotItemNum, 5)
    , PARAM_INIT(mSLSlotFruitNum, 10)
    , PARAM_INIT(mSLSlotFirstHitCollectRate, 0.1f)
    , PARAM_INIT(mSLSlotHitCollectRate, 0.1f)
    , PARAM_INIT(mSLTransYOffset, 350.0f)
    , PARAM_INIT(mSLStopSlotTime0, 3000)
    , PARAM_INIT(mSLStopSlotTime1, 2000)
    , PARAM_INIT(mSLStopSlotTime2, 1000)
    , PARAM_INIT(mSLSpicyTime, 2000)
    , PARAM_INIT(mSLPrepareSlotTime, 400)
{
	TParams::load(mPrmPath);
}

TBossTelesaManager::TBossTelesaManager(const char* name)
    : TEnemyManager(name)
{
}

void TBossTelesaManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TBossTelesaSaveLoadParams("/enemy/bosstelesa.prm");
	TEnemyManager::load(stream);
}

TSpineEnemy* TBossTelesaManager::createEnemyInstance()
{
	return new TBossTelesa("ボステレサ");
}

void TBossTelesaManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "btelesa.bmd", 0x15300000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TBossTelesaManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TEnemyManager::perform(cue, graphics);
}


BOOL TBossTelesaBody::receiveMessage(THitActor* sender, u32 message)
{
	TBossTelesa* boss = mOwner;
	if (message == 0 && !sender->isActorType(0x80000001)) {
		if (boss->mSpine->getCurrentNerve()
		    == &TNerveBossTelesaPrepareSlot::theNerve())
			boss->mSpine->pushNerve(&TNerveBossTelesaSpit::theNerve());
	}
	if (message == 15) {
		if (boss->mSpine->getCurrentNerve()
		    == &TNerveBossTelesaPrepareSlot::theNerve())
			boss->mSpine->pushNerve(&TNerveBossTelesaFreeze::theNerve());
	}
	return TRUE;
}

BOOL TBossTelesaTongue::receiveMessage(THitActor*, u32 message)
{
	if (message == 15) {
		TBossTelesa* boss = mOwner;
		if (boss->mSpine->getCurrentNerve()
		    == &TNerveBossTelesaAppear::theNerve())
			boss->mSpine->pushNerve(&TNerveBossTelesaSlotStart::theNerve());
	}
	return TRUE;
}

void TBossTelesaKillSmallEnemy::checkHit()
{
	mHit = false;
	for (int i = 0; i < mColCount; ++i) {
		THitActor* actor = mCollisions[i];
		if (actor->checkActorType(0x10000000)) {
			u32 type         = actor->getActorType();
			TLiveActor* live = (TLiveActor*)actor;
			if (type == 0x10000013) {
				THamuKuri* kuri = (THamuKuri*)live;
				kuri->selectCapHolder();
			}
			live->kill();
		}
	}
	JGeometry::TVec3<f32> diff = SMS_GetMarioPos();
	diff -= mPosition;
	diff.y = 0.0f;
	if (MsVECMag2((Vec*)&diff) < 300.0f) {
		mOwner->forceHide();
		mHit = true;
	}
}

void TTelesaSlot::initMapObj()
{
	TSlotDrum::initMapObj();
	onLiveFlag(LIVE_FLAG_UNK10);
	unk14C = 160.0f;
	unk150 = mPosition.y;
	unk154 = 2.0f;
	unk158 = 2.0f;
	unk15C = 0.01f;
	unk160 = 0.5f;
	unk164 = 0;
	unk168 = 45;
	unk140 = mDamageRadius / 3.0f;
	unk144 = mDamageHeight;
	mMapCollision = new TMapCollisionMove;
	mMapCollision->init(2, 0, 0, nullptr);
	randomReset();
}

void TTelesaSlot::randomReset()
{
	TMsRange<s32> range(0, 8);
	for (int i = 0; i < 3; ++i) {
		unk13C[i]   = unk168 * range.rand();
		mRolling[i] = false;
	}
}

void TTelesaSlot::calcRootMatrix()
{
	u8 spinning = 0;
	for (int i = 0; i < 3; ++i) {
		if (0.0f != unk138[i])
			spinning = 1;
	}
	if (spinning) {
		if (unk1E0)
			SMSGetMSound()->startSoundActor(MSD_SE_OBJ_SLOT_SPIN, &mPosition,
			                                0, nullptr, 0, 4);
		unk1E0 = 1 - unk1E0;
	}
	TSlotDrum::calcRootMatrix();
}

void TTelesaSlot::moveObject()
{
	TLiveActor::moveObject();
	for (int i = 0; i < unk148; ++i) {
		if (mStopped[i]) {
			if (unk1A4 == getForcastResult(i)) {
				mRolling[i] = false;
				mStopped[i] = false;
			}
		}
		if (unk138[i] != 0.0f) {
			if (fabsf(unk138[i]) > unk160) {
				unk13C[i] += unk138[i];
				if (!mRolling[i]) {
					if (unk138[i] > 0.0f)
						unk138[i] -= unk15C;
					else
						unk138[i] += unk15C;
				}
				if (unk13C[i] >= 360.0f)
					unk13C[i] -= 360.0f;
				if (unk13C[i] <= 0.0f)
					unk13C[i] += 360.0f;
			} else {
				unk13C[i] += unk138[i];
				if (unk13C[i] >= 360.0f)
					unk13C[i] -= 360.0f;
				if (unk13C[i] <= 0.0f)
					unk13C[i] += 360.0f;
				if (!mRolling[i] && (int)fabsf(unk13C[i]) % unk168 == 0) {
					unk13C[i] = (f32)(unk168 * (int)(unk13C[i] / (f32)unk168));
					unk138[i] = 0.0f;
					SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_SLT_STOP,
					                                &mPosition, 0, nullptr, 0,
					                                4);
					for (int j = 0; j < unk148; ++j) {
						if (mRolling[j]) {
							TMsRange<f32> range(0.0f, 1.0f);
							f32 rate = mOwner->mParams->mSLSlotHitCollectRate.get();
							if (range.rand() <= rate)
								mStopped[j] = true;
							else
								mRolling[j] = false;
						}
					}
					u8 allStopped = 1;
					for (int k = 0; k < 3; ++k) {
						if (0.0f != unk138[k])
							allStopped = 0;
					}
					if (allStopped) {
						TBossTelesa* boss = mOwner;
						if (boss->mSlot->getSlotResult() == 2
						    || boss->mSlot->getSlotResult() == 0) {
							boss->unk374.zero();
							gpMarioParticleManager->emit(225, &boss->unk374, 0,
							                             nullptr);
							if (boss->mSlot->getSlotResult() == 2)
								SMSGetMSound()->startSoundActor(
								    MSD_SE_BS_TELESA_FANFALE_1,
								    &boss->mPosition, 0, nullptr, 0, 4);
							else
								SMSGetMSound()->startSoundActor(
								    MSD_SE_BS_TELESA_FANFALE_2,
								    &boss->mPosition, 0, nullptr, 0, 4);
						} else {
							SMSGetMSound()->startSoundActor(
							    MSD_SE_BS_TELESA_FANFALE_3, &boss->mPosition, 0,
							    nullptr, 0, 4);
						}
					}
				}
			}
		}
	}
}

void TTelesaSlot::moveStart()
{
	unk19C = true;
	unk19B = true;
	for (int i = 0; i < 3; ++i) {
		mRolling[i] = true;
		mStopped[i] = false;
		f32 f       = 1.0f;
		if (i == 0)
			f = -1.0f;
		if (i == 1)
			f = -0.8f;
		unk138[i] = f * unk158;
	}
}

u32 TTelesaSlot::touchWater(THitActor*) { return 0; }

void TTelesaSlot::forceStopSlot(int idx)
{
	TMsRange<f32> range(0.0f, 1.0f);
	if (unk19C) {
		f32 rate = mOwner->mParams->mSLSlotFirstHitCollectRate.get();
		if (SMS_GetMarioHP() == 1)
			rate = 0.9f;
		if (range.rand() <= rate) {
			unk1A4 = 2;
			if (SMS_GetMarioHP() <= 3)
				unk1A4 = 0;
			mStopped[idx] = true;
		} else {
			unk1A4        = getForcastResult(idx);
			mRolling[idx] = false;
		}
		if (unk1A4 == mOwner->unk1A8)
			unk1A4 = 3;
		if (unk1A4 == 0) {
			if (mOwner->unk370 == 0)
				unk1A4 = 1;
			else if (SMS_GetMarioHP() >= 6)
				unk1A4 = 3;
		}
		unk19C = false;
	}
}

bool TTelesaSlot::isRollDrum()
{
	if (mRolling[0])
		return true;
	if (mRolling[1])
		return true;
	if (mRolling[2])
		return true;
	unk19B = false;
	return false;
}

// The ROM calls this out of line from moveObject().
#pragma dont_inline on
int TTelesaSlot::getSlotResult()
{
	int result = getDrumResult(0);
	for (int i = 1; i < 3; ++i)
		if (result != getDrumResult(i))
			return -1;
	return result;
}
#pragma dont_inline off

int TTelesaSlot::getDrumResult(int i)
{
	f32 ang = unk13C[i];
	return getResultFromAng(ang);
}

int TTelesaSlot::getForcastResult(int idx)
{
	f32 angle = unk13C[idx];
	f32 speed = unk138[idx];
	int n = 0;
	do {
		if (fabsf(speed) > unk160) {
			angle += speed;
			if (speed > 0.0f)
				speed -= unk15C;
			else
				speed += unk15C;
			if (angle >= 360.0f)
				angle -= 360.0f;
			if (angle <= 0.0f)
				angle += 360.0f;
		} else {
			angle += speed;
			if (angle >= 360.0f)
				angle -= 360.0f;
			if (angle <= 0.0f)
				angle += 360.0f;
			if ((int)fabsf(angle) % unk168 == 0) {
				angle = (int)(angle / unk168) * unk168;
				break;
			}
		}
	} while (++n <= 10000);
	return getResultFromAng(angle);
}

int TTelesaSlot::getResultFromAng(f32 ang)
{
	if (ang < 44.0f)
		return 0;
	if (ang < 89.0f)
		return 1;
	if (ang < 134.0f)
		return 3;
	if (ang < 179.0f)
		return 2;
	if (ang < 224.0f)
		return 0;
	if (ang < 269.0f)
		return 1;
	if (ang < 314.0f)
		return 3;
	if (ang < 359.0f)
		return 2;
	return 2;
}

TBossTelesa::TBossTelesa(const char* name)
    : TSpineEnemy(name)
{
	unk150 = 1;
	unk154 = nullptr;
	unk158 = nullptr;
	mParams = nullptr;
	mCurBckIdx = -1;
	mPrevBckIdx = -1;
	mBlendRatio = 0.0f;
	mHitActors[0] = nullptr;
	mSlot = nullptr;
	unk188 = nullptr;
	unk18C = 0;
	unk1A8 = 0;
	mItemNum = 0;
	unk350 = 0;
	unk354 = nullptr;
	unk358 = 0;
	unk35A = 1;
	unk35B = 1;
	unk35C = nullptr;
	unk360 = 0.0f;
	unk364 = 0.0f;
	unk368 = nullptr;
	unk36C = nullptr;
	unk370 = 3;
	unk384 = 0;
}

void TBossTelesa::reset()
{
	TSpineEnemy::reset();
	onHitFlag(HIT_FLAG_NO_COLLISION);
	onLiveFlag(LIVE_FLAG_UNK8);
	onLiveFlag(LIVE_FLAG_UNK10);
	onLiveFlag(LIVE_FLAG_HIDDEN);
	unk18C = 0;
	TBossTelesaSaveLoadParams* params = mParams;
	setHitParams(params->mSLAttackRadius.get(), params->mSLAttackHeight.get(),
	             params->mSLDamageRadius.get(), params->mSLDamageHeight.get());
	gpMarDirector->fireStartDemoCamera("btelesa_roll_camera", nullptr, -1, 0.0f,
	                                   true, nullptr, 0, nullptr,
	                                   JDrama::TFlagT<u16>(0));
}

void TBossTelesa::loadAfter()
{
	// PAL first gathers the stage's actor-type 0x4000019A objects into the
	// three references at this+0x178, then finds actor type 0x400001A6 and
	// gives it a back-reference to this boss at object offset 0x1A0.
	const u32 slotActorType = 0x4000019A;
	if (gpMapObjManager->getObjNumWithActorType(slotActorType)) {
		int foundIndex = 0;
		for (int i = 0; i < gpMapObjManager->getObjNum(); i++) {
			TMapObjBase* obj = gpMapObjManager->getObj(i);
			// the `? 1 : 0` materialises the bool the ROM branches on
			if (obj->getActorType() == slotActorType ? true : false)
				mStageSlotObjects[foundIndex++] = obj;
		}
	}

	const u32 ownerActorType = 0x400001A6;
	if (gpMapObjManager->getObjNumWithActorType(ownerActorType)) {
		for (int i = 0; i < gpMapObjManager->getObjNum(); i++) {
			TMapObjBase* obj = gpMapObjManager->getObj(i);
			if (obj->getActorType() == ownerActorType ? true : false) {
				mSlot         = (TTelesaSlot*)obj;
				mSlot->mOwner = this;
			}
		}
	}

	// Rebuild the 20 fruit/coin prop references.  Each entry resolves its
	// .bas path to a TNameRef, hashes it, then walks the tree with
	// `TNameRef::searchF(key, name)` to reach the concrete node.
	for (int i = 0; i < 20; i++) {
		const char* name = btelesa_bastable[i];
		u16 key = JDrama::TNameRef::calcKeyCode(name);
		JDrama::TNameRef* ref =
		    JDrama::TNameRefGen::getInstance()->getRootNameRef();
		this->mSlotFruits[i] = (TMapObjBase*)ref->searchF(key, name);
	}

	// 25 prop instances pre-registered at the origin, in five separate
	// unrolled loops (6 + 6 + 2 + 6 + 5) that all store through the
	// single running index `n` at this+0x2A8 + 4*n - so the last five
	// spill past mSlotFruits[20] into the five pointers after it.
	//
	// All three vector arguments are omitted, i.e. they are
	// newAndRegisterObj's defaults: position (0,0,0), rotation (0,0,0),
	// scale (1,1,1).  That is why each loop materialises its own private
	// triple of stack temporaries (0x314/0x320/0x32C for the first loop,
	// 0x2F0/0x2FC/0x308 for the second, and so on downwards) instead of
	// sharing one set - five call sites, five sets.  Writing the vectors
	// out by hand will NOT reproduce that.
	int n = 0;
	for (int i = 0; i < 6; i++)
		mSlotFruits[n++] = gpMapObjManager->newAndRegisterObj("FruitCoconut");
	for (int i = 0; i < 6; i++)
		mSlotFruits[n++] = gpMapObjManager->newAndRegisterObj("FruitPapaya");
	for (int i = 0; i < 2; i++)
		mSlotFruits[n++] = gpMapObjManager->newAndRegisterObj("FruitPine");
	for (int i = 0; i < 6; i++)
		mSlotFruits[n++] = gpMapObjManager->newAndRegisterObj("FruitDurian");
	for (int i = 0; i < 5; i++)
		mSlotFruits[n++] = gpMapObjManager->newAndRegisterObj("bottle_large");

	// The 22 boss particle effects, each guarded by its one-byte
	// `gParticleFlagLoaded` sentinel so it is only ever loaded once.  The
	// ROM emits these as 22 unrolled blocks - no loop - each of the shape
	//
	//   lis/addi r3, gParticleFlagLoaded ; addi rN, r3, <id>
	//   lbz r0, <id>(r3) ; cmplwi r0, 0 ; bne .L_skip
	//   lwz r3, gpResourceManager ; addi r4, <path> ; li r5, <id>
	//   bl load__18JPAResourceManagerFPCcUs
	//   li r0, 1 ; stb r0, 0x0(rN)
	//
	// so this must stay unrolled: a loop would fold the address
	// computation and change the block shape.  The IDs are 0xD7..0xE1,
	// then 0x19E..0x1A7, then 0x1F0 - not one contiguous range.
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_fhit.jpa", 0xD7);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_fhit_pe.jpa", 0xD8);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_fhit_gr.jpa", 0xD9);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_fhit_or.jpa", 0xDA);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_damage.jpa", 0xDB);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_down.jpa", 0xDC);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_down_pe.jpa", 0xDD);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_down_gr.jpa", 0xDE);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_down_or.jpa", 0xDF);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_spicy_hit.jpa", 0xE0);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_fubuki.jpa", 0xE1);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_yodare1.jpa", 0x19E);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_yodare2.jpa", 0x19F);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_yodare3.jpa", 0x1A0);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_ase.jpa", 0x1A1);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_spicy_a.jpa", 0x1A2);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_spicy_b.jpa", 0x1A3);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_spicy_d.jpa", 0x1A4);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_chika_a.jpa", 0x1A5);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_chika_b.jpa", 0x1A6);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_glow.jpa", 0x1A7);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_spicy_c.jpa", 0x1F0);

	// TODO(bosstelesa, measured, 47.4 % -> stalled here): two things are
	// still missing, and both are worth more than the rest of the tuning.
	//
	// 1. THE FRAME.  The ROM's frame is 0x370 (880 B); ours is 0x130
	//    (304 B).  Everything in the ROM's 0x0..0x283 is DEAD - not one
	//    `addi rX, r1, imm` and not one load or store touches it; the five
	//    newAndRegisterObj vector triples at 0x284..0x337 are the only
	//    locals, and the save area is 0x340..0x36F.  A `char` pad sized by
	//    bisection to 576 B reproduces 0x370 exactly and the prologue then
	//    matches instruction for instruction - but it moves the SCORE NOT
	//    AT ALL (47.4 % either way), so it is a fakematch that buys
	//    nothing and has been removed again.  The real answer is almost
	//    certainly a local the original declared that MWCC allocated and
	//    then never referenced.  576 = 36 * 16; 20 entries * 32 would be
	//    640 and overshoots by exactly 64, the size of an 8-register
	//    save area, which is suggestive but not conclusive.
	//
	// 2. REGISTER PRESSURE.  With the frame matched, the only prologue
	//    difference left is `stmw r24` (6 callee-saved: r24..r29) against
	//    our `stmw r25` (5: r25..r29), and everything downstream shifts
	//    with it - the ROM keeps `this` in r31 and the rodata base in r30,
	//    we keep `this` in r29 and the rodata base in r31.  The original
	//    therefore has exactly one more long-lived value live across this
	//    function than we do.  That is the same missing local as (1), seen
	//    from the other side, and finding it should fix both.
	//
	// 3. MISSING TAIL.  After the 22 particle loads the ROM still has a
	//    block we do not emit at all: `bl TNameRef::calcKeyCode`, then an
	//    INDIRECT call through vtable slot +0x1C with a string argument at
	//    .rodata+0x864 (`addi r5, r30, 0x864` / `mtlr r12` / `blrl`), then
	//    `lwz r0, 0xf0(r3)` / `ori r0, r0, 1` / `stw r0, 0xf0(r3)` to set a
	//    flag on the returned object, and finally
	//    `mr r3, r31` / `bl JDrama::TNameRef::loadAfter()`.  That last call
	//    takes `this` (r31 = the TBossTelesa) as its argument, so it is a
	//    TNameRefGen-style deferred-load hook, not a TNameRef member.
}

void TBossTelesa::kill()
{
	if (mSpine->getCurrentNerve() != &TNerveBossTelesaDie::theNerve())
		mSpine->pushNerve(&TNerveBossTelesaDie::theNerve());
}

MtxPtr TBossTelesa::getTakingMtx()
{
	mTakingMtx.set(mStageSlotObjects[0]->mMActor->getModel()->getAnmMtx(1));
	mTakingMtx.ref(1, 3) = mStageSlotObjects[0]->mPosition.y - 120.0f;
	return mTakingMtx;
}

void TBossTelesa::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if ((cue & CUE_ENTRY) && !checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_CLIPPED_OUT)
	    && mSpine->getCurrentNerve() == &TNerveBossTelesaDie::theNerve()
	    && unk350) {
		mMActor->offMakeDL();
		SMS_AddDamageFogEffect(mMActor->getModel()->getModelData(), mPosition,
		                       graphics);
	}
	offLiveFlag(LIVE_FLAG_CLIPPED_OUT);
	TSpineEnemy::perform(cue, graphics);
	mHitActors[0]->THitActor::perform(cue, graphics);
	mHitActors[1]->THitActor::perform(cue, graphics);
	mHitActors[2]->THitActor::perform(cue, graphics);
	if (mSlot)
		mSlot->testPerform(cue, graphics);
	if (unk188)
		unk188->testPerform(cue, graphics);
}

BOOL TBossTelesa::receiveMessage(THitActor*, u32) { return FALSE; }

const char** TBossTelesa::getBasNameTable() const { return btelesa_bastable; }

// The slot machine: 28 calls, 13 of them rand().  Three reels, three
// possible pay-outs (fruits / coins / enemy managers), each launched out
// of the boss's 5th joint matrix with a random yaw.
//
// TODO(bosstelesa): `manNameTable` is emitted by the ROM as the
// function-local static `manNameTable$3929`; MWCC numbers function-local
// statics itself so the `$NNNN` suffix will not line up.  The nine
// .rodata strings and the 36-byte .data table are otherwise recovered.
void TBossTelesa::generateSlotItem()
{
	// Enemy-manager names, indexed by the same slot number as gpConductor.
	// The nine names are written as raw Shift-JIS escapes because the rest of
	// this file stores Japanese as a mojibake of the SJIS bytes, and
	// re-mojibaking the escapes would not round-trip.  In order they read
	// "Bubble manager", "Hamukuri manager", "Yakiguri manager",
	// "Bomhei manager", "Poihana manager", "Denki-NokoNoko manager",
	// "Popo manager", "Getso manager", "Tobi-Puku manager".
	static const char* manNameTable[] = {
		"\x83\x6f\x83\x75\x83\x8b\x83\x7d\x83\x6c\x81\x5b\x83\x57\x83\x83\x81\x5b",
		"\x83\x6e\x83\x80\x83\x4e\x83\x8a\x83\x7d\x83\x6c\x81\x5b\x83\x57\x83\x83\x81\x5b",
		"\x83\x84\x83\x4c\x83\x4f\x83\x8a\x83\x7d\x83\x6c\x81\x5b\x83\x57\x83\x83\x81\x5b",
		"\x83\x7b\x83\x80\x95\xba\x83\x7d\x83\x6c\x81\x5b\x83\x57\x83\x83\x81\x5b",
		"\x83\x7c\x83\x43\x83\x6e\x83\x69\x83\x7d\x83\x6c\x81\x5b\x83\x57\x83\x83\x81\x5b",
		"\x93\x64\x8b\x43\x83\x6d\x83\x52\x83\x6d\x83\x52\x83\x7d\x83\x6c\x81\x5b\x83\x57\x83\x83\x81\x5b",
		"\x83\x7c\x83\x7c\x83\x7d\x83\x6c\x81\x5b\x83\x57\x83\x83\x81\x5b",
		"\x83\x51\x83\x62\x83\x5c\x81\x5b\x83\x7d\x83\x6c\x81\x5b\x83\x57\x83\x83\x81\x5b",
		"\x82\xc6\x82\xd1\x83\x76\x83\x4e\x83\x7d\x83\x6c\x81\x5b\x83\x57\x83\x83\x81\x5b",
	};

	unk368 = 0;
	mItemNum = 0;

	// The result only counts when all three drums show the same symbol;
	// any mismatch is recorded as -1 ("no jackpot").
	TTelesaSlot* slot = mSlot;
	int result = slot->getResultFromAng(slot->unk13C[0]);
	for (int i = 1; i < 3; i++) {
		if (result != slot->getResultFromAng(slot->unk13C[i])) {
			result = -1;
			break;
		}
	}
	unk1A8 = result;

	TBossTelesaSaveLoadParams* params = mParams;
	s32 itemNum = params->mSLSlotItemNum.get();
	MtxPtr slotMtx = mMActor->getModel()->getAnmMtx(5);
	f32 itemStep = 120.0f / (f32)itemNum;
	f32 yOffset = itemStep * (f32)itemNum * 0.5f;

	if (unk1A8 == 2) {
		// ------------------------------------------------ fruits
		s32 n = params->mSLSlotFruitNum.get();
		if (n > 20)
			n = 20;
		TMsRange<s32> range(0, n);
		s32 idx = range.rand();
		f32 fruitStep = 160.0f / (f32)n;
		Mtx mtx;

		for (s32 i = 0; i < n; i++) {
			TMapObjBase** fruit = mSlotFruits + i;
			if (mSlotFruits[i]->mHolder)
				continue;

			JGeometry::TVec3<f32> vec;
			vec.set(0.0f, 0.0f, 200.0f);
			MsMtxSetRotRPH(mtx, mScaling.x,
			               160.0f / (f32)idx * (f32)idx + (mScaling.y - yOffset),
			               mScaling.z);
			idx = MsWrap<s32>(idx + i, 0, n);
			PSMTXMultVec(mtx, (Vec*)&vec, (Vec*)&vec);

			JGeometry::TVec3<f32> dir;
			MsVECNormalize((Vec*)&vec, (Vec*)&dir);

			// The ROM only ever writes .y and .z of this vector and then
			// reads their difference back, so the .x slot is a real but
			// permanently unused local.
			JGeometry::TVec3<f32> spread;
			spread.y = 6.0f;
			spread.z = 10.0f;
			f32 spreadDiff = spread.z - spread.y;

			if (i == 0 || i == 4) {
				// The two hidden bonus fruits live past the twenty
				// pre-registered ones.
				fruit = mSlotFruits + i + 20;
				(*fruit)->makeObjAppeared();
				(*fruit)->offLiveFlag(0x9FFFFFFF);
				(*fruit)->mVelocity.x
				    = dir.x * (spread.y + (dir.z - spread.y) * MsRandF());
				(*fruit)->mVelocity.y = -2.0f;
				(*fruit)->mVelocity.z
				    = dir.z * (spread.y + spreadDiff * MsRandF());
				if (i == 0) {
					(*fruit)->mVelocity.x = dir.x
					                        * (spread.y
					                           + (dir.z - spread.y) * MsRandF());
					(*fruit)->mVelocity.y = -2.0f;
					(*fruit)->mVelocity.z = dir.z
					                        * (spread.y
					                           + spreadDiff * MsRandF())
					                        * 2.0f;
				}
				(*fruit)->offLiveFlag(0xE3FFFFFF);
			} else {
				(*fruit)->makeObjAppeared();
				(*fruit)->offLiveFlag(0x9FFFFFFF);
				(*fruit)->mVelocity.x
				    = dir.x * (spread.y + (dir.z - spread.y) * MsRandF());
				(*fruit)->mVelocity.y = -2.0f;
				(*fruit)->mVelocity.z
				    = dir.z * (spread.y + spreadDiff * MsRandF());
				(*fruit)->offLiveFlag(0xE3FFFFFF);
			}
			(*fruit)->mRotation.set(0.0f, 90.0f, 0.0f);
			mItems[mItemNum] = *fruit;
			mItems[i]->onHitFlag(HIT_FLAG_NO_COLLISION);
			mItems[i]->mScaling.set(1.5f, 1.5f, 1.5f);
			mItems[mItemNum]->mPosition.set(slotMtx[0][3] + vec.x,
			                               slotMtx[1][3] - 50.0f,
			                               slotMtx[2][3] + vec.z);
			mItemNum++;
		}
	} else if (unk1A8 == 0) {
		// ------------------------------------------------ coins
		s32 n = params->mSLSlotFruitNum.get();
		if (n > 10)
			n = 10;
		f32 coinStep = 120.0f / (f32)n;
		f32 coinY = coinStep * (f32)n * 0.5f;
		if (unk370)
			unk370--;
		else
			unk370 = 0;

		Mtx mtx;
		for (s32 i = 0; i < n; i++) {
			if (i >= 10)
				break;
			JGeometry::TVec3<f32> dir;
			dir.set(0.0f, 0.0f, 250.0f);
			MsMtxSetRotRPH(mtx, mScaling.x,
			               coinStep * (f32)i + (mScaling.y - coinY),
			               mScaling.z);
			PSMTXMultVec(mtx, (Vec*)&dir, (Vec*)&dir);
			MsVECNormalize((Vec*)&dir, (Vec*)&dir);

			JGeometry::TVec3<f32> spread;
			spread.y = 0.8f;
			spread.z = 3.5f;
			f32 spreadDiff = spread.z - spread.y;
			dir.y = 10.0f;

			f32 kick = mSlot->unk158;
			dir.x = dir.x * (spread.y + spreadDiff * MsRandF()) * kick;
			dir.z = dir.z * (spread.y + spreadDiff * MsRandF()) * kick;

			TMapObjBase* coin = gpItemManager->makeObjAppeared(0x200E);
			coin->mPosition.set(slotMtx[0][3], slotMtx[1][3] - 250.0f,
			                    slotMtx[2][3]);
			coin->mVelocity.set(dir.x, dir.y, dir.z);
			coin->offLiveFlag(0xE3FFFFFF);
			coin->mRotation.set(0.0f, 0.0f, 0.0f);
			((TItem*)coin)->killByTimer(0x3C0);
			mItems[mItemNum] = mCoins[i];
			mItems[mItemNum]->offLiveFlag(0x9FFFFFFF);
			mItemNum++;
		}
	} else {
		// ------------------------------------- enemy managers
		s32 n = params->mSLSlotItemNum.get();
		s32 idx = 0;
		switch (unk1A8) {
		case 1:
			idx = mHitPoints > 2 ? 1 : 2;
			break;
		case -1:
			n *= 2;
			idx = 0;
			break;
		case 3:
			break;
		}

		s32 lo = 1;
		s32 hi = 7;
		if (mHitPoints == 1) {
			lo = 1;
			hi = 8;
		}
		TMsRange<s32> range(lo, hi);
		s32 pick = range.rand();

		Mtx mtx;
		for (s32 i = 0; i < n; i++) {
			if (unk1A8 == 3) {
				if (((pick / 2) * 2) - i == 0)
					pick++;
				if (pick > hi)
					pick = 1;
				idx = pick;
			}
			// gpConductor is declared as a single TConductor*, but the ROM
			// indexes an array of conductors with it (the slot number ends
			// up in the base register of one SDA21-relative load), so the
			// cast is spelled out here.  TODO(bosstelesa): the declaration
			// in include/Enemy/Conductor.hpp is probably wrong.
			TConductor* cond = ((TConductor**)gpConductor)[idx];
			TWalkerEnemy* enemy
			    = (TWalkerEnemy*)cond->makeOneEnemyAppear(
			        mPosition, manNameTable[idx], 2);
			if (enemy == nullptr)
				continue;
			if (idx != 0) {
				mItems[mItemNum] = (TMapObjBase*)enemy;
				mItemNum++;
			}

			JGeometry::TVec3<f32> dir;
			dir.set(0.0f, 0.0f, 200.0f);
			MsMtxSetRotRPH(mtx, mScaling.x,
			               itemStep * (f32)i + (mScaling.y - yOffset),
			               mScaling.z);
			PSMTXMultVec(mtx, (Vec*)&dir, (Vec*)&dir);
			MsVECNormalize((Vec*)&dir, (Vec*)&dir);

			JGeometry::TVec3<f32> spread;
			spread.y = 0.5f;
			spread.z = 1.0f;
			f32 spreadDiff = spread.z - spread.y;
			dir.y = 2.0f;

			f32 kick = mSlot->unk158;
			dir.x = dir.x * (spread.y + spreadDiff * MsRandF()) * kick;
			dir.y = dir.y
			       * (spread.y + spreadDiff * MsRandF() + 2.0f);
			dir.z = dir.z * (spread.y + spreadDiff * MsRandF()) * kick;

			enemy->mPosition.set(slotMtx[0][3], slotMtx[1][3] - 250.0f,
			                     slotMtx[2][3]);
			enemy->mVelocity = dir;
			enemy->mPosition.y += 10.0f;
			enemy->onLiveFlag(LIVE_FLAG_AIRBORNE);
			PSMTXCopy(slotMtx, enemy->mMActor->getModel()->getBaseTRMtx());
			enemy->mMActor->calc();
			enemy->initAttacker(this);
		}
	}
}

// forceHide() calls this out of line in the ROM.
#pragma dont_inline on
void TBossTelesa::forceAllItemKill()
{
	for (int i = 0; i < mItemNum; ++i) {
		if (mItems[i]->mHolder) {
			SMS_SendMessageToMario(mItems[i], 8);
			mItems[i]->mHolder = nullptr;
		}
		mItems[i]->mPosition.set(0.0f, 0.0f, 0.0f);
		mItems[i]->onHitFlag(HIT_FLAG_NO_COLLISION);
		if (!mItems[i]->checkLiveFlag(LIVE_FLAG_DEAD)) {
			mItems[i]->kill();
			gpMarioParticleManager->emit(PARTICLE_MS_TLS_CHANGE,
			                             &mItems[i]->mPosition, 0, nullptr);
		}
	}
}
#pragma dont_inline off

#pragma dont_inline on
void TBossTelesa::forceHide()
{
	if (mSpine->getCurrentNerve() != &TNerveBossTelesaDie::theNerve()
	    && !mMActor->checkCurBckFromIndex(4)
	    && !mMActor->checkCurBckFromIndex(0)) {
		forceAllItemKill();
		unk368 = nullptr;
		if (unk350)
			SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_ESCAPE, &mPosition,
			                                0, nullptr, 0, 4);
		else
			SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_DISAPPEAR,
			                                &mPosition, 0, nullptr, 0, 4);
		mSpine->reset();
		mSpine->setNext(&TNerveBossTelesaHide::theNerve());
	}
}
#pragma dont_inline off

DEFINE_NERVE(TNerveBossTelesaDie, TLiveActor)
{
	TBossTelesa* self = (TBossTelesa*)spine->getBody();

	// First entry: tick the death counter down and pop the boss's two
	// sub-actors out of collision while their hit points remain.  None of
	// these branches return -- they all fall through to the shared tail
	// below, which is what the ROM's `b` out of each branch shows.
	if (spine->getTime() == 0) {
		self->unk388 = 0;
		if (self->unk350 && self->mHitPoints != 0)
			self->mHitPoints--;

		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->mHitActors[0]->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->mHitActors[1]->onHitFlag(HIT_FLAG_NO_COLLISION);

		if (self->mHitPoints != 0) {
			if (self->unk350) {
				self->getMActor()->setBrkFromIndex(1);
				self->setBckAnm(2);
				gpCameraShake->startShake(
				    static_cast<EnumCamShakeMode>(0x1f), 1.0f);
			} else {
				self->setBckAnm(5);
				self->getMActor()->setBrkFromIndex(0);
				gpCameraShake->startShake(
				    static_cast<EnumCamShakeMode>(0x20), 1.0f);
			}
		} else {
			// final blow
			MSBgm::stopBGM(0x8001000D, 10);
			gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x21), 1.0f);
			self->setBckAnm(3);
			self->getMActor()->setBrkFromIndex(1);
			// mSlot + 0x24 is TActor::mScaling (THitActor derives from
			// JDrama::TActor), zeroed through `stfsu` + two `stfs`.
			self->mSlot->mScaling.set(0.0f, 0.0f, 0.0f);
			if (gpMSound->gateCheck(MSD_SE_BS_TELESA_DOWN))
				MSoundSESystem::MSoundSE::startSoundActor(
				    MSD_SE_BS_TELESA_DOWN, &self->mPosition, 0, nullptr, 0, 4);
		}
	}

	// Shared tail.  Once hit points are gone, run the burst and shine
	// sequence; while they remain, walk the death-animation ladder.  The
	// ROM branches *over* the burst block when hit points are still up,
	// so the burst has to be the fall-through here.
	if (self->mHitPoints == 0) {
		// The burst runs once the animation has finished, then a 0xf0-tick
		// delay hands over to TNerveBossTelesaPrepareSlot.
		if (self->checkCurAnmEnd(0)) {

			if (self->unk388 == 0) {
				// unk374 is node 1's translation on the boss model.  The chain
				// is mMActor -> model (+0x4) -> mNodeMatrices (+0x58), i.e.
				// getModel()->getAnmMtx(1); the translation column then
				// reads at +0xc/+0x1c/+0x2c, i.e.
				// mtx[0][3]/mtx[1][3]/mtx[2][3].
				MtxPtr mtx = self->mMActor->getModel()->getAnmMtx(1);
				self->unk374.set(mtx[0][3], mtx[1][3], mtx[2][3]);

				// The first emitter is fixed at 0xdc; the second is picked
				// from unk380 as an if/else-if/else chain (the ROM emits
				// cmpwi/bne pairs, not a jump table, so this must not be a
				// switch).
				gpMarioParticleManager->emit(0xdc, &self->unk374, 0, nullptr);
				if (self->unk380 == 0xd8) {
					gpMarioParticleManager->emit(0xdd, &self->unk374, 0, nullptr);
				} else if (self->unk380 == 0xd9) {
					gpMarioParticleManager->emit(0xde, &self->unk374, 0, nullptr);
				} else {
					gpMarioParticleManager->emit(0xdf, &self->unk374, 0, nullptr);
				}

				self->forceAllItemKill();

				// The next six stores are fully unrolled: one f32 per
				// mStageSlotObjects[i], interleaved with one f32 per slot
				// drum angle on mSlot.  Only the loop below is rolled.
				// Actor type 0x4000019A identifies the stage roulettes.
				// Their rotation field belongs to TRoulette, not TMapObjBase.
				static_cast<TRoulette*>(self->mStageSlotObjects[0])->unk144 = 0.0f;
				self->mSlot->mRouletteRollSpeeds[0] = 0.0f;
				static_cast<TRoulette*>(self->mStageSlotObjects[1])->unk144 = 0.0f;
				self->mSlot->mRouletteRollSpeeds[1] = 0.0f;
				static_cast<TRoulette*>(self->mStageSlotObjects[2])->unk144 = 0.0f;
				self->mSlot->mRouletteRollSpeeds[2] = 0.0f;

				for (int i = 0; i < 3; ++i) {
					static_cast<TRoulette*>(self->mStageSlotObjects[i])
					    ->setRollSp(self->mSlot->mRouletteRollSpeeds[i]);
				}
			}

			// The ROM branches *over* the counter bump when unk388 has passed
			// 0xf0, so the shine path has to be the fall-through here.
			if (self->unk388 > 0xf0) {
				self->unk388 = 0;
				gpItemManager->makeShineAppearWithDemo(
				    "シャイン（ボス用）", "ボスシャインカメラ", self->mPosition.x,
				    self->mPosition.y, self->mPosition.z);
				// mLiveFlag is at +0xf0; 0x1 is LIVE_FLAG_DEAD, 0x8 is
				// LIVE_FLAG_UNK8 and the rlwinm clears
				// TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH (0x20000 on GMSP01).
				// This trio is a verbatim match for TNerveTelesaDie.
				self->onLiveFlag(LIVE_FLAG_DEAD);
				self->onLiveFlag(LIVE_FLAG_UNK8);
				self->offLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
				self->mHolder = nullptr;
				self->onHitFlag(HIT_FLAG_NO_COLLISION);
				self->mHitActors[0]->onHitFlag(HIT_FLAG_NO_COLLISION);
				self->mHitActors[1]->onHitFlag(HIT_FLAG_NO_COLLISION);
				self->stopAnmSound();
				spine->reset();
				return TRUE;
			}

			self->unk388++;
		}
	} else if (self->checkCurAnmEnd(0)) {
		if (self->getMActor()->checkCurBckFromIndex(5)) {
			self->getMActor()->setBrkFromIndex(2);
			self->setBckAnm(7);
		} else if (self->getMActor()->checkCurBckFromIndex(7)) {
			self->setBckAnm(6);
		} else {
			SMS_ResetDamageFogEffect(self->mMActor->getModel()->getModelData());
			if (self->getMActor()->checkCurBckFromIndex(6)) {
				self->offHitFlag(HIT_FLAG_NO_COLLISION);
				self->mHitActors[0]->offHitFlag(HIT_FLAG_NO_COLLISION);
				self->mHitActors[1]->offHitFlag(HIT_FLAG_NO_COLLISION);
				self->setBckAnm(15);
				self->getMActor()->setBtpFromIndex(2);
				spine->reset();
				spine->setNext(&TNerveBossTelesaPrepareSlot::theNerve());
				spine->pushAfterCurrent(&TNerveBossTelesaPrepareSlot::theNerve());
			} else {
				self->damageRecover();
			}
			return TRUE;
		}
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBossTelesaSpit, TLiveActor)
{
	TBossTelesa* self = (TBossTelesa*)spine->getBody();

	int time = spine->getTime();
	if (time == 0 || !self->getMActor()->checkCurBckFromIndex(14)) {
		self->setBckAnm(14);
	} else if (self->getMActor()->getFrameCtrl(0)->checkPass(40.0f)) {
		self->genAttacker();
	}
	return self->checkCurAnmEnd(0) ? TRUE : FALSE;
}

DEFINE_NERVE(TNerveBossTelesaHide, TLiveActor)
{
	TBossTelesa* self = (TBossTelesa*)spine->getBody();

	if (!self->getMActor()->checkCurBckFromIndex(4)) {
		self->setBckAnm(4);
		self->getMActor()->setBtpFromIndex(2);
	}
	if (self->checkCurAnmEnd(0)) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->mHitActors[0]->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->mHitActors[1]->onHitFlag(HIT_FLAG_NO_COLLISION);
		SMSRumbleMgr->start(20, 15, static_cast<f32*>(nullptr));
		self->rouletteStart();
		spine->pushAfterCurrent(&TNerveBossTelesaHideWait::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBossTelesaHideWait, TLiveActor)
{
	TBossTelesa* self = (TBossTelesa*)spine->getBody();

	int time = spine->getTime();
	if (time == 0) {
		self->onLiveFlag(LIVE_FLAG_HIDDEN);
		self->unk350 = false;
		// the boss fades out further the fewer hit points it has left
		u32 minAlpha = 0;
		u32 maxAlpha = 254;
		self->mTevColorB.a = MsClamp<u32>(
		    (u8)(TBossTelesa::mNormalAlpha
		         + (self->getMaxHitPoints() - self->mHitPoints) * 30),
		    minAlpha, maxAlpha);
		self->mSlot->mScaling.set(0.0f, 0.0f, 0.0f);
		self->getMActor()->setBrkFromIndex(2);
		s16 endFrame = self->getMActor()->getFrameCtrl(5)->getEnd();
		self->getMActor()->getFrameCtrl(5)->setFrame(endFrame);
	} else {
		JGeometry::TVec3<f32> diff = self->mPosition;
		diff.sub(*gpMarioPos);
		if (spine->getTime() > 400
		    && !((TBossTelesaKillSmallEnemy*)self->mHitActors[2])->mHit) {
			spine->pushAfterCurrent(&TNerveBossTelesaAppear::theNerve());
			self->offLiveFlag(LIVE_FLAG_HIDDEN);
			return TRUE;
		}
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBossTelesaAppear, TLiveActor)
{
	TBossTelesa* self = (TBossTelesa*)spine->getBody();

	int time = spine->getTime();
	if (time == 0 && !self->getMActor()->checkCurBckFromIndex(0)) {
		self->setBckAnm(0);
		if (!self->unk384) {
			self->unk384 = true;
			MSBgm::startBGM(MSD_BGM_MAP_SELECT);
		}
		self->mSlot->mScaling.set(1.0f, 1.0f, 1.0f);
		self->mSlot->randomReset();
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
		self->mHitActors[0]->offHitFlag(HIT_FLAG_NO_COLLISION);
		self->mHitActors[1]->offHitFlag(HIT_FLAG_NO_COLLISION);
	} else if (self->checkCurAnmEnd(0)
	           && !self->getMActor()->checkCurBckFromIndex(15)) {
		self->setBckAnm(15);
		self->getMActor()->setBtpFromIndex(2);
	}

	if (self->getMActor()->checkCurBckFromIndex(0)
	    && self->getMActor()->getFrameCtrl(0)->checkPass(40.0f)) {
		gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x22), 1.0f);
		if (gpMSound->gateCheck(0x292a))
			MSoundSESystem::MSoundSE::startSoundActor(0x292a, &self->mPosition, 0,
			                                          nullptr, 0, 4);
	}

	if (spine->getTime() > 800) {
		if (spine->getTime()
		        % (TBossTelesa::mTelesaGenerateInterval
		           + (self->getMaxHitPoints() - self->mHitPoints) * 100)
		    == 1)
			self->setBckAnm(14);
	}
	self->unk364 *= 0.9f;
	return FALSE;
}

DEFINE_NERVE(TNerveBossTelesaSlotStart, TLiveActor)
{
	TBossTelesa* self = (TBossTelesa*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(11);
		self->getMActor()->setBtpFromIndex(11);
	}

	// Frame 53 of the start animation is the trigger: spin the drum up,
	// clear the slot's enemies, and on the animation's end set the
	// "spit an item" flag before pushing the SpitSlotItem nerve.
	if (self->getMActor()->checkCurBckFromIndex(11)) {
		if (self->getMActor()->getFrameCtrl(0)->checkPass(53.0f)) {
			self->unk18C     = true;
			self->mSlot->moveStart();
			self->unk354->telesaForceKill();
		}
		if (self->checkCurAnmEnd(0)) {
			self->setBckAnm(15);
			self->getMActor()->setBtpFromIndex(2);
			self->mSlot->forceStopSlot(1);
			if (!self->mSlot->isRollDrum() && self->unk18C) {
				self->unk18C = false;
				spine->pushAfterCurrent(
				    &TNerveBossTelesaSpitSlotItem::theNerve());
				return TRUE;
			}
		}
	}

	// slow decay on the drum speed while the nerve runs
	self->unk364 *= 0.99f;
	return FALSE;
}

DEFINE_NERVE(TNerveBossTelesaSpitSlotItem, TLiveActor)
{
	TBossTelesa* self = (TBossTelesa*)spine->getBody();

	// Blend into the spit animation once the drum has dropped far enough
	// below its rest height.
	if (!self->getMActor()->checkCurBckFromIndex(14)
	    && self->unk364 < TBossTelesa::mBaseHoseiPosY - 300.0f) {
		self->setBckAnm(14);
	} else if (self->checkCurAnmEnd(0) && spine->getTime() > 600) {
		// The animation is done and we have waited long enough: reset the
		// spin counter and push the slot-prepare nerve.
		spine->pushAfterCurrent(&TNerveBossTelesaPrepareSlot::theNerve());
		self->unk368 = 0;

		// Drop the hit flag on every live item that is a slot prize
		// (actor type 0x2000 + 14), and on the ones that already stopped
		// (actor type 0x2000 + 2).
		for (int i = 0; i < self->mItemNum; i++) {
			TMapObjBase* item = self->mItems[i];
			if (item->mLiveFlag & 0x80000000)
				continue;
			if (item->getActorType() == 0x200E
			    || item->getActorType() == 0x2002)
				item->offHitFlag(HIT_FLAG_NO_COLLISION);
		}
		return TRUE;
	}

	// slow the drum again once the spit animation has had a moment
	if (spine->getTime() > 200)
		self->unk364 -= 2.0f;
	return FALSE;
}

DEFINE_NERVE(TNerveBossTelesaPrepareSlot, TLiveActor)
{
	TBossTelesa* self = (TBossTelesa*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(15);
		self->getMActor()->setBtpFromIndex(2);
	}

	// Tick the slot timer while the spin animation is running, and step the
	// roulette forward once the configured interval has elapsed.
	if (self->unk350 && self->checkCurAnmEnd(0)) {
		self->unk36C++;
		if (self->getMActor()->checkCurBckFromIndex(12)) {
			if (self->unk36C > self->mParams->mSLPrepareSlotTime.get())
				self->setBckAnm(13);
		} else {
			self->setBckAnm(15);
			self->getMActor()->setBtpFromIndex(2);
			self->unk36C = 0;
			self->unk350 = false;
		}
	}

	// Count how many of the live items have already been collected; when the
	// timer passes the last one, clear them all and cut to the hide nerve.
	int alive = 0;
	for (int i = 0; i < self->mItemNum; i++) {
		if (!(self->mItems[i]->mLiveFlag & 0x80000000))
			alive++;
	}
	if (alive && self->unk368 > self->unk374.z) {
		self->unk368 = 0;
		self->forceAllItemKill();
		if (self->unk350 && gpMSound->gateCheck(0x2968))
			MSoundSESystem::MSoundSE::startSoundActor(
			    0x2968, &self->mPosition, 0, nullptr, 0, 4);
		spine->pushAfterCurrent(&TNerveBossTelesaHide::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBossTelesaFreeze, TLiveActor)
{
	TBossTelesa* self = (TBossTelesa*)spine->getBody();

	if (self->getMActor()->checkCurBckFromIndex(16)) {
		if (self->checkCurAnmEnd(0)) {
			self->unk350 = false;
			u32 minAlpha = 0;
			u32 maxAlpha = 254;
			self->mTevColorB.a = MsClamp<u32>(
			    (u8)(TBossTelesa::mNormalAlpha
			         + (self->getMaxHitPoints() - self->mHitPoints) * 30),
			    minAlpha, maxAlpha);
			return TRUE;
		}
	} else {
		self->setBckAnm(16);
		if (gpMSound->gateCheck(0x28e7))
			MSoundSESystem::MSoundSE::startSoundActor(0x28e7, &self->mPosition, 0,
			                                          nullptr, 0, 4);
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBossTelesaFallDemo, TLiveActor)
{
	TBossTelesa* self = (TBossTelesa*)spine->getBody();

	int time = spine->getTime();
	if (time == 0) {
		self->onLiveFlag(LIVE_FLAG_HIDDEN);
		if (SMS_SendMessageToMario(self, 4))
			self->mHeldObject = (TTakeActor*)SMS_GetMarioHitActor();
		self->getMActor()->setFrameRate(0.0f, 0);
		self->mSlot->mScaling.set(0.0f, 0.0f, 0.0f);
	}
	if (self->rouletteFall()) {
		JGeometry::TVec3<f32> diff = self->mPosition;
		diff -= *gpMarioPos;
		if (self->slotFall()) {
			self->offHitFlag(HIT_FLAG_NO_COLLISION);
			self->mHitActors[0]->offHitFlag(HIT_FLAG_NO_COLLISION);
			self->mHitActors[1]->offHitFlag(HIT_FLAG_NO_COLLISION);
			spine->reset();
			spine->setNext(&TNerveBossTelesaHideWait::theNerve());
			spine->pushAfterCurrent(&TNerveBossTelesaHideWait::theNerve());
			return TRUE;
		}
	}
	return FALSE;
}
