// rogue include: the original TU opens .rodata with this dummy string
// pair, ahead of every other string constant in the object.
#include <M3DUtil/InfectiousStrings.hpp>

#include <MoveBG/MapObjBianco.hpp>
#include <Camera/CubeManagerBase.hpp>
#include <Camera/CubeMapTool.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/MapObjMessenger.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Watergun.hpp>
#include <System/Application.hpp>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// NOTE: this unit is -inline deferred, so functions below are defined in the
// reverse of their retail address order.

static f32 sRadius = 2380.0f;
static f32 sSubZ   = 150.0f;
static f32 sSpeed  = 0.05f;
static f32 sAngleAdd;

f32 TMapObjRootPakkun::mTremblePower = 15.0f;
f32 TMapObjRootPakkun::mTrembleAccel = 0.95f;
f32 TMapObjRootPakkun::mTrembleBrake = 0.98f;
int TMapObjRootPakkun::mTrembleTime  = 360;

// TODO: values unknown, these are UNUSED in the retail map
f32 TBiancoWatermill::mRotAccel;
f32 TBiancoWatermill::mEnemyRotAccel;
f32 TBiancoWatermill::mRotSpeedDownRate;
f32 TBiancoWatermill::mRotSpeedMax;
f32 TBiancoWatermill::mRotSpeedMin;

f32 TBiancoWatermillVertical::mRotAccel         = 0.15f;
f32 TBiancoWatermillVertical::mRotSpeedDownRate = 0.005f;
f32 TBiancoWatermillVertical::mRotSpeedMax      = 3.0f;
f32 TBiancoWatermillVertical::mBridgeRotRate    = 0.03f;

f32 TBiancoMiniWindmill::mRotWaterAccel = 0.01f;
f32 TBiancoMiniWindmill::mFriction      = 0.01f;
f32 TBiancoMiniWindmill::mRotSpeedMax   = 10.0f;

static f32 sMessengerPosZ = 200.0f;
static f32 sMessengerPosY = 6400.0f;

f32 TLeafBoatRotten::mAlphaDownSpeed       = 0.5f;
f32 TLeafBoatRotten::mCollisionRemoveAlpha = 100.0f;
// TODO: values unknown, these are UNUSED in the retail map
int TLeafBoatRotten::mBoatFlushTime;
int TLeafBoatRotten::mBoatFlushInterval;
GXColorS10 TLeafBoatRotten::mRottenColor = { 100, 100, 180, 255 };

// TBigWindmill ---------------------------------------------------------------

void TBigWindmill::control()
{
	TMapObjBase::control();
	mRotation.z -= sSpeed;
	mRotation.z = MsWrap(mRotation.z, 0.0f, 360.0f);
	setRootMtxRotZ();
	gpMSound->startSoundActorWithInfo(0x3047, &mPosition, nullptr,
	                                  fabsf(sSpeed), 0, 0, &unk148, 0, 4);

	f32 angle = mRotation.z + sAngleAdd;
	for (int i = 0; i < 4; ++i) {
		MtxPtr mtx = unk138[i]->getModel()->getAnmMtx(0);
		f32 rad    = 0.017453294f * angle;
		mtx[0][3]  = sRadius * cosf(rad) + mPosition.x;
		mtx[1][3]  = sRadius * sinf(rad) + mPosition.y - mYOffset;
		mtx[2][3]  = mPosition.z - sSubZ;
		unk138[i]->mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);
		angle += 90.0f;
		if (angle > 360.0f)
			angle -= 360.0f;
	}
}

void TBigWindmill::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	for (int i = 0; i < 4; ++i) {
		unk138[i] = TMapObjBaseManager::newAndRegisterObj("bigWindmillBlock");
		unk138[i]->initMapObj();
		unk138[i]->getModel()->calc();
	}
}

// TMapObjRootPakkun ----------------------------------------------------------

void TMapObjRootPakkun::drawObject(JDrama::TGraphics* graphics)
{
	TLiveActor::drawObject(graphics);
	if (fabsf(gpMarioPos->z - mPosition.z) < 10000.0f) {
		unk138->movement();
		if (!isStateTimerEngaged()) {
			unk138->tremble(mTremblePower, mTrembleAccel, mTrembleBrake,
			                mTrembleTime);
			mStateTimer = mTrembleTime;
		}
	}
}

void TMapObjRootPakkun::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = new TTrembleModelEffect;
	unk138->init(mMActor->getModel());
	unk138->tremble(100.0f, 1.0f, 1.0f, 12000);
}

// TBiancoWatermill -----------------------------------------------------------

void TBiancoWatermill::turnByEnemy(THitActor*, const TBGCheckData*) { }

// TODO: body is unknown
void TBiancoWatermill::turn(const JGeometry::TVec3<f32>&, const TBGCheckData*,
                            f32)
{
}

u32 TBiancoWatermill::touchWater(THitActor*) { return 0; }

void TBiancoWatermill::control()
{
	mRotation.z -= unk138;
	gpMSound->startSoundActorWithInfo(0x3043, &mPosition, nullptr,
	                                  fabsf(unk138), 0, 0, &unk13C, 0, 4);
}

void TBiancoWatermill::initMapObj()
{
	TMapObjBase::initMapObj();
	if (strcmp(unkF4, "BiaWatermill01") == 0)
		mBodyRadius = 1200.0f;
	else if (strcmp(unkF4, "BiaWatermill00") == 0)
		mBodyRadius = 1200.0f;
}

TBiancoWatermill::TBiancoWatermill(const char* name)
    : TMapObjBase(name)
    , unk138(0.3f)
    , unk13C(nullptr)
{
}

// TBiancoWatermillVertical ---------------------------------------------------

u32 TBiancoWatermillVertical::touchWater(THitActor* water)
{
	if (!getWaterPlane(water)) {
		unk144 = 1;
		return 0;
	}

	if (!waterHitPlane(water))
		return 0;

	const JGeometry::TVec3<f32>& pos   = getWaterPos(water);
	const JGeometry::TVec3<f32>& speed = getWaterSpeed(water);

	JGeometry::TVec3<f32> dir(speed.x, 0.0f, speed.z);
	if (dir.x != 0.0f || dir.z != 0.0f)
		MsVECNormalize(&dir, &dir);

	JGeometry::TVec3<f32> vertical;
	getVerticalVecToTargetXZ(pos.x, pos.z, &vertical);
	MsVECNormalize(&vertical, &vertical);

	f32 rate = (mBodyRadius - getDistanceXZ(pos)) / mBodyRadius;
	if (dir.dot(vertical) > 0.0f) {
		if (unk138 < mRotSpeedMax)
			unk138 += mRotAccel * rate;
	} else {
		if (unk138 > -mRotSpeedMax)
			unk138 -= mRotAccel * rate;
	}
	return 1;
}

void TBiancoWatermillVertical::setGroundCollision()
{
	if (unk144 || mColCount != 0) {
		MtxPtr mtx = getModel()->getAnmMtx(0);
		if (mMapCollisionManager->unk8)
			mMapCollisionManager->unk8->moveMtx(mtx);
		unk144 = 0;
	}
}

void TBiancoWatermillVertical::control()
{
	if (unk138 != unk13C) {
		if (unk138 > unk13C) {
			unk138 -= mRotSpeedDownRate;
			if (unk138 < unk13C)
				unk138 = unk13C;
		} else {
			unk138 += mRotSpeedDownRate;
			if (unk138 > unk13C)
				unk138 = unk13C;
		}
	}

	mRotation.y += unk138;
	mRotation.y = MsWrap(mRotation.y, 0.0f, 360.0f);

	f32 bridgeSpeed = unk138 * mBridgeRotRate;
	unk140->mRotation.y += bridgeSpeed;
	unk140->mRotation.y = MsWrap(unk140->mRotation.y, 0.0f, 360.0f);

	gpMSound->startSoundActorWithInfo(0x3040, &mPosition, nullptr,
	                                  fabsf(unk138), 0, 0, &unk148, 0, 4);
	gpMSound->startSoundActorWithInfo(0x3042, &unk140->mPosition, nullptr,
	                                  fabsf(bridgeSpeed), 0, 0, &unk14C, 0, 4);
}

void TBiancoWatermillVertical::loadAfter()
{
	TMapObjBase::loadAfter();
	if (strcmp(getName(), "BiaWatermillVertical 0") == 0)
		unk140 = (TMapObjBase*)JDrama::TNameRefGen::search("BiaTurnBridge 0");
	else
		unk140 = (TMapObjBase*)JDrama::TNameRefGen::search("BiaTurnBridge 1");
	mBodyRadius = 1000.0f;
}

void TBiancoWatermillVertical::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	stream.read(&unk13C, 4);
	unk13C /= 1000.0f;
	unk138 = unk13C;
}

TBiancoWatermillVertical::TBiancoWatermillVertical(const char* name)
    : TMapObjBase(name)
    , unk138(0.0f)
    , unk13C(0.0f)
    , unk140(nullptr)
    , unk144(0)
    , unk148(nullptr)
    , unk14C(nullptr)
{
}

// TBiancoMiniWindmill --------------------------------------------------------

u32 TBiancoMiniWindmill::touchWater(THitActor* water)
{
	const JGeometry::TVec3<f32>& pos = getWaterPos(water);
	if (pos.y < mPosition.y + sMessengerPosY - 300.0f)
		return 1;

	const JGeometry::TVec3<f32>& speed = getWaterSpeed(water);
	MtxPtr mtx                         = getModel()->getAnmMtx(0);
	if (speed.x * mtx[0][2] + speed.y * mtx[1][2] + speed.z * mtx[2][2]
	    > 0.0f)
		return 0;

	unk154 += mRotWaterAccel;
	if (unk154 > mRotSpeedMax) {
		unk154 = mRotSpeedMax;
		JGeometry::TVec3<f32> point(
		    mPosition.x, 550.0f + unk15C->mPosition.y, mPosition.z);
		mAppearYSpeed = 0.0f;
		appearObjFromPoint(point);
	}
	return 1;
}

void TBiancoMiniWindmill::calc()
{
	TMtx34f rot;
	rot.identity();
	MtxPtr mtx = rot;
	MsMtxSetRotZ(mtx, unk150);
	MTXConcat(getModel()->getAnmMtx(0), mtx, mtx);
	MtxPtr base = getModel()->getAnmMtx(1);
	mtx[0][3]   = base[0][3];
	mtx[1][3]   = base[1][3];
	mtx[2][3]   = base[2][3];
	MTXCopy(mtx, getModel()->getAnmMtx(1));

	if (gpMSound->getDistPowFromCamera(unk15C->mPosition) < 36000000.0f)
		gpMSound->startSoundActorWithInfo(0x3045, &unk15C->mPosition,
		                                  nullptr, fabsf(unk154), 0, 0,
		                                  &unk160, 0, 4);
}

void TBiancoMiniWindmill::control()
{
	if (unk154 > unk158)
		unk154 -= mFriction;
	else
		unk154 = unk158;
	unk150 += unk154;
	unk150 = MsWrap(unk150, 0.0f, 360.0f);
}

void TBiancoMiniWindmill::initMapObj()
{
	TMapObjBase::initMapObj();
	mAppearSpeed = 0.0f;
	unk15C = new TMapObjMessenger("地形オブジェメッセンジャー");
	unk15C->initHitActor(0, 1, 0, 0.0f, 0.0f, 300.0f, 500.0f);
	unk15C->mPosition.x = sMessengerPosZ * MsSin(mRotation.y) + mPosition.x;
	unk15C->mPosition.y = mPosition.y + sMessengerPosY;
	unk15C->mPosition.z = sMessengerPosZ * MsCos(mRotation.y) + mPosition.z;
}

TBiancoMiniWindmill::TBiancoMiniWindmill(const char* name)
    : THideObjBase(name)
{
	unk150 = 360.0f * MsRandF();
	unk154 = 0.0f;
	unk158 = 1.0f + MsRandF();
	unk15C = nullptr;
	unk160 = nullptr;
}

// TLeafBoat ------------------------------------------------------------------

void TLeafBoat::touchActor(THitActor* actor)
{
	if (actor->isActorType(0x80000001))
		return;

	JGeometry::TVec3<f32> dir(actor->mPosition.x - mPosition.x, 0.0f,
	                          actor->mPosition.z - mPosition.z);
	if (dir.dot(JGeometry::TVec3<f32>(mVelocity)) < 0.0f)
		return;

	if (dir.x != 0.0f || dir.z != 0.0f)
		MsVECNormalize(&dir, &dir);

	f32 dot = dir.dot(JGeometry::TVec3<f32>(mVelocity));
	if (actor->checkActorType(0x10000000)) {
		mVelocity.x -= (1.0f + unk138) * (dir.x * dot);
		mVelocity.z -= (1.0f + unk138) * (dir.z * dot);
	} else {
		mVelocity.x -= (1.0f + unk13C) * (dir.x * dot);
		mVelocity.z -= (1.0f + unk13C) * (dir.z * dot);
	}
}

// TODO: the ROM keeps this out-of-line in bind(), find out why instead of
// forcing it
#pragma dont_inline on
void TLeafBoat::touchWall(JGeometry::TVec3<f32>* pos,
                          TBGWallCheckRecord* record)
{
	int wallNum = record->mResultWallsNum;
	for (int i = 0; i < wallNum; ++i) {
		const TBGCheckData* wall = record->mResultWalls[i];
		if (JGeometry::TVec3<f32>(mVelocity).dot(wall->getNormal()) < 0.0f) {
			f32 dist = pos->dot(wall->getNormal()) + wall->mPlaneDistance;
			pos->x += (mBodyRadius - dist) * wall->getNormal().x;
			pos->z += (mBodyRadius - dist) * wall->getNormal().z;
			JGeometry::TVec3<f32> velocity(mVelocity);
			calcReflectingVelocity(wall, 1.0f, &velocity);
			mVelocity.x = velocity.x * unk140;
			mVelocity.z = velocity.z * unk140;
			break;
		}
	}
}
#pragma dont_inline off

void TLeafBoat::bind()
{
	JGeometry::TVec3<f32> pos(mPosition);
	pos.x += JGeometry::TVec3<f32>(mVelocity).x;
	pos.z += JGeometry::TVec3<f32>(mVelocity).z;

	const TBGCheckData* ground;
	if (gpMap->checkGroundIgnoreWaterSurface(pos.x, mPosition.y - mYOffset,
	                                         pos.z, &ground)
	    > mPosition.y - mYOffset - 50.0f) {
		JGeometry::TVec3<f32> velocity(mVelocity);
		calcReflectingVelocity(ground, 1.0f, &velocity);
		mVelocity.x *= -1.0f;
		mVelocity.z *= -1.0f;
		pos = mPosition;
	}

	JGeometry::TVec3<f32> center(pos.x, pos.y - mYOffset, pos.z);
	TBGWallCheckRecord record(center, mBodyRadius, 4, 2);
	if (gpMap->isTouchedWallsAndMoveXZ(&record))
		touchWall(&pos, &record);

	// TODO: the ROM calls TVec3<f32>::sub out-of-line here (it is never
	// inlined anywhere in the game), our JGeometry header inlines it.
	JGeometry::TVec3<f32> delta(pos);
	delta.sub(mPosition);
	mLinearVelocity = delta;

	f32 marioY = gpMarioPos->y;
	f32 y      = mPosition.y - mYOffset;
	f32 dx     = gpMarioPos->x - mPosition.x;
	f32 dz     = gpMarioPos->z - mPosition.z;
	if (marioY <= y && y - 100.0f < marioY
	    && dx * dx + dz * dz < mBodyRadius * mBodyRadius)
		SMS_SendMessageToMario(this, 0xE);
}

void TLeafBoat::control()
{
	TMapObjBase::control();
	if (marioHipAttack())
		mVelocity.y -= unk154;

	if (marioIsOn()) {
		mVelocity.y -= unk150;
		if (SMS_GetMarioWaterGun()->mIsEmitWater > 0) {
			MtxPtr mtx = SMS_GetMarioWaterGun()->getEmitMtx(0);
			mVelocity.x -= mtx[0][0] * unk144;
			mVelocity.z -= mtx[2][0] * unk144;
		}
	}

	int cube = gpCubeStream->getInCubeNo(mPosition);
	if (cube != -1) {
		TCubeStreamInfo& info = (TCubeStreamInfo&)(*gpCubeStream->unk14)[cube];
		Mtx mtx;
		MsMtxSetXYZRPH(mtx, 0.0f, 0.0f, 0.0f, info.unk18.x, info.unk18.y,
		               info.unk18.z);
		f32 speed = 0.0001f * info.unk40;
		mVelocity.x += mtx[0][2] * speed;
		mVelocity.z += mtx[2][2] * speed;
	}

	mPosition.y += mVelocity.y;
	mVelocity.y += unk158 * (mInitialPosition.y - (mPosition.y - mYOffset));
	mVelocity.y *= unk15C;
	mVelocity.x *= unk148;
	mVelocity.z *= unk148;
}

void TLeafBoat::calc()
{
	if (unk144 == 0.0f)
		return;

	if (unk160 > 8) {
		if (fabsf(mVelocity.x) + fabsf(mVelocity.z) > 0.1f) {
			unk164.set(mPosition.x, mPosition.y - mYOffset, mPosition.z);
			JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
			emitAndBindScale(0x1E8, 3, &unk164, scale);
			emitAndBindScale(0x107, 1, &unk164, scale);
		}
		unk160 = 0;
	} else {
		++unk160;
	}
}

void TLeafBoat::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = 1.0f;
	unk13C = 0.5f;
	unk140 = 0.5f;
	unk148 = 0.998f;
}

TLeafBoat::TLeafBoat(const char* name)
    : TMapObjBase(name)
    , unk138(0.0f)
    , unk13C(0.0f)
    , unk140(0.0f)
    , unk144(0.03f)
    , unk148(0.0f)
    , unk14C(1.2f)
    , unk150(0.03f)
    , unk154(2.0f)
    , unk158(0.005f)
    , unk15C(0.98f)
    , unk160(0)
{
	unk164.zero();
}

// TLeafBoatRotten ------------------------------------------------------------

void TLeafBoatRotten::control()
{
	TLeafBoat::control();
	if (marioIsOn() && isState(1)) {
		mStateTimer = unk170;
		mState      = 2;
	}

	switch (mState) {
	case 2: {
		f32 rate   = (f32)mStateTimer / (f32)unk170;
		unk178.r = (u8)((255 - mRottenColor.r) * rate + mRottenColor.r);
		unk178.g = (u8)((255 - mRottenColor.g) * rate + mRottenColor.g);
		unk178.b = (u8)((255 - mRottenColor.b) * rate + mRottenColor.b);
		if (!isStateTimerEngaged()) {
			unk174 = 255.0f;
			mState = 3;
		}
		break;
	}
	case 3:
		unk174 -= mAlphaDownSpeed;
		unk178.a = (u8)unk174;
		if (unk174 < mCollisionRemoveAlpha) {
			if (mMapCollisionManager->unk8->isSetUp())
				removeMapCollision();
		}
		if (unk174 <= 0.0f) {
			mScaling.set(1.0f, 1.0f, 1.0f);
			makeObjDefault();
			appear();
			unk178.r = 255;
			unk178.g = 255;
			unk178.b = 255;
			unk178.a = 255;
			mState   = 1;
		}
		break;
	}
}

void TLeafBoatRotten::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TMapObjBase::perform(cue, graphics);
}

void TLeafBoatRotten::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	stream.read(&unk170, 4);
	unk170 *= 10;
	SMS_InitPacket_OneTevColor(getModel(), 0, GX_TEVREG0, &unk178);
}

TLeafBoatRotten::TLeafBoatRotten(const char* name)
    : TLeafBoat(name)
    , unk170(0)
{
	unk178.r = 255;
	unk178.g = 255;
	unk178.b = 255;
	unk178.a = 255;
}

// TLampSeesaw ----------------------------------------------------------------

void TLampSeesaw::touchPlayer(THitActor*)
{
	if (marioIsOn())
		mPartner->pushDown(-unk140);
}

void TLampSeesaw::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	f32 depth;
	stream.read(&depth, 4);
	unk13C = mInitialPosition.y - depth;
	stream.read(&unk140, 4);
	unk140 *= 0.0001f;
}

TLampSeesaw::TLampSeesaw(const char* name)
    : TMapObjBase(name)
    , mPartner(nullptr)
    , unk140(0.01f)
{
}

// TLampSeesawMain ------------------------------------------------------------

void TLampSeesawMain::pushDown(f32 amount)
{
	mState = 2;
	unk144 -= amount;
}

// TODO: size not verified
void TLampSeesawMain::move()
{
	f32 next = mPosition.y + unk144;
	if (next < unk13C || mPartner->mPosition.y - unk144 < mPartner->unk13C) {
		if (fabsf(unk144) < unk150)
			unk144 = 0.0f;
		else
			unk144 *= -unk14C;
	} else {
		mPosition.y = next;
		mPartner->mPosition.y -= unk144;
	}
	unk144 *= unk148;
}

void TLampSeesawMain::touchPlayer(THitActor*)
{
	if (marioIsOn())
		pushDown(unk140);
}

void TLampSeesawMain::control()
{
	TMapObjBase::control();
	switch (mState) {
	case 2:
		move();
		if (fabsf(unk144) < unk150)
			mState = 3;
		break;
	case 3:
		if (unk144 < 0.0f)
			unk144 = -unk150;
		else
			unk144 = unk150;
		move();
		if (fabsf(unk144) <= unk150
		    && fabsf(mInitialPosition.y - mPosition.y) < unk150) {
			unk144 = 0.0f;
			mState = 1;
		}
		break;
	}
}

void TLampSeesawMain::loadAfter()
{
	char name[64];
	int len          = strlen("ランプシーソーＡ");
	const char* self = getName();
	char c0          = self[len];
	char c1          = self[len + 1];
	char c2          = self[len + 2];
	char c3          = self[len + 3];
	snprintf(name, 0x40, "ランプシーソーＢ００");
	name[len]     = c0;
	name[len + 1] = c1;
	name[len + 2] = c2;
	name[len + 3] = c3;
	mPartner = (TLampSeesaw*)JDrama::TNameRefGen::search(name);
	mPartner->mPartner = this;
}

TLampSeesawMain::TLampSeesawMain(const char* name)
    : TLampSeesaw(name)
    , unk144(0.0f)
    , unk148(0.998f)
    , unk14C(0.8f)
    , unk150(0.5f)
{
}

// TBiancoBell ----------------------------------------------------------------

// TODO: body is a guess, size not verified
void TBiancoBell::stopToRing() { mMActor->getFrameCtrl(0)->setRate(0.0f); }

void TBiancoBell::ring()
{
	if (getMActor()->getFrameCtrl(0)->getFrame() == 0.0f
	    || getMActor()->getFrameCtrl(0)->getFrame()
	               + getMActor()->getFrameCtrl(0)->getRate()
	           >= getMActor()->getFrameCtrl(0)->getEnd() - 1.0f) {
		startAnim(mRingAnm);
		getMActor()->getFrameCtrl(0)->setRate(SMSGetAnmFrameRate());
		if (mRingSound)
			gpMSound->startSoundActor(0x89B8, &mPosition, 0, nullptr, 0, 4);
	}
}

void TBiancoBell::ringSingle()
{
	if (getMActor()->getFrameCtrl(0)->getFrame() == 0.0f
	    || getMActor()->getFrameCtrl(0)->getFrame()
	               + getMActor()->getFrameCtrl(0)->getRate()
	           >= getMActor()->getFrameCtrl(0)->getEnd() - 1.0f) {
		startAnim(4);
		getMActor()->getFrameCtrl(0)->setRate(SMSGetAnmFrameRate());
		gpMSound->startSoundActor(0x89B8, &mPosition, 0, nullptr, 0, 4);
	}
}

u32 TBiancoBell::touchWater(THitActor*)
{
	ringSingle();
	return 1;
}

void TBiancoBell::touchPlayer(THitActor*) { ringSingle(); }

void TBiancoBell::initMapObj()
{
	TMapObjBase::initMapObj();
	if (strcmp(getName(), "BiaBell 0") == 0) {
		mRingAnm   = 1;
		mRingSound = 0;
	} else if (strcmp(getName(), "BiaBell 1") == 0) {
		mRingAnm   = 2;
		mRingSound = 1;
	} else {
		mRingAnm   = 3;
		mRingSound = 0;
	}
}

TBiancoBell::TBiancoBell(const char* name)
    : TMapObjBase(name)
    , mRingAnm(0)
    , mRingSound(0)
{
}

// TBellWatermill -------------------------------------------------------------

u32 TBellWatermill::touchWater(THitActor*)
{
	unk190 = 1;
	if (fabsf(unk158) > unk16C) {
		unk178 += unk180;
		unk158 += unk15C;
	} else {
		unk158 += unk15C;
	}
	if (unk158 > unk164)
		unk158 = unk164;
	return 1;
}

void TBellWatermill::control()
{
	TMapObjBase::control();
	if (unk158 == 0.0f && unk178 == 0.0f && unk170 == 0.0f)
		return;

	unk154 += MsClamp(unk158, -unk16C, unk16C);
	unk154 = MsWrap(unk154, 0.0f, 360.0f);

	gpMSound->startSoundActorWithInfo(0x3044, &mPosition, nullptr,
	                                  fabsf(unk158), 0, 0, &unk1A4, 0, 4);

	if (fabsf(unk158) < fabsf(unk160))
		unk158 = 0.0f;
	if (!unk190 && unk158 != 0.0f)
		unk158 -= unk160;

	if (unk170 < 0.0f) {
		if (fabsf(unk178) < unk17C) {
			unk170 = 0.0f;
			unk178 = 0.0f;
		} else {
			unk170 -= unk178;
			unk178 *= -unk188;
		}
	} else if (!unk190 && unk170 != 0.0f) {
		unk178 -= unk184;
	}

	unk170 += unk178;
	if (unk170 > unk174) {
		mBells[0]->ring();
		mBells[1]->ring();
		mBells[2]->ring();
		unk170 = unk174;
		if (unk1A0) {
			for (int i = 0; i < 5; ++i) {
				TMapObjBase* coin = gpItemManager->makeObjAppeared(0x2000000E);
				if (coin) {
					coin->mPosition.set(mPosition);
					f32 rx = MsRandF();
					f32 ry = MsRandF();
					coin->mVelocity.set(10.0f, 100.0f * ry + 10.0f,
					                    10.0f * rx - 5.0f);
					coin->offLiveFlag(LIVE_FLAG_UNK10);
				}
			}
			unk1A0 = 0;
		}
	}

	mPosition.y = unk170 + mInitialPosition.y + mYOffset;
	unk190      = 0;

	Mtx mtx;
	Mtx yRot;
	MtxPtr ptr  = mtx;
	mRotation.z = unk154;
	MsMtxSetRotZ(ptr, mRotation.z);
	if (mRotation.y != 0.0f) {
		MsMtxSetRotZ(ptr, mRotation.z);
		MsMtxSetRotY(yRot, mRotation.y);
		MTXConcat(yRot, ptr, ptr);
	} else {
		MsMtxSetRotZ(ptr, mRotation.z);
	}

	ptr[0][3] = mPosition.x;
	ptr[1][3] = mPosition.y;
	ptr[2][3] = mPosition.z;
	ptr[1][3] -= mYOffset;
	MTXCopy(ptr, getModel()->getAnmMtx(0));
}

void TBellWatermill::loadAfter()
{
	TMapObjTurn::loadAfter();
	unk150    = 2;
	unk15C    = -0.02f;
	unk160    = -0.008f;
	unk164    = 10.0f;
	unk18C    = 10.0f;
	unk174    = 1000.0f;
	unk180    = 0.15f;
	unk184    = 0.1f;
	unk16C    = 4.0f;
	unk188    = 0.5f;
	unk17C    = 1.0f;
	mBells[0] = (TBiancoBell*)JDrama::TNameRefGen::search("BiaBell 0");
	mBells[1] = (TBiancoBell*)JDrama::TNameRefGen::search("BiaBell 1");
	mBells[2] = (TBiancoBell*)JDrama::TNameRefGen::search("BiaBell 2");
	unk1A0    = 1;
}

TBellWatermill::TBellWatermill(const char* name)
    : TMapObjTurn(name)
    , unk16C(0.0f)
    , unk170(0.0f)
    , unk174(0.0f)
    , unk178(0.0f)
    , unk17C(0.0f)
    , unk180(0.0f)
    , unk184(0.0f)
    , unk188(0.0f)
    , unk18C(0.0f)
    , unk190(0)
    , unk1A0(0)
    , unk1A4(nullptr)
{
}

// TWoodLog -------------------------------------------------------------------

void TWoodLog::control()
{
	TMapObjFloatOnSea::control();

	Mtx inv;
	MTXInverse(getModel()->getAnmMtx(0), inv);
	JGeometry::TVec3<f32> marioPos;
	marioPos.set(SMS_GetMarioPos());
	JGeometry::TVec3<f32> local;
	MTXMultVec(inv, &marioPos, &local);

	if (SMS_IsMarioStatusTypeSwimming() && -232.0f < local.y
	    && -141.0f < local.x && local.x < 141.0f && -441.0f < local.z
	    && local.z < 441.0f) {
		if (local.x > 0.0f)
			local.x = 141.0f;
		else
			local.x = -141.0f;

		JGeometry::TVec3<f32> world;
		MTXMultVec(getModel()->getAnmMtx(0), &local, &world);
		SMS_MarioMoveRequest(world);
	}
}
