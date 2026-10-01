#include <Enemy/AmiNoko.hpp>

// rogue include: the original TU opens .rodata with the dummy string
// pair and the four MtxCalcType names from M3DUtil/InfectiousStrings.hpp;
// without it every string offset in this object is shifted.
#include <M3DUtil/InfectiousStrings.hpp>
#include <Strategic/ObjManager.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/ObjModel.hpp> // TMActorKeeper
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Strategic/Strategy.hpp> // TIdxGroupObj
#include <System/MarDirector.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp> // TBGCheckData::mNormal, for attackToMario
#include <Map/MapCollisionData.hpp> // TBGWallCheckRecord, for calcDirection
#include <Player/MarioAccess.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <System/Particles.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <Enemy/Enemy.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <MSound/MSound.hpp> // gpMSound, for emitEffects
#include <System/EmitterViewObj.hpp> // gpMarioParticleManager, for emitEffects

// All four sqrt/inv_sqrt call sites in this ROM (WalkOnFence, Turn,
// TAmiHit::perform, calcDirection) are out-of-line calls to the weak copies
// in Animal.a boid.cpp, while TNerveAmiNokoDie expands the same body at its
// one site - so the default inline in JGUtil.hpp is right for Die and wrong
// for the other four. The header cannot serve both (one spelling, one
// mangled name), hence this TU-local non-inlined wrapper.
// FABRICATED: the callee is orig_inv_sqrt, not
// inv_sqrt__Q29JGeometry8TUtil<f>Ff, so the `bl` still shows as one
// mismatched instruction. Making JGUtil.hpp out-of-line and adding a
// *_inline twin was MEASURED across the repo: it is 7 units better and
// ~40 units worse, -32.2 points summed. See docs/AGENT_MATCHING_TIPS.md.
#pragma dont_inline on
static f32 orig_sqrt(f32 v) { return JGeometry::TUtil<f32>::sqrt(v); }
static f32 orig_inv_sqrt(f32 v) { return JGeometry::TUtil<f32>::inv_sqrt(v); }
#pragma dont_inline off

// This TU is -inline deferred: the definition order below is the reverse of
// the .text layout in mario.MAP.

static const char* amiNoko_bastable[] = {
	0,
	"/scene/amiNoko/bas/aminoko_flying1_start.bas",
	"/scene/amiNoko/bas/aminoko_hit1.bas",
	0,
	"/scene/amiNoko/bas/aminoko_run1_loop.bas",
	0,
	0,
	"/scene/amiNoko/bas/aminoko_run2_loop.bas",
	0,
	0,
	"/scene/amiNoko/bas/aminoko_turn1_loop.bas",
	0,
	0,
	"/scene/amiNoko/bas/aminoko_turn2_loop.bas",
	0,
	0,
};


TAmiNokoManager::TAmiNokoManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TAmiNokoManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TAmiNokoParams("/enemy/amiNoko.prm");
	TSmallEnemyManager::load(stream);
}

// TODO: 0x10220000 flags not fully decoded (J3DMLF_* bitfield)
void TAmiNokoManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "aminoko_model1.bmd", 0x10220000, 0 },
		{ 0, 0, 0 },
	};
	createModelDataArray(entry);
}

TSmallEnemy* TAmiNokoManager::createEnemyInstance() { return 0; }

BOOL TAmiHit::receiveMessage(THitActor* sender, u32 message)
{
	return mParent->receiveMessage(sender, message);
}

void TAmiHit::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_MOVE) {
		JGeometry::TVec3<f32> v = mParent->unk19C;
		// TVec3::setLength() spelled out so the inv_sqrt call and the two
		// epsilon tests survive verbatim.
		f32 lsq = v.x * v.x + v.y * v.y + v.z * v.z;
		if (lsq <= JGeometry::TUtil<f32>::epsilon())
			v.zero();
		else
			v.scale(1.0f * orig_inv_sqrt(lsq), v);
		v.scale(100.0f, v);

		mPosition = mParent->mPosition;
		mPosition.x += v.x;
		mPosition.y += v.y;
		mPosition.z += v.z;
		mPosition.y -= 0.5f * mAttackHeight;

		// Raw bit test (clrlwi), not checkLiveFlag(): MWCC turns the single-bit
		// mask into a rlwinm. on the loaded word.
		if (!(mParent->mLiveFlag & LIVE_FLAG_DEAD)) {
			for (int i = 0; i < (int)mColCount; i++) {
				if (mCollisions[i]->mActorType == 0x80000001)
					mParent->attackToMario();
			}
		}
	}

	THitActor::perform(cue, graphics);
}


TAmiNoko::TAmiNoko(const char* name)
    : TWalkerEnemy(name)
{
	unk194 = 0;
	unk198 = 0;
	unk20C = 1;
	unk19C.set(0.0f, 1.0f, 0.0f);
	unk1A8.set(0.0f, 0.0f, 1.0f);
	unk1B4 = unk19C;
	unk1C0 = unk1A8;
}

void TAmiNoko::load(JSUMemoryInputStream& stream)
{
	TSpineEnemy::load(stream);
	stream.read(&mCoinId, 4);
}

void TAmiNoko::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000021;
	unk150     = 0x11;
	mSpine->initWith(&TNerveAmiNokoWalkOnFence::theNerve());
	unk208 = getSaveParam();
	reset();
	setMActorAndKeeper();
	onLiveFlag(LIVE_FLAG_UNK10);
	initialGraphNode();
	if (gpMarDirector->mMap == 8)
		unk20C = 0;
	unkE8    = 0;
	TAmiHit* amiHit = new TAmiHit("アミノコ当り判定");
	amiHit->mParent = this;
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(amiHit);
	amiHit->initHitActor(0x10000021, 1, 0x80000000, 120.0f, 240.0f, 120.0f,
	                     240.0f);
	amiHit->offHitFlag(HIT_FLAG_NO_COLLISION);
	mAmiHit = amiHit;
}

void TAmiNoko::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor = mMActorKeeper->createMActor("aminoko_model1.bmd", 3);
}

void TAmiNoko::reset() { TWalkerEnemy::reset(); }

void TAmiNoko::behaveToWater(THitActor*)
{
	if (mSpine->getCurrentNerve() != &TNerveAmiNokoFreeze::theNerve()) {
		mSpine->pushNerve(&TNerveAmiNokoFreeze::theNerve());
		mSprayedByWaterCooldown = 0;
	}
}

void TAmiNoko::attackToMario()
{
	if (mSpine->getCurrentNerve() == &TNerveSmallEnemyChange::theNerve())
		return;
	if (unk194 == nullptr)
		return;

	// not a bool: the original tests it with cmpwi, not the bool clrlwi trick
	int canAttack = 1;
	switch (unk198) {
	case 0:
		if (SMS_GetMarioRfPlane() != nullptr
		    && gpMarioPos->y < mPosition.y) {
			canAttack = 0;
		}
		break;
	case 1:
		if (SMS_GetMarioGrPlane() != nullptr
		    && 5.0f + gpMarioPos->y > mPosition.y) {
			canAttack = 0;
		}
		break;
	case 2: {
		const TBGCheckData* plane = SMS_GetMarioWlPlane();
		if (plane != nullptr
		    && plane->mNormal.dot(unk194->mNormal) < 0.0f) {
			canAttack = 0;
		}
		break;
	}
	default:
		break;
	}

	if (!canAttack)
		return;
	if (!SMS_SendMessageToMario(this, 9))
		return;
	if (mSpine->getCurrentNerve() == &TNerveAmiNokoFreeze::theNerve())
		return;

	mSpine->pushNerve(&TNerveAmiNokoFreeze::theNerve());
}
void TAmiNoko::setWalkAnm()
{
	if (unk20C)
		setBckAnm(5);
	else
		setBckAnm(5);
}

bool TAmiNoko::isHitValid(u32 cue)
{
	if (cue == 0xC || cue == 1) {
		// the y component is 0.0f in the original, which is why the dot
		// product keeps a `fmuls` for the 0.0f * unk19C.y term
		JGeometry::TVec3<f32> d(mPosition.x - gpMarioPos->x, 0.0f,
		                        mPosition.z - gpMarioPos->z);

		// dead call in the original: the result is never read.
		matan(unk19C.z, unk19C.x);

		if (d.dot(unk19C) > 0.0f || cue == 1)
			mSpine->pushNerve(&TNerveAmiNokoDie::theNerve());
	}

	if (cue == 0xB)
		return true;
	return false;
}


void TAmiNoko::calcDirection()
{
	// TODO(nonmatching) 78.6%. The inv_sqrt part is solved (see the
	// orig_inv_sqrt wrapper at the top of the file).
	// What is left is almost entirely the stack
	// frame: ours is 0x108, the original 0x1C0. The used locals line up
	// (dir at 0xE8 vs 0x1A4, the TBGWallCheckRecord at 0xAC vs 0x174) so the
	// difference is 0xB8 bytes of frame MWCC reserved and never touched -
	// the per-unused-class-local quirk, ~15 TVec3 worth. Padding it with a
	// dummy array is a fakematch, so it stays. The rest is register/schedule
	// noise in the six clamp blocks (MWCC negates `t <= b` and spends a cror
	// instead of a ble - same instruction count).
	JGeometry::TVec3<f32> dir = getUnkF4().getPoint();
	dir.x -= mPosition.x;
	dir.y -= mPosition.y;
	dir.z -= mPosition.z;

	// isZero() + setLength(one()) spelled out: the target computes
	// squared() once (MWCC CSEs the copy inside setLength), re-tests it
	// against epsilon(), and only then calls the out-of-line inv_sqrt.
	f32 lsq = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
	if (lsq <= JGeometry::TUtil<f32>::epsilon()) {
		dir.set(1.0f, 0.0f, 0.0f);
	} else if (lsq <= JGeometry::TUtil<f32>::epsilon()) {
		dir.zero();
	} else {
		dir.scale(1.0f * orig_inv_sqrt(lsq), dir);
	}

	TBGWallCheckRecord rec;
	rec.mCenter.set(mPosition.x, mPosition.y, mPosition.z);
	rec.mRadius     = 10.0f;
	rec.mMaxResults = 4;
	rec.mFlags      = 0;
	int wallCount = gpMap->isTouchedWallsAndMoveXZ(&rec);

	const TBGCheckData* best = nullptr;
	int bestIndex            = -1;
	f32 bestDist             = -1.0f;

	for (int i = 0; i < wallCount; i++) {
		f32 d = rec.mResultWalls[i]->mNormal.dot(mPosition)
		        + rec.mResultWalls[i]->mPlaneDistance;
		d = fabsf(d);
		if (bestDist > d || bestDist < 0.0f) {
			bestDist  = d;
			unk198    = 2;
			bestIndex = i;
		}
	}
	// NB: with bestIndex still -1 this reads mResultWalls[-1], which lands on
	// the mFlags word the constructor just wrote to 0 - i.e. nullptr.
	best = rec.mResultWalls[bestIndex];

	const TBGCheckData* plane;
	gpMap->checkGround(mPosition.x, mPosition.y + getHeadHeight(),
	                   mPosition.z, &plane);
	mGroundPlane = plane;

	if (mGroundPlane) {
		f32 d = mGroundPlane->mNormal.dot(mPosition)
		        + mGroundPlane->mPlaneDistance;
		if (d >= 0.0f && (bestDist > d || bestDist < 0.0f)) {
			bestDist = d;
			unk198   = 0;
			best     = mGroundPlane;
		}
	}

	gpMap->checkRoof(mPosition, &plane);
	if (plane) {
		f32 d = plane->mNormal.dot(mPosition) + plane->mPlaneDistance;
		if (d >= 0.0f && (bestDist > d || bestDist < 0.0f)) {
			bestDist = d;
			unk198   = 1;
			best     = plane;
		}
	}
	if (best)
		unk194 = best;

	// mSLMtxRotSpeed is the per-frame nudge the two basis vectors get towards
	// the new normal.
	f32 step = static_cast<TAmiNokoParams*>(getSaveParam())
	               ->mSLMtxRotSpeed.get();

	JGeometry::TVec3<f32> n;
	if (unk194)
		n = unk194->mNormal;
	else
		n.set(0.0f, 1.0f, 0.0f);

	if (n.dot(unk19C) <= -1.0f)
		unk19C = n;

	PSVECNormalize(&unk19C, &unk19C);

	if (unk19C.x < n.x) {
		f32 t = unk19C.x + step;
		unk19C.x = t <= n.x ? t : n.x;
	} else {
		f32 t = unk19C.x - step;
		unk19C.x = t <= n.x ? n.x : t;
	}
	if (unk19C.y < n.y) {
		f32 t = unk19C.y + step;
		unk19C.y = t <= n.y ? t : n.y;
	} else {
		f32 t = unk19C.y - step;
		unk19C.y = t <= n.y ? n.y : t;
	}
	if (unk19C.z < n.z) {
		f32 t = unk19C.z + step;
		unk19C.z = t <= n.z ? t : n.z;
	} else {
		f32 t = unk19C.z - step;
		unk19C.z = t <= n.z ? n.z : t;
	}
	if (dir.dot(unk1A8) < -0.1f) {
		Mtx mtx;
		PSMTXRotAxisRad(mtx, &n, JGeometry::TUtil<f32>::halfPI());
		PSMTXMultVec(mtx, &dir, &dir);
	}

	if (unk1A8.x < dir.x) {
		f32 t = unk1A8.x + step;
		unk1A8.x = t <= dir.x ? t : dir.x;
	} else {
		f32 t = unk1A8.x - step;
		unk1A8.x = t <= dir.x ? dir.x : t;
	}
	if (unk1A8.y < dir.y) {
		f32 t = unk1A8.y + step;
		unk1A8.y = t <= dir.y ? t : dir.y;
	} else {
		f32 t = unk1A8.y - step;
		unk1A8.y = t <= dir.y ? dir.y : t;
	}
	if (unk1A8.z < dir.z) {
		f32 t = unk1A8.z + step;
		unk1A8.z = t <= dir.z ? t : dir.z;
	} else {
		f32 t = unk1A8.z - step;
		unk1A8.z = t <= dir.z ? dir.z : t;
	}
	PSVECNormalize(&unk1A8, &unk1A8);

	// perpendicular of the (unk19C, unk1A8) plane, back on unk19C
	JGeometry::TVec3<f32> m;
	m.cross(unk19C, unk1A8);
	dir.cross(m, unk19C);
	if (!dir.isZero()) {
		unk1B4 = unk19C;
		unk1C0 = unk1A8;
	}
	if (!dir.isZero())
		unk1A8 = dir;
}

void TAmiNoko::emitEffects()
{
	// NB: the gateCheck is explicit and the *static* startSoundActor is
	// called; MSound::startSoundActor() would gate a second time.
	if (gpMSound->gateCheck(0x20F1))
		MSoundSESystem::MSoundSE::startSoundActor(0x20F1, &mPosition, 0,
		                                          nullptr, 0, 4);

	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x180, getMActor()->getModel()->getAnmMtx(11), 1, this);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x180, getMActor()->getModel()->getAnmMtx(10), 1,
	    (void*)((u8*)this + 0x214));
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x180, getMActor()->getModel()->getAnmMtx(9), 1,
	    (void*)((u8*)this + 0x428));
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x182, getMActor()->getModel()->getAnmMtx(11), 1, this);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x181, getMActor()->getModel()->getAnmMtx(11), 1, this);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x183, getMActor()->getModel()->getAnmMtx(11), 1, this);

	// positive form: the original branches over the body, it does not
	// short-circuit out of a negated condition
	if (mSpine->getCurrentNerve() == &TNerveAmiNokoFreeze::theNerve()
	    && mSpine->getTime() < 46) {
		JPABaseEmitter* emitter
		    = gpMarioParticleManager->emitAndBindToMtxPtr(
		        0x17D, getMActor()->getModel()->getAnmMtx(0), 1, this);
		if (emitter) {
			JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
			emitter->setGlobalScale(scale);
		}

		// the original reads +0xC/+0x1C/+0x2C of node matrix 6 (a 0x10
		// stride) into unk1FC, then emits at &unk1FC
		const f32* src = (const f32*)getMActor()->getModel()->getAnmMtx(6);
		unk1FC.set(src[3], src[7], src[11]);

		emitter = gpMarioParticleManager->emitAndBindToPosPtr(
		    0x17E, &unk1FC, 1, this);
		if (emitter) {
			JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
			emitter->setGlobalScale(scale);
		}
	}
}
void TAmiNoko::calcRootMatrix()
{
	emitEffects();

	if (isBckAnm(0)) {
		MtxPtr m = reinterpret_cast<MtxPtr>(&unk1CC);
		m[0][3] = mPosition.x;
		m[1][3] = mPosition.y;
		m[2][3] = mPosition.z;

		PSMTXCopy(m, getModel()->getBaseTRMtx());
		getModel()->setBaseScale(mScaling);
		return;
	}

	// Column 0 is the normal of the (unk19C, unk1A8) plane. When that pair
	// is degenerate the (unk1B4, unk1C0) backup plane is used instead, and if
	// that one is degenerate too the normal falls back to +X. The two basis
	// vectors written into columns 1/2 follow whichever pair was picked, but
	// the translation is always built from unk19C - quirk of the original.
	MtxPtr mtx = getModel()->getBaseTRMtx();

	JGeometry::TVec3<f32> nrm;
	JGeometry::TVec3<f32> side;
	JGeometry::TVec3<f32> fwd;

	nrm.cross(unk19C, unk1A8);
	if (nrm.isZero()) {
		side = unk1B4;
		fwd  = unk1C0;
		nrm.cross(side, fwd);
		if (nrm.isZero())
			nrm.set(1.0f, 0.0f, 0.0f);
	} else {
		side = unk19C;
		fwd  = unk1A8;
	}

	// TODO(nonmatching): the original's frame is 0x68, ours 0x58 - it has one
	// more 12-byte class local at 0x2c that emits no code (MWCC reserves stack
	// for unused class locals). Probing with an extra unused TVec3 lands all
	// three real slots on the target offsets and takes the score to 92.3%, so
	// the body is right; the padding itself is deliberately not committed.
	// That extra pressure also seems to be what makes the original spill and
	// reload side.x/fwd.x for the third component of the second cross()
	// (2 extra lfs there) and park `this` in r30 instead of r31.
	PSVECNormalize(&nrm, &nrm);

	mtx[0][0] = nrm.x;
	mtx[1][0] = nrm.y;
	mtx[2][0] = nrm.z;
	mtx[0][1] = side.x;
	mtx[1][1] = side.y;
	mtx[2][1] = side.z;
	mtx[0][2] = fwd.x;
	mtx[1][2] = fwd.y;
	mtx[2][2] = fwd.z;
	mtx[0][3] = mPosition.x - 30.0f * unk19C.x;
	mtx[1][3] = mPosition.y - 30.0f * unk19C.y;
	mtx[2][3] = mPosition.z - 30.0f * unk19C.z;

	getModel()->setBaseScale(mScaling);
	PSMTXCopy(mtx, unk1CC);
}

void TAmiNoko::bind()
{
	if (isBckAnm(0)) {
		JGeometry::TVec3<f32> v = mPosition;
		v += mLinearVelocity;
		v += mVelocity;

		mVelocity.y -= getGravityY();
		if (mVelocity.y < mVelocityMinY)
			mVelocity.y = mVelocityMinY;

		if (checkLiveFlag(LIVE_FLAG_UNK1000)) {
			mGroundHeight = gpMap->checkGroundIgnoreWaterSurface(
			    v.x, v.y + mHeadHeight, v.z, &mGroundPlane);
		} else {
			mGroundHeight = gpMap->checkGround(
			    v.x, v.y + mHeadHeight, v.z, &mGroundPlane);
		}

		mGroundHeight += 1.0f;
		if (v.y <= 0.05f + mGroundHeight) {
			if (mGroundPlane->isIllegalData())
				kill();

			offLiveFlag(LIVE_FLAG_AIRBORNE);
			// TODO: the original stores x/y/z in order here; .zero() stores
			// them in reverse and a batched ctor costs two extra lfs.
			mVelocity.zero();
			v.y = mGroundHeight;
		} else {
			onLiveFlag(LIVE_FLAG_AIRBORNE);
		}

		mLinearVelocity = v - mPosition;
	} else {
		TLiveActor::bind();
	}
}


void TAmiNoko::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & 0x2)
		calcDirection();
	TSmallEnemy::perform(cue, graphics);
	mAmiHit->perform(cue, graphics);
}

f32 TAmiNoko::getGravityY() const
{
	if (mSpine->getCurrentNerve() == &TNerveAmiNokoDie::theNerve())
		return 0.0f;

	return mGravity;
}

const char** TAmiNoko::getBasNameTable() const { return amiNoko_bastable; }

// Not marked inline on purpose: MWCC has to see the body at the call sites so
// that both TNerveAmiNokoWalkOnFence and TNerveAmiNokoTurn get the expansion.
// The map lists this as UNUSED (0x1EC) because the original compiler emitted a
// standalone copy it then inlined away at both sites.
inline void TAmiNoko::creepToCurPathNode(f32 maxStep, f32 scale)
{
	JGeometry::TVec3<f32> dir = getUnkF4().getPoint();
	dir.x -= mPosition.x;
	dir.y -= mPosition.y;
	dir.z -= mPosition.z;

	f32 dist2 = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
	if (dist2 <= JGeometry::TUtil<f32>::epsilon())
		return;

	f32 len = orig_sqrt(dist2);
	if (maxStep <= len)
		len = maxStep;

	f32 d = dir.dot(dir);
	if (d <= JGeometry::TUtil<f32>::epsilon()) {
		dir.zero();
	} else {
		dir.scale(1.0f * orig_inv_sqrt(d), dir);
	}
	dir.x *= len;
	dir.y *= len;
	dir.z *= len;
	mLinearVelocity += dir;
}

DEFINE_NERVE(TNerveAmiNokoWalkOnFence, TLiveActor)
{
	TAmiNoko* self = (TAmiNoko*)spine->getBody();

	if (spine->getTime() == 0)
		self->setWalkAnm();

	if (self->isBckAnm(5) || self->isBckAnm(8)) {
		if (self->checkCurAnmEnd(0)) {
			if (self->unk20C)
				self->setBckAnm(4);
			else
				self->setBckAnm(7);
		}
	}

	JGeometry::TVec3<f32> dir = self->getUnkF4().getPoint();
	dir.x -= self->mPosition.x;
	dir.y -= self->mPosition.y;
	dir.z -= self->mPosition.z;

	f32 dist = orig_sqrt(dir.z * dir.z
	                      + (dir.x * dir.x + dir.y * dir.y));
	if (dist < 1.5f) {
		if (self->checkCurAnmEnd(0)) {
			if (self->isBckAnm(3) || self->isBckAnm(6)) {
				self->goToRandomNextGraphNode();
				self->mSpine->pushAfterCurrent(&TNerveAmiNokoTurn::theNerve());
				return TRUE;
			}
			if (self->isBckAnm(4)) {
				self->setBckAnm(3);
			} else if (self->isBckAnm(7)) {
				self->setBckAnm(6);
			}
		}
	}

	self->creepToCurPathNode(3.0f, 1.0f);

	return FALSE;
}

DEFINE_NERVE(TNerveAmiNokoTurn, TLiveActor)
{
	TAmiNoko* self = (TAmiNoko*)spine->getBody();

	if (spine->getTime() == 0) {
		if (self->unk20C)
			self->setBckAnm(0xB);
		else
			self->setBckAnm(0xE);
	}

	if (self->isBckAnm(0xB) || self->isBckAnm(0xE)) {
		if (self->checkCurAnmEnd(0)) {
			if (self->unk20C)
				self->setBckAnm(0xA);
			else
				self->setBckAnm(0xD);
		}
	}

	JGeometry::TVec3<f32> dir = self->getUnkF4().getPoint();
	dir.x -= self->mPosition.x;
	dir.y -= self->mPosition.y;
	dir.z -= self->mPosition.z;
	if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f)
		dir.x = 1.0f;
	PSVECNormalize(&dir, &dir);

	if (dir.dot(self->unk1A8) > 0.8f && self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(9) || self->isBckAnm(0xC)) {
			if (self->unk20C)
				self->setBckAnm(0xF);
			else
				self->setBckAnm(0xF);
		} else if (self->isBckAnm(0xF)) {
			self->mSpine->pushAfterCurrent(&TNerveAmiNokoWalkOnFence::theNerve());
			return TRUE;
		} else if (self->unk20C) {
			self->setBckAnm(9);
		} else {
			self->setBckAnm(0xC);
		}
	}

	self->creepToCurPathNode(0.0f, 1.0f);

	return FALSE;
}

// UNUSED in the original: the class is never instantiated, so the map only
// records the three symbols below. The .bss singleton theNerve() declares is
// not - it is what puts the .sdata2 global-object table of __sinit_amiNoko
// _cpp at the right offsets.
// TODO: reconstructed, no ground truth (the original body was dead-stripped).
// TNerveAmiNokoAttack is dead code in the original: nothing in the TU ever
// references the class, so the map lists execute/theNerve/__dt__ as UNUSED.
// What is NOT dead is the 12-byte .bss singleton theNerve() declares - it is
// the fifth one, and it is what puts the global-object table walked by
// __sinit_amiNoko_cpp at the right offsets (without it that function sits at
// 99.9% with every addi r5, r31, 0x3c off by 0xC).
//
// TODO: the body itself is a reconstruction - the original was dead-stripped
// before it reached the map, so there is no ground truth. It compiles to
// 0x68 bytes against the 0x6C the map records, i.e. one instruction short.
// Twelve natural shapes were measured (ternary index, early return, shared
// virtual call, three-way isBckAnm, else-arm, flat conditions, ...): they
// land on 0x58 / 0x68 / 0x70 / 0x74 / 0x94 and never on 0x6C. Closing the gap
// would mean inserting an instruction with no reason to exist, so it stays.
DEFINE_NERVE(TNerveAmiNokoAttack, TLiveActor)
{
	TAmiNoko* self = (TAmiNoko*)spine->getBody();

	if (spine->getTime() == 0) {
		if (self->unk20C)
			self->setBckAnm(0x11);
		else
			self->setBckAnm(0x12);
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAmiNokoDie, TLiveActor)
{
	TAmiNoko* self = (TAmiNoko*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(1);
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->mAmiHit->onHitFlag(HIT_FLAG_NO_COLLISION);
	}

	if (self->checkCurAnmEnd(0) && self->isBckAnm(1)) {
		JGeometry::TVec3<f32> dir = self->mPosition - *gpMarioPos;
		if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f)
			dir.x = 1.0f;

		MtxPtr mtx = self->mMActor->getModel()->getBaseTRMtx();
		JGeometry::TVec3<f32> up;
		up.x = mtx[0][1];
		up.y = mtx[1][1];
		up.z = mtx[2][1];
		MsVECNormalize(&up, &up);
		up.x *= 20.0f;
		up.y *= 20.0f;
		up.z *= 20.0f;
		self->mPosition.y += 10.0f;
		self->mVelocity = up;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->setBckAnm(0);
	}

	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x174, self->mMActor->getModel()->getBaseTRMtx(), 1, self);

	if (self->isBckAnm(0) && spine->getTime() > 30) {
		JGeometry::TVec3<f32> toMario = self->mPosition - *gpMarioPos;
		bool hitWall = gpMap->isTouchedOneWallAndMoveXZ(
		    &self->mPosition.x, self->mPosition.y, &self->mPosition.z,
		    3.0f * self->mBodyRadius);
		if (hitWall) {
			self->mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
			JPABaseEmitter* emitter = gpMarioParticleManager->emitWithRotate(
			    0xE2, &self->mPosition, 0,
			    (s16)(182.04445f * self->mRotation.y), 0, 0, 0);
			if (emitter) {
				emitter->mGlobalDynamicsScale.set(self->mScaling);
				emitter->mGlobalParticleScale.set(self->mScaling);
			}
			emitter = gpMarioParticleManager->emitWithRotate(
			    0xE3, &self->mPosition, 0,
			    (s16)(182.04445f * self->mRotation.y), 0, 0, 0);
			if (emitter)
				SMSSetEmitterPolColor(emitter, 6);
		}
		if (!hitWall) {
			if (self->isAirborne()) {
				// the one call site the ROM expands inline rather than calling
				f32 dist = JGeometry::TUtil<f32>::sqrt(
				    toMario.z * toMario.z
				    + (toMario.x * toMario.x + toMario.y * toMario.y));
				if (dist <= 10000.0f)
					return FALSE;
			}
		}

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
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAmiNokoFreeze, TLiveActor)
{
	TAmiNoko* self = (TAmiNoko*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(2);
		J3DModel* model = self->getMActor()->getModel();
		JPABaseEmitter* emitter
		    = gpMarioParticleManager->emitAndBindToMtxPtr(
		        0xCA, model->getAnmMtx(0), 0, nullptr);
		if (emitter) {
			JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
			emitter->setGlobalScale(scale);
		}
	}

	if (self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(2)) {
			self->setBckAnm(0xF);
		} else {
			TSmallEnemyParams* params
			    = (TSmallEnemyParams*)self->getSaveParam();
			if (spine->getTime() > params->mSLFreezeWait.value) {
				return TRUE;
			}
		}
	}
	return FALSE;
}
