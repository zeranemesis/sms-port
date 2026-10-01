#include <MoveBG/MapObjMamma.hpp>

// rogue include: dummy string pair, needed to match the .rodata prologue
#include <System/DummyStrings.hpp>
#include <MoveBG/MapObjBall.hpp>
#include <MoveBG/ItemManager.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <MSound/MSound.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <System/MarDirector.hpp>
#include <M3DUtil/MActor.hpp>

#include <Camera/Camera.hpp>
#include <Camera/CameraShake.hpp>
#include <Enemy/Beam.hpp>
#include <Enemy/SleepBossHanachan.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapMirror.hpp>
#include <Map/MapStaticObject.hpp>
#include <Map/JointObj.hpp>
#include <Map/JointModel.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DNode.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <M3DUtil/MActorUtil.hpp>
#include <Player/MarioAccess.hpp>
#include <MoveBG/MapObjFlag.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/MapObjWave.hpp>
#include <System/Particles.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <System/TargetArrow.hpp>
#include <GC2D/GCConsole2.hpp>

// The original calls JGeometry::TUtil<f32>::sqrt(v) / JGeometry::TUtil<f32>::inv_sqrt(v) out-of-line here
// (bl sqrt__Q29JGeometry8TUtil<f>Ff), with the range guard inside the
// callee. JGUtil.hpp only offers the inline spelling, so MWCC always
// expands these sites and the call never appears.
// FABRICATED: the callee is orig_sqrt/orig_inv_sqrt, so the `bl` itself still shows as
// one mismatched instruction. Making JGUtil.hpp out-of-line instead was
// measured repo-wide at -32.2 points - see docs/AGENT_MATCHING_TIPS.md.
#pragma dont_inline on
static f32 orig_sqrt(f32 v) {
	return JGeometry::TUtil<f32>::sqrt(v);
}
static f32 orig_inv_sqrt(f32 v) {
	return JGeometry::TUtil<f32>::inv_sqrt(v);
}
#pragma dont_inline off


// fabricated: the per-shape record the mirror operator walks.  Only the
// offsets the ROM touches are modelled -- 0x14 = next, 0x40/0x4C = the shape's
// bounding box, 0x60 = the material handed to SMS_ShowJoint().
struct TMirrorShapeNode {
	/* 0x0 */ u8 pad0[0x14];
	/* 0x14 */ TMirrorShapeNode* mNext;
	/* 0x18 */ u8 pad18[0x28];
	/* 0x40 */ JGeometry::TVec3<f32> mMin;
	/* 0x4C */ JGeometry::TVec3<f32> mMax;
	/* 0x58 */ u8 pad58[0x8];
	/* 0x60 */ void* mMaterial;
};

// NOTE: this TU uses -inline deferred, so definitions are emitted in reverse
// source order; keep them in reverse of marioEU.MAP address order.
//
// TODO: the .sdata layout below (mWitherTime .. mWaitTime) was recovered from
// the map's .sdata symbol list; the trailing unnamed word is still unaccounted
// for.
s32 TSandBase::mWitherTime                 = 800;
f32 TSandBase::mScaleMin                   = 0.00001f;
f32 TSandBombBase::mFiringFrameSpeed       = 3.0f;
f32 TSandBombBase::mFiringFrameDownSpeed   = 0.2f;
f32 TSandBombBase::mExplodeFrameSpeed      = 1.0f;
f32 TSandBombBase::mMarioJumpRate          = 0.12f;
f32 TSandBombBase::mExlodingRumbleTime     = 20.0f;
f32 TSandCastle::mCollisionRate            = 0.4f;
s32 TLeanMirror::mGoTargetTime             = 600;
s32 TLeanMirror::mDemoWaitTime             = -1;
s32 TLeanMirror::mDemoLightTime            = 360;
f32 TMammaBlockRotate::mRotSpeed           = 0.1f;
f32 TMammaBlockRotate::mRotReturnSpeed     = 128.0f;
f32 TMammaBlockRotate::mRotEnd             = 0.01f;
f32 TMammaBlockRotate::mMapGoSpeed         = 1.0f;
f32 TMammaBlockRotate::mMapBackSpeed       = 0.1f;
s32 TMammaBlockRotate::mWaitTime           = 600;

f32 TMapObjBall::getDepthAtFloating() { return unk18C; }

//
// TSandLeaf
//

// TODO: the map also lists a TWatermelon (a further derived goal watermelon,
// vtable 0x1F0, with its own control()).  It is never instantiated from this
// TU, so it is not declared here; see include/MoveBG/MapObjMamma.hpp.
u32 TSandLeaf::touchWater(THitActor* hit_actor)
{
	unk138->grow();
	return 1;
}

void TSandLeaf::control()
{
	TMapObjBase::control();
	mGroundHeight = gpMap->checkGround(mPosition.x, mPosition.y + 200.0f,
	                                   mPosition.z, &mGroundPlane);
	mPosition.y   = mGroundHeight;
}

//
// TSandBase
//

// UNUSED in the ROM (0x34 bytes).  The body below is a placeholder: nothing
// in this TU or the map's inlined copies reveals what the test was.
void TSandBase::isDown() const
{
}

bool TSandBase::withering()
{
	mScaling.y -= unk13C;
	if (mScaling.y < mScaleMin)
		mScaling.y = mScaleMin;

	SMSGetMSound()->startSoundActor(0x2099, &unk144->mPosition, 0, nullptr, 0,
	                                4);
	// the ROM materialises this comparison into a bool instead of using the
	// CR bit, so the ternary form has to be spelled out
	return mScaling.y <= mScaleMin ? true : false;
}

TSandBase::TSandBase(const char* name)
    : TMapObjBase(name)
    , unk138(0.0f)
    , unk13C(0.0f)
    , unk144(nullptr)
{
}

//
// TSandLeafBase
//

void TSandLeafBase::grow()
{
	// The ROM tests the two states with two plain compares, so this is a ||
	// and not a switch.
	if (mState == 1 || mState == 4) {
		if (mScaling.y < 1.0f) {
			mScaling.y += unk138;
			if (mScaling.y > 1.0f)
				mScaling.y = 1.0f;
		}
		if (mState == 1) {
			mMapCollisionManager->changeCollision(1);
			Mtx mtx;
			MsMtxSetTRS(mtx, mPosition, mRotation, mScaling);
			TMapCollisionBase* model = mMapCollisionManager->getUnk8();
			PSMTXCopy(mtx, model->unk20);
			model->setUp();
			unk144->startControlAnim(2);
			mState = 4;
		}
		f32 frame = unk144->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
		unk144->getMActor()->getFrameCtrl(ANM_TYPE_BCK)
		    ->setFrame(frame + SMSGetAnmFrameRate());
		SMSRumbleMgr->start(0x15, 5, &mPosition);
		SMSGetMSound()->startSoundActor(0x2099, &unk144->mPosition, 0, nullptr,
		                                0, 4);
		mStateTimer = mWitherTime;
	}
}

void TSandLeafBase::control()
{
	TMapObjBase::control();
	switch (mState) {
	case 2:
		SMSRumbleMgr->start(0x13, -1, &mPosition);
		if (withering()) {
			SMSRumbleMgr->stop(0x13);
			mMapCollisionManager->changeCollision(0);
			TMapCollisionBase* model = mMapCollisionManager->getUnk8();
			Mtx mtx;
			MsMtxSetTRS(mtx, mPosition, mRotation, mScaling);
			PSMTXCopy(mtx, model->unk20);
			model->setUp();
			mStateTimer = unk140;
			mState      = 3;
		}
		break;
	case 3:
		if (mStateTimer <= 0) {
			if (unk144->animIsFinished()) {
				unk144->awake();
				unk144->startAnim(1);
				SMSGetMSound()->startSoundActor(0x3802, &unk144->mPosition, 0,
				                                nullptr, 0, 4);
				mState = 5;
			}
		}
		break;
	case 5:
		if (unk144->animIsFinished()) {
			unk144->startAnim(0);
			mState = 1;
		}
		break;
	}
}

void TSandLeafBase::initMapObj()
{
	unk138     = 0.003f;
	unk13C     = 0.001f;
	unk140     = 0;
	mScaling.y = mScaleMin;
	TMapObjBase::initMapObj();

	unk144     = static_cast<TSandBomb*>(TMapObjBaseManager::newAndRegisterObj(
        "SandLeaf", mPosition, JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f),
        JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f)));
	unk144->unk138 = this;
	unk144->awake();
}

//
// TSandBomb
//

void TSandBomb::makeObjAppeared()
{
	TMapObjBase::makeObjAppeared();
	startControlAnim(1);
	startControlAnim(2);
}

u32 TSandBomb::touchWater(THitActor* hit_actor)
{
	unk140 = 0;
	f32 frame = getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	getMActor()->getFrameCtrl(ANM_TYPE_BCK)
	    ->setFrame(frame + TSandBombBase::mFiringFrameSpeed);
	frame = getMActor()->getFrameCtrl(ANM_TYPE_BRK)->getFrame();
	getMActor()->getFrameCtrl(ANM_TYPE_BRK)
	    ->setFrame(frame + TSandBombBase::mFiringFrameSpeed);
	frame = getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	soundBas(0x289A, 7.0f, TSandBombBase::mFiringFrameSpeed);
	soundBas(0x289B, 50.0f, TSandBombBase::mFiringFrameSpeed);
	soundBas(0x289C, 100.0f, TSandBombBase::mFiringFrameSpeed);
	soundBas(0x289D, 150.0f, TSandBombBase::mFiringFrameSpeed);
	if (getMActor()->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
		unk138->grow();
		startControlAnim(3);
		startControlAnim(4);
		startControlAnim(5);
		onLiveFlag(1);
	}
	return 1;
}

u32 TSandBomb::getSDLModelFlag() const
{
	return 0;
}

void TSandBomb::initMapObj() { TMapObjBase::initMapObj(); }

//
// TSandBombBase
//

void TSandBombBase::withered()
{
	mStateTimer = unk140;
	mState      = 3;
	unk144->sleep();
}

void TSandBombBase::expanded()
{
	// the ROM drives BCK (anmtype 0) here, not BRK, and only ever tests
	// animIsFinished() once
	f32 frame = unk144->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	unk144->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(frame + unk150);
	SMSGetMSound()->startSoundActor(0x20C6, &mPosition, 0, nullptr, 0, 4);
	if (animIsFinished())
		mState = 2;
}

void TSandBombBase::exploding()
{
	f32 frame = getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	getMActor()->getFrameCtrl(ANM_TYPE_BCK)
	    ->setFrame(frame + mExplodeFrameSpeed);
	frame = unk144->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	unk144->getMActor()->getFrameCtrl(ANM_TYPE_BCK)
	    ->setFrame(frame + mExplodeFrameSpeed);

	f32 dist = getDistanceXZ(*gpMarioPos);
	if (mActorType - 0x4000 <= 0xD0 ? true : false) {
		f32 cur = getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
		if (cur < 80.0f) {
			f32 marioY = gpMarioPos->y - 30.0f;
			if (cur > marioY) {
				if (dist < unk154) {
					SMS_SendMessageToMario(this, 7);
					SMS_ThrowMario(
					    JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f),
					    mMarioJumpRate * (unk154 - dist));
				}
			}
		}
	}
	if (animIsFinished()) {
		startControlAnim(6);
		mState = 8;
	}
}

void TSandBombBase::explode()
{
	startControlAnim(1);
	mScaling.y             = 1.0f;
	mMapCollisionManager->changeCollision(1);
	TMapCollisionBase* model = mMapCollisionManager->getUnk8();
	model->setUp();
	if (model->getUnkC())
		model->moveSRT(mPosition, mScaling, mRotation);
	JPABaseEmitter* emitter =
	    gpMarioParticleManager->emit(0x55, &mPosition, 0, nullptr);
	emitter->mGlobalDynamicsScale.set(unk14C, unk14C, unk14C);
	emitter->mGlobalParticleScale.set(unk14C, unk14C, unk14C);

	// The ROM folds the two map comparisons into a single bool.
	s32 mode = 0;
	if (gpMarDirector->mMap == 3 || gpMarDirector->mMap == 4)
		mode = 1;
	if (mode) {
		gpCameraShake->startShake(CAM_SHAKE_MODE_UNK13, 1.0f);
	}
	SMSGetMSound()->startSoundActor(0x28A4, &mPosition, 0, nullptr, 0, 4);
	SMSRumbleMgr->start(0x15, mExlodingRumbleTime, &mPosition);
	mState = 7;
}

void TSandBombBase::waitBeforeExplode()
{
	mState      = 6;
	mStateTimer = unk148;
}

void TSandBombBase::grow() { mState = 5; }

void TSandBombBase::control()
{
	TMapObjBase::control();
	switch (mState) {
	case 1: {
		f32 frame = getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame()
		            - mFiringFrameDownSpeed;
		if (frame < 0.0f) {
			getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(frame);
			getMActor()->getFrameCtrl(ANM_TYPE_BRK)->setFrame(frame);
		}
	} break;
	case 5: {
		f32 speed = mExplodeFrameSpeed;
		f32 frame = getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
		getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(frame + speed);
		frame = unk144->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
		unk144->getMActor()->getFrameCtrl(ANM_TYPE_BCK)
		    ->setFrame(frame + speed);
		frame = unk144->getMActor()->getFrameCtrl(ANM_TYPE_BRK)->getFrame();
		unk144->getMActor()->getFrameCtrl(ANM_TYPE_BRK)
		    ->setFrame(frame + speed);
		frame = unk144->getMActor()->getFrameCtrl(ANM_TYPE_BTP)->getFrame();
		unk144->getMActor()->getFrameCtrl(ANM_TYPE_BTP)
		    ->setFrame(frame + speed);
		if (animIsFinished())
			waitBeforeExplode();
	} break;
	case 6:
		if (mStateTimer <= 0)
			explode();
		break;
	case 7: exploding(); break;
	case 8: expanded(); break;
	case 2:
		SMSRumbleMgr->start(0x13, -1, &mPosition);
		if (withering()) {
			withered();
			SMSRumbleMgr->stop(0x13);
		}
		break;
	case 3:
		if (mStateTimer <= 0) {
			mState = 1;
			unk144->awake();
			unk144->startControlAnim(1);
			unk144->startControlAnim(2);
		}
		break;
	}
	if (unk144->getColNum() == 0)
		unk144->unk140 = 0;
}

TMapObjBase* TSandBombBase::findTriggerActor()
{
	// TODO: the exact helper the ROM used is not recovered; this produces the
	// same call shape but registers "SandBomb" here.
	return TMapObjBaseManager::newAndRegisterObj(
	    "SandBomb", mPosition, JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f),
	    JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
}

void TSandBombBase::loadAfter()
{
	// TODO: unk144 is typed TSandBomb* so that the 0x138/0x140 accesses below
	// type-check; for TSandCastle the real pointee is a different map object
	// that is not declared anywhere yet.
	unk144         = static_cast<TSandBomb*>(findTriggerActor());
	unk144->unk138 = this;
	unk144->appear();
}

void TSandBombBase::initMapObj()
{
	unk138     = 0.006f;
	unk13C     = 0.005f;
	unk140     = 60;
	unk148     = 60;
	unk154     = 1000.0f;
	mScaling.y = mScaleMin;
	TMapObjBase::initMapObj();
	unk150 = 0.5f;
	if (strcmp(getUnkF4(), "SandBombBasePyramid") == 0) {
		unk14C = 1.3f;
		unk154 = 1200.0f;
	} else if (strcmp(getUnkF4(), "SandBombBaseShit") == 0) {
		unk14C = 1.3f;
		unk154 = 1500.0f;
	} else if (strcmp(getUnkF4(), "SandBombBaseStar") == 0) {
		unk14C = 1.2f;
	} else if (strcmp(getUnkF4(), "SandBombBaseTurtle") == 0) {
		unk14C = 1.2f;
	}
	SMS_LoadParticle("/scene/mapObj/SandBomb.jpa", 0x55);
}

TSandBombBase::TSandBombBase(const char* name)
    : TSandBase(name)
    , unk148(0)
    , unk14C(1.0f)
    , unk150(0.0f)
    , unk154(0.0f)
{
}

//
// TSandCastle
//

bool TSandCastle::withering()
{
	f32 frame = getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(frame + unk13C);
	frame = getMActor()->getFrameCtrl(ANM_TYPE_BRK)->getFrame();
	getMActor()->getFrameCtrl(ANM_TYPE_BRK)->setFrame(frame + unk13C);
	frame = getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	// The ROM converts the frame's end through the u32->f32 magic pair, i.e.
	// the s16 cast is spelled out.
	f32 end   = (f32)(s16)getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getEnd();
	mScaling.y = mCollisionRate * (end - 4503599627370496.0f - frame)
	              / (end - 4503599627370496.0f);
	if (frame < 240.0f) {
		// TODO: bit 31 of mLiveFlag has no name in LiveActor.hpp yet.
		if (!unk158->checkLiveFlag(0x80000000)) {
			unk158->kill();
			gpTargetArrow->unk14 = 0;
		}
	}
	// the ROM materialises this comparison into a bool
	if (animIsFinished()) {
		sleep();
		return true;
	}
	return false;
}

void TSandCastle::expanded()
{
	f32 frame = unk144->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	unk144->getMActor()->getFrameCtrl(ANM_TYPE_BCK)
	    ->setFrame(frame + unk150);
	SMSGetMSound()->startSoundActor(0x20C6, &mPosition, 0, nullptr, 0, 4);
	// written out longhand: the ROM runs this pair twice
	if (animIsFinished())
		mState = 2;
	if (animIsFinished()) {
		mState = 2;
		startControlAnim(2);
		startControlAnim(3);
	}
}

void TSandCastle::explode()
{
	startControlAnim(1);
	mScaling.y             = 1.0f;
	mMapCollisionManager->changeCollision(1);
	TMapCollisionBase* model = mMapCollisionManager->getUnk8();
	model->setUp();
	if (model->getUnkC())
		model->moveSRT(mPosition, mScaling, mRotation);
	JPABaseEmitter* emitter =
	    gpMarioParticleManager->emit(0x55, &mPosition, 0, nullptr);
	emitter->mGlobalDynamicsScale.set(unk14C, unk14C, unk14C);
	emitter->mGlobalParticleScale.set(unk14C, unk14C, unk14C);

	s32 mode = 0;
	if (gpMarDirector->mMap == 3 || gpMarDirector->mMap == 4)
		mode = 1;
	if (mode) {
		gpCameraShake->startShake(CAM_SHAKE_MODE_UNK13, 1.0f);
	}
	SMSGetMSound()->startSoundActor(0x28A4, &mPosition, 0, nullptr, 0, 4);
	SMSRumbleMgr->start(0x15, mExlodingRumbleTime, &mPosition);
	mState = 7;
	awake();
	unk158->appear();
	startControlAnim(3);
}

// TODO: the ROM materialises the state test into a bool before branching, so
// this has to go through the isState() inline, not a plain compare.
static s32 SandCastleCallBack(u32 unused1, u32 unused2)
{
	if (unused2 == 1) {
		gpTargetArrow->unk14 = 1;
		JGeometry::TVec3<f32> pos(8400.0f, 300.0f, 8150.0f);
		gpTargetArrow->setPos(pos);
		// TODO: the real call hands the camera's look-at vector to the second
		// parameter, which the current declaration of warpPosAndAt() cannot
		// express.
		gpCamera->warpPosAndAt(
		    matan(gpCamera->unk124.x - gpCamera->unk148.x,
		          gpCamera->unk124.z - gpCamera->unk148.z),
		    0);
	}
	return 1;
}

void TSandCastle::waitBeforeExplode()
{
	JDrama::TFlagT<u16> flag = 0;
	mState                  = 6;
	gpMarDirector->fireStartDemoCamera("mamma1_sandcastle", nullptr, -1, 0.0f,
	                                   true, SandCastleCallBack, 0, nullptr,
	                                   flag);
	mStateTimer = unk148;
	unk15C      = 1;
}

void TSandCastle::calcRootMatrix()
{
	// The ROM materialises the state test into a bool before branching, so
	// this has to go through the isState() inline, not a plain compare.
	if (isState(STATE_SHRINKING))
		return;
	TMapObjBase::calcRootMatrix();
}

TMapObjBase* TSandCastle::findTriggerActor()
{
	// The ROM computes the 16-bit key from the name with
	// JDrama::TNameRef::calcKeyCode() and passes it as searchF's first
	// argument; the name pointer is reused for the second argument.
	return static_cast<TMapObjBase*>(
	    JDrama::TNameRefGen::getInstance()->getRootNameRef()->searchF(
	        JDrama::TNameRef::calcKeyCode("砂の城爆発の芽"), "砂の城爆発の芽"));
}

void TSandCastle::loadAfter()
{
	unk144         = static_cast<TSandBomb*>(findTriggerActor());
	unk144->unk138 = this;
	unk144->appear();
	unk158 = static_cast<TMapObjBase*>(
	    JDrama::TNameRefGen::getInstance()->getRootNameRef()->searchF(
	        JDrama::TNameRef::calcKeyCode("ステージ切替（砂の城）"),
	        "ステージ切替（砂の城）"));
	unk158->makeObjDead();
}

void TSandCastle::initMapObj()
{
	TSandBombBase::initMapObj();
	unk13C = 0.11f;
	unk148 = 120;
	sleep();
}

TSandCastle::TSandCastle(const char* name)
    : TSandBombBase(name)
    , unk158(nullptr)
    , unk15C(0)
{
}

//
// TLeanMirror
//

// UNUSED in the ROM (0x1C bytes); the body is a guess.
void TLeanMirror::enemyIsOn() const
{
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK1))
		return;
}

void TLeanMirror::draw() const
{
	MtxPtr mtx = getModel()->getAnmMtx(0);
	JGeometry::TVec3<f32> pos(mtx[0][1], mtx[1][1], mtx[2][1]);
	pos *= 350.0f * 0.001f * mBodyRadius;
	pos += mPosition;

	JGeometry::TVec3<f32> at(mtx[0][1], mtx[1][1], mtx[2][1]);
	at *= 10000.0f;
	at += mPosition;

	gpBeamManager->requestCone(pos, at, 1.7f * mBodyRadius, true, true, false);
}

// UNUSED in the ROM (0xAC bytes).
void TLeanMirror::updateSpeedVec(const JGeometry::TVec3<f32>& vec, f32 rate)
{
	unk14C += vec * rate;
}

BOOL TLeanMirror::receiveMessage(THitActor* sender, u32 message)
{
	// The ROM is a plain if/else-if chain, not a switch, and the lean update
	// is written out once per branch (there is no UNUSED inline for it in the
	// map), so all three copies are kept.
	if (message == 0) {
		sendMsg(0x100016, 0x100016);
		f32 rate = unk164;
		MtxPtr mtx = getModel()->getAnmMtx(0);
		unk14C.x += rate * ((sender->mPosition.x - mPosition.x)
		                       / fabsf(unk138 * mtx[0][0]) - mtx[0][1]);
		unk14C.z += rate * ((sender->mPosition.z - mPosition.z)
		                       / fabsf(unk138 * mtx[2][2]) - mtx[2][1]);
	} else if (message == 1) {
		sendMsg(0x100016, 0x100016);
		f32 rate = unk168;
		MtxPtr mtx = getModel()->getAnmMtx(0);
		unk14C.x += rate * ((sender->mPosition.x - mPosition.x)
		                       / fabsf(unk138 * mtx[0][0]) - mtx[0][1]);
		unk14C.z += rate * ((sender->mPosition.z - mPosition.z)
		                       / fabsf(unk138 * mtx[2][2]) - mtx[2][1]);
	} else if (message == 3) {
		f32 rate = unk170;
		MtxPtr mtx = getModel()->getAnmMtx(0);
		unk14C.x += rate * ((sender->mPosition.x - mPosition.x)
		                       / fabsf(unk138 * mtx[0][0]) - mtx[0][1]);
		unk14C.z += rate * ((sender->mPosition.z - mPosition.z)
		                       / fabsf(unk138 * mtx[2][2]) - mtx[2][1]);
	} else if (message == 8) {
		unk19C--;
		if (unk19C == 0)
			release();
		return TRUE;
	} else {
		return FALSE;
	}
	return TRUE;
}

void TLeanMirror::touchPlayer(THitActor* hit_actor)
{
	if (isState(STATE_IDLE) && marioIsOn()) {
		MtxPtr mtx = getModel()->getAnmMtx(0);
		f32 dx     = fabsf(unk138 * mtx[0][0]);
		f32 dz     = fabsf(unk138 * mtx[2][2]);
		unk14C.x += unk160
		            * ((hit_actor->mPosition.x - mPosition.x) / dx - mtx[0][1]);
		unk14C.z += unk160
		            * ((hit_actor->mPosition.z - mPosition.z) / dz - mtx[2][1]);
		if (!unk1AC) {
			MSBgm::startBGM(0x80010011);
			MSBgm::setTrackVolume(0, 0.0f, 10, 0);
			unk1AC = 1;
		}
	}
}

void TLeanMirror::touchEnemy(THitActor* hit_actor)
{
	// TODO: the ROM also tests a byte at 0x1B0 of the touched actor, which
	// belongs to a base class this TU does not see; the flag name is unknown.
	if (hit_actor->mActorType - 0x1000 <= 0x16 ? true : false) {
		MtxPtr mtx = getModel()->getAnmMtx(0);
		f32 dx     = fabsf(unk138 * mtx[0][0]);
		f32 dz     = fabsf(unk138 * mtx[2][2]);
		unk14C.x += unk16C * ((hit_actor->mPosition.x - mPosition.x) / dx
		                      - mtx[0][1]);
		unk14C.z += unk16C * ((hit_actor->mPosition.z - mPosition.z) / dz
		                      - mtx[2][1]);
	}
}

void TLeanMirror::release()
{
	MtxPtr mtx = getModel()->getAnmMtx(0);
	f32 wx     = mtx[0][1];
	f32 wy     = mtx[1][1];
	f32 wz     = mtx[2][1];
	unk18C.x   = wy * unk180.z - wz * unk180.y;
	unk18C.y   = wz * unk180.x - wx * unk180.z;
	unk18C.z   = wx * unk180.y - wy * unk180.x;
	f32 len    = orig_sqrt(unk18C.x * unk18C.x
	                                         + unk18C.y * unk18C.y
	                                         + unk18C.z * unk18C.z);
	// The ROM builds the divisor out of a u32->f32 magic pair, so the s16
	// cast has to be spelled out.
	unk198     = fabsf(atan2f(
                len, wy * unk180.y + wx * unk180.x + wz * unk180.z))
	           / (f32)(s16)(s32)(-mGoTargetTime);
	mStateTimer = mGoTargetTime;
	mState      = 2;
	offMapObjFlag(MAP_OBJ_FLAG_ENABLE_WALL_COLLISION);
	SMS_MarioMoveRequest(unk1A0);

	if (strcmp(getUnkF4(), "mirrorS") == 0) {
		JDrama::TFlagT<u16> flag = 0;
		gpMarDirector->fireStartDemoCamera(
		    "ぐらぐら鏡Ｓカメラ", nullptr, mGoTargetTime + mDemoWaitTime, 0.0f,
		    true, nullptr, 0, nullptr, flag);
	} else if (strcmp(getUnkF4(), "mirrorM") == 0) {
		JDrama::TFlagT<u16> flag = 0;
		gpMarDirector->fireStartDemoCamera(
		    "ぐらぐら鏡Ｍカメラ", nullptr, mGoTargetTime + mDemoWaitTime, 0.0f,
		    true, nullptr, 0, nullptr, flag);
	} else if (strcmp(getUnkF4(), "mirrorL") == 0) {
		JDrama::TFlagT<u16> flag = 0;
		gpMarDirector->fireStartDemoCamera(
		    "ぐらぐら鏡Ｌカメラ", nullptr, mGoTargetTime + mDemoWaitTime, 0.0f,
		    true, nullptr, 0, nullptr, flag);
	}
	MSBgm::stopTrackBGM(1, 10);
}

// TODO: the demo-camera shake callback body is not reconstructed; the map
// records it at 8 bytes, i.e. a bare "return 0".
static s32 startCameraShakeSE(u32 unused1, u32 unused2)
{
	return 0;
}

void TLeanMirror::controlGoTarget()
{
	MtxPtr model = getModel()->getAnmMtx(0);
	JGeometry::SMatrix34C<f32> mtx;
	mtx.set(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
	        0.0f);
	makeMtxRotByAxis(unk18C, unk198, mtx);
	concatOnlyRotFromLeft(mtx, model, model);

	if (mStateTimer <= 0) {
		unk17C->putOnLight(this);
		if (unk17C->mWhite) {
			TSleepBossHanachan* boss = static_cast<TSleepBossHanachan*>(
			    JDrama::TNameRefGen::getInstance()->getRootNameRef()->searchF(
			        JDrama::TNameRef::calcKeyCode("居眠りボスハナチャン"),
			        "居眠りボスハナチャン"));
			if (boss)
				boss->startFall(unk17C->mPosition.x,
				                unk17C->mPosition.y + 1100.0f,
				                unk17C->mPosition.z);
			JDrama::TFlagT<u16> flag = 0;
			gpMarDirector->fireStartDemoCamera(
			    "太陽石点灯カメラ", nullptr, -1, 0.0f, true,
			    startCameraShakeSE, reinterpret_cast<uintptr_t>(&mPosition),
			    nullptr, flag);
		} else {
			JDrama::TFlagT<u16> flag = 0;
			gpMarDirector->fireStartDemoCamera(
			    "太陽石点灯カメラ", &unk17C->mPosition, mDemoLightTime, 0.0f,
			    true, nullptr, 0, nullptr, flag);
		}
		mStateTimer = mDemoLightTime + 0x78;
		mState      = 3;
	}
}

void TLeanMirror::controlShake()
{
	// The ROM materialises the first test into a bool, so it has to be spelled
	// out; the rest of the chain tests the raw flags.
	if ((unk19C != 0 ? true : false) && unk1AC && SMS_IsMarioTouchGround4cm()
	    && SMS_GetMarioGrPlane() != reinterpret_cast<const TBGCheckData*>(this)) {
		MSBgm::stopTrackBGM(1, 10);
		MSBgm::setTrackVolume(0, 1.0f, 10, 0);
		unk1AC = 0;
	}

	f32 len2 = unk14C.x * unk14C.x + unk14C.y * unk14C.y + unk14C.z * unk14C.z;
	if (!(len2 <= JGeometry::TUtil<f32>::epsilon())) {
		unk14C *= unk15C;
		JGeometry::TVec3<f32> axis(unk14C.x, 0.0f, unk14C.z);
		rotateVecByAxisY(&axis, JGeometry::TUtil<f32>::halfPI());
		MtxPtr model = getModel()->getAnmMtx(0);
		// only the horizontal part of the lean decides the shake angle
		f32 len = unk14C.x * unk14C.x + unk14C.z * unk14C.z;
		if (len > 0.0f)
			len = static_cast<f32>(sqrt(static_cast<double>(len)));
		JGeometry::SMatrix34C<f32> mtx;
		mtx.set(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		        1.0f, 0.0f);
		makeMtxRotByAxis(axis, len * unk158, mtx);
		concatOnlyRotFromLeft(mtx, model, model);

		if (model[1][1] < unk174) {
			MtxPtr m2 = getModel()->getAnmMtx(0);
			f32 d     = m2[2][1] * unk14C.z + unk14C.x * m2[0][1];
			if (d > 0.0f) {
				f32 speed = JGeometry::TUtil<f32>::sqrt(
				    unk14C.x * unk14C.x + unk14C.y * unk14C.y
				    + unk14C.z * unk14C.z);
				SMSGetMSound()->startSoundActorWithInfo(0x3849, nullptr, &mPosition,
				                                        fabsf(speed), 0, 0, nullptr,
				                                        0, 4);
			}
			unk14C *= -unk178;
			MtxPtr src = getModel()->getAnmMtx(0);
			MtxPtr dst = getModel()->getAnmMtx(0);
			PSMTXCopy(dst, src);
		} else {
			MtxPtr src = getModel()->getAnmMtx(0);
			MtxPtr dst = getModel()->getAnmMtx(0);
			PSMTXCopy(src, dst);
		}
	}
}

void TLeanMirror::control()
{
	TMapObjBase::control();
	switch (mState) {
	case 1:
		controlShake();
		SMSGetMSound()->startSoundActorWithInfo(
		    0x3048, nullptr, &mPosition,
		    fabsf(JGeometry::TUtil<f32>::sqrt(unk14C.x * unk14C.x
		                                      + unk14C.y * unk14C.y
		                                      + unk14C.z * unk14C.z)),
		    0, 0, nullptr, 0, 4);
		break;
	case 2:
		controlGoTarget();
		SMSGetMSound()->startSoundActorWithInfo(
		    0x304A, nullptr, &mPosition,
		    fabsf(JGeometry::TUtil<f32>::sqrt(unk14C.x * unk14C.x
		                                      + unk14C.y * unk14C.y
		                                      + unk14C.z * unk14C.z)),
		    0, 0, nullptr, 0, 4);
		break;
	case 3:
		if (mStateTimer <= 0) {
			if (unk17C->mLightCount < 3) {
				MSBgm::setTrackVolume(0, 1.0f, 10, 0);
				if (unk17C->mLightCount == 1)
					gpMarDirector->getConsole()->startAppearBalloon(0x32,
					                                                   true);
				else
					gpMarDirector->getConsole()->startAppearBalloon(0x33,
					                                                   true);
			}
			if (unk17C->unk7C > 0.0f)
				unk17C->mEmitter->mChildSpawnRate = unk17C->unk7C;
			mState = 4;
		}
		break;
	}
}

void TLeanMirror::loadAfter()
{
	TMapObjBase::loadAfter();
	// The ROM computes the 16-bit key from the name with
	// JDrama::TNameRef::calcKeyCode() and passes it as searchF's first
	// argument; the name pointer is reused for the second argument.
	unk17C = static_cast<TShiningStone*>(
	    JDrama::TNameRefGen::getInstance()->getRootNameRef()->searchF(
	        JDrama::TNameRef::calcKeyCode("ShiningStone"), "ShiningStone"));
	unk180 = unk17C->mPosition - mPosition;
	f32 len2 = unk180.x * unk180.x + unk180.y * unk180.y + unk180.z * unk180.z;
	if (len2 <= JGeometry::TUtil<f32>::epsilon()) {
		unk180.zero();
	} else {
		// TODO: the ROM calls the out-of-line JGeometry copy of inv_sqrt()
		// here; the header's inline gets expanded instead.
		unk180 *= 1.0f * orig_inv_sqrt(len2);
	}
}

u32 TLeanMirror::getSDLModelFlag() const
{
	return 0;
}

void TLeanMirror::initMapObj()
{
	TMapObjBase::initMapObj();
	unk158 = 0.03f;
	unk15C = 0.999f;
	unk160 = 0.0001f;
	unk168 = 1.0f;
	unk16C = 0.0002f;
	unk170 = 0.0001f;
	unk174 = 0.865f;
	unk178 = 0.5f;
	if (strcmp(getUnkF4(), "mirrorS") == 0) {
		unk164 = 0.002f;
		unk168 = 1.0f;
		unk174 = 0.87f;
		unk19C = 1;
	} else if (strcmp(getUnkF4(), "mirrorM") == 0) {
		unk164 = 0.004f;
		unk19C = 2;
	} else {
		unk164 = 0.006f;
		unk19C = 3;
	}
}

void TLeanMirror::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	f32 value;
	stream.read(&value, sizeof(value));
	unk138 = value * 100.0f * 0.5f;
	unk13C = unk138;
	char name[0x40];
	char file[0x40];
	if (gpMarDirector->unk7D == 1) {
		stream.readString(name, sizeof(name));
		stream.read(&unk1A0, sizeof(unk1A0.x));
		stream.read(&unk1A0.y, sizeof(unk1A0.y));
		stream.read(&unk1A0.z, sizeof(unk1A0.z));
	}
	// TODO: TMirrorModelObj is newsed and then has its model pointer
	// overwritten; the derived type's own constructor body is not recovered.
	TMirrorModelObj* mirror = new TMirrorModelObj();
	mirror->unk28 = nullptr;
	snprintf(file, sizeof(file), "/scene/mapObj/%sTop.bmd", getUnkF4());
	mirror->init(file);
	mirror->unk28 = getModel();
	if (gpMarDirector->unk7D != 1)
		mState = 4;
	// framePadLoad: the ROM's frame is 0x30 larger than the locals above
	// account for (value at 0xC0, two 0x40 name buffers at 0x80 and 0x40,
	// and 0x40 bytes of unused stack below that).  The extra 0x30 has not
	// been identified; without it every stack offset in the function is
	// shifted and the function is ~11% off.
	char framePadLoad[0x30];
}

TLeanMirror::TLeanMirror(const char* name)
    : TMapObjBase(name)
    , unk138(0.0f)
    , unk13C(0.0f)
    , unk158(0.0f)
    , unk15C(0.0f)
    , unk160(0.0f)
    , unk164(0.0f)
    , unk168(0.0f)
    , unk16C(0.0f)
    , unk170(0.0f)
    , unk17C(nullptr)
    , unk198(0.0f)
    , unk19C(0)
    , unk1AC(0)
    , unk1AE(0)
{
	unk140.zero();
	unk14C.zero();
	unk180.zero();
	unk18C.zero();
	unk1A0.zero();
}

//
// TShiningStone
//

// UNUSED in the ROM (0x98 bytes).
void TShiningStone::endDemo()
{
	mWhite = 0;
}

void TShiningStone::putOnLight(TLiveActor* live_actor)
{
	if (strcmp(live_actor->getName(), "mirrorS") == 0) {
		mMirror[0]->setBck("shiningstonegreen");
		mMirror[0]->setBrk("shiningstonegreen");
		mGreen = 1;
	} else if (strcmp(live_actor->getName(), "mirrorM") == 0) {
		mMirror[1]->setBck("shiningstoneblue");
		mMirror[1]->setBrk("shiningstoneblue");
		mBlue = 1;
	} else if (strcmp(live_actor->getName(), "mirrorL") == 0) {
		mMirror[2]->setBck("shiningstonered");
		mMirror[2]->setBrk("shiningstonered");
		mRed = 1;
	}

	switch (mLightCount) {
	case 0:
		mEmitter = gpMarioParticleManager->emit(0x143, &mPosition, 1, this);
		mEmitter->mChildSpawnRate = 3.0f;
		unk7C                    = 1.5f;
		SMSGetMSound()->startSoundActor(0x2893, &mPosition, 0, nullptr, 0, 4);
		break;
	case 1:
		mEmitter = gpMarioParticleManager->emit(0x144, &mPosition, 1, this);
		mEmitter->mChildSpawnRate = 0.4f;
		unk7C                    = 0.2f;
		SMSGetMSound()->startSoundActor(0x2894, &mPosition, 0, nullptr, 0, 4);
		break;
	case 2:
		mEmitter = gpMarioParticleManager->emit(0x145, &mPosition, 1, this);
		unk7C    = 0.0f;
		SMSGetMSound()->startSoundActor(0x2895, &mPosition, 0, nullptr, 0, 4);
		break;
	}

	gpMarioParticleManager->emit(0x56, &mPosition, 0, nullptr);
	mLightCount++;
	if (mLightCount == 3) {
		mMirror[3]->setBck("shiningstonewhite");
		mMirror[3]->setBrk("shiningstonewhite");
		mWhite = 1;
	}
}

void TShiningStone::perform(u32 cue, JDrama::TGraphics* graphics)
{
	for (int i = 0; i < 4; i++) {
		mMirror[i]->perform(cue, graphics);
		if (mLightCount > 0)
			gpMarioParticleManager->emit(0x143, &mPosition, 1, this);
		if (mLightCount > 1)
			gpMarioParticleManager->emit(0x144, &mPosition, 1, this);
		if (mLightCount > 2)
			gpMarioParticleManager->emit(0x145, &mPosition, 1, this);
	}
	mStone->perform(cue, graphics);
}

void TShiningStone::load(JSUMemoryInputStream& stream)
{
	TActor::load(stream);
	// The ROM keeps these four names in a .rodata pointer table and copies
	// the whole 16 bytes into a stack array (which it then overlaps with the
	// unused tail row of `mtx`) before walking it with an index.
	const char* names[4] = {
		"太陽石in鏡",
		"/scene/mapObj/ShiningStone.bmd",
		"shiningstone",
		"/scene/mapObj/ShiningStone1.jpa"
	};
	f32 scale = 182.04444885253906f;
	Mtx mtx;
	// Only the rotation arguments are scaled: the ROM passes the raw
	// mPosition as x/y/z and mRotation*scale as the three short angles.
	MsMtxSetXYZRPH(mtx, mPosition.x, mPosition.y, mPosition.z,
	               mRotation.x * scale, mRotation.y * scale,
	               mRotation.z * scale);
	mMirror = new MActor*[4];
	for (int i = 0; i < 4; i++) {
		mMirror[i] = SMS_MakeMActorWithAnmData(
		    names[i], gpMapObjManager->getMActorAnmData(), 3, 0x1002);
		// the ROM passes &model->unk20 (0x20), not getAnmMtx(0) (0x58)
		PSMTXCopy(mtx, mMirror[i]->getModel()->getBaseTRMtx());
		// TODO: the mirror actor is constructed with the same "太陽石in鏡"
		// name in every iteration.
		TMirrorActor* mirror_actor = new TMirrorActor("太陽石in鏡");
		mirror_actor->init(mMirror[i]->getModel(), 0x1A);
	}
	mStone = SMS_MakeMActorWithAnmData(
	    "/scene/mapObj/ShiningStone.bmd", gpMapObjManager->getMActorAnmData(),
	    3, 0x1002);
	mStone->setBpk("shiningstone");
	mStone->setBtk("shiningstone");
	PSMTXCopy(mtx, mStone->getModel()->getBaseTRMtx());
	SMS_LoadParticle("/scene/mapObj/ShiningStone1.jpa", 0x143);
	SMS_LoadParticle("/scene/mapObj/ShiningStone2.jpa", 0x144);
	SMS_LoadParticle("/scene/mapObj/ShiningStone3.jpa", 0x145);
	SMS_LoadParticle("/scene/mapObj/ShiningStoneF.jpa", 0x56);
}

TShiningStone::TShiningStone(const char* name)
    : THitActor(name)
    , mLightCount(0)
    , mEmitter(nullptr)
    , unk7C(0.0f)
{
	mGreen = 0;
	mBlue  = 0;
	mRed   = 0;
	mWhite = 0;
}

//
// TMammaBlockRotate
//

u32 TMammaBlockRotate::touchWater(THitActor* hit_actor)
{
	// the ROM drives the tilt through mRotation.y (0x34), not mRotation.x
	if (isState(STATE_ROTATING)) {
		mRotation.y += mRotSpeed;
		if (mRotation.y > mRotEnd)
			mState = STATE_GO;
	}
	return 1;
}

void TMammaBlockRotate::control()
{
	TMapObjBase::control();
	switch (mState) {
	case STATE_ROTATING: {
		// the ROM drives the tilt through mRotation.y (0x34), not mRotation.x
		f32 rot = mRotation.y;
		if (rot > 0.0f)
			mRotation.y = rot - mRotReturnSpeed;
		else
			mRotation.y = 0.0f;
	} break;
	case STATE_GO:
		moveJoint(unk140->getJoint(), 0.0f, -mMapGoSpeed, 0.0f);
		moveJoint(unk13C->getJoint(), 0.0f, -mMapGoSpeed, 0.0f);
		reinterpret_cast<J3DNode*>(unk138->getModel())->entryIn();
		{
			JGeometry::TVec3<f32> vec(0.0f, unk140->getJoint()->getTransformInfo().mTranslate.y,
			                          0.0f);
			unk144->setUp();
			unk144->moveTrans(vec);
			vec.set(0.0f, unk140->getJoint()->getTransformInfo().mTranslate.y, 0.0f);
			unk148->setUp();
			unk148->moveTrans(vec);
			if (unk140->getJoint()->getTransformInfo().mTranslate.y < 0.0f) {
				mStateTimer = mWaitTime;
				mState      = STATE_WAIT;
			}
		}
		break;
	case STATE_WAIT:
		if (mStateTimer <= 0)
			mState = STATE_BACK;
		break;
	case STATE_BACK:
		moveJoint(unk140->getJoint(), 0.0f, mMapBackSpeed, 0.0f);
		moveJoint(unk13C->getJoint(), 0.0f, mMapBackSpeed, 0.0f);
		{
			JGeometry::TVec3<f32> vec(0.0f, unk140->getJoint()->getMin().x, 0.0f);
			unk144->setUp();
			unk144->moveTrans(vec);
			vec.set(0.0f, unk140->getJoint()->getMax().x, 0.0f);
			unk148->setUp();
			unk148->moveTrans(vec);
		}
		reinterpret_cast<J3DNode*>(unk138->getModel())->entryIn();
		{
			J3DJoint* joint = unk140->getJoint();
			if (joint->getTransformInfo().mTranslate.y < joint->getMax().y - joint->getMin().y)
				mState = STATE_ROTATING;
		}
		break;
	}
}

void TMammaBlockRotate::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = gpMap->getModelManager()->getJointModel(0);
	unk13C = unk138->getChild(0)->getChild(0)->getChild(1);
	f32 height
	    = unk13C->getJoint()->getMax().y - unk13C->getJoint()->getMin().y;
	moveJoint(unk13C->getJoint(), 0.0f, height, 0.0f);
	{
		JGeometry::TVec3<f32> vec(0.0f, height, 0.0f);
		unk144->setUp();
		unk144->moveTrans(vec);
	}
	unk140 = unk138->getChild(0)->getChild(0)->getChild(2);
	height  = unk140->getJoint()->getMax().y - unk140->getJoint()->getMin().y;
	moveJoint(unk140->getJoint(), 0.0f, height, 0.0f);
	{
		JGeometry::TVec3<f32> vec(0.0f, height, 0.0f);
		unk148->setUp();
		unk148->moveTrans(vec);
	}
	// TODO: J3DModel should derive from J3DNode (see
		// libs/JSystem/include/.../J3DModel.hpp); the cast is what lets the
		// vtable slot through.
		reinterpret_cast<J3DNode*>(unk138->getModel())->entryIn();
}

void TMammaBlockRotate::load(JSUMemoryInputStream& stream)
{
	unk144 = new TMapCollisionMove();
	unk144->init("/scene/mapObj/MammaBlockDown.col", 0, this);
	unk148 = new TMapCollisionMove();
	unk148->init("/scene/mapObj/MammaBlockUp.col", 0, this);
	TMapObjBase::load(stream);
}

TMammaBlockRotate::TMammaBlockRotate(const char* name)
    : TMapObjBase(name)
    , unk138(nullptr)
    , unk13C(nullptr)
    , unk140(nullptr)
    , unk144(nullptr)
    , unk148(nullptr)
{
}

//
// TMammaYacht
//

void TMammaYacht::control()
{
	TMapObjBase::control();
	// TODO: the enum values are decoded from a single (x - 0x102) <= 3 test
	// plus two equality tests; the individual names are guesses.
	u16 ground = *reinterpret_cast<const u16*>(mGroundPlane);
	if (ground == 0x100 || ground == 0x101 || (u16)(ground - 0x102) <= 3
	    || ground == 0x4104) {
		f32 height = gpMapObjWave->getWaveHeight(mPosition.x, mPosition.z);
		mPosition.y = mInitialPosition.y + height;
		unk138->mPosition.y = mPosition.y - 50.0f;
	}
}

void TMammaYacht::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = new TMapObjFlag("旗");
	// TODO: these three vectors are written with the stfsu form in the ROM,
	// i.e. a three-argument set() on a freshly allocated block.
	JGeometry::TVec3<f32>* p
	    = reinterpret_cast<JGeometry::TVec3<f32>*>(unk138);
	p->x = mPosition.x + 2.0f;
	p->y = mPosition.y + 1315.0f - 190.0f;
	p->z = mPosition.z - 15.0f;
	p++;
	p->x = 0.0f;
	p->y = 180.0f;
	p->z = 0.0f;
	p++;
	p->x = 1.0f;
	p->y = 2.5f;
	p->z = 3.8f;
	unk138->init("MammaYacht00");
}

//
// TSandBird
//

void TSandBird::control()
{
	TJointCoin::control();
	SMSGetMSound()->startSoundActor(0x217C, &mPosition, 0, nullptr, 0, 4);
	SMSGetMSound()->startSoundSystemSE(0x217D, 0, nullptr, 0);

	for (int i = 0; i < unk13C; i++) {
		THitActor* obj = unk140[i];
		// the ROM materialises both range tests into bools, so the ternary
		// form has to be spelled out
		if ((obj->mActorType - 0x2000 <= 0xE ? true : false)
		    || (obj->mActorType - 0x4000 <= 0x23 ? true : false)) {
			gpMarioParticleManager->emitAndBindToPosPtr(
			    0x159, &obj->mPosition, 1, nullptr);
			gpMarioParticleManager->emitAndBindToPosPtr(
			    0x15A, &obj->mPosition, 1, nullptr);
		}
	}

	// TODO: the ROM inlines a camera test that reads the current demo camera's
	// mode; the accessor below is the closest equivalent found.
	if (gpCamera->isSimpleDemoCamera() && gpCamera->mMode == 0x49) {
		if (!unk150) {
			const TBGCheckData* plane = *gpMarioGroundPlane;
			if (plane && (u32)(*(reinterpret_cast<const u32*>(plane) + 0x4C / 4) - 0x4000)
			    <= 0x2C9) {
				gpMarDirector->getConsole()->startAppearBalloon(0x2C, false);
				mStateTimer = 0x960;
				unk150      = 1;
			}
		}
	}
	if (!unk151 && unk150) {
		if (mStateTimer <= 0) {
			gpMarDirector->getConsole()->startDisappearBalloon(0x2C, false);
			unk151 = 1;
		}
	}
}

TMapObjBase* TSandBird::makeObjFromJointName(const char* name, u16 index)
{
	if (TMapObjBase* obj = TJointCoin::makeObjFromJointName(name, index))
		return obj;
	if (strstr(name, "none") == nullptr)
		return makeObj("SandBirdBlock", index);
	return nullptr;
}

bool TSandBird::nameIsObj(const char* name)
{
	return strstr(name, "none") == nullptr ? true : false;
}

void TSandBird::initMapObj()
{
	TJointCoin::initMapObj();
	SMS_LoadParticle("/scene/map/map/ms_sunadori_a.jpa", 0x159);
	SMS_LoadParticle("/scene/map/map/ms_sunadori_b.jpa", 0x15A);
}

TSandBird::TSandBird(const char* name)
    : TJointCoin(name)
    , unk150(0)
    , unk151(0)
{
}

//
// TGoalWatermelon
//

u32 TWatermelonStatic::touchWater(THitActor* hit_actor)
{
	return 1;
}

void TGoalWatermelon::touchActor(THitActor* hit_actor)
{
	// TODO: the BCK/BRK/demo names used here are guesses -- the string table
	// around the "シャイン（お化けスイカ用）" literal in .rodata has not been
	// decoded yet, so only the surrounding code shape is trustworthy.
	// Both tests are nested ifs, not an && chain: the ROM materialises two
	// separate bools and tests them in sequence.
	if (isState(STATE_HIDDEN)) {
		if (hit_actor->mActorType - 0x4000 <= 0xD0 ? true : false) {
			unk13C = static_cast<TMapObjBase*>(hit_actor);
			unk13C->getMActor()->setBck("watermelon_shrink");
			unk13C->getMActor()->setBtk("watermelon_shrink");
			unk13C->offMapObjFlag(MAP_OBJ_FLAG_UNK8);
			unk13C->setVelocity(JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f));
			JDrama::TFlagT<u16> flag = 0;
			gpMarDirector->fireStartDemoCamera(
			    "スイカシャインカメラ", &unk13C->mPosition, -1, 0.0f, true, 0,
			    0, 0, flag);
			mState = 2;
		}
	}
}

//
// TGoalWatermelon
//

void TGoalWatermelon::control()
{
	TMapObjBase::control();
	switch (mState) {
	case 0:
	case 1:
		break;
	case 2:
		if (unk13C->animIsFinished()) {
			gpItemManager->makeShineAppearWithDemoOffset(
			    "シャイン（お化けスイカ用）", "スイカシャインカメラ", 0.0f,
			    0.0f, 0.0f);
			mState = 3;
		}
		break;
	default:
		break;
	}
}

void TGoalWatermelon::loadAfter()
{
	TMapObjBase::loadAfter();
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	unk138 = static_cast<TLiveActor*>(
	    JDrama::TNameRefGen::search("シャイン（お化けスイカ用）"));
	// TODO: the ROM interleaves the loads and the stores here (and reloads
	// the shine only once), which the plain TVec3::set() does not reproduce.
	unk138->mPosition.set(unk140, unk144, unk148);
	unk138->calcRootMatrix();
}

void TGoalWatermelon::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	char name[0x20];
	stream.readString(name, sizeof(name));
	stream.read(&unk140, sizeof(unk140));
	stream.read(&unk144, sizeof(unk144));
	stream.read(&unk148, sizeof(unk148));
}

TGoalWatermelon::TGoalWatermelon(const char* name)
    : TMapObjBase(name)
    , unk138(nullptr)
    , unk13C(0)
{
	unk148 = 0.0f;
	unk144 = 0.0f;
	unk140 = 0.0f;
}

//
// TMammaMirrorMapOperator
//

// UNUSED in the ROM (0x54 bytes).
void TMammaMirrorMapOperator::show(int param_1)
{
	unkC = 0;
}

// UNUSED in the ROM (0x54 bytes).
void TMammaMirrorMapOperator::hide(int param_1)
{
	unkC = 0;
}

void TMammaMirrorMapOperator::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & 2) {
		if (gpMirrorModelManager->isUnk18Present()) {
			int idx = gpMirrorModelManager->unk18;
			f32 dist = JGeometry::TUtil<f32>::sqrt(
			    (gpMirrorModelManager->unk24->unk98.x - unkB8[idx].x)
			            * (gpMirrorModelManager->unk24->unk98.x - unkB8[idx].x)
			    + (gpMirrorModelManager->unk24->unk98.y - unkB8[idx].y)
			          * (gpMirrorModelManager->unk24->unk98.y - unkB8[idx].y)
			    + (gpMirrorModelManager->unk24->unk98.z - unkB8[idx].z)
			          * (gpMirrorModelManager->unk24->unk98.z - unkB8[idx].z));
			for (int i = 0; i < 8; i++) {
				f32 d = JGeometry::TUtil<f32>::sqrt(
				    (gpMirrorModelManager->unk24->unk98.x - unk30[i].x)
				            * (gpMirrorModelManager->unk24->unk98.x - unk30[i].x)
				    + (gpMirrorModelManager->unk24->unk98.y - unk30[i].y)
				          * (gpMirrorModelManager->unk24->unk98.y - unk30[i].y)
				    + (gpMirrorModelManager->unk24->unk98.z - unk30[i].z)
				          * (gpMirrorModelManager->unk24->unk98.z - unk30[i].z));
				if (d > unk90[i] || d < dist) {
					if (unkB0[i]) {
						SMS_ShowJoint(
						    reinterpret_cast<J3DMaterial*>(
						    reinterpret_cast<char*>(unk10[i]) + 0x60), true);
						unkB0[i] = 0;
					}
				} else {
					if (!unkB0[i]) {
						SMS_ShowJoint(
						    reinterpret_cast<J3DMaterial*>(
						    reinterpret_cast<char*>(unk10[i]) + 0x60), false);
						unkB0[i] = 1;
					}
				}
			}
		} else {
			for (int i = 0; i < 8; i++) {
				if (!unkB0[i]) {
					SMS_ShowJoint(reinterpret_cast<J3DMaterial*>(
						    reinterpret_cast<char*>(unk10[i]) + 0x60),
					              false);
					unkB0[i] = 1;
				}
			}
		}
	}
}

void TMammaMirrorMapOperator::loadAfter()
{
	JDrama::TViewObj::loadAfter();
	// Each name is hashed with JDrama::TNameRef::calcKeyCode() first, and the
	// resulting 16-bit key becomes searchF's first argument while the name
	// pointer itself is reused for the second.
	// The ROM writes all three anchors out longhand (there is no loop and no
	// UNUSED inline for it), so the three reads are spelled out here too.
	JDrama::TActor* a0 = static_cast<JDrama::TActor*>(
	    JDrama::TNameRefGen::getInstance()->getRootNameRef()->searchF(
	        JDrama::TNameRef::calcKeyCode("mirrorS"), "mirrorS"));
	JDrama::TActor* a1 = static_cast<JDrama::TActor*>(
	    JDrama::TNameRefGen::getInstance()->getRootNameRef()->searchF(
	        JDrama::TNameRef::calcKeyCode("mirrorM"), "mirrorM"));
	JDrama::TActor* a2 = static_cast<JDrama::TActor*>(
	    JDrama::TNameRefGen::getInstance()->getRootNameRef()->searchF(
	        JDrama::TNameRef::calcKeyCode("mirrorL"), "mirrorL"));
	unkB8[0].set(a0->mPosition.x, a0->mPosition.y, a0->mPosition.z);
	unkB8[1].set(a1->mPosition.x, a1->mPosition.y, a1->mPosition.z);
	unkB8[2].set(a2->mPosition.x, a2->mPosition.y, a2->mPosition.z);

	// The four mirror shapes are read out of the "鏡内地形" static object.
	JDrama::TNameRef* ref
	    = JDrama::TNameRefGen::getInstance()->getRootNameRef()->searchF(
	        JDrama::TNameRef::calcKeyCode("鏡内地形"), "鏡内地形");
	J3DModelData* data = static_cast<TMapStaticObj*>(ref)->getModelData();
	TMirrorShapeNode* node
	    = *reinterpret_cast<TMirrorShapeNode**>(reinterpret_cast<u8*>(data)
	                                             + 0x20
	                                           + 0x8);
	for (int i = 0; i < 4; i++) {
		unk10[i] = node;
		unk30[i].set(0.5f * (node->mMax.x + node->mMin.x),
		             0.5f * (node->mMax.y + node->mMin.y),
		             0.5f * (node->mMax.z + node->mMin.z));
		// TODO: the ternary and the clamp are written out the way the ROM
		// orders them (store the picked value, then re-read and clamp it).
		if (0.5f * (node->mMax.x - node->mMin.x)
		    > 0.5f * (node->mMax.z - node->mMin.z))
			unk90[i] = 0.5f * (node->mMax.x - node->mMin.x);
		else
			unk90[i] = 0.5f * (node->mMax.z - node->mMin.z);
		unk90[i] = unk90[i] + 2000.0f;
		if (unk90[i] > 3000.0f)
			unk90[i] = 3000.0f;
		node = node->mNext;
	}
}

TMammaMirrorMapOperator::TMammaMirrorMapOperator(const char* name)
    : JDrama::TViewObj(name)
{
	for (int i = 0; i < 8; i++) {
		unk10[i] = nullptr;
		unk30[i].zero();
		unk90[i] = 0.0f;
		unkB0[i] = 0;
	}
	for (int i = 0; i < 3; i++) {
		unkB8[i].zero();
	}
}

//
// TSandEgg
//

u32 TSandEgg::getSDLModelFlag() const
{
	return 0;
}
