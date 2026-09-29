
#include <Enemy/Igaiga.hpp>
#include <Enemy/Walker.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/AreaCylinder.hpp>
#include <Enemy/Conductor.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/MapEventSink.hpp>
#include <Map/MapMirror.hpp>
#include <MoveBG/MapObjBianco.hpp>
#include <Strategic/MirrorActor.hpp>
#include <Map/PollutionManager.hpp>
#include <MoveBG/ItemManager.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/SharedParts.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/MtxUtil.hpp>
#include <MarioUtil/TexUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <System/Particles.hpp>
#include <System/EmitterViewObj.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/JMath.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <M3DUtil/InfectiousStrings.hpp>
#include <Player/MarioAccess.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>


static const char* igaiga_bastable[] = {
	"/scene/igaiga/bas/igaiga_down1.bas",
	"/scene/igaiga/bas/igaiga_down2.bas",
	nullptr,
	nullptr,
	"/scene/igaiga/bas/igaiga_shoot1.bas",
	"/scene/igaiga/bas/igaiga_waterdown1.bas",
	"/scene/igaiga/bas/igaiga_waterhit1.bas",
	nullptr,
};

static const char* gorogoro_bastable[] = { nullptr, nullptr, nullptr, nullptr };

TRollEnemy* gpCurRollEnemy;

f32 TRollEnemy::mBoundVal = 80.0f;
f32 TIgaiga::mReachNodeDist = 300.0f;
f32 TRollEnemy::mTransYOffset;

TRollEnemySaveLoadParams::TRollEnemySaveLoadParams(const char* path)
    : TWalkerEnemyParams(path)
    , PARAM_INIT(mSLGenerateInterval, 300)
    , PARAM_INIT(mSLExpandRate, 1.0f)
    , PARAM_INIT(mSLExpandMax, 1.5f)
    , PARAM_INIT(mSLBoundVYMax, 15.0f)
    , PARAM_INIT(mSLGroundOffsetY, 150.0f)
{
	TParams::load(mPrmPath);
}

TRollEnemy::TRollEnemy(const char* name)
    : TWalkerEnemy(name)
    , unk194(0.0f)
    , unk198(0.0f)
    , unk19C(0.0f)
    , unk1A0(0.0f)
    , unk1A4(nullptr)
    , unk1A8(0)
    , unk1AC(0.0f)
    , unk1B0(1.0f)
{
}

void TRollEnemy::reset()
{
	gpCurRollEnemy = this;
	TWalkerEnemy::reset();
	unk194 = TMsRange<f32>(0.0f, 360.0f).rand();
	unk158 = 1.0f;

	JGeometry::TVec3<f32> pos;
	getTracer()->getGraph()->getGraphNode(0).getPoint(&pos);
	mPosition = pos;
	mPosition.y += 10.0f;

	getTracer()->getGraph()->getGraphNode(1).getPoint(&pos);
	JGeometry::TVec3<f32> dir(pos.x - mPosition.x, 0.0f, pos.z - mPosition.z);
	mRotation.y = MsWrap(MsGetRotFromZaxisY(dir), 0.0f, 360.0f);

	unk198 = 1.5f * mMarchSpeed;
	unk19C = mMarchSpeed;
	unk1A0 = 0.0f;
	getTracer()->mCurrIdx = 0;
}

void TRollEnemy::walkBehavior(int param_1, f32 param_2)
{
	if (!unk1A8)
		TWalkerEnemy::walkBehavior(param_1, param_2);

	if (isAirborne() && mPosition.y > mGroundHeight + 20.0f) {
		f32 height = (mPosition.y - mGroundHeight) / mBoundVal;
		f32 result = MsWrap(height, 0.0f, unk1A4->mSLBoundVYMax.get());
		if (unk1A0 < result)
			unk1A0 = result;
	} else if (!mGroundPlane->isWaterSurface()) {
		unk1A8 = 0;
		if (unk1A0 > unk1B0) {
			bound();
			mVelocity = JGeometry::TVec3<f32>(0.0f, unk1A0, 0.0f);
			onLiveFlag(LIVE_FLAG_AIRBORNE);
			mPosition.y += 5.0f;
			unk1A0 = 0.0f;
			boundSE();
		}
	}

	if (mPosition.y < mGroundHeight + 30.0f)
		rollSE();

	if (unk128 > 300) {
		onLiveFlag(LIVE_FLAG_MELT_ON_DEATH);
		kill();
	}
}

void TRollEnemy::behaveToWater(THitActor*)
{
	mSprayedByWaterCooldown = 0;
	if (unk158 < unk1A4->mSLExpandMax.get()) {
		f32 rate = unk1A4->mSLExpandRate.get();
		mBodyScale *= rate;
		unk158 *= rate;
		mScaledBodyRadius *= rate;
		mScaling.x = mScaling.y = mScaling.z = mScaling.z * rate;

		f32 attackRadius = getSaveParams()->getSLAttackRadius();
		f32 attackHeight = getSaveParams()->getSLAttackHeight();
		f32 damageRadius = getSaveParams()->getSLDamageRadius();
		f32 damageHeight = getSaveParams()->getSLDamageHeight();

		attackRadius *= mBodyScale / unk154;
		attackHeight *= mBodyScale / unk154;
		damageRadius *= mBodyScale / unk154;
		damageHeight *= mBodyScale / unk154;
		setHitParams(attackRadius, attackHeight, damageRadius, damageHeight);
	}
}

void TRollEnemy::attackToMario() { SMS_SendMessageToMario(this, 0xE); }

void TRollEnemy::flagJump()
{
	JGeometry::TVec3<f32> goal;
	getTracer()->getCurrent().getPoint(&goal);
	mPosition.y += 30.0f;
	f32 speed = unk124->unkC;
	JGeometry::TVec3<f32> vel
	    = calcVelocityToJumpToY(goal, speed, getGravityY());
	unk1A8    = 1;
	mVelocity = vel;
	onLiveFlag(LIVE_FLAG_AIRBORNE);
}

bool TRollEnemy::isCollidMove(THitActor* other)
{
	bool isTarget = other->mActorType == 0x4000022B ? true : false;
	if (isTarget) {
		kill();
		return true;
	}
	other->receiveMessage(this, 0xE);
	return false;
}

bool TRollEnemy::isReachedToGoalXZ()
{
	JGeometry::TVec3<f32> tmp = getUnk104().getPoint();
	tmp -= mPosition;
	if (!unk1A8)
		tmp.y = 0.0f;

	if (MsVECMag2(&tmp) < 200.0f)
		return true;
	else
		return false;
}

void TRollEnemy::setBehavior()
{
	if (mPosition.y > mGroundHeight + 50.0f)
		return;

	if (mSpine->getTime() % getSaveParam2()->mSLPolluteInterval.get() != 0)
		return;

	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return;

	if (mSpine->getCurrentNerve() == &TNerveSmallEnemyDie::theNerve())
		return;

	if (!mIsPolluter)
		return;

	f32 size = 2.0f;
	if (!checkLiveFlag(LIVE_FLAG_HIDDEN)) {
		if (!mIsAmpPolluter) {
			size = getSaveParam2()->getSLPolluteRange();
		} else {
			int rMin  = getSaveParam2()->getSLPolluteRMin();
			int rMax  = getSaveParam2()->getSLPolluteRMax();
			int cycle = getSaveParam2()->getSLPolluteCycle();
			int time  = mSpine->getTime() % cycle;
			size      = (f32)rMin
			     + mBodyScale
			           * ((f32)(rMax - rMin)
			              * JMASSin(182.04445f * (180.0f * (f32)time / (f32)cycle)));
		}
	}

	gpPollution->stampGround(1, unk1AC * mLinearVelocity.x + mPosition.x,
	                         mPosition.y,
	                         unk1AC * mLinearVelocity.z + mPosition.z,
	                         32.0f * size);
}

void TIgaigaPolluteModelManager::init(TLiveActor* param_1)
{
	TEnemyPolluteModelManager::init(param_1);

	void* res = JKRFileLoader::getGlbResource(
	    "/scene/igaiga/stamp_igaiga_model1.bmd");
	SDLModelData* modelData = new SDLModelData(J3DModelLoaderDataBase::load(
	    res, J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
	             | (1 << J3DMLF_TevStageNumShift)));

	for (int i = 0; i < unk14; ++i)
		unk18[i] = new TIgaigaPolluteModel(param_1, 0, modelData);
}

void TIgaigaPolluteModel::setAnm()
{
	unk10->getMActor()->setBckFromIndex(7);
	unk10->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
}

TIgaigaManager::TIgaigaManager(const char* name)
    : TSmallEnemyManager(name)
    , unk64(0)
    , unk68(nullptr)
{
	gpCurRollEnemy = nullptr;
}

void TIgaigaManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TRollEnemySaveLoadParams("/enemy/igaiga.prm");
	unk68 = new TWaterEmitInfo("/enemy/igaigawater.prm");
}

void TIgaigaManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "igaiga_model1.bmd", 0x11240000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

// TODO: unreferenced 12-byte constants (0,0,0) and (1,1,1) sit in .rodata
// between the manager's createModelData string and the Gorogoro strings in the
// target; the code that used them is unknown, they only keep the string pool
// offsets right.
static const Vec sZeroVec = { 0.0f, 0.0f, 0.0f };
static const Vec sOneVec = { 1.0f, 1.0f, 1.0f };

TSmallEnemy* TIgaigaManager::createEnemyInstance() { return new TIgaiga; }

void TIgaigaManager::initSetEnemies()
{
	unk60 = new TIgaigaPolluteModelManager;
	unk60->init((TLiveActor*)unk18[0]);
}

void TIgaigaManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TEnemyManager::perform(cue, graphics);
	unk60->perform(cue, graphics);
}

static int RollEnemyBodyCallback(J3DNode* param_1, int param_2)
{
	if (param_2 == 0) {
		if (gpCurRollEnemy == nullptr || !gpCurRollEnemy->isRolling())
			return true;

		J3DJoint* joint = (J3DJoint*)param_1;
		MtxPtr anmMtx = gpCurRollEnemy->getModel()->getAnmMtx(joint->getJntNo());

		Mtx local_44;
		MtxPtr rotMtx = local_44;
		MsMtxSetRotX(rotMtx, gpCurRollEnemy->unk194);
		anmMtx[1][3] += TRollEnemy::mTransYOffset;

		MTXConcat(anmMtx, rotMtx, anmMtx);
		MTXConcat(J3DSys::mCurrentMtx, rotMtx, J3DSys::mCurrentMtx);
	}
	return true;
}

TIgaiga::TIgaiga(const char* name)
    : TRollEnemy(name)
    , unk1B4(0)
    , unk1B8(0)
    , unk1BC(true)
    , unk1CC(1.0f)
    , unk1D0(0)
    , unk1E4(1.0f)
    , unk1E8(0)
{
}

void TIgaiga::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000017;
	unk150     = 17;
	offHitFlag(HIT_FLAG_UNK10000000 | HIT_FLAG_UNK8000000);
	onHitFlag(HIT_FLAG_UNK40000000);
	mSpine->initWith(&TNerveIgaigaRollOnGraph::theNerve());
	unk1A4 = (TRollEnemySaveLoadParams*)getSaveParam();
	getMActor()->setJointCallback(1, &RollEnemyBodyCallback);
	getMActor()->setBtkFromIndex(0);
	unk124->setGraph(gpConductor->getGraphByName("igaiga"));
}

void TIgaiga::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("igaiga_model1.bmd", 0);
}

void TIgaiga::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);
}

void TIgaiga::calcRootMatrix()
{
	gpCurRollEnemy = this;
	TSpineEnemy::calcRootMatrix();
}

bool TIgaiga::isRolling()
{
	if (mSpine->getCurrentNerve() == &TNerveIgaigaRollOnGraph::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveIgaigaShootFromCannon::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveIgaigaWaterHit::theNerve())
		return true;
	return false;
}

void TIgaiga::behaveToWater(THitActor*)
{
	mSprayedByWaterCooldown = 0;
	if (unk1E4 < unk1A4->mSLExpandMax.get())
		unk1E4 *= unk1A4->mSLExpandRate.get();

	unk165 = true;
	if (mSpine->getCurrentNerve() != &TNerveIgaigaWaterHit::theNerve())
		mSpine->pushNerve(&TNerveIgaigaWaterHit::theNerve());
}

void TIgaiga::reset()
{
	TRollEnemy::reset();
	initialGraphNode();
	offLiveFlag(LIVE_FLAG_UNK10);
	unk1B4 = 0;
	unk1B8 = TMsRange<s32>(50, 100).rand() * 120;
	unk1BC = true;
	mPosition.y += 20.0f;
	gpMap->checkGround(mPosition.x, mPosition.y + mHeadHeight, mPosition.z,
	                   &mGroundPlane);
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	unk1E4 = 1.0f;
	unk1CC = 1.0f;
	unk1AC = -30.0f;
	unk1B0 = 2.0f;
	unk1E8 = 0;
}

void TIgaiga::kill()
{
	unk194 = 0.0f;
	TSmallEnemy::kill();
}

void TIgaiga::moveObject()
{
	TWalkerEnemy::moveObject();

	unk1CC = MsClamp(unk1CC - 0.0002f, 0.5f, 1.0f);

	f32 attackRadius = getSaveParams()->getSLAttackRadius();
	f32 attackHeight = getSaveParams()->getSLAttackHeight();
	f32 damageRadius = getSaveParams()->getSLDamageRadius();
	f32 damageHeight = getSaveParams()->getSLDamageHeight();
	f32 scale        = unk154 * unk1CC;
	mBodyScale       = MsClamp(unk1E4 * scale, scale, 3.0f * mBodyScale);
	f32 ratio        = mBodyScale / unk154;
	f32 clamped      = MsClamp(unk1CC * unk1E4, 1.0f, 1.2f);
	mScaledBodyRadius = 8.0f * (mBodyScale * mBodyRadius) * clamped;
	mScaling.x = mScaling.y = mScaling.z = mBodyScale;
	setHitParams(attackRadius * ratio, attackHeight * ratio,
	             damageRadius * ratio, damageHeight * ratio);
	mMarchSpeed = unk1A4->mSLMarchSpeedLow.get();
	mTurnSpeed  = unk1A4->getSLTurnSpeedLow();

	if (getTracer()->getCurrent().checkFlag(0x40)) {
		JGeometry::TVec3<f32> point;
		getTracer()->getCurrent().getPoint(&point);
		if (mPosition.y < point.y + 50.0f) {
			kill();
			unk1BC = true;
		}
	}
}

void TIgaiga::rollSE()
{
	SMSGetMSound()->startSoundActorSpecial(MSD_SE_EN_IGAIGA_ROLL, &mPosition,
	                                       mScaling.x, mMarchSpeed, 0, nullptr,
	                                       0, 4);
}

void TIgaiga::boundSE()
{
	gpMSound->startSoundActorWithInfo(MSD_SE_EN_IGAIGA_BOUND, &mPosition, nullptr,
	                                  fabsf(mGroundPlane->getNormal().y), 0, 0,
	                                  nullptr, 0, 4);
}

void TIgaiga::walkBehavior(int param_1, f32 param_2)
{
	TRollEnemy::walkBehavior(param_1, param_2);

	f32 speedX = mLinearVelocity.x;
	f32 speedZ = mLinearVelocity.z;
	if (unk1A8) {
		JGeometry::TVec3<f32> a = mVelocity;
		JGeometry::TVec3<f32> b = a;
		JGeometry::TVec3<f32> c = a;
		speedZ                  = b.z;
		speedX                  = c.x;
	}
	f32 speed = JGeometry::TUtil<f32>::sqrt(speedX * speedX + speedZ * speedZ);
	unk194 += 4.0f * (speed / (mBodyRadius * unk1CC * unk1E4));

	if (unk1B4 != 0) {
		unk1B4 += 1;
		if (unk1B4 > unk1B8)
			unk1B4 = 0;
	}

	if (!isAirborne() && mGroundPlane != nullptr && mGroundPlane->mActor != nullptr)
		((TLiveActor*)mGroundPlane->mActor)->receiveMessage(this, 0xE);

	if (unk138 != nullptr && unk138->mActor != nullptr)
		((TLiveActor*)unk138->mActor)->receiveMessage(this, 0xE);
}

bool TIgaiga::isReachedToGoalXZ()
{
	JGeometry::TVec3<f32> tmp = getUnk104().getPoint();
	tmp -= mPosition;
	if (!unk1A8)
		tmp.y = 0.0f;
	tmp.y = 0.0f;

	if (MsVECMag2(&tmp) < mReachNodeDist)
		return true;
	else
		return false;
}

void TIgaiga::setWalkAnm() { setBckAnm(3); }

void TIgaiga::setDeadAnm()
{
	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT))
		unk1C0 = mPosition;
	else {
		MtxPtr mtx = getMActor()->getModel()->getAnmMtx(0);
		unk1C0.set(mtx[0][3], mtx[1][3], mtx[2][3]);
	}

	gpMarioParticleManager->emit(0xCB, &unk1C0, 0, nullptr);

	if (unk1BC)
		setBckAnm(0);
	else
		setBckAnm(1);

	TIgaigaManager* manager     = (TIgaigaManager*)mManager;
	JGeometry::TVec3<f32> scale = mScaling;
	f32 factor                  = unk1CC * unk1E4;
	scale.x *= factor;
	scale.y *= factor;
	scale.z *= factor;
	mPosition.y = mGroundHeight;
	scale.x = MsClamp(scale.x, 0.8f, 1.5f);
	scale.y = scale.z = scale.x;
	manager->unk60->generatePolluteModel(mPosition, scale);
}

void TIgaiga::setMeltAnm()
{
	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT))
		unk1C0 = mPosition;
	else {
		MtxPtr mtx = getMActor()->getModel()->getAnmMtx(0);
		unk1C0.set(mtx[0][3], mtx[1][3], mtx[2][3]);
	}

	if (!checkLiveFlag(LIVE_FLAG_CLIPPED_OUT) && SMS_IsMarioTouchGround4cm()) {
		if (mScaling.x > mBodyScale)
			SMSRumbleMgr->start(0x15, 10, &mPosition);
		else
			SMSRumbleMgr->start(0x14, 10, &mPosition);
	}

	TIgaigaManager* manager = (TIgaigaManager*)mManager;
	manager->unk68->mPos.value = mPosition;
	gpModelWaterManager->emitRequest(*manager->unk68);

	JGeometry::TVec3<f32> scale(mScaling * 0.5f);

	JPABaseEmitter* emitter
	    = gpMarioParticleManager->emit(0xA1, &unk1C0, 0, nullptr);
	if (emitter)
		emitter->setGlobalScale(scale);

	emitter = gpMarioParticleManager->emit(0xA2, &unk1C0, 0, nullptr);
	if (emitter)
		emitter->setGlobalScale(scale);

	setBckAnm(5);

	if (TMsRange<f32>(0.0f, 1.0f).rand() < 0.2f)
		gpItemManager->makeObjAppear(mPosition.x, 20.0f + mPosition.y,
		                             mPosition.z, 0x20000002, true);
}

const char** TIgaiga::getBasNameTable() const { return igaiga_bastable; }

bool TIgaiga::isHitValid(u32 message)
{
	unk1BC = true;
	if (message == 1)
		unk1BC = false;
	return true;
}

void TIgaiga::bound()
{
	if (unk1A0 > 5.0f) {
		setBckAnm(2);
		if (!checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)
		    && SMS_IsMarioTouchGround4cm()) {
			if (mScaling.x > mBodyScale)
				SMSRumbleMgr->start(0x15, 10, &mPosition);
			else
				SMSRumbleMgr->start(0x14, 10, &mPosition);
		}
	}
}

void TIgaiga::shoot(JGeometry::TVec3<f32>& target)
{
	mSpine->setNext(&TNerveIgaigaShootFromCannon::theNerve());
	unk1D8 = target;
	offLiveFlag(LIVE_FLAG_UNK10);
	unk1A8 = 1;
}

DEFINE_NERVE(TNerveIgaigaRollOnGraph, TLiveActor)
{
	TIgaiga* self = (TIgaiga*)spine->getBody();

	if (spine->getTime() == 0)
		self->setWalkAnm();

	if (self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(2))
			self->setBckAnm(3);
	}

	if (self->isReachedToGoalXZ()) {
		if (self->jumpToNextGraphNode() >= 0)
			self->flagJump();

		if (self->getTracer()->getCurrent().checkFlag(0x40))
			return false;

		self->goToRandomNextGraphNode();
	}

	self->walkBehavior(2, 1.0f);
	return false;
}

DEFINE_NERVE(TNerveIgaigaWaterHit, TLiveActor)
{
	TIgaiga* self = (TIgaiga*)spine->getBody();

	if (spine->getTime() == 0)
		self->setBckAnm(6);

	if (self->unk1E4 >= self->unk1A4->mSLExpandMax.get()) {
		if (self->unk1E8 > 20) {
			self->onLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
			spine->pushAfterCurrent(&TNerveSmallEnemyDie::theNerve());
			return true;
		}
		self->unk1E8 += 1;
	} else if (self->checkCurAnmEnd(0)) {
		if (!self->unsetUnk165()) {
			self->setBckAnm(3);
			spine->pushAfterCurrent(&TNerveIgaigaRollOnGraph::theNerve());
			return true;
		}
	}

	if (self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(2))
			self->setBckAnm(3);
	}

	if (self->isReachedToGoalXZ()) {
		if (self->jumpToNextGraphNode() >= 0)
			self->flagJump();

		if (self->getTracer()->getCurrent().checkFlag(0x40))
			return false;

		self->goToRandomNextGraphNode();
	}

	self->walkBehavior(2, 1.0f);
	return false;
}

DEFINE_NERVE(TNerveIgaigaShootFromCannon, TLiveActor)
{
	TIgaiga* self = (TIgaiga*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(4);
		self->mPosition.y += 10.0f;
		self->mVelocity = self->unk1D8;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
	} else if (self->checkCurAnmEnd(0) && !self->isAirborne()) {
		self->bound();
		spine->pushAfterCurrent(&TNerveIgaigaRollOnGraph::theNerve());
		return true;
	}

	self->walkBehavior(2, 1.0f);
	return false;
}

void TGorogoroPolluteModelManager::init(TLiveActor* param_1)
{
	TEnemyPolluteModelManager::init(param_1);

	void* res = JKRFileLoader::getGlbResource(
	    "/scene/gorogoro/bosspaku_head_stamp.bmd");
	SDLModelData* modelData = new SDLModelData(J3DModelLoaderDataBase::load(
	    res, J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
	             | (1 << J3DMLF_TevStageNumShift)));

	for (int i = 0; i < unk14; ++i)
		unk18[i] = new TGorogoroPolluteModel(param_1, 0, modelData);
}

void TGorogoroPolluteModel::setAnm()
{
	unk10->getMActor()->setBckFromIndex(3);
	unk10->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
}

TGorogoroManager::TGorogoroManager(const char* name)
    : TSmallEnemyManager(name)
    , unk60(0)
    , unk64(nullptr)
    , unk68(true)
    , unk6C(nullptr)
    , unk70(nullptr)
{
}

void TGorogoroManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TRollEnemySaveLoadParams("/enemy/gorogoro.prm");

	static const char* anmlist[] = { "bosspaku_head_move", nullptr };
	createSharedMActorSet(anmlist);
}

void TGorogoroManager::loadAfter()
{
	unk64 = (TMapEventSink*)JDrama::TNameRefGen::getInstance()
	            ->getRootNameRef()
	            ->search("イベント（地形沈むビアンコ）");
	unk70 = (TAreaCylinderManager*)gpConductor->search("ゴロゴロ発生マネージャー");
}

TSmallEnemy* TGorogoroManager::createEnemyInstance() { return new TGorogoro; }

void TGorogoroManager::initSetEnemies()
{
	unk6C = new TGorogoroPolluteModelManager;
	unk6C->init((TLiveActor*)unk18[0]);

	static const char* graphlist[] = { "gorogoro0", "gorogoro1" };
	TGraphWeb* graph;
	for (int i = 0; i < mObjNum; ++i) {
		graph = gpConductor->getGraphByName(graphlist[i % 2]);
		if (graph->isDummy())
			graph = gpConductor->getGraphByName(graphlist[0]);

		if (!graph->isDummy()) {
			TGorogoro* gorogoro = (TGorogoro*)unk18[i];
			JGeometry::TVec3<f32> point;
			graph->getGraphNode(0).getPoint(&point);
			gorogoro->getTracer()->setGraph(graph);
			gorogoro->mPosition = point;
			gorogoro->unk1E8    = graph->getNodeNum() - 1;
		}
	}
}

void TGorogoroManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "bosspaku_head.bmd", 0x10300000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

BOOL TGorogoroManager::inArea(const JGeometry::TVec3<f32>& position)
{
	return unk70 == nullptr ? TRUE : unk70->contain(position);
}

void TGorogoroManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & 1) {
		unk60 += 1;
		int interval = ((TRollEnemySaveLoadParams*)unk38)->mSLGenerateInterval.get();
		if (unk60 > interval) {
			unk60 = 0;
			if (inArea(*gpMarioPos)) {
				for (int i = 0; i < getActiveObjNum(); ++i) {
					TGorogoro* gorogoro = (TGorogoro*)unk18[i];
					if (!gorogoro->checkLiveFlag(LIVE_FLAG_DEAD))
						continue;

					if (unk64 != nullptr) {
						if (!unk64->isBuried(1)) {
						if (unk68) {
							unk68 = false;
							gorogoro->reset();

							JGeometry::TVec3<f32> pos;
							gorogoro->getTracer()
							    ->getGraph()
							    ->getGraphNode(10)
							    .getPoint(&pos);
							gorogoro->mPosition = pos;
							gorogoro->getTracer()->mCurrIdx = 10;
							gorogoro->getTracer()->mPrevIdx = 9;
							gorogoro->getTracer()
							    ->getGraph()
							    ->getGraphNode(11)
							    .getPoint(&pos);
							JGeometry::TVec3<f32> dir(
							    pos.x - gorogoro->mPosition.x, 0.0f,
							    pos.z - gorogoro->mPosition.z);
							gorogoro->mRotation.y = MsAngleWrap(
							    MsGetRotFromZaxisY(dir));
							gorogoro->setGoalPath(TPathNode(pos));

							gorogoro = (TGorogoro*)unk18[1];
							gorogoro->reset();
							gorogoro->getTracer()
							    ->getGraph()
							    ->getGraphNode(16)
							    .getPoint(&pos);
							gorogoro->mPosition = pos;
							gorogoro->getTracer()->mCurrIdx = 16;
							gorogoro->getTracer()->mPrevIdx = 15;
							gorogoro->getTracer()
							    ->getGraph()
							    ->getGraphNode(17)
							    .getPoint(&pos);
							JGeometry::TVec3<f32> dir2(
							    pos.x - gorogoro->mPosition.x, 0.0f,
							    pos.z - gorogoro->mPosition.z);
							gorogoro->mRotation.y = MsAngleWrap(
							    MsGetRotFromZaxisY(dir2));
							gorogoro->setGoalPath(TPathNode(pos));
						} else {
							gorogoro->reset();
						}
						} else {
							unk60 = interval;
						}
					} else {
						gorogoro->reset();
					}
					break;
				}
			}
		}
	}

	TEnemyManager::perform(cue, graphics);
	unk6C->perform(cue, graphics);
}

void TGorogoroManager::requestPolluteModel(JGeometry::TVec3<f32>& position,
                                           JGeometry::TVec3<f32>& scale)
{
	unk6C->generatePolluteModel(position, scale);
}

void TGorogoro::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000019;
	unk150     = 49;
	offHitFlag(HIT_FLAG_UNK40000000 | HIT_FLAG_UNK10000000
	           | HIT_FLAG_UNK8000000);
	mSpine->initWith(&TNerveGorogoroRollOnGraph::theNerve());
	unk1A4 = (TRollEnemySaveLoadParams*)getSaveParam();

	TMirrorActor* mirror = new TMirrorActor("ゴロゴロin鏡");
	mirror->init(getMActor()->getModel(), 0x18);
	unk1ED.a = 0xFF;

	const ResTIMG* image
	    = (const ResTIMG*)JKRFileLoader::getGlbResource("/scene/map/pollution/H_ma_rak.bti");
	if (image) {
		SMS_ChangeTextureAll(getMActor()->getModel()->getModelData(),
		                     "M_dummy", *image);
		SMS_ChangeTextureAll(mirror->unk14->getModelData(), "M_dummy", *image);
	}

	for (u16 i = 0; i < getMActor()->getModel()->getModelData()->getMaterialNum();
	     ++i) {
		SMS_InitPacket_OneTevKColor(getMActor()->getModel(), i, GX_KCOLOR0,
		                            &unk1ED);
		SMS_InitPacket_OneTevKColor(mirror->unk14, i, GX_KCOLOR0, &unk1ED);
	}

	getMActor()->setJointCallback(1, &RollEnemyBodyCallback);
	unk130 = 1;
}

void TGorogoro::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);

	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)
	    && gpMirrorModelManager->isInMirror(mPosition)) {
		if (cue & 2) {
			calcRootMatrix();
			getMActor()->calc();
		}
		if (cue & 4)
			getMActor()->viewCalc();
	}
}

void TGorogoro::calcRootMatrix()
{
	gpCurRollEnemy = this;
	if (mSpine->getCurrentNerve() == &TNerveGorogoroDie::theNerve()) {
		TSpineEnemy::calcRootMatrix();
		if (checkLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH)) {
			unk1B4.mMtx[0][3] = mPosition.x;
			unk1B4.mMtx[2][3] = mPosition.z;
		}
	} else if (!isEaten()) {
		f32 groundOffsetY = unk1A4->mSLGroundOffsetY.get();
		if (mPosition.y < mGroundHeight + 20.0f) {
			JPABaseEmitter* emitter
			    = gpMarioParticleManager->emitAndBindToMtxPtr(
			        0x175, getMActor()->getModel()->getAnmMtx(0), 1, this);
			if (emitter)
				emitter->setGlobalScale(mScaling);
		}

		f32 y = groundOffsetY * unk158 + mPosition.y;
		MsMtxSetXYZRPH(getModel()->getBaseTRMtx(), mPosition.x, y, mPosition.z,
		               mRotation.x, mRotation.y, mRotation.z);
		getModel()->setBaseScale(mScaling);
	}
}

void TGorogoro::reset()
{
	unk130 = 1;
	TRollEnemy::reset();
	offLiveFlag(LIVE_FLAG_UNK1000);
	unk1ED.a = 0xFF;
	unk1AC   = -10.0f;
	unk1B0   = 1.0f;
}

void TGorogoro::kill()
{
	unk194 = 0.0f;
	if (mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()) {
		if (mSpine->getCurrentNerve() != &TNerveGorogoroDie::theNerve()) {
			mSpine->reset();
			mSpine->setNext(&TNerveGorogoroDie::theNerve());
			mSpine->pushAfterCurrent(mSpine->getDefault());
			onLiveFlag(LIVE_FLAG_UNK8);
		}
	}
}

void TGorogoro::forceKill()
{
	if (mGroundPlane->isIllegalData())
		return;

	if (mGroundPlane->isPool() || mGroundPlane->isWaterSurface()) {
		if (!isAirborne()) {
			if (mSpine->getCurrentNerve() != &TNerveGorogoroDie::theNerve()) {
				mSpine->reset();
				mSpine->setNext(&TNerveGorogoroDie::theNerve());
				mSpine->pushAfterCurrent(mSpine->getDefault());
				onLiveFlag(LIVE_FLAG_UNK20000);
				onLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
			}
		}
	}
}

void TGorogoro::behaveToWater(THitActor*)
{
	mSprayedByWaterCooldown = 0;
	if (unk158 < unk1A4->mSLExpandMax.get()) {
		f32 rate = unk1A4->mSLExpandRate.get();
		mBodyScale *= rate;
		unk158 *= rate;
		mScaledBodyRadius *= rate;
		mScaling.z *= rate;
		mScaling.y = mScaling.x = mScaling.z;

		f32 attackRadius = getSaveParams()->getSLAttackRadius();
		f32 attackHeight = getSaveParams()->getSLAttackHeight();
		f32 damageRadius = getSaveParams()->getSLDamageRadius();
		f32 damageHeight = getSaveParams()->getSLDamageHeight();
		attackRadius *= mBodyScale / unk154;
		attackHeight *= mBodyScale / unk154;
		damageRadius *= mBodyScale / unk154;
		damageHeight *= mBodyScale / unk154;
		setHitParams(attackRadius, attackHeight, damageRadius, damageHeight);
	}

	unk1ED.a = (mHitPoints * 255) / getMaxHitPoints();
	if (mHitPoints < 2)
		mHitPoints = 1;

	JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x176, getMActor()->getModel()->getAnmMtx(0), 1, this);
	if (emitter)
		emitter->setGlobalScale(mScaling);
}

void TGorogoro::rollSE()
{
	gpMSound->startSoundActorWithInfo(0x2054, &mPosition, nullptr,
	                                  fabsf(mGroundPlane->getNormal().y), 0, 0,
	                                  nullptr, 0, 4);
}

void TGorogoro::boundSE()
{
	gpMSound->startSoundActorWithInfo(0x2844, &mPosition, nullptr,
	                                  fabsf(mGroundPlane->getNormal().y), 0, 0,
	                                  nullptr, 0, 4);
}

void TGorogoro::walkBehavior(int param_1, f32 param_2)
{
	if (mPosition.y > mGroundHeight)
		onLiveFlag(LIVE_FLAG_AIRBORNE);

	if (!checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		if (mGroundPlane != nullptr && mGroundPlane->mActor != nullptr
		    && mGroundPlane->mActor->mActorType == 0x4000009A) {
			((TBiancoWatermill*)mGroundPlane->mActor)
			    ->turnByEnemy((THitActor*)this, mGroundPlane);
			TRollEnemy::walkBehavior(param_1, 0.2f * param_2);
			return;
		}

		const TBGCheckData* roof;
		gpMap->checkRoof(mPosition.x, mPosition.y + mHeadHeight, mPosition.z,
		                 &roof);
		if (roof != nullptr && roof->mActor != nullptr
		    && roof->mActor->mActorType == 0x4000009A) {
			((TBiancoWatermill*)roof->mActor)
			    ->turnByEnemy((THitActor*)this, roof);
			TRollEnemy::walkBehavior(param_1, 0.3f * param_2);
			return;
		}

		if (unk138 != nullptr && unk138->mActor != nullptr) {
			TBGWallCheckRecord record(mPosition.x, mPosition.y + mHeadHeight,
			                          mPosition.z, mBodyScale * mWallRadius, 4,
			                          0);
			if (gpMap->isTouchedWallsAndMoveXZ(&record)) {
				for (int i = 0; i < record.mResultWallsNum; ++i) {
					const TLiveActor* actor = record.mResultWalls[i]->mActor;
					if (actor != nullptr && actor->mActorType == 0x4000009A)
						((TBiancoWatermill*)actor)
						    ->turnByEnemy((THitActor*)this,
						                  record.mResultWalls[i]);
				}
				TRollEnemy::walkBehavior(param_1, 0.2f * param_2);
				return;
			}
		}
	}

	mTurnSpeed = unk1A4->getSLTurnSpeedLow();
	TRollEnemy::walkBehavior(param_1, param_2);
	unk194 += 0.4f * mMarchSpeed;

	if (mSpine->getCurrentNerve() != &TNerveGorogoroDie::theNerve()) {
		if (mPosition.y < mGroundHeight + 10.0f
		    && mGroundPlane->isWaterSurface()) {
			onLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
			onLiveFlag(LIVE_FLAG_UNK20000);
			kill();
		}
	}
}

void TGorogoro::flagJump()
{
	JGeometry::TVec3<f32> goal;
	getTracer()->getCurrent().getPoint(&goal);
	mPosition.y += 30.0f;
	f32 speed = unk124->unkC;
	JGeometry::TVec3<f32> vel
	    = calcVelocityToJumpToY(goal, speed, getGravityY());
	unk1A8    = 1;
	mVelocity = vel;
	onLiveFlag(LIVE_FLAG_AIRBORNE);
}

void TGorogoro::setDeadAnm()
{
	JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
	    0xBF, getMActor()->getModel()->getAnmMtx(1), 0, nullptr);
	if (emitter)
		emitter->setGlobalScale(mScaling);

	setBckAnm(0);

	if (gpMSound->gateCheck(0x2856))
		MSoundSESystem::MSoundSE::startSoundActor(0x2856, &mPosition, 0,
		                                          nullptr, 0, 4);
}

void TGorogoro::setMeltAnm()
{
	setBckAnm(1);
	unk130 = 0;
	onLiveFlag(LIVE_FLAG_UNK1000);
	MtxPtr mtx = unk1B4.mMtx;
	MTXCopy(getMActor()->getModel()->getBaseTRMtx(), mtx);
	unk1B4.mMtx[1][3] = mGroundHeight;

	JPABaseEmitter* emitter
	    = gpMarioParticleManager->emitAndBindToMtxPtr(0xBE, mtx, 0, nullptr);
	if (emitter)
		emitter->setGlobalScale(mScaling);

	if (gpMSound->gateCheck(0x2913))
		MSoundSESystem::MSoundSE::startSoundActor(0x2913, &mPosition, 0,
		                                          nullptr, 0, 4);
}

void TGorogoro::bound()
{
	((TGorogoroManager*)mManager)->unk6C->generatePolluteModel(mPosition,
	                                                           mScaling);
	if (!checkLiveFlag(LIVE_FLAG_CLIPPED_OUT) && SMS_IsMarioTouchGround4cm())
		SMSRumbleMgr->start(0x15, 10, &mPosition);
}

const char** TGorogoro::getBasNameTable() const { return gorogoro_bastable; }

bool TGorogoro::isRolling()
{
	if (mSpine->getCurrentNerve() == &TNerveGorogoroRollOnGraph::theNerve()
	    || isBckAnm(1))
		return true;
	return false;
}

void TGorogoro::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("bosspaku_head.bmd", 3);
}

DEFINE_NERVE(TNerveGorogoroRollOnGraph, TLiveActor)
{
	TGorogoro* self = (TGorogoro*)spine->getBody();

	if (spine->getTime() == 0) {
		self->goToShortestNextGraphNode();
		self->setBckAnm(2);
	}

	if (self->isReachedToGoalXZ()) {
		if (self->jumpToNextGraphNode() >= 0)
			self->flagJump();
		else
			self->goToShortestNextGraphNode();
	}

	self->walkBehavior(2, 1.0f);
	return false;
}

DEFINE_NERVE(TNerveGorogoroDie, TLiveActor)
{
	TGorogoro* self = (TGorogoro*)spine->getBody();

	if (spine->getTime() < 2) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		if (self->getGroundPlane()->isWaterSurface() && !self->isAirborne())
			self->generateEffectColumWater();

		if (self->checkLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH)) {
			self->setMeltAnm();
		} else {
			((TGorogoroManager*)self->mManager)
			    ->unk6C->generatePolluteModel(self->mPosition, self->mScaling);
			self->setDeadAnm();
			self->setDeadEffect();
		}
	} else if (self->checkCurAnmEnd(0) || spine->getTime() > 360) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->onLiveFlag(LIVE_FLAG_DEAD);
		self->onLiveFlag(LIVE_FLAG_UNK8);
		self->offLiveFlag(LIVE_FLAG_HIDDEN);
		self->offLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
		self->mHolder = nullptr;
		self->stopAnmSound();
		spine->reset();
		spine->setNext(&TNerveSmallEnemyDie::theNerve());
		spine->pushAfterCurrent(spine->getDefault());
		self->genRandomItem();
		return true;
	}

	if (self->checkLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH))
		self->walkBehavior(2, 0.5f);

	return false;
}
