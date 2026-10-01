// rogue include: the original TU opens .rodata with this dummy string
// pair, ahead of every other string constant in the object.
#include <M3DUtil/InfectiousStrings.hpp>

#include <MoveBG/MapObjFence.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/MapObjMessenger.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/Graph.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Yoshi.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapCollisionManager.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JGeometry/JGMatrix34.hpp>
#include <PowerPC_EABI_Support/Msl/MSL_C/MSL_Common/string.h>
#include <Strategic/Strategy.hpp>
#include <math.h>

#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

const char cDirtyFileName[] = "/scene/map/pollution/H_ma_rak.bti";
const char cDirtyTexName[]  = "H_ma_rak_dummy";

// This unit is reverse_fn_order: with -inline deferred MWCC emits functions in
// the reverse of their source order.

f32 TRevolvingFenceInner::mSpeed      = 4.0f;
f32 TFenceWater::mWaterAccel          = 2.1f;
f32 TFenceWater::mBackSpeed           = 3.0f;
int TFenceWater::mTurnedWaitTime      = 600;
f32 TRailFence::mFallHeight           = 50000.0f;
int TRailFence::mWaitTime             = 240;

BOOL TFence::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SUPER_HIP_DROP) {
		startBck("fence_normal_shake");
		return true;
	}

	return false;
}

void TFence::initMapCollisionData()
{
	mMapCollisionManager = new TMapCollisionManager(1, "mapObj", this);

	if (strcmp(unkF4, "fence3x3") != 0) {
		if (fabsf(mRotation.x) < 1.0f && fabsf(mRotation.z) < 1.0f)
			mMapCollisionManager->init("fence_normal_v_tool", 0, nullptr);
		else
			mMapCollisionManager->init("fence_h_tool", 0, nullptr);
	} else {
		if (fabsf(mRotation.x) < 1.0f && fabsf(mRotation.z) < 1.0f)
			mMapCollisionManager->init("fence_half_v_tool", 0, nullptr);
		else
			mMapCollisionManager->init("fence_half_h_tool", 0, nullptr);
	}

	mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation, mScaling);
}

void TFence::initMapObj()
{
	if (strstr(unkF4, "bamboo"))
		unk138 = 1;

	TMapObjBase::initMapObj();
}

BOOL TRevolvingFenceOuter::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SUPER_HIP_DROP) {
		startBck("fence_revolve_outer_shake");
		((TFence*)unk13C)->startBck("fence_revolve_inner_shake");
		return true;
	}

	return false;
}

void TRevolvingFenceOuter::initMapCollisionData()
{
	mMapCollisionManager = new TMapCollisionManager(1, "mapObj", this);

	if (fabsf(mRotation.x) < 1.0f && fabsf(mRotation.z) < 1.0f)
		mMapCollisionManager->init("fence_revolve_outer_v_tool", 0, nullptr);
	else
		mMapCollisionManager->init("fence_revolve_outer_h_tool", 0, nullptr);

	mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation, mScaling);

	if (unk138)
		unk13C = TMapObjBaseManager::newAndRegisterObj(
		    "bambooFence_revolve_inner", mPosition, mRotation,
		    JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
	else
		unk13C = TMapObjBaseManager::newAndRegisterObj(
		    "fence_revolve_inner", mPosition, mRotation,
		    JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));

	unk13C->appear();
}

BOOL TRevolvingFenceInner::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SUPER_HIP_DROP && !unk140) {
		if (isState(1)) {
			SMSGetMSound()->startSoundActor(MSD_SE_OBJ_FENCE_REVERSE1,
			                                &mPosition, 0, nullptr, 0, 4);
			mState = 3;
			startBck("fence_revolve_inner_roll_down");
			offMapObjFlag(MAP_OBJ_FLAG_UNK100);
			return true;
		}

		if (isState(2)) {
			SMSGetMSound()->startSoundActor(MSD_SE_OBJ_FENCE_REVERSE2,
			                                &mPosition, 0, nullptr, 0, 4);
			mState = 4;
			startBck("fence_revolve_inner_roll_up");
			offMapObjFlag(MAP_OBJ_FLAG_UNK100);
			return true;
		}
	}

	if (message == HIT_MESSAGE_SUPER_HIP_DROP && unk140) {
		f32 angle
		    = MsWrap(180.0f * (getRotYFromAxisZ(*gpMarioPos) / 3.14f)
		                 + mInitialRotation.y,
		             -180.0f, 180.0f);

		if ((-180.0f < angle && angle < -90.0f)
		    || (0.0f < angle && angle < 90.0f)) {
			SMSGetMSound()->startSoundActor(MSD_SE_OBJ_FENCE_REVERSE1,
			                                &mPosition, 0, nullptr, 0, 4);
			if (isState(1))
				mState = 3;
			else
				mState = 4;
		} else {
			SMSGetMSound()->startSoundActor(MSD_SE_OBJ_FENCE_REVERSE2,
			                                &mPosition, 0, nullptr, 0, 4);
			if (isState(1))
				mState = 5;
			else
				mState = 6;
		}
		return true;
	}

	return false;
}

void TRevolvingFenceInner::calcCurrentMtx()
{
	mRotation.y = unk13C + mInitialRotation.y;
	// TODO: the result is thrown away in retail too
	MsWrap(mRotation.y, 0.0f, 360.0f);
	MtxPtr mtx = getModel()->getAnmMtx(0);
	MsMtxSetRotY(mtx, mRotation.y);
	mtx[0][3] = mPosition.x;
	mtx[1][3] = mPosition.y - mYOffset;
	mtx[2][3] = mPosition.z;
}

void TRevolvingFenceInner::controlWall()
{
	switch (mState) {
	case 3:
		unk13C += mSpeed;
		if (unk13C > 180.0f) {
			unk13C      = 180.0f;
			mRotation.y = unk13C + mInitialRotation.y;
			mState      = 2;
		}
		calcCurrentMtx();
		break;
	case 4:
		unk13C += mSpeed;
		if (unk13C > 360.0f) {
			unk13C      = 0.0f;
			mRotation.y = unk13C + mInitialRotation.y;
			mState      = 1;
		}
		calcCurrentMtx();
		break;
	case 5:
		unk13C -= mSpeed;
		if (unk13C < -180.0f) {
			unk13C      = 180.0f;
			mRotation.y = unk13C + mInitialRotation.y;
			mState      = 2;
		}
		calcCurrentMtx();
		break;
	case 6:
		unk13C -= mSpeed;
		if (unk13C < 0.0f) {
			unk13C      = 0.0f;
			mRotation.y = unk13C + mInitialRotation.y;
			mState      = 1;
		}
		calcCurrentMtx();
		break;
	}
}

void TRevolvingFenceInner::controlGroundRoof()
{
	// TODO: still 96.6%. The ROM's switch tree splits at 4
	// (cmpwi 4 / beq / bge / cmpwi 3 / bge / b / cmpwi 6 / beq / bge) while ours
	// splits at 5 and emits a `cmpwi r0, 7` upper-bound test. Both encode the
	// same mapping ({4,6}->1, {3,5}->2); only the decision-tree shape differs.
	// Listing the {4,6} group first is what got us here from 95.1%.
	switch (mState) {
	case 4:
	case 6:
		if (getMActor()->curAnmEndsNext()) {
			mState = 1;
			getMActor()->setFrameRate(0.0f, 0);
			getMActor()->getFrameCtrl(0)->setFrame(0.0f);
			getMActor()->calc();
			onMapObjFlag(MAP_OBJ_FLAG_UNK100);
		}
		break;
	case 3:
	case 5:
		if (getMActor()->curAnmEndsNext()) {
			mState = 2;
			getMActor()->setFrameRate(0.0f, 0);
			getMActor()->getFrameCtrl(0)->setFrame(0.0f);
			getMActor()->calc();
			onMapObjFlag(MAP_OBJ_FLAG_UNK100);
		}
		break;
	}
}

void TRevolvingFenceInner::setGroundCollision()
{
	if (SMS_GetYoshi()->isHatched()
	    && mPosition.x - mBodyRadius < SMS_GetYoshi()->getTranslation().x
	    && mPosition.x + mBodyRadius > SMS_GetYoshi()->getTranslation().x
	    && mPosition.z - mBodyRadius < SMS_GetYoshi()->getTranslation().z
	    && mPosition.z + mBodyRadius > SMS_GetYoshi()->getTranslation().z) {
		TMtx34f mtx;
		mtx.set(getModel()->getAnmMtx(0));
		if (mMapCollisionManager->getUnk8())
			mMapCollisionManager->getUnk8()->moveMtx(mtx);
	}

	TMapObjBase::setGroundCollision();
}

void TRevolvingFenceInner::control()
{
	TMapObjBase::control();

	if (unk140)
		controlWall();
	else
		controlGroundRoof();
}

void TRevolvingFenceInner::initMapCollisionData()
{
	mMapCollisionManager = new TMapCollisionManager(1, "mapObj", this);

	if (fabsf(mRotation.x) < 80.0f && fabsf(mRotation.z) < 80.0f)
		mMapCollisionManager->init("fence_revolve_inner_v_tool", 1, nullptr);
	else
		mMapCollisionManager->init("fence_revolve_inner_h_tool", 1, nullptr);
}

void TRevolvingFenceInner::initMapObj()
{
	TFence::initMapObj();

	if (fabsf(mRotation.x) < 1.0f && fabsf(mRotation.z) < 1.0f)
		unk140 = 1;
	else
		unk140 = 0;

	mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation, mScaling);
}

void TFenceWater::draw() const { }

BOOL TFenceWater::receiveMessage(THitActor* sender, u32 message)
{
	if (!isState(3) && message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		unk13C = mWaterAccel;
		if (unk13C > 0.0f)
			changeStatusToGo();
		return true;
	}

	return false;
}

void TFenceWater::changeStatusToGo()
{
	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_WATER_FENCE_FW, &mPosition, 0,
	                                nullptr, 0, 4);
	mState = 2;
}

void TFenceWater::changeStatusToWait()
{
	unk140 = 0.0f;
	unk13C = 0.0f;
	mState = 1;
}

void TFenceWater::controlRotation()
{
	switch (mState) {
	case 1:
		break;
	case 2:
		unk140 -= unk13C;
		if (unk140 <= -90.0f) {
			unk140 = -90.0f;
			unk13C = 0.0f;
			mState = 3;
			startStateTimer(mTurnedWaitTime);
		}
		break;
	case 3:
		if (!isStateTimerEngaged()) {
			SMSGetMSound()->startSoundActor(MSD_SE_OBJ_WATER_FENCE_REV,
			                                &mPosition, 0, nullptr, 0, 4);
			unk13C = mBackSpeed;
			mState = 4;
		}
		break;
	case 4:
		unk140 += unk13C;
		if (unk140 >= 0.0f)
			changeStatusToWait();
		break;
	}
}

void TFenceWater::control()
{
	TMapObjBase::control();
	controlRotation();

	mRotation.y = MsWrap(unk140 + mInitialRotation.y, 0.0f, 360.0f);
	unk144->mPosition.x = 500.0f * MsCos(mRotation.y) + mPosition.x;
	unk144->mPosition.z = mPosition.z - 500.0f * MsSin(mRotation.y);
}

void TFenceWater::initMapCollisionData() { TMapObjBase::initMapCollisionData(); }

void TFenceWater::initMapObj()
{
	TFence::initMapObj();

	unk144        = new TMapObjMessenger("地形オブジェメッセンジャー");
	unk144->unk68 = this;
	unk144->initHitActor(mActorType, 1, 0, 0.0f, 0.0f, 100.0f, 300.0f);
	unk144->offHitFlag(HIT_FLAG_NO_COLLISION);
	unk144->mPosition.set(mPosition.x, mPosition.y - 150.0f, mPosition.z);

	static_cast<TIdxGroupObj*>(
	    JDrama::TNameRefGen::search("オブジェクトグループ"))
	    ->getChildren()
	    .push_back(unk144);
}

// fabricated
// TODO: find the real inline this came from; it builds a ZYX euler rotation
// out of degrees with sinf/cosf
static inline void setEulerRotate(TMtx34f& mtx, f32 x, f32 y, f32 z)
{
	mtx.identity();

	f32 sx = sinf(0.017453294f * x);
	f32 sy = sinf(0.017453294f * y);
	f32 sz = sinf(0.017453294f * z);
	f32 cx = cosf(0.017453294f * x);
	f32 cy = cosf(0.017453294f * y);
	f32 cz = cosf(0.017453294f * z);

	mtx.ref(0, 0) = cy * cz;
	mtx.ref(1, 0) = cy * sz;
	mtx.ref(2, 0) = -sy;
	mtx.ref(0, 1) = sx * sy * cz - cx * sz;
	mtx.ref(1, 1) = sx * sy * sz + cx * cz;
	mtx.ref(2, 1) = sx * cy;
	mtx.ref(0, 2) = cx * cz * sy + sx * sz;
	mtx.ref(1, 2) = cx * sz * sy - sx * cz;
	mtx.ref(2, 2) = cx * cy;
}

void TFenceWaterH::control()
{
	TMapObjBase::control();
	controlRotation();

	mRotation.z = MsWrap(unk140 + mInitialRotation.z, 0.0f, 360.0f);

	TMtx34f rotY;
	setEulerRotate(rotY, 0.0f, mRotation.y, 0.0f);
	TMtx34f rotZ;
	setEulerRotate(rotZ, 0.0f, 0.0f, mRotation.z);
	MTXConcat(rotY, rotZ, rotY);

	rotY.ref(0, 3) = mPosition.x;
	rotY.ref(1, 3) = mPosition.y;
	rotY.ref(2, 3) = mPosition.z;
	MTXCopy(rotY, getModel()->getAnmMtx(0));

	unk144->mPosition.x = mPosition.x;
	unk144->mPosition.y = mPosition.y - 150.0f;
}

void TFenceWaterH::changeStatusToGo()
{
	TFenceWater::changeStatusToGo();
	setUpMapCollision(1);
}

void TFenceWaterH::changeStatusToWait()
{
	TFenceWater::changeStatusToWait();
	setUpMapCollision(0);
}

BOOL TRailFence::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SUPER_HIP_DROP) {
		SMSGetMSound()->startSoundActor(MSD_SE_OBJ_MVING_FENCT_PNCH,
		                                &mPosition, 0, nullptr, 0, 4);
		setUpMapCollision(1);
		offMapObjFlag(MAP_OBJ_FLAG_UNK100);
		mState = 2;
		return true;
	}

	return false;
}

void TRailFence::falling()
{
	JGeometry::TVec3<f32> velocity = mVelocity;
	mPosition.y += velocity.y;
	mVelocity.y -= mGravity;
	if (mVelocity.y < -100.0f)
		mVelocity.y = -100.0f;

	if (mPosition.y < mInitialPosition.y - mFallHeight) {
		mPosition.x = mInitialPosition.x;
		mPosition.y = mInitialPosition.y;
		mPosition.z = mInitialPosition.z;
		setUpMapCollision(0);
		unk13C->setToNearest(mPosition);
		makeObjAppeared();
		calcRootMatrix();
		getModel()->calc();
		onMapObjFlag(MAP_OBJ_FLAG_UNK100);
	}
}

void TRailFence::goOnRail()
{
	if (!unk13C->getGraph())
		return;

	JGeometry::TVec3<f32> dir = unk13C->getCurrentPos();
	dir -= mPosition;

	if (dir.squared() < 50.0f) {
		TGraphTracer* tracer = unk13C;
		const TRailNode* node = tracer->getCurrent().getRailNode();
		if (node->mConnectionNum == 0 && (node->mFlags & 8)) {
			SMSGetMSound()->startSoundActor(MSD_SE_OBJ_MVING_FENCT_SET,
			                                &mPosition, 0, nullptr, 0, 4);
			startStateTimer(mWaitTime);
			startAnim(1);
			mState = 3;
			return;
		}

		tracer->moveToShortestNext();
		dir.set(unk13C->getCurrentPos());
	}

	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_MVING_FENCE_MOVE, &mPosition, 0,
	                                nullptr, 0, 4);
	VECNormalize(&dir, &dir);
	dir.scale(unk140);
	mLinearVelocity += dir;
}

void TRailFence::control()
{
	TMapObjBase::control();

	switch (mState) {
	case 1:
		break;
	case 2:
		goOnRail();
		break;
	case 3:
		if (!isStateTimerEngaged()) {
			removeMapCollision();
			SMSGetMSound()->startSoundActor(MSD_SE_OBJ_SUPERBLOCK_BREAK,
			                                &mPosition, 0, nullptr, 0, 4);
			mState = 4;
		}
		break;
	case 4:
		falling();
		break;
	}
}

void TRailFence::initMapCollisionData() { TMapObjBase::initMapCollisionData(); }

void TRailFence::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);

	char graphName[0x40];
	stream.readString(graphName, 0x40);
	TGraphWeb* graph = gpConductor->getGraphByName(graphName);
	if (graph && !graph->isDummy()) {
		unk13C->setGraph(graph);
		unk13C->setToNearest(mPosition);
	}

	unk140   = 8.0f;
	mGravity = 0.3f;
}

TRailFence::TRailFence(const char* name)
    : TFence(name)
{
	unk13C = new TGraphTracer;
	unk140 = 0.0f;
}
