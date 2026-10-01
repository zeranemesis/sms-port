#include <Enemy/bombhei.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/EffectObj.hpp>
#include <Enemy/Graph.hpp>
#include <Player/MarioAccess.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <Map/MapData.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/ObjModel.hpp>
#include <System/Application.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <Camera/CameraShake.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjBlock.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <stdlib.h>

// rogue include: mtx calc type names; it drags in System/DummyStrings.hpp,
// which opens this object's .rodata with the dummy string pair and the four
// names, exactly like the original TU.
#include <M3DUtil/InfectiousStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template statics,
// which is what marioEU.dol registers from __sinit_<TU>_cpp.
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// Everything below is a transcription of build/GMSP01/asm/Enemy/bombhei.s.
// Functions whose bodies could not be recovered carry a TODO.

// NOTE: the binary has this in .sdata (i.e. statically initialised to 1),
// not .sbss -- the object file stores the initial value, no code is emitted.
bool TBombHei::mSerialBomb = true;

// The binary materialises the instance's name with a single `li r4, ...`, so
// the string lives in .sdata2 -- which only happens for a literal of at most
// 8 bytes. The literal really is three characters wide (83 7B 83 80 95 BA 00
// in the original object), *not* the four-character "ボムヘイ" the manager
// uses, so it is spelled out byte for byte to keep the section placement.
static const char bombhei_name[]
    = "\x83\x7b\x83\x80\x95\xba";

static const char* bombhei_bastable[] = {
	"/scene/bombhei/bas/downnejibomb_down1.bas",
	nullptr,
	nullptr,
	"/scene/bombhei/bas/nejibomb_land1.bas",
	nullptr,
	nullptr,
	"/scene/bombhei/bas/nejibomb_stop_down1.bas",
};

// ---------------------------------------------------------------------------
// Reverse of the order the functions appear in mario.MAP (this TU is
// -inline deferred, so the definition order in the file is the reverse of the
// emission order in the object).
// ---------------------------------------------------------------------------

void TBombHeiManager::clipEnemies(JDrama::TGraphics* graphics) { }

TBombHeiManager::~TBombHeiManager() { }

TBombHei::~TBombHei() { }

bool TBombHei::doKeepDistance() { return mIsBomb; }

void TBombHei::setAfterDeadEffect() { }

TBombHeiSaveLoadParams::TBombHeiSaveLoadParams(const char* path)
    : TWalkerEnemyParams(path)
    , PARAM_INIT(mSLBombTime, 1000)
    , PARAM_INIT(mSLBombRange, 300.0f)
    , PARAM_INIT(mSLThrownVY, 50.0f)
    , PARAM_INIT(mSLThrownRateXZ, 0.5f)
    , PARAM_INIT(mSLThrownGravityY, 1.5f)
    , PARAM_INIT(mSLShootVelocity, 12.0f)
{
	TParams::load(mPrmPath);
}

TBombHeiManager::TBombHeiManager(const char* name)
    : TSmallEnemyManager(name)
    , mDeadCoinCount(0)
{
}

void TBombHeiManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TBombHeiSaveLoadParams("/enemy/bombhei.prm");
}

void TBombHeiManager::createModelData()
{
	static TModelDataLoadEntry entries[] = {
		{ "nejibomb_model1.bmd", 0x10230000, 0 },
		{ "downnejibomb_model1.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entries);
}

TSpineEnemy* TBombHeiManager::createEnemyInstance()
{
	return new TBombHei(bombhei_name);
}

// UNUSED in the map (0x24 bytes); the compiler folds it into genEventCoin().
bool TBombHeiManager::canMakeDeadCoin()
{
	if (mDeadCoinCount < 20) {
		mDeadCoinCount++;
		return true;
	}

	return false;
}

TBombHei::TBombHei(const char* name)
    : TWalkerEnemy(name)
    , mBombParam(nullptr)
    , mBombTimer(0)
    , mIsBomb(true)
    , mMadeDeadCoin(false)
{
}

void TBombHei::init(TLiveManager* liveManager)
{
	TWalkerEnemy::init(liveManager);
	mActorType = 0x1000001E;
	unk150 = 0x11;
	mBombParam = getBombParam();
	mSpine->initWith(&TNerveBombHeiGenerate::theNerve());

	// TODO: the loop body is empty here too -- presumably a leftover debug
	// spin over the model's joint count for the first instance.
	if (mInstanceIndex == 0) {
		for (u8 i = 0; i < getModel()->getModelData()->mJointNum; ++i) {
		}
	}
}

void TBombHei::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 2);
	mMActor = mMActorKeeper->createMActor("nejibomb_model1.bmd", 0);
	mMActorKeeper->createMActor("downnejibomb_model1.bmd", 3);
}

void TBombHei::behaveToWater(THitActor* hitActor)
{
	// The `? true : false` wrappers stop MWCC folding the two consecutive
	// BCK indices into a range check.
	int anm = mCurrentBckAnm;
	if ((anm == 4 ? true : false) || (anm == 3 ? true : false)) {
		if (mHitPoints == 0)
			mSpine->pushNerve(&TNerveBombHeiWaitExplosion::theNerve());

		// NOTE: the binary only reaches the cooldown store from inside the
		// branch -- the false arm jumps straight past it to the epilogue.
		mSprayedByWaterCooldown = 20;
	}
}

void TBombHei::changeOut()
{
	// The binary loads gpMSound into a scratch register and then copies it
	// into the argument register (lwz r0 / mr r3, r0), which is what a named
	// local produces here.
	MSound* msound = gpMSound;
	if (msound->gateCheck(MSD_SE_EN_TELSA_RECOVER))
		MSoundSESystem::MSoundSE::startSoundActor(
		    MSD_SE_EN_TELSA_RECOVER, &mPosition, 0, nullptr, 0, 4);
	onLiveFlag(LIVE_FLAG_DEAD);
	genEventCoin();
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mPosition = mJuiceBlock->getPosition();
	gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_TLS_CHANGE,
	                                            &mPosition, 0, nullptr);
	getMActor()->setFrameRate(SMSGetAnmFrameRate(), ANM_TYPE_BCK);
	mJuiceBlock->kill();
	mJuiceBlock = nullptr;
}

bool TBombHei::isHitValid(u32 message)
{
	if (message == HIT_MESSAGE_UNKB) {
		onLiveFlag(LIVE_FLAG_DEAD);
		onHitFlag(HIT_FLAG_NO_COLLISION);
		genEventCoin();
		return false;
	}

	return false;
}

void TBombHei::kill()
{
	if (!checkLiveFlag(LIVE_FLAG_DEAD)) {
		mHitPoints = 1;
		if (mSpine->getCurrentNerve() != &TNerveBombHeiExplosion::theNerve()) {
			mSpine->reset();
			mSpine->setNext(&TNerveBombHeiExplosion::theNerve());
			mSpine->pushAfterCurrent(&TNerveBombHeiExplosion::theNerve());
		}
		onLiveFlag(LIVE_FLAG_UNK40);
	}
}

void TBombHei::genEventCoin()
{
	// NOTE: the binary reads mManager before testing mMadeDeadCoin, and
	// materialises the appearance flags as (0x2000 << 16) | 0x0E.
	TBombHeiManager* manager = (TBombHeiManager*)mManager;
	if (!mMadeDeadCoin)
		return;

	if (!manager->canMakeDeadCoin())
		return;

	TMapObjBase* coin = gpItemManager->makeObjAppear(
	    mPosition.x, mPosition.y, mPosition.z, 0x2000000E, true);
	if (coin) {
		coin->mPosition.y = mPosition.y;

		JGeometry::TVec3<f32> dir(*gpMarioPos);
		// NOTE: TVec3<f32>::sub() is out of line in the original object
		// (an external `sub` call); the shared JGVec3.hpp inlines it, so
		// this call site can never match.
		dir.sub(mPosition);

		JGeometry::TVec3<f32> norm(dir);
		MsVECNormalize(&norm, &norm);
		coin->mVelocity.set(20.0f * norm.x, 20.0f, 20.0f * norm.z);
		coin->offLiveFlag(LIVE_FLAG_UNK10);
	}
}

void TBombHei::setWalkAnm() { setBckAnm(4); }

void TBombHei::setFreezeAnm() { setBckAnm(1); }

void TBombHei::setDeadAnm()
{
	mMActor = getActorKeeper()->getMActor("downnejibomb_model1.bmd");
	// TMsRange is what produces the binary's shape: the ctor stores both
	// bounds into the (non-SRA'd, it has a user dtor) stack object, rand() is
	// called with the range live in the callee-saved f31, and the result is
	// `mMin + range * MsRandF()` evaluated left to right. The random value
	// lands in TActor::mRotation.y (offset 0x34).
	TMsRange<f32> range(0.0f, 360.0f);
	mRotation.y = range.rand();
	mIsBomb = false;
	setBckAnm(0);
	gpCameraShake->startShake(CAM_SHAKE_MODE_UNK6, 1.0f);
	SMSRumbleMgr->start(0x15, 5, static_cast<f32*>(nullptr));
}

void TBombHei::calcRootMatrix()
{
	TSpineEnemy::calcRootMatrix();
	if (gpMarDirector->mFlags & 0xF) {
		onLiveFlag(LIVE_FLAG_DEAD);
		onHitFlag(HIT_FLAG_NO_COLLISION);
	}

	// The `? true : false` is what produces the binary's `li 1 / b / li 0 /
	// clrlwi.` materialisation of the condition.
	if (mCurrentBckAnm == 0 ? true : false) {
		if (getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(2.0f)) {
			TSpineEnemy* effectBase = gpConductor->makeOneEnemyAppear(
			    mPosition, "エフェクト爆発マネージャー", 1);
			if (effectBase != nullptr) {
				TEffectExplosion* effect = (TEffectExplosion*)effectBase;
				effect->generate(mPosition, mScaling);
			}
		}
	}
}

void TBombHei::attackToMario()
{
	if (mSpine->getCurrentNerve() == &TNerveBombHeiExplosion::theNerve())
		SMS_SendMessageToMario(this, HIT_MESSAGE_UNKA);
}

// ---------------------------------------------------------------------------
// UNUSED in the map (0xC4 bytes). The body is the 48-instruction block shared
// by forceKill()/isDamageToCannon()/kill() below, so it is recovered exactly.
// ---------------------------------------------------------------------------
void TBombHei::bombIn()
{
	if (mSpine->getCurrentNerve() != &TNerveBombHeiExplosion::theNerve()) {
		mSpine->reset();
		mSpine->setNext(&TNerveBombHeiExplosion::theNerve());
		onLiveFlag(LIVE_FLAG_CALC_INT_FRAME);
		mHitPoints = 1;
	}
}

void TBombHei::behaveToTaken(THitActor* hitActor)
{
	if (mSpine->getCurrentNerve() != &TNerveBombHeiPickUp::theNerve()) {
		// ACTOR_TYPE_PLAYER | 1, with the `? true : false` wrapper the
		// binary uses (note the `addis`/low-half compare it produces).
		if (hitActor->mActorType == (ACTOR_TYPE_PLAYER | 1) ? true : false)
			mMadeDeadCoin = true;
		mSpine->pushNerve(&TNerveBombHeiPickUp::theNerve());
	}
}

void TBombHei::behaveToRelease()
{
	if (unk164 || mSpine->getCurrentNerve() == &TNerveBombHeiWalkExplosion::theNerve()) {
		if (mSpine->getCurrentNerve() != &TNerveBombHeiThrown::theNerve())
			mSpine->pushNerve(&TNerveBombHeiThrown::theNerve());
	}
}

void TBombHei::reset()
{
	TWalkerEnemy::reset();
	mBombTimer = 0;
	mMadeDeadCoin = false;
	unk164 = false;
	mIsBomb = true;
	mMActor = getActorKeeper()->getMActor("nejibomb_model1.bmd");
}

f32 TBombHei::getGravityY() const
{
	if (mSpine->getCurrentNerve() == &TNerveBombHeiThrown::theNerve())
		return mBombParam->mSLThrownGravityY.get();

	return mGravity;
}

void TBombHei::walkBehavior(int param_1, float param_2)
{
	// The `self` local keeps `this` in r30 across the gateCheck() call, and
	// the named `msound` local is what makes the binary load gpMSound into a
	// scratch register and copy it into the argument register
	// (lwz r0 / mr r3, r0). Only the stack frame still differs (see below).
	TBombHei* self = this;
	MSound* msound = gpMSound;
	if (msound->gateCheck(MSD_SE_EN_BOMBHEI_ZENMAI))
		MSoundSESystem::MSoundSE::startSoundActor(
		    MSD_SE_EN_BOMBHEI_ZENMAI, &self->mPosition, 0, nullptr, 0, 4);
	TWalkerEnemy::walkBehavior(param_1, param_2);
}

void TBombHei::moveObject()
{
	TWalkerEnemy::moveObject();

	if (mSpine->getCurrentNerve() != &TNerveBombHeiThrown::theNerve()
	    && mSpine->getCurrentNerve() != &TNerveBombHeiExplosion::theNerve()
	    && mSpine->getCurrentNerve() != &TNerveBombHeiGenerate::theNerve()
	    && mSpine->getCurrentNerve() != &TNerveSmallEnemyChange::theNerve()) {
		mBombTimer++;
		if (mBombTimer > mBombParam->mSLBombTime.get()) {
			unk164 = false;
			mBombTimer = 0;
			if (mSpine->getCurrentNerve() == &TNerveBombHeiWaitExplosion::theNerve())
				return;
			mSpine->pushNerve(&TNerveBombHeiWalkExplosion::theNerve());
		}
	}
}

bool TBombHei::isCollidMove(THitActor* hitActor)
{
	// The two actor-type tests are equality tests in the binary (the
	// `cmplwi`/`bne` pair), not the range checks they look like, and the
	// `? true : false` wrappers reproduce the `li 1 / b / li 0 / clrlwi.`
	// shape. The call at the end is TSmallEnemy::receiveMessage(this, 0)
	// dispatched on hitActor (vtable slot 0xA0).
	if (mSerialBomb) {
		TSmallEnemy* other = (TSmallEnemy*)hitActor;
		if ((u32)(hitActor->mActorType - 0x1000) == 0x1E ? true : false) {
			// the binary tests the *other* bomb's nerve as a materialised
			// bool before looking at ours, so chain the two explicitly.
			if (other->mSpine->getCurrentNerve()
			    == &TNerveBombHeiExplosion::theNerve() ? true : false) {
				if (mSpine->getCurrentNerve()
				    != &TNerveBombHeiExplosion::theNerve())
					mSpine->pushNerve(&TNerveBombHeiExplosion::theNerve());
			}
		}
	}

	if ((u32)(hitActor->mActorType - 0x80000000) == 0x13 ? true : false) {
		if (mSpine->getCurrentNerve() == &TNerveBombHeiExplosion::theNerve())
			((TSmallEnemy*)hitActor)->receiveMessage(this, 0);

		if (mSpine->getCurrentNerve() == &TNerveBombHeiThrown::theNerve())
			mSpine->pushNerve(&TNerveBombHeiExplosion::theNerve());
	}

	return true;
}

void TBombHei::forceKill()
{
	if (mGroundPlane->isIllegalData())
		return;

	// The pool set is a subset of isWaterSurface()'s set, which looks like
	// leftover redundancy in the original. The `? true : false` wrapper is
	// what forces the binary's `li 1 / b / li 0 / clrlwi.` shape.
	if ((mGroundPlane->mBGType == BG_TYPE_DEATH_PLANE ? true : false)
	    || mGroundPlane->isPool() || mGroundPlane->isWaterSurface()) {
		if (isAirborne())
			return;

		if (checkLiveFlag(LIVE_FLAG_UNK10))
			return;

		if (mSpine->getCurrentNerve() == &TNerveBombHeiExplosion::theNerve())
			return;

		mSpine->reset();
		mSpine->setNext(&TNerveBombHeiExplosion::theNerve());
		mSpine->pushAfterCurrent(mSpine->getDefault());
		onLiveFlag(LIVE_FLAG_CALC_INT_FRAME);
		mHitPoints = 1;
	}
}

// UNUSED in the map (0x8C bytes) and never emitted, so the body below is a
// guess -- unlike bombIn()/canMakeDeadCoin() there is nothing in this object
// to recover it from.
bool TBombHei::isExplosion()
{
	return mSpine->getCurrentNerve() == &TNerveBombHeiExplosion::theNerve();
}

bool TBombHei::isDamageToCannon()
{
	if (mSpine->getCurrentNerve() == &TNerveBombHeiThrown::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveBombHeiExplosion::theNerve()) {
		onHitFlag(HIT_FLAG_NO_COLLISION);
		return true;
	}

	return false;
}

const char** TBombHei::getBasNameTable() const { return bombhei_bastable; }

DEFINE_NERVE(TNerveBombHeiGenerate, TLiveActor)
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor = self->getActorKeeper()->getMActor("nejibomb_model1.bmd");
		self->setBckAnm(2);
		self->mMActor->setBtpFromIndex(1);
		self->mMActor->setFrameRate(0.0f, ANM_TYPE_BTP);
	}

	if (self->getHolder())
		self->mMActor->setFrameRate(0.0f, ANM_TYPE_BCK);

	if (!(self->checkLiveFlag(LIVE_FLAG_AIRBORNE) ? true : false)
	    && !self->getHolder()) {
		if (self->isBckAnm(2)) {
			self->setBckAnm(3);
		} else if (self->checkCurAnmEnd(0)) {
			spine->pushAfterCurrent(&TNerveBombHeiAttack::theNerve());
			return TRUE;
		}
	} else {
		JGeometry::TVec3<f32> velocity(self->mVelocity);
		if (velocity.y > 0.0f && !self->isBckAnm(2)) {
			self->mMActor
			    = self->getActorKeeper()->getMActor("nejibomb_model1.bmd");
			self->setBckAnm(2);
			self->mMActor->setBtpFromIndex(1);
			self->mMActor->setFrameRate(0.0f, ANM_TYPE_BTP);
		}
	}

	return FALSE;
}

DEFINE_NERVE(TNerveBombHeiAttack, TLiveActor)
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setWalkAnm();
		self->unk164 = false;
		self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));
	}

	self->walkBehavior(2, 1.0f);
	return FALSE;
}

DEFINE_NERVE(TNerveBombHeiWalkExplosion, TLiveActor)
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(5);
		self->mMActor->setBtpFromIndex(0);
	} else if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveBombHeiExplosion::theNerve());
		return TRUE;
	} else {
		s32 frame = (s32)self->mMActor->getFrameCtrl(ANM_TYPE_BTP)->getFrame();
		if (frame % 40 == 0) {
			if (gpMSound->gateCheck(MSD_SE_EN_BOMBHEI_COUNT))
				MSoundSESystem::MSoundSE::startSoundActor(
				    MSD_SE_EN_BOMBHEI_COUNT, &self->mPosition, 0, nullptr,
				    0, 4);
		}

		self->walkBehavior(2, 0.6f);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x17F, self->mMActor->getModel()->getAnmMtx(3), 1, self);
	}

	return FALSE;
}

DEFINE_NERVE(TNerveBombHeiWaitExplosion, TLiveActor)
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(6);
		self->unk164 = true;
	}

	if (self->unk164) {
		if (self->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(60.0f))
			self->getMActor()->setFrameRate(0.0f, ANM_TYPE_BCK);

		if (self->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(10.0f)) {
			self->getMActor()->setFrameRate(SMSGetAnmFrameRate(),
			                                ANM_TYPE_BTP);
			// NOTE: the binary's `cror eq, gt, eq` shows this is a >= test.
			if (self->getCurAnmFrameNo(ANM_TYPE_BTP) >= 1.0f)
				self->getMActor()->setFrameRate(0.0f, ANM_TYPE_BTP);
		}
	} else {
		self->getMActor()->setFrameRate(SMSGetAnmFrameRate(), ANM_TYPE_BCK);
		if (!self->getMActor()->checkCurAnmFromIndex(0, ANM_TYPE_BTP))
			self->getMActor()->setBtpFromIndex(0);

		if (self->checkCurAnmEnd(0)) {
			if (spine->getTime() > 150) {
				spine->pushAfterCurrent(&TNerveBombHeiExplosion::theNerve());
				return TRUE;
			}
		} else {
			s32 frame
			    = (s32)self->getMActor()->getFrameCtrl(ANM_TYPE_BTP)
			          ->getFrame();
			if (frame % 40 == 0) {
				if (gpMSound->gateCheck(MSD_SE_EN_BOMBHEI_COUNT))
					MSoundSESystem::MSoundSE::startSoundActor(
					    MSD_SE_EN_BOMBHEI_COUNT, &self->mPosition, 0,
					    nullptr, 0, 4);
			}

			gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x17F, self->getMActor()->getModel()->getAnmMtx(3), 1,
			    self);
		}
	}

	return FALSE;
}

DEFINE_NERVE(TNerveBombHeiPickUp, TLiveActor)
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0 && self->unk164 == 0)
		return TRUE;

	return FALSE;
}

DEFINE_NERVE(TNerveBombHeiThrown, TLiveActor)
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0) {
		TBombHeiSaveLoadParams* param = self->getBombParam();
		s16 angle = *gpMarioAngleY;
		f32 power = *gpMarioThrowPower;

		// The binary evaluates both trig table lookups up front (sharing the
		// shifted index), then multiplies power into each, then the rate, and
		// only afterwards builds the stack temporary that is word-copied
		// into mVelocity. sin feeds x, cos feeds z.
		f32 sinA = JMASSin(angle);
		f32 cosA = JMASCos(angle);
		f32 vx = power * sinA;
		f32 vz = power * cosA;
		vx *= param->mSLThrownRateXZ.get();
		vz *= param->mSLThrownRateXZ.get();

		JGeometry::TVec3<f32> velocity;
		velocity.x = vx;
		velocity.z = vz;
		velocity.y = param->mSLThrownVY.get();
		self->mVelocity = velocity;
		self->mPosition.y += 2.0f;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
	}

	if (spine->getTime() == 120)
		self->offHitFlag(HIT_FLAG_NO_COLLISION);

	if (self->checkLiveFlag(LIVE_FLAG_AIRBORNE) ? true : false) {
		self->genEventCoin();
		spine->pushAfterCurrent(&TNerveBombHeiExplosion::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveBombHeiExplosion, TLiveActor)
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0) {
		TBombHeiSaveLoadParams* param = self->getBombParam();
		self->mBombRadius
		    = (param->mSLBombRange.get() * self->mBodyScale)
		    / self->mAttackRadius;
		self->setDeadAnm();
		self->onLiveFlag(LIVE_FLAG_UNK8);
		if (self->getHolder() == gpMarioAddress)
			self->sendAttackMsgToMario();
	}

	self->unk164 = false;
	if (self->mGroundPlane->isWaterSurface()) {
		TSpineEnemy* effectBase = gpConductor->makeOneEnemyAppear(
		    self->mPosition, "エフェクト爆発水柱マネージャー", 1);
		if (effectBase != nullptr) {
			JGeometry::TVec3<f32> scale(2.0f);
			((TEffectBombColumWater*)effectBase)
			    ->generate(self->mPosition, scale);
		}
	}

	if (self->mGroundPlane->isSand()) {
		TSpineEnemy* effectBase = gpConductor->makeOneEnemyAppear(
		    self->mPosition, "エフェクト砂柱マネージャー", 1);
		if (effectBase != nullptr) {
			JGeometry::TVec3<f32> scale(0.7f);
			((TEffectColumSand*)effectBase)->generate(self->mPosition, scale);
		}
	}

	if (self->unk190 < self->mBombRadius) {
		self->unk190 *= 1.2f;
	} else if (self->checkCurAnmEnd(0)) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->onLiveFlag(LIVE_FLAG_DEAD);
		self->onLiveFlag(LIVE_FLAG_UNK8);
		self->offLiveFlag(LIVE_FLAG_UNK10000);
		self->mHolder = nullptr;
		self->stopAnmSound();
		spine->reset();
		spine->setDefaultNext();
		spine->pushAfterCurrent(spine->getDefault());
		return TRUE;
	}

	self->expandCollision();
	return FALSE;
}
