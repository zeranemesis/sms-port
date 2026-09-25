#include <Enemy/ElecNokonoko.hpp>
#include <Enemy/Conductor.hpp>
#include <Camera/Camera.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <System/Particles.hpp>
#include <System/MarDirector.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/ShadowUtil.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <Player/MarioAccess.hpp>
#include <stdlib.h>
#include <math.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// TODO: this translation unit is freshly scaffolded from mario.MAP. Class
// layouts only contain the fields verified from the ctors and the functions
// decompiled so far.

// TODO: particle ids 0xCA and 0x17A-0x17F are not named in Particles.hpp yet
// (probably ms_dennoko_* effects).

static const char* dennoko_bastable[] = {
	"/scene/dennoko/bas/dennoko_catch1.bas",
	"/scene/dennoko/bas/dennoko_down1.bas",
	"/scene/dennoko/bas/dennoko_elec_down1.bas",
	"/scene/dennoko/bas/dennoko_hit1.bas",
	nullptr,
	nullptr,
	"/scene/dennoko/bas/dennoko_mogaki1_loop.bas",
	"/scene/dennoko/bas/dennoko_mogaki1_start.bas",
	nullptr,
	nullptr,
	"/scene/dennoko/bas/dennoko_run1_loop.bas",
	nullptr,
	"/scene/dennoko/bas/dennoko_shoot1.bas",
	"/scene/dennoko/bas/dennoko_supply1.bas",
	nullptr,
	"/scene/dennoko/bas/dennoko_turn1_loop.bas",
	nullptr,
	nullptr,
};

bool TElecNokonoko::mReflectSw = true;
u8 TElecNokonoko::mCarapaceJntIndex;

// fabricated
static inline bool isCarapaceOn(const TElecNokonoko* self)
{
	return self->unk1A4 == 0 ? true : false;
}

// fabricated
static inline void setMaterial(MActor* actor, J3DMaterialTable* table)
{
	J3DModel* model = actor->getModel();
	model->getModelData()->setMaterialTable(table, J3DMatCopyFlag_All);
	actor->initDL();
	actor->getModel()->lock();
}

void createNokonokoThunder(JGeometry::TVec3<f32>)
{
	// TODO: UNUSED in the map (size 0x60), contents unknown
}

TElecNokonokoSaveLoadParams::TElecNokonokoSaveLoadParams(const char* path)
    : TWalkerEnemyParams(path)
    , PARAM_INIT(mSLReadyTime, 300)
    , PARAM_INIT(mSLCarapaceGravity, 0.01f)
    , PARAM_INIT(mSLCarapaceSpeed, 5.0f)
    , PARAM_INIT(mSLCarapaceTurnSpeed, 2.0f)
    , PARAM_INIT(mSLCarapaceSpinSpeed, 2.0f)
    , PARAM_INIT(mSLCarapaceShootRange, 500.0f)
    , PARAM_INIT(mSLCarapaceFlyDist, 100.0f)
{
	TParams::load(mPrmPath);
}

TElecNokonokoManager::TElecNokonokoManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TElecNokonokoManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TElecNokonokoSaveLoadParams("/enemy/elecNokonoko.prm");
	unk60 = J3DModelLoaderDataBase::loadMaterialTable(
	    JKRGetResource("/scene/dennoko/dennoko_model1.bmt"));
}

void TElecNokonokoManager::initSetEnemies() { }

TSpineEnemy* TElecNokonokoManager::createEnemyInstance()
{
	return new TElecNokonoko;
}

void TElecNokonokoManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "dennoko_model1.bmd", 0x10220000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TElecNokonokoManager::clipEnemies(JDrama::TGraphics* graphics)
{
	f32 radius;
	f32 far;
	if (unk38 == nullptr) {
		far    = gpConductor->getCondParams().mEnemyFarClip.get();
		radius = 300.0f;
	} else {
		far    = unk38->mSLFarClip.get();
		radius = unk38->mSLClipRadius.get();
	}

	SetViewFrustumClipCheckPerspective(
	    gpCamera->getFovy(), gpCamera->getAspect(), graphics->mNearPlane, far);

	for (int i = 0; i < mObjNum; ++i) {
		TElecNokonoko* noko = (TElecNokonoko*)unk18[i];

		if (ViewFrustumClipCheck(graphics, &noko->mPosition, radius))
			noko->offLiveFlag(LIVE_FLAG_CLIPPED_OUT);
		else
			noko->onLiveFlag(LIVE_FLAG_CLIPPED_OUT);

		if (!noko->mCarapace->isState(0)) {
			if (ViewFrustumClipCheck(graphics, &noko->mCarapace->mPosition,
			                         radius))
				noko->mCarapace->offLiveFlag(LIVE_FLAG_CLIPPED_OUT);
			else
				noko->mCarapace->onLiveFlag(LIVE_FLAG_CLIPPED_OUT);
		}
	}
}

void TElecNokonokoManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TEnemyManager::perform(cue, graphics);
	for (int i = 0; i < getActiveObjNum(); ++i)
		((TElecNokonoko*)getObj(i))->mCarapace->perform(cue, graphics);
}

TElecNokonoko::TElecNokonoko(const char* name)
    : TWalkerEnemy(name)
    , mCarapace(nullptr)
    , unk198(0)
    , unk1A4(0)
{
}

void TElecNokonoko::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x1000000A;
	unk150     = 0x11;
	mParams    = (TElecNokonokoSaveLoadParams*)getSaveParam();
	mCarapace  = new TElecCarapace("ノコノコ甲羅");
	mSpine->initWith(&TNerveWalkerGraphWander::theNerve());
	mCarapace->loadInit(this, "koura_model1.bmd");

	setMaterial(mCarapace->mMActor, ((TElecNokonokoManager*)mManager)->unk60);

	unk19C = TMsRange<s32>(0, 300).rand();
	offHitFlag(HIT_FLAG_NO_COLLISION);
}

void TElecNokonoko::rest()
{
	TWalkerEnemy::reset();
	mScaledBodyRadius = 140.0f;
}

void TElecNokonoko::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	reset();
}

void TElecNokonoko::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("dennoko_model1.bmd", 3);

	setMaterial(mMActor, ((TElecNokonokoManager*)mManager)->unk60);
}

void TElecNokonoko::moveObject()
{
	TWalkerEnemy::moveObject();
	if (isBckAnm(11) && checkCurAnmEnd(0))
		setBckAnm(10);
}

void TElecNokonoko::attackToMario()
{
	if (mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()) {
		TSmallEnemy::attackToMario();
		if (mSpine->getCurrentNerve() != &TNerveElecNokonokoAttack::theNerve()
		    && mSpine->getCurrentNerve()
		           != &TNerveElecNokonokoCollect::theNerve()
		    && mSpine->getCurrentNerve() != &TNerveElecNokonokoShoot::theNerve()
		    && isCarapaceOn(this))
			mSpine->pushNerve(&TNerveElecNokonokoAttack::theNerve());
	}
}

void TElecNokonoko::calcRootMatrix()
{
	TSpineEnemy::calcRootMatrix();
	if (isCarapaceOn(this)) {
		const TNerveBase<TLiveActor>* nerve;
		if ((nerve = mSpine->getCurrentNerve()) != &TNerveElecNokonokoFreeze::theNerve()
		    && nerve != &TNerveSmallEnemyDie::theNerve()) {
			if (mSpine->getCurrentNerve()
			    != &TNerveElecNokonokoCollect::theNerve()) {
				SMSGetMSound()->startSoundActor(MSD_SE_EN_DENNOKO_SPARK1,
				                                &mPosition, 0, nullptr, 0, 4);
				if (JPABaseEmitter* emitter
				    = gpMarioParticleManager->emitAndBindToMtxPtr(
				        0x17A, getMActor()->getModel()->getAnmMtx(7), 1, this))
					emitter->setGlobalScale(mScaling);
				if (JPABaseEmitter* emitter
				    = gpMarioParticleManager->emitAndBindToMtxPtr(
				        0x17B, getMActor()->getModel()->getAnmMtx(7), 1, this))
					emitter->setGlobalScale(mScaling);
				if (JPABaseEmitter* emitter
				    = gpMarioParticleManager->emitAndBindToMtxPtr(
				        0x17C, getMActor()->getModel()->getAnmMtx(7), 1, this))
					emitter->setGlobalScale(mScaling);
			}
		}
	}

	if (mCurrentBckAnm == 2) {
		if (JPABaseEmitter* emitter
		    = gpMarioParticleManager->emitAndBindToMtxPtr(
		        0x17D, getMActor()->getModel()->getAnmMtx(0), 1, this))
			emitter->setGlobalScale(mScaling);

		MtxPtr mtx = getMActor()->getModel()->getAnmMtx(8);
		unk1A8.set(mtx[0][3], mtx[1][3], mtx[2][3]);
		if (JPABaseEmitter* emitter
		    = gpMarioParticleManager->emitAndBindToPosPtr(0x17E, &unk1A8, 1,
		                                                  this))
			emitter->setGlobalScale(mScaling);

		if (getMActor()->getFrameCtrl(0)->checkPass(72.0f)) {
			if (JPABaseEmitter* emitter
			    = gpMarioParticleManager->emitAndBindToPosPtr(0x17F, &unk1A8,
			                                                  1, this))
				emitter->setGlobalScale(mScaling);
		}
	}
}

void TElecNokonoko::sendAttackMsgToMario()
{
	// TODO: message 9 is the electric shock, not named in THitMessageType yet
	if (unk1A4 == 0)
		SMS_SendMessageToMario(this, 9);
	else
		SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
}

BOOL TElecNokonoko::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_UNKD || message == HIT_MESSAGE_UNKB) {
		onLiveFlag(LIVE_FLAG_DEAD);
		kill();
		mCarapace->kill();
	}

	if (message == HIT_MESSAGE_TAKE && mHolder == nullptr) {
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mHolder = (TTakeActor*)sender;
		return TRUE;
	}

	if ((message == HIT_MESSAGE_PUT || message == HIT_MESSAGE_THROWN)
	    && mHolder == sender) {
		mHolder = nullptr;
		return TRUE;
	}

	if (message == HIT_MESSAGE_TRAMPLE) {
		if (unk1A4 == 1) {
			mHitPoints = 1;
			kill();
			return TRUE;
		}
		SMS_SendMessageToMario(this, 9);
		return FALSE;
	}

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		if (!changeByJuice())
			behaveToWater(sender);
		else
			mCarapace->kill();
		return TRUE;
	}

	return FALSE;
}

bool TElecNokonoko::isResignationAttack()
{
	f32 range = mParams->mSLCarapaceShootRange.get();
	if (!checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		if ((unk104.getPoint() - mPosition).length() < range) {
			mSpine->pushAfterCurrent(&TNerveElecNokonokoShoot::theNerve());
			return true;
		}
	}
	return false;
}

void TElecNokonoko::behaveToFindMario()
{
	mSpine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
	mSpine->pushAfterCurrent(&TNerveWalkerAttack::theNerve());
	mSpine->pushAfterCurrent(&TNerveElecNokonokoTurn::theNerve());
	setGoalPath(TPathNode((THitActor*)gpMarioAddress));
}

void TElecNokonoko::behaveToWater(THitActor*)
{
	if ((!isBckAnm(12) || !(getCurAnmFrameNo(0) > 58.0f))
	    && mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()) {
		if (isBckAnm(0) && unk1A4 == 0)
			return;

		unk165                   = true;
		mSprayedByWaterCooldown = 0;
		if (mSpine->getCurrentNerve()
		    != &TNerveElecNokonokoFreeze::theNerve())
			mSpine->pushNerve(&TNerveElecNokonokoFreeze::theNerve());
	}
}

void TElecNokonoko::catchIn()
{
	// TODO: UNUSED in the map (size 0x50), contents unknown
}

void TElecNokonoko::shootIn()
{
	// TODO: UNUSED in the map (size 0x44), contents unknown
}

const char** TElecNokonoko::getBasNameTable() const
{
	return dennoko_bastable;
}

void TElecNokonoko::setWaitAnm()
{
	unk198 = 0;
	setBckAnm(17);
}

void TElecNokonoko::setWalkAnm()
{
	if (!isBckAnm(10))
		setBckAnm(11);
}

void TElecNokonoko::setRunAnm()
{
	if (!isBckAnm(10))
		setBckAnm(11);
}

void TElecNokonoko::setDeadAnm() { setBckAnm(1); }

void TElecNokonoko::setMeltAnm()
{
	setBckAnm(2);
	onLiveFlag(LIVE_FLAG_UNK8);
	JGeometry::TVec3<f32> zero(0.0f, 0.0f, 0.0f);
	unk18C               = 3;
	mCarapace->mVelocity = zero;
	if (JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
	        0xCA, getMActor()->getModel()->getAnmMtx(0), 0, nullptr))
		emitter->setGlobalScale(mScaling);
}

void TElecNokonoko::genRandomItem()
{
	mCarapace->kill();
	gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_TLS_CHANGE,
	                                            &mPosition, 0, nullptr);
	TSmallEnemy::genRandomItem();
	if (checkLiveFlag(LIVE_FLAG_UNK10000)) {
		if (TMapObjBase* obj = gpItemManager->makeObjAppear(
		        mCarapace->mPosition.x, mCarapace->mPosition.y,
		        mCarapace->mPosition.z, 0x2000000E, true)) {
			obj->mVelocity.set(0.0f, 20.0f, 0.0f);
			obj->offLiveFlag(LIVE_FLAG_UNK10);
		}
	}
}

bool TElecNokonoko::isShootReady()
{
	// TODO: UNUSED in the map (size 0x6c), contents unknown
	return false;
}

bool TElecNokonoko::isCatchReady()
{
	return mSpine->getCurrentNerve() == &TNerveElecNokonokoCollect::theNerve()
	           ? true
	           : false;
}

void TElecNokonoko::forceCatchReady()
{
	if (mSpine->getCurrentNerve() == &TNerveSmallEnemyDie::theNerve())
		return;
	if (mSpine->getCurrentNerve() == &TNerveElecNokonokoFreeze::theNerve())
		return;
	if (mSpine->getCurrentNerve() == &TNerveElecNokonokoCollect::theNerve())
		return;
	mSpine->setNext(&TNerveElecNokonokoCollect::theNerve());
}

bool TElecNokonoko::isDeadByThunder()
{
	if (mSpine->getCurrentNerve() == &TNerveElecNokonokoFreeze::theNerve()) {
		JGeometry::TVec3<f32> diff = mPosition - mCarapace->mPosition;
		if (VECMag(&diff) < 200.0f)
			return true;
	}
	return false;
}

void TElecNokonoko::recoverCarapace()
{
	// TODO: UNUSED in the map (size 0x124), contents unknown
}

TElecCarapace::TElecCarapace(const char* name)
    : TEnemyAttachment(name)
    , mOwner(nullptr)
    , unk170(nullptr)
    , unk174(true)
    , unk175(0)
    , unk176(0)
    , unk178(0.0f)
    , unk17C(0.0f)
    , unk180(0)
    , unk184(0)
    , unk188(0.0f)
    , unk198(0.0f)
{
}

void TElecCarapace::loadInit(TSpineEnemy* owner, const char* name)
{
	TEnemyAttachment::loadInit(owner, name);
	mOwner = (TElecNokonoko*)unk160;
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(this);

	initHitActor(0x1000000B, 3, 0x98000000, 80.0f, 80.0f, 60.0f, 60.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	unk150 = 0;
	mSpine->initWith(&TNerveElecCarapaceMove::theNerve());
	if (TMsRange<s32>(0, 300).rand() < 150)
		unk174 = false;
	mHeadHeight = 80.0f;
}

void TElecCarapace::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TEnemyAttachment::perform(cue, graphics);
	if (cue & CUE_MOVE) {
		if (unk180 != 0) {
			++unk180;
			if (unk180 > 5)
				unk180 = 0;
		}
	}

	if (cue & CUE_ENTRY) {
		if (!isState(0) && !mOwner->checkLiveFlag(LIVE_FLAG_DEAD)) {
			if (mLiveFlag & (LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN))
				return;

			TCircleShadowRequest request;
			request.mPosition = mPosition;
			if (!isAirborne()) {
				request.mPosition.y       = mGroundHeight;
				request.mNeedsGroundCheck = 0;
			}
			request.mRadiusX = request.mRadiusZ = mOwner->mScaledBodyRadius;
			request.mRotationY                  = mRotation.y;
			gpBindShadowManager->request(request, getActorType());
		}
	}
}

void TElecCarapace::setBehavior()
{
	if (mOwner->checkLiveFlag(LIVE_FLAG_DEAD))
		kill();
	if (unk168)
		mPosition.y = mGroundHeight;
	unk168 = 0;
}

void TElecCarapace::behaveToHitGround()
{
	if (unk176)
		unk184 = 1;
	if (mGroundPlane->isWaterSurface())
		kill();
	unk176 = 0;
	unk168 = 1;
	offLiveFlag(LIVE_FLAG_AIRBORNE);
	mVelocity.set(0.0f, 0.0f, 0.0f);
}

void TElecCarapace::kill() { TEnemyAttachment::kill(); }

void TElecCarapace::behaveToHitWall(const TBGCheckData* wall)
{
	if (unk180 <= 0 && TElecNokonoko::mReflectSw) {
		unk180 = 1;
		unk184 = 0;
		unk176 = 1;
		unk175 = 1;
		f32 t  = -1.5f * mLinearVelocity.dot(wall->getNormal());
		mVelocity.x = t * wall->getNormal().x;
		mVelocity.y = 3.0f;
		mVelocity.z = t * wall->getNormal().z;
		mPosition.y = 2.0f + mGroundHeight;
		setGoalPath(TPathNode(mOwner->mPosition));
	}
}

f32 TElecCarapace::getNowGravity()
{
	return ((TElecNokonokoSaveLoadParams*)mOwner->getSaveParam())
	    ->mSLCarapaceGravity.get();
}

void TElecCarapace::appear()
{
	if (unk150 == 0) {
		unk150    = 1;
		mPosition = mOwner->mPosition;
		f32 scale = mOwner->mScaling.x;
		unk164    = scale;
		mScaling.x = mScaling.y = mScaling.z = scale;
		mBodyRadius                          = 50.0f;
		unk170                               = nullptr;
		onHitFlag(HIT_FLAG_NO_COLLISION);
	}
}

void TElecCarapace::shoot()
{
	unk180 = 0;
	if (unk150 != 2) {
		JGeometry::TVec3<f32> target = *gpMarioPos - mPosition;
		MsVECNormalize(&target, &target);
		f32 flyDist = mOwner->mParams->mSLCarapaceFlyDist.get();
		target.x *= flyDist;
		target.y *= mPosition.y;
		target.z *= flyDist;
		target.x += mPosition.x;
		target.y += mPosition.y;
		target.z += mPosition.z;

		unk174 = !unk174;
		unk188 = 0.0f;
		unk150 = 2;
		unk176 = 0;
		unk168 = 0;
		unk184 = 0;
		unk175 = 0;
		offHitFlag(HIT_FLAG_NO_COLLISION);
		mSpine->initWith(&TNerveElecCarapaceMove::theNerve());
		setGoalPath(TPathNode(target));
		setZigParameter();
	}
}

void TElecCarapace::setZigParameter()
{
	f32 cycle = TMsRange<f32>(3.0f, 5.0f).rand();
	// TODO: the target calls TVec3::dot out of line here, unknown why
	unk178 = cycle * (unk104.getPoint() - mPosition).length();
	unk17C = TMsRange<f32>(20.0f, 30.0f).rand();
}

void TElecCarapace::bind()
{
	control();
	TEnemyAttachment::bind();
}

void TElecCarapace::calcRootMatrix()
{
	MsMtxSetXYZRPH(getMActor()->getModel()->getBaseTRMtx(), mPosition.x, mPosition.y,
	               mPosition.z, mRotation.x, mRotation.y + unk188,
	               mRotation.z);
	getMActor()->getModel()->setBaseScale(mScaling);
	SMSGetMSound()->startSoundActor(MSD_SE_EN_DENNOKO_SPARK2, &mPosition, 0,
	                                nullptr, 0, 4);
	if (JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
	        0x17A, getMActor()->getModel()->getAnmMtx(2), 1, this))
		emitter->setGlobalScale(mOwner->mScaling);
	if (JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
	        0x17B, getMActor()->getModel()->getAnmMtx(2), 1, this))
		emitter->setGlobalScale(mOwner->mScaling);
	if (JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
	        0x17C, getMActor()->getModel()->getAnmMtx(2), 1, this))
		emitter->setGlobalScale(mOwner->mScaling);
}

void TElecCarapace::sendMessage()
{
	for (int i = 0; i < getColNum(); ++i) {
		THitActor* hit = getCollision(i);
		if (hit->isActorType(0x80000001)) {
			if (SMS_SendMessageToMario(this, 9)) {
				onHitFlag(HIT_FLAG_NO_COLLISION);
				if (mSpine->getCurrentNerve()
				    != &TNerveElecCarapaceWait::theNerve())
					mSpine->pushNerve(&TNerveElecCarapaceWait::theNerve());
			}
		} else if (hit == mOwner) {
			offHitFlag(HIT_FLAG_NO_COLLISION);
		} else if (hit->isActorType(0x1000001)) {
			// TODO: the results of these rand() calls are discarded, probably
			// an effect that got commented out
			TMsRange<s32> range(0, 360);
			for (int j = 0; j < 5; ++j) {
				rand();
				rand();
				rand();
			}
		} else if (TElecNokonoko::mReflectSw) {
			reflect(hit);
		}
	}
}

BOOL TElecCarapace::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_UNKD) {
		gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_TLS_CHANGE,
		                                            &mPosition, 0, nullptr);
		kill();
	}

	if (message == HIT_MESSAGE_TRAMPLE)
		SMS_SendMessageToMario(this, 9);

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER)
		return TRUE;

	return FALSE;
}

void TElecCarapace::reflect(THitActor* actor)
{
	if (unk170 != actor) {
		unk184 = 0;
		unk170 = actor;
		unk176 = 1;
		unk175 = 0;

		JGeometry::TVec3<f32> dir(actor->mPosition.x - mPosition.x, 0.0f,
		                          actor->mPosition.z - mPosition.z);
		if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f)
			dir.x += 1.0f;
		MsVECNormalize(&dir, &dir);

		JGeometry::TVec3<f32> normal(0.0f, 0.0f, 0.0f);
		if (fabsf(dir.z / dir.x) > 1.0f) {
			if (actor->mPosition.z > mPosition.z)
				normal.z = 1.0f;
			else
				normal.z = -1.0f;
		} else {
			if (actor->mPosition.x > mPosition.x)
				normal.x = 1.0f;
			else
				normal.x = -1.0f;
		}

		f32 t = -7.0f * dir.dot(normal);
		mVelocity.x = dir.x * t;
		mVelocity.y = 2.0f;
		mVelocity.z = dir.z * t;
		mPosition.y = 2.0f + mGroundHeight;

		unk174 = false;
		if ((mVelocity.x > 0.0f && mVelocity.z > 0.0f)
		    || (mVelocity.x < 0.0f && mVelocity.z < 0.0f))
			unk174 = true;

		setGoalPath(TPathNode(mOwner->mPosition));
	}
}

void TElecCarapace::move()
{
	// TODO: UNUSED in the map (size 0x94), contents unknown
}

bool TElecCarapace::isMove()
{
	return mSpine->getCurrentNerve() == &TNerveElecCarapaceWait::theNerve()
	           ? false
	           : true;
}

DEFINE_NERVE(TNerveElecNokonokoShoot, TLiveActor)
{
	TElecNokonoko* self = (TElecNokonoko*)spine->getBody();
	if (spine->getTime() == 0)
		self->setBckAnm(9);

	if (self->isBckAnm(9)) {
		if (self->checkCurAnmEnd(0)) {
			self->setBckAnm(12);
			self->unk198 = 0;
		}
	} else if (self->isBckAnm(12)) {
		if (self->getMActor()->getFrameCtrl(0)->checkPass(60.0f))
			self->mCarapace->appear();

		if (self->getCurAnmFrameNo(0) < 62.0f)
			self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);

		if (self->getMActor()->getFrameCtrl(0)->checkPass(62.0f)) {
			self->mCarapace->shoot();
			self->unk1A4 = 1;
		}

		if (self->checkCurAnmEnd(0)) {
			spine->pushAfterCurrent(&TNerveElecNokonokoCollect::theNerve());
			return TRUE;
		}
	}

	return FALSE;
}

DEFINE_NERVE(TNerveElecNokonokoCollect, TLiveActor)
{
	TElecNokonoko* self = (TElecNokonoko*)spine->getBody();
	if (spine->getTime() == 0) {
		if (!self->isBckAnm(0))
			self->setBckAnm(8);
		self->setGoalPath(TPathNode(self->mCarapace));
	}

	if (self->mCarapace->isMove())
		self->getMActor()->setFrameRate(SMSGetAnmFrameRate(), 0);
	else
		self->getMActor()->setFrameRate(0.0f, 0);

	if (self->isBckAnm(0)) {
		int frame = self->getCurAnmFrameNo(0);
		if (frame > 20)
			self->mCarapace->onHitFlag(HIT_FLAG_NO_COLLISION);

		if (frame > 32) {
			self->unk1A4 = 0;
			self->mCarapace->kill();
		}

		if (self->checkCurAnmEnd(0))
			return TRUE;
	} else if (spine->getTime() > 800
	           || self->mCarapace->checkLiveFlag(LIVE_FLAG_DEAD)) {
		spine->pushAfterCurrent(&TNerveElecNokonokoRebirth::theNerve());
		return TRUE;
	}

	self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);
	return FALSE;
}

DEFINE_NERVE(TNerveElecNokonokoTurn, TLiveActor)
{
	TElecNokonoko* self = (TElecNokonoko*)spine->getBody();
	if (spine->getTime() == 0) {
		self->setBckAnm(16);
		self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));
	}

	if (self->isBckAnm(15)
	    && MsIsInSight(self->mPosition, self->mRotation.y, *gpMarioPos,
	                   ((TSmallEnemyParams*)self->getSaveParam())->mSLSearchLength.get(), 60.0f,
	                   0.0f))
		self->setBckAnm(14);

	if (self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(16))
			self->setBckAnm(15);
		else if (self->isBckAnm(14))
			return TRUE;
	}

	if (self->mPosition.x - self->mCarapace->mPosition.x == 0.0f
	    && self->mPosition.z - self->mCarapace->mPosition.z == 0.0f)
		self->mPosition.x += 1.0f;

	self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);
	if (spine->getTime() > 500)
		return TRUE;

	return FALSE;
}

DEFINE_NERVE(TNerveElecNokonokoFreeze, TLiveActor)
{
	TElecNokonoko* self = (TElecNokonoko*)spine->getBody();
	if (spine->getTime() == 0) {
		if (isCarapaceOn(self)) {
			self->setBckAnm(3);
			gpMarioParticleManager->emitAndBindToMtxPtr(
			    0xCA, self->getMActor()->getModel()->getAnmMtx(0), 0, nullptr);
		} else {
			self->setBckAnm(7);
		}
	}

	if (self->isBckAnm(3) && self->getCurAnmFrameNo(0) < 25.0f) {
		MtxPtr mtx = self->getMActor()->getModel()->getAnmMtx(8);
		self->unk1A8.set(mtx[0][3], mtx[1][3], mtx[2][3]);
		if (JPABaseEmitter* emitter
		    = gpMarioParticleManager->emitAndBindToPosPtr(0x17E, &self->unk1A8,
		                                                  1, self))
			emitter->setGlobalScale(self->mScaling);
	}

	if (self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(7)) {
			self->setBckAnm(6);
		} else if (self->isBckAnm(6)) {
			if (!self->unsetUnk165() && isCarapaceOn(self))
				self->setBckAnm(5);
			else
				self->setBckAnm(6);
		} else {
			return TRUE;
		}
	}

	if (spine->getTime() > 400) {
		spine->reset();
		spine->setDefaultNext();
		spine->pushAfterCurrent(&TNerveElecNokonokoRebirth::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveElecNokonokoRebirth, TLiveActor)
{
	TElecNokonoko* self = (TElecNokonoko*)spine->getBody();
	if (spine->getTime() == 0) {
		self->setBckAnm(13);
		self->unk1A4 = 0;
		gpMarioParticleManager->emitAndBindToPosPtr(
		    PARTICLE_MS_TLS_CHANGE, &self->mCarapace->mPosition, 0, nullptr);
		self->mCarapace->kill();
	}

	if (self->getMActor()->getFrameCtrl(0)->checkPass(88.0f))
		gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_TLS_CHANGE,
		                                            &self->mPosition, 0,
		                                            nullptr);

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveElecNokonokoAttack, TLiveActor)
{
	TElecNokonoko* self = (TElecNokonoko*)spine->getBody();
	if (spine->getTime() == 0 || !self->isBckAnm(3))
		self->setBckAnm(3);

	if (self->checkCurAnmEnd(0))
		return TRUE;

	return FALSE;
}

DEFINE_NERVE(TNerveElecCarapaceMove, TLiveActor)
{
	TElecCarapace* self                 = (TElecCarapace*)spine->getBody();
	TElecNokonokoSaveLoadParams* params = self->mOwner->mParams;
	f32 spinSpeed                       = params->mSLCarapaceSpinSpeed.get();
	f32 speed                           = params->mSLCarapaceSpeed.get();
	f32 turnSpeed                       = params->mSLCarapaceTurnSpeed.get();
	if (self->unk175)
		self->walkToCurPathNode(speed, turnSpeed, 0.0f);
	else
		self->zigzagToCurPathNode(speed, turnSpeed, self->unk178,
		                          self->unk17C);

	self->unk188 += spinSpeed;
	if (self->unk188 > 360.0f)
		self->unk188 -= 360.0f;

	if (self->unk184) {
		f32 catchDist = 64.0f * self->mOwner->mParams->mSLCarapaceSpeed.get();
		if ((self->unk104.getPoint() - self->mPosition).length() < catchDist) {
			if (!self->mOwner->isCatchReady())
				self->mOwner->forceCatchReady();
			spine->pushAfterCurrent(&TNerveElecCarapaceReturn::theNerve());
			return TRUE;
		}
	}

	JGeometry::TVec3<f32> diff = self->getUnk104().getPoint();
	diff -= self->mPosition;
	diff.y = 0.0f;
	if (!self->unk176 && MsVECMag2(&diff) < 100.0f) {
		if (self->unk184) {
			spine->pushAfterCurrent(&TNerveElecCarapaceReturn::theNerve());
			return TRUE;
		}

		self->unk184 = 1;
		self->setGoalPath(TPathNode(self->mOwner->mPosition));
	}

	return FALSE;
}

DEFINE_NERVE(TNerveElecCarapaceWait, TLiveActor)
{
	if (spine->getTime() > 60)
		return TRUE;
	return FALSE;
}

DEFINE_NERVE(TNerveElecCarapaceReturn, TLiveActor)
{
	TElecCarapace* self = (TElecCarapace*)spine->getBody();
	if (spine->getTime() == 0) {
		JGeometry::TVec3<f32> ownerPos = self->mOwner->mPosition;
		self->unk18C.set(0.015625f * (ownerPos.x - self->mPosition.x),
		                 0.015625f * (ownerPos.y - self->mPosition.y),
		                 0.015625f * (ownerPos.z - self->mPosition.z));
		if (self->mOwner->isBckAnm(8))
			self->mOwner->setBckAnm(0);
	}

	if (spine->getTime() < 20) {
		if (self->mOwner->isBckAnm(8))
			self->mOwner->setBckAnm(0);
	}

	if (self->mOwner->isDeadByThunder()) {
		self->mOwner->onLiveFlag(LIVE_FLAG_UNK10000);
		self->mOwner->kill();
		gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_TLS_CHANGE,
		                                            &self->mPosition, 0,
		                                            nullptr);
	}

	self->unk188 += self->mOwner->mParams->mSLCarapaceSpinSpeed.get();
	if (self->unk188 > 360.0f)
		self->unk188 -= 360.0f;

	self->mPosition.x += self->unk18C.x;
	self->mPosition.y += self->unk18C.y;
	self->mPosition.z += self->unk18C.z;

	JGeometry::TVec3<f32> ownerPos = self->mOwner->mPosition;
	if (self->unk18C.x > 0.0f) {
		if (self->mPosition.x > ownerPos.x)
			self->mPosition.x = ownerPos.x;
	} else if (self->mPosition.x < ownerPos.x) {
		self->mPosition.x = ownerPos.x;
	}

	if (self->unk18C.y > 0.0f) {
		if (self->mPosition.y > ownerPos.y)
			self->mPosition.y = ownerPos.y;
	} else if (self->mPosition.y < ownerPos.y) {
		self->mPosition.y = ownerPos.y;
	}

	if (self->unk18C.z > 0.0f) {
		if (self->mPosition.z > ownerPos.z)
			self->mPosition.z = ownerPos.z;
	} else if (self->mPosition.z < ownerPos.z) {
		self->mPosition.z = ownerPos.z;
	}

	if (self->mOwner->checkLiveFlag(LIVE_FLAG_DEAD)) {
		self->mScaling.y *= 0.8f;
		if (self->mScaling.y < 0.01f)
			self->kill();
	}

	return FALSE;
}
