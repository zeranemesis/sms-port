#include <Enemy/TabePuku.hpp>

// rogue include: the original TU opens .rodata with the dummy string pair and
// the four MActorMtxCalcType names from M3DUtil/InfectiousStrings.hpp; without
// it every string offset in this object is shifted.
#include <System/DummyStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template statics,
// which is what marioEU.dol registers from __sinit_<TU>_cpp (see the same
// block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

#include <MarioUtil/MathUtil.hpp>
#include <Strategic/Spine.hpp>
#include <Enemy/Graph.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionData.hpp>
#include <MSound/MSound.hpp>
#include <M3DUtil/MActor.hpp>
#include <Map/MapData.hpp>
#include <Player/MarioAccess.hpp>
#include <System/Particles.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>

// This TU is -inline deferred: the definition order below is the reverse of
// the .text layout in marioEU.MAP.

static const char* tabepuku_bastable[] = {
	"/scene/tabepuku/bas/pukupuku_chase.bas",
	"/scene/tabepuku/bas/pukupuku_search.bas",
	"/scene/tabepuku/bas/pukupuku_swim.bas",
};

// ============================================================== params

class TTabePukuParams : public TSmallEnemyParams {
public:
	TTabePukuParams(const char* name)
	    : TSmallEnemyParams(name)
	    , PARAM_INIT(mMarchSpeed, 0.15f)
	    , PARAM_INIT(mAttackSpeed, 0.22f)
	    , PARAM_INIT(mDiveSpeed, 0.4f)
	    , PARAM_INIT(mWaterFric, 0.95f)
	    , PARAM_INIT(mTurnSlepRate, 0.05f)
	    , PARAM_INIT(mApartHeight, 500.0f)
	    , PARAM_INIT(mCorrectY, -40.0f)
	    , PARAM_INIT(mCorrectZ, 150.0f)
	    , PARAM_INIT(mTerritoryRange, 1000.0f)
	    , PARAM_INIT(mDragLength, 2500.0f)
	{
	}

	/* 0x2D4 */ TParamRT<f32> mMarchSpeed;
	/* 0x2E8 */ TParamRT<f32> mAttackSpeed;
	/* 0x2FC */ TParamRT<f32> mDiveSpeed;
	/* 0x310 */ TParamRT<f32> mWaterFric;
	/* 0x324 */ TParamRT<f32> mTurnSlepRate;
	/* 0x338 */ TParamRT<f32> mApartHeight;
	/* 0x34C */ TParamRT<f32> mCorrectY;
	/* 0x360 */ TParamRT<f32> mCorrectZ;
	/* 0x374 */ TParamRT<f32> mTerritoryRange;
	/* 0x388 */ TParamRT<f32> mDragLength;
};

// ============================================================== nerves

// ============================================================== instance
//
// The two helpers below are UNUSED in marioEU.MAP (setMomentumFromQuat is
// 0x1d4 bytes there) - they were inlined into every call site. They are
// defined before the nerves so MWCC can see the bodies there too.

// applies the current orientation's forward vector to the velocity
void TTabePuku::setMomentumFromQuat()
{
	f32 fx = 2.0f * (mQuat.x * mQuat.z + mQuat.w * mQuat.y);
	f32 fy = 2.0f * (mQuat.y * mQuat.z - mQuat.w * mQuat.x);
	f32 fz = 1.0f - 2.0f * (mQuat.x * mQuat.x + mQuat.y * mQuat.y);

	fx *= mMarchSpeed;
	fy *= mMarchSpeed;
	fz *= mMarchSpeed;

	f32 fric = getParams()->mWaterFric.get();

	JGeometry::TVec3<f32> vel = mVelocity;
	vel.x = vel.x * fric + fx;
	vel.y = vel.y * fric + fy;
	vel.z = vel.z * fric + fz;
	mVelocity = vel;
}

// points mRotation.y along the current velocity
void TTabePuku::calcYawFromVelocity()
{
	if (mVelocity.z == 0.0f) {
		mRotation.y = 0.0f <= mVelocity.x ? 90.0f : -90.0f;
	} else if (0.0f <= mVelocity.z) {
		mRotation.y = 0.005f * ((f32)matan(mVelocity.x, mVelocity.z) - 180.0f);
	} else {
		mRotation.y = 180.0f
		              - 0.005f
		                    * ((f32)matan(mVelocity.x, -mVelocity.z) - 180.0f);
	}
}

DEFINE_NERVE(TNerveTabePukuGraphWander, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->getTracer()->reset();
		self->goToShortestNextGraphNode();
		self->setBckAnm(2);
		self->mMarchSpeed = self->getParams()->mMarchSpeed.get();
	}

	if (self->isReachedToGoal())
		self->goToRandomNextGraphNode();

	if (self->isFindMario(1.0f)) {
		spine->pushAfterCurrent(&TNerveTabePukuFound::theNerve());
		return TRUE;
	}

	JGeometry::TVec3<f32> pos = self->getUnk104().getPoint() - self->mPosition;
	pos.add(JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f));
	self->swimTo(pos);
	return FALSE;
}

DEFINE_NERVE(TNerveTabePukuFound, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(1);
		self->mMarchSpeed = 0.0f;
	}

	self->setMomentumFromQuat();
	self->calcYawFromVelocity();

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveTabePukuAttack::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveTabePukuRecoverGraph, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->getTracer()->reset();
		self->getTracer()->reset2();
		self->goToShortestNextGraphNode();
		self->mMarchSpeed = self->getParams()->mMarchSpeed.get();
	}

	if (self->isReachedToGoal()) {
		spine->pushAfterCurrent(&TNerveTabePukuGraphWander::theNerve());
		return TRUE;
	}

	JGeometry::TVec3<f32> pos = self->getUnk104().getPoint() - self->mPosition;

	if (self->checkLiveFlag(LIVE_FLAG_UNK1000000) && self->mTouchedWall == 0) {
		pos.add(JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f));
	} else {
		pos.add(JGeometry::TVec3<f32>(0.0f, 10000.0f, 0.0f));
	}
	self->swimTo(pos);
	return FALSE;
}

DEFINE_NERVE(TNerveTabePukuAttack, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	if (spine->getTime() == 0) {
		THitActor* mario = (THitActor*)gpMarioAddress;
		JGeometry::TVec3<f32> pos(0.0f, 0.0f, 0.0f);
		if (mario) {
			pos.x = mario->mPosition.x;
			pos.y = mario->mPosition.y;
			pos.z = mario->mPosition.z;
		}
		TPathNode node;
		node.unk0 = mario;
		node.unk4 = pos;
		self->setGoalPath(node);
		self->mMarchSpeed = self->getParams()->mAttackSpeed.get();
	}

	bool away = false;

	if (fabsf(gpMarioPos->y - self->mPosition.y)
	    > self->getParams()->mApartHeight.get()) {
		away = true;
	} else {
		JGeometry::TVec3<f32> v = self->getUnk104().getPoint() - self->mPosition;
		if (JGeometry::TUtil<f32>::sqrt(v.dot(v))
		    > self->getParams()->mTerritoryRange.get()) {
			away = true;
		} else {
			JGeometry::TVec3<f32> nearest
			    = self->getTracer()->getGraph()->getNearestPosOnGraphLink(
			        self->mPosition);
			nearest.x -= self->mPosition.x;
			nearest.y -= self->mPosition.y;
			nearest.z -= self->mPosition.z;
			f32 range = self->getParams()->mTerritoryRange.get();
			away = range * range <= nearest.dot(nearest) ? true : false;
		}
	}

	if (away || self->mTouchedWall != 0) {
		spine->pushAfterCurrent(&TNerveTabePukuRecoverGraph::theNerve());
		return TRUE;
	}

	JGeometry::TVec3<f32> pos = self->getUnk104().getPoint() - self->mPosition;
	pos.add(JGeometry::TVec3<f32>(0.0f, 150.0f, 0.0f));
	self->swimTo(pos);
	return FALSE;
}

DEFINE_NERVE(TNerveTabePukuBite, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	self->setBckAnm(2);

	if (gpMSound->gateCheck(MSD_SE_EN_TOBIPUKU_BITE))
		gpMSound->startSoundActor(MSD_SE_EN_TOBIPUKU_BITE, &self->mPosition, 0,
		                          0, 0, 4);

	spine->pushAfterCurrent(&TNerveTabePukuDive::theNerve());
	return TRUE;
}

DEFINE_NERVE(TNerveTabePukuDive, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mDiveStartY = self->mPosition.y;
		self->setBckAnm(2);
		self->getMActor()->getFrameCtrl(0)->setRate(
		    2.0f * SMSGetAnmFrameRate());
		self->mMarchSpeed = self->getParams()->mDiveSpeed.get();
	}

	JGeometry::TVec3<f32> target(0.0f, self->mBodyScale - self->mPosition.y,
	                             0.0f);
	self->swimTo(target);

	bool done = false;
	if (self->mPosition.y - self->mDiveStartY
	        < -self->getParams()->mApartHeight.get()
	    && self->mPosition.y - self->mBodyScale < 200.0f
	    && !self->checkLiveFlag(LIVE_FLAG_UNK1000000)) {
		done = true;
	}

	if (done) {
		spine->pushAfterCurrent(&TNerveTabePukuDrag::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveTabePukuDrag, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mDragVec.set(0.0f, 0.0f, 1.0f);

		f32 rnd  = (f32)(s16)rand();
		f32 axis = 0.5f * (1.0f / 4096.0f) * rnd * 6.14159274f;
		f32 s    = sinf(axis);
		f32 c    = cosf(axis);

		// TODO: the target rotates mDragVec with a hand-written rotation
		// matrix whose off-axis terms are literal 0.0f multiplies (they
		// survive as -0.0f fma chains, and the result goes through the
		// out-of-line TVec3<f32>::set<f,f,f>). The plain form below is a
		// guess and does not match.
		self->mDragVec.set(c * self->mDragVec.x - s * self->mDragVec.z,
		                   c * self->mDragVec.y,
		                   s * self->mDragVec.x + c * self->mDragVec.z);

		TPathNode node;
		node.unk0 = nullptr;
		node.unk4 = self->mPosition;
		self->setGoalPath(node);
		self->mMarchSpeed = self->getParams()->mDiveSpeed.get();
	}

	self->swimTo(self->mDragVec);

	bool hitMario = false;
	if (self->mTouchedWall == 0 && self->checkLiveFlag(LIVE_FLAG_UNK1000000)) {
		JGeometry::TVec3<f32> to;
		if (self->getUnkF4().unk0)
			to = self->getUnkF4().unk0->mPosition;
		else
			to = self->getUnkF4().unk4;
		to.sub(self->mPosition);

		if (JGeometry::TUtil<f32>::sqrt(to.dot(to))
		    < self->getParams()->mDragLength.get()) {
			hitMario = true;
		}
	}

	if (hitMario) {
		SMS_SendMessageToMario(self, 8);
		self->mHeldObject = nullptr;
		spine->pushAfterCurrent(&TNerveTabePukuRecoverGraph::theNerve());
		return TRUE;
	}
	return FALSE;
}

// The ROM calls TRotation3<...>::setQuat out of line at both call sites in
// this TU; one extra inline level is what it takes to make MWCC refuse the
// expansion (the callee is 27 instructions, well past the fold threshold).
typedef JGeometry::TRotation3<JGeometry::TMatrix34<JGeometry::SMatrix34C<f32> > >
    TTabePukuRot3;

static inline void tabe_setQuat(TTabePukuRot3& mtx,
                                const JGeometry::TQuat4<f32>& quat)
{
	mtx.setQuat(quat);
}

// ============================================================== manager

void TTabePukuManager::createModelData()
{
	// TODO: 0x2110 flags not fully decoded (J3DMLF_* bitfield)
	static const TModelDataLoadEntry entry[] = {
		{ "tabepuku.bmd", 0x2110, 0 },
		{ 0, 0, 0 },
	};
	createModelDataArray(entry);
}

void TTabePukuManager::load(JSUMemoryInputStream& stream)
{
	TTabePukuParams* params = new TTabePukuParams("/enemy/tabepuku.prm");
	if (params)
		params->load(params->mPrmPath);

	unk38 = params;
	TSmallEnemyManager::load(stream);
}

TTabePukuManager::TTabePukuManager(const char* name)
    : TSmallEnemyManager(name)
{
}

// ============================================================== instance

void TTabePuku::swimTo(const JGeometry::TVec3<f32>& target)
{
	// the target copies the argument (as an int load/store triple) before it
	// computes the squared length of the *argument*, so the copy is a separate
	// statement that has to come first
	JGeometry::TVec3<f32> dir = target;

	f32 d = target.squared();

	if (JGeometry::TUtil<f32>::epsilonEquals(0.0f, d)) {
		setMomentumFromQuat();
		calcYawFromVelocity();
		return;
	}

	if (dir.squared() <= JGeometry::TUtil<f32>::epsilon()) {
		dir.zero();
	} else {
		dir.scale(1.0f * JGeometry::TUtil<f32>::inv_sqrt(dir.squared()), dir);
	}

	JGeometry::TVec4<f32> q;

	if (JGeometry::TUtil<f32>::epsilonEquals(-1.0f, dir.z)) {
		f32 h = 0.5f * 0.4f;
		q.x   = 0.0f;
		q.y   = sinf(h);
		q.z   = 0.0f;
		q.w   = cosf(h);
	} else {
		f32 ax = dir.x;
		f32 ay = -dir.y;
		f32 az = 0.0f;
		f32 len = az * az + (ay * ay + ax * ax);
		len     = 1.0f < len ? JGeometry::TUtil<f32>::sqrt(len) : len;

		if (!(len < JGeometry::TUtil<f32>::epsilon())) {
			q.x = 0.0f;
			q.y = 0.0f;
			q.z = 0.0f;
			q.w = 1.0f;
		} else {
			f32 angle = atan2f(1.0f * dir.z + 0.0f * dir.x + 0.0f * dir.y, len);
			f32 h     = 0.5f * angle;
			f32 s     = sinf(h) / len;
			q.x       = ay * s;
			q.y       = ax * s;
			q.z       = az * s;
			q.w       = cosf(h);
		}
	}

	// spherical interpolation of the current orientation towards q
	f32 t = getParams()->mTurnSlepRate.get();

	JGeometry::TVec4<f32> a = mQuat;
	f32 sa                = a.dot(a);
	if (sa < JGeometry::TUtil<f32>::epsilon()) {
		a.x = 0.0f;
		a.y = 0.0f;
		a.z = 0.0f;
		a.w = 0.0f;
	} else {
		a.scale(1.0f * JGeometry::TUtil<f32>::inv_sqrt(sa), a);
	}

	JGeometry::TVec4<f32> b = q;
	f32 sb                = b.dot(b);
	if (sb < JGeometry::TUtil<f32>::epsilon()) {
		b.x = 0.0f;
		b.y = 0.0f;
		b.z = 0.0f;
		b.w = 0.0f;
	} else {
		b.scale(1.0f * JGeometry::TUtil<f32>::inv_sqrt(sb), b);
	}

	f32 cosang = a.x * b.x + a.y * b.y;
	cosang     = a.z * b.z + cosang;
	cosang     = a.w * b.w + cosang;

	bool flip = false;
	if (cosang < 0.0f) {
		cosang = -cosang;
		flip    = true;
	}

	f32 wa, wb;
	if (1.0f - cosang < JGeometry::TUtil<f32>::epsilon()) {
		wa = 1.0f - t;
		wb = t;
	} else {
		f32 sd   = sinf(cosang);
		wa       = sinf((1.0f - t) * cosang) / sd;
		wb       = sinf(t * cosang) / sd;
	}

	if (flip)
		wb = -wb;

	mQuat.x = a.x * wa + b.x * wb;
	mQuat.y = a.y * wa + b.y * wb;
	mQuat.z = a.z * wa + b.z * wb;
	mQuat.w = a.w * wa + b.w * wb;

	// renormalise
	d = mQuat.x * mQuat.x;
	d = mQuat.y * mQuat.y + d;
	d = mQuat.z * mQuat.z + d;
	d = mQuat.w * mQuat.w + d;

	if (d < JGeometry::TUtil<f32>::epsilon()) {
		mQuat.x = 0.0f;
		mQuat.y = 0.0f;
		mQuat.z = 0.0f;
		mQuat.w = 0.0f;
	} else {
		f32 s = 1.0f * JGeometry::TUtil<f32>::inv_sqrt(d);
		mQuat.x = mQuat.x * s;
		mQuat.y = mQuat.y * s;
		mQuat.z = mQuat.z * s;
		mQuat.w = mQuat.w * s;
	}

	// push the orientation forward
	setMomentumFromQuat();
	calcYawFromVelocity();
}

bool TTabePuku::doKeepDistance()
{
	bool res = true;

	if (mSpine->getLatestNerve() != &TNerveTabePukuAttack::theNerve()) {
		const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();

		if (nerve != &TNerveTabePukuBite::theNerve()
		    && nerve != &TNerveTabePukuDive::theNerve()
		    && nerve != &TNerveTabePukuDrag::theNerve()) {
			res = false;
		}
	}
	return !res;
}

bool TTabePuku::isFindMario(float distance) { return isFindMarioFromParam(distance); }

void TTabePuku::forceKill() { }

void TTabePuku::behaveToWater(THitActor*) { }

void TTabePuku::attackToMario()
{
	const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();

	if (nerve == &TNerveTabePukuBite::theNerve()
	    || nerve == &TNerveTabePukuDive::theNerve()
	    || nerve == &TNerveTabePukuDrag::theNerve()) {
		return;
	}

	nerve = mSpine->getLatestNerve();

	if (nerve == &TNerveTabePukuGraphWander::theNerve()
	    || nerve == &TNerveTabePukuRecoverGraph::theNerve()) {
		return;
	}

	if (SMS_SendMessageToMario(this, HIT_MESSAGE_TAKE)) {
		mHeldObject       = (TTakeActor*)SMS_GetMarioHitActor();
		mSpine->reset();
		mSpine->setNext(&TNerveTabePukuBite::theNerve());
	}
}

const char** TTabePuku::getBasNameTable() const
{
	return (const char**)tabepuku_bastable;
}

MtxPtr TTabePuku::getTakingMtx()
{
	mTakingMtx.setQuat(mQuat);

	f32 cz = getParams()->mCorrectZ.get();

	f32 tx = mTakingMtx.at(0, 2) * cz + mPosition.x;
	f32 ty = mTakingMtx.at(1, 2) * cz + mPosition.y;
	f32 tz = mTakingMtx.at(2, 2) * cz + mPosition.z;

	f32 cy = getParams()->mCorrectY.get();

	mTakingMtx.ref(0, 3) = mTakingMtx.at(0, 1) * cy + tx;
	mTakingMtx.ref(1, 3) = mTakingMtx.at(1, 1) * cy + ty;
	mTakingMtx.ref(2, 3) = mTakingMtx.at(2, 1) * cy + tz;

	return (MtxPtr)&mTakingMtx;
}

BOOL TTabePuku::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_TRAMPLE:
	case HIT_MESSAGE_HIP_DROP:
		return FALSE;
	default:
		return TSmallEnemy::receiveMessage(sender, message);
	}
}

void TTabePuku::calcRootMatrix()
{
	// the explicit (cond) ? true : false is what makes MWCC materialise the
	// test into a register (li 1 / b / li 0 / cmpwi) as the target does
	bool held = mHolder ? true : false;
	if (held) {
		TSpineEnemy::calcRootMatrix();
		return;
	}

	TTabePukuRot3 mtx;
	tabe_setQuat(mtx, mQuat);
	mtx.ref(0, 3) = mPosition.x;
	mtx.ref(1, 3) = mPosition.y;
	mtx.ref(2, 3) = mPosition.z;

	// the target calls getModel() three times rather than caching the pointer
	getModel()->setBaseScale(mScaling);
	PSMTXCopy(mtx, getModel()->getBaseTRMtx());

	JGeometry::TVec3<f32> scale(1.0f, 1.0f, 1.0f);
	JPABaseEmitter* emitter = SMS_EasyEmitParticle(
	    PARTICLE_MS_PUKU_AWA, getModel()->getAnmMtx(mMouthIndex), this, scale);

	if (emitter) {
		f32 h = -mPosition.y / 100.0f;
		if (h <= 0.0f)
			h = 0.0f;
		// the clamp is on the s32 and the s16 narrowing happens on the store,
		// which is why the target emits extsh *after* the cmpwi
		s32 life = (s32)h * 20 + 2;
		if (200 < life)
			life = 200;
		emitter->mBaseLifetime = life;

		if (mSpine->getLatestNerve() == &TNerveTabePukuAttack::theNerve())
			mTakingMtx.ref(0, 2) = 0.1f;
	}
}

void TTabePuku::bind()
{
	TSmallEnemyParams* p = mHit->mOwner->getSaveParam2();

	mHit->mAttackRadius = (f32)p->mSLAttackRadius.get();
	mHit->mAttackHeight = (f32)p->mSLAttackHeight.get();
	mHit->mDamageRadius = (f32)p->mSLDamageRadius.get();
	mHit->mDamageHeight = (f32)p->mSLDamageHeight.get();
	mHit->calcEntryRadius();

	mHit->updateTerrainCollsion();
	mHit->bind();

	mLinearVelocity = mHit->mVel;
	mTouchedWall    = mHit->mTouchedWall;

	if (mHit->mAirborne)
		onLiveFlag(LIVE_FLAG_AIRBORNE);
	else
		offLiveFlag(LIVE_FLAG_AIRBORNE);

	mGroundPlane = mHit->mGroundPlane;
	mGroundHeight = mHit->mGroundY;
}

void TTabePuku::control()
{
	TLiveActor::control();

	int playerType = ACTOR_TYPE_PLAYER | 1;
	THitActor** p = mHit->mCollisions;
	THitActor** e = p + mHit->mColCount;

	for (; p != e; p++) {
		THitActor* col = *p;

		if (col->mActorType == playerType)
			mHit->mOwner->attackToMario();
	}

	const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();

	if (nerve == &TNerveTabePukuBite::theNerve()
	    || nerve == &TNerveTabePukuDive::theNerve()
	    || nerve == &TNerveTabePukuDrag::theNerve()) {
		gpMSound->startSoundActor(MSD_SE_EN_TOBIPUKU_CHEW, &mPosition, 0, 0,
		                          0, 4);
	}
}

void TTabePuku::perform(u32 cue, JDrama::TGraphics* graphics)
{
	mHit->perform(cue, graphics);
	TSmallEnemy::perform(cue, graphics);
}

void TTabePuku::reset() { mScaledBodyRadius = 130.0f; }

void TTabePuku::init(TLiveManager* manager)
{
	mManager = manager;
	manager->manageActor(this);
	setMActorAndKeeper();

	mSpine->initWith(&TNerveTabePukuGraphWander::theNerve());

	initHitActor(0x1000, 0x35, 0, 0.0f, 0.0f, 0.0f, 0.0f);
	onHitFlag(HIT_FLAG_NO_COLLISION);

	mHit = new TTPHitActor("スズプク用当たり");
	if (mHit)
		mHit->mOwner = this;

	mHit->init();

	mHit->mPosition = mPosition;

	JGeometry::TQuat4<f32> quat = SMS_Eular2Quat(mRotation);
	mQuat.x = quat.x;
	mQuat.y = quat.y;
	mQuat.z = quat.z;
	mQuat.w = quat.w;

	mMouthIndex
	    = getModel()->getModelData()->getJointName()->getIndex("jnt_mouth_up");

	initAnmSound();
}

TTabePuku::TTabePuku(const char* name)
    : TSmallEnemy(name)
{
	onLiveFlag(LIVE_FLAG_UNK1000);
}

// ============================================================== hit actor

void TTPHitActor::bind()
{
	const TBGCheckData* checkData;

	JGeometry::TVec3<f32> pos = mPosition;
	pos.add(mVel);
	// the target copies the owner's mVelocity into a local first, which makes
	// MWCC emit the int load/store pair instead of three lfs
	JGeometry::TVec3<f32> vel = mOwner->mVelocity;
	pos.add(vel);
	pos.add(mOwner->mLinearVelocity);

	mGroundY = gpMap->checkGroundIgnoreWaterSurface(
	    pos.x, pos.y + mMouthYOffset, pos.z, &mGroundPlane);
	mGroundY += 1.0f;

	if (pos.y < mGroundY + 0.05f) {
		mAirborne = 0;

		f32 d = 1.0f - (mGroundPlane->mNormal.dot(pos)
		                - mGroundPlane->mNormal.dot(
		                      JGeometry::TVec3<f32>(pos.x, mGroundY, pos.z)));
		if (0.0f < d) {
			pos.x += mGroundPlane->mNormal.x * d;
			pos.y += mGroundPlane->mNormal.y * d;
			pos.z += mGroundPlane->mNormal.z * d;
		}
		pos.y = mGroundY;
	} else {
		mAirborne = 1;
	}

	if (0.0f >= pos.y + mMouthYOffset)
		pos.y = -mMouthYOffset;

	TBGWallCheckRecord rec;
	rec.set(pos.x, pos.y, pos.z, mMouthRadius, 1, 0);

	mTouchedWall = gpMap->isTouchedWallsAndMoveXZ(&rec) ? 1 : 0;

	pos.x = rec.mCenter.x;
	pos.z = rec.mCenter.z;

	JGeometry::TVec3<f32> delta = pos;
	delta.sub(mPosition);

	mVel         = delta;
	mPosition    = pos;
}

void TTPHitActor::updateTerrainCollsion()
{
	mMouthYOffset = mAttackHeight;
	mMouthRadius  = mAttackRadius;

	// rotate the owner's swim quaternion into a direction and turn it into a
	// step
	f32 qx = mOwner->mQuat.x;
	f32 qy = mOwner->mQuat.y;
	f32 qz = mOwner->mQuat.z;
	f32 qw = mOwner->mQuat.w;

	f32 ax = 2.0f * (qx * qz + qw * qy);
	f32 ay = 2.0f * (qy * qz - qw * qx);
	f32 az = 1.0f - 2.0f * (qx * qx + qy * qy);

	f32 bx = -ay * ax;
	f32 by = -az * ax;
	f32 bz = -ax * ay;
	az     = 0.0f * ax;

	if (mOwner->mHeldObject) {
		f32 h = mOwner->mHeldObject->mDamageHeight;
		mMouthYOffset += h;
		mMouthRadius += mOwner->mHeldObject->mDamageRadius;
	}

	f32 half = 0.5f * mAttackHeight;
	f32 z    = mOwner->mQuat.w * ax + mOwner->mQuat.x + 0.0f * ax;
	f32 x    = mOwner->mQuat.y * ay + mOwner->mQuat.y + -1.0f * az;
	f32 y    = mOwner->mQuat.z * az + mOwner->mQuat.z + 0.0f * ax;

	// TODO: the axis/angle reconstruction above is a placeholder guess; the
	// target builds a full rotation here and this needs a second pass.
	JGeometry::TVec3<f32> step(x, y, z);
	step.x -= mPosition.x;
	step.y -= mPosition.y;
	step.z -= mPosition.z;

	JGeometry::TVec3<f32> newPos;
	newPos.x = step.x;
	newPos.y = step.y;
	newPos.z = step.z;

	mVel.x = newPos.x - mPosition.x;
	mVel.y = newPos.y - mPosition.y;
	mVel.z = newPos.z - mPosition.z;
	mPosition.set(newPos);
}

BOOL TTPHitActor::receiveMessage(THitActor* sender, u32 message)
{
	return mOwner->receiveMessage(sender, message);
}

void TTPHitActor::init()
{
	initHitActor(0x1000, 0x35, 1, 10.0f, 10.0f, 10.0f, 10.0f);

	offHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);

	// TODO: the tail of the target function looks the actor up by name in the
	// JDrama name table and then pushes something into a JGadget::TList<void*>
	// that lives on the stack. The exact shape is not understood yet; the
	// lookup is reproduced so at least the call and the string match.
	JDrama::TNameRefGen::search("えたぶくろ");
}

TTabePukuManager::~TTabePukuManager() { }

TTabePuku::~TTabePuku() { }

TTPHitActor::~TTPHitActor() { }
