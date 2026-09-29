#include <dolphin/mtx.h>
#include <Enemy/BossWanwan.hpp>
#include <Enemy/Graph.hpp>
#include <System/Particles.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <System/Application.hpp>
#include <System/EmitterViewObj.hpp>
#include <math.h>
#include <MSound/MSound.hpp>
#include <Camera/CameraShake.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <Player/MarioAccess.hpp>
#include <System/MarDirector.hpp>
#include <GC2D/GCConsole2.hpp>
#include <MSound/MSoundSE.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>

// rogue include: dummy string pair, needed to match the .rodata prologue
#include <System/DummyStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

static JGeometry::TVec3<f32> BW_BATH_POS(-1000.0f, 4.5f, -6217.2f);
static JGeometry::TVec3<f32> BW_PICKET_START(6012.84f, 0.0f, 7323.15f);
static JGeometry::TVec3<f32> BW_HEAD_START(5741.72f, -100.0f, 6311.62f);

static const char* bwanwan_bastable[7] = {
	"/scene/bwanwan/bas/bwanwan_bark.bas",
	nullptr,
	"/scene/bwanwan/bas/bwanwan_shake.bas",
	nullptr,
	"/scene/bwanwan/bas/bwanwan_wait.bas",
	"/scene/bwanwan/bas/bwanwan_wait2.bas",
	nullptr,
};

TBWParams::TBWParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLMarchSpeed, 6.0f)
    , PARAM_INIT(mSLTurnSpeed, 1.0f)
    , PARAM_INIT(mSLLeashNodeLen, 120.0f)
    , PARAM_INIT(mSLPicketHeight, 100.0f)
    , PARAM_INIT(mSLPicketRadius, 100.0f)
    , PARAM_INIT(mSLChainHitHeight, 100.0f)
    , PARAM_INIT(mSLChainHitRadius, 100.0f)
    , PARAM_INIT(mSLChainGroundRadius, 60.0f)
    , PARAM_INIT(mSLPullLimit, 1.0f)
    , PARAM_INIT(mSLAttackSpeed, 10.0f)
    , PARAM_INIT(mSLStunTimer, 4000)
    , PARAM_INIT(mSLSearchLength, 10000.0f)
    , PARAM_INIT(mSLSearchAngle, 60.0f)
    , PARAM_INIT(mSLBWHitPointMax, 255)
    , PARAM_INIT(mSLHeadGap, 150.0f)
    , PARAM_INIT(mSLShakeLengthMax, 3000.0f)
    , PARAM_INIT(mSLShakeLengthMaxHP0, 2000.0f)
{
	TParams::load(mPrmPath);
}

void TBWLeashNode::calcTemperature()
{
	if (mIndex == 0)
		return;

	int prev = mIndex - 1;
	f32 diff = mLeash->mNodes[prev]->mTemperature - mTemperature;

	f32 delta;
	if (diff < 0.0f) {
		if (diff < -0.1f)
			delta = -0.02f;
		else
			delta = -0.005f;
	} else {
		if (diff > 0.1f)
			delta = 0.02f;
		else
			delta = 0.005f;
	}

	mTemperature += delta;
	if (mTemperature < 0.0f)
		mTemperature = 0.0f;
	if (mTemperature > 1.0f)
		mTemperature = 1.0f;
}

void TBWLeashNode::calcMatrix()
{
	// TODO: not decompiled
}

void TBWLeashNode::perform(u32, JDrama::TGraphics*)
{
	// TODO: not decompiled
}

TBWLeash::TBWLeash(TBossWanwan*, int, const char*)
    : TViewObj("")
{
	// TODO: not decompiled
}

void TBWLeash::perform(u32, JDrama::TGraphics*)
{
	// TODO: not decompiled
}

BOOL TBWPicket::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->mActorType == 0x80000001) {
		if (message == 1) {
			TBossWanwan* owner = mOwner;
			owner->unk17C      = 1;
			owner->unk184      = 0;
			if (gpMSound->gateCheck(0x28c0))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x28c0, &mPosition, 0, nullptr, 0, 4);
			return TRUE;
		}
		if (message == 4) {
			TBossWanwan* owner = mOwner;
			if (owner->unk17C) {
				JPABaseEmitter* emitter = gpMarioParticleManager->emit(
				    0xae, &owner->unk158->mPosition, 0, nullptr);
				if (emitter)
					emitter->setGlobalScale(
					    JGeometry::TVec3<f32>(0.3f, 0.5f, 0.3f));
			}
			owner->unk194 = 0;
			owner->unk17C = 0;
			mHolder       = static_cast<TTakeActor*>(sender);
			return TRUE;
		}
		if (message == 7 || message == 8) {
			mHolder = nullptr;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL TBWPicket::moveRequest(const JGeometry::TVec3<f32>&)
{
	// TODO: not decompiled
	return FALSE;
}

MtxPtr TBWPicket::getTakingMtx()
{
	return reinterpret_cast<MtxPtr>(reinterpret_cast<char*>(this) + 0x74);
}

void TBWPicket::perform(u32, JDrama::TGraphics*)
{
	// TODO: not decompiled
}

BOOL TBWHit::receiveMessage(THitActor* sender, u32 message)
{
	return mOwner->receiveMessage(sender, message);
}

void TBWHit::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_MOVE) {
		if (mJointIndex >= 0)
			mOwner->getJointTransByIndex(mJointIndex, &mPosition);

		for (int i = 0; i < mColCount; ++i) {
			THitActor* other = mCollisions[i];
			if (mOwner->mHitPoints != 0 && other->mActorType == 0x80000001)
				other->receiveMessage(mOwner, 10);
		}
	}
	THitActor::perform(cue, graphics);
}

void TBWBinder::bind(TLiveActor*)
{
	// TODO: not decompiled
}

void TBossWanwanMtxCalc::joinAnm(int index)
{
	M3UMtxCalcSIAnmBlendQuat::joinAnm(
	    mOwner->getActorKeeper()->getMActorAnmData()->getUnk2C()->getAnmPtr(
	        index));
}

void TBossWanwanMtxCalc::calc(u16)
{
	// TODO: not decompiled
}

TBossWanwan::TBossWanwan(const char* name)
    : TSpineEnemy(name)
    , mMtxCalc(nullptr)
    , mLeash(nullptr)
    , unk158(nullptr)
    , unk168(0.0f)
    , unk16C(0)
    , unk17C(0)
    , unk180(0)
    , unk184(0)
    , unk188(0)
    , unk18C(false)
    , unk18D(false)
    , unk190(0)
    , unk194(1)
    , unk195(false)
    , unk198(0)
    , unk19C(0)
    , unk1A0(false)
    , unk1A4(0.0f)
    , unk1A8(0.0f)
    , unk1AC(0.0f)
    , unk1B0(0)
    , unk1B4(0)
{
	mBinder = new TBWBinder;
}

void TBossWanwan::init(TLiveManager*)
{
	// TODO: not decompiled
}

void TBossWanwan::shakeCamera(int mode)
{
	if (!SMS_IsMarioTouchGround4cm())
		return;

	f32 dist = MsSqrtf(mDistToMarioSquared);

	f32 lengthMax    = getBWParams()->mSLShakeLengthMax.get();
	f32 lengthMaxHP0 = getBWParams()->mSLShakeLengthMaxHP0.get();

	f32 ratio;
	if (getMActor()->checkCurBckFromIndex(0))
		ratio = 1.0f;
	else
		ratio = (f32)mHitPoints / (f32)getBWParams()->mSLBWHitPointMax.get();

	f32 length = lengthMax * ratio + lengthMaxHP0 * (1.0f - ratio);
	f32 rest   = length - dist;
	if (rest < 0.0f)
		return;

	f32 power = rest / length;
	if (power > 1.0f)
		power = 1.0f;

	gpCameraShake->startShake((EnumCamShakeMode)mode, power * ratio);
	SMSRumbleMgr->start(8, &mPosition);
}

BOOL TBossWanwan::receiveMessage(THitActor*, u32)
{
	// TODO: not decompiled
	return FALSE;
}

void TBossWanwan::changeBck(int index)
{
	mMtxCalc->joinAnm(index);
	getMActor()->setFrameCtrlForBck(index);
	unk178 = 10.0f / getMActor()->getFrameCtrl(0)->getEnd();
	setAnmSound(bwanwan_bastable[index]);
}

void TBossWanwan::calcRootMatrix()
{
	getModel()->setBaseScale(mScaling);
	MsMtxSetXYZRPH(getModel()->getBaseTRMtx(), mPosition.x,
	               500.0f + mPosition.y, mPosition.z, mRotation.x, mRotation.y,
	               mRotation.z);
}

void TBossWanwan::slideToCurPathNode(f32, f32)
{
	// TODO: not decompiled
}

void TBossWanwan::control()
{
	// TODO: not decompiled
}

void TBossWanwan::emitEffects()
{
	// TODO: not decompiled
}

void TBossWanwan::perform(u32, JDrama::TGraphics*)
{
	// TODO: not decompiled
}

TBossWanwanManager::TBossWanwanManager(const char* name)
    : TEnemyManager(name)
{
}

void TBossWanwanManager::initJParticle()
{
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_jump_rock.jpa", 0xad);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_jump_smoke.jpa", 0xae);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_downyuge.jpa", 0xb0);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_hibana.jpa", 0xaf);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_deadyuge.jpa", 0xb1);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_yugami.jpa", 0x1ee);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_hityuge.jpa", 0x167);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_kira.jpa", 0x168);
}

TSpineEnemy* TBossWanwanManager::createEnemyInstance()
{
	return new TBossWanwan("ボスワンワン");
}

void TBossWanwanManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "bwanwan_body.bmd", 0x10220000, 0 },
		{ "bwanwan_chain.bmd", 0x10220000, 0 },
		{ "bwanwan_picket.bmd", 0x10220000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TBossWanwanManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TBWParams("/enemy/bosswanwan.prm");
	TEnemyManager::load(stream);

	initJParticle();
}

DEFINE_NERVE(TNerveBWGraphWander, TLiveActor)
{
	// TODO: not decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBWRoll, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		J3DFrameCtrl* ctrl = self->getMActor()->getFrameCtrl(0);
		ctrl->setFrame(0.0f);
		ctrl->setRate(0.0f);
		self->unk16C = 1;
	}

	if (self->isReachedToGoal()) {
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		J3DFrameCtrl* ctrl = self->getMActor()->getFrameCtrl(0);
		ctrl->setRate(SMSGetAnmFrameRate());
		return TRUE;
	}

	f32 speed = self->getBWParams()->mSLAttackSpeed.get();
	self->walkToCurPathNode(speed, self->getTurnSpeed(), 0.0f);
	return FALSE;
}

DEFINE_NERVE(TNerveBWBark, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		self->changeBck(0);
		self->unk16C = 0;
		self->unk168 = 0.0f;
		if (self->unk194 == 0) {
			if (self->unk17C) {
				JPABaseEmitter* emitter = gpMarioParticleManager->emit(
				    0xae, &self->unk158->mPosition, 0, nullptr);
				if (emitter)
					emitter->setGlobalScale(
					    JGeometry::TVec3<f32>(0.3f, 0.5f, 0.3f));
			}
			self->unk194   = 0;
			self->unk17C   = 0;
			const Vec* pos = &self->unk158->mPosition;
			if (gpMSound->gateCheck(0x2966))
				MSoundSESystem::MSoundSE::startSoundActor(0x2966, pos, 0,
				                                          nullptr, 0, 4);
		}
	}

	if (spine->getTime() == 0x118)
		self->mHitPoints = self->getBWParams()->mSLBWHitPointMax.get();

	if (self->getMActor()->curAnmEndsNext(0, nullptr)) {
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		if ((self->unk198 & 2) == 0)
			gpMarDirector->getConsole()->startAppearBalloon(0x1b, true);
		self->unk198 |= 2;
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBWJump, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		const JGeometry::TVec3<f32>& goal = self->getUnk104().getPoint();
		f32 speed                         = self->unk124->unkC;
		self->mVelocity
		    = self->calcVelocityToJumpToY(goal, speed, self->getGravityY());
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->unk16C = 0;
	}

	if (self->isReachedToGoal()) {
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		return TRUE;
	}

	self->walkToCurPathNode(0.0f, self->getTurnSpeed(), 0.0f);
	return FALSE;
}

DEFINE_NERVE(TNerveBWStun, TLiveActor)
{
	// TODO: not decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBWWakeup, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		self->changeBck(6);
		self->getMActor()->setBtpFromIndex(2);
		self->unk16C = 0;
		self->unk168 = 0.0f;
	}

	if (self->getMActor()->curAnmEndsNext(0, nullptr)) {
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBWJumpToBath, TLiveActor)
{
	// TODO: not decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBWDie, TLiveActor)
{
	// TODO: not decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBWJumpAway, TLiveActor)
{
	// TODO: not decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBWShake, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();
	MActor* actor     = self->getMActor();

	if (spine->getTime() == 0)
		self->changeBck(2);

	if (actor->curAnmEndsNext(0, nullptr)) {
		if (self->unk17C) {
			JPABaseEmitter* emitter = gpMarioParticleManager->emit(
			    0xae, &self->unk158->mPosition, 0, nullptr);
			if (emitter)
				emitter->setGlobalScale(
				    JGeometry::TVec3<f32>(0.3f, 0.5f, 0.3f));
		}
		self->unk194 = false;
		self->unk17C = 0;
		const Vec* pos = &self->unk158->mPosition;
		if (gpMSound->gateCheck(0x2967))
			MSoundSESystem::MSoundSE::startSoundActor(0x2967, pos, 0, nullptr,
			                                          0, 4);
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBWFall, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		const JGeometry::TVec3<f32>& target = self->unk158->mPosition;
		JGeometry::TVec3<f32> velocity
		    = self->calcVelocityToJumpToY(target, 5.0f, self->getGravityY());
		self->setGoalPath(TPathNode(self->unk158->mPosition));
		self->mVelocity = velocity;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->unk16C = 0;
	}

	if (self->isReachedToGoal()) {
		self->mPosition = self->unk158->mPosition;
		self->unk124->reset();
		self->unk124->reset2();
		self->goToShortestNextGraphNode();
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		return TRUE;
	}
	return FALSE;
}
