#include <Enemy/FruitsBoat.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/Graph.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/question.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <MoveBG/MapObjWave.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorData.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/ShadowUtil.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Yoshi.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTransform.hpp>
#include <string.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

extern f32 SMSGetAnmFrameRate();

// This TU is -inline deferred: the definition order below is the reverse of
// the .text layout in marioEU.MAP.

TFruitsBoatParams::TFruitsBoatParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLMoveSpeed, 4.0f)
    , PARAM_INIT(mSLRotSpeed, 0.1f)
    , PARAM_INIT(mSLBckMoveSpeed, 0.2f)
{
	TParams::load(mPrmPath);
}

TFruitsBoat::TFruitsBoat(const char* name)
    : TSpineEnemy(name)
    , unk150(0)
    , mShadowRadiusX(800.0f)
    , mShadowRadiusZ(800.0f)
    , mBckTrack(nullptr)
    , mBckTrackCtrl(nullptr)
    , mRollAxis(1.0f, 0.0f, 0.0f)
    , mRollAngle(0.0f)
    , mRollSpeed(0.0f)
{
	onLiveFlag(LIVE_FLAG_UNK10);
}

int TFruitsBoat::setBckTrack(const char* name)
{
	MActorAnmDataEach<J3DAnmTransformKey>* bcks
	    = mManager->getMActorAnmData()->mBckAnms;
	for (int i = 0; i < bcks->getAnmNum(); ++i) {
		if (strcmp(name, bcks->getName(i)) == 0) {
			mBckTrack     = bcks->getAnmPtr(i);
			mBckTrackCtrl = new J3DFrameCtrl(0);
			mBckTrackCtrl->init(mBckTrack->getFrameMax());
			mBckTrackCtrl->setAttribute(mBckTrack->getAttribute());
			f32 speed = getSaveLoadParam()->mSLBckMoveSpeed.get();
			mBckTrackCtrl->setRate(speed * SMSGetAnmFrameRate());
			return 0;
		}
	}
	return -1;
}

void TFruitsBoat::setJumpReaction()
{
	const char* name;
	switch (getBoatType()) {
	case 0:
		name = "shipdolpic";
		break;
	case 1:
		name = "shipdolpic2";
		break;
	case 2:
		name = "shipdolpic3";
		break;
	default:
		goto end;
	}

	if (!mMActor->checkCurAnm(name, 0) || mMActor->curAnmEndsNext(0, nullptr))
		mMActor->setBck(name);

end:
	offLiveFlag(LIVE_FLAG_UNK20000);
}

void TFruitsBoat::traceBckTrack()
{
	mBckTrackCtrl->update();
	f32 frame = mBckTrackCtrl->getFrame();
	mBckTrack->setFrame(frame);

	J3DTransformInfo info0;
	mBckTrack->getTransform(0, &info0);
	J3DTransformInfo info1;
	mBckTrack->getTransform(1, &info1);

	mPosition.x = info0.mTranslate.x + info1.mTranslate.x;
	mPosition.y = info0.mTranslate.y + info1.mTranslate.y;
	mPosition.z = info0.mTranslate.z + info1.mTranslate.z;

	mRotation.x
	    = (info0.mRotation.x + info1.mRotation.x) * (360.0f / 65536.0f);
	mRotation.y
	    = (info0.mRotation.y + info1.mRotation.y) * (360.0f / 65536.0f);
	mRotation.z
	    = (info0.mRotation.z + info1.mRotation.z) * (360.0f / 65536.0f);

	mScaling.x = info0.mScale.x * info1.mScale.x;
	mScaling.y = info0.mScale.y * info1.mScale.y;
	mScaling.z = info0.mScale.z * info1.mScale.z;
}

int TFruitsBoat::getBoatType() const
{
	return ((TFruitsBoatManager*)mManager)->mBoatType;
}

Mtx* TFruitsBoat::getRootJointMtx() const
{
	return (Mtx*)mMActor->getModel()->getAnmMtx(0);
}

void TFruitsBoat::rowToCurPathNode(f32 speed)
{
	if (unk124->getGraph()->getSplineRail() ? TRUE : FALSE) {
		f32 splineSpeed = unk124->calcSplineSpeed(speed);
		unk124->traceSpline(splineSpeed);

		JGeometry::TVec3<f32> pos;
		JGeometry::TVec3<f32> rot;
		unk124->getGraph()->getSplineRail()->getPosAndRot(unk124->unk14, &pos,
		                                                  &rot);
		pos.sub(mPosition);
		mLinearVelocity.add(pos);
		mRotation.y = rot.y;
		if (splineSpeed < 0.0f)
			mRotation.y = MsAngleWrap(mRotation.y + 180.0f);
	} else {
		walkToCurPathNode(speed, mTurnSpeed, 0.0f);
	}

	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_PONPONSEN, &mPosition, 0,
	                                nullptr, 0, 4);
}

void TFruitsBoat::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);

	char managerName[256];
	stream.readString(managerName, 256);
	TLiveManager* manager
	    = static_cast<TLiveManager*>(JDrama::TNameRefGen::search(managerName));

	char graphName[256];
	stream.readString(graphName, 256);
	TGraphWeb* graph = gpConductor->getGraphByName(graphName);
	unk124->init(graph);

	mGroundPlane = TMap::getIllegalCheckData();

	init(manager);

	if (graph == nullptr || graph->isDummy()) {
		if (setBckTrack(graphName) == 0)
			mSpine->initWith(&TNerveFruitsBoatBckTrace::theNerve());
	}
}

void TFruitsBoat::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	initHitActor(0x4000007B, 1, 0xC0000000, 0.0f, 0.0f, 0.0f, 0.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);

	unk124->reset();
	goToShortestNextGraphNode();
	if (unk124->getGraph()->unk14 != nullptr) {
		mPosition = unk124->getCurrentPos();
		unk124->moveToShortestNext();
	}

	switch (getBoatType()) {
	case 0:
		mMapCollisionManager
		    = new TMapCollisionManager(1, "/scene/fruitsboat", this);
		mMActor = mMActorKeeper->createMActor("ShipDolpic.bmd", 0);
		mMapCollisionManager->init("ShipDolpic.col", 1, nullptr);
		mAttackRadius = 850.0f;
		calcEntryRadius();
		mAttackHeight = 600.0f;
		calcEntryRadius();
		break;
	case 1:
		mMapCollisionManager
		    = new TMapCollisionManager(1, "/scene/fruitsboatb", this);
		mMActor = mMActorKeeper->createMActor("ShipDolpic2.bmd", 0);
		mMapCollisionManager->init("ShipDolpic2.col", 1, nullptr);
		mAttackRadius = 750.0f;
		calcEntryRadius();
		mAttackHeight = 480.0f;
		calcEntryRadius();
		break;
	case 2:
		mMapCollisionManager
		    = new TMapCollisionManager(1, "/scene/fruitsboatc", this);
		mMActor = mMActorKeeper->createMActor("ShipDolpic3.bmd", 0);
		mMapCollisionManager->init("ShipDolpic3.col", 1, nullptr);
		mAttackRadius = 1000.0f;
		calcEntryRadius();
		mAttackHeight = 300.0f;
		calcEntryRadius();
		break;
	case 3:
	default:
		mMapCollisionManager
		    = new TMapCollisionManager(1, "/scene/fruitsboatd", this);
		mMActor = mMActorKeeper->createMActor("ShipDolpic4.bmd", 0);
		mMapCollisionManager->init("ShipDolpic4.col", 1, nullptr);
		mAttackRadius = 760.0f;
		calcEntryRadius();
		mAttackHeight = 270.0f;
		calcEntryRadius();
		break;
	}

	mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation, mScaling);

	mSpine->initWith(&TNerveFruitsBoatGraphWander::theNerve());

	if (unk124->getGraph() == nullptr) {
		onLiveFlag(LIVE_FLAG_UNK10000);
	} else if (unk124->getCurrent().getRailNode()->mFlags & 0x80) {
		onLiveFlag(LIVE_FLAG_UNK10000);
	} else {
		offLiveFlag(LIVE_FLAG_UNK10000);
	}

	mMarchSpeed = getSaveLoadParam()->mSLMoveSpeed.get();
	mTurnSpeed  = getSaveLoadParam()->mSLRotSpeed.get();

	offLiveFlag(LIVE_FLAG_CLIPPED_OUT);
	onLiveFlag(LIVE_FLAG_UNK20);
	offLiveFlag(LIVE_FLAG_UNK100);
	mMActor->setLightType(2);
	calcRootMatrix();
	mMActor->calc();
}

BOOL TFruitsBoat::receiveMessage(THitActor* sender, u32 message)
{
	return FALSE;
}

void TFruitsBoat::setGroundCollision()
{
	JGeometry::TVec3<f32> diff = mPosition;
	diff.sub(*gpMarioPos);

	if (mColCount != 0 || JGeometry::TUtil<f32>::sqrt(diff.squared()) < 1000.0f
	    || (SMS_GetYoshi()->isHatched()
	        && mPosition.x - 1000.0f < SMS_GetYoshi()->getTranslation().x
	        && mPosition.x + 1000.0f > SMS_GetYoshi()->getTranslation().x
	        && mPosition.z - 1000.0f < SMS_GetYoshi()->getTranslation().z
	        && mPosition.z + 1000.0f > SMS_GetYoshi()->getTranslation().z)) {
		MtxPtr mtx = getModel()->getAnmMtx(0);
		if (mMapCollisionManager->getUnk8())
			mMapCollisionManager->getUnk8()->moveMtx(mtx);
	}
}

void TFruitsBoat::calcRootMatrix()
{
	J3DModel* model = getModel();
	MtxPtr mtx      = model->getBaseTRMtx();
	MsMtxSetRotRPH(mtx, mRotation.x, mRotation.y, mRotation.z);

	Mtx roll;
	MTXRotAxisRad(roll, &mRollAxis, DEG_TO_RAD(mRollAngle));
	MTXConcat(roll, mtx, mtx);
	MTXTransApply(mtx, mtx, mPosition.x, mPosition.y, mPosition.z);
	model->setBaseScale(mScaling);
}

// TODO: fabricated, same shape as the helpers in RiccoHook.cpp and
// walkerEnemy.cpp
static inline JGeometry::TVec3<f32> polarXZ(f32 theta, f32 radius)
{
	f32 c = radius * MsCos(theta);
	f32 s = radius * MsSin(theta);
	return JGeometry::TVec3<f32>(s, 0.0f, c);
}

void TFruitsBoat::moveObject()
{
	// Pitch the boat along the waves: sample the wave height at the bow and
	// at the stern and aim the boat along the line joining them.
	JGeometry::TVec3<f32> front = polarXZ(mRotation.y, 300.0f);
	JGeometry::TVec3<f32> pos   = mPosition;

	JGeometry::TVec3<f32> bow = pos;
	bow += front;
	JGeometry::TVec3<f32> stern = pos;
	stern -= front;

	bow.y   = gpMapObjWave->getWaveHeight(bow.x, bow.z);
	stern.y = gpMapObjWave->getWaveHeight(stern.x, stern.z);

	JGeometry::TVec3<f32> dir = bow;
	dir.sub(stern);
	JGeometry::TVec3<f32> rot = MsGetRotFromZaxis(dir);
	rot.x *= 0.5f;

	f32 pitch = MsAngleDiff(rot.x, mRotation.x);
	if (pitch >= 0.0f)
		pitch = MsMin(pitch, 1.0f);
	else
		pitch = MsMax(pitch, -1.0f);
	mRotation.x += pitch;

	// Roll the boat when Mario lands on it.
	const TBGCheckData* plane = SMS_GetMarioGrPlane();
	if (!checkLiveFlag(LIVE_FLAG_UNK20000)) {
		if (plane != nullptr && plane->mActor == this
		    && SMS_IsMarioTouchGround4cm()) {
			JGeometry::TVec3<f32> toMario = *gpMarioPos;
			toMario -= mPosition;
			toMario.y  = 0.0f;
			f32 length = toMario.length();
			if (length != 0.0f) {
				static JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
				toMario.normalize();
				mRollAxis.cross(up, toMario);
				mRollAxis.normalize();
				mRollSpeed += 0.0003f * length;
			}

			onLiveFlag(LIVE_FLAG_UNK20000);
			offLiveFlag(LIVE_FLAG_UNK10000);
			if (plane->isBounceOnLanding())
				setJumpReaction();
		}
	} else {
		if (plane == nullptr || plane->mActor != this
		    || !SMS_IsMarioTouchGround4cm())
			offLiveFlag(LIVE_FLAG_UNK20000);
	}

	// Slowly turn the roll axis to face Mario while he stands on the boat.
	if (checkLiveFlag(LIVE_FLAG_UNK20000)) {
		JGeometry::TVec3<f32> toMario = *gpMarioPos;
		toMario -= mPosition;
		toMario.y = 0.0f;
		if (toMario.length() != 0.0f) {
			toMario.normalize();
			static JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
			JGeometry::TVec3<f32> axis;
			axis.cross(up, toMario);
			axis.normalize();
			mRollAxis.x += 0.1f * (axis.x - mRollAxis.x);
			mRollAxis.y += 0.1f * (axis.y - mRollAxis.y);
			mRollAxis.z += 0.1f * (axis.z - mRollAxis.z);
		}
	}

	mRollSpeed += 0.01f * -MsSin(mRollAngle);
	mRollAngle += mRollSpeed;
	if (mRollAngle < -8.0f) {
		mRollAngle = -8.0f;
		mRollSpeed = -mRollSpeed;
	} else if (mRollAngle > 8.0f) {
		mRollAngle = 8.0f;
		mRollSpeed = -mRollSpeed;
	}
	mRollSpeed *= 0.99f;

	TLiveActor::moveObject();
}

void TFruitsBoat::requestShadow()
{
	if (checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8))
		return;

	if (!checkLiveFlag(LIVE_FLAG_UNK200 | LIVE_FLAG_CLIPPED_OUT)
	    || checkLiveFlag(LIVE_FLAG_UNK400)) {
		TCircleShadowRequest request;
		request.mPosition   = mPosition;
		request.mRadiusX    = mShadowRadiusX;
		request.mRadiusZ    = mShadowRadiusZ;
		request.mShadowType = SHADOW_TYPE_SHIP;
		request.mRotationY  = (s16)mRotation.y;
		if (checkLiveFlag(LIVE_FLAG_UNK400))
			gpBindShadowManager->forceRequest(request, getActorType());
		else
			gpBindShadowManager->request(request, getActorType());
	}

	if (!checkLiveFlag(LIVE_FLAG_UNK200 | LIVE_FLAG_CLIPPED_OUT)
	    && !checkActorType(ACTOR_TYPE_UNK40000000))
		gpQuestionManager->request(mPosition, mScaledBodyRadius);
}

TFruitsBoatManager::TFruitsBoatManager(int boat_type, const char* name)
    : TEnemyManager(name)
    , mBoatType(boat_type)
{
}

void TFruitsBoatManager::createModelData()
{
	switch (mBoatType) {
	case 0: {
		static const TModelDataLoadEntry entry[]
		    = { { "ShipDolpic.bmd", 0x10210000, 0 }, { 0 } };
		createModelDataArray(entry);
		break;
	}
	case 1: {
		static const TModelDataLoadEntry entry[]
		    = { { "ShipDolpic2.bmd", 0x10210000, 0 }, { 0 } };
		createModelDataArray(entry);
		break;
	}
	case 2: {
		static const TModelDataLoadEntry entry[]
		    = { { "ShipDolpic3.bmd", 0x10210000, 0 }, { 0 } };
		createModelDataArray(entry);
		break;
	}
	case 3:
	default: {
		static const TModelDataLoadEntry entry[]
		    = { { "ShipDolpic4.bmd", 0x10210000, 0 }, { 0 } };
		createModelDataArray(entry);
		break;
	}
	}
}

void TFruitsBoatManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TFruitsBoatParams("/enemy/fruitsBoat.prm");
	TEnemyManager::load(stream);
}

TSpineEnemy* TFruitsBoatManager::createEnemyInstance() { return nullptr; }

DEFINE_NERVE(TNerveFruitsBoatGraphWander, TLiveActor)
{
	TFruitsBoat* self = (TFruitsBoat*)spine->getBody();

	if (self->unk124->getGraph() == nullptr
	    || self->unk124->getGraph()->isDummy())
		return FALSE;

	if (self->isReachedToGoal()) {
		TGraphNode& node = self->unk124->getCurrent();
		if (node.getRailNode()->mFlags & 0x100)
			self->onLiveFlag(LIVE_FLAG_UNK10000);
		if (node.getRailNode()->mFlags & 0x400)
			self->unk150 ^= 1;

		self->goToDirectedNextGraphNode(polarXZ(self->mRotation.y, 1.0f));
		if (!self->checkLiveFlag(LIVE_FLAG_UNK10000))
			self->rowToCurPathNode(self->mMarchSpeed);

		spine->pushAfterCurrent(&TNerveFruitsBoatGraphWander::theNerve());
		return TRUE;
	}

	if (self->checkLiveFlag(LIVE_FLAG_UNK10000))
		return FALSE;

	self->rowToCurPathNode(self->mMarchSpeed);
	return FALSE;
}

DEFINE_NERVE(TNerveFruitsBoatBckTrace, TLiveActor)
{
	TFruitsBoat* self = (TFruitsBoat*)spine->getBody();
	self->traceBckTrack();
	return FALSE;
}
