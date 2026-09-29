
#include <Enemy/Enemy.hpp>
#include <Enemy/Conductor.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
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
#include <System/Application.hpp>
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
	// Frame-padding: target frame is 8 bytes larger (MWCC stack-padding quirk).
	char framePad_8_setMActorAndKeeper[8];
	(void)framePad_8_setMActorAndKeeper;
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("flower.bmd", 3);
	setMaterialToMActor(mMActor, ((TSamboFlowerManager*)mManager)->unk64);
}


BOOL TSamboFlower::receiveMessage(THitActor* sender, u32 message)
{
	// Frame-padding: target frame is 8 bytes larger (MWCC stack-padding quirk).
	char framePad_8_receiveMessage[8];
	(void)framePad_8_receiveMessage;
	if (message == 0xF) {
		if (!unk150) {
			unk150 = 1;
			unk154 = 0;
			gpMarioParticleManager->emit(0xB2, &mPosition, 0, nullptr);
			mMActor->setBck("flower_hit");
			if (unk160 && unk164) {
				--*unk164;
				u32 id = MSD_SE_OBJ_FLOWER_OPEN_0 + *unk164;
				SMSGetMSound()->startSoundActor(id, &mPosition, 0, nullptr, 0,
				                                4);
			}
		}
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
		THanaSambo* owner = mOwner;
		owner->unk165     = true;
		if (!owner->changeByJuice()) {
			if (owner->mSpine->getCurrentNerve()
			    == &TNerveHanaSamboWait::theNerve())
				owner->mSpine->pushNerve(&TNerveHanaSamboFreeze::theNerve());
		}
		return TRUE;
	}
	return FALSE;
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
	for (int i = 0; i < head->mColCount; ++i) {
		if (head->getCollision(i)->isActorType(0x80000001))
			SMS_SendMessageToMario(head, 0xE);
	}

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
		unk194->onHitFlag(1);
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
	unk194->onHitFlag(1);
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
	// Frame-padding: target frame is 8 bytes larger, 4 above and 4 below the
	// locals (MWCC stack-padding quirk). TODO: find the real locals.
	char framePad_4_createPollen[4];
	(void)framePad_4_createPollen;
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
	char framePad_4b_createPollen[4];
	(void)framePad_4b_createPollen;
}


bool THanaSambo::isCollidMove(THitActor*) { return false; }

BOOL THanaSambo::isHitValid(u32 message)
{
	if (message == 0xB) {
		onLiveFlag(LIVE_FLAG_HIDDEN);
		return TRUE;
	}
	return FALSE;
}

void THanaSambo::behaveToWater(THitActor*) { }

BOOL TNerveHanaSamboAppear::execute(TSpineBase<TLiveActor>* spine) const
{
	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->offLiveFlag(LIVE_FLAG_HIDDEN);
		self->offHitFlag(1);
		self->setBckAnm(6);
		gpMarioParticleManager->emit(0xB6, &self->mPosition, 0, nullptr);
		gpMarioParticleManager->emit(0xB7, &self->mPosition, 0, nullptr);
		TSamboFlower* flower = self->unk1A8;
		flower->onHitFlag(1);
		flower->mMActor->setBck("flower_fwait");
		flower->onLiveFlag(LIVE_FLAG_DEAD);
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
	// Frame-padding: target frame is 8 bytes larger (MWCC stack-padding quirk).
	char framePad_8_attack[8];
	(void)framePad_8_attack;
	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(3);
		self->unk1B0 = 1;
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
	// Frame-padding: target frame is 32 bytes larger (MWCC stack-padding quirk).
	char framePad_32_setAfterDeadEffect[32];
	(void)framePad_32_setAfterDeadEffect;
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


void TSamboHead::calcRootMatrix()
{
	gpCurSamboHead = this;
	TSpineEnemy::calcRootMatrix();
}


const char** TSamboHead::getBasNameTable() const { return sambohead_bastable; }


BOOL TNerveSamboHeadAppear::execute(TSpineBase<TLiveActor>* spine) const
{
	TSamboHead* self = (TSamboHead*)spine->getBody();

	if (spine->getTime() == 0) {
		self->offLiveFlag(LIVE_FLAG_HIDDEN);
		self->offHitFlag(1);
		TSamboFlower* bud = self->unk198;
		if (bud->unk150) {
			TSamboFlower* flower = bud;
			flower->unk150 = 1;
			flower->unk154 = 0;
			gpMarioParticleManager->emit(0xB2, &flower->mPosition, 0, nullptr);
			flower->mMActor->setBck("flower_hit");
			if (flower->unk160 && flower->unk164) {
				--*flower->unk164;
				SMSGetMSound()->startSoundActor(
				    MSD_SE_OBJ_FLOWER_OPEN_0 + *flower->unk164, &flower->mPosition,
				    0, nullptr, 0, 4);
			}
			self->setBckAnm(10);
		} else {
			self->setBckAnm(10);
		}
		gpMarioParticleManager->emit(0xB6, &self->mPosition, 0, nullptr);
		gpMarioParticleManager->emit(0xB7, &self->mPosition, 0, nullptr);
		TSamboFlower* flower = self->unk198;
		flower->onHitFlag(1);
		flower->mMActor->setBck("flower_fwait");
		flower->onLiveFlag(LIVE_FLAG_DEAD);
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
		if (self->unk198 == nullptr) {
			self->unk198 = (TSamboFlower*)gpConductor->makeOneEnemyAppear(
			    self->mPosition, "サンボフラワーマネージャー", 1);
			self->unk198->reset();
		}
		self->unk198->offHitFlag(1);
		self->unk198->onLiveFlag(LIVE_FLAG_UNK10);
		self->unk198->offLiveFlag(LIVE_FLAG_DEAD);
		self->unk198->mPosition.y = self->mGroundHeight;
		self->unk198->mPosition   = self->mPosition;
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
