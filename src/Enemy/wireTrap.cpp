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

	// The target converts the int through MWCC's `2^52 + 2^31` magic pair (the
	// `xoris` is part of that sequence, not a negation -- doScaleDown()'s
	// `1.0f / (f32)param` compiles the same way and does match), divides by
	// 10.0f in double and lets the `stfs` do the narrowing.
	mScaleRate = (f32)scaleRate / 10.0f;

	if (wireNumber == -1)
		wireNumber = 0;
	mWireNumber = (s16)wireNumber;

	// TODO: the target looks the same material up in both branches and only
	// the tev colour differs; the duplicated getIndex() call is in the
	// original too.  The register id is the raw value 3.  The index is kept in
	// a full word and only narrowed at the call, so the local is an int.
	if (matIndex == -1) {
		int index = getModel()->getModelData()->mMaterialName->getIndex(
		    cMatName);
		SMS_InitPacket_OneTevColor(getModel(), (u16)index, (GXTevRegID)3,
		                           &cRedColor);
	} else {
		int index = getModel()->getModelData()->mMaterialName->getIndex(
		    cMatName);
		SMS_InitPacket_OneTevColor(getModel(), (u16)index, (GXTevRegID)3,
		                           &cBlueColor);
	}

	getWireBinder()->init(mPosition);

	mSpine->reset();
	mSpine->setNext(getNerveFromMode(mWireNumber));

	// The direction is built through the out-of-line
	// JGeometry::TVec3<f32>::set(f32, f32, f32), which is why the target
	// keeps it in memory (and why the constructor spelling does not match).
	s32 trigAngle = (s32)(182.04445f * mRotation.y);
	u32 trigIndex = (u16)trigAngle >> jmaSinShift;
	JGeometry::TVec3<f32> dir;
	dir.set(1.0f * jmaSinTable[trigIndex], 0.0f,
	        1.0f * jmaCosTable[trigIndex]);

	// NOTE: the target reads the binder's direction vector through
	// getWireBinder()->getDir() here, not through the out-of-line
	// getWireDir().
	mWireLength = 0.0f <= dir.dot(getWireBinder()->getDir()) ? 1.0f : -1.0f;
	mMomentum   = 1.0f;
}

// TODO: not reconstructed yet -- see mario.MAP for the UNUSED list.
void TWireTrap::initCollision() { }
void TWireTrap::initParticle() { }
void TWireTrap::initWire() { }
void TWireTrap::initThisColor(const GXColorS10*) { }

BOOL TWireTrap::receiveMessage(THitActor* sender, u32 message)
{
	// The jump table in the target covers messages 4..0xF; only 4, 7, 8, 0xB
	// and 0xF have a body, everything else (including anything out of range)
	// jumps straight to TSpineEnemy::receiveMessage().
	switch (message) {
	// NOTE: the case order below is load-bearing -- MWCC emits the switch
	// bodies in source order, and the target puts the 0xF body immediately
	// after the jump-table dispatch.
	case HIT_MESSAGE_SPRAYED_BY_WATER: {
		JGeometry::TVec3<f32> one(1.0f, 1.0f, 1.0f);
		SMS_EasyEmitParticle<E_SMS_EFFECT_ONETIME_NORMAL>(
		    E_SMS_EFFECT_ONETIME_NORMAL(0xe7), &mPosition, (const void*)nullptr,
		    one);
		// NOTE: no gateCheck() here, unlike moveObject().
		gpMSound->startSoundSet(0x6802, &mPosition, 0, 0.0f, 0, 0, 4);
		setScaleTimer(30);

		// NOTE: `one` has to be a named local declared *before* these two (it
		// is still emitted last in the frame) and `delta` before `dir`; that
		// combination is what reproduces the target's stack slot order
		// (delta @ 0x54, dir @ 0x60, one @ 0x6c).  The frame itself is still
		// 8 bytes short (0x78 vs 0x80) -- see the TODO at the bottom.
		JGeometry::TVec3<f32> delta = mPosition;
		delta -= *gpMarioPos;
		// The target reads the binder's direction vector straight through
		// getWireBinder()->getDir() here.
		JGeometry::TVec3<f32> dir = getWireBinder()->getDir();
		dir *= mWireLength;

		if (0.0f <= delta.dot(dir)) {
			mScale = getWireTrapParams()->mInWaterPowerRate.get();
		} else {
			mScale = -getWireTrapParams()->mInWaterPowerRate.get();
		}
		return TRUE;
	}

	case HIT_MESSAGE_TAKE:
		// 0x68 is TTakeActor::mHolder, not a "has a model" pointer.
		if (mHolder == nullptr) {
			onHitFlag(HIT_FLAG_NO_COLLISION);
			mHolder = (TTakeActor*)sender;
			return TRUE;
		}
		break;

	case HIT_MESSAGE_THROWN:
	case HIT_MESSAGE_UNK8:
		if (mHolder != nullptr) {
			mHolder = nullptr;
			return TRUE;
		}
		break;

	case HIT_MESSAGE_UNKB:
		// vtable slot 0xE4 -- kill().
		kill();
		return TRUE;

	default:
		break;
	}
	return TSpineEnemy::receiveMessage(sender, message);
	// TODO: the target's frame is 0x80 bytes and its locals start at 0x54;
	// this build ends up with a 0x78 byte frame whose locals start at 0x48.
	// The relative slot order is right, so the only thing missing is 12 bytes
	// of (unreferenced) spill area under the locals.  Spelling `delta` as the
	// temporary of `(mPosition - *gpMarioPos)` puts the slot at 0x68 and makes
	// the frame 0x88 instead, so that is not the answer.
}

// TODO: not reconstructed yet.
void TWireTrap::behaveHitWater(THitActor*) { }

void TWireTrap::kill()
{
	if (checkLiveFlag(LIVE_FLAG_DEAD))
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
// TSpineEnemy::calcRootMatrix() when 0x68 (TTakeActor::mHolder, see
// receiveMessage) is set, and otherwise emits a bound particle per frame
// before rebuilding the root matrix from mPosition/mRotation/mScaling.
void TWireTrap::calcRootMatrix() { }

void TWireTrap::moveObject()
{
	// TODO: the target re-reads mScaleTimer between the test and the
	// decrement (and keeps the first read in a scratch register), so the
	// decrement must not reuse the value the test produced -- that is the
	// signature of the by-value getter, whose return value is bound to a
	// compiler temporary instead of being shared with the caller's read.
	if (getScaleTimer() > 0) {
		setScaleTimer(getScaleTimer() - 1);
		if (mScaleTimer <= 0)
			mScale = 0.0f;
	}

	setHitParams(20.0f * mScaling.x, 30.0f * mScaling.y, 40.0f * mScaling.x,
	             40.0f * mScaling.y);
	calcEntryRadius();
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
//
// Inlining shapes that were needed to get the three move nerves to line up
// (all measured with tools/decomp-diff.py, see the notes on each function):
//  * `v *= f` (TVec3::operator*=) instead of `v.scale(f)`.  `operator*=` is
//    the only place the compiler is willing to leave `TVec3::scale` as a real
//    `bl`, which is what the target does at all four of its call sites.
//  * the scale factor must be a single ternary expression.  Written as
//    `f32 scale = 1.0f; if (...) scale = ...;` MWCC merges the two branches
//    (one hoisted `lfs` of 1.0f, no `b`), and written as an if/else it pushes
//    calcMomentum over the inline budget so the nerves stop inlining it.
//  * the direction sign has to be a nested ternary, not an if/else-if chain:
//    the chain costs enough extra to push doSearchMove past the inline
//    threshold, and the nerve then calls it out of line (38% instead of 88%).

// The shared "scale the wire direction and hand it to TLiveActor" step; every
// do*Move() ends with it.  0xdc bytes in mario.MAP -- the out-of-line copy
// compiles to exactly that, which is what pins the body down.
void TWireTrap::calcMomentum()
{
	JGeometry::TVec3<f32> velocity = getWireDir();

	// Note the operand order: the target computes 1.0f + mScale * timer/30,
	// i.e. the wire shrinks while the timer runs.  The `xoris` on the int is
	// part of MWCC's (int)->(f32) conversion sequence, not a negation of the
	// source, so the term must be written with a plain cast.
	f32 scale = mScaleTimer > 0 ? 1.0f + mScale * (f32)mScaleTimer / 30.0f
	                            : 1.0f;

	velocity *= mWireLength * scale;
	velocity *= mScaleRate;
	mLinearVelocity = velocity;
}

// TODO: "is the wire trap currently reacting to being hit?" -- body unknown.
f32 TWireTrap::getWaterPow() const // 0x3c
{
	return 0.0f;
}

// TODO: the map gives these 0x114 / 0x104 bytes while calcMomentum() above
// compiles to 0xdc, so the originals do a little more than calcMomentum; the
// inlined copies inside the two nerves are byte-identical to calcMomentum's
// body, so the extra work is not visible from there.
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
	if (getSearchTimer() > 0)
		setSearchTimer(getSearchTimer() - 1);

	JGeometry::TVec3<f32> delta = *gpMarioPos;
	delta -= mPosition;

	// The target reads the binder's direction vector straight through
	// getWireBinder()->getDir() here (both inlined), while every other call
	// site in the object goes through the out-of-line getWireDir().
	f32 dot = delta.dot(getWireBinder()->getDir());

	// The nested ternary is load-bearing: the equivalent if/else-if chain
	// makes doSearchMove too big for the inliner, and the nerve then calls it
	// out of line (TNerveWireTrapSearch::execute drops from 88% to 38%).
	// The target's three `li r0, 1 / li r0, -1 / li r0, 0` fall out of it.
	int dir = dot > 0.0f ? 1 : (dot < 0.0f ? -1 : 0);

	// Plain cast, not -(f32)dir: MWCC's (int)->(f32) sequence already starts
	// with an `xoris`, and adding the negation on top produces an extra
	// `fneg` the target does not have.
	mWireLength = (f32)dir;

	// The tail is literally calcMomentum(): calling it (rather than repeating
	// the code) is what pushes TVec3::scale one inline level deep enough to
	// stay an out-of-line `bl`, exactly as in the target.
	calcMomentum();
}

bool TWireTrap::doScaleUp() // 0xa4
{
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mMomentum = mMomentum
	            + 1.0f / (f32)getWireTrapParams()->mScaleTimerMax.get();

	if (1.0f <= mMomentum) {
		mMomentum = 1.0f;
		offHitFlag(HIT_FLAG_NO_COLLISION);
		return true;
	}
	return false;
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
	// TODO: the target never inlines this -- there is a real `bl getWireDir`
	// at all seven call sites in the object -- while this build inlines it and
	// leaves `bl getWireBinder` + `bl getDir` behind at the call sites inside
	// the move nerves.  Spelling the binder out as a reference is enough to
	// make the body look big enough for the inliner to leave it alone at the
	// deeper of those sites (TNerveWireTrapReturnMove 94.4% -> 97.2%); the
	// out-of-line copy is unchanged (still 3 instructions, still matching).
	// The dot product in doSearchMove() is the one place the target *does*
	// inline, and there it spells out getWireBinder()->getDir().
	const TWireBinder& binder = *getWireBinder();
	return binder.getDir();
}

bool TWireTrap::isReflect() const { return false; } // 0x14

// The target calls this out of line from load() (`bl getNerveFromMode` right
// after the mVertebrae.clear() that reset() does), so it must not be inlinable.
// The pragma has to be turned off again on the very next line: leaving it on
// for the rest of the TU stops every later definition from being inlined and
// costs the unit ~35 pp.
#pragma dont_inline on
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
#pragma dont_inline off

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

	// TODO: the target re-reads mSearchTimer between the test and the
	// decrement (`lwz / cmpwi / ble / lwz / subi / stw`); none of the getter
	// and operator spellings tried here stops MWCC from folding the two loads
	// together, so one instruction is still missing here.
	if (self->getSearchTimer() > 0)
		self->setSearchTimer(self->getSearchTimer() - 1);

	// `bool` (not BOOL) plus an explicit assignment in both arms: that is
	// what makes the target materialise `li r4, 1` / `li r4, 0` in each arm
	// and then re-test the result with `clrlwi. r0, r4, 24` instead of
	// keeping the flag in a register.
	bool ret;
	if (self->getWireBinder()->isEndWire(self->mPosition, self->mWireLength)) {
		self->mWireLength = self->mWireLength * -1.0f;
		ret = true;
	} else {
		self->doReturnMove();
		ret = false;
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

	if (self->getSearchTimer() > 0)
		self->setSearchTimer(self->getSearchTimer() - 1);

	// Same `bool ret` + explicit per-arm assignment reconstruction as in
	// TNerveWireTrapReturnMove.
	bool ret;
	if (self->getWireBinder()->isEndWire(self->mPosition, self->mWireLength)) {
		ret = true;
	} else {
		self->doOnewayMove();
		ret = false;
	}
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
		// Spelled as the negation of `<`: written as `0.0f >= mWireLength`
		// MWCC decomposes the test into `cror eq, gt, eq` + `bne` and drops
		// the unconditional branch; the target branches with a plain `bge`.
		// One `b` is still missing (the target's if has an empty else).
		f32 t = 0.0f;
		if (!(0.0f < self->mWireLength))
			t = 1.0f;
		t += 0.01f * self->mWireLength;
		self->getWireBinder()->getPoint(&self->mPosition, t);
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveWireTrapSearch, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();

	// The target stores 0 into the return slot on *both* paths (`li r4, 0`
	// at the top of the body and `li r4, 0` at the end of it), so the
	// pushAfterCurrent() pair below is unreachable and this nerve always
	// returns FALSE.  The original almost certainly meant `ret = TRUE` in
	// the "Mario is on the wire" arm; written as it is, it is the only
	// reading that reproduces the branch layout *and* keeps the dead block.
	bool ret;
	if (SMS_IsMarioOnWire()) {
		self->doSearchMove();
		ret = false;
	} else {
		ret = false;
	}

	if (ret) {
		spine->pushAfterCurrent(this);
		spine->pushAfterCurrent(&TNerveWireTrapWait::theNerve());
		return TRUE;
	}
	return FALSE;
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
