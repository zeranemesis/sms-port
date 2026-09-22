#include <Enemy/Rocket.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/Graph.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JGeometry/JGPosition3.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionData.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Player/WaterGun.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/ObjManager.hpp>
#include <Strategic/Spine.hpp>
#include <MarioUtil/EffectUtil.hpp>
#include <System/MarDirector.hpp>
#include <System/MarioGamePad.hpp>
#include <System/Particles.hpp>
#include <System/ParamInst.hpp>

// rogue includes needed for matching sinit & bss
#include <M3DUtil/InfectiousStrings.hpp>
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

f32 TRocket::mTestAng_x     = 0.0f;
f32 TRocket::mTestAng_y     = 90.0f;
f32 TRocket::mTestAng_z     = 0.0f;
f32 TRocket::mNozzleOffsetZ = 25.0f;
f32 TRocket::mColOffsetY    = 20.0f;

TRocketSaveLoadParams::TRocketSaveLoadParams(const char* path)
    : TSmallEnemyParams(path)
    , PARAM_INIT(mSLReleaseSpeed, 10.0f)
    , PARAM_INIT(mSLFlyGravity, 0.0f)
    , PARAM_INIT(mSLFlyLimitTime, 300)
{
	TParams::load(mPrmPath);
}

TRocketManager::TRocketManager(const char* name)
    : TSmallEnemyManager(name)
    , mCanPossess(true)
    , unk64(0)
    , mExpWaterEmitInfo(nullptr)
{
}

void TRocketManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TRocketSaveLoadParams("/enemy/rocket.prm");
	mExpWaterEmitInfo = new TWaterEmitInfo("/enemy/rocketexpwater.prm");
}

void TRocketManager::loadAfter() { JDrama::TNameRef::loadAfter(); }

TSpineEnemy* TRocketManager::createEnemyInstance() { return new TRocket; }

void TRocketManager::initSetEnemies()
{
	for (int i = 0; i < mObjNum; ++i) {
		TGraphWeb* graph = gpConductor->getGraphByName("main");
		TRocket* rocket  = (TRocket*)getObj(i);
		if (rocket->checkLiveFlag(LIVE_FLAG_DEAD) && !graph->isDummy()) {
			int index = TMsRange<s32>(0, graph->getNodeNum()).rand();
			JGeometry::TVec3<f32> point;
			graph->getGraphNode(index).getPoint(&point);
			rocket->mPosition = point;
			rocket->mPosition.y += 5.0f;
			rocket->onLiveFlag(LIVE_FLAG_AIRBORNE);
			rocket->reset();
		}
	}
}

void TRocketManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "rocket.bmd",
		  J3DMLF_MaterialPEFull | (4 << J3DMLF_TevStageNumShift), 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TRocketManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & 1) {
		for (int i = 0; i < getActiveObjNum(); ++i) {
			TSpineEnemy* rocket = getObj(i);
			if (rocket->checkLiveFlag(LIVE_FLAG_DEAD))
				rocket->reset();
		}
	}

	TEnemyManager::perform(cue, graphics);
}

TRocket::TRocket(const char* name)
    : TSmallEnemy(name)
    , mIsPossessed(false)
    , mHasInitPos(false)
{
}

void TRocket::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	mInitPos    = mPosition;
	mHasInitPos = true;
	reset();
}

void TRocket::init(TLiveManager* manager)
{
	TSmallEnemy::init(manager);
	mActorType = 0x1000002b;
	unk150     = 0x11;
	mParams    = (TRocketSaveLoadParams*)getSaveParam();
	mSpine->initWith(&TNerveRocketWait::theNerve());
	onHitFlag(HIT_FLAG_UNK8000000);
}

void TRocket::calcRootMatrix()
{
	if (mIsPossessed) {
		getModel()->setBaseScale(mScaling);

		JGeometry::TPosition3<JGeometry::TMatrix34<JGeometry::SMatrix34C<f32> > >
		    mtx;
		if (mSpine->getCurrentNerve() == &TNerveRocketFly::theNerve()) {
			mtx.translation(mPosition.x, mPosition.y, mPosition.z);
		} else {
			MTXCopy(SMS_GetMarioWaterGun()->getEmitMtx(0), mtx);

			// TODO: the zero checks below look like they check the "wrong"
			// scale, but that is what the target does. Probably an inline.
			JGeometry::TVec3<f32> xDir;
			mtx.getXDir(xDir);
			f32 scaleX = xDir.length();
			JGeometry::TVec3<f32> yDir;
			mtx.getYDir(yDir);
			f32 scaleY = yDir.length();
			JGeometry::TVec3<f32> zDir;
			mtx.getZDir(zDir);
			f32 scaleZ = zDir.length();

			if (0.0f != scaleZ) {
				mtx.ref(0, 0) /= scaleX;
				mtx.ref(1, 0) /= scaleX;
				mtx.ref(2, 0) /= scaleX;
			}
			if (0.0f != scaleX) {
				mtx.ref(0, 1) /= scaleY;
				mtx.ref(1, 1) /= scaleY;
				mtx.ref(2, 1) /= scaleY;
			}
			if (0.0f != scaleY) {
				mtx.ref(0, 2) /= scaleZ;
				mtx.ref(1, 2) /= scaleZ;
				mtx.ref(2, 2) /= scaleZ;
			}

			JGeometry::TPosition3<
			    JGeometry::TMatrix34<JGeometry::SMatrix34C<f32> > >
			    offset;
			offset.translation(mNozzleOffsetZ, 0.0f, 0.0f);
			MTXConcat(mtx, offset, mtx);
			mPosition.x = mtx.at(0, 3);
			mPosition.y = mtx.at(1, 3) - mColOffsetY;
			mPosition.z = mtx.at(2, 3);
		}

		Mtx rot;
		MsMtxSetRotRPH(rot, mTestAng_x, mTestAng_y, mTestAng_z);
		MTXConcat(mtx, rot, mtx);
		MTXCopy(mtx, getModel()->getBaseTRMtx());
	} else {
		TSpineEnemy::calcRootMatrix();
	}

	if (isBckAnm(1))
		SMSGetMSound()->startSoundActor(MSD_SE_PO_PETBOTTLE_FLY, &mPosition, 0,
		                                nullptr, 0, 4);
}

void TRocket::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("rocket.bmd", 3);
}

void TRocket::reset()
{
	mIsPossessed = false;
	TSmallEnemy::reset();
	if (mHasInitPos)
		mPosition = mInitPos;
	onLiveFlag(LIVE_FLAG_UNK10);
	offLiveFlag(LIVE_FLAG_UNK800);
	onLiveFlag(LIVE_FLAG_UNK8);
	mSpine->initWith(&TNerveRocketWait::theNerve());
}

void TRocket::attackToMario()
{
	if (mSpine->getCurrentNerve() == &TNerveRocketWait::theNerve()
	    && ((TRocketManager*)mManager)->mCanPossess)
		mSpine->pushNerve(&TNerveRocketPossessedNozzle::theNerve());
}

void TRocket::behaveToWater(THitActor*) { attackToMario(); }

void TRocket::bind()
{
	if (checkLiveFlag(LIVE_FLAG_UNK10))
		return;

	if (mSpine->getCurrentNerve() == &TNerveRocketPossessedNozzle::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveRocketFly::theNerve()) {
		TBGWallCheckRecord record(mPosition.x, mPosition.y, mPosition.z,
		                          mBodyScale * mWallRadius, 1, 0);
		if (gpMap->isTouchedWallsAndMoveXZ(&record)) {
			if (record.mResultWalls[0]->getActor())
				((THitActor*)record.mResultWalls[0]->getActor())
				    ->receiveMessage(this, HIT_MESSAGE_ATTACK);
			kill();
		} else if (mSpine->getCurrentNerve()
		           == &TNerveRocketFly::theNerve()) {
			TLiveActor::bind();
			if (!isAirborne()) {
				if (mGroundPlane->getActor())
					((THitActor*)mGroundPlane->getActor())
					    ->receiveMessage(this, HIT_MESSAGE_ATTACK);
				kill();
			}
		}
	} else {
		TLiveActor::bind();
	}
}

void TRocket::setDeadAnm()
{
	TRocketManager* manager   = (TRocketManager*)mManager;
	JGeometry::TVec3<f32> pos = mPosition;
	// TODO: the target also reads mExpWaterEmitInfo->mDir here and throws
	// the value away, probably through some inline
	manager->mExpWaterEmitInfo->mPos.value = pos;
	gpModelWaterManager->emitRequest(*manager->mExpWaterEmitInfo);

	if (mIsPossessed)
		releaseNozzle();

	onLiveFlag(LIVE_FLAG_UNK20000);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0xC1, mMActor->getModel()->getBaseTRMtx(), 0, nullptr);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0xC2, mMActor->getModel()->getBaseTRMtx(), 0, nullptr);
}

f32 TRocket::getGravityY() const
{
	f32 gravity = mGravity;
	if (mSpine->getCurrentNerve() == &TNerveRocketFly::theNerve())
		gravity = mParams->mSLFlyGravity.get();
	return gravity;
}

bool TRocket::isCollidMove(THitActor* actor)
{
	if (mSpine->getCurrentNerve() == &TNerveRocketFly::theNerve()) {
		if (actor->checkActorType(ACTOR_TYPE_BOSS)
		    || actor->isActorType(0x1000001f)) {
			if (actor->receiveMessage(this, HIT_MESSAGE_TRAMPLE))
				kill();
		}
	}
	return false;
}

void TRocket::possessedNozzle()
{
	((TRocketManager*)mManager)->mCanPossess = false;
	offLiveFlag(LIVE_FLAG_UNK10);
	mIsPossessed = true;
}

void TRocket::releaseNozzle()
{
	((TRocketManager*)mManager)->mCanPossess = true;
	mIsPossessed                             = false;
}

bool TRocket::checkTrigger()
{
	SMS_SendMessageToMario(this, HIT_MESSAGE_UNK5);

	if ((u8)gpMarDirector->unk18[0]->mCompSPos[3] > 20 && mHitPoints > 1)
		--mHitPoints;

	if (!isBckAnm(2)) {
		SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_PO_WATER_FULL, 0, nullptr,
		                                   0);
		setBckAnm(2);
	}

	if (gpMarDirector->unk18[0]->checkFrameMeaning(TMarioGamePad::MEANING_R)) {
		unk190 = 2.0f;
		expandCollision();
		SMSGetMSound()->startSoundActor(MSD_SE_PO_PETBOTTLE_FLY, &mPosition, 0,
		                                nullptr, 0, 4);
		SMSRumbleMgr->start(0x15, 5, (f32*)nullptr);
		return true;
	}

	return false;
}

// TODO: incorrect size (0x6C vs 0x80 in the map), the split between this and
// TNerveRocketFly::execute is a guess
void TRocket::flyBehavior()
{
	JGeometry::TVec3<f32> velocity = mVelocity;
	mRotation.x                    = MsGetRotFromZaxis(velocity).x;
	gpMarioParticleManager->emitAndBindToPosPtr(0x179, &mPosition, 1, this);
}

bool TRocket::isAttack()
{
	return mSpine->getCurrentNerve() == &TNerveRocketFly::theNerve() ? true
	                                                                 : false;
}

static const char* rocket_bastable[] = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};

const char** TRocket::getBasNameTable() const { return rocket_bastable; }

DEFINE_NERVE(TNerveRocketPossessedNozzle, TLiveActor)
{
	TRocket* self = (TRocket*)spine->getBody();
	if (spine->getTime() == 0) {
		SMSRumbleMgr->start(0x15, 10, (f32*)nullptr);
		SMSGetMSound()->startSoundActor(MSD_SE_MA_GET_ITEM, &self->mPosition,
		                                0, nullptr, 0, 4);
		SMSGetMSound()->startSoundActor(MSD_SE_PO_GET_PETBOTTLE,
		                                &self->mPosition, 0, nullptr, 0, 4);
		self->possessedNozzle();
		self->setBckAnm(0);
	}

	if (self->checkTrigger()) {
		spine->pushAfterCurrent(&TNerveRocketFly::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveRocketFly, TLiveActor)
{
	TRocket* self = (TRocket*)spine->getBody();
	if (spine->getTime() == 0) {
		self->setBckAnm(1);
		MtxPtr mtx = SMS_GetMarioWaterGun()->getEmitMtx(0);
		f32 speed  = self->mParams->mSLReleaseSpeed.get();
		JGeometry::TVec3<f32> velocity;
		velocity.x      = speed * mtx[0][0];
		velocity.y      = speed * mtx[1][0];
		velocity.z      = speed * mtx[2][0];
		self->mVelocity = velocity;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->releaseNozzle();
		f32 angle = MsGetRotFromZaxisY(velocity);
		self->mRotation.set(0.0f, MsAngleWrap(angle), 0.0f);
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
	}

	if (!self->isBckAnm(1))
		self->setBckAnm(1);

	self->flyBehavior();

	if (self->mSpine->getTime() > self->mParams->mSLFlyLimitTime.get())
		self->kill();

	// TODO: the result is unused, maybe some inline that got optimized out
	if (!self->checkLiveFlag(LIVE_FLAG_CLIPPED_OUT))
		self->getModel();

	return false;
}

DEFINE_NERVE(TNerveRocketWait, TLiveActor)
{
	TRocket* self = (TRocket*)spine->getBody();
	if (spine->getTime() == 0) {
		self->onLiveFlag(LIVE_FLAG_UNK10);
		self->setBckAnm(3);
	}
	return false;
}
