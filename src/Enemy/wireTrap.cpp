#include <Enemy/WireTrap.hpp>

#include <Enemy/Launcher.hpp>        // TNerveWaitForever (TWireTrap::init/kill)
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <MSound/MSound.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <Player/MarioAccess.hpp> // gpMarioPos / SMS_IsMarioOnWire
#include <Strategic/ObjModel.hpp> // TMActorKeeper
#include <Strategic/Spine.hpp>
#include <System/Particles.hpp>

// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp and the four MActorMtxCalcType names;
// without them every string offset in this object is shifted.
#include <M3DUtil/InfectiousStrings.hpp>
#include <System/DummyStrings.hpp>
// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// TODO: this is listed in mario.MAP as an UNUSED 4-byte function in an
// anonymous namespace.  The body is unknown; the size suggests a single
// instruction, i.e. a debug-only reporting stub that is compiled out.
namespace {
void SMSReportVec(const char*, const JGeometry::TVec3<f32>&) { }

const char* cMatName  = "_mat_1";
GXColorS10 cRedColor   = { 20, 210, 0, 15 };
GXColorS10 cBlueColor  = { 20, 0, 210, 0 };
}

// ---------------------------------------------------------------------------
// The definitions below are in reverse mario.MAP order, which is what the
// `-inline deferred` compilation of this TU requires.
// ---------------------------------------------------------------------------

TWireTrap::TWireTrap(const char* name)
    : TSpineEnemy(name)
{
	mMoveMode = 0;
	mScaleTimer = 0;
}

void TWireTrap::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("wire_trap.bmd", 0);
	// TODO: the cast is only needed because Enemy/WireBinder.hpp declares
	// `class TWireBinder : TBinder` (private) where the rest of the codebase
	// uses a public base; see the note in wireTrap.hpp.
	mBinder = (TBinder*)new TWireBinder;
	mSpine->initWith(&TNerveWaitForever<TLiveActor>::theNerve());
	initHitActor(0x10000026, 2, 0x90000000, 20.0f, 0.0f, 40.0f, 40.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	SMS_LoadParticle("/scene/wireTrap/jpa/ms_wrt_biri_a.jpa", 0x190);
	SMS_LoadParticle("/scene/wireTrap/jpa/ms_wrt_biri_b.jpa", 0x191);
}

void TWireTrap::load(JSUMemoryInputStream& stream)
{
	TSpineEnemy::load(stream);

	int scaleRate;
	int wireNumber;
	int matIndex;
	stream.read(&scaleRate, sizeof(scaleRate));
	stream.read(&wireNumber, sizeof(wireNumber));
	stream.read(&mWaitTime, sizeof(mWaitTime));
	stream.read(&matIndex, sizeof(matIndex));

	mScaleRate = (f32)scaleRate / 10.0f;

	if (wireNumber == -1)
		wireNumber = 0;
	mWireNumber = (s16)wireNumber;

	// TODO: the target looks the same material up in both branches and only
	// the tev colour differs; the duplicated getIndex() call is in the
	// original too.  The register id is the raw value 3.
	if (matIndex == -1) {
		u16 index = (u16)getModel()->getModelData()->mMaterialName->getIndex(
		    cMatName);
		SMS_InitPacket_OneTevColor(getModel(), index, (GXTevRegID)3,
		                           &cRedColor);
	} else {
		u16 index = (u16)getModel()->getModelData()->mMaterialName->getIndex(
		    cMatName);
		SMS_InitPacket_OneTevColor(getModel(), index, (GXTevRegID)3,
		                           &cBlueColor);
	}

	getWireBinder()->init(mPosition);

	mSpine->reset();
	mSpine->setNext(getNerveFromMode(mWireNumber));

	// Build the wire direction out of the actor's Y rotation and pick which
	// side of the wire it points at.  TODO: the target keeps `dir` in memory
	// because it hands &dir to an out-of-line TVec3::set(); ours scalarises
	// it, which is where the remaining frame difference comes from.
	s32 trigAngle = (s32)(182.04445f * mRotation.y);
	u32 trigIndex = (u16)trigAngle >> jmaSinShift;
	JGeometry::TVec3<f32> dir(1.0f * jmaSinTable[trigIndex], 0.0f,
	                         1.0f * jmaCosTable[trigIndex]);

	mWireLength = 0.0f <= dir.dot(getWireDir()) ? 1.0f : -1.0f;
	mMomentum   = 1.0f;
}

// TODO: not reconstructed yet -- see mario.MAP for the UNUSED list.
void TWireTrap::initCollision() { }
void TWireTrap::initParticle() { }
void TWireTrap::initWire() { }
void TWireTrap::initThisColor(const GXColorS10*) { }

BOOL TWireTrap::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: the jump table covers messages 4..0xF; the case bodies have not
	// been decompiled yet.
	return TSpineEnemy::receiveMessage(sender, message);
}

// TODO: not reconstructed yet.
void TWireTrap::behaveHitWater(THitActor*) { }

void TWireTrap::kill()
{
	// TODO: the flag is 0x80000000.  Strategic/LiveActor.hpp's LIVE_FLAG_*
	// enum stops at LIVE_FLAG_UNK10000000 (0x20000000 in GMSP01), so there is
	// no name for it yet; this is the last "UNKNOWN" bit of mLiveFlag.
	if (checkLiveFlag(0x80000000))
		return;

	onLiveFlag(LIVE_FLAG_DEAD);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mSpine->reset();
	mSpine->setNext(&TNerveWaitForever<TLiveActor>::theNerve());

	SMS_EasyEmitParticle<E_SMS_EFFECT_ONETIME_NORMAL>(
	    E_SMS_EFFECT_ONETIME_NORMAL(0xe4), &mPosition, (const void*)nullptr,
	    JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
	SMS_EasyEmitParticle<E_SMS_EFFECT_ONETIME_NORMAL>(
	    E_SMS_EFFECT_ONETIME_NORMAL(0xe6), &mPosition, (const void*)nullptr,
	    JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
}

// TODO: not reconstructed yet.
void TWireTrap::behaveHitWireTrap(TWireTrap*, const JGeometry::TVec3<f32>&,
                                 const JGeometry::TVec3<f32>&)
{
}

// TODO: not reconstructed yet.  The target bails out to
// TSpineEnemy::calcRootMatrix() when 0x68 (TWireTrap's "has a model" pointer)
// is set, and otherwise emits a bound particle per frame before rebuilding
// the root matrix from mPosition/mRotation/mScaling.
void TWireTrap::calcRootMatrix() { }

void TWireTrap::moveObject()
{
	// TODO: the target re-reads mScaleTimer between the test and the
	// decrement, and the frame is 0x10 bigger than the plain field access
	// needs, so the original almost certainly went through a getter here.
	if (mScaleTimer > 0) {
		setScaleTimer(getScaleTimer() - 1);
		if (mScaleTimer <= 0)
			mScale = 0.0f;
	}

	setHitParams(20.0f * mScaling.x, 30.0f * mScaling.y, 40.0f * mScaling.x,
	             40.0f * mScaling.y);
	checkHitActors();
	TLiveActor::moveObject();

	if (gpMSound->gateCheck(0x20bf))
		MSoundSESystem::MSoundSE::startSoundActor(0x20bf, &mPosition, 0,
		                                          nullptr, 0, 4);
}

// The `do*Move` / `doScale*` helpers below are all UNUSED in mario.MAP: each
// one is a real member function that the matching nerve `execute()` inlines
// (which is why they never get a live call site).  Their bodies were
// recovered from the inlined copies inside the nerve bodies, so the sizes
// recorded next to each one come straight from the map.

// The shared "scale the wire direction and hand it to TLiveActor" step; every
// do*Move() ends with it.  0xdc bytes in mario.MAP.
void TWireTrap::calcMomentum()
{
	JGeometry::TVec3<f32> velocity = getWireDir();

	f32 scale = 1.0f;
	if (mScaleTimer > 0)
		scale = 1.0f + mScale * (f32)mScaleTimer / 30.0f;

	velocity.scale(mWireLength * scale);
	velocity.scale(mScaleRate);
	mLinearVelocity = velocity;
}

// TODO: "is the wire trap currently reacting to being hit?" -- body unknown.
f32 TWireTrap::getWaterPow() const // 0x3c
{
	return 0.0f;
}

// TODO: the bodies of doOnewayMove / doReturnMove are the same in the target
// (0x104 / 0x114 bytes); they only differ in which direction flag they use.
void TWireTrap::doReturnMove() // 0x114
{
	calcMomentum();
}

void TWireTrap::doOnewayMove() // 0x104
{
	calcMomentum();
}

void TWireTrap::doSearchMove() // 0x1b8
{
	if (mSearchTimer > 0)
		mSearchTimer--;

	JGeometry::TVec3<f32> delta = *gpMarioPos;
	delta -= mPosition;

	f32 dot = delta.dot(getWireDir());

	int dir = 0;
	if (dot > 0.0f)
		dir = 1;
	else if (dot < 0.0f)
		dir = -1;

	mWireLength = (f32)dir;

	JGeometry::TVec3<f32> v = getWireDir();
	if (mScaleTimer <= 0)
		v.scale(1.0f);
	else
		v.scale(1.0f + mScale * ((f32)mScaleTimer - 30.0f) / 30.0f);
	v.scale(mScaleRate);
	mLinearVelocity = v;
}

BOOL TWireTrap::doScaleUp() // 0xa4
{
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mMomentum = mMomentum
	            + 1.0f / (f32)getWireTrapParams()->mScaleTimerMax.get();

	if (1.0f <= mMomentum) {
		mMomentum = 1.0f;
		offHitFlag(HIT_FLAG_NO_COLLISION);
		return TRUE;
	}
	return FALSE;
}

bool TWireTrap::doScaleDown() // 0xa8
{
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mMomentum = mMomentum
	            - 1.0f / (f32)getWireTrapParams()->mScaleTimerMax.get();

	if (mMomentum <= 0.0f) {
		mMomentum = 0.0f;
		offHitFlag(HIT_FLAG_NO_COLLISION);
		return TRUE;
	}
	return FALSE;
}

void TWireTrap::doResetToEdge() { } // 0x48

// TODO: not reconstructed yet.  The target walks TLiveActor::mCollisions and,
// for every other wire trap whose actor type is 0x10000026 (and which is not
// `this`), nudges both traps' momentum down and fires the "wait" nerve.  It
// calls getWireBinder()->getDir() plus getDirAtWirePos() six times per pair,
// which is what the six JGeometry::TVec3 locals in the target frame are.
void TWireTrap::checkHitActors() { }

// TODO: the following are all UNUSED in mario.MAP; bodies are guesses made
// from their call sites.
void TWireTrap::updateCollision() { }  // 0x54
void TWireTrap::emitEffects() { }      // 0xc8
void TWireTrap::setMoveMode(int mode)  // 0x160
{
	mMoveMode = mode;
}
JGeometry::TVec3<f32> TWireTrap::getDirAtWirePos() const // 0x3c
{
	return getWireDir();
}

const JGeometry::TVec3<f32>& TWireTrap::getWireDir() const
{
	return getWireBinder()->getDir();
}

bool TWireTrap::isReflect() const { return false; } // 0x14

TNerveBase<TLiveActor>* TWireTrap::getNerveFromMode(int mode)
{
	switch (mode) {
	case 0:
		return (TNerveBase<TLiveActor>*)&TNerveWireTrapReturnMove::theNerve();
	case 1:
		return (TNerveBase<TLiveActor>*)&TNerveWireTrapOnewayMove::theNerve();
	case 2:
		return (TNerveBase<TLiveActor>*)&TNerveWireTrapSearch::theNerve();
	default:
		return nullptr;
	}
}

f32 TWireTrap::getRangePosInWire() const { return 0.0f; } // 0x2c

TWireBinder* TWireTrap::getWireBinder() const { return (TWireBinder*)mBinder; }

bool TWireTrap::isStartWire() const { return false; } // 0x30
bool TWireTrap::isEndWire() const { return false; }   // 0x30

TWireTrapParams::TWireTrapParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mInWaterPowerRate, 0.9f)
    , PARAM_INIT(mScaleTimerMax, 60)
    , PARAM_INIT(mGoTimerMax, 90)
{
	TParams::load(mPrmPath);
}

TWireTrapManager::TWireTrapManager(const char* name) : TEnemyManager(name) { }

void TWireTrapManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TWireTrapParams("/enemy/wiretrap.prm");
	TEnemyManager::load(stream);
}

void TWireTrapManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "wire_trap.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

// ---------------------------------------------------------------------------
// Nerves
// ---------------------------------------------------------------------------

DEFINE_NERVE(TNerveWireTrapReturnMove, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();

	if (self->mSearchTimer > 0)
		self->mSearchTimer--;

	BOOL ret = self->getWireBinder()->isEndWire(self->mPosition,
	                                           self->mWireLength);
	if (ret) {
		self->mWireLength = self->mWireLength * -1.0f;
	} else {
		self->doReturnMove();
	}

	if (ret) {
		spine->pushAfterCurrent(this);
		spine->pushAfterCurrent(&TNerveWireTrapWait::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveWireTrapOnewayMoveStart, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();

	if (self->doScaleUp())
		return TRUE;
	return FALSE;
}

DEFINE_NERVE(TNerveWireTrapOnewayMove, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();

	if (spine->getTime() == 0 && self->mMomentum < 1.0f) {
		spine->pushAfterCurrent(this);
		spine->pushAfterCurrent(&TNerveWireTrapGoWait::theNerve());
		spine->pushAfterCurrent(&TNerveWireTrapOnewayMoveStart::theNerve());
		return TRUE;
	}

	if (self->mSearchTimer > 0)
		self->mSearchTimer--;

	BOOL ret = self->getWireBinder()->isEndWire(self->mPosition,
	                                           self->mWireLength);
	if (!ret)
		self->doOnewayMove();

	if (ret) {
		spine->pushAfterCurrent(this);
		spine->pushAfterCurrent(&TNerveWireTrapWait::theNerve());
		spine->pushAfterCurrent(&TNerveWireTrapOnewayMoveEnd::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveWireTrapOnewayMoveEnd, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();

	if (self->doScaleDown()) {
	// TODO: the target merges the branch with `cror eq, gt, eq` + `bne` and
	// tests the doScaleDown() result with `cmpwi` (no bool mask); our build
	// keeps a two-branch form and re-masks the inlined bool return. Same
	// operand order and semantics, different branch canonicalization.
	f32 t = (0.0f >= self->mWireLength ? 1.0f : 0.0f)
	        + 0.01f * self->mWireLength;
		self->getWireBinder()->getPoint(&self->mPosition, t);
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveWireTrapSearch, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();

	BOOL ret = SMS_IsMarioOnWire();
	if (ret)
		self->doSearchMove();

	if (ret) {
		spine->pushAfterCurrent(this);
		spine->pushAfterCurrent(&TNerveWireTrapWait::theNerve());
	}
	return ret;
}

DEFINE_NERVE(TNerveWireTrapWait, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();
	if (self->mWaitTime < spine->getTime())
		return TRUE;
	return FALSE;
}

DEFINE_NERVE(TNerveWireTrapGoWait, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();
	if (self->getWireTrapParams()->mGoTimerMax.get() < spine->getTime())
		return TRUE;
	return FALSE;
}
