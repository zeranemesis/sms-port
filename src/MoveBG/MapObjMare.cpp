#include <MoveBG/MapObjMare.hpp>

// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MSound/MSound.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Map/MapEventMare.hpp>
#include <Map/MapData.hpp>
#include <Map/MapWireManager.hpp>
#include <MoveBG/ItemManager.hpp>
#include <Enemy/Cannon.hpp>
#include <System/MarDirector.hpp>
#include <MSound/SoundEffects.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// ---------------------------------------------------------------------------
// ORDER NOTE: this TU is compiled with `-inline deferred`, so the compiler
// emits the function bodies in REVERSE of the order they are written here.
// Every definition below is therefore laid out as the exact reverse of the
// `marioEU.MAP` .text layout for MoveBG.a(MapObjMare.cpp).
// ---------------------------------------------------------------------------

// File statics. The .sdata order below mirrors the map's, which is the
// declaration order of the original file.
f32 TCogwheelScale::mWaterLeakSpeed = 0.01f;
static f32 sRadius                = 800.0f;
f32 TCogwheel::mRopeWidthX        = 10.0f;
f32 TCogwheel::mRopeWidthZ        = 7.0f;
f32 TCogwheel::mTexPosRate        = 0.01f;
f32 TCogwheel::mMinSpeed          = 3.0f;
static f32 mGrowStartFrame        = 90.0f;
static f32 mGrowEndFrame          = 175.0f;

// ===========================================================================
// TCogwheelScale
// ===========================================================================

u32 TCogwheelScale::touchWater(THitActor* param_1)
{
	if (mAccel < mLimit)
		mAccel += 1.0f;
	return 1;
}

BOOL TCogwheelScale::receiveMessage(THitActor* sender, u32 message)
{
	// 0x1 is the "scale the cogwheel down" message the plates send each other.
	if (message == 1) {
		// fabricated: the original walks a TCogwheel pointer
		((TCogwheel*)mCogwheel)->mSpeed += mWaterLeakMul;
		return TRUE;
	}
	return TMapObjBase::receiveMessage(sender, message);
}

void TCogwheelScale::touchPlayer(THitActor* param_1)
{
	if (marioIsOn())
		mWaterLeakPos = mRotPos;

	// Only react when Mario is at least 150.0f below the top of the water.
	if (mPosition.y - mYOffset > 150.0f + gpMarioPos->y) {
		// The top plate reverses a spinning wheel, the bottom plate reverses
		// a wheel that is already turning the other way.
		if ((mCogwheelScaleIsTop && mCogwheel->mSpeed > 0.0f)
		    || (!mCogwheelScaleIsTop && mCogwheel->mSpeed < 0.0f)) {
			mCogwheel->mSpeed *= -mCogwheel->mReverseRate;
			if (fabs(mCogwheel->mSpeed) < TCogwheel::mMinSpeed)
				mCogwheel->mSpeed = 0.0f;
			if (marioHeadAttack())
				mCogwheel->mSpeed
				    = mCogwheel->mSpeed * *gpMarioSpeedY * mWaterLeakValue;
		}
	}
	mAccel = 0.0f;
}

void TCogwheelScale::control()
{
	// TODO: unconfirmed -- the sound id and the gate are not recovered yet.
	mWaterLeakPos = 0.0f;
	TMapObjBase::control();
	if (mAccel > 0.0f) {
		mAccel -= mWaterLeakSpeed;
		SMSGetMSound()->startSoundActorWithInfo(0x3061, &mPosition, nullptr,
		                                        fabs(mAccel), 0, 0, nullptr,
		                                        0, 4);
		if (mAccel < 0.0f)
			mAccel = 0.0f;
	}
}

TCogwheelScale::TCogwheelScale(const char* name)
	: TMapObjBase(name)
	, mRotSpeed(0.0f)
	, mRotPos(0.0f)
	, mAccel(0.0f)
	, mLimit(0.0f)
	, mWaterLeakPos(0.0f)
	, mWaterLeakValue(0.01f)
	, mWaterLeakMul(5.0f)
	, mCogwheelScaleIsTop(0)
	, mCogwheel(nullptr)
{
}

// ===========================================================================
// TCogwheel
// ===========================================================================

void TCogwheel::initDraw() const
{
	// TODO: unconfirmed
}

void TCogwheel::draw() const
{
	// TODO: unconfirmed
}

void TCogwheel::rebound()
{
	// TODO: unconfirmed
}

void TCogwheel::calc()
{
	mRotation.z = 360.0f * ((-mAngle) / (3.14f * (2.0f * sRadius)));
	Mtx mtxRotZ;
	Mtx mtxRotY;
	makeRootMtxRotZ((MtxPtr)mtxRotZ);
	// the 4th column is re-zeroed after each root matrix is built
	mtxRotZ[0][3] = 0.0f;
	mtxRotZ[1][3] = 0.0f;
	mtxRotZ[2][3] = 0.0f;
	makeRootMtxRotY((MtxPtr)mtxRotY);
	mtxRotY[0][3] = 0.0f;
	mtxRotY[1][3] = 0.0f;
	mtxRotY[2][3] = 0.0f;
	MtxPtr modelMtx = getModel()->getAnmMtx(0);
	PSMTXConcat((MtxPtr)mtxRotY, (MtxPtr)mtxRotZ, modelMtx);
	modelMtx[0][3] = mPosition.x;
	modelMtx[1][3] = mPosition.y;
	modelMtx[2][3] = mPosition.z;
}

void TCogwheel::control()
{
	TMapObjBase::control();
	mAngle += mSpeed;
	// The scale tips towards whichever side is heavier; both "weight" terms
	// are the plate's / pot's rotation, accel and leak position summed.
	mSpeed += mAcceleration
	          * ((mPlate->mRotSpeed + mPlate->mAccel + mPlate->mWaterLeakPos)
	             - (mPot->mRotSpeed + mPot->mAccel + mPot->mWaterLeakPos));
	mSpeed *= mFriction;
	if (mAngle < mAngleLimitHigh && mSpeed < 0.0f) {
		mSpeed *= -mReverseRate;
		if (fabs(mSpeed) < mMinSpeed) {
			mSpeed = 0.0f;
		}
	}
	if (mAngle > mRopeLength - mAngleLimitHigh && mSpeed > 0.0f) {
		mSpeed *= -mReverseRate;
		if (fabs(mSpeed) < mMinSpeed) {
			mSpeed = 0.0f;
		}
	}
	mPlate->mPosition.y = mPosition.y - mAngle + mPlate->mYOffset;
	mPot->mPosition.y = mPosition.y - (mRopeLength - mAngle);
	if (fabs(mSpeed) > 0.01f) {
		SMSGetMSound()->startSoundActorWithInfo(0x3060, &mPosition, nullptr,
		                                        10.0f * fabs(mSpeed), 0, 0,
		                                        nullptr, 0, 4);
	}
}

void TCogwheel::initMapObj()
{
	TMapObjBase::initMapObj();
	// rotate the boom offset into world space around Y by the initial tilt
	f32 rad   = 0.017453294f * mRotation.y;
	f32 cos_r = cosf(rad);
	f32 sin_r = sinf(rad);
	f32 dx = sRadius * cos_r - 0.0f * sin_r;
	f32 dz = sRadius * sin_r + 0.0f * cos_r;
	JGeometry::TVec3<f32> one(1.0f, 1.0f, 1.0f);

	JGeometry::TVec3<f32> plate_pos(mPosition.x + dx, mPosition.y,
	                                mPosition.z - dz);
	mPlate = static_cast<TCogwheelScale*>(TMapObjBaseManager::newAndRegisterObj(
		"cogwheel_plate", plate_pos, mRotation, one));
	mPlate->mCogwheelScaleIsTop = 1;
	mPlate->mCogwheel           = this;
	mPlate->appear();
	mPlatePos = plate_pos;

	JGeometry::TVec3<f32> pot_pos(mPosition.x - dx, mPosition.y,
	                              mPosition.z + dz);
	mPot = static_cast<TCogwheelScale*>(TMapObjBaseManager::newAndRegisterObj(
		"cogwheel_pot", pot_pos, mRotation, one));
	mPot->mCogwheelScaleIsTop = 0;
	mPot->mCogwheel           = this;
	mPot->appear();
	mPotPos = pot_pos;

	if (strcmp(getName(), "天秤上") == 0) {
		mAcceleration   = 0.003f;
		mFriction       = 0.99f;
		mReverseRate    = 0.8f;
		mRopeLength     = 3800.0f;
		mAngleLimitLow  = 1000.0f;
		mAngleLimitHigh = 1800.0f;
		mPot->mRotSpeed = 0.0f;
		mPot->mRotPos   = 0.0f;
		mPot->mLimit    = 14.0f;
		mPlate->mRotSpeed = 10.0f;
		mPlate->mRotPos   = 0.0f;
		mPlate->mLimit    = 0.0f;
	} else {
		mAcceleration   = 0.008f;
		mFriction       = 0.98f;
		mReverseRate    = 0.8f;
		mRopeLength     = 3950.0f;
		mAngleLimitLow  = 1000.0f;
		mAngleLimitHigh = 1900.0f;
		mPot->mRotSpeed = 0.0f;
		mPot->mRotPos   = 0.0f;
		mPot->mLimit    = 14.0f;
		mPlate->mRotSpeed = 10.0f;
		mPlate->mRotPos   = 0.0f;
		mPlate->mLimit    = 0.0f;
	}
	mAngle = mRopeLength * 0.5f;
}

TCogwheel::TCogwheel(const char* name)
	: TMapObjBase(name)
	, mSpeed(0.0f)
	, mAngle(0.0f)
	, mAcceleration(0.0f)
	, mFriction(0.0f)
	, mReverseRate(0.0f)
	, mRopeLength(0.0f)
	, mPlate(nullptr)
	, mPlatePos()
	, mAngleLimitLow(0.0f)
	, mPot(nullptr)
	, mPotPos()
	, mAngleLimitHigh(0.0f)
{
}

// ===========================================================================
// TMapObjElasticCode
// ===========================================================================

void TMapObjElasticCode::draw() const
{
	// TODO: unconfirmed
}

void TMapObjElasticCode::control()
{
	TMapObjBase::control();
	// 0xB0 is mVelocity.y, 0x110 is mInitialPosition.y, 0x6C mHeldObject.
	// vtable slot 58 is getGravityY, slot 69 is moveRequest.
	mVelocity.y *= mFriction;
	f32 delta = getGravityY() - (mInitialPosition.y - mPosition.y) * mSpringConst;
	mVelocity.y += delta + mVelocity.y;
	if (mHeldObject != nullptr) {
		mVelocity.y -= mSpeed;
		JGeometry::TVec3<f32> pos = mHeldObject->mPosition;
		JGeometry::TVec3<f32> vel = mVelocity;
		pos.y += vel.y;
		mHeldObject->moveRequest(pos);
	}
	JGeometry::TVec3<f32> vel = mVelocity;
	mPosition.y += vel.y;
}

void TMapObjElasticCode::initMapObj()
{
	TMapObjBase::initMapObj();
	mFriction    = 0.997f;
	mGravity     = 0.01f;
	mSpeed       = 2.0f;
	mSpringConst = 0.0005f;
}

// ===========================================================================
// TMapObjGrowTree
// ===========================================================================

void TMapObjGrowTree::getGrowHeightFromRate(float param_1) const
{
	// TODO: unconfirmed
}

void TMapObjGrowTree::updateHeight()
{
	// TODO: unconfirmed
}

u32 TMapObjGrowTree::touchWater(THitActor* param_1)
{
	// TODO: unconfirmed
	return 0;
}

void TMapObjGrowTree::control()
{
	// TODO: unconfirmed
}

void TMapObjGrowTree::loadAfter()
{
	TMapObjBase::loadAfter();
	removeMapCollision();
}

void TMapObjGrowTree::initMapObj()
{
	TMapObjBase::initMapObj();
	mGrowHeight    = 1000.0f;
	mGrowSpeed     = 0.5f;
	mGrowRate      = 0.1f;
	mAppearTime    = 360;
	mMinGrowHeight = mDamageHeight;
	mMActor->setBtp("moyasi_wink");
}

TMapObjGrowTree::TMapObjGrowTree(const char* name)
	: TMapObjBase(name)
	, mGrowHeight(0.0f)
	, mGrowSpeed(0.0f)
	, mGrowRate(0.0f)
	, mAppearTime(0)
	, mMinGrowHeight(0.0f)
{
}

// ===========================================================================
// TWireBell
// ===========================================================================

void TWireBell::initDraw() const
{
	// TODO: unconfirmed
}

void TWireBell::draw() const
{
	// TODO: unconfirmed
}

void TWireBell::control()
{
	Mtx mtx;
	gpMapWireManager->getPointPosInNthWire(mWireNo, mPosition, &mPosOnWire);
	mPosition.x = mPosOnWire.x;
	mPosition.y = mPosOnWire.y - mLength;
	mPosition.z = mPosOnWire.z;
	MsMtxSetTRS((MtxPtr)mtx, mPosition, mRotation, mScaling);
	PSMTXCopy(getModel()->getAnmMtx(0), (MtxPtr)mtx);
}

void TWireBell::loadAfter()
{
	TMapObjBase::loadAfter();
	mWireNo = gpMapWireManager->getWireNo(mPosition);
}

TWireBell::TWireBell(const char* name)
	: TMapObjBase(name)
	, mWireNo(-1)
	, mLength(200.0f)
	, mLimitRotY(10.0f)
	, mLimitRotX(5.0f)
	, mTexPosRate(0.01f)
	, mPosOnWire()
{
}

// ===========================================================================
// TMapObjPuncher
// ===========================================================================

void TMapObjPuncher::touchPlayer(THitActor* param_1)
{
	awake();
	startAnim(1);
	JGeometry::TVec3<f32> toMario;
	makeVecToLocalZ(1.0f, &toMario);
	JGeometry::TVec3<f32> offset = toMario;
	offset.scale(100.0f);
	JGeometry::TVec3<f32> target = *gpMarioPos;
	target.add(offset);
	SMS_MarioMoveRequest(target);
	SMS_SendMessageToMario(this, 7);
	SMS_ThrowMario(toMario, mThrowPower);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
	emitAndScale(0xE5, 0, &mPosition, scale);
	emitAndScale(0xE6, 0, &mPosition, scale);
	SMSGetMSound()->startSoundActor(0x387D, &mPosition, 0, nullptr, 0, 4);
	mState = 2;
}

void TMapObjPuncher::control()
{
	TMapObjBase::control();
	switch (mState) {
	case 2:
		soundBas(0x385F, 101.0f, mMActor->getFrameCtrl(0)->getRate());
		if (animIsFinished()) {
			JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
			emitAndScale(0xE5, 0, &mPosition, scale);
			emitAndScale(0xE6, 0, &mPosition, scale);
			SMSGetMSound()->startSoundActor(0x387D, &mPosition, 0, nullptr,
			                                0, 4);
			kill();
		}
	}
}

void TMapObjPuncher::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	s16 value;
	stream >> value;
	// the target's u16 -> f32 conversion is spelled as the raw xor-and-
	// subtract here, matching the ROM's double-precision magic constant
	mThrowPower = (f32)(u16)value;
	sleep();
	offHitFlag(HIT_FLAG_CANNOT_ATTACK);
}

// ===========================================================================
// TMuddyBoat
// ===========================================================================

void TMuddyBoat::moveByWater()
{
	// TODO: unconfirmed
}

void TMuddyBoat::calcRootMatrix() { }

void TMuddyBoat::kill()
{
	// TODO: 0x39 has no name in System/Particles.hpp's enums yet.
	mSpeed          = 0.0f;
	mWaterLeakValue = 0.0f;
	// TODO: 0x39 has no name in System/Particles.hpp's enums yet, so it is
	// spelled as a cast of the existing PARTICLE_MS_M_AMIATTACK slot.
	SMS_EasyEmitParticle((E_SMS_EFFECT_ONETIME_NORMAL)0x39, &mTargetPos,
	                     nullptr, JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_DORO_BROKEN, &mPosition, 0,
	                                nullptr, 0, 4);
	MtxPtr base = getModel()->getBaseTRMtx();
	PSMTXCopy(getModel()->getAnmMtx(0), base);
	// rlwinm r0, r0, 0, 24, 22 clears the low 22 map-obj flag bits
	unkF8 &= 0xFFC00000;
	onLiveFlag(LIVE_FLAG_UNK10);
	startAnim(1);
	startAnim(2);
	setState(2);
}

void TMuddyBoat::touchWall(JGeometry::TVec3<float>* param_1,
                           const TBGWallCheckRecord& param_2)
{
	// TODO: unconfirmed
}

void TMuddyBoat::bindToWall(const JGeometry::TVec3<float>& param_1, float param_2,
                            JGeometry::TVec3<float>* param_3)
{
	// TODO: unconfirmed
}

void TMuddyBoat::bind()
{
	// TODO: unconfirmed
}

void TMuddyBoat::control()
{
	// TODO: unconfirmed
}

void TMuddyBoat::calc()
{
	// TODO: unconfirmed
}

u32 TMuddyBoat::getSDLModelFlag() const
{
	// TODO: unconfirmed value
	return 0;
}

void TMuddyBoat::initMapObj()
{
	TMapObjBase::initMapObj();
	mAccelPos      = 0.04f;
	mSpeedFriction = 0.998f;
	mWaterLeakRate = 0.002f;
	mWaterLeakMul  = 0.997f;
	mAccelNeg      = 0.01f;
	mAppearTime    = 600;
	// TODO: 0x34 is not a real map number in the game's map enum yet; the
	// target tests gpMarDirector->mMap against it directly.
	if (SMSGetMarDirector()->getCurrentMap() == 0x34) {
		mWallDepthA = 126.0f;
		mWallHeight = 185.0f;
		mWallDepthB = 150.0f;
		mWallDepthC = 170.0f;
		mWallWidth  = 185.0f;
	} else {
		mWallDepthA = 100.0f;
		mWallHeight = 170.0f;
		mWallDepthB = 150.0f;
		mWallDepthC = 180.0f;
		mWallWidth  = 100.0f;
	}
	mScale.set(3.0f, 2.0f, 5.0f);
}

TMuddyBoat::TMuddyBoat(const char* name)
	: TMapObjBase(name)
	, mAccelPos(0.0f)
	, mAccelNeg(0.0f)
	, mSpeed(0.0f)
	, mSpeedFriction(0.0f)
	, mWaterLeakRate(0.0f)
	, mWaterLeakValue(0.0f)
	, mWaterLeakMul(0.0f)
	, mWallHeight(0.0f)
	, mWallDepthA(0.0f)
	, mWallDepthB(0.0f)
	, mWallDepthC(0.0f)
	, mWallWidth(0.0f)
	, mAppearTime(0)
	, mCount(0)
	, mTargetPos()
	, mScale()
{
}

// ===========================================================================
// TMareFall
// ===========================================================================

// The waterfall's upper lip; the map's sinit writes 2827.0f / 8604.0f /
// 7202.0f into it, so it is a namespace-scope TVec3 with a real
// initialiser (hence the `init` guard-free direct stores in __sinit).
static JGeometry::TVec3<f32> fall_upper_pos(2827.0f, 8604.0f, 7202.0f);

void TMareFall::calc()
{
	SMSGetMSound()->startSoundActor(MSD_SE_GE_FALL, &mPosition, 0, nullptr,
	                                0, 4);
	SMSGetMSound()->startSoundActor(MSD_SE_GE_FALL_UPPER, &fall_upper_pos, 0,
	                                nullptr, 0, 4);
	// TODO: exact shape of the two emit() calls is right, but the frame is
	// still 8 bytes short of the target's 0x28 -- something in the original
	// left a dead 8-byte stack object here that has not been identified.
	const void* arg = this;
	gpMarioParticleManager->emit(0x149, &mPosition, 1, arg);
	gpMarioParticleManager->emit(0x14A, &mPosition, 1, arg);
}

void TMareFall::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	SMS_LoadParticle("/scene/mapObj/mareFallSplash.jpa", 0x149);
	SMS_LoadParticle("/scene/mapObj/mareFallSmoke.jpa", 0x14A);
}

// ===========================================================================
// TMareCork
// ===========================================================================

void TMareCork::loadAfter()
{
	mCannon = static_cast<TCannon*>(JDrama::TNameRefGen::search("砲台"));
	// message 4 asks the cannon to release its cork
	if (mCannon->receiveMessage(this, 4)) {
		mHeldObject = mCannon;
	}
	SMS_LoadParticle("/scene/map/map/ms_mare_gunwat_a.jpa", 0x14C);
	SMS_LoadParticle("/scene/map/map/ms_mare_gunwat_b.jpa", 0x14D);
	SMS_LoadParticle("/scene/map/map/ms_mare_gunwat_c.jpa", 0x14E);
	TMapObjBase::loadAfter();
	mVel.setAll(0.0f);
	initAnmSound();
}

void TMareCork::moveObject()
{
	if (mCannon->isObject() && !mIsMoving) {
		mMActor->setBck("marecork");
		setAnmSound("/scene/mapObj/marecork.bas");
		removeMapCollision();
		mIsMoving = 1;
	}
}

void TMareCork::calcRootMatrix()
{
	if (mIsMoving) {
		// both wire bells count as collected once the animation has run this
		// far; only the second call's result is branched on.
		mMActor->getFrameCtrl(0)->checkPass(350.0f);
		if (mMActor->getFrameCtrl(0)->checkPass(250.0f)) {
			mCannon->startChorobeiShout();
			gpItemManager->makeShineAppearWithDemo("シャイン（ボス用）",
			                                      "ボスシャインカメラ",
			                                      mPosition.x, mPosition.y,
			                                      mPosition.z);
			mShinePos.set(2773.0f, 8618.0f, 7006.0f);
			JPABaseEmitter* emitter = gpMarioParticleManager->emitWithRotate(
				0x44, &mShinePos, 0x4000, 0xd82, 0, 0, nullptr);
			if (emitter) {
				emitter->mGlobalDynamicsScale.setAll(2.5f);
				emitter->mGlobalParticleScale.setAll(2.5f);
			}
		}
	}
	TMapObjBase::calcRootMatrix();
}

MtxPtr TMareCork::getTakingMtx()
{
	// mNodeMatrices[2] corresponds to the cork joint
	return mMActor->getModel()->getAnmMtx(2);
}

void TMareCork::drawObject(JDrama::TGraphics* graphics)
{
	TLiveActor::drawObject(graphics);
	if (mIsMoving && mMActor->getFrameCtrl(0)->getFrame() > 250.0f) {
		mShinePos.set(2773.0f, 8618.0f, 7006.0f);
		SMSGetMSound()->startSoundActor(MSD_SE_ENV_FALL_JET_LEVEL,
		                                &mShinePos, 0, nullptr, 0, 4);
		gpMarioParticleManager->emitAndBindToPosPtr(0x14C, &mVel, 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(0x14D, &mVel, 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(0x14E, &mVel, 1, this);
	}
}

// ===========================================================================
// TMareEventPoint
// ===========================================================================

BOOL TMareEventPoint::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: the exact meaning of the 0x1000 particle flag and the
	// 0.1f normal-Z threshold have not been confirmed.
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		int water_id = TMapObjBase::getWaterID(sender);
		// The target assigns this comparison to a bool before testing it,
		// which is where the extra `li r0,1 / li r0,0` pair comes from.
		bool is_spray;
		if (gpModelWaterManager->getFlagBottom4Bits(water_id) == 1)
			is_spray = true;
		else
			is_spray = false;
		if (is_spray) {
			const TBGCheckData* plane = TMapObjBase::getWaterPlane(sender);
			if (plane != nullptr) {
				if (TMapObjBase::getWaterPlane(sender)->mNormal.y < 0.1f) {
					TMareEventDepressWall* wall
					    = (TMareEventDepressWall*)mMareEventDepressWall;
					if (wall->startEvent()) {
						gpMarioParticleManager->emit(0xE7, &sender->mPosition,
						                             0, nullptr);
						SMSGetMSound()->startSoundSet(
						    MSD_SE_EN_COMMON_W_HIT_OK, &mPosition, 0,
						    0.0f, 0, 0, 4);
						return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}

void TMareEventPoint::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);
	initHitActor(0x40000236, 0, 0, 0.0f, 0.0f, 300.0f, 600.0f);
}
