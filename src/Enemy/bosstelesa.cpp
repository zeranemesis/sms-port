
#include <Enemy/bosstelesa.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
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
	if (gpMapObjManager->getObjNumWithActorType(slotActorType) != 0) {
		int foundIndex = 0;
		for (int i = 0; i < gpMapObjManager->getObjNum(); i++) {
			TMapObjBase* obj = gpMapObjManager->getObj(i);
			if (obj->getActorType() == slotActorType)
				mStageSlotObjects[foundIndex++] = obj;
		}
	}

	const u32 ownerActorType = 0x400001A6;
	if (gpMapObjManager->getObjNumWithActorType(ownerActorType) != 0) {
		for (int i = 0; i < gpMapObjManager->getObjNum(); i++) {
			TMapObjBase* obj = gpMapObjManager->getObj(i);
			if (obj->getActorType() == ownerActorType) {
				mSlot         = (TTelesaSlot*)obj;
				mSlot->mOwner = this;
			}
		}
	}

	// TODO: PAL continues by creating/registering the roulette props and
	// initializing their per-object position/rotation/scale data. That part
	// is intentionally left untouched until its fields and resources are
	// identified from the complete method.
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

const char** TBossTelesa::getBasNameTable() const { return btelesa_bastable; }

DEFINE_NERVE(TNerveBossTelesaDie, TLiveActor)
{
	// TODO: not yet decompiled
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
		// alpha fades out as the boss loses hit points
		s32 alpha = TBossTelesa::mNormalAlpha + (self->unk13C - 0) * 30;
		if (alpha > 0xFE)
			alpha = 0xFE;
		else if (alpha < 0)
			alpha = 0;
		self->mTevColorB.a = (u8)alpha;
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
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBossTelesaSpitSlotItem, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBossTelesaPrepareSlot, TLiveActor)
{
	// TODO: not yet decompiled
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
