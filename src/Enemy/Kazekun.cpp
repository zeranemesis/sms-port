#include <Enemy/Kazekun.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
// rogue include: pulls in the four MActorMtxCalcType names that the
// original .rodata carries after the dummy string pair.
#include <M3DUtil/InfectiousStrings.hpp>
#include <M3DUtil/MActor.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <System/Particles.hpp>
#include <Player/MarioAccess.hpp>
#include <MSound/MSound.hpp>
#include <MarioUtil/MtxUtil.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// TODO: this TU is only partially decompiled. TKazekun::init/
// calcRootMatrix/attackToMario/behaveToWater, TKazekunParams,
// TKazekunManager::load and ::createModelData, flyAroundMario and
// doAttackPose all still need reconstruction.
// See build/GMSP01/asm/Enemy/Kazekun.s.
//
// NOTE: this file uses -inline deferred, under which the compiler emits
// function bodies in the REVERSE of source order (see
// docs/AGENT_MATCHING_TIPS.md / validate-symbol-order.py). Functions below
// are intentionally ordered so the emitted .text matches mario.MAP.

TKazekun::TKazekun(const char* name)
    : TSmallEnemy(name)
{
	unk1B0 = 0;
	onLiveFlag(LIVE_FLAG_UNK10);
}

void TKazekun::init(TLiveManager* manager)
{
	mManager = manager;
	manager->manageActor(this);

	mMActorKeeper = new TMActorKeeper(manager, 1);
	mMActor        = mMActorKeeper->createMActor("kazekun.bmd", 0);

	mSpine->initWith(&TNerveKazekunSearch::theNerve());

	mHeadHeight        = 40.0f;
	mBodyRadius        = 50.0f;
	mScaledBodyRadius  = 50.0f;

	initHitActor(0x10000029, 1, 0x80000000, mBodyScale * mBodyRadius,
	            mBodyScale * mHeadHeight, mBodyScale * mBodyRadius,
	            mBodyScale * mHeadHeight);

	onHitFlag(HIT_FLAG_NO_COLLISION);

	SMS_LoadParticle("/scene/kazekun/jpa/ms_kaze_appear.jpa", 0xcf);
	SMS_LoadParticle("/scene/kazekun/jpa/ms_kaze_wind.jpa", 0x189);
	SMS_LoadParticle("/scene/kazekun/jpa/ms_kaze_blur.jpa", 0x18a);

	initAnmSound();

	unk194.set(mPosition);

	reset();
}

void TKazekun::reset()
{
	// Frame-padding: target frame is 8 bytes larger (MWCC stack-padding quirk).
	char framePad_8_reset[8];
	(void)framePad_8_reset;

	unk1A0.set(0.0f, 0.0f, 0.0f, 1.0f);
	JGeometry::TVec3<f32> temp = unk194;
	mPosition.set(temp);
	onLiveFlag(LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8);
	setAnmSound(nullptr);
}

void TKazekun::calcRootMatrix()
{
	if (mMActor) {
		TSpineEnemy::calcRootMatrix();
	} else {
		// TODO: quaternion -> matrix reconstruction
	}
}

void TKazekun::bind()
{
	mLinearVelocity += mVelocity;
}

void TKazekun::behaveToWater(THitActor*)
{
	bool same = mSpine->getLatestNerve() == &TNerveKazekunTurn::theNerve()
	    || mSpine->getLatestNerve() == &TNerveKazekunPreAttack::theNerve()
	    || mSpine->getLatestNerve() == &TNerveKazekunAttack::theNerve();
	if (same) {
		mSpine->reset();
		mSpine->setNext(&TNerveKazekunHitWater::theNerve());
	}
}

static const char* Kazekun_bastable[] = {
	"/scene/Kazekun/bas/kazekun_appear.bas",
	"/scene/Kazekun/bas/kazekun_attack.bas",
	nullptr,
	"/scene/Kazekun/bas/kazekun_vanish.bas",
	"/scene/Kazekun/bas/kazekun_wait.bas",
};

const char** TKazekun::getBasNameTable() const
{
	return Kazekun_bastable;
}

void TKazekun::attackToMario()
{
	bool same = mSpine->getLatestNerve() == &TNerveKazekunTurn::theNerve()
	    || mSpine->getLatestNerve() == &TNerveKazekunPreAttack::theNerve()
	    || mSpine->getLatestNerve() == &TNerveKazekunAttack::theNerve();
	if (same) {
		SMS_SendMessageToMario(this, 0xe);
	}
}

bool TKazekun::isCollidMove(THitActor*)
{
	return false;
}

void TKazekun::setDeadAnm()
{
	mMActor->getFrameCtrl(0)->init(1);
	mMActor->getFrameCtrl(0)->setFrame(0.0f);
}

TKazekunParams::TKazekunParams(const char* name)
    : TSmallEnemyParams(name)
    , PARAM_INIT(mAppearDist, 1000.0f)
    , PARAM_INIT(mAroundDist, 400.0f)
    , PARAM_INIT(mAroundSpeed, 30.0f)
    , PARAM_INIT(mAroundTime, 600)
    , PARAM_INIT(mAttackSpeed, 30.0f)
    , PARAM_INIT(mAirFric, 0.97f)
    , PARAM_INIT(mResetTime, 300)
    , PARAM_INIT(mResetTimeHitting, 1500)
    , PARAM_INIT(mPoseTime, 120)
    , PARAM_INIT(mDicideTiming, 0.1f)
    , PARAM_INIT(mTurnOffsetY, 200.0f)
    , PARAM_INIT(mLostOffsetYUp, 500.0f)
    , PARAM_INIT(mLostOffsetYDown, 500.0f)
    , PARAM_INIT(mPoseSpeed, 7.6f)
    , PARAM_INIT(mPoseOmegaRate, 0.04f)
{
	load(name);
}

TKazekunManager::TKazekunManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TKazekunManager::load(JSUMemoryInputStream& stream)
{
	TKazekunParams* params = new TKazekunParams("/enemy/kazekun.prm");
	unk38 = params;
	params->mSLAttackRadius.set(50);
	params->mSLAttackHeight.set(40);
	params->mSLDamageRadius.set(50);
	params->mSLDamageHeight.set(40);
	TSmallEnemyManager::load(stream);
	unk5C = 0;
}

void TKazekunManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
	    { "kazekun.bmd", 0x10210000, 0 },
	    { nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

DEFINE_NERVE(TNerveKazekunSearch, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->reset();
	}

	self->updateSquareToMario();

	f32 range = self->getSaveParam2()->mAppearDist.get();
	if (self->getDistToMarioSquared() <= range * range) {
		spine->pushAfterCurrent(&TNerveKazekunAppear::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunAppear, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->offLiveFlag(LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8);
		// 0xcf: particle id with no name in System/Particles.hpp
		gpMarioParticleManager->emit(0xcf, &self->mPosition, 0, nullptr);
		self->mMActor->setBck("kazekun_appear");
		self->setCurAnmSound();
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveKazekunTurn::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunTurn, TLiveActor)
{
	bool lost;
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBck("kazekun_wait");
		self->setCurAnmSound();
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
	}

	self->flyAroundMario();

	lost = true;
	f32 diff = gpMarioPos->y - self->unk194.y;
	if (!(diff < -self->getSaveParam2()->mLostOffsetYDown.get()
	      || self->getSaveParam2()->mLostOffsetYUp.get() < diff)) {
		lost = false;
	}

	if (lost) {
		spine->pushAfterCurrent(&TNerveKazekunDisappear::theNerve());
		return true;
	}

	if (self->getSaveParam2()->mAroundTime.get() < (f32)spine->getTime()) {
		spine->pushAfterCurrent(&TNerveKazekunPreAttack::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunPreAttack, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->doAttackPose(true);
		gpMSound->startSoundActor(MSD_SE_EN_KAZEKUN_READY, &self->mPosition, 0,
		                          nullptr, 0, 4);
		self->setAnmSound(nullptr);
	}

	if (self->getSaveParam2()->mPoseTime.get()
	        * self->getSaveParam2()->mDicideTiming.get()
	    < spine->getTime()) {
		JGeometry::TVec3<f32> pos = SMS_GetMarioPos();
		self->setGoalPath(TPathNode(pos));
	}

	self->doAttackPose(false);

	if (self->getSaveParam2()->mPoseTime.get() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveKazekunAttack::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunAttack, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBck("kazekun_attack");
		self->setCurAnmSound();

		JGeometry::TVec3<f32> dir = self->unk104.getPoint();
		dir -= self->mPosition;
		dir.setLength(self->getSaveParam2()->mAttackSpeed.get());
		self->mVelocity = dir;

		JGeometry::TQuat4<f32> prev = self->unk1A0;
		JGeometry::TVec3<f32> vel   = self->mVelocity;

		TPosition3f mtx;
		JGeometry::TVec3<f32> up;
		up.set(0.0f, 1.0f, 0.0f);
		SMS_CalcToDirMatrix(mtx, vel, up);

		JGeometry::TQuat4<f32> q;
		mtx.getQuat(q);

		JGeometry::TVec3<f32> axis;
		mtx.getYDir(axis);

		JGeometry::TQuat4<f32> rot;
		rot.setRotate(axis, 0.0f);

		q.mul(rot);
		prev.slerp(q, 0.1f);
		prev.normalize();
		self->unk1A0 = prev;
	}

	JGeometry::TQuat4<f32> prev = self->unk1A0;
	JGeometry::TVec3<f32> vel   = self->mVelocity;

	TPosition3f mtx;
	JGeometry::TVec3<f32> up;
	up.set(0.0f, 1.0f, 0.0f);
	SMS_CalcToDirMatrix(mtx, vel, up);

	JGeometry::TQuat4<f32> q;
	mtx.getQuat(q);

	JGeometry::TVec3<f32> axis;
	mtx.getYDir(axis);

	JGeometry::TQuat4<f32> rot;
	rot.setRotate(axis, 0.0f);

	q.mul(rot);
	prev.slerp(q, 0.1f);
	prev.normalize();
	self->unk1A0 = prev;

	JGeometry::TVec3<f32> v = self->mVelocity;
	v.scale(self->getSaveParam2()->mAirFric.get());
	self->mVelocity = v;

	if (v.squared() < 1.0f) {
		spine->pushAfterCurrent(&TNerveKazekunDisappear::theNerve());
		self->unk1B0 = self->getSaveParam2()->mResetTime.get();
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunDisappear, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBck("kazekun_vanish");
		self->setCurAnmSound();
		// 0xcf: particle id with no name in System/Particles.hpp
		gpMarioParticleManager->emit(0xcf, &self->mPosition, 0, nullptr);

		JGeometry::TVec3<f32> vel(0.0f, 0.0f, 0.0f);
		self->mVelocity = vel;

		self->onHitFlag(HIT_FLAG_NO_COLLISION);
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveKazekunWait::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunWait, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->onLiveFlag(LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8);
		self->setAnmSound(nullptr);
	}

	if (self->unk1B0 < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveKazekunSearch::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunHitWater, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBck("kazekun_hit");
		self->setCurAnmSound();
		gpMSound->startSoundActor(MSD_SE_EN_KAZEKUN_DOWN, &self->mPosition, 0,
		                          nullptr, 0, 4);
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveKazekunDisappear::theNerve());
		self->unk1B0 = self->getSaveParam2()->mResetTimeHitting.get();
		return true;
	}

	JGeometry::TVec3<f32> vel(0.0f, 0.0f, 0.0f);
	self->mVelocity = vel;
	return false;
}
