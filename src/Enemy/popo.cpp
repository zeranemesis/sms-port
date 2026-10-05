#include <Enemy/popo.hpp>
#include <Enemy/Graph.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/MarioFlags.hpp>
#include <Player/Mario.hpp>
#include <Player/WaterGun.hpp>
#include <Strategic/Spine.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionData.hpp>
#include <Strategic/ObjModel.hpp>
#include <M3DUtil/MActor.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <System/Particles.hpp>
#include <MSound/MSound.hpp>
#include <System/EmitterViewObj.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

// TODO: SMSGetAnmFrameRate() lives in System/Application.hpp, which is far too
// heavy for this TU; Enemy/Koopa.hpp forward-declares it the same way.
extern f32 SMSGetAnmFrameRate();

// rogue include: mtx calc type names, needed to match the .rodata prologue
// (it drags in System/DummyStrings.hpp, which is needed too)
#include <M3DUtil/InfectiousStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// TODO: this translation unit started out freshly scaffolded from mario.MAP.
// The manager, constructor, parameter, init, reset, kill and receiveMessage
// paths are now matched; the nerve bodies, the model callbacks and the
// remaining behaviour hooks are still placeholders.

TPopo* gpCurPopo;

bool TPopo::mRollSw = true;
bool TPopo::mTriggerSw = true;
f32 TPopo::mTestAng_x = 90.0f;
f32 TPopo::mTestAng_y = 90.0f;
f32 TPopo::mTestAng_z;
f32 TPopo::mNozzleOffsetZ = -15.0f;
u8 TPopo::mCenterJntIndex = 1;
u8 TPopo::mMouthJntIndex = 2;
u8 TPopo::mRLegJntIndex = 5;
u8 TPopo::mLLegJntIndex = 11;
u8 TPopo::mRHandJntIndex = 7;
u8 TPopo::mLHandJntIndex = 9;
f32 TPopo::mTestBodyScale = 35.0f;
bool TPopo::mBrkFlag = true;
f32 TPopo::mColOffsetY = 20.0f;
f32 TPopo::mColMinVal = 0.6f;
bool TPopo::mExplosionSw;
bool TPopo::mLevelShootSw = true;

static const char* popo_bastable[] = {
	"/scene/popo/bas/popo_chase.bas",
	0,
	0,
	0,
	0,
	"/scene/popo/bas/popo_jump.bas",
	"/scene/popo/bas/popo_wait.bas",
};

// TPopoManager, TPopoCollision and TPopo have empty destructors that were
// defined inline in the original header (mario.MAP lists all three as weak
// rather than global), so they no longer need a definition in this file.

TPopoSaveLoadParams::TPopoSaveLoadParams(const char* path)
    : TWalkerEnemyParams(path)
    , PARAM_INIT(mSLMoveDist, 100.0f)
    , PARAM_INIT(mSLMoveGravity, 0.1f)
    , PARAM_INIT(mSLMoveJumpSp, 10.0f)
    , PARAM_INIT(mSLAttackDist, 100.0f)
    , PARAM_INIT(mSLAttackGravity, 0.1f)
    , PARAM_INIT(mSLAttackJumpSp, 10.0f)
    , PARAM_INIT(mSLReleaseSpeed, 10.0f)
    , PARAM_INIT(mSLFlyGravity, 0.0f)
    , PARAM_INIT(mSLFlyLimitTime, 300)
    , PARAM_INIT(mSLExplosionEmitTime, 60)
    , PARAM_INIT(mSLWaterScaleMax, 2.0f)
    , PARAM_INIT(mSLThrownGravity, 0.5f)
    , PARAM_INIT(mSLPumpRate, 0.0001f)
    , PARAM_INIT(mSLLevelLimit, 1.2f)
    , PARAM_INIT(mSLScaleRate, 0.99f)
{
	TParams::load(mPrmPath);
}

TPopoManager::TPopoManager(const char* name)
    : TSmallEnemyManager(name)
    , unk60(1)
    , unk64(nullptr)
    , unk68(nullptr)
{
	gpCurPopo = nullptr;
	unk5C = 0;
}

void TPopoManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TPopoSaveLoadParams("/enemy/popo.prm");
	unk64 = new TWaterEmitInfo("/enemy/popowater.prm");
	unk68 = new TWaterEmitInfo("/enemy/popoexpwater.prm");
}

TSpineEnemy* TPopoManager::createEnemyInstance()
{
	return new TPopo;
}

void TPopoManager::initSetEnemies()
{
	// The original computes isDummy() and compares it against FALSE, but the
	// outcome is discarded: this looks like a debug check whose body was never
	// written (or was stripped).
	TGraphWeb* graph = getObj(0)->getTracer()->getGraph();
	bool usable = graph != nullptr && graph->isDummy() == FALSE;
	(void)usable;
}

void TPopoManager::createModelData()
{
	// TODO: 0x210 is a raw J3DMLF_* combination; the two relevant bits
	// have not been identified yet.
	static TModelDataLoadEntry entry[] = {
		{ "popoH.bmd", 0x210, 0 },
		{ "popoL.bmd", 0x210, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TPopoManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// As in TPopo::kill, the retail frame carries 8 bytes of slack that this
	// body never touches.
	
	
	// TODO: 0x1A4 is TPopo::unk1A4[0]; the flag is unnamed so far.
	if (cue & 1) {
		for (int i = 0; i < getActiveObjNum(); ++i) {
			TPopo* popo = (TPopo*)unk18[i];
			if (popo->unk1A4 && popo->checkLiveFlag(LIVE_FLAG_DEAD))
				popo->reset();
		}
	}
	TEnemyManager::perform(cue, graphics);
}

BOOL TPopoCollision::receiveMessage(THitActor* sender, u32 message)
{
	// While the owner is flying, the collision body is inert; otherwise the
	// message goes straight to the owner's own hit-actor handling.
	TLiveActor* owner = (TLiveActor*)mOwner;
	if (owner->mSpine->getCurrentNerve() != &TNervePopoFly::theNerve())
		return mOwner->receiveMessage(sender, message);
	return FALSE;
}

// TODO: callback signatures are guessed from J3D animation frame callback
// usage elsewhere; not yet verified against this file's call sites.
static int PopoNonScaleCallback(J3DNode*, int);
static int PopoPossessedCallback(J3DNode*, int);
static int PopoRollCallback(J3DNode*, int);

static int PopoRollCallback(J3DNode* node, int param_1)
{
	TRotation3f rot;
	TRotation3f scaling;

	if (param_1 == 0) {
		if (!gpCurPopo)
			return 1;

		TPopo* self  = gpCurPopo;
		MtxPtr joint = self->getModel()->getAnmMtx(
		    ((J3DJoint*)node)->getJntNo());

		scaling.ref(0, 3) = 0.0f;
		scaling.ref(1, 3) = 0.0f;
		scaling.ref(2, 3) = 0.0f;
		scaling.setScale(self->mBodyScale, self->mBodyScale, self->mBodyScale);

		// The body rolls around the centre joint, except while flying where
		// it is pinned to a fixed half turn.
		//
		// TODO: the retail build fills the basis in an order that is rotated
		// by one relative to the rotation it describes (see the individual
		// ref() rows below), so `rot` is not the transform it looks like.
		if (self->mSpine->getCurrentNerve() == &TNervePopoFly::theNerve()
		        ? true
		        : false) {
			f32 s = MsSin(180.0f);
			f32 c = MsCos(180.0f);

			rot.ref(0, 0) = c;
			rot.ref(0, 1) = 0.0f;
			rot.ref(0, 2) = s;
			rot.ref(1, 0) = 0.0f;
			rot.ref(1, 1) = 0.0f;
			rot.ref(1, 2) = 1.0f;
			rot.ref(2, 0) = 0.0f;
			rot.ref(2, 1) = 0.0f;
			rot.ref(2, 2) = -s;
			rot.ref(0, 3) = 0.0f;
			rot.ref(1, 3) = c;
			rot.ref(2, 3) = 0.0f;
		} else {
			f32 s = MsSin(self->unk1B8);
			f32 c = MsCos(self->unk1B8);

			rot.ref(0, 0) = 1.0f;
			rot.ref(0, 1) = 0.0f;
			rot.ref(0, 2) = 0.0f;
			rot.ref(1, 0) = 0.0f;
			rot.ref(1, 1) = 0.0f;
			rot.ref(1, 2) = c;
			rot.ref(2, 0) = -s;
			rot.ref(2, 1) = 0.0f;
			rot.ref(2, 2) = 0.0f;
			rot.ref(0, 3) = s;
			rot.ref(1, 3) = c;
			rot.ref(2, 3) = 0.0f;
		}

		MTXConcat(joint, rot, joint);
		MTXConcat(joint, scaling, joint);
		MTXConcat(J3DSys::mCurrentMtx, rot, J3DSys::mCurrentMtx);
		MTXConcat(J3DSys::mCurrentMtx, scaling, J3DSys::mCurrentMtx);
	}

	return 1;
}

static int PopoPossessedCallback(J3DNode* node, int param_1)
{
	TRotation3f yawMtx;
	TRotation3f scaling;
	JGeometry::TVec3<f32> vec;

	if (param_1 == 0 && gpCurPopo) {
		// TODO: the retail build materialises this disjunction into an int
		// (li 1 / li 0 / clrlwi.) before testing it, which needs the `? true
		// : false` spelling -- but that spelling stops MWCC inlining the
		// second theNerve() call, which costs more than it gains.
		if (gpCurPopo->mSpine->getCurrentNerve() == &TNervePopoFly::theNerve()
		        || gpCurPopo->mSpine->getCurrentNerve()
		               == &TNervePopoExplosion::theNerve()
		        || gpCurPopo->unk1B4) {
			if (gpCurPopo->unk198 >= 1.1f) {
				// The mouth joint is scaled by the water level.
				f32 scale       = gpCurPopo->unk198;
				MtxPtr joint
				    = gpCurPopo->getModel()->getAnmMtx(
				        ((J3DJoint*)node)->getJntNo());

				scaling.ref(0, 3) = 0.0f;
				scaling.ref(1, 3) = 0.0f;
				scaling.ref(2, 3) = 0.0f;
				scaling.setScale(scale, scale, scale);

				MTXConcat(joint, scaling, joint);
				MTXConcat(J3DSys::mCurrentMtx, scaling, J3DSys::mCurrentMtx);

				if (gpCurPopo->unk1BC) {
					// A copy of the joint transform, yawed by 270 degrees,
					// becomes the anchor of the water jet; the three
					// basis-vector lengths go out as the emitter's scale.
					MTXCopy(joint, gpCurPopo->unk1D0);

					MsMtxSetRotRPH(yawMtx, 1.0f, 270.0f, 1.0f);
					MTXConcat(gpCurPopo->unk1D0, yawMtx, gpCurPopo->unk1D0);

					vec.x = joint[0][0];
					vec.y = joint[1][0];
					vec.z = joint[2][0];
					gpCurPopo->unk230.y = vec.length();

					vec.x = joint[0][1];
					vec.y = joint[1][1];
					vec.z = joint[2][1];
					gpCurPopo->unk230.z = vec.length();

					vec.x = joint[0][2];
					vec.y = joint[1][2];
					vec.z = joint[2][2];
					gpCurPopo->unk230.x = vec.length();

					JPABaseEmitter* emitter
					    = gpMarioParticleManager->emitAndBindToMtxPtr(
					        0x13C, gpCurPopo->unk1D0, 1, gpCurPopo);
					if (emitter)
						emitter->setGlobalScale(gpCurPopo->unk230);
				}
			}
		}
	}

	return 1;
}

// TODO: callback signatures are guessed from J3D animation frame callback
// usage elsewhere; not yet verified against this file's call sites.
// TODO: bodies not yet decompiled; the retail versions return 1.
static int PopoNonScaleCallback(J3DNode* node, int param_1)
{
	// Only the first frame of a joint callback is interesting here.
	if (param_1 == 0 && gpCurPopo) {
		if (gpCurPopo->mSpine->getCurrentNerve() == &TNervePopoFly::theNerve()
		    || gpCurPopo->mSpine->getCurrentNerve()
		           == &TNervePopoExplosion::theNerve()
		    || gpCurPopo->unk1B4) {
			// Limbs are shrunk so the water jet does not look like it scales
			// the body.
			f32 scale       = 0.9f * gpCurPopo->mBodyScale;
			MtxPtr joint
			    = gpCurPopo->getModel()->getAnmMtx(((J3DJoint*)node)->getJntNo());

			TRotation3f scaling;
			scaling.ref(0, 3) = 0.0f;
			scaling.ref(1, 3) = 0.0f;
			scaling.ref(2, 3) = 0.0f;
			scaling.setScale(scale, scale, scale);

			MTXConcat(joint, scaling, joint);
			MTXConcat(J3DSys::mCurrentMtx, scaling, J3DSys::mCurrentMtx);
		}
	}

	return 1;
}

TPopo::TPopo(const char* name)
    : TWalkerEnemy(name)
    , unk194(0)
    , unk198(1.0f)
    , unk19C(0)
    , unk1A0(30.0f)
    , unk1A4(0)
    , unk1B4(0)
    , unk1B8(0.0f)
    , unk1CC(0)
    , unk1CD(false)
    , unk23C(nullptr)
{
}

void TPopo::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	unk1A8 = mPosition;
	unk1A4 = 1;
	reset();
}

void TPopo::init(TLiveManager* liveManager)
{
	TWalkerEnemy::init(liveManager);

	mActorType = 0x100D;

	if (mInstanceIndex == 0) {
		// The loop body is empty in the retail build; getModel() is an
		// out-of-line call, so the comparison survives optimisation.
		J3DModelData* tables = getModel()->getModelData();
		for (u8 i = 0; i < tables->getJointNum(); ++i) {
		}
	}

	unk150 = 0x11;
	unk194 = (TPopoSaveLoadParams*)getSaveParam2();
	mSpine->initWith(&TNerveWalkerGraphWander::theNerve());
	onLiveFlag(LIVE_FLAG_UNK4000);

	mMActor->setJointCallback(mCenterJntIndex, PopoRollCallback);
	mMActorKeeper->getMActor("popoL.bmd")
	    ->setJointCallback(mCenterJntIndex, PopoRollCallback);
	mMActor->setJointCallback(mMouthJntIndex, PopoPossessedCallback);
	mMActor->setJointCallback(mRLegJntIndex, PopoNonScaleCallback);
	mMActor->setJointCallback(mLLegJntIndex, PopoNonScaleCallback);
	mMActor->setJointCallback(mRHandJntIndex, PopoNonScaleCallback);
	mMActor->setJointCallback(mLHandJntIndex, PopoNonScaleCallback);

	unk188 = 0.0f;
	unk23C = new TPopoCollision("ポポコリジョン");

	TEnemyNameRefGroup* group = (TEnemyNameRefGroup*)
	    JDrama::TNameRef::search("敵グループ");
	group->mObjects.insert(group->mObjects.end(), unk23C);

	unk23C->initHitActor(0x100D, 2, 0x9800, 80.0f, 80.0f, 80.0f, 80.0f);
	unk23C->onHitFlag(HIT_FLAG_NO_COLLISION);
	unk23C->setOwner(this);
}

void TPopo::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);
	unk23C->THitActor::perform(cue, graphics);
}

void TPopo::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 2);
	mMActor       = mMActorKeeper->createMActor("popoH.bmd", 3);
	mMActorKeeper->createMActor("popoL.bmd", 3);
}

void TPopo::reset()
{
	gpCurPopo = this;
	TWalkerEnemy::reset();
	unk165 = false;
	unk1B4 = false;
	unk198 = 1.0f;
	unk1B8 = 0.0f;
	unk19C = 0;
	mScaledBodyRadius = mBodyScale * mBodyRadius * 15.0f;
	unk190 = 0.2f;
	expandCollision();
	mMActor = mMActorKeeper->getMActor("popoL.bmd");
	if (unk1A4) {
		onLiveFlag(LIVE_FLAG_UNK10);
		mSpine->initWith(&TNervePopoWait::theNerve());
		mPosition = unk1A8;
		offLiveFlag(LIVE_FLAG_UNK800);
	}
	unk23C->onHitFlag(HIT_FLAG_NO_COLLISION);
	unk18C = 0;
}

bool TPopo::checkTrigger()
{
	// TODO: the meaning of the two fields read off Mario's game pad below is
	// still unknown; the field names are correct but the semantics are not.
	unk1BC = 0;

	if (!gpMarioOriginal->onYoshi() && SMS_GetMarioWaterGun()->mCurrentNozzle != 0) {
		getFocalPoint();
		return false;
	}

	SMS_SendMessageToMario(this, 5);

	f32 maxScale = unk194->getWaterScaleMax();
	int trigger  = (int)gpMarioOriginal->mGamePad->mCompSPos[3];

	if ((u8)trigger > 0x14) {
		unk1BC = 1;

		if (gpMSound->gateCheck(0x20C2))
			gpMSound->startSoundActorWithInfo(0x20C2, &mPosition, nullptr,
			                                  unk198, 0, 0, nullptr, 0, 4);

		mSprayedByWaterCooldown = 0;
		unk165                 = true;
		unk198 += (0.0f - 4503599627370496.0f) * unk194->getPumpRate();

		if (unk198 > maxScale) {
			unk198 = maxScale;
			if (!mBrkFlag)
				mMActor->setFrameRate(SMSGetAnmFrameRate(), ANM_TYPE_BRK);
		}

		if (mBrkFlag)
			mMActor->getFrameCtrl(ANM_TYPE_BRK)->setFrame(
			    unk1A0 * unk198 / maxScale);
	}

	if (!(gpMarioOriginal->mGamePad->mEnabledFrameMeaning & 0x400) && !mTriggerSw) {
		if (mLevelShootSw || unk198 >= maxScale)
			unk1CC = 1;
	}

	if (mLevelShootSw && unk198 < maxScale - 0.1f && unk198 > 1.0f)
		unk198 *= unk194->getLevelLimit();

	if ((u8)trigger >= 0x14) {
		if (unk1CC || unk198 > unk194->getLevelLimit()) {
			if (gpMSound->gateCheck(0x28CD))
				gpMSound->startSoundActor(0x28CD, &mPosition, 0, nullptr, 0, 4);

			// TODO: 0x1 has no name in Strategic/HitActor.hpp's enum yet.
			mHitFlags |= 0x1;
			unk23C->mHitFlags &= ~0x1u;
		}
	} else {
		// Blowing up leaves a scale behind for the shockwave effect.
		unk158 = (8.0f * unk198 + 8.0f) * (mBodyScale * unk154);

		if (unk198 >= maxScale)
			mMActor->getFrameCtrl(ANM_TYPE_BTP)->setFrame(5.0f);
	}

	return false;
}

void TPopo::behaveToWater(THitActor* hitActor)
{
	if (mSpine->getCurrentNerve() == &TNervePopoFly::theNerve())
		return;

	if (mSpine->getCurrentNerve() == &TNervePopoExplosion::theNerve())
		return;

	if (mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()) {
		if (mSpine->getCurrentNerve() == &TNervePopoPossessedNozzle::theNerve()) {
			mSprayedByWaterCooldown = 0;
		} else if (checkLiveFlag2(LIVE_FLAG_AIRBORNE)) {
			// Pushed away from Mario, then let gravity take over.
			JGeometry::TVec3<f32> vel = mVelocity;
			JGeometry::TVec3<f32> dir;
			dir.x = mPosition.x - SMS_GetMarioPos().x;
			dir.y = 0.0f;
			dir.z = mPosition.z - SMS_GetMarioPos().z;
			MsVECNormalize(&dir, &dir);
			dir.scale(12.0f);
			dir.y = -1.0f;
			dir += vel;
			mVelocity = dir;
		} else if (mSpine->getCurrentNerve()
		           != &TNerveSmallEnemyFreeze::theNerve()) {
			mSpine->pushNerve(&TNerveSmallEnemyFreeze::theNerve());
		}
	}
}

f32 TPopo::getGravityY() const
{
	f32 gravity = mGravity;

	if (mSpine->getCurrentNerve() == &TNerveWalkerGraphWander::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveWalkerEscape::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveWalkerAttack::theNerve())
		return unk194->getMoveGravity();

	if (mSpine->getCurrentNerve() == &TNervePopoAttack::theNerve())
		gravity = unk194->getAttackGravity();
	else if (mSpine->getCurrentNerve() == &TNervePopoFly::theNerve())
		gravity = unk194->getFlyGravity();
	else if (mSpine->getCurrentNerve() == &TNervePopoThrown::theNerve())
		gravity = unk194->getThrownGravity();

	return gravity;
}

void TPopo::behaveToFindMario()
{
	if (SMS_CheckMarioFlag(MARIO_FLAG_HAS_FLUDD)
	    && ((TPopoManager*)mManager)->unk60 != 0
	    && SMS_GetMarioWaterGun()->mCurrentNozzle == 0
	    && !gpMarioOriginal->onYoshi()) {
		setGoalPath(TPathNode((THitActor*)gpMarioAddress));
		mSpine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
		mSpine->pushAfterCurrent(&TNervePopoAttack::theNerve());
	} else {
		mSpine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
	}
}

void TPopo::walkBehavior(int param_1, float param_2)
{
	// TODO: neither parameter is read in the binary.
	if (!checkLiveFlag2(LIVE_FLAG_AIRBORNE)) {
		JGeometry::TVec3<f32> v = unk104.getPoint();
		v.set(unk104.getPoint().x - mRotation.x, 0.0f,
		      unk104.getPoint().z - mRotation.z);

		if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
			v.x += 1.0f;
		MsVECNormalize(&v, &v);

		// +-20 degrees of random yaw, aimed at the goal node.
		f32 minAng = -20.0f;
		f32 maxAng = 20.0f;
		f32 dist   = unk194->getMoveDist();
		f32 jumpSp = unk194->getMoveJumpSp();
		f32 scale  = 1.0f;

		if (mSpine->getCurrentNerve() == &TNervePopoAttack::theNerve()) {
			setBckAnm(0);
			dist   = unk194->getAttackDist();
			jumpSp = unk194->getAttackJumpSp();
			scale  = 10.0f;
		}

		v.x = mRotation.x + v.x * dist
		      + scale
		            * (minAng
		               + (maxAng - minAng)
		                     * ((1.0f / 32768.0f) * (f32)rand()));
		v.z = mRotation.z + v.z * dist
		      + scale
		            * (minAng
		               + (maxAng - minAng)
		                     * ((1.0f / 32768.0f) * (f32)rand()));
		v.y = mPosition.y;

		if (mSpine->getCurrentNerve() == &TNerveWalkerEscape::theNerve())
			setBckAnm(5);

		mLinearVelocity
		    = calcVelocityToJumpToY(v, jumpSp * scale, getGravityY());

		mPosition.y += 2.0f;
		onLiveFlag(LIVE_FLAG_AIRBORNE);

		if (mSpine->getCurrentNerve()
		    == &TNerveWalkerGraphWander::theNerve()) {
			setGoalPath(TPathNode(v));
			setBckAnm(5);
		}
	} else {
		if (mLinearVelocity.y > 1.5f)
			mPosition.y += 0.5f * mLinearVelocity.y;
		if (mLinearVelocity.y < -1.0f)
			mPosition.y += 0.2f * mLinearVelocity.y;
	}

	// TODO: the binary copies mLinearVelocity into a stack slot that is then
	// never read; probably a leftover temporary from the original source.
	JGeometry::TVec3<f32> vel = mLinearVelocity;
	unk1B8 += 1.0f;
	if (mSpine->getCurrentNerve() == &TNervePopoAttack::theNerve())
		unk1B8 += 2.0f;
	if (unk1B8 > 360.0f)
		unk1B8 -= 360.0f;
	if (!mRollSw)
		unk1B8 = 0.0f;

	JGeometry::TVec3<f32> vel2 = mLinearVelocity;
	if (vel2.y > 0.0f)
		walkToCurPathNode(0.0f, mTurnSpeed, 0.0f);
}

void TPopo::attackToMario()
{
	if (mSpine->getCurrentNerve() == &TNervePopoAttack::theNerve()
	    || (mSpine->getCurrentNerve() == &TNervePopoWait::theNerve()
	        && ((TPopoManager*)mManager)->unk60 != 0)) {
		mSpine->pushNerve(&TNervePopoPossessedNozzle::theNerve());
	} else if (mSpine->getCurrentNerve() != &TNerveWalkerEscape::theNerve()
	           && mSpine->getCurrentNerve() == &TNerveWalkerGraphWander::theNerve()) {
		sendAttackMsgToMario();

		// Hop straight at Mario; the vertical component comes from the
		// initialised accumulator below.
		JGeometry::TVec3<f32> vel(0.0f, 0.0f, 0.0f);
		JGeometry::TVec3<f32> dir;

		dir.x = mPosition.x - SMS_GetMarioPos().x;
		dir.y = mPosition.y - SMS_GetMarioPos().y;
		dir.z = mPosition.z - SMS_GetMarioPos().z;

		MsVECNormalize(&dir, &dir);
		mVelocity.x = dir.x;
		mVelocity.z = dir.z;

		f32 speed = mBodyScale * unk1BC;
		dir.x *= speed;
		dir.y *= speed;
		dir.z *= speed;
		vel += dir;

		mLinearVelocity = vel;
	}
}

void TPopo::calcRootMatrix()
{
	f32 len0, len1, len2;
	TPosition3f root;
	JGeometry::TVec3<f32> axis[3];
	TPosition3f nozzleMtx;
	TPosition3f bodyMtx;
	TPosition3f rphMtx;

	// The centre joint drives both the collision body and the emitter.
	gpCurPopo = this;

	MtxPtr centerJnt = getModel()->getAnmMtx(mCenterJntIndex);

	unk23C->mPosition.set(centerJnt[0][3], centerJnt[1][3], centerJnt[2][3]);

	if (unk1B4) {
		unk190 = 0.8f * unk198 / unk194->getWaterScaleMax();
		if (unk190 < mColMinVal)
			unk190 = mColMinVal;

		expandCollision();

		getModel()->setBaseScale(mScaling);

		// The water jet hangs off the emitter, so the root is built from the
		// gun's own matrix with each basis vector normalised by its length.
		// While flying there is no gun to follow and the body just keeps its
		// own position.
		if (mSpine->getCurrentNerve() == &TNervePopoFly::theNerve()) {
			root.translation(mPosition.x, mPosition.y, mPosition.z);
		} else {
			MTXCopy(SMS_GetMarioWaterGun()->getEmitMtx(0), root);

			axis[0].x = root.at(0, 0);
			axis[0].y = root.ref(1, 0);
			axis[0].z = root.ref(2, 0);
			len0 = axis[0].length();
			axis[1].x = root.ref(0, 1);
			axis[1].y = root.at(1, 1);
			axis[1].z = root.ref(2, 1);
			len1 = axis[1].length();
			axis[2].x = root.ref(0, 2);
			axis[2].y = root.ref(1, 2);
			axis[2].z = root.at(2, 2);
			len2 = axis[2].length();

			// TODO: each guard in the retail build tests the length of the
			// *next* axis, not the one it divides by. Reproduced verbatim.
			if (len2 != 0.0f) {
				root.ref(0, 0) /= len0;
				root.ref(1, 0) /= len0;
				root.ref(2, 0) /= len0;
			}

			if (len0 != 0.0f) {
				root.ref(0, 1) /= len1;
				root.ref(1, 1) /= len1;
				root.ref(2, 1) /= len1;
			}

			if (len1 != 0.0f) {
				root.ref(0, 2) /= len2;
				root.ref(1, 2) /= len2;
				root.ref(2, 2) /= len2;
			}

			// The nozzle sits 7 units of body scale forward of the mouth,
			// offset by mNozzleOffsetZ.
			nozzleMtx.translation(7.0f * unk198 + mNozzleOffsetZ, 0.0f, 0.0f);
			MTXConcat(nozzleMtx, root, root);

			bodyMtx.translation(mTestBodyScale * unk198, 0.0f, 0.0f);
			MTXConcat(root, bodyMtx, bodyMtx);

			mPosition.x = bodyMtx.at(0, 3);
			mPosition.y = bodyMtx.at(1, 3) - mColOffsetY * unk198;
			mPosition.z = bodyMtx.at(2, 3);
		}

		// While being pumped the water jet is bound to the centre joint.
		if (unk1BC) {
			MTXCopy(mMActor->getModel()->getAnmMtx(mCenterJntIndex), unk200);

			unk200[0][3] = bodyMtx.at(0, 3);
			unk200[1][3] = bodyMtx.at(1, 3);
			unk200[2][3] = bodyMtx.at(2, 3);

			JPABaseEmitter* emitter
			    = gpMarioParticleManager->emitAndBindToMtxPtr(0x13D, unk200, 1,
			                                                    this);
			if (emitter)
				emitter->setGlobalScale(unk230);
		}

		MsMtxSetRotRPH(rphMtx, mTestAng_x, mTestAng_y, mTestAng_z);
		MTXConcat(root, rphMtx, root);
		getModel()->setBaseTRMtx(root);
	} else {
		TSpineEnemy::calcRootMatrix();
	}
}

void TPopo::kill()
{
	// The retail frame is 8 bytes larger than anything this body needs; the
	// slack is most likely a leftover temporary from the original source.
	
	
	if (unk1B4) {
		((TPopoManager*)mManager)->unk60 = 1;
		unk1B4 = 0;
	}
	TSmallEnemy::kill();
	unk23C->onHitFlag(HIT_FLAG_NO_COLLISION);
}

void TPopo::forceKill()
{
	// Killing a popo that is standing on an illegal, deadly or watery plane
	// pops it open; otherwise it is simply made harmless.
	bool illegal = mGroundPlane->mFlags & BG_CHECK_FLAG_ILLEGAL;
	bool deadly  = mGroundPlane->mBGType == BG_TYPE_DEATH_PLANE;
	bool pool    = mGroundPlane->mBGType == BG_TYPE_POOL
	             || mGroundPlane->mBGType == BG_TYPE_INDOOR_POOL
	             || mGroundPlane->mBGType == BG_TYPE_SHADED_POOL;
	bool water   = mGroundPlane->mBGType == BG_TYPE_WATER
	             || mGroundPlane->mBGType == BG_TYPE_DAMAGING_WATER
	             || mGroundPlane->mBGType == BG_TYPE_SEA_WATER
	             || mGroundPlane->mBGType == BG_TYPE_DAMAGING_SEA_WATER
	             || mGroundPlane->mBGType == BG_TYPE_POOL
	             || mGroundPlane->mBGType == BG_TYPE_INDOOR_POOL
	             || mGroundPlane->mBGType == BG_TYPE_SHADED_POOL;

	if (illegal || deadly || pool || water) {
		if (checkLiveFlag2(LIVE_FLAG_AIRBORNE))
			return;

		if (mLiveFlag & LIVE_FLAG_UNK10)
			return;

		if (!gpMap->isInArea(mPosition.x, mPosition.z)) {
			if (mSpine->getCurrentNerve() != &TNervePopoExplosion::theNerve()) {
				mSpine->reset();
				mSpine->pushNerve(&TNervePopoExplosion::theNerve());
			}
		}
	}

	mHitPoints = 1;
	onLiveFlag(LIVE_FLAG_CALC_INT_FRAME);
}

void TPopo::bind()
{
	// Anything the popo's own hit actor is touching reacts first.
	TPopoCollision* col = unk23C;

	for (int i = 0; i < col->mColCount; ++i) {
		THitActor* hit = col->mCollisions[i];
		bool isMario = (hit->mActorType + 0x8000) == 1;

		if (isMario)
			((TPopo*)col->getOwner())->attackToMario();
		else
			((TPopo*)col->getOwner())->behaveToHitOthers(hit);
	}

	if (mLiveFlag & LIVE_FLAG_UNK10)
		return;

	if (mSpine->getCurrentNerve() == &TNervePopoPossessedNozzle::theNerve()
	    && unk198 > 1.2f) {
		// TODO: the retail code tests this pair and then does nothing; the
		// test may be a leftover debug check.
	}

	if (!mExplosionSw && mSpine->getCurrentNerve() == &TNervePopoFly::theNerve()) {
		mGroundHeight = gpMap->checkGround(mPosition.x, mPosition.y + mHeadHeight,
		                                   mPosition.z, &mGroundPlane);

		// Sitting on the floor with the engine idling pops the popo open.
		if (mPosition.y < 30.0f + mGroundHeight
		    && fabs(mVelocity.x) < 1.0f && fabs(mVelocity.z) < 1.0f) {
			mSpine->pushNerve(&TNervePopoExplosion::theNerve());
		}

		TBGWallCheckRecord record(mPosition.x, mPosition.y, mPosition.z,
		                          unk198 * (mBodyScale * mWallRadius), 1, 0);

		if (gpMap->isTouchedWallsAndMoveXZ(&record))
			mSpine->pushNerve(&TNervePopoExplosion::theNerve());
	}

	if (mSpine->getCurrentNerve() == &TNervePopoFly::theNerve())
		TLiveActor::bind();
	else
		TLiveActor::bind();
}

bool TPopo::isHitValid(u32 message)
{
	if (message == HIT_MESSAGE_UNKB)
		return true;

	if (message <= HIT_MESSAGE_HIP_DROP)
		mSpine->pushNerve(&TNervePopoExplosion::theNerve());

	return false;
}

bool TPopo::isFindMario(float param_1)
{
	if (mSpine->getTime() > 100) {
		// The `? true : false` is what produces the retail `li 1 / li 0 /
		// clrlwi.` pair here; a plain bool test compiles to `cmpwi` instead.
		if (gpMarioOriginal->mFlag & MARIO_FLAG_VISIBLE ? true : false)
			return false;

		TSmallEnemyParams* param = getSaveParam2();

		JGeometry::TVec3<f32> marioPos(SMS_GetMarioPos().x, SMS_GetMarioPos().y,
		                               SMS_GetMarioPos().z);
		f32 length = param->getSLSearchLength() * param_1;
		f32 angle  = param->getSLSearchAngle() * param_1;
		f32 aware  = param->getSLSearchAware() * param_1;

		return isInSight(marioPos, length, angle, aware) ? true : false;
	}
	return false;
}

bool TPopo::isCollidMove(THitActor* hitActor)
{
	if (mSpine->getCurrentNerve() == &TNervePopoFly::theNerve()) {
		if (hitActor->receiveMessage(this, 0))
			mSpine->pushNerve(&TNervePopoExplosion::theNerve());
	}
	return false;
}

void TPopo::flyBehavior()
{
	unk19C++;
	if (unk19C > unk194->getFlyLimitTime()) {
		unk19C = 0;
		mSpine->pushNerve(&TNervePopoExplosion::theNerve());
	}

	if (unk198 > 1.0f)
		unk198 *= 0.999f;

	// While airborne the emitter follows the actor, otherwise it sticks to
	// the mouth joint.
	TWaterEmitInfo* info = ((TPopoManager*)mManager)->unk64;
	JGeometry::TVec3<f32> pos;

	if (checkLiveFlag2(LIVE_FLAG_UNK10000000)) {
		pos = mPosition;
	} else {
		MtxPtr joint = getModel()->getAnmMtx(mMouthJntIndex);
		pos.x = joint[0][3];
		pos.y = joint[1][3];
		pos.z = joint[2][3];
	}

	info->mPos.value = pos;
	gpModelWaterManager->emitRequest(*info);

	if (gpMSound->gateCheck(0x20CE))
		gpMSound->startSoundActor(0x20CE, &mPosition, 0, nullptr, 0, 4);
}

void TPopo::explosion()
{
	// The body scale keeps shrinking for as long as the popo stays alive.
	if (unk198 > 1.0f)
		unk198 *= 0.9f;

	TWaterEmitInfo* info = ((TPopoManager*)mManager)->unk68;

	JGeometry::TVec3<f32> pos = mPosition;
	pos.y += 100.0f;

	// Every other frame the emitted direction is mirrored vertically.
	if ((mSpine->getTime() % 2) == 0) {
		JGeometry::TVec3<f32> dir = info->mDir.get();
		dir.y = -dir.y;
		info->mDir.value = dir;
	}

	// The number of particles shrinks with the body scale, but never drops
	// below two.
	f32 count = (f32)info->mNum.get() * (unk198 / unk194->getWaterScaleMax());
	if (count < 2.0f)
		count = 2.0f;
	info->mNum.set((s32)count);

	info->mPos.value = pos;
	gpModelWaterManager->emitRequest(*info);
}

void TPopo::possessedIn()
{
	// The retail frame is 0x10 bytes larger than anything this body needs.
	
	
	mMActor = mMActorKeeper->getMActor("popoH.bmd");
	setBckAnm(3);
	mMActor->setBtpFromIndex(0);
	mMActor->setFrameRate(0.0f, ANM_TYPE_BTP);
	if (!mExplosionSw)
		onHitFlag(HIT_FLAG_NO_COLLISION);
	mMActor->setBrkFromIndex(0);
	mMActor->getFrameCtrl(ANM_TYPE_BRK)->setFrame(0.0f);
	unk1A0 = 30.0f;
	mMActor->setFrameRate(0.0f, ANM_TYPE_BRK);
	offLiveFlag(LIVE_FLAG_UNK10);
	unk1B8 = 90.0f;
	unk1B4 = true;
	gpMSound->startSoundActor(0x2861, &mPosition, 0, nullptr, 0, 4);
	unk1CC = 0;
	unk1CD = false;
}

void TPopo::thrownByChorobei()
{
	// The vertebrae stack is cleared and the nerve set directly (no push),
	// which is TSpineBase::initWith().
	mSpine->initWith(&TNervePopoThrown::theNerve());
}

const char** TPopo::getBasNameTable() const
{
	return (const char**)popo_bastable;
}

DEFINE_NERVE(TNervePopoPossessedNozzle, TLiveActor)
{
	TPopo* self = (TPopo*)spine->getBody();

	if (spine->getTime() == 0) {
		if (((TPopoManager*)self->mManager)->unk60 == 0) {
			spine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
			return TRUE;
		}
		((TPopoManager*)self->mManager)->unk60 = 0;
		self->possessedIn();
	}

	if (self->checkCurAnmEnd(0)) {
		if (self->unk165) {
			self->unk165 = false;
			self->setBckAnm(3);
			self->mMActor->setFrameRate(SMSGetAnmFrameRate(), ANM_TYPE_BTP);
		} else {
			self->setBckAnm(4);
			self->mMActor->getFrameCtrl(ANM_TYPE_BTP)->setFrame(0.0f);
			self->mMActor->setFrameRate(0.0f, ANM_TYPE_BTP);
		}
	}

	if (self->checkTrigger()) {
		spine->pushAfterCurrent(&TNervePopoFly::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNervePopoAttack, TLiveActor)
{
	TPopo* self = (TPopo*)spine->getBody();

	if (spine->getTime() == 0)
		self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));

	if (!self->checkLiveFlag2(LIVE_FLAG_AIRBORNE)) {
		if (((TPopoManager*)self->mManager)->unk60 == 0)
			return TRUE;

		// TODO: the flag has no name in Player/MarioFlags.hpp yet.
		if (gpMarioOriginal->checkFlag(MARIO_FLAG_VISIBLE))
			return TRUE;

		if (fabs(SMS_GetMarioPos().y - self->mPosition.y)
		    > ((TSmallEnemyParams*)self->getSaveParam())
		          ->getSLGiveUpHeight())
			return TRUE;

		if (self->isResignationAttack())
			return TRUE;

		self->walkBehavior(0, 1.0f);
	}
	return FALSE;
}

DEFINE_NERVE(TNervePopoFly, TLiveActor)
{
	TPopo* self = (TPopo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(2);

		// The water gun's emit matrix gives the direction to be flung in; the
		// scale is the body scale relative to the maximum water scale.
		MtxPtr emitMtx = SMS_GetMarioWaterGun()->getEmitMtx(0);
		f32 scale     = self->unk198 / self->unk194->getWaterScaleMax();
		f32 speed     = self->unk194->getReleaseSpeed() * scale;

		JGeometry::TVec3<f32> dir;
		dir.x = speed * emitMtx[0][0];
		dir.y = speed * emitMtx[1][0];
		dir.z = speed * emitMtx[2][0];
		self->mLinearVelocity = dir;

		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		if (self->unk1B4) {
			((TPopoManager*)self->mManager)->unk60 = 1;
			self->unk1B4 = false;
		}

		// Yaw the body along the fling direction. This is
		// MsGetRotFromZaxisY(), but that inline in MarioUtil/MathUtil.hpp
		// compares with >= where the binary uses >.
		f32 ang;
		if (dir.z == 0.0f)
			ang = dir.x > 0.0f ? 90.0f : -90.0f;
		else if (dir.z > 0.0f)
			ang = 0.005493164f * matan(dir.z, dir.x);
		else
			ang = 180.0f - 0.005493164f * matan(-dir.z, dir.x);

		self->mRotation.set(0.0f, MsWrap<f32>(ang, 0.0f, 360.0f), 0.0f);

		if (TPopo::mExplosionSw)
			self->offHitFlag(HIT_FLAG_NO_COLLISION);
	} else if (!self->checkLiveFlag2(LIVE_FLAG_AIRBORNE)) {
		spine->pushAfterCurrent(&TNervePopoExplosion::theNerve());
		return TRUE;
	}

	if (spine->getTime() > 5) {
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
		self->unk23C->offHitFlag(HIT_FLAG_NO_COLLISION);
	}
	self->flyBehavior();
	return FALSE;
}

DEFINE_NERVE(TNervePopoExplosion, TLiveActor)
{
	TPopo* self = (TPopo*)spine->getBody();

	if (spine->getTime() == 0) {
		JGeometry::TVec3<f32> zero(0.0f, 0.0f, 0.0f);
		self->mLinearVelocity = zero;
		self->mMActor->setFrameRate(0.0f, ANM_TYPE_BRK);
		if (self->unk1B4) {
			((TPopoManager*)self->mManager)->unk60 = 1;
			self->unk1B4 = false;
		}
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->onLiveFlag(LIVE_FLAG_UNK8);

		MtxPtr joint = self->mMActor->getModel()->getAnmMtx(self->mCenterJntIndex);
		self->unk1C0.set(joint[0][3], joint[1][3], joint[2][3]);
		gpMarioParticleManager->emit(0xA1, &self->unk1C0, 0, nullptr);
		gpMarioParticleManager->emit(0xA2, &self->unk1C0, 0, nullptr);

		if (gpMSound->gateCheck(0x297F))
			gpMSound->startSoundActor(0x297F, &self->mPosition, 0, nullptr, 0,
			                          4);
	}

	if (spine->getTime() > self->unk194->getExplosionEmitTime()) {
		self->onLiveFlag(LIVE_FLAG_DEAD);
		self->onLiveFlag(LIVE_FLAG_UNK8);
		self->offLiveFlag(LIVE_FLAG_UNK8000000 | LIVE_FLAG_UNK10000000);
		self->offLiveFlag(LIVE_FLAG_UNK2000 | LIVE_FLAG_CALC_INT_FRAME);
		self->mHolder = nullptr;
		spine->reset();
		self->stopAnmSound();
		spine->pushNerve(&TNerveSmallEnemyDie::theNerve());
		return TRUE;
	}

	self->explosion();
	return FALSE;
}

DEFINE_NERVE(TNervePopoWait, TLiveActor)
{
	TPopo* self = (TPopo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->onLiveFlag(LIVE_FLAG_UNK10);
		self->receiveMessage(self, HIT_MESSAGE_PUT);
	}

	// both the current path node and its copy point at Mario, and the pending
	// path is reset
	self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));

	self->walkToCurPathNode(0.0f, 0.0f, self->mTurnSpeed);
	return FALSE;
}

DEFINE_NERVE(TNervePopoThrown, TLiveActor)
{
	TPopo* self = (TPopo*)spine->getBody();

	if (spine->getTime() > 30 && !self->checkLiveFlag2(LIVE_FLAG_AIRBORNE)) {
		spine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
		return TRUE;
	}
	return FALSE;
}
