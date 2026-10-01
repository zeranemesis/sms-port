#include <Enemy/ChuuHana.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/Conductor.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DNode.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <System/Particles.hpp>
#include <System/MarDirector.hpp>
#include <GC2D/GCConsole2.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/MirrorActor.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapMirror.hpp>
#include <Player/MarioAccess.hpp>
#include <dolphin/mtx.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// NOTE: this TU is -inline deferred, so the out-of-line functions below are
// defined in the *reverse* order of mario.MAP's .text layout.

// fabricated: fills the 24 bytes of .rodata (12 zero bytes then 1.0f, 1.0f,
// 1.0f) that the original emits between the InfectiousStrings literals and the
// tyuhana_bastable BAS paths.
static void dummy(Vec* v)
{
	*v = (Vec) { 0.0f, 0.0f, 0.0f };
	*v = (Vec) { 1.0f, 1.0f, 1.0f };
}

static TChuuHana* gpCurChuuHana;

static const char* tyuhana_bastable[] = {
	"/scene/tyuhana/bas/tyuhana_chance_end.bas",
	nullptr,
	"/scene/tyuhana/bas/tyuhana_chance_start.bas",
	"/scene/tyuhana/bas/tyuhana_jump.bas",
	"/scene/tyuhana/bas/tyuhana_push.bas",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/tyuhana/bas/tyuhana_walk.bas",
};

s32 TChuuHana::mCheckOnPanelTimeRoll = 20;
s32 TChuuHana::mCheckOnPanelTime     = 400;
u8 TChuuHana::mBodyJntIndex          = 1;
u8 TChuuHana::mEyeJntIndex           = 12;
u8 TChuuHana::mFootJntIndex          = 5;
u8 TChuuHana::mNewSw                 = 1;
u8 TChuuHana::mCompareHeight         = 1;
f32 TChuuHana::mSmallMirrorR         = 650.0f;
f32 TChuuHana::mMediumMirrorR        = 900.0f;
f32 TChuuHana::mLargeMirrorR         = 1100.0f;
u8 TChuuHana::mAttackVersion         = 1;
u8 TChuuHana::mDamageSw              = 1;

TChuuHanaSaveLoadParams::TChuuHanaSaveLoadParams(const char* path)
    : TWalkerEnemyParams(path)
    , PARAM_INIT(mSLGetWaterPow, 1.0f)
    , PARAM_INIT(mSLGetGroundPow, 1.0f)
    , PARAM_INIT(mSLKeepBalanceTime, 200)
    , PARAM_INIT(mSLCheckFrame, 5)
    , PARAM_INIT(mSLReverseHeightS, 15.0f)
    , PARAM_INIT(mSLStretchHeightS, 10.0f)
    , PARAM_INIT(mSLMediumStretchHeightS, 7.0f)
    , PARAM_INIT(mSLSmallStretchHeightS, 3.0f)
    , PARAM_INIT(mSLReverseHeightM, 15.0f)
    , PARAM_INIT(mSLStretchHeightM, 10.0f)
    , PARAM_INIT(mSLMediumStretchHeightM, 7.0f)
    , PARAM_INIT(mSLSmallStretchHeightM, 3.0f)
    , PARAM_INIT(mSLReverseHeightL, 15.0f)
    , PARAM_INIT(mSLStretchHeightL, 10.0f)
    , PARAM_INIT(mSLMediumStretchHeightL, 7.0f)
    , PARAM_INIT(mSLSmallStretchHeightL, 3.0f)
    , PARAM_INIT(mSLWalkGravity, 4.0f)
    , PARAM_INIT(mSLWaterHitGravity, 0.2f)
    , PARAM_INIT(mSLJumpGravity, 0.2f)
    , PARAM_INIT(mSLJumpSp, 12.0f)
    , PARAM_INIT(mSLJumpHeight, 300.0f)
    , PARAM_INIT(mSLGetWaterPow2, 1.0f)
    , PARAM_INIT(mSLTacklePow, 100.0f)
    , PARAM_INIT(mSLDashRate, 2.0f)
    , PARAM_INIT(mSLAttackTimer, 300)
    , PARAM_INIT(mSLHitWaterTimer, 60)
{
	TParams::load(mPrmPath);
}

TChuuHanaManager::TChuuHanaManager(const char* name)
    : TSmallEnemyManager(name)
    , unk60(0)
    , unk68(0)
    , unk6C(0)
    , unk70(0)
{
	gpCurChuuHana = nullptr;
	for (int i = 0; i < 3; ++i)
		unk64[i] = 0;
}

void TChuuHanaManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TChuuHanaSaveLoadParams("/enemy/chuuhana.prm");
}

void TChuuHanaManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & 2) {
		// TODO: matches except for a 0x20 byte smaller stack frame
		if (SMS_GetMarioGrPlane()->mActor != nullptr
		    && SMS_GetMarioGrPlane()->mActor->getActorType() == 0x400000CF) {
			if (unk60 < 900) {
				++unk60;
				if (unk60 == 900)
					SMSGetMarDirector()->getConsole()->startAppearBalloon(0x2E, true);
			}
		}

		if (unk68 >= 60 && unk68 < 80) {
			unk60 = 1000;
			unk68 = 80;
			SMSGetMarDirector()->getConsole()->startAppearBalloon(0x2F, true);
		}

		if (unk6C == 5 || unk6C == 10) {
			++unk6C;
			SMSGetMarDirector()->getConsole()->startAppearBalloon(0x31, true);
		}

		if (unk70 == 1) {
			++unk70;
			unk68 = 80;
			SMSGetMarDirector()->getConsole()->startAppearBalloon(0x30, true);
		}
	}

	TEnemyManager::perform(cue, graphics);
}

TSmallEnemy* TChuuHanaManager::createEnemyInstance() { return new TChuuHana; }

void TChuuHanaManager::initSetEnemies()
{
	static const char* graphlist[] = {
		"kohana0", "kohana1", "kohana1", "kohana2", "kohana2", "kohana2",
	};

	for (int i = 0; i < mCapacity; ++i) {
		TGraphWeb* graph   = gpConductor->getGraphByName(graphlist[i]);
		TChuuHana* chuuHana = (TChuuHana*)unk18[i];

		if (i == 0)
			chuuHana->unk21C = &unk64[0];
		else if (i < 3)
			chuuHana->unk21C = &unk64[1];
		else
			chuuHana->unk21C = &unk64[2];

		JGeometry::TVec3<f32> point;
		graph->getGraphNode(TMsRange<s32>(0, graph->getNodeNum()).rand())
		    .getPoint(&point);
		chuuHana->mPosition = point;
		chuuHana->mPosition.y += 50.0f;
		chuuHana->onLiveFlag(LIVE_FLAG_AIRBORNE);
		chuuHana->getTracer()->setGraph(graph);
		chuuHana->reset();
	}
}

static BOOL ChuuHanaBodyCallback(J3DNode* node, int param_2)
{
	// TODO: not yet reconstructed. Rolls the body joint around the roll
	// axis (unk204 x up) by unk210 degrees while in TNerveChuuHanaRoll.
	return TRUE;
}

TChuuHanaAseParCallback::TChuuHanaAseParCallback(TChuuHana* owner)
    : mOwner(owner)
{
}

void TChuuHanaAseParCallback::execute(JPABaseEmitter* emitter,
                                      JPABaseParticle*)
{
	if (!mOwner->checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		emitter->setGlobalRTMatrix(mOwner->getMActor()->getModel()->getAnmMtx(
		    TChuuHana::mEyeJntIndex));
		emitter->setGlobalScale(JGeometry::TVec3<f32>(2.5f, 2.5f, 2.5f));
	}
}

void TChuuHanaAseParCallback::draw(JPABaseEmitter*, JPABaseParticle*) { }

TChuuHana::TChuuHana(const char* name)
    : TWalkerEnemy(name)
    , unk194(0.0f)
    , unk198(0.0f)
    , unk19C(0.0f)
    , unk1A0(0)
    , unk1A4(0)
    , unk1A8(0.0f)
    , unk1AC(0)
    , unk1B0(1)
    , unk1B1(0)
    , unk1B2(0)
    , unk1B8(0.0f)
    , unk210(0.0f)
    , unk214(0)
    , unk215(0)
    , unk218(nullptr)
    , unk21C(nullptr)
    , unk220(0.0f)
    , unk224(0)
    , unk228(this)
{
}

void TChuuHana::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000016;
	unk150     = 0x11;
	offHitFlag(HIT_FLAG_UNK40000000);
	mSpine->initWith(&TNerveChuuHanaWalkOnPanel::theNerve());
	mMActor->setJointCallback(mBodyJntIndex, ChuuHanaBodyCallback);
	unk130 = 1;
	mMActor->initNormalMotionBlend();
	unk1B4 = (TChuuHanaSaveLoadParams*)getSaveParam();
	mMActor->getModel()->calc();
	TMirrorActor* mirrorActor = new TMirrorActor("チュウハナin鏡");
	mirrorActor->init(getModel(), 0);
}

void TChuuHana::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("default.bmd", 3);
}

void TChuuHana::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);

	JGeometry::TVec3<f32> marioPos = SMS_GetMarioPos();
	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)
	    && (gpMirrorModelManager->isInMirror(mPosition)
	        || gpMirrorModelManager->isInMirror(marioPos))) {
		if (cue & 2) {
			calcRootMatrix();
			mMActor->calc();
		}

		if (cue & 4)
			mMActor->viewCalc();
	}
}

void TChuuHana::reset()
{
	gpCurChuuHana = this;
	TWalkerEnemy::reset();
	unk215      = 0;
	unk1A4      = 30;
	mHeadHeight = 200.0f;
	unk1F8      = mPosition;
	unk19C      = 0.0f;
	unk198      = 0.0f;
	unk1A0      = 0;
	unk224      = 0;
	setSafeGoal();
}

void TChuuHana::setBckAnm(int index)
{
	unk194 = 1.0f;
	// TODO: matches except for a 0x10 byte smaller stack frame
	getMActor()->setMotionBlendRatioForBck(unk194);
	getMActor()->setBckOldMotionBlendAnmPtr(getMActor()->getBckAnm());
	TSmallEnemy::setBckAnm(index);
}

void TChuuHana::behaveToWater(THitActor*)
{
	// Sprinting into the chuuhana launches it away from Mario. Rolling chuuhana
	// keep rolling, chuuhana that are standing on something or busy get shoved
	// off in the direction of Mario, and a balancing chuuhana is launched at
	// Mario instead.
	unk165 = true;
	unk224 = 0;
	((TChuuHanaManager*)mManager)->unk68++;

	if (mSpine->getCurrentNerve() == &TNerveChuuHanaRoll::theNerve()) {
		JGeometry::TVec3<f32> dir(mPosition.x - gpMarioPos->x, 0.0f,
		                          mPosition.z - gpMarioPos->z);
		MsVECNormalize(&dir, &dir);
		dir.scale(unk1B4->mSLGetWaterPow.get(), dir);

		JGeometry::TVec3<f32> vel = mVelocity;
		vel = dir + vel;
		vel.y     = 0.0f;
		mVelocity = vel;

		mLiveFlag |= LIVE_FLAG_AIRBORNE;
		mPosition.y += 10.0f;
	} else if (mSpine->getCurrentNerve() == &TNerveChuuHanaWalkOnPanel::theNerve()
	           || mSpine->getCurrentNerve() == &TNerveChuuHanaAttack::theNerve()
	           || mSpine->getCurrentNerve() == &TNerveChuuHanaWait::theNerve()
	           || (mNewSw
	               && mSpine->getCurrentNerve()
	                      == &TNerveChuuHanaStick::theNerve())) {
		unk165 = true;
		if (mAttackVersion)
			*unk21C = 1;

		JGeometry::TVec3<f32> dir(mPosition.x - gpMarioPos->x, 0.0f,
		                          mPosition.z - gpMarioPos->z);
		MsVECNormalize(&dir, &dir);
		dir.scale(unk1B4->mSLGetWaterPow2.get(), dir);

		if (!isAirborne()) {
			if (mCompareHeight)
				mPosition.y += 2.0f;
			else
				mPosition.y += 1.0f;
		}

		mVelocity = dir;
		mLiveFlag |= LIVE_FLAG_AIRBORNE;

		if (mSpine->getCurrentNerve() != &TNerveChuuHanaStick::theNerve())
			mSpine->pushNerve(&TNerveChuuHanaStick::theNerve());

		mSprayedByWaterCooldown = 0;
	} else if (!mNewSw
	           && mSpine->getCurrentNerve()
	                  == &TNerveChuuHanaKeepBalance::theNerve()) {
		JGeometry::TVec3<f32> dir(mPosition.x - gpMarioPos->x, 10.0f,
		                          mPosition.z - gpMarioPos->z);
		MsVECNormalize(&dir, &dir);
		dir.scale(2.0f * unk1B4->mSLGetWaterPow.get(), dir);
		mVelocity = dir;

		mLiveFlag |= LIVE_FLAG_AIRBORNE;
		mPosition.y += 20.0f;
	}
}

void TChuuHana::attackToMario()
{
	// TODO: not yet reconstructed
}

void TChuuHana::moveObject()
{
	// TODO: not yet reconstructed
	TWalkerEnemy::moveObject();
}

bool TChuuHana::isCollidMove(THitActor* actor)
{
	// Get shoved into a random direction. Only other enemies can shove us.
	u32 actorType = actor->getActorType();
	bool canShove = actorType - ACTOR_TYPE_ENEMY <= 0x16 ? true : false;
	if (canShove) {
		bool rolling = mSpine->getCurrentNerve() == &TNerveChuuHanaRoll::theNerve()
		                   ? true
		                   : false;
		if (rolling) {
			if (mSpine->getCurrentNerve() != &TNerveChuuHanaWalkOnPanel::theNerve())
				goto done;

			mSpine->pushNerve(&TNerveChuuHanaRoll::theNerve());
			goto done;
		}

		if (unk1B2 != 0)
			goto done;

		if (mSpine->getCurrentNerve() == &TNerveChuuHanaAttack::theNerve())
			goto done;

		if (((TLiveActor*)actor)->getInstanceIndex() <= mInstanceIndex)
			goto done;

		// one chance in four to actually react
		if (TMsRange<s32>(0, 100).rand() % 4 != 0)
			goto done;

		setSafeGoal();
		unk1B2 = 1;
	}

done:
	return !(mSpine->getCurrentNerve() == &TNerveChuuHanaObject::theNerve());
}

void TChuuHana::isRolling()
{
	// TODO: UNUSED in the target (0x8C bytes), not yet reconstructed
}

void TChuuHana::forceRoll()
{
	// TODO: UNUSED in the target (0xC4 bytes), not yet reconstructed
}

void TChuuHana::calcRootMatrix()
{
	gpCurChuuHana = this;
	if (mSpine->getCurrentNerve() == &TNerveChuuHanaJumpPrepare::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveChuuHanaFall2::theNerve()) {
		J3DModel* model = mMActor->getModel();
		MsMtxSetXYZRPH(model->getBaseTRMtx(), mPosition.x,
		               mPosition.y + unk220, mPosition.z, mRotation.x,
		               mRotation.y, mRotation.z);
		model->setBaseScale(mScaling);
	} else {
		TSpineEnemy::calcRootMatrix();
	}

	if (mSpine->getCurrentNerve() == &TNerveChuuHanaKeepBalance::theNerve())
		gpMarioParticleManager->emitParticleCallBack(0x130, &mPosition, 1,
		                                             &unk228, this);

	if (isBckAnm(6)
	    && mMActor->getFrameCtrl(ANM_TYPE_BCK)->checkPass(2.0f)) {
		if (JPABaseEmitter* emitter
		    = gpMarioParticleManager->emitAndBindToMtxPtr(
		        0x54, mMActor->getModel()->getAnmMtx(mBodyJntIndex), 0,
		        nullptr)) {
			emitter->setGlobalScale(mScaling);
		}
	}
}

void TChuuHana::bind()
{
	// TODO: not yet reconstructed
	TLiveActor::bind();
}

void TChuuHana::margeVelocity(JGeometry::TVec3<f32>&)
{
	// TODO: UNUSED in the target (0xC4 bytes), not yet reconstructed
}

BOOL TChuuHana::receiveMessage(THitActor* sender, u32 message)
{
	if (unk1A0 == 0) {
		if (mSpine->getCurrentNerve() == &TNerveChuuHanaWalkOnPanel::theNerve()
		    || mSpine->getCurrentNerve()
		           == &TNerveChuuHanaKeepBalance::theNerve()) {
			if (message == HIT_MESSAGE_HIP_DROP)
				unk1A0 = 1;
		}
	}

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		if (mSprayedByWaterCooldown == 0) {
			mSprayedByWaterCooldown = 1;
			behaveToWater(sender);
		}
		unk165 = 1;
		gpMarioParticleManager->emit(0xE7, &sender->mPosition, 0, nullptr);
		gpMSound->startSoundSet(0x6802, &mPosition, 0, 0.0f, 0, 0, 4);
		return TRUE;
	}

	return FALSE;
}

void TChuuHana::setWalkAnm()
{
	bool reverse = false;
	if (mCurrentBckAnm < 0)
		reverse = true;

	setBckAnm(12);
	if (reverse)
		mMActor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(10.0f * mInstanceIndex);
}

void TChuuHana::kill()
{
	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return;

	onLiveFlag(LIVE_FLAG_HIDDEN);
	if (unk218 != nullptr) {
		unk218->receiveMessage(this, HIT_MESSAGE_UNK8);
		unk218 = nullptr;
	}
	TSmallEnemy::kill();
}

// TODO: the flag bit this function asks for is unnamed; MapData.hpp only
// defines BG_CHECK_FLAG_ILLEGAL (0x10). The binary tests bit 27 of
// TBGCheckData::mFlags, which is a u16, so the test is always false. Passing
// 0x10 reproduces that instruction exactly: when the operand is narrowed to
// its declared 16 bits, MWCC encodes the mask as bit 31 - log2(flag).
#define PLANE_FLAG_UNK27 0x10

void TChuuHana::forceKill()
{
	if (mGroundPlane->checkFlag(PLANE_FLAG_UNK27)
	    || !(mGroundPlane->isDeathPlane() || mGroundPlane->isPool()
	         || mGroundPlane->isWaterSurface())) {
		if (gpMap->isInArea(mPosition.x, mPosition.z)
		    && !mGroundPlane->checkFlag(PLANE_FLAG_UNK27))
			return;
	}

	kill();
}

f32 TChuuHana::getGravityY() const
{
	f32 gravity = TLiveActor::getGravityY();
	if (mSpine->getCurrentNerve() == &TNerveChuuHanaWalkOnPanel::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveChuuHanaKeepBalance::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveChuuHanaForceJumped::theNerve())
		gravity = unk1B4->mSLWalkGravity.get();
	else if (mSpine->getCurrentNerve() == &TNerveChuuHanaStick::theNerve())
		gravity = unk1B4->mSLWaterHitGravity.get();
	else if (mSpine->getCurrentNerve() == &TNerveChuuHanaFall2::theNerve()
	         || mSpine->getCurrentNerve()
	                == &TNerveChuuHanaJumpPrepare::theNerve())
		gravity = unk1B4->mSLJumpGravity.get();
	return gravity;
}

void TChuuHana::checkOnPanel()
{
	// TODO: UNUSED in the target (0x148 bytes), not yet reconstructed
}

// TODO: fake pragma. In the target TUtil<f32>::sqrt is inlined here, which
// presumably makes willFall too big to be auto-inlined into the nerves; in
// our build sqrt stays out-of-line (unknown why) and willFall would get
// inlined into TNerveChuuHanaAttack::execute without this.
#pragma dont_inline on
bool TChuuHana::willFall(long time)
{
	f32 radius = mSmallMirrorR;
	if (mInstanceIndex > 0)
		radius = mMediumMirrorR;
	if (mInstanceIndex > 2)
		radius = mLargeMirrorR;

	if (time == mCheckOnPanelTimeRoll)
		radius += 250.0f;

	if (unk218 != nullptr) {
		if (mPosition.distance(unk218->mPosition) > radius) {
			setSafeGoal();
			return true;
		}
	}

	unk1B2 = 0;
	return false;
}
#pragma dont_inline off

// TODO: matches except for a 0x10 byte smaller stack frame
void TChuuHana::setGoal()
{
	JGeometry::TVec3<f32> goal;
	goal.set(mPosition);
	TMsRange<f32> range(-30.0f, 30.0f);
	f32 angle = range.rand();
	JGeometry::TVec3<f32> dir(0.0f, 0.0f, 1.0f);
	Mtx mtx;
	MsMtxSetRotRPH(mtx, mRotation.x, mRotation.y + angle, mRotation.z);
	MTXMultVec(mtx, &dir, &dir);
	goal.x += 1000.0f * dir.x;
	goal.z += 1000.0f * dir.z;
	setGoalPath(TPathNode(goal));
	unk1A4 = mCheckOnPanelTime;
	unk1B2 = 0;
}

void TChuuHana::setSafeGoal()
{
	unk1A4 = mCheckOnPanelTime;
	JGeometry::TVec3<f32> point;
	getTracer()
	    ->getGraph()
	    ->getGraphNode(
	        TMsRange<s32>(0, getTracer()->getGraph()->getNodeNum()).rand())
	    .getPoint(&point);
	setGoalPath(TPathNode(point));
	unk1B2 = 1;
}

void TChuuHana::rolling()
{
	// TODO: UNUSED in the target (0x120 bytes), not yet reconstructed
}

void TChuuHana::rollStart()
{
	// TODO: UNUSED in the target (0x18 bytes), not yet reconstructed
}

void TChuuHana::checkStretchType()
{
	// unk1A8 is the current stretch amount. Once it passes the threshold for
	// the instance's size the chuuhana reacts: balancing ones flip over, and
	// the rest switch to the force-jumped nerve.
	if (mSpine->getCurrentNerve() == &TNerveChuuHanaKeepBalance::theNerve()) {
		f32 limit = unk1B4->mSLReverseHeightS.get();
		if (mInstanceIndex > 0)
			limit = unk1B4->mSLReverseHeightM.get();
		if (mInstanceIndex > 2)
			limit = unk1B4->mSLReverseHeightL.get();

		if (unk1A8 > limit) {
			unk1B1 = 1;
			unk214 = 1;
			mSpine->pushNerve(&TNerveChuuHanaFall2::theNerve());
			mSpine->pushNerve(&TNerveChuuHanaJumpPrepare::theNerve());
		}
		return;
	}

	((TChuuHanaManager*)mManager)->unk6C++;

	f32 limit = unk1B4->mSLStretchHeightS.get();
	if (mInstanceIndex > 0)
		limit = unk1B4->mSLStretchHeightM.get();
	if (mInstanceIndex > 2)
		limit = unk1B4->mSLStretchHeightL.get();

	if (unk1A8 > limit) {
		unk1B1 = 0;
		unk214 = 0;
		setBckAnm(8);
		mSpine->pushNerve(&TNerveChuuHanaForceJumped::theNerve());
		return;
	}

	limit = unk1B4->mSLMediumStretchHeightS.get();
	if (mInstanceIndex > 0)
		limit = unk1B4->mSLMediumStretchHeightM.get();
	if (mInstanceIndex > 2)
		limit = unk1B4->mSLMediumStretchHeightL.get();

	if (unk1A8 > limit) {
		unk1B1 = 0;
		unk214 = 0;
		setBckAnm(9);
		mSpine->pushNerve(&TNerveChuuHanaForceJumped::theNerve());
		return;
	}

	limit = unk1B4->mSLSmallStretchHeightS.get();
	if (mInstanceIndex > 0)
		limit = unk1B4->mSLSmallStretchHeightM.get();
	if (mInstanceIndex > 2)
		limit = unk1B4->mSLSmallStretchHeightL.get();

	if (unk1A8 > limit) {
		unk1B1 = 0;
		unk214 = 0;
		setBckAnm(10);
		mSpine->pushNerve(&TNerveChuuHanaForceJumped::theNerve());
	}
}

void TChuuHana::entryCollision()
{
	// TODO: UNUSED in the target (0x44 bytes), not yet reconstructed
}

void TChuuHana::eventKill()
{
	// TODO: UNUSED in the target (0x4C bytes), not yet reconstructed
}

void TChuuHana::getEffectMtx()
{
	// TODO: UNUSED in the target (0x118 bytes), not yet reconstructed
}

const char** TChuuHana::getBasNameTable() const { return tyuhana_bastable; }

DEFINE_NERVE(TNerveChuuHanaWalkOnPanel, TLiveActor)
{
	TChuuHana* self = (TChuuHana*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setWalkAnm();
		*self->unk21C = 0;
	}

	if (self->unk218 == nullptr) {
		if (self->mGroundPlane->mActor != nullptr) {
			self->unk1F8 = self->mGroundPlane->mActor->mPosition;
			self->unk218 = (TLiveActor*)self->mGroundPlane->mActor;
		}
	} else {
		self->walkBehavior(2, 1.0f);
	}

	self->unk1A4 += 1;
	if (self->unk1A4 > 20) {
		self->unk1A4 = 0;
		if (self->willFall(TChuuHana::mCheckOnPanelTime))
			self->unk1A4 = -100;

		if (!self->isAirborne() && self->mGroundPlane->mActor == nullptr
		    && self->mPosition.y + 200.0f < self->unk1F8.y)
			spine->pushNerve(&TNerveChuuHanaFall2::theNerve());
	}

	if (self->isReachedToGoalXZ())
		self->setGoal();

	if (*self->unk21C != 0) {
		spine->pushNerve(&TNerveChuuHanaAttack::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveChuuHanaForceJumped, TLiveActor)
{
	TChuuHana* self = (TChuuHana*)spine->getBody();

	if (spine->getTime() == 0)
		self->setSafeGoal();

	if (self->unk214 != 0) {
		if (self->getCurAnmFrameNo(0) > 80.0f) {
			if (self->mGroundPlane->mActor != nullptr)
				const_cast<TLiveActor*>(self->mGroundPlane->mActor)
				    ->receiveMessage(self, 3);
			self->unk214 = 0;
		}
	}

	if (self->checkCurAnmEnd(0)) {
		if (self->unk1B1 != 0) {
			spine->pushNerve(&TNerveChuuHanaRoll::theNerve());
		} else {
			// back to the default (walk on panel) nerve, and queue it again
			// so the flower keeps circling the flower bed.
			spine->reset();
			spine->setDefaultNext();
			spine->pushAfterCurrent(spine->getDefault());
		}
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveChuuHanaKeepBalance, TLiveActor)
{
	// TODO: not yet reconstructed
	return FALSE;
}

DEFINE_NERVE(TNerveChuuHanaStick, TLiveActor)
{
	// TODO: not yet reconstructed
	return FALSE;
}

DEFINE_NERVE(TNerveChuuHanaRoll, TLiveActor)
{
	// TODO: not yet reconstructed
	return FALSE;
}

DEFINE_NERVE(TNerveChuuHanaFall, TLiveActor) { return FALSE; }

DEFINE_NERVE(TNerveChuuHanaFall2, TLiveActor)
{
	TChuuHana* self = (TChuuHana*)spine->getBody();

	if (spine->getTime() == 0)
		self->setBckAnm(6);

	self->unk220 *= 0.98f;
	if (!self->isAirborne() || spine->getTime() > 800) {
		spine->pushAfterCurrent(&TNerveChuuHanaObject::theNerve());
		self->kill();
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveChuuHanaObject, TLiveActor) { return FALSE; }

DEFINE_NERVE(TNerveChuuHanaAttack, TLiveActor)
{
	TChuuHana* self = (TChuuHana*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(12);
		self->getMActor()->setFrameRate(2.0f * SMSGetAnmFrameRate(),
		                                ANM_TYPE_BCK);
		self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));
	}

	if (SMS_GetMarioGroundPlane()->mActor != self->unk218)
		*self->unk21C = 0;

	if (spine->getTime() > self->unk1B4->mSLAttackTimer.get()) {
		spine->pushAfterCurrent(&TNerveChuuHanaWalkOnPanel::theNerve());
		spine->pushAfterCurrent(&TNerveChuuHanaWait::theNerve());
		return TRUE;
	}

	self->unk1A4 += 1;
	if (self->unk1A4 > 20) {
		self->unk1A4 = 0;
		if (self->willFall(TChuuHana::mCheckOnPanelTime))
			self->unk1A4 = -100;

		if (!self->isAirborne() && self->getGroundPlane()->mActor == nullptr
		    && self->mPosition.y + 200.0f < self->unk1F8.y)
			self->mSpine->pushNerve(&TNerveChuuHanaFall2::theNerve());
	}

	if (self->isReachedToGoalXZ())
		self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));

	self->walkBehavior(2, self->unk1B4->mSLDashRate.get());
	return FALSE;
}

DEFINE_NERVE(TNerveChuuHanaJumpPrepare, TLiveActor)
{
	TChuuHana* self = (TChuuHana*)spine->getBody();

	if (spine->getTime() == 0)
		self->setBckAnm(3);

	self->unk220
	    = self->mPosition.y
	      - self->getMActor()->getModel()->getAnmMtx(
	          TChuuHana::mFootJntIndex)[1][3];

	if (self->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(10.0f)) {
		JGeometry::TVec3<f32> target;
		target.x = 2.0f * self->unk1F8.x - self->mPosition.x;
		target.y = 2.0f * self->unk1F8.y - self->mPosition.y;
		target.z = 2.0f * self->unk1F8.z - self->mPosition.z;
		*self->unk21C = 0;
		target.y += self->unk1B4->mSLJumpHeight.get();
		f32 jumpSp = self->unk1B4->mSLJumpSp.get();
		JGeometry::TVec3<f32> vel
		    = self->calcVelocityToJumpToY(target, jumpSp, self->getGravityY());
		vel.y += 10.0f;
		self->mPosition.y += 10.0f;
		self->mVelocity = vel;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->unk130 = 0;
	}

	if (self->checkCurAnmEnd(0))
		return TRUE;

	return FALSE;
}

DEFINE_NERVE(TNerveChuuHanaWait, TLiveActor)
{
	TChuuHana* self = (TChuuHana*)spine->getBody();

	if (spine->getTime() == 0)
		self->setBckAnm(11);

	if (self->checkCurAnmEnd(0))
		return TRUE;

	return FALSE;
}
