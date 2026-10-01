#include <Enemy/Kazekun.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
// rogue include: pulls in the four MActorMtxCalcType names that the
// original .rodata carries after the dummy string pair.
#include <M3DUtil/InfectiousStrings.hpp>
#include <M3DUtil/MActor.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <System/Particles.hpp>
#include <Player/MarioAccess.hpp>
#include <MSound/MSound.hpp>
#include <MarioUtil/MtxUtil.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// ============= out-of-line JGeometry helpers =============
//
// The retail build calls the JGeometry header templates out of line --
// `bl set<f>__Q29JGeometry8TVec3<f>Ffff`,
// `bl set<f>__Q29JGeometry8TVec4<f>Fffff`,
// `bl getQuat__Q29JGeometry64TRotation3<...>::getQuat`,
// `bl slerp__Q29JGeometry9TQuat4<f>FRCQ29JGeometry9TQuat4<f>f` -- while
// MWCC always expands them at the call site, which is what blows
// TNerveKazekunAttack::execute() up from 0x580 to 0x1048 bytes and keeps it
// at 0.0% match.  MWCC ignores `#pragma dont_inline` on a *header* body, so
// these TU-local wrappers exist purely to give the call sites the right
// shape.  Their names deliberately do NOT match the retail symbols (they
// stay "extra" in objdiff until the shared headers are fixed) -- see the
// libs/ JGeometry template out-of-lining bug.
#pragma dont_inline on
static void origVec3Set(JGeometry::TVec3<f32>* v, f32 x, f32 y, f32 z)
{
	v->set(x, y, z);
}
#pragma dont_inline off

#pragma dont_inline on
static void origVec4Set(JGeometry::TVec4<f32>* v, f32 x, f32 y, f32 z, f32 w)
{
	v->set(x, y, z, w);
}
#pragma dont_inline off

#pragma dont_inline on
static void origVec3Scale(JGeometry::TVec3<f32>* dest, f32 f,
                          const JGeometry::TVec3<f32>& src)
{
	dest->scale(f, src);
}
#pragma dont_inline off

#pragma dont_inline on
static f32 origVec3Dot(const JGeometry::TVec3<f32>* a,
                       const JGeometry::TVec3<f32>* b)
{
	return a->dot(*b);
}
#pragma dont_inline off

#pragma dont_inline on
static void origVec4Scale(JGeometry::TVec4<f32>* dest, f32 f,
                          const JGeometry::TVec4<f32>& src)
{
	dest->scale(f, src);
}
#pragma dont_inline off

#pragma dont_inline on
static f32 origVec4Dot(const JGeometry::TVec4<f32>* a,
                       const JGeometry::TVec4<f32>* b)
{
	return a->dot(*b);
}
#pragma dont_inline off

// TPosition3f's default constructor is an out-of-line `bl
// __ct__TMatrix34<...>` in the retail build; MWCC expands the empty body and
// deletes the call.
#pragma dont_inline on
static void origMtxCtor(TPosition3f* mtx)
{
	new (mtx) TPosition3f();
}
#pragma dont_inline off

// TUtil<f32>::inv_sqrt is a weak out-of-line function in the retail build
// (`bl inv_sqrt__Q29JGeometry8TUtil<f>Ff`); MWCC expands the Newton-Raphson
// body at most call sites.
#pragma dont_inline on
static f32 origInvSqrt(f32 mag)
{
	return JGeometry::TUtil<f32>::inv_sqrt(mag);
}
#pragma dont_inline off

// JGeometry::TVec3<f>::setLength(v, length) written out so each of the three
// callees can be routed through the wrappers above, which is the shape the
// retail build has.
static void origVec3SetLength(JGeometry::TVec3<f32>* dest,
                              const JGeometry::TVec3<f32>& v, f32 length)
{
	f32 lsq = origVec3Dot(&v, &v);
	if (lsq <= JGeometry::TUtil<f32>::epsilon()) {
		dest->x = 0.0f;
		dest->y = 0.0f;
		dest->z = 0.0f;
		return;
	}
	origVec3Scale(dest, length * origInvSqrt(lsq), v);
}

// ditto for TVec4<f>, used by TQuat4<f>::normalize().
static void origVec4SetLength(JGeometry::TVec4<f32>* dest,
                              const JGeometry::TVec4<f32>& v, f32 length)
{
	f32 lsq = origVec4Dot(&v, &v);
	if (lsq <= JGeometry::TUtil<f32>::epsilon()) {
		dest->x = 0.0f;
		dest->y = 0.0f;
		dest->z = 0.0f;
		dest->w = 0.0f;
		return;
	}
	origVec4Scale(dest, length * origInvSqrt(lsq), v);
}

#pragma dont_inline on
static void origMtxGetQuat(const TPosition3f& mtx, JGeometry::TQuat4<f32>& q)
{
	mtx.getQuat(q);
}
#pragma dont_inline off

#pragma dont_inline on
static void origQuatSlerp(JGeometry::TQuat4<f32>* q,
                          const JGeometry::TQuat4<f32>& target, f32 t)
{
	q->slerp(target, t);
}
#pragma dont_inline off

#pragma dont_inline on
static f32 origSqrt(f32 mag)
{
	return JGeometry::TUtil<f32>::sqrt(mag);
}
#pragma dont_inline off

// TQuat4<f>::setRotate(axis, angle) with `halfAngle` already halved by the
// caller (the header's setRotate folds a constant angle in half at compile
// time, which is why 0x3c vs 0x7853982f shows up as a literal in the retail
// asm). The three components are stored individually there, so this wrapper
// does not go through TVec3<f>::scale the way the header does.
static void origQuatSetRotate(JGeometry::TQuat4<f32>* q, f32 ax, f32 ay,
                              f32 az, f32 halfAngle)
{
	f32 s = sinf(halfAngle);
	q->x = ax * s;
	q->y = ay * s;
	q->z = az * s;
	q->w = cosf(halfAngle);
}

// TQuat4<f>::mul(other) with the result stored component-wise (the retail
// build inlines set<f>TVec4<f> at these two call sites, unlike the one in
// TNerveKazekunAttack::execute which calls it).
static void origQuatMulInPlace(JGeometry::TQuat4<f32>* q,
                               const JGeometry::TQuat4<f32>& other)
{
	f32 nx = q->w * other.x + q->x * other.w + q->y * other.z
	        - q->z * other.y;
	f32 ny = q->w * other.y + q->y * other.w + q->z * other.x
	        - q->x * other.z;
	f32 nz = q->w * other.z + q->z * other.w + q->x * other.y
	        - q->y * other.x;
	f32 nw = q->w * other.w - q->x * other.x - q->y * other.y
	        - q->z * other.z;
	q->x = nx;
	q->y = ny;
	q->z = nz;
	q->w = nw;
}

// JGeometry::TQuat4<f>::rotate(v, dest) written out because
// libs/JSystem/include/JSystem/JGeometry/JGQuat4.hpp gets the z term's sign
// wrong (`- q.w * -z` where the retail build has `+ q.w * -z`, see the
// "Incollect regalloc" note in that file). Everything else matches.
static void origQuatRotateVec(const JGeometry::TQuat4<f32>* q,
                              const JGeometry::TVec3<f32>& v,
                              JGeometry::TVec3<f32>& dest)
{
	f32 vx = v.x;
	f32 vy = v.y;
	f32 vz = v.z;
	f32 qw = q->w;
	f32 qz = q->z;
	f32 qy = q->y;
	f32 qx = q->x;

	f32 ax = qw * vx + qy * vz - qz * vy;
	f32 ay = qw * vy + qx * vz - qz * vx;
	f32 az = qw * vz + qx * vy - qy * vx;
	f32 aw = -qx * vx - qy * vy - qz * vz;

	f32 rx = ax * qw + ay * -qz - az * -qy + aw * -qx;
	f32 ry = -ax * -qz + ay * qw + az * -qx + aw * -qy;
	f32 rz = ax * -qy - ay * -qx + az * qw + aw * -qz;

	dest.x = rx;
	dest.y = ry;
	dest.z = rz;
}

// TODO: this TU is only partially decompiled. TKazekun::init/
// calcRootMatrix/attackToMario/behaveToWater, TKazekunParams,
// TKazekunManager::load and ::createModelData and doAttackPose all still
// need reconstruction. doAttackPose is blocked: the second half of it builds
// a quaternion from TKazekun::unk1A0's xyz with a 20-term fma expression
// that does not reduce to any of JGeometry's mul/rotate/setRotate formulas
// (see build/GMSP01/asm/Enemy/Kazekun.s at 0x80107650).
// See build/GMSP01/asm/Enemy/Kazekun.s.
//
// NOTE: this file uses -inline deferred, under which the compiler emits
// function bodies in the REVERSE of source order (see
// docs/AGENT_MATCHING_TIPS.md / validate-symbol-order.py). Functions below
// are intentionally ordered so the emitted .text matches mario.MAP.

// marioEU.MAP places this function in Enemy.a Kazekun.cpp, and it is emitted
// immediately after TKazekun::TKazekun, so it must be the first definition in
// this file.
// `#pragma dont_inline` is load-bearing: the retail callers in
// TNerveKazekunAttack::execute reach this through a real `bl`, and without it
// MWCC expands the body at both call sites (adding ~0x580 bytes each).
#pragma dont_inline on
void SMS_CalcToDirMatrix(TPosition3f& param_1,
                         const JGeometry::TVec3<f32>& param_2,
                         const JGeometry::TVec3<f32>& param_3)
{
	// asm order: param_2 is normalised into a stack temp first, then
	// v2 = param_3 X v1 and v3 = v1 X v2, each normalised in turn.
	//
	// The two normalise() steps below are written out rather than using
	// TVec3<f32>::setLength because the retail build expands them with two
	// quirks that are load-bearing for the frame layout: the eps test is
	// emitted twice (MWCC CSE'd setLength's own test against the guard, so
	// the all-zero arm is dead), and the live arm substitutes +Z as a
	// degenerate-direction sentinel rather than zeroing. The third vector
	// is a plain setLength. inv_sqrt stays out of line here (see
	// origInvSqrt), unlike dot()/scale() which are inlined.
	JGeometry::TVec3<f32> v1 = param_2;
	f32 lsq1 = v1.x * v1.x + v1.y * v1.y + v1.z * v1.z;
	if (lsq1 <= JGeometry::TUtil<f32>::epsilon()) {
		v1.x = 0.0f;
		v1.y = 0.0f;
		v1.z = 1.0f;
	} else if (lsq1 <= JGeometry::TUtil<f32>::epsilon()) {
		v1.x = 0.0f;
		v1.y = 0.0f;
		v1.z = 0.0f;
	} else {
		f32 s1 = 1.0f * origInvSqrt(lsq1);
		v1.x *= s1;
		v1.y *= s1;
		v1.z *= s1;
	}

	JGeometry::TVec3<f32> v2;
	v2.cross(param_3, v1);
	f32 lsq2 = v2.x * v2.x + v2.y * v2.y + v2.z * v2.z;
	if (lsq2 <= JGeometry::TUtil<f32>::epsilon()) {
		v2.x = 0.0f;
		v2.y = 0.0f;
		v2.z = 1.0f;
	} else if (lsq2 <= JGeometry::TUtil<f32>::epsilon()) {
		v2.x = 0.0f;
		v2.y = 0.0f;
		v2.z = 0.0f;
	} else {
		f32 s2 = 1.0f * origInvSqrt(lsq2);
		v2.x *= s2;
		v2.y *= s2;
		v2.z *= s2;
	}

	JGeometry::TVec3<f32> v3;
	v3.cross(v1, v2);
	f32 lsq3 = v3.x * v3.x + v3.y * v3.y + v3.z * v3.z;
	if (lsq3 <= JGeometry::TUtil<f32>::epsilon()) {
		v3.x = 0.0f;
		v3.y = 0.0f;
		v3.z = 0.0f;
	} else {
		f32 s3 = 1.0f * origInvSqrt(lsq3);
		v3.x *= s3;
		v3.y *= s3;
		v3.z *= s3;
	}

	param_1.setXDir(v2);
	param_1.setYDir(v3);
	param_1.setZDir(v1);
}
#pragma dont_inline off

TKazekun::TKazekun(const char* name)
    : TSmallEnemy(name)
{
	unk1B0 = 0;
	onLiveFlag(LIVE_FLAG_UNK10);
}

void TKazekun::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);

	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor        = mMActorKeeper->createMActor("kazekun.bmd", 0);

	mSpine->initWith(&TNerveKazekunSearch::theNerve());

	mHeadHeight        = 40.0f;
	mBodyRadius        = 50.0f;
	mScaledBodyRadius  = 50.0f;

	initHitActor(0x10000029, 1, 0x80000000, mBodyScale * mBodyRadius,
	            mBodyScale * mHeadHeight, mBodyScale * mBodyRadius,
	            mBodyScale * mHeadHeight);

	onHitFlag(HIT_FLAG_NO_COLLISION);

	SMS_LoadParticle("/scene/kazekun/jpa/ms_kaze_appear.jpa", 0xcf);
	SMS_LoadParticle("/scene/kazekun/jpa/ms_kaze_wind.jpa", 0x189);
	SMS_LoadParticle("/scene/kazekun/jpa/ms_kaze_blur.jpa", 0x18a);

	initAnmSound();

	unk194.set(mPosition);

	reset();
}

void TKazekun::reset()
{
	unk1A0.set(0.0f, 0.0f, 0.0f, 1.0f);
	JGeometry::TVec3<f32> temp = unk194;
	mPosition.set(temp);
	onLiveFlag(LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8);
	setAnmSound(nullptr);
}

void TKazekun::calcRootMatrix()
{
	if (isTaken()) {
		TSpineEnemy::calcRootMatrix();
		return;
	}

	// The Kazekun has no MActor once it is flying, so its root matrix is
	// driven straight from the orientation quaternion kept in unk1A0.
	TPosition3f mtx;
	mtx.setQuat(unk1A0);
	mtx.setTrans(mPosition);
	PSMTXCopy(mtx, getModel()->getBaseTRMtx());

	TSpineBase<TLiveActor>* spine = mSpine;
	bool same = spine->getLatestNerve() == &TNerveKazekunTurn::theNerve()
	    || spine->getLatestNerve() == &TNerveKazekunPreAttack::theNerve()
	    || spine->getLatestNerve() == &TNerveKazekunAttack::theNerve()
	    || spine->getLatestNerve() == &TNerveKazekunHitWater::theNerve();
	if (same) {
		// 0x189/0x18a: the wind + blur emitters, ids with no name in
		// System/Particles.hpp
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x189, getModel()->getBaseTRMtx(), 1, this);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x18a, getModel()->getBaseTRMtx(), 1, this);
	}
}

void TKazekun::bind()
{
	mLinearVelocity += mVelocity;
}

void TKazekun::behaveToWater(THitActor*)
{
	TSpineBase<TLiveActor>* spine = mSpine;
	bool same = spine->getLatestNerve() == &TNerveKazekunTurn::theNerve()
	    || spine->getLatestNerve() == &TNerveKazekunPreAttack::theNerve()
	    || spine->getLatestNerve() == &TNerveKazekunAttack::theNerve();
	if (same) {
		mSpine->reset();
		mSpine->setNext(&TNerveKazekunHitWater::theNerve());
	}
}

static const char* Kazekun_bastable[] = {
	"/scene/Kazekun/bas/kazekun_appear.bas",
	"/scene/Kazekun/bas/kazekun_attack.bas",
	nullptr,
	"/scene/Kazekun/bas/kazekun_vanish.bas",
	"/scene/Kazekun/bas/kazekun_wait.bas",
};

const char** TKazekun::getBasNameTable() const
{
	return Kazekun_bastable;
}

void TKazekun::attackToMario()
{
	TSpineBase<TLiveActor>* spine = mSpine;
	bool same = spine->getLatestNerve() == &TNerveKazekunTurn::theNerve()
	    || spine->getLatestNerve() == &TNerveKazekunPreAttack::theNerve()
	    || spine->getLatestNerve() == &TNerveKazekunAttack::theNerve();
	if (same) {
		SMS_SendMessageToMario(this, 0xe);
	}
}

bool TKazekun::isCollidMove(THitActor*)
{
	return false;
}

void TKazekun::setDeadAnm()
{
	mMActor->getFrameCtrl(0)->init(1);
	mMActor->getFrameCtrl(0)->setFrame(0.0f);
}

void TKazekun::flyAroundMario()
{
	// Horizontal offset to Mario, with the height replaced by a steerable
	// climb rate: the closer the Kazekun already is, the faster it climbs.
	JGeometry::TVec3<f32> dir = *gpMarioPos;
	dir.y += getSaveParam2()->mTurnOffsetY.get();
	dir -= mPosition;

	// The closer the Kazekun already is, the faster it climbs: the raw
	// height difference is clamped to +/-400 and scaled by 0.0025.
	f32 rise = dir.y < -400.0f ? -400.0f
	                           : (dir.y > 400.0f ? 400.0f : dir.y);
	f32 climb = rise * 0.0025f;
	dir.y = 0.0f;

	// Then clamp the normalised distance into [0, 2] and steer the heading
	// by the leftover.
	f32 ratio = origSqrt(origVec3Dot(&dir, &dir))
	          / getSaveParam2()->mAroundDist.get();
	f32 around = ratio < 0.0f ? 0.0f : (ratio > 2.0f ? 2.0f : ratio);

	TPosition3f mtx;
	origMtxCtor(&mtx);
	JGeometry::TVec3<f32> up;
	up.set(0.0f, 1.0f, 0.0f);
	SMS_CalcToDirMatrix(mtx, dir, up);

	JGeometry::TQuat4<f32> q;
	origMtxGetQuat(mtx, q);

	JGeometry::TQuat4<f32> rot;
	origQuatSetRotate(&rot, mtx.at(0, 1), mtx.at(1, 1), mtx.at(2, 1),
	                  0.5f * (1.5707964f * (2.0f - around)));

	origQuatMulInPlace(&q, rot);
	unk1A0 = q;

	// Face straight up out of the rotated frame, then blend in the climb
	// rate and the circling speed.
	JGeometry::TVec3<f32> v(0.0f, 0.0f, 1.0f);
	f32 gain = 1.0f + fabsf(climb);
	origQuatRotateVec(&q, v, v);
	v.y = climb;
	v.x *= gain;
	v.y *= gain;
	v.z *= gain;

	f32 speed = getSaveParam2()->mAroundSpeed.get();
	v.x *= speed;
	v.y *= speed;
	v.z *= speed;

	mLinearVelocity = v;
}

TKazekunParams::TKazekunParams(const char* name)
    : TSmallEnemyParams(name)
    , PARAM_INIT(mAppearDist, 1000.0f)
    , PARAM_INIT(mAroundDist, 400.0f)
    , PARAM_INIT(mAroundSpeed, 30.0f)
    , PARAM_INIT(mAroundTime, 600)
    , PARAM_INIT(mAttackSpeed, 30.0f)
    , PARAM_INIT(mAirFric, 0.97f)
    , PARAM_INIT(mResetTime, 300)
    , PARAM_INIT(mResetTimeHitting, 1500)
    , PARAM_INIT(mPoseTime, 120)
    , PARAM_INIT(mDicideTiming, 0.1f)
    , PARAM_INIT(mTurnOffsetY, 200.0f)
    , PARAM_INIT(mLostOffsetYUp, 500.0f)
    , PARAM_INIT(mLostOffsetYDown, 500.0f)
    , PARAM_INIT(mPoseSpeed, 7.6f)
    , PARAM_INIT(mPoseOmegaRate, 0.04f)
{
	load(mPrmPath);
}

TKazekunManager::TKazekunManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TKazekunManager::load(JSUMemoryInputStream& stream)
{
	TKazekunParams* params = new TKazekunParams("/enemy/kazekun.prm");
	unk38 = params;
	params->mSLAttackRadius.set(50);
	params->mSLAttackHeight.set(40);
	params->mSLDamageRadius.set(50);
	params->mSLDamageHeight.set(40);
	TSmallEnemyManager::load(stream);
	unk5C = 0;
}

void TKazekunManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
	    { "kazekun.bmd", 0x10210000, 0 },
	    { nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

DEFINE_NERVE(TNerveKazekunSearch, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->reset();
	}

	self->updateSquareToMario();

	f32 range = self->getSaveParam2()->mAppearDist.get();
	if (self->getDistToMarioSquared() <= range * range) {
		spine->pushAfterCurrent(&TNerveKazekunAppear::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunAppear, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->offLiveFlag(LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8);
		// 0xcf: particle id with no name in System/Particles.hpp
		gpMarioParticleManager->emit(0xcf, &self->mPosition, 0, nullptr);
		self->mMActor->setBck("kazekun_appear");
		self->setCurAnmSound();
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveKazekunTurn::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunTurn, TLiveActor)
{
	bool lost;
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBck("kazekun_wait");
		self->setCurAnmSound();
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
	}

	self->flyAroundMario();

	lost = true;
	f32 diff = gpMarioPos->y - self->unk194.y;
	if (!(diff < -self->getSaveParam2()->mLostOffsetYDown.get()
	      || self->getSaveParam2()->mLostOffsetYUp.get() < diff)) {
		lost = false;
	}

	if (lost) {
		spine->pushAfterCurrent(&TNerveKazekunDisappear::theNerve());
		return true;
	}

	if (self->getSaveParam2()->mAroundTime.get() < (f32)spine->getTime()) {
		spine->pushAfterCurrent(&TNerveKazekunPreAttack::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunPreAttack, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->doAttackPose(true);
		gpMSound->startSoundActor(MSD_SE_EN_KAZEKUN_READY, &self->mPosition, 0,
		                          nullptr, 0, 4);
		self->setAnmSound(nullptr);
	}

	if (self->getSaveParam2()->mPoseTime.get()
	        * self->getSaveParam2()->mDicideTiming.get()
	    < spine->getTime()) {
		JGeometry::TVec3<f32> pos = SMS_GetMarioPos();
		self->setGoalPath(TPathNode(pos));
	}

	self->doAttackPose(false);

	if (self->getSaveParam2()->mPoseTime.get() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveKazekunAttack::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunAttack, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBck("kazekun_attack");
		self->setCurAnmSound();

		JGeometry::TVec3<f32> dir = self->unk104.getPoint();
		dir -= self->mPosition;
		origVec3SetLength(&dir, dir,
		                  self->getSaveParam2()->mAttackSpeed.get());
		self->mVelocity = dir;

		// Bend the flying orientation towards the velocity direction. The
		// retail build writes this sequence out twice -- once inside the
		// time == 0 block and once unconditionally -- so it is duplicated
		// here rather than factored into a helper.
		JGeometry::TQuat4<f32> prev = self->unk1A0;
		JGeometry::TVec3<f32> vel   = self->mVelocity;

		TPosition3f mtx;
		origMtxCtor(&mtx);
		JGeometry::TVec3<f32> up;
		origVec3Set(&up, 0.0f, 1.0f, 0.0f);
		SMS_CalcToDirMatrix(mtx, vel, up);

		JGeometry::TQuat4<f32> q;
		origMtxGetQuat(mtx, q);

		JGeometry::TVec3<f32> axis;
		origVec3Set(&axis, mtx.at(0, 1), mtx.at(1, 1), mtx.at(2, 1));

		JGeometry::TQuat4<f32> rot;
		// TQuat4<f>::setRotate(axis, 0.0f), written out because the retail
		// build takes TVec3<f>::scale out of line there.
		origVec3Scale(&rot.xyz(), sinf(0.0f), axis);
		rot.w = cosf(0.0f);

		// JGeometry::TQuat4<f>::mul(rot), written out because the ROM's
		// version takes set<f>TVec4<f> out of line (see origVec4Set) and
		// because its y term is z*rot.x - x*rot.z, not x*rot.z - z*rot.x.
		f32 nx = q.w * rot.x + q.x * rot.w + q.y * rot.z - q.z * rot.y;
		f32 ny = q.w * rot.y + q.y * rot.w + q.z * rot.x - q.x * rot.z;
		f32 nz = q.w * rot.z + q.z * rot.w + q.x * rot.y - q.y * rot.x;
		f32 nw = q.w * rot.w - q.x * rot.x - q.y * rot.y - q.z * rot.z;
		origVec4Set(&q, nx, ny, nz, nw);

		origQuatSlerp(&prev, q, 0.1f);
		origVec4SetLength(&prev, prev, 1.0f);
		self->unk1A0 = prev;
	}

	{
		JGeometry::TQuat4<f32> prev = self->unk1A0;
		JGeometry::TVec3<f32> vel   = self->mVelocity;

		TPosition3f mtx;
		origMtxCtor(&mtx);
		JGeometry::TVec3<f32> up;
		origVec3Set(&up, 0.0f, 1.0f, 0.0f);
		SMS_CalcToDirMatrix(mtx, vel, up);

		JGeometry::TQuat4<f32> q;
		origMtxGetQuat(mtx, q);

		JGeometry::TVec3<f32> axis;
		origVec3Set(&axis, mtx.at(0, 1), mtx.at(1, 1), mtx.at(2, 1));

		JGeometry::TQuat4<f32> rot;
		origVec3Scale(&rot.xyz(), sinf(0.0f), axis);
		rot.w = cosf(0.0f);

		f32 nx = q.w * rot.x + q.x * rot.w + q.y * rot.z - q.z * rot.y;
		f32 ny = q.w * rot.y + q.y * rot.w + q.z * rot.x - q.x * rot.z;
		f32 nz = q.w * rot.z + q.z * rot.w + q.x * rot.y - q.y * rot.x;
		f32 nw = q.w * rot.w - q.x * rot.x - q.y * rot.y - q.z * rot.z;
		origVec4Set(&q, nx, ny, nz, nw);

		origQuatSlerp(&prev, q, 0.1f);
		origVec4SetLength(&prev, prev, 1.0f);
		self->unk1A0 = prev;
	}

	JGeometry::TVec3<f32> v = self->mVelocity;
	// The retail build reads the air friction through the getSaveParam2()
	// virtual exactly once and scales the three components in line (no `bl
	// scale` here, unlike the setLength calls above).
	f32 airFric = self->getSaveParam2()->mAirFric.get();
	v.x *= airFric;
	v.y *= airFric;
	v.z *= airFric;
	self->mVelocity = v;

	if (v.x * v.x + v.y * v.y + v.z * v.z < 1.0f) {
		spine->pushAfterCurrent(&TNerveKazekunDisappear::theNerve());
		self->unk1B0 = self->getSaveParam2()->mResetTime.get();
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunDisappear, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBck("kazekun_vanish");
		self->setCurAnmSound();
		// 0xcf: particle id with no name in System/Particles.hpp
		gpMarioParticleManager->emit(0xcf, &self->mPosition, 0, nullptr);

		JGeometry::TVec3<f32> vel(0.0f, 0.0f, 0.0f);
		self->mVelocity = vel;

		self->onHitFlag(HIT_FLAG_NO_COLLISION);
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveKazekunWait::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunWait, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->onLiveFlag(LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8);
		self->setAnmSound(nullptr);
	}

	if (self->unk1B0 < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveKazekunSearch::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunHitWater, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBck("kazekun_hit");
		self->setCurAnmSound();
		gpMSound->startSoundActor(MSD_SE_EN_KAZEKUN_DOWN, &self->mPosition, 0,
		                          nullptr, 0, 4);
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveKazekunDisappear::theNerve());
		self->unk1B0 = self->getSaveParam2()->mResetTimeHitting.get();
		return true;
	}

	JGeometry::TVec3<f32> vel(0.0f, 0.0f, 0.0f);
	self->mVelocity = vel;
	return false;
}
