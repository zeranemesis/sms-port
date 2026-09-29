#include <Animal/BeeHive.hpp>

// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <Animal/boid.hpp>
#include <M3DUtil/MActor.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MSound/MSound.hpp>
#include <Strategic/ObjModel.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <Player/MarioAccess.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <Strategic/ObjManager.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <System/Particles.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// Fabricated: the parameter block is never emitted as a symbol (the ctor is
// always inlined into TBeeHiveManager::load), so it lives in this TU only.
// The member names/defaults come from the .prm keys and the values stored by
// the inlined ctor in the original.
class TBeeHiveParams : public TSpineEnemyParams {
public:
	TBeeHiveParams(const char* path);

	TParamRT<u32> mGiveupTimer;
	TParamRT<f32> mGiveupRange;
	TParamRT<u32> mDecrimentTimer;
	TParamRT<f32> mDecay;
	TParamRT<f32> mRebound;
	TParamRT<f32> mAngleMaxAdd;
	TParamRT<f32> mShakePower;
	TParamRT<f32> mFallAngularVel;
	TParamRT<f32> mSearchRange;
};

TBeeHiveParams::TBeeHiveParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mGiveupTimer, 600)
    , PARAM_INIT(mGiveupRange, 1750.0f)
    , PARAM_INIT(mDecrimentTimer, 60)
    , PARAM_INIT(mDecay, 0.007f)
    , PARAM_INIT(mRebound, 0.98f)
    , PARAM_INIT(mAngleMaxAdd, 0.08f)
    , PARAM_INIT(mShakePower, 0.003f)
    , PARAM_INIT(mFallAngularVel, 0.01f)
    , PARAM_INIT(mSearchRange, 800.0f)
{
	TParams::load(mPrmPath);
}

DEFINE_NERVE(TNerveBeeHiveReset, TLiveActor)
{
	TBeeHive* hive = (TBeeHive*)spine->getBody();

	if (spine->getTime() == 0) {
		hive->reset();
		hive->offLiveFlag(LIVE_FLAG_HIDDEN);
		hive->mScaling.set(0.0f, 0.0f, 0.0f);
	}

	// The nest grows back into existence by ramping mScaling from 0 to 1 in
	// steps of 0.01 per frame.
	bool appeared = false;

	hive->mScaling.x += 0.01f;

	if (1.0f <= hive->mScaling.x) {
		hive->mScaling.set(1.0f, 1.0f, 1.0f);
		appeared = true;
	} else {
		hive->mScaling.y += 0.01f;
		hive->mScaling.z += 0.01f;
	}

	if (appeared) {
		spine->pushAfterCurrent(&TNerveBeeHiveWait::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveBeeHiveWait, TLiveActor)
{
	TBeeHive* hive = (TBeeHive*)spine->getBody();

	if (spine->getTime() == 0) {
		hive->onLiveFlag(LIVE_FLAG_UNK10 | LIVE_FLAG_AIRBORNE);

		if (hive->mBeeNum < 3) {
			for (int i = 0; i < 3; ++i)
				hive->appearBee(i);
			hive->mBeeNum = 3;
		}

		hive->unk150->mBaseSpeed         = 25.0f;
		hive->unk150->mNeighborRadius    = 80.0f;
		hive->unk150->mYawSpeed          = 8.0f;
		hive->unk150->mPitchSpeed        = 8.0f;
		hive->unk150->mMaxPitch          = 85.0f;
		hive->unk150->mAlignmentStrength = 0.001f;

		TPathNode goal(hive->mPosition);
		JGeometry::TVec3<f32> goalOffset(0.0f, 0.0f, 0.0f);

		hive->unk150->mGoal       = goal;
		hive->unk150->mGoalOffset = goalOffset;
	}

	if (hive->doWait()) {
		spine->pushAfterCurrent(&TNerveBeeHiveFall::theNerve());
		return TRUE;
	}

	if (hive->getParams()->mDecrimentTimer.get() < spine->getTime()) {
		if (3 < hive->mBeeNum) {
			--hive->mBeeNum;

			TRealoidActor* bee = hive->getRealoid(hive->mBeeNum);
			if (!(bee->mFlags & TRealoidActor::FLAG_UNK2)) {
				bee->onFlag(TRealoidActor::FLAG_UNK2);
				bee->onHitFlag(HIT_FLAG_NO_COLLISION);
			}
		}
	}

	spine->pushAfterCurrent(this);
	return TRUE;
}

DEFINE_NERVE(TNerveBeeHiveMarioWaterIn, TLiveActor)
{
	TBeeHive* hive = (TBeeHive*)spine->getBody();

	if (spine->getTime() == 0) {
		hive->unk150->mBaseSpeed         = 25.0f;
		hive->unk150->mNeighborRadius    = 80.0f;
		hive->unk150->mYawSpeed          = 8.0f;
		hive->unk150->mPitchSpeed        = 8.0f;
		hive->unk150->mMaxPitch          = 85.0f;
		hive->unk150->mAlignmentStrength = 0.001f;

		THitActor* mario = (THitActor*)gpMarioAddress;

		JGeometry::TVec3<f32> pos(0.0f, 0.0f, 0.0f);
		if (mario)
			pos.set(mario->mPosition);

		hive->unk150->mGoal.unk0 = mario;
		hive->unk150->mGoal.unk4 = pos;
		hive->unk150->mGoalOffset.set(0.0f, 0.0f, 500.0f);
	}

	if (hive->unk150->getBoidNum() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveBeeHiveReset::theNerve());
		return TRUE;
	}

	// The hive stops chasing Mario once he is inside a pool.
	bool inWater
	    = SMS_CheckMarioFlag(MARIO_FLAG_VISIBLE)
	      && SMS_CheckMarioFlag(MARIO_FLAG_IN_SHALLOW_WATER)
	      || !(*gpMarioGroundPlane)->isWaterSurface()
	      || SMS_CheckMarioFlag(MARIO_FLAG_IN_WATER);

	if (!inWater) {
		spine->pushAfterCurrent(&TNerveBeeHiveAttack::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveBeeHiveAttack, TLiveActor)
{
	TBeeHive* hive = (TBeeHive*)spine->getBody();

	if (spine->getTime() == 0) {
		hive->unk150->mBaseSpeed         = 25.0f;
		hive->unk150->mNeighborRadius    = 80.0f;
		hive->unk150->mYawSpeed          = 8.0f;
		hive->unk150->mPitchSpeed        = 8.0f;
		hive->unk150->mMaxPitch          = 85.0f;
		hive->unk150->mAlignmentStrength = 0.001f;

		THitActor* mario = (THitActor*)gpMarioAddress;

		JGeometry::TVec3<f32> pos(0.0f, 0.0f, 0.0f);
		if (mario)
			pos.set(mario->mPosition);

		hive->unk150->mGoal.unk0 = mario;
		hive->unk150->mGoal.unk4 = pos;
		hive->unk150->mGoalOffset.set(0.0f, 0.0f, 200.0f);
	}

	bool inWater
	    = SMS_CheckMarioFlag(MARIO_FLAG_VISIBLE)
	      && SMS_CheckMarioFlag(MARIO_FLAG_IN_SHALLOW_WATER)
	      || !(*gpMarioGroundPlane)->isWaterSurface()
	      || SMS_CheckMarioFlag(MARIO_FLAG_IN_WATER);

	if (inWater) {
		spine->pushAfterCurrent(&TNerveBeeHiveMarioWaterIn::theNerve());
		return TRUE;
	}

	if (hive->mBeeNum == 0)
		return FALSE;

	JGeometry::TVec3<f32> marioPos = *gpMarioPos;
	marioPos -= hive->getCenterOfGravity();

	hive->moveObject();

	if (hive->mGravity * hive->mGravity <= marioPos.squared())
		spine->pushAfterCurrent(&TNerveBeeHiveReset::theNerve());
	else
		return FALSE;

	return TRUE;
}

DEFINE_NERVE(TNerveBeeHiveBreak, TLiveActor)
{
	TBeeHive* hive = (TBeeHive*)spine->getBody();

	if (spine->getTime() == 0) {
		hive->mMActor = hive->mMActorKeeper->getMActor("bee_nest_break.bmd");
		hive->mMActor->setBck("bee_nest_break");
		hive->onHitFlag(HIT_FLAG_NO_COLLISION);

		SMS_EasyEmitParticle(PARTICLE_MS_ENM_DISAP_A, &hive->mPosition, hive,
		                     JGeometry::TVec3<f32>(2.0f, 2.0f, 2.0f));
		SMS_EasyEmitParticle(PARTICLE_MS_ENM_DISAP_B, &hive->mPosition, hive,
		                     JGeometry::TVec3<f32>(2.0f, 2.0f, 2.0f));

		if (gpMSound->gateCheck(MSD_SE_EN_BEENEST_LAND))
			gpMSound->startSoundActor(MSD_SE_EN_BEENEST_LAND, &hive->mPosition,
			                          0, 0, 0, 4);

		for (int i = 0; i < hive->unk150->getBoidNum(); ++i)
			hive->appearBee(i);
		hive->mBeeNum = hive->unk150->getBoidNum();

		hive->unk150->mBaseSpeed         = 25.0f;
		hive->unk150->mNeighborRadius    = 80.0f;
		hive->unk150->mYawSpeed          = 8.0f;
		hive->unk150->mPitchSpeed        = 8.0f;
		hive->unk150->mMaxPitch          = 85.0f;
		hive->unk150->mAlignmentStrength = 0.001f;

		THitActor* mario = (THitActor*)gpMarioAddress;

		JGeometry::TVec3<f32> pos(0.0f, 0.0f, 0.0f);
		if (mario)
			pos.set(mario->mPosition);

		hive->unk150->mGoal.unk0 = mario;
		hive->unk150->mGoal.unk4 = pos;
		hive->unk150->mGoalOffset.set(0.0f, 0.0f, 200.0f);
	}

	if (hive->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveBeeHiveAttack::theNerve());
		hive->onLiveFlag(LIVE_FLAG_HIDDEN);
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveBeeHiveFall, TLiveActor)
{
	TBeeHive* hive = (TBeeHive*)spine->getBody();

	if (spine->getTime() == 0) {
		hive->offLiveFlag(LIVE_FLAG_UNK8 | LIVE_FLAG_UNK10 | LIVE_FLAG_UNK20);
		hive->onLiveFlag(LIVE_FLAG_AIRBORNE);

		if (gpMSound->gateCheck(MSD_SE_EN_BEENEST_OFF))
			gpMSound->startSoundActor(MSD_SE_EN_BEENEST_OFF, &hive->mPosition, 0,
			                          0, 0, 4);

		JGeometry::TVec3<f32> vel;
		hive->mTargetQuat.getZDir(vel);
		hive->mLinearVelocity = vel;

		hive->mVec188.y = hive->getParams()->mFallAngularVel.get();

		TMapObjBase* item = hive->unk1AC;

		if (item != nullptr && item->isActorType(0x2000E))
			item = gpItemManager->makeObjAppear(0x2000E);

		if (item != nullptr) {
			item->makeObjAppeared();
			item->JSGSetTranslation(hive->mPosition);
			item->mLinearVelocity.set(0.0f, 0.0f, 0.0f);
			item->offLiveFlag(LIVE_FLAG_UNK8 | LIVE_FLAG_UNK10
			                  | LIVE_FLAG_UNK20);
			item->onLiveFlag(LIVE_FLAG_AIRBORNE);
			hive->unk1AC = nullptr;
		}
	}

	hive->mVec188.x += hive->mVec188.y;

	if (!hive->checkLiveFlag(LIVE_FLAG_UNK4000)) {
		spine->pushAfterCurrent(&TNerveBeeHiveBreak::theNerve());
		return TRUE;
	}

	return FALSE;
}

void TBeeHiveManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "bee_body.bmd", 0x10210000, 0 },
		{ "bee_nest.bmd", 0x10210000, 0 },
		{ "bee_nest_break.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TBeeHiveManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TBeeHiveParams("/Animal/beehive.prm");
	TEnemyManager::load(stream);
}

TBeeHiveManager::TBeeHiveManager(const char* name)
    : TEnemyManager(name)
{
	// TODO: not yet decompiled
}

JGeometry::TVec3<f32> TBeeHive::getCenterOfGravity() const
{
	if (mBeeNum == 0)
		return JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);

	JGeometry::TVec3<f32> sum(0.0f, 0.0f, 0.0f);

	for (int i = 0; i < mBeeNum; ++i)
		sum += unk150->getBoid(i)->mPosition;

	// The original does not divide by mBeeNum directly: it assembles the
	// divisor's bits by hand and then undoes the bias with a magic constant.
	// Transcribed as-is -- it does not evaluate to 1.0f / mBeeNum.
	f64 biased;
	*(u32*)&biased       = 0x43300000;
	*((u32*)&biased + 1) = (u32)(mBeeNum ^ 0x8000);

	f32 inv = 1.0f / (f32)(biased - 4503601774854144.0);

	sum *= inv;
	return sum;
}

// The original TU calls appearBee out of line from the nerves, so it must
// not be inlined at its call sites here.
#pragma dont_inline on
void TBeeHive::appearBee(int index)
{
	TRealoidActor* bee = getRealoid(index);
	if (!(bee->mFlags & TRealoidActor::FLAG_UNK4)
	    && (bee->mFlags & TRealoidActor::FLAG_UNK2)) {
		bee->offFlag(TRealoidActor::FLAG_UNK2);
		bee->offHitFlag(HIT_FLAG_NO_COLLISION);
		unk150->getBoid(index)->mPosition = mPosition;
	}
}
#pragma dont_inline off

BOOL TBeeHive::doWait()
{
	// TODO: not yet decompiled. Notes from the asm, in order:
	//  - vtable 0x108 is TSpineEnemy::getSaveParam(); the returned params block
	//    has floats at 0xF4 / 0x108 / 0x11C that drive a spring on mVec188
	//    (mVec188.y += mVec188.x * -p[0xF4]; mVec188.y *= p[0x108];
	//    mVec188.x += mVec188.y;), then mVec188.z is integrated with p[0x11C]
	//    and both are clamped (z to [0, 1.5707964], x to +/-z).
	//  - MSD_SE id 0x28F7 is played via MSoundSESystem::startSoundActorWithInfo
	//    whenever |mVec188.x| crosses a threshold.
	//  - mTargetQuat/mQuat (TVec4) get normalised with TUtil<f32>::inv_sqrt,
	//    then a 0.99 lerp plus a setEulerX-style pitch from acosf/sinf, then
	//    renormalised. mVec188 is a swing/pitch oscillator.
	//  - the tail compares |mVec188.x| against the file-static
	//    "cAngleLimit__9@unnamed@" and returns that bool.
	//  - unknown: the params-block member names, and why the accumulator is
	//    built from getManager()/MarioPos with the same double bit trick that
	//    getCenterOfGravity() uses.
	return FALSE;
}

void TBeeHive::reset()
{
	mPosition = mHomePos;

	JGeometry::TRotation3<JGeometry::SMatrix34C<f32> > rot;
	MsMtxSetRotRPH(rot, mRotation.x, 0.0f, mRotation.z);
	rot.getQuat(mVec178);

	mTargetQuat.setEulerY(mRotation.y);
	mQuat = mTargetQuat;

	mVec188.x = 0.0f;
	mVec188.y = 0.0f;
	mVec188.z = 0.015707964f;

	mCollisionIdx = 0;
	onLiveFlag(LIVE_FLAG_UNK10 | LIVE_FLAG_AIRBORNE);
	mMActor = mMActorKeeper->getMActor("bee_nest.bmd");
	offHitFlag(HIT_FLAG_NO_COLLISION);
}

// TODO: the original calls TRotation3::setSQ out of line here; our build
// inlines it, which shifts the frame and the whole tail of the function.
void TBeeHive::calcRootMatrix()
{
	JGeometry::TRotation3<JGeometry::TMatrix34<JGeometry::SMatrix34C<f32> > >
	    mtx;
	JGeometry::TQuat4<f32> quat = mVec178;
	JGeometry::TQuat4<f32> shake;

	// The nest wobbles: spin the stored orientation by the target quaternion,
	// then by a pitch proportional to the current swing angle.
	shake.setEulerX(mVec188.x);
	quat.mul(quat, mTargetQuat);
	quat.mul(quat, shake);

	mtx.setSQ(mScaling, quat);
	mtx.ref(0, 3) = mPosition.x;
	mtx.ref(1, 3) = mPosition.y;
	mtx.ref(2, 3) = mPosition.z;
	mtx.ref(1, 3) += 120.0f;

	PSMTXCopy(mtx, getModel()->getBaseTRMtx());
}

void TBeeHive::controlSound()
{
	if (mBeeNum == 0)
		return;

	f32 x = 0.0f;
	f32 y = x;
	f32 z = x;
	int count = 0;
	for (int i = 0; i < mBeeNum; ++i) {
		TRealoidActor* bee = getRealoid(i);
		if (!(bee->mFlags & TRealoidActor::FLAG_UNK2_OR_UNK4)) {
			x += bee->mPosition.x;
			y += bee->mPosition.y;
			z += bee->mPosition.z;
			++count;
		}
	}

	if (count == 0)
		return;

	f32 inv = 1.0f / count;
	mSoundPos.x = x * inv;
	mSoundPos.y = y * inv;
	mSoundPos.z = z * inv;
	gpMSound->startBeeSe((Vec*)&mSoundPos, count);
}

void TBeeHive::controlCollision()
{
	int idx = mCollisionIdx;
	int num = mBeeNum;

	TRealoidActor* bee = getRealoid(idx);
	bee->checkHitActors();
	bee->onHitFlag(HIT_FLAG_CANNOT_ATTACK);

	// TODO: this first wrap is dead but reproduces a dead compare in the
	// target (and keeps control() from inlining us); probably an inline
	// "next index" helper in the original.
	int next = idx + 1;
	if (num <= next)
		next = 0;

	if (num <= ++mCollisionIdx)
		mCollisionIdx = 0;

	next = mCollisionIdx;
	if (num <= next)
		next = 0;

	TRealoidActor* nextBee = getRealoid(next);
	if (!(nextBee->mFlags & TRealoidActor::FLAG_UNK2_OR_UNK4))
		nextBee->offHitFlag(HIT_FLAG_CANNOT_ATTACK);
}

void TBeeHive::bind()
{
	// TODO: not yet decompiled. Notes from the asm:
	//  - early-outs on mLiveFlag & LIVE_FLAG_UNK10.
	//  - pos = mPosition + mLinearVelocity + mVelocity, then a gravity step via
	//    the vtable-0xE8 virtual, clamped against TLiveActor::mVelocityMinY.
	//  - the ground probe converts mVec188.x through the JMA 16-bit angle tables
	//    (jmaSinShift / jmaCosTable) and calls TMap::checkGround with
	//    SHORTANGLE2DEG-style factors 57.295776 and 182.04445.
	//  - blocked in Map.hpp: TMap::isTouchedOneWallAndMoveXZ is declared as
	//    (f32*, f32, f32*, f32) but the original calls it as
	//    (f32, f32, f32*, f32*), so this cannot be written without patching
	//    include/Map/Map.hpp.
}

void TBeeHive::control()
{
	controlCollision();
	TLiveActor::control();
	controlSound();
}

void TBeeHive::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TRealoid::perform(cue, graphics);
	TSpineEnemy::perform(cue, graphics);
}

BOOL TBeeHive::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: not yet decompiled. Notes from the asm:
	//  - switch on 0/1/0xC (restart the Fall/Wait chain) and 0xF (the hive was
	//    hit): everything else returns FALSE.
	//  - the 0xF arm normalises (mPosition - *gpMarioPos), feeds it to
	//    mQuat.setRotate((0,0,1), dir, 1.0f) and then integrates mVec188.y
	//    with the same 0x43300000 double trick getCenterOfGravity() uses --
	//    that expression is the reason this arm is still missing.
	//  - both arms finish with identity33() on a local matrix translated to
	//    sender->mPosition, emitAndBindToMtx(0xE7) and startSoundSet(0x6802).
	return FALSE;
}

TRealoidActor* TBeeHive::createRealoidActor(MActor* actor)
{
	return new TBee(actor, this);
}

void TBeeHive::load(JSUMemoryInputStream& stream)
{
	loadDefault(stream, "bee_body.bmd", 2);

	u32 coinEventID;
	u32 nestEventID;
	stream.read(&coinEventID, sizeof(u32));
	stream.read(&nestEventID, sizeof(u32));

	// The item the nest is hanging from.
	unk1AC = TMapObjBaseManager::newAndRegisterObjByEventID(coinEventID, "");

	int num = unk150->getBoidNum();

	unk1B8 = new TMapObjBase*[num];

	// One coin per bee, plus a last one hanging off the nest itself.
	TMapObjBase** coin  = unk1B8;
	TMapObjBase** nestC = &unk1B8[num - 1];

	for (; coin != nestC; ++coin) {
		*coin = TMapObjBaseManager::newAndRegisterObj(
			"coin", JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f),
			JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f),
			JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
	}

	*coin = TMapObjBaseManager::newAndRegisterObjByEventID(nestEventID, "");

	if (*coin == nullptr) {
		*coin = TMapObjBaseManager::newAndRegisterObj(
			"coin", JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f),
			JGeometry::TVec3<f32>(0.0f, 0.0f, 1.0f),
			JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
	}

	mMActorKeeper->createMActor("bee_nest_break.bmd", 3);
	mMActor = mMActorKeeper->createMActor("bee_nest.bmd", 3);

	for (int i = 0; i < unk150->getBoidNum(); ++i) {
		TBee* bee = getBee(i);
		bee->init();
		bee->onFlag(TRealoidActor::FLAG_UNK2);
	}

	reset();
}

// TBee::receiveMessage() calls this out of line, so keep it that way.
#pragma dont_inline on
void TBeeHive::receiveMessageFromChild(TBee* bee)
{
	// A bee that already dropped its coin does nothing.
	if (bee->mFlags & TRealoidActor::FLAG_UNK4)
		return;

	bee->onFlag(TRealoidActor::FLAG_UNK4);
	bee->onHitFlag(HIT_FLAG_NO_COLLISION);

	TMapObjBase* coin = unk1B8[unk1BC];

	// The last bee spawns a fresh coin; the rest share the ones load() made.
	if (unk1BC != unk150->getBoidNum() - 1)
		coin = gpItemManager->makeObjAppear(0x2000E);

	if (coin != nullptr) {
		// NOTE: this is the call the target makes at vtable offset 0xFC, but
		// our TMapObjBase has one virtual too many ahead of makeObjAppeared(),
		// so we emit 0x100. See also TNerveBeeHiveFall::execute.
		coin->makeObjAppeared();
		coin->mPosition = bee->mPosition;
		coin->mVelocity.set(0.0f, 15.0f, 0.0f);
		coin->offLiveFlag(LIVE_FLAG_UNK20 | LIVE_FLAG_UNK40);
	}

	++unk1BC;
}
#pragma dont_inline off

void TBeeHive::init(TLiveManager* liveManager)
{
	mManager = liveManager;
	liveManager->manageActor(this);
	onLiveFlag(LIVE_FLAG_UNK8 | LIVE_FLAG_UNK10);

	mSpine->initWith(&TNerveBeeHiveWait::theNerve());

	mHomePos = mPosition;
	mBeeNum  = 0;
	unk1BC   = 0;

	initHitActor(0x10000031, 0, -0x80000000, 50.0f, 50.0f, 100.0f, 100.0f);
	onHitFlag(HIT_FLAG_CANNOT_ATTACK);
}

TBeeHive::TBeeHive(const char* name)
    : TRealoid(name)
{
	unk1AC = nullptr;
}

TBee::TBee(MActor* actor, TBeeHive* owner)
    : TRealoidActor(actor)
    , mOwner(owner)
{
}

BOOL TBee::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_TAKE:
		if (mHolder != nullptr)
			return FALSE;
		mHolder = (TTakeActor*)sender;
		SMS_EasyEmitParticle(PARTICLE_MS_ENM_WATHIT, &mPosition, nullptr,
		                     JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
		return TRUE;
	case HIT_MESSAGE_UNK8:
		if (mHolder == nullptr)
			return FALSE;
		mHolder = nullptr;
		return TRUE;
	case HIT_MESSAGE_UNKB:
		mOwner->receiveMessageFromChild(this);
		return TRUE;
	}
	return FALSE;
}

void TBee::init()
{
	initHitActor(0x1000002F, 1, -0x80000000, 20.0f, 20.0f, 50.0f, 50.0f);

	TIdxGroupObj* group
	    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search(
	          "蜂の棲む家"));
	group->getChildren().push_back(this);

	onHitFlag(HIT_FLAG_NO_COLLISION);
}

TBeeHiveManager::~TBeeHiveManager()
{
	// TODO: not yet decompiled
}

TBeeHive::~TBeeHive()
{
	// TODO: not yet decompiled
}

TBee::~TBee()
{
	// TODO: not yet decompiled
}
