
#include <Enemy/Enemy.hpp>
#include <Enemy/Conductor.hpp>


// rogue include: the original TU opens .rodata with the dummy string pair
// from System/DummyStrings.hpp followed by the four MtxCalcTypeName entries
// from M3DUtil/InfectiousStrings.hpp; without them every string offset in
// this object is shifted.
#include <M3DUtil/InfectiousStrings.hpp>
#include <Enemy/HanaSambo.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/ShadowUtil.hpp>
#include <Player/MarioAccess.hpp>
#include <Enemy/PathNode.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <System/Particles.hpp>
#include <System/EmitterViewObj.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <Map/Map.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/Item.hpp>
#include <MoveBG/ItemManager.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <System/Application.hpp>
#include <System/EmitterViewObj.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// This TU is -inline deferred: the definition order below is the reverse of
// the .text layout in marioEU.MAP.

u8 THanaSambo::mHeadJntIndex   = 3;
u8 THanaSambo::mPollenJntIndex = 6;

static const char* sambo_bastable[] = {
	"/scene/sambo/bas/sambo_down.bas",
	nullptr,
	nullptr,
	nullptr,
	"/scene/sambo/bas/sambo_Fhide.bas",
	nullptr,
	"/scene/sambo/bas/sambo_Fset.bas",
	"/scene/sambo/bas/sambo_Fwait.bas",
	"/scene/sambo/bas/sambo_hit.bas",
	nullptr,
	"/scene/sambo/bas/sambo_Ydown.bas",
};

static const char* sambohead_bastable[] = {
	"/scene/sambohead/bas/flower_shoot.bas",
	"/scene/sambohead/bas/samboHead_crash.bas",
	"/scene/sambohead/bas/samboHead_dance.bas",
	"/scene/sambohead/bas/samboHead_down.bas",
	"/scene/sambohead/bas/samboHead_Fhide.bas",
	"/scene/sambohead/bas/samboHead_hit.bas",
	"/scene/sambohead/bas/samboHead_hit_end.bas",
	"/scene/sambohead/bas/samboHead_jump_end.bas",
	"/scene/sambohead/bas/samboHead_jump_start.bas",
	nullptr,
	"/scene/sambohead/bas/samboHead_set.bas",
	"/scene/sambohead/bas/samboHead_turn.bas",
	nullptr,
};

static TSamboHead* gpCurSamboHead;

TSamboLeaf::TSamboLeaf(TSamboFlowerManager* manager, SDLModelData* data,
                       const char* name)
    : JDrama::TViewObj(name)
    , mModel(nullptr)
    , unk44(0)
    , unk48(manager)
{
	mModel = new SDLModel(data, 3, 1);
}


void TSamboLeaf::generate(JGeometry::TVec3<f32>& position)
{
	mPosition  = position;
	mPosition.y += 10.0f;
	unk44       = true;
}


void TSamboLeaf::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (!unk44)
		return;

	if (cue & CUE_MOVE) {
		mPosition += mVelocity;

		if (mVelocity.y > -20.0f)
			mVelocity.y -= ((TSamboFlowerSaveLoadParams*)unk48->unk38)
			                   ->mSLLeafGravity.get();

		const TBGCheckData* ground;
		if (mPosition.y
		    < gpMap->checkGround(mPosition.x, 20.0f + mPosition.y,
		                         mPosition.z, &ground))
			unk44 = false;
	}

	if (cue & CUE_CALC_ANIM) {
		Mtx mtx;
		MsMtxSetXYZRPH(mtx, mPosition.x, mPosition.y, mPosition.z, 0.0f,
		               unk20.y, 0.0f);

		const JGeometry::TVec3<f32>& rot = MsGetRotFromZaxis(mVelocity);
		Mtx rotMtx;
		MsMtxSetRotZ(rotMtx, -rot.x);
		MTXConcat(mtx, rotMtx, mtx);

		mModel->setBaseTRMtx(mtx);
		mModel->setBaseScale(unk2C);
		mModel->calc();
	}

	if (cue & CUE_CALC_VIEW)
		mModel->viewCalc();

	if (cue & CUE_ENTRY)
		mModel->entry();
}


TSamboFlowerCoinUnit::TSamboFlowerCoinUnit(int count)
    : unk0(nullptr)
    , unk10(0)
    , unk14(count)
    , unk18(nullptr)
    , unk1C(count)
{
	unk0 = new TSamboFlower*[count];
}


void TSamboFlowerCoinUnit::add(TSamboFlower* flower)
{
	if (unk10 < unk14) {
		unk0[unk10]   = flower;
		flower->unk164 = &unk1C;
		unk10++;
	}
}


void TSamboFlowerCoinUnit::checkGenCoin()
{
	// TODO: the retail frame is 0x30 bytes larger and every local sits 0x20
	// higher up; MWCC put the extra slack at the top of the frame, so plain
	// frame padding does not reproduce it.
	if (!unk18)
		return;

	bool bloomed = true;
	for (int i = 0; i < unk10; ++i) {
		if (!unk0[i]->isBloomEnd())
			bloomed = false;
	}

	if (!bloomed)
		return;

	int total = 0;
	for (int i = 0; i < unk10; ++i) {
		TSamboFlower* flower = unk0[i];
		if (flower->unk168)
			total++;
		flower->unk160 = false;
	}

	if (total > 0) {
		Mtx mtx;
		int spawned = 0;
		for (int i = 0; i < unk10; ++i) {
			if (!unk0[i]->unk168)
				continue;

			f32 rate = (f32)spawned / (f32)total;

			JGeometry::TVec3<f32> offset(
			    0.0f, 0.0f, unk0[i]->unk16C->mSLCoinCircleR.get());

			MsMtxSetRotY(mtx, 360.0f * rate);
			MTXMultVec(mtx, &offset, &offset);

			TMapObjBase* coin = unk0[i]->unk168;
			if (coin->isActorType(0x2000000E))
				coin = gpItemManager->makeObjAppear(0x2000000E);

			if (coin) {
				coin->appear();

				JGeometry::TVec3<f32> position = unk4;
				position += offset;
				coin->mPosition = position;

				MsVECNormalize(&offset, &offset);

				TSamboFlowerSaveLoadParams* prm = unk0[i]->unk16C;
				coin->mVelocity.set(
				    offset.x * prm->mSLCoinVelocityXZ.get(),
				    8.0f * rate + prm->mSLCoinVelocityY.get(),
				    offset.z * prm->mSLCoinVelocityXZ.get());
				coin->offLiveFlag(LIVE_FLAG_UNK10);
				spawned++;
			}
		}

		SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_COIN_APPEAR, spawned,
		                                   nullptr, 0);
	}

	unk18 = nullptr;
}


TSamboFlowerSaveLoadParams::TSamboFlowerSaveLoadParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLLeafVelocityXZ, 4.0f)
    , PARAM_INIT(mSLLeafVelocityY, 6.0f)
    , PARAM_INIT(mSLLeafGravity, 0.2f)
    , PARAM_INIT(mSLBudDist, 2000.0f)
    , PARAM_INIT(mSLBloomTimer, 300)
    , PARAM_INIT(mSLCoinCircleR, 100.0f)
    , PARAM_INIT(mSLCoinVelocityXZ, 12.0f)
    , PARAM_INIT(mSLCoinVelocityY, 10.0f)
    , PARAM_INIT(mSLSeedShootRange, 500.0f)
    , PARAM_INIT(mSLSeedShootInterval, 200)
    , PARAM_INIT(mSLSeedGravity, 0.1f)
    , PARAM_INIT(mSLSeedSpeedXZ, 10.0f)
    , PARAM_INIT(mSLSeedSpeedY, 10.0f)
{
	TParams::load(mPrmPath);
}


void TSamboFlowerManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
	unk38 = new TSamboFlowerSaveLoadParams("/enemy/samboflower.prm");
	unk64 = J3DModelLoaderDataBase::loadMaterialTable(
	    JKRGetResource("/scene/samboflower/flower_orange.bmt"));
}


void TSamboFlowerManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "flower.bmd", 0x10220000, 0 },
		{ 0 },
	};
	createModelDataArray(entry);
}


// TODO: weak in the retail binary (inline in the class body) but not inlined
// into createEnemyInstance; defined out of line until that can be reproduced.
#pragma dont_inline on
TSamboFlower::TSamboFlower(const char* name)
    : TSpineEnemy(name)
    , unk150(0)
    , unk154(0)
    , unk158(-1)
    , unk15C(-1)
    , unk160(0)
    , unk164(0)
    , unk168(0)
{
}
#pragma dont_inline off


TSpineEnemy* TSamboFlowerManager::createEnemyInstance()
{
	return new TSamboFlower("サンボフラワー");
}


void TSamboFlower::load(JSUMemoryInputStream& stream)
{
	TSpineEnemy::load(stream);
	stream.read(&unk15C, 4);
	stream.read(&unk158, 4);
	unk160 = 1;
	reset();
	offLiveFlag(LIVE_FLAG_UNK800);
}


void TSamboFlower::loadAfter()
{
	JDrama::TNameRef::loadAfter();
	if (unk15C >= 0) {
		TMapObjBase* coin = TMapObjBaseManager::newAndRegisterObj("coin");
		if (coin)
			unk168 = coin;
	}
}


void TSamboFlower::init(TLiveManager* manager)
{
	mManager = manager;
	manager->manageActor(this);
	setMActorAndKeeper();
	unk130 = 1;
	unk16C = (TSamboFlowerSaveLoadParams*)getSaveParam();
	if (unk16C) {
		mBodyRadius       = unk16C->mSLBodyRadius.get();
		mWallRadius       = unk16C->mSLWallRadius.get();
		mHeadHeight       = unk16C->mSLHeadHeight.get();
		mScaledBodyRadius = mBodyScale * mBodyRadius;
	}
	initHitActor(0x10000027, 1, 0x80000000, mBodyRadius, mHeadHeight,
	             mBodyRadius, mHeadHeight);
	onHitFlag(1);
	offLiveFlag(LIVE_FLAG_UNK400);
	initAnmSound();
	mActorType = 0x10000027;
	unk150     = 0;
	onLiveFlag(LIVE_FLAG_DEAD);
}


void TSamboFlower::setMActorAndKeeper()
{

	
	
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("flower.bmd", 3);
	setMaterialToMActor(mMActor, ((TSamboFlowerManager*)mManager)->unk64);
}


BOOL TSamboFlower::receiveMessage(THitActor* sender, u32 message)
{

	
	
	if (message == 0xF) {
		if (!unk150)
			bloom();
		return TRUE;
	}
	return FALSE;
}


void TSamboFlower::reset()
{
	TSpineEnemy::reset();
	mMActor->setBck("flower_wait");
	offHitFlag(1);
	offLiveFlag(LIVE_FLAG_UNK800);
	offLiveFlag(LIVE_FLAG_DEAD);
	offLiveFlag(LIVE_FLAG_UNK10);
}


void TSamboFlower::moveObject()
{
	if (unk150) {
		if (checkCurAnmEnd(0) && mMActor->checkCurAnm("flower_hit", 0))
			mMActor->setBck("flower_fwait");

		if (unk160) {
			++unk154;
			if (unk154 > unk16C->mSLBloomTimer.get()) {
				if (mMActor->checkCurAnm("flower_fwait", 0)) {
					unk150 = 0;
					if (unk164)
						++*unk164;
					mMActor->setBck("flower_hit");
					s16 end = mMActor->getFrameCtrl(0)->getEnd();
					mMActor->getFrameCtrl(0)->setFrame(end);
					mMActor->setFrameRate(-SMSGetAnmFrameRate(), 0);
				} else if (mMActor->getFrameCtrl(0)->getFrame() < 1.0f) {
					unk154 = 0;
					mMActor->setBck("flower_wait");
				}
			}
		}
	}

	if (!checkLiveFlag(LIVE_FLAG_UNK10))
		mPosition.y = gpMap->checkGround(mPosition.x, 100.0f + mPosition.y,
		                                 mPosition.z, &mGroundPlane);
}


void TSamboFlower::drawObject(JDrama::TGraphics*)
{
	if (!checkLiveFlag(LIVE_FLAG_DEAD)) {
		TCircleShadowRequest request;
		request.mPosition = mPosition;
		request.mRadiusX = request.mRadiusZ = 120.0f;
		gpBindShadowManager->request(request, mActorType & 0xFFFF0000);
		mMActor->setLightData(mGroundPlane, mPosition);
		mMActor->entry();
	}
}


THanaSamboSaveLoadParams::THanaSamboSaveLoadParams(const char* path)
    : TSmallEnemyParams(path)
    , PARAM_INIT(mSLAttackDist, 200.0f)
    , PARAM_INIT(mSLAttackInterval, 200)
    , PARAM_INIT(mSLAppearDist, 800.0f)
    , PARAM_INIT(mSLHideDist, 1000.0f)
    , PARAM_INIT(mSLAttackingTime, 30)
    , PARAM_INIT(mSLHeadAttackRadius, 60.0f)
    , PARAM_INIT(mSLHeadAttackHeight, 20.0f)
    , PARAM_INIT(mSLHeadDamageRadius, 80.0f)
    , PARAM_INIT(mSLHeadDamageHeight, 40.0f)
{
	TParams::load(mPrmPath);
}

THanaSamboManager::THanaSamboManager(const char* name)
    : TSmallEnemyManager(name)
{
}


void THanaSamboManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new THanaSamboSaveLoadParams("/enemy/hanasambo.prm");
}


TSpineEnemy* THanaSamboManager::createEnemyInstance()
{
	return new THanaSambo("ハナサンボ");
}


void THanaSamboManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "sambo.bmd", 0x10220000, 0 },
		{ "samboD.bmd", 0x10220000, 0 },
		{ 0 },
	};
	createModelDataArray(entry);
}


BOOL THanaSamboHead::receiveMessage(THitActor* sender, u32 message)
{
	if (message <= 1) {
		mOwner->kill();
		return TRUE;
	}
	if (message == 0xF) {
		mOwner->waterDamage();
		return TRUE;
	}
	return FALSE;
}


void THanaSamboHead::checkHit()
{
	for (int i = 0; i < mColCount; ++i) {
		if (mCollisions[i]->isActorType(0x80000001))
			SMS_SendMessageToMario(this, 0xE);
	}
}


THanaSambo::THanaSambo(const char* name)
    : TSmallEnemy(name)
    , unk194(0)
    , unk198(0)
    , unk1A8(0)
    , unk1AC(0)
{
}


void THanaSambo::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	reset();
	setGoalPath(TPathNode((THitActor*)gpMarioAddress));
}


void THanaSambo::init(TLiveManager* manager)
{
	TSmallEnemy::init(manager);
	mActorType = 0x1000001A;
	unk150     = 0x11;
	unk198     = (THanaSamboSaveLoadParams*)getSaveParam();
	mSpine->initWith(&TNerveHanaSamboHide::theNerve());
	unk1AC = new TMBindShadowBody(this, getModel(), 1.5f);
	setGoalPath(TPathNode((THitActor*)gpMarioAddress));

	if (mInstanceIndex == 0) {
		// TODO: what this loop did is unknown; it only walks the joint count of
		// the death model.
		for (u8 i = 0; i < getActorKeeper()
		                       ->getMActor("samboD.bmd")
		                       ->getModel()
		                       ->getModelData()
		                       ->getJointNum();
		     ++i) {
		}
	}

	unk194 = new THanaSamboHead;
	((TIdxGroupObj*)JDrama::TNameRefGen::search("敵グループ"))->add(unk194);
	unk194->initHitActor(0x1000001B, 2, 0x80000000,
	                     unk198->mSLHeadAttackRadius.get() * mBodyScale,
	                     unk198->mSLHeadAttackHeight.get() * mBodyScale,
	                     unk198->mSLHeadDamageRadius.get() * mBodyScale,
	                     unk198->mSLHeadDamageHeight.get() * mBodyScale);
	unk194->mOwner = this;
}


void THanaSambo::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 2);
	mMActor       = mMActorKeeper->createMActor("sambo.bmd", 3);
	mMActorKeeper->createMActor("samboD.bmd", 3);
}


void THanaSambo::moveObject()
{
	TSmallEnemy::moveObject();
	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		unk194->mPosition = mPosition;
	} else {
		MtxPtr mtx = getModel()->getAnmMtx(mHeadJntIndex);
		unk194->mPosition.x = mtx[0][3];
		unk194->mPosition.y = mtx[1][3];
		unk194->mPosition.z = mtx[2][3];
	}

	THanaSamboHead* head = unk194;
	head->checkHit();

	if (!checkLiveFlag(LIVE_FLAG_HIDDEN)
	    || mSpine->getCurrentNerve() == &TNerveHanaSamboHide::theNerve()) {
		mPosition.x = unk19C.x;
		mPosition.z = unk19C.z;
	}
}


void THanaSambo::drawObject(JDrama::TGraphics* graphics)
{
	TLiveActor::drawObject(graphics);
	if (!(mLiveFlag & (LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8)))
		unk1AC->entryDrawShadow();
}


void THanaSambo::kill()
{
	if (!checkLiveFlag(LIVE_FLAG_DEAD)) {
		mHitPoints = 1;
		if (isBckAnm(7))
			unk18C = 3;
		if (mSpine->getCurrentNerve() != &TNerveHanaSamboDie::theNerve()) {
			mSpine->reset();
			mSpine->setNext(&TNerveHanaSamboDie::theNerve());
			mSpine->pushAfterCurrent(&TNerveHanaSamboDie::theNerve());
		}
		unk194->kill();
		onLiveFlag(LIVE_FLAG_UNK40);
	}
}


void THanaSambo::reset()
{
	TSmallEnemy::reset();
	unk165 = false;
	unk19C = mPosition;
}


void THanaSambo::setWaitAnm()
{
	setBckAnm(7);
	unk1B0 = 0;
}


void THanaSambo::setDeadAnm()
{
	mMActor = getActorKeeper()->getMActor("samboD.bmd");
	if (unk1B0)
		setBckAnm(10);
	else
		setBckAnm(0);
	unk194->kill();
	unk1B0 = 0;
	onLiveFlag(LIVE_FLAG_UNK8);
}


const char** THanaSambo::getBasNameTable() const { return sambo_bastable; }


void THanaSambo::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);
	if (!(mLiveFlag & (LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN | LIVE_FLAG_CLIPPED_OUT)))
		unk194->THitActor::perform(cue, graphics);
}


void THanaSambo::createPollen()
{

	// locals (MWCC stack-padding quirk). TODO: find the real locals.
	
	
	JGeometry::TVec3<f32> pos;
	Mtx transform;
	MtxPtr jointMtx = mMActor->getModel()->getAnmMtx(mPollenJntIndex);
	pos.x = jointMtx[0][3];
	pos.y = jointMtx[1][3];
	pos.z = jointMtx[2][3];
	MsMtxSetRotRPH(transform, 0.0f, 0.0f, -90.0f);
	PSMTXConcat(jointMtx, transform, transform);

	JPABaseEmitter* emitter;
	if (mSpine->getCurrentNerve() == &TNerveHanaSamboWait::theNerve()) {
		emitter = gpMarioParticleManager->emit(0xB2, &pos, 0, nullptr);
		transform[1][3] += 200.0f;
	} else {
		emitter = gpMarioParticleManager->emit(0xB3, &pos, 0, nullptr);
		JGeometry::TVec3<f32> offset(0.0f, 0.0f, 200.0f);
		Mtx rotation;
		MsMtxSetRotRPH(rotation, mRotation.x, mRotation.y, mRotation.z);
		PSMTXMultVec(rotation, (Vec*)&offset, (Vec*)&offset);
		transform[0][3] += offset.x;
		transform[1][3] += offset.y;
		transform[2][3] += offset.z;
	}
	if (emitter)
		emitter->setGlobalSRTMatrix(transform);
	
	
}


bool THanaSambo::isCollidMove(THitActor*) { return false; }

bool THanaSambo::isHitValid(u32 message)
{
	if (message == 0xB) {
		onLiveFlag(LIVE_FLAG_HIDDEN);
		return true;
	}
	return false;
}

void THanaSambo::behaveToWater(THitActor*) { }


void THanaSambo::initFlower()
{
	if (!unk1A8) {
		unk1A8 = (TSamboFlower*)gpConductor->makeOneEnemyAppear(
		    mPosition, "サンボフラワーマネージャー", 1);
		unk1A8->reset();
	}
	unk1A8->onLiveFlag(LIVE_FLAG_UNK10);
	unk1A8->offLiveFlag(LIVE_FLAG_DEAD);
	unk1A8->mPosition   = mPosition;
	unk1A8->mPosition.y = mGroundHeight;
}


void THanaSambo::setAttackAnm()
{
	setBckAnm(3);
	unk1B0 = 1;
}


void THanaSambo::waterDamage()
{
	unk165 = true;

	if (changeByJuice())
		return;

	if (mSpine->getCurrentNerve() != &TNerveHanaSamboWait::theNerve())
		return;

	mSpine->pushNerve(&TNerveHanaSamboFreeze::theNerve());
}


BOOL TNerveHanaSamboAppear::execute(TSpineBase<TLiveActor>* spine) const
{
	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->offLiveFlag(LIVE_FLAG_HIDDEN);
		self->offHitFlag(1);
		self->setBckAnm(6);
		gpMarioParticleManager->emit(0xB6, &self->mPosition, 0, nullptr);
		gpMarioParticleManager->emit(0xB7, &self->mPosition, 0, nullptr);
		self->unk1A8->hide();
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveHanaSamboWait::theNerve());
		self->setWaitAnm();
		if (gpMarioPos->y < 100.0f + self->mPosition.y) {
			self->updateSquareToMario();
			if (self->unk198->mSLAttackDist.get()
			        * self->unk198->mSLAttackDist.get()
			    > self->getDistToMarioSquared())
				spine->pushAfterCurrent(&TNerveHanaSamboAttack::theNerve());
		}
		return TRUE;
	}

	self->walkToCurPathNode(0.0f, 3.0f * self->getTurnSpeed(), 0.0f);
	return FALSE;
}

BOOL TNerveHanaSamboWait::execute(TSpineBase<TLiveActor>* spine) const
{
	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0)
		self->setWaitAnm();

	if (!self->checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)
	    && self->getMActor()->getFrameCtrl(0)->checkPass(16.0f))
		self->createPollen();

	if (spine->getTime() > self->unk198->mSLAttackInterval.get()
	    && gpMarioPos->y < 100.0f + self->mPosition.y) {
		self->updateSquareToMario();
		if (self->unk198->mSLAttackDist.get() * self->unk198->mSLAttackDist.get()
		    > self->getDistToMarioSquared()) {
			spine->pushAfterCurrent(&TNerveHanaSamboAttack::theNerve());
			return TRUE;
		}
	}

	self->updateSquareToMario();
	if (self->unk198->mSLHideDist.get() * self->unk198->mSLHideDist.get()
	    < self->getDistToMarioSquared()) {
		spine->pushAfterCurrent(&TNerveHanaSamboHide::theNerve());
		return TRUE;
	}

	self->walkToCurPathNode(0.0f, self->getTurnSpeed(), 0.0f);
	return FALSE;
}


BOOL TNerveHanaSamboAttack::execute(TSpineBase<TLiveActor>* spine) const
{

	
	
	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setAttackAnm();
	} else if (self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(3)) {
			self->createPollen();
			SMSGetMSound()->startSoundActor(0x291B, &self->mPosition, 0, nullptr,
			                                0, 4);
			self->setBckAnm(1);
		} else if (self->isBckAnm(1)) {
			if (spine->getTime() > self->unk198->mSLAttackingTime.get()
			    && !self->unsetUnk165())
				self->setBckAnm(2);
			else
				self->setBckAnm(1);
		} else {
			spine->pushAfterCurrent(&TNerveHanaSamboWait::theNerve());
			return TRUE;
		}
	}
	return FALSE;
}


void TSamboFlower::bloom()
{
	unk150 = 1;
	unk154 = 0;
	gpMarioParticleManager->emit(0xB2, &mPosition, 0, nullptr);
	mMActor->setBck("flower_hit");
	if (unk160 && unk164) {
		--*unk164;
		u32 id = MSD_SE_OBJ_FLOWER_OPEN_0 + *unk164;
		SMSGetMSound()->startSoundActor(id, &mPosition, 0, nullptr, 0, 4);
	}
}


bool TSamboFlower::isBloomEnd() { return mMActor->checkCurAnm("flower_fwait", 0); }


void TSamboFlower::hide()
{
	onHitFlag(1);
	mMActor->setBck("flower_fwait");
	onLiveFlag(LIVE_FLAG_DEAD);
}


void TSamboFlowerManager::loadAfter()
{
	void* leafRes
	    = JKRGetResource("/scene/samboflower/leaf.bmd");
	SDLModelData* leafData
	    = new SDLModelData(J3DModelLoaderDataBase::load(leafRes, 0x10210000));

	unk60 = new TSamboLeaf*[0x12];
	for (int i = 0; i < 0x12; i++)
		unk60[i] = new TSamboLeaf(this, leafData);

	unk58 = 0;
	for (int i = 0; i < gpItemManager->getObjNum(); i++) {
		if (strstr(gpItemManager->getObj(i)->getName(), "コイン（フラワー用）"))
			unk58++;
	}

	unk54 = new TSamboFlowerCoinUnit*[unk58];

	int* counts = new int[unk58];
	for (int i = 0; i < unk58; i++)
		counts[i] = 0;

	for (int i = 0; i < getObjNum(); i++) {
		if (strstr(getObj(i)->getName(), "フラワー（コイン用）")) {
			TSamboFlower* flower = (TSamboFlower*)getObj(i);
			if (flower->unk158 < unk58)
				counts[flower->unk158]++;
		}
	}

	for (int i = 0; i < unk58; i++)
		unk54[i] = new TSamboFlowerCoinUnit(counts[i]);

	for (int i = 0; i < gpItemManager->getObjNum(); i++) {
		if (strstr(gpItemManager->getObj(i)->getName(),
		           "コイン（フラワー用）")) {
			TFlowerCoin* coin = (TFlowerCoin*)gpItemManager->getObj(i);
			int idx           = coin->unk158;
			if (idx < unk58) {
				unk54[idx]->unk18 = coin;
				unk54[idx]->unk4  = coin->mPosition;
				coin->kill();
			}
		}
	}

	for (int i = 0; i < getObjNum(); i++) {
		if (strstr(getObj(i)->getName(), "フラワー（コイン用）")) {
			TSamboFlower* flower = (TSamboFlower*)getObj(i);
			if (flower->unk158 < unk58)
				unk54[flower->unk158]->add(flower);
		}
	}

	JDrama::TNameRef::loadAfter();
}


void TSamboFlowerManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (unk58 > 0 && (cue & 2)) {
		for (int i = 0; i < unk58; ++i)
			unk54[i]->checkGenCoin();
	}
	TEnemyManager::perform(cue, graphics);
	for (int i = 0; i < 18; ++i)
		unk60[i]->perform(cue, graphics);
}


void TSamboFlowerManager::dropLeaf(JGeometry::TVec3<f32>& position,
                                   JGeometry::TVec3<f32>& scaling)
{
	f32 rotY[] = { 0.0f, 120.0f, 240.0f };

	int dropped = 0;
	for (int i = 0; i < 0x12; i++) {
		TSamboLeaf* leaf = unk60[i];
		if (!leaf->unk44) {
			leaf->generate(position);

			TSamboFlowerSaveLoadParams* prm
			    = (TSamboFlowerSaveLoadParams*)unk38;
			f32 velXZ = prm->mSLLeafVelocityXZ.get();
			f32 velY  = prm->mSLLeafVelocityY.get();
			TMsRange<f32> speedXZ(velXZ, 1.2f * velXZ);
			TMsRange<f32> speedY(velY, 1.2f * velY);

			JGeometry::TVec3<f32> velocity(0.0f, speedY.rand(), speedXZ.rand());

			Mtx mtx;
			MsMtxSetRotRPH(mtx, 0.0f, rotY[i], 0.0f);
			MTXMultVec(mtx, &velocity, &velocity);

			unk60[i]->unk20.set(0.0f, rotY[i] - 90.0f, 0.0f);
			unk60[i]->mVelocity = velocity;
			unk60[i]->unk2C     = scaling;
			dropped++;
		}

		if (dropped >= 3)
			break;
	}
}


BOOL TNerveHanaSamboHide::execute(TSpineBase<TLiveActor>* spine) const
{
	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(4);
		gpMarioParticleManager->emit(0xB8, &self->mPosition, 0, nullptr);
		gpMarioParticleManager->emit(0xB9, &self->mPosition, 0, nullptr);
	}

	if (spine->getTime() == 75)
		self->initFlower();

	if (self->checkCurAnmEnd(0)) {
		self->initFlower();
		self->onHitFlag(1);
		self->setBckAnm(6);
		self->getMActor()->setFrameRate(0.0f, 0);
		self->onLiveFlag(LIVE_FLAG_HIDDEN);
	}

	self->updateSquareToMario();
	if (self->getDistToMarioSquared()
	    < self->unk198->mSLAppearDist.get()
	          * self->unk198->mSLAppearDist.get()) {
		spine->pushAfterCurrent(&TNerveHanaSamboAppear::theNerve());
		return TRUE;
	}

	return FALSE;
}


BOOL TNerveHanaSamboDie::execute(TSpineBase<TLiveActor>* spine) const
{
	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->onHitFlag(1);
		self->setDeadAnm();
	} else if (self->checkCurAnmEnd(0) || spine->getTime() > 300) {
		static int jIndexTable[] = { 1, 3, 4, 5 };

		for (int i = 0; i < 4; ++i) {
			MtxPtr mtx = self->getMActor()->getModel()->getAnmMtx(jIndexTable[i]);
			self->unk1B4[i].set(mtx[0][3], mtx[1][3], mtx[2][3]);

			if (JPABaseEmitter* emitter = gpMarioParticleManager->emit(
			        0xE4, &self->unk1B4[i], 0, nullptr))
				emitter->setGlobalScale(self->mScaling);

			if (JPABaseEmitter* emitter = gpMarioParticleManager->emit(
			        0xE6, &self->unk1B4[i], 0, nullptr))
				emitter->setGlobalScale(self->mScaling);
		}

		self->onLiveFlag(LIVE_FLAG_DEAD);
		self->onLiveFlag(LIVE_FLAG_UNK8);
		self->offLiveFlag(LIVE_FLAG_HIDDEN);
		self->offLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
		self->mHolder = nullptr;
		self->stopAnmSound();
		self->onHitFlag(1);
		spine->reset();
		spine->setNext(&TNerveSmallEnemyDie::theNerve());
		spine->pushAfterCurrent(spine->getDefault());
		self->genRandomItem();
	}

	return FALSE;
}


BOOL TNerveHanaSamboFreeze::execute(TSpineBase<TLiveActor>* spine) const
{
	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0)
		self->setBckAnm(5);

	if (self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(5)) {
			if (self->unsetUnk165())
				self->setBckAnm(8);
		} else {
			if (self->unsetUnk165())
				self->setBckAnm(8);
			else
				return TRUE;
		}
	}
	return FALSE;
}


u8 TSamboHead::mBodyJntIndex;

static int SamboHeadRollCallback(J3DNode* node, int unk)
{

	
	
	// TODO: the retail frame puts the axis/rate triple at +0xac, `up` at
	// +0xdc, `side` at +0xe8, `velocity` at +0xf4 and the Mtx at +0x100; MWCC
	// hands us a different set of slots, so every lfs/stfs in the three
	// dot-product blocks below still differs in its offset (90.4% overall).
	if (unk == 0) {
		if (!gpCurSamboHead || !gpCurSamboHead->isUseCallBack())
			return TRUE;

		J3DJoint* joint = (J3DJoint*)node;
		MtxPtr anmMtx = gpCurSamboHead->getModel()->getAnmMtx(joint->getJntNo());

		JGeometry::TVec3<f32> velocity = gpCurSamboHead->mVelocity;
		if (velocity.x == 0.0f && velocity.z == 0.0f)
			velocity.x = 0.001f;

		JGeometry::TVec3<f32> side;
		JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
		VECCrossProduct(&up, &velocity, &side);

		JGeometry::TVec3<f32> zAxis(anmMtx[0][2], anmMtx[1][2], anmMtx[2][2]);
		JGeometry::TVec3<f32> yAxis(anmMtx[0][1], anmMtx[1][1], anmMtx[2][1]);
		JGeometry::TVec3<f32> xAxis(anmMtx[0][0], anmMtx[1][0], anmMtx[2][0]);

		f32 rateZ = 0.0f;
		if (zAxis.squared() != 0.0f)
			rateZ = side.dot(zAxis) / zAxis.squared();

		f32 rateY = 0.0f;
		if (yAxis.squared() != 0.0f)
			rateY = side.dot(yAxis) / yAxis.squared();

		f32 rateX = 0.0f;
		if (xAxis.squared() != 0.0f)
			rateX = side.dot(xAxis) / xAxis.squared();

		JGeometry::TVec3<f32> axis(rateX, rateY, rateZ);

		Mtx rotMtx;
		MTXRotAxisRad(rotMtx, &axis, 0.017453292f * gpCurSamboHead->unk1AC);
		MTXConcat(anmMtx, rotMtx, anmMtx);
		MTXConcat(J3DSys::mCurrentMtx, rotMtx, J3DSys::mCurrentMtx);
	}
	return TRUE;
}


TSamboHeadSaveLoadParams::TSamboHeadSaveLoadParams(const char* path)
    : TWalkerEnemyParams(path)
    , PARAM_INIT(mSLAppearDist, 800.0f)
    , PARAM_INIT(mSLHideDist, 1000.0f)
    , PARAM_INIT(mSLMoveDist, 100.0f)
    , PARAM_INIT(mSLMoveGravity, 0.1f)
    , PARAM_INIT(mSLJumpSp, 10.0f)
    , PARAM_INIT(mSLJumpPrepareTime, 20)
    , PARAM_INIT(mSLHitJumpSpXZ, 12.0f)
    , PARAM_INIT(mSLHitJumpSpY, 10.0f)
    , PARAM_INIT(mSLHitJumpGravity, 1.0f)
    , PARAM_INIT(mSLHitJumpSpRateXZ, 0.3f)
    , PARAM_INIT(mSLHitJumpSpRateY, 0.3f)
    , PARAM_INIT(mSLJumpAngY, 30.0f)
{
	TParams::load(mPrmPath);
}

TSamboHeadManager::TSamboHeadManager(const char* name)
    : TSmallEnemyManager(name)
{
	gpCurSamboHead = nullptr;
}


void TSamboHeadManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TSamboHeadSaveLoadParams("/enemy/sambohead.prm");
}


void TSamboHeadManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "samboHead.bmd", 0x10220000, 0 },
		{ 0 },
	};
	createModelDataArray(entry);
}


TSpineEnemy* TSamboHeadManager::createEnemyInstance()
{
	return new TSamboHead("サンボヘッド");
}


TSamboHead::TSamboHead(const char* name)
    : TWalkerEnemy(name)
{
	unk194 = 0;
	unk198 = 0;
	unk19C = 0;
	unk1AC = 0.0f;
	unk1B0 = 0;
}


void TSamboHead::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	reset();
	setGoalPath(TPathNode((THitActor*)gpMarioAddress));
}


void TSamboHead::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("samboHead.bmd", 3);
}


void TSamboHead::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x1000001B;
	unk150     = 0x11;
	unk194     = (TSamboHeadSaveLoadParams*)getSaveParam();
	mSpine->initWith(&TNerveSamboHeadHide::theNerve());
	setGoalPath(TPathNode((THitActor*)gpMarioAddress));
	mMActor->setJointCallback(mBodyJntIndex, SamboHeadRollCallback);
}


void TSamboHead::reset()
{
	gpCurSamboHead = this;
	TWalkerEnemy::reset();
	unk165 = false;
	unk1B0 = 0;
	unk19C = 0;
	unk1AC = 0.0f;
	offLiveFlag(LIVE_FLAG_UNK8);
}


void TSamboHead::kill()
{
	mHitPoints = 1;
	if (mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()) {
		mSpine->reset();
		mSpine->setNext(&TNerveSmallEnemyDie::theNerve());
		mSpine->pushAfterCurrent(&TNerveSmallEnemyDie::theNerve());
	}
	onLiveFlag(LIVE_FLAG_UNK40);
}


void TSamboHead::behaveToWater(THitActor*)
{
	if (mSpine->getCurrentNerve() == &TNerveSamboHeadHide::theNerve())
		return;

	if (mSpine->getCurrentNerve() == &TNerveSamboHeadAppear::theNerve())
		return;

	if (mSpine->getCurrentNerve() == &TNerveSamboHeadHitWall::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveSmallEnemyDie::theNerve())
		return;

	JGeometry::TVec3<f32> velocity = mLinearVelocity;
	velocity.y                     = 0.0f;

	JGeometry::TVec3<f32> jump(mPosition.x - gpMarioPos->x, 0.0f,
	                           mPosition.z - gpMarioPos->z);
	MsVECNormalize(&jump, &jump);
	jump.scale(unk194->mSLHitJumpSpXZ.get());
	jump.y = unk194->mSLHitJumpSpY.get();

	if (mSpine->getCurrentNerve() != &TNerveSamboHeadHitWater::theNerve())
		mSpine->pushNerve(&TNerveSamboHeadHitWater::theNerve());
	else
		jump.add(velocity);

	setVelocity(jump);
	mPosition.y += 2.0f;
	onLiveFlag(LIVE_FLAG_AIRBORNE);
}


void TSamboHead::attackToMario()
{
	sendAttackMsgToMario();

	if (isAirborne()) {
		JGeometry::TVec3<f32> velocity(mPosition.x - gpMarioPos->x, 10.0f,
		                               mPosition.z - gpMarioPos->z);
		MsVECNormalize(&velocity, &velocity);
		velocity.scale(8.0f);
		setVelocity(velocity);
	} else if (mSpine->getCurrentNerve()
	           == &TNerveSamboHeadAttack::theNerve()) {
		mSpine->pushNerve(&TNerveSmallEnemyFreeze::theNerve());
	}
}


f32 TSamboHead::getGravityY() const
{
	f32 gravity = mGravity;
	if (mSpine->getCurrentNerve() == &TNerveSamboHeadAttack::theNerve())
		gravity = unk194->mSLMoveGravity.get();
	if (mSpine->getCurrentNerve() == &TNerveSamboHeadHitWater::theNerve())
		gravity = unk194->mSLHitJumpGravity.get();
	return gravity;
}


void TSamboHead::setDeadAnm() { setBckAnm(3); }


void TSamboHead::setAfterDeadEffect()
{

	
	
	JPABaseEmitter* emitter;
	if (isBckAnm(1)) {
		emitter = gpMarioParticleManager->emit(0xE5, &mPosition, 0, nullptr);
		if (emitter)
			{
			emitter->mGlobalDynamicsScale.set(1.5f, 1.5f, 1.5f);
			emitter->mGlobalParticleScale.set(1.5f, 1.5f, 1.5f);
		}
	} else {
		emitter = gpMarioParticleManager->emit(0xE4, &mPosition, 0, nullptr);
		if (emitter)
			{
			emitter->mGlobalDynamicsScale.set(1.5f, 1.5f, 1.5f);
			emitter->mGlobalParticleScale.set(1.5f, 1.5f, 1.5f);
		}
	}
	emitter = gpMarioParticleManager->emit(0xE6, &mPosition, 0, nullptr);
	if (emitter)
		{
			emitter->mGlobalDynamicsScale.set(1.5f, 1.5f, 1.5f);
			emitter->mGlobalParticleScale.set(1.5f, 1.5f, 1.5f);
		}

	SMSGetMSound()->startSoundActor(MSD_SE_EN_COMMON_SMOKE, &mPosition, 0,
	                                nullptr, 0, 4);
}


void TSamboHead::setCrashAnm()
{
	setBckAnm(1);

	JGeometry::TVec3<f32> scale(1.5f);

	if (JPABaseEmitter* emitter = gpMarioParticleManager->emitWithRotate(
	        0xE2, &mPosition, 0, DEG2SHORTANGLE(mRotation.y), 0, 0, nullptr))
		emitter->setGlobalScale(scale);

	if (JPABaseEmitter* emitter = gpMarioParticleManager->emitWithRotate(
	        0xE3, &mPosition, 0, DEG2SHORTANGLE(mRotation.y), 0, 0, nullptr)) {
		emitter->setGlobalScale(scale);
		SMSSetEmitterPolColor(emitter, 6);
	}
}


void TSamboHead::calcRootMatrix()
{
	gpCurSamboHead = this;
	TSpineEnemy::calcRootMatrix();
}


void TSamboHead::genEventCoin()
{
	if (isBckAnm(1)) {
		Mtx mtx;
		MtxPtr matrix = mtx;
		for (int i = 0; i < 3; ++i) {
			MsMtxSetRotY(matrix, mRotation.y - 60.0f + 60.0f * i);
			JGeometry::TVec3<f32> offset(0.0f, 0.0f, 100.0f);
			MTXMultVec(matrix, &offset, &offset);

			TMapObjBase* coin;
			if (i == 1 && mCoin) {
				coin = mCoin;
				if (coin->isActorType(0x2000000E))
					coin = gpItemManager->makeObjAppear(0x2000000E);

				if (coin) {
					coin->appear();
					coin->mPosition = mPosition;
				}
			} else {
				coin = gpItemManager->makeObjAppear(
				    mPosition.x + offset.x, mPosition.y, mPosition.z + offset.z,
				    0x2000000E, true);
			}

			if (coin) {
				coin->mPosition.y = mPosition.y;
				MsVECNormalize(&offset, &offset);
				coin->mVelocity.set(offset.x * 4.0f,
				                    TMsRange<f32>(8.0f, 16.0f).rand(),
				                    offset.z * 4.0f);
				coin->offLiveFlag(LIVE_FLAG_UNK10);
			}
		}
	} else {
		TSmallEnemy::genEventCoin();
	}
}


bool TSamboHead::isUseCallBack()
{
	if (mSpine->getCurrentNerve() == &TNerveSamboHeadAttack::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveSamboHeadHitWater::theNerve()
	    || mSpine->getCurrentNerve()
	           == &TNerveSamboHeadRecoverWater::theNerve())
		return true;

	return false;
}


const char** TSamboHead::getBasNameTable() const { return sambohead_bastable; }


void TSamboHead::initFlower()
{
	if (!unk198) {
		unk198 = (TSamboFlower*)gpConductor->makeOneEnemyAppear(
		    mPosition, "サンボフラワーマネージャー", 1);
		unk198->reset();
	}
	unk198->offHitFlag(1);
	unk198->onLiveFlag(LIVE_FLAG_UNK10);
	unk198->offLiveFlag(LIVE_FLAG_DEAD);
	unk198->mPosition.y = mGroundHeight;
	unk198->mPosition   = mPosition;
}


BOOL TNerveSamboHeadAppear::execute(TSpineBase<TLiveActor>* spine) const
{
	TSamboHead* self = (TSamboHead*)spine->getBody();

	if (spine->getTime() == 0) {
		self->offLiveFlag(LIVE_FLAG_HIDDEN);
		self->offHitFlag(1);
		if (self->unk198->unk150) {
			self->unk198->bloom();
			self->setBckAnm(10);
		} else {
			self->setBckAnm(10);
		}
		gpMarioParticleManager->emit(0xB6, &self->mPosition, 0, nullptr);
		gpMarioParticleManager->emit(0xB7, &self->mPosition, 0, nullptr);
		self->unk198->hide();
	}

	if (spine->getTime() == 20) {
		((TSamboFlowerManager*)self->unk198->mManager)
		    ->dropLeaf(self->mPosition, self->mScaling);
	}

	if (self->checkCurAnmEnd(0)) {
		self->setBckAnm(12);
		spine->pushAfterCurrent(&TNerveSamboHeadAttack::theNerve());
		return TRUE;
	}
	return FALSE;
}


BOOL TNerveSamboHeadAttack::execute(TSpineBase<TLiveActor>* spine) const
{
	TSamboHead* self = (TSamboHead*)spine->getBody();

	if (!self->isAirborne()) {
		if (self->unk19C > self->unk194->mSLJumpPrepareTime.get()
		    && self->checkCurAnmEnd(0)) {
			self->unk19C = 0;
			self->updateSquareToMario();

			JGeometry::TVec3<f32> goal = self->getUnk104().getPoint();
			goal.set(gpMarioPos->x - self->mPosition.x, 0.0f,
			         gpMarioPos->z - self->mPosition.z);
			if (goal.x == 0.0f && goal.y == 0.0f && goal.z == 0.0f)
				goal.x += 1.0f;

			MsVECNormalize(&goal, &goal);

			f32 moveDist = self->unk194->mSLMoveDist.get();
			goal.x       = goal.x * moveDist + self->mPosition.x;
			goal.z       = goal.z * moveDist + self->mPosition.z;
			goal.y       = self->mPosition.y;

			f32 jumpSp = self->unk194->mSLJumpSp.get();
			self->setVelocity(
			    self->calcVelocityToJumpToY(goal, jumpSp, self->getGravityY()));
			self->mPosition.y += 2.0f;
			self->onLiveFlag(LIVE_FLAG_AIRBORNE);
			self->setBckAnm(8);
		} else {
			self->unk19C++;
		}

		if (self->checkCurAnmEnd(0) && self->isBckAnm(7))
			self->setBckAnm(12);

		self->getMActor()->setFrameRate(SMSGetAnmFrameRate(), 0);
	} else {
		JGeometry::TVec3<f32> velocity = self->mLinearVelocity;
		if (velocity.y < 0.0f && self->isBckAnm(8)) {
			self->setBckAnm(7);
			self->getMActor()->setFrameRate(0.0f, 0);
		}
	}

	if (self->mPosition.y > 30.0f + self->mGroundHeight) {
		f32 limit                      = self->unk194->mSLJumpAngY.get();
		JGeometry::TVec3<f32> velocity = self->mLinearVelocity;
		self->unk1AC = MsClamp(MsGetRotFromZaxis(velocity).x, -limit, limit);
	} else {
		self->unk1AC *= 0.8f;
	}

	f32 turnSpeed = self->getTurnSpeed();
	if (self->isAirborne())
		turnSpeed = 5.0f;

	self->walkToCurPathNode(0.0f, turnSpeed, 0.0f);
	return FALSE;
}


BOOL TNerveSamboHeadHide::execute(TSpineBase<TLiveActor>* spine) const
{
	TSamboHead* self = (TSamboHead*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(4);
		self->onHitFlag(1);
		gpMarioParticleManager->emit(0xB8, &self->mPosition, 0, nullptr);
		gpMarioParticleManager->emit(0xB9, &self->mPosition, 0, nullptr);
	} else if (self->checkCurAnmEnd(0)) {
		self->onLiveFlag(LIVE_FLAG_HIDDEN);
		self->setBckAnm(12);
		self->initFlower();
	} else {
		if (self->isFindMario(1.0f)) {
			self->updateSquareToMario();
			f32 sq = self->unk194->mSLAppearDist.get();
			sq *= sq;
			if (self->getDistToMarioSquared() < sq) {
				spine->pushAfterCurrent(&TNerveSamboHeadAppear::theNerve());
				return TRUE;
			}
		}
	}
	self->walkToCurPathNode(0.0f, self->getTurnSpeed(), 0.0f);
	return FALSE;
}


BOOL TNerveSamboHeadHitWater::execute(TSpineBase<TLiveActor>* spine) const
{
	TSamboHead* self = (TSamboHead*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(5);
		self->unk1A0 = self->mLinearVelocity;
	}

	if (self->isHitWallInBound()) {
		self->unk1AC = 0.0f;
		if (self->unk1B0)
			self->mRotation.y += 180.0f;

		spine->reset();
		spine->setNext(&TNerveSamboHeadHitWall::theNerve());
		spine->pushAfterCurrent(&TNerveSamboHeadHitWall::theNerve());
		return TRUE;
	}

	f32 limit = self->unk194->mSLJumpAngY.get();
	if (self->isBckAnm(6)) {
		if (spine->getTime() < 100)
			self->unk1AC = MsClamp(self->unk1AC - 3.0f, -limit, limit);
		else
			self->unk1AC = MsClamp(3.0f + self->unk1AC, -limit, 0.0f);
	} else {
		self->unk1AC = MsClamp(3.0f + self->unk1AC, -limit, limit);
	}

	if (self->isBckAnm(5) && !self->isAirborne())
		self->setBckAnm(7);

	if (self->isBckAnm(6)) {
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->mPosition.y = 1.0f + self->mGroundHeight;
		self->setVelocity(self->unk1A0);
	}

	if (!self->isAirborne())
		self->setBckAnm(6);

	if (self->checkCurAnmEnd(0) && self->isBckAnm(6)) {
		f32 rate = self->unk194->mSLHitJumpSpRateXZ.get();
		self->setBckAnm(6);
		self->unk1A0.x *= rate;
		self->unk1A0.z *= rate;
		self->unk1A0.y = 0.0f;
		self->setVelocity(self->unk1A0);
		self->mPosition.y = self->mGroundHeight;
		self->offLiveFlag(LIVE_FLAG_AIRBORNE);
		self->setBckAnm(12);
		spine->setNext(&TNerveSamboHeadAttack::theNerve());
		spine->pushAfterCurrent(&TNerveSamboHeadRecoverWater::theNerve());
		return TRUE;
	}

	self->walkToCurPathNode(0.0f, self->getTurnSpeed(), 0.0f);
	return FALSE;
}


BOOL TNerveSamboHeadRecoverWater::execute(TSpineBase<TLiveActor>* spine) const
{
	TSamboHead* self = (TSamboHead*)spine->getBody();

	if (spine->getTime() == 0)
		self->setBckAnm(12);

	self->unk1AC *= 0.99f;
	if (self->checkCurAnmEnd(0) && self->unk1AC < 1.0f)
		return TRUE;
	return FALSE;
}


BOOL TNerveSamboHeadHitWall::execute(TSpineBase<TLiveActor>* spine) const
{
	TSamboHead* self = (TSamboHead*)spine->getBody();

	if (spine->getTime() == 0)
		self->setCrashAnm();

	int waitTime = ((TSmallEnemyManager*)self->getManager())->unk5C;
	if (self->checkCurAnmEnd(0)
	    && spine->getTime()
	           > waitTime
	                 + self->getMActor()->getFrameCtrl(0)->getEnd()) {
		self->onLiveFlag(LIVE_FLAG_DEAD);
		self->onLiveFlag(LIVE_FLAG_UNK8);
		self->onLiveFlag(LIVE_FLAG_UNK20000);
		self->offLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
		self->mHolder = nullptr;
		self->stopAnmSound();
		spine->reset();
		spine->setNext(&TNerveSmallEnemyDie::theNerve());
		spine->pushAfterCurrent(&TNerveSmallEnemyDie::theNerve());
		self->genRandomItem();
		return TRUE;
	}

	return FALSE;
}
