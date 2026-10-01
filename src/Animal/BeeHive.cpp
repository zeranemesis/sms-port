#include <Animal/BeeHive.hpp>

// rogue include: the original TU opens .rodata with the dummy string pair from
// System/DummyStrings.hpp followed by the four MtxCalcTypeName entries; without
// it every string offset in this object is shifted.
#include <M3DUtil/InfectiousStrings.hpp>
#include <Animal/boid.hpp>
#include <M3DUtil/MActor.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MSound/MSound.hpp>
#include <Strategic/ObjModel.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <Map/Map.hpp>
#include <JSystem/JMath.hpp>
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

namespace {
// TU-local: doWait() compares the nest's swing angle against it and returns
// that. Nothing in this TU writes it, so it is a plain zero-initialised
// static in .sbss -- the game must poke it from elsewhere (or it is simply
// still 0.0f, which makes doWait() return "still swinging" forever).
f32 cAngleLimit;
} // namespace

// The original object emits JGeometry's TQuat4::setRotate,
// TRotation3::identity33 and TVec3::scale as out-of-line `weak` symbols and
// reaches them with `bl` (see the `setRotate__Q29JGeometry9TQuat4<f>...`,
// `identity33__Q29JGeometry64TRotation3<...>` and `scale__Q29JGeometry8TVec3<f>`
// entries that marioEU.MAP attributes to Animal.a BeeHive.cpp). The headers
// define them inline, so call sites expand a 30-odd instruction body instead
// and the whole frame and register allocation slide. JSystem is off-limits, so
// route the three sites through TU-local wrappers -- that restores the call
// shape without touching a shared header.
#pragma dont_inline on
static void orig_identity33(
    JGeometry::TRotation3<JGeometry::TMatrix34<JGeometry::SMatrix34C<f32> > >*
        m)
{
	m->identity33();
}
static void orig_setRotate(JGeometry::TQuat4<f32>* q,
                           const JGeometry::TVec3<f32>& a,
                           const JGeometry::TVec3<f32>& b, f32 amount)
{
	q->setRotate(a, b, amount);
}
static void orig_scale(JGeometry::TVec3<f32>* v, f32 s,
                       const JGeometry::TVec3<f32>& b)
{
	v->scale(s, b);
}
static f32 orig_dot(const JGeometry::TVec3<f32>& v,
                    const JGeometry::TVec3<f32>& b)
{
	return v.dot(b);
}
static f32 orig_inv_sqrt(f32 v) { return JGeometry::TUtil<f32>::inv_sqrt(v); }
#pragma dont_inline off

TBee::TBee(MActor* actor, TBeeHive* owner)
    : TRealoidActor(actor)
    , mOwner(owner)
{
}

void TBee::init()
{
	initHitActor(0x1000002F, 1, -0x80000000, 20.0f, 20.0f, 50.0f, 50.0f);

	TIdxGroupObj* group
	    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search(
	          "敵グループ"));
	group->getChildren().push_back(this);

	onHitFlag(HIT_FLAG_NO_COLLISION);
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

// Fabricated: the parameter block is never emitted as a symbol (the ctor is
// always inlined into TBeeHiveManager::load), so it lives in this TU only.
// The member names come from the .prm key strings in the original's .rodata
// and the defaults from the values the inlined ctor stores.
class TBeeHiveParams : public TSpineEnemyParams {
public:
	TBeeHiveParams(const char* path);

	TParamRT<s32> mGiveupTimer;
	TParamRT<f32> mGiveupRange;
	TParamRT<s32> mDecrimentTimer;
	TParamRT<f32> mRebound;
	TParamRT<f32> mDecay;
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
    , PARAM_INIT(mRebound, 0.007f)
    , PARAM_INIT(mDecay, 0.98f)
    , PARAM_INIT(mAngleMaxAdd, 0.08f)
    , PARAM_INIT(mShakePower, 0.003f)
    , PARAM_INIT(mFallAngularVel, 0.01f)
    , PARAM_INIT(mSearchRange, 800.0f)
{
	TParams::load(mPrmPath);
}

TBeeHive::TBeeHive(const char* name)
    : TRealoid(name)
{
	unk1AC = nullptr;
}

void TBeeHive::init(TLiveManager* liveManager)
{
	mManager = liveManager;
	mManager->manageActor(this);
	onLiveFlag(LIVE_FLAG_UNK8 | LIVE_FLAG_UNK10);

	mSpine->initWith(&TNerveBeeHiveWait::theNerve());

	mHomePos = mPosition;
	mBeeNum  = 0;
	unk1BC   = 0;

	initHitActor(0x10000031, 0, -0x80000000, 50.0f, 50.0f, 100.0f, 100.0f);
	onHitFlag(HIT_FLAG_CANNOT_ATTACK);
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

TRealoidActor* TBeeHive::createRealoidActor(MActor* actor)
{
	return new TBee(actor, this);
}

BOOL TBeeHive::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER:
	{
		// Mario splashed the nest: swing it away from him.
		JGeometry::TVec3<f32> dir = mPosition;
		dir -= *gpMarioPos;

		// A nest that is already falling keeps its orientation.
		if (!mSpine->isNerve(&TNerveBeeHiveFall::theNerve())) {
			// Only the horizontal part steers the swing.
			JGeometry::TVec3<f32> flat = dir;
			flat.y = 0.0f;

			{
				f32 len = orig_dot(flat, flat);
				if (len <= JGeometry::TUtil<f32>::epsilon())
					flat.zero();
				else
					orig_scale(&flat,
					           JGeometry::TUtil<f32>::one()
					               * orig_inv_sqrt(len),
					           flat);
			}

			// A dead-swing nest always kicks forwards.
			f32 sign = 1.0f;
			if (fabsf(mVec188.y) >= 0.0001f)
				sign = mVec188.y > 0.0f ? 1 : (mVec188.y < 0.0f ? -1 : 0);

			orig_setRotate(&mQuat, JGeometry::TVec3<f32>(0.0f, 0.0f, 1.0f),
			               flat, 1.0f);

			// NOTE: the 0x43300000/2^52 double round-trip in the target is
			// MWCC's int->float lowering for "sign"; it is reproduced by
			// assigning the int difference straight into the f32.
			mVec188.y += sign * getParams()->mShakePower.get();
		}

		// A copy of the sender's transform drives the impact effect.
		JGeometry::TRotation3<JGeometry::TMatrix34<JGeometry::SMatrix34C<f32> > >
		    mtx;

		orig_identity33(&mtx);
		mtx.ref(0, 3) = sender->mPosition.x;
		mtx.ref(1, 3) = sender->mPosition.y;
		mtx.ref(2, 3) = sender->mPosition.z;

		gpMarioParticleManager->emitAndBindToMtx(PARTICLE_MS_ENM_WATHIT, mtx,
		                                         0, nullptr);

		gpMSound->startSoundSet(0x6802, (Vec*)&sender->mPosition, 0, 0.0f,
		                        0, 0, 4);
		return TRUE;
	}
	case HIT_MESSAGE_TRAMPLE:
	case HIT_MESSAGE_HIP_DROP:
	case HIT_MESSAGE_PUNCH:
		// A fresh kick: drop the queued chain and start the fall over.
		if (mSpine->isNerve(&TNerveBeeHiveWait::theNerve())) {
			mSpine->reset();
			mSpine->setNext(&TNerveBeeHiveFall::theNerve());
		}
		return TRUE;
	}
	return FALSE;
}

void TBeeHive::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TRealoid::perform(cue, graphics);
	TSpineEnemy::perform(cue, graphics);
}

void TBeeHive::control()
{
	controlCollision();
	TLiveActor::control();
	controlSound();
}

void TBeeHive::bind()
{
	if (checkLiveFlag(LIVE_FLAG_UNK10))
		return;

	JGeometry::TVec3<f32> pos = mPosition;
	pos += mLinearVelocity;
	pos += mVelocity;

	mVelocity.y -= getGravityY();
	if (mVelocity.y < mVelocityMinY)
		mVelocity.y = mVelocityMinY;

	// The hanging nest is probed at its chain's angle: the swing angle
	// mVec188.x is converted through the JMA 16-bit angle tables and the
	// resulting cosine is used as a -60 unit drop.
	s32 trigAngle = (s32)(182.04445f * (57.295776f * mVec188.x));
	f32 drop      = -60.0f * (-1.0f + jmaCosTable[(u16)trigAngle >> jmaSinShift]);

	mGroundHeight = gpMap->checkGround(pos.x, drop + (pos.y + mHeadHeight),
	                                   pos.z, &mGroundPlane);
	mGroundHeight += 1.0f;

	if (pos.y + drop <= mGroundHeight + 0.05f) {
		// The nest dies when it lands on a plane flagged as illegal.
		if (mGroundPlane->isIllegalData())
			kill();

		offLiveFlag(LIVE_FLAG_UNK40 | LIVE_FLAG_AIRBORNE);
		mVelocity.set(0.0f, 0.0f, 0.0f);
		pos.y = mGroundHeight - drop;
	} else {
		onLiveFlag(LIVE_FLAG_AIRBORNE);
	}

	gpMap->isTouchedOneWallAndMoveXZ(&pos.x, pos.y + mHeadHeight, &pos.z,
	                                 mBodyRadius);

	mLinearVelocity = pos - mPosition;
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

// TODO: (a) the original calls TRotation3::setSQ out of line here while our
// build inlines it, which shifts the frame and the tail of the function;
// (b) the y/z terms of JGeometry::TQuat4<T>::mul() look transposed in
// libs/JSystem's JGQuat4.hpp, so the product below does not match either.
void TBeeHive::calcRootMatrix()
{
	JGeometry::TQuat4<f32> quat;
	JGeometry::TQuat4<f32> shake;
	JGeometry::TRotation3<JGeometry::TMatrix34<JGeometry::SMatrix34C<f32> > >
	    mtx;

	// The nest wobbles: spin the stored orientation by a pitch proportional
	// to the current swing angle.
	quat = mVec178;
	shake.setEulerX(mVec188.x);
	quat.mul(quat, shake);

	mtx.setSQ(mScaling, quat);
	mtx.ref(0, 3) = mPosition.x;
	mtx.ref(1, 3) = mPosition.y;
	mtx.ref(2, 3) = mPosition.z;
	mtx.ref(1, 3) += 120.0f;

	PSMTXCopy(mtx, getModel()->getBaseTRMtx());
}

// The nest hangs from a chain, so mVec188 is a spring/damper oscillator:
// x = swing angle, y = swing velocity, z = pitch accumulator. This integrates
// it by one frame and returns whether the swing has grown past cAngleLimit.
BOOL TBeeHive::doWait()
{
	f32 vel = mVec188.y;

	f32 spring = mVec188.x * -getParams()->mRebound.get();
	mVec188.y += spring;
	mVec188.y *= getParams()->mDecay.get();
	mVec188.x += mVec188.y;

	// Once the angle swings outside the accumulated pitch, grow the pitch and
	// pull the angle back inside it.
	if (mVec188.x < -mVec188.z || mVec188.z < mVec188.x) {
		mVec188.z += getParams()->mAngleMaxAdd.get();

		f32 max = JGeometry::TUtil<f32>::halfPI();

		mVec188.z = (mVec188.z < 0.0f)
		                ? 0.0f
		                : (mVec188.z <= max ? mVec188.z : max);

		mVec188.x = (mVec188.x < -mVec188.z)
		                ? -mVec188.z
		                : (mVec188.x <= mVec188.z ? mVec188.x : mVec188.z);
	}

	mVec188.y = 0.0f;

	f32 swing = fabsf(mVec188.x);

	if (gpMSound->gateCheck(MSD_SE_EN_BEENEST_SWING)) {
		MSoundSESystem::MSoundSE::startSoundActorWithInfo(
		    MSD_SE_EN_BEENEST_SWING, &mPosition, nullptr, swing, 0, 0, nullptr, 0,
		    4);
	}

	// NOTE: the three-argument epsilonEquals() is spelled out because the
	// two-argument one constant-folds "-epsilon()" away, which is not what the
	// original emitted (it negates the loaded constant at run time).
	bool stopped
	    = JGeometry::TUtil<f32>::epsilonEquals(0.0f, mVec188.y,
	                                            JGeometry::TUtil<f32>::epsilon());

	if (!stopped && vel * mVec188.y <= 0.0f) {
		f32 swing2 = fabsf(mVec188.x);

		if (gpMSound->gateCheck(MSD_SE_EN_BEENEST_SWING)) {
			MSoundSESystem::MSoundSE::startSoundActorWithInfo(
			    MSD_SE_EN_BEENEST_SWING, &mPosition, nullptr, swing2, 0, 0,
			    nullptr, 0, 4);
		}
	}

	// The chain's direction is slerped a little towards the stored target so
	// the nest slowly turns to follow Mario.
	mTargetQuat.slerp(mQuat, 0.01f);

	f32 len = mTargetQuat.squared();
	if (len <= JGeometry::TUtil<f32>::epsilon())
		mTargetQuat.zero();
	else
		mTargetQuat.scale(JGeometry::TUtil<f32>::one()
		                  * JGeometry::TUtil<f32>::inv_sqrt(len));

	JGeometry::TVec3<f32> diff = *gpMarioPos;
	diff -= mPosition;

	TBeeHiveParams* params = getParams();

	if (diff.squared()
	    <= params->mSearchRange.get() * params->mSearchRange.get()) {
		unk150->mBaseSpeed         = 25.0f;
		unk150->mNeighborRadius    = 80.0f;
		unk150->mYawSpeed          = 8.0f;
		unk150->mPitchSpeed        = 8.0f;
		unk150->mMaxPitch          = 85.0f;
		unk150->mAlignmentStrength = 0.001f;

		JGeometry::TVec3<f32> offset;
		// NOTE: the original almost certainly used a TPathNode(THitActor*)
		// constructor here; our fabricated one copies the position with the
		// 3-argument set(), which loads the components in the wrong order.
		TPathNode goal;
		THitActor* mario = (THitActor*)gpMarioAddress;

		goal.unk0 = mario;
		goal.unk4.set(0.0f, 0.0f, 0.0f);
		if (mario)
			goal.unk4.set(mario->mPosition);

		unk150->mGoal = goal;

		offset.set(0.0f, 200.0f, 0.0f);
		unk150->mGoalOffset = offset;
	} else {
		unk150->mBaseSpeed         = 25.0f;
		unk150->mNeighborRadius    = 80.0f;
		unk150->mYawSpeed          = 8.0f;
		unk150->mPitchSpeed        = 8.0f;
		unk150->mMaxPitch          = 85.0f;
		unk150->mAlignmentStrength = 0.001f;

		JGeometry::TVec3<f32> offset;
		TPathNode goal;

		goal.unk0 = nullptr;
		goal.unk4 = mPosition;

		unk150->mGoal = goal;

		offset.set(0.0f, 0.0f, 0.0f);
		unk150->mGoalOffset = offset;
	}

	return cAngleLimit <= fabsf(mVec188.x);
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

JGeometry::TVec3<f32> TBeeHive::getCenterOfGravity() const
{
	int num = mBeeNum;

	if (num == 0)
		return JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);

	JGeometry::TVec3<f32> sum(0.0f, 0.0f, 0.0f);

	for (int i = 0; i < num; ++i)
		sum += unk150->getBoid(i)->mPosition;

	// NOTE: Gekko has no int->float instruction, so "1.0f / num" goes through
	// an int->f64 conversion (the 0x43300000/0x8000 bit trick below is MWCC's
	// doing, not ours) and is then divided as a single.
	f32 inv = 1.0f / num;

	sum *= inv;
	return sum;
}

TBeeHiveManager::TBeeHiveManager(const char* name)
    : TEnemyManager(name)
{
	// TODO: not yet decompiled
}

void TBeeHiveManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TBeeHiveParams("/Animal/beehive.prm");
	TEnemyManager::load(stream);
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

DEFINE_NERVE(TNerveBeeHiveFall, TLiveActor)
{
	TBeeHive* hive = (TBeeHive*)spine->getBody();

	if (spine->getTime() == 0) {
		hive->offLiveFlag(LIVE_FLAG_UNK10);
		hive->onLiveFlag(LIVE_FLAG_AIRBORNE);

		// NOTE: MSoundSESystem::MSoundSE's entry point is called directly --
		// going through MSound::startSoundActor() would gateCheck() twice,
		// which the original does not.
		if (gpMSound->gateCheck(MSD_SE_EN_BEENEST_OFF))
			MSoundSESystem::MSoundSE::startSoundActor(
			    MSD_SE_EN_BEENEST_OFF, &hive->mPosition, 0, nullptr, 0, 4);

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
			item->offLiveFlag(LIVE_FLAG_UNK10);
			item->onLiveFlag(LIVE_FLAG_AIRBORNE);
			hive->unk1AC = nullptr;
		}
	}

	hive->mVec188.x += hive->mVec188.y;

	// NOTE: bit 24 (0x01000000) has no name in LiveActor.hpp's enum; the
	// original tests it, not LIVE_FLAG_UNK4000.
	if (!hive->checkLiveFlag(0x01000000u)) {
		spine->pushAfterCurrent(&TNerveBeeHiveBreak::theNerve());
		return TRUE;
	}

	return FALSE;
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

		// NOTE: direct call, see TNerveBeeHiveFall::execute.
		if (gpMSound->gateCheck(MSD_SE_EN_BEENEST_LAND))
			MSoundSESystem::MSoundSE::startSoundActor(
			    MSD_SE_EN_BEENEST_LAND, &hive->mPosition, 0, nullptr, 0, 4);

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
