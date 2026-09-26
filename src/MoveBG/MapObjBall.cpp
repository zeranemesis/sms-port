
#include <MoveBG/MapObjBall.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionData.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MoveBG/Item.hpp>
#include <System/FlagManager.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

u32 TResetFruit::mFruitLivingTime       = 0x3840;
f32 TResetFruit::mScaleUpSpeed           = 1.05f;
f32 TResetFruit::mBreakingScaleSpeed     = 0.96f;
u32 TResetFruit::mFruitWaitTimeToAppear  = 0x168;

TMapObjBall::TMapObjBall(const char* name)
	: TMapObjGeneral(name)
{
	unk148 = 0.0f;
	unk14C = 0.0f;
	unk150 = 0.0f;
	unk154 = 0.0f;
	unk158 = 0.0f;
	unk15C = 0.0f;
	unk160 = 0.0f;
	unk164 = 0.0f;
	unk168 = 0.0f;
	unk16C = 0.0f;
	unk170 = 0.0f;
	unk174 = 0.0f;
	unk178 = 0.0f;
	unk17C = 0.0f;
	unk180 = 0.0f;
	unk184 = 0.0f;
	unk188 = 0.0f;
	unk18C = 0.0f;
	unk190 = 0.0f;
	unk194 = 0;
	mInitialScaling.z = 0.0f;
	mInitialScaling.y = 0.0f;
	mInitialScaling.x = 0.0f;
}

TMapObjBall::~TMapObjBall() { }

void TMapObjBall::touchWaterSurface() { kill(); }

void TMapObjBall::touchPollution() { kill(); }

void TMapObjBall::put()
{
	TMapObjGeneral::put();
	calcCurrentMtx();
}

void TMapObjBall::touchRoof(JGeometry::TVec3<f32>* velocity)
{
	if (velocity->y > unk140)
		velocity->y = unk140;

	calcReflectingVelocity(unk13C, mMapObjData->mPhysical->unk4->unk4,
	                       &mVelocity);
}

void TMapObjBall::makeObjAppeared()
{
	TMapObjBase::makeObjAppeared();
	calcCurrentMtx();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	mtx[0][3]  = mPosition.x;
	mtx[1][3]  = mPosition.y + mBodyRadius;
	mtx[2][3]  = mPosition.z;

	if (isActorType(0x40000394) && mtx[1][1] > 0.0f)
		mtx[1][3] -= 50.0f * mtx[1][1];

	if (isActorType(0x40000392))
		mtx[1][3] -= 10.0f * (0.25f - mtx[1][1]);

	unkE8 = 0;
}

void TMapObjBall::checkWallCollision(JGeometry::TVec3<f32>* position)
{
	JGeometry::TVec3<f32> center(position->x, position->y + mBodyRadius,
	                            position->z);
	TBGWallCheckRecord record(center, mBodyRadius, 4,
	                          mMapObjData->mPhysical->mWallCheckFlags);

	if (gpMap->isTouchedWallsAndMoveXZ(&record)) {
		unk138      = record.mResultWalls[0];
		position->x = record.mCenter.x;
		position->z = record.mCenter.z;
		touchWall(position, &record);
	} else {
		unk138 = nullptr;
	}
}

void TMapObjBall::makeObjDefault()
{
	TMapObjBase::makeObjDefault();
	MtxPtr mtx  = getModel()->getAnmMtx(0);
	mtx[0][3]   = mPosition.x;
	mtx[1][3]   = mPosition.y + mBodyRadius;
	mtx[2][3]   = mPosition.z;
}

void TMapObjBall::hold(TTakeActor* holder)
{
	JGeometry::TVec3<f32> velocity = mVelocity;
	if (velocity.length() <= 10.0f) {
		TMapObjGeneral::hold(holder);
		mVelocity.zero();
	}
}

void TBigWatermelon::checkWallCollision(JGeometry::TVec3<f32>* position)
{
	TMapObjGeneral::checkWallCollision(position);
}

void TBigWatermelon::touchWall(JGeometry::TVec3<f32>* position,
	                           TBGWallCheckRecord* record)
{
	TMapObjBall::touchWall(position, record);
}

void TBigWatermelon::touchGround(JGeometry::TVec3<f32>* position)
{
	TMapObjBall::touchGround(position);
}

u32 TResetFruit::getLivingTime() const { return mFruitLivingTime; }

void TResetFruit::killByTimer(int timer)
{
	startStateTimer(timer);
	onMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
	mState = 0xB;
}

void TResetFruit::thrown()
{
	TMapObjGeneral::thrown();
	mState = 0xB;
}

void TResetFruit::makeObjLiving()
{
	if (!isStateTimerEngaged()) {
		onMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
		startStateTimer(getLivingTime());
	}
	offLiveFlag(LIVE_FLAG_UNK10);
	mState = 0xB;
}

void TResetFruit::initMapObj()
{
	TMapObjBall::initMapObj();
	SMS_InitPacket_OneTevColor(getModel(), 0, GX_TEVREG0, &mRottenColor);
}

TResetFruit::TResetFruit(const char* name)
    : TMapObjBall(name)
{
	mRottingScaleSpeed = 0.0f;
	unk1A4             = 0;
	mRottenColor.r     = 0xFF;
	mRottenColor.g     = 0xFF;
	mRottenColor.b     = 0xFF;
	mRottenColor.a     = 0xFF;
}

TResetFruit::~TResetFruit() { }

TRandomFruit::TRandomFruit(const char* name)
	: TResetFruit(name)
{
	memset(unk1A8, 0, sizeof(unk1A8));
}

TRandomFruit::~TRandomFruit() { }

TBigWatermelon::TBigWatermelon(const char* name)
	: TMapObjBall(name)
{
	unk198 = 0;
	unk19C = 0;
	unk1A0 = 0.0f;
}

void TBigWatermelon::loadAfter()
{
	TMapObjGeneral::loadAfter();
	TShine* shine = static_cast<TShine*>(
	    JDrama::TNameRefGen::search("シャイン（お化けスイカ用）"));
	shine->mPosition.set(-4659.0f, 460.0f, 13620.0f);
}

void TBigWatermelon::touchWaterSurface()
{
	emitColumnWater();
	if (gpMSound->gateCheck(0x3875))
		MSoundSESystem::MSoundSE::startSoundActor(0x3875, &mPosition, 0,
		                                         nullptr, 0, 4);
	kill();
}

void TCoverFruit::loadAfter()
{
    TMapObjBase::loadAfter();
    if (TFlagManager::smInstance->getBool(0x1038B))
        makeObjDead();
}

void TCoverFruit::calcRootMatrix()
{
	if (mHolder != nullptr) {
		MtxPtr mtx = mHolder->getTakingMtx();
		PSMTXCopy(mtx, getModel()->getBaseTRMtx());
		mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);
	} else {
		f32 rotZ = mRotation.z;
		f32 y    = mPosition.y - mYOffset;
		f32 rotY = mRotation.y;
		f32 rotX = mRotation.x;
		f32 z    = mPosition.z;
		f32 x    = mPosition.x;
		J3DModel* model = getModel();
		MtxPtr mtx      = model->getBaseTRMtx();
		s16 rotZ16      = rotZ * (65536.0f / 360.0f);
		s16 rotY16      = rotY * (65536.0f / 360.0f);
		s16 rotX16      = rotX * (65536.0f / 360.0f);
		MsMtxSetXYZRPH(mtx, x, y, z, rotX16, rotY16, rotZ16);
	}
	getModel()->setBaseScale(mScaling);
}

BOOL TCoverFruit::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->isActorType(0x08000083) && message == HIT_MESSAGE_TAKE) {
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mHolder = (TTakeActor*)sender;
		return true;
	}

	if (message == HIT_MESSAGE_UNKB) {
		kill();
		TFlagManager::smInstance->setBool(true, 0x1038B);
		return true;
	}

	return false;
}
