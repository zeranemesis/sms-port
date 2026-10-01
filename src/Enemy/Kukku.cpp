#include <Enemy/Kukku.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp followed by the four MActor mtx-calc
// type names; InfectiousStrings.hpp pulls both in, in that order, which is
// what every retail enemy TU carries.
#include <M3DUtil/InfectiousStrings.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/ObjModel.hpp>
#include <M3DUtil/MActor.hpp>
#include <Map/PollutionManager.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/ItemManager.hpp>
#include <System/Particles.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <Player/MarioAccess.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <Animal/AnimalBase.hpp>
#include <Enemy/Graph.hpp>

// rogue includes: pulls in JALList.hpp's JALList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <MarioUtil/TexUtil.hpp>
#include <Strategic/Strategy.hpp>

// ============= static data =============

static const char* tori_bastable[] = {
	"/scene/tori/bas/tori_back.bas",
	"/scene/tori/bas/tori_down.bas",
	nullptr,
	"/scene/tori/bas/tori_fall_end.bas",
	"/scene/tori/bas/tori_hit.bas",
	"/scene/tori/bas/tori_wait.bas",
};

namespace {

// How many coins TKukku::dropCoins() scatters, picked at random.
const int cDropCoinNumTable[] = { 3, 3, 1, 2 };

} // namespace

// ============= out-of-line JGeometry helpers =============
//
// The retail build calls the JGeometry header templates out of line
// (`bl set<f>__Q29JGeometry8TVec3<f>Ffff`, `bl TQuat4<f>::rotate`), while
// MWCC always expands them at the call site -- which drags a spilled copy of
// the vector through the frame (dropCoins() ends up 0x80 bigger).  MWCC
// ignores `#pragma dont_inline` for a *header* body, so these TU-local
// wrappers exist purely to give the call sites the right shape.
#pragma dont_inline on
static void origVec3Set(JGeometry::TVec3<f32>* v, f32 x, f32 y, f32 z)
{
	v->set(x, y, z);
}
#pragma dont_inline off

#pragma dont_inline on
static void origQuatRotate(const JGeometry::TQuat4<f32>* q,
                           const JGeometry::TVec3<f32>& v,
                           JGeometry::TVec3<f32>& dest)
{
	q->rotate(v, dest);
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
static void origVec3Scale(JGeometry::TVec3<f32>* dest,
                          f32 f,
                          const JGeometry::TVec3<f32>& src)
{
	dest->scale(f, src);
}
#pragma dont_inline off

// The ROM reaches TNerveKukkuFall::theNerve() through a real `bl` from
// TKukku::updateRotation (twice) but *inlines* the static-local guard from
// behaveToWater and calcRootMatrix. `#pragma dont_inline` is per-definition,
// so TNerveKukkuFall::theNerve() itself stays inlinable and only the two
// updateRotation comparisons go through this wrapper.
#pragma dont_inline on
static const TNerveKukkuFall& origFallNerve()
{
	return TNerveKukkuFall::theNerve();
}
#pragma dont_inline off

// TNerveKukkuPostFall::theNerve() goes the other way round: the ROM calls it
// out of line from all four comparison sites (updateRotation x2,
// behaveToWater, calcRootMatrix), so the wrapper is used everywhere.
#pragma dont_inline on
static const TNerveKukkuPostFall& origPostFallNerve()
{
	return TNerveKukkuPostFall::theNerve();
}
#pragma dont_inline off

// ============= TKukkuBall =============

// TODO: this TU is a from-scratch scaffold (no header existed before).
// Field offsets, base classes and most function bodies were reconstructed
// from the target assembly in build/GMSP01/asm/Enemy/Kukku.s and have not all
// been matched yet with decomp-diff.
//
// Source order below matches tools/validate-symbol-order.py (this TU is
// -inline deferred, so the non-weak/out-of-line function order is the
// *reverse* of mario.MAP's .text layout -- TKukkuBall/TEnemyCoinUnit come
// first, the nerves' execute() bodies come last).

void TKukkuBall::init()
{
	initHitActor(0x1000002E, 1, 0x8000, 30.0f, 30.0f, 0.0f, 0.0f);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);

	// Register in the "center" hit group so Mario can kick the balls around.
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("センター"))
	    ->getChildren()
	    .push_back(this);

	ResTIMG* tex = (ResTIMG*)JKRFileLoader::getGlbResource(
	    "/scene/map/pollution/H_ma_rak.bti");
	if (tex != nullptr) {
		SMS_ChangeTextureAll(mMActor->getModel()->getModelData(),
		                     "K_name_dummy", *tex);
	}
}

void TKukkuBall::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (unk6C & 0x80000000) {
		return;
	}

	if (cue & CUE_CALC_ANIM) {
		// The ball has no bones worth animating, so its "root" matrix is
		// just the identity carrying the ball's position.
		JGeometry::TRotation3<JGeometry::TMatrix34<
		    JGeometry::SMatrix34C<f32> > >
		    mtx;
		mtx.identity33();
		mtx.ref(0, 3) = mPosition.x;
		mtx.ref(1, 3) = mPosition.y;
		mtx.ref(2, 3) = mPosition.z;

		mMActor->getModel()->setBaseScale(mScaling);
		PSMTXCopy(reinterpret_cast<MtxPtr>(&mtx),
		          mMActor->getModel()->getBaseTRMtx());
		mMActor->getModel()->calc();
	}

	if (cue & 0x80000000) {
		THitActor** end = mCollisions + mColCount;

		for (THitActor** it = mCollisions; it != end; it++) {
			THitActor* actor = *it;

			if (actor->isActorType(0x80000001)) {
				SMS_SendMessageToMario(this, 0xE);
				onHitFlag(1);
				unk6C |= 1;
				gpPollution->stamp(1, mPosition.x, mPosition.y,
				                   mPosition.z, 500.0f);
			}
		}

		unk70.y -= 0.9f;
		unk70 *= 0.94f;

		JGeometry::TVec3<f32> newPos(mPosition);
		newPos += unk70;

		const TBGCheckData* bg;
		gpMap->checkGround(newPos.x, newPos.y + mPosition.y, newPos.z, &bg);

		if (newPos.y <= newPos.x + 1.0f + 0.05f) {
			onHitFlag(1);
			unk6C |= 1;
			gpPollution->stamp(1, mPosition.x, mPosition.y, mPosition.z,
			                   500.0f);
		}

		gpMap->isTouchedOneWallAndMoveXZ(&newPos.x, newPos.y + mPosition.y,
		                                 &newPos.z, mPosition.x);
		mPosition = newPos;
	}

	if (!(unk6C & 4)) {
		mMActor->perform(cue, graphics);
	}
}

// TODO: fully inlined (UNUSED) helper class used by TKukku::dropCoins() to
// hand out coins one at a time from a small fixed pool; reconstructed purely
// from the mangled UNUSED symbol names/sizes in mario.MAP, never verified
// against real asm. Sizes don't match yet (see validate-symbol-order.py
// output), so the guessed bodies below are definitely incomplete.

TEnemyCoinUnit::TEnemyCoinUnit(int num)
    : unk0(num)
{
}

void TEnemyCoinUnit::init()
{
	// TODO: not yet reconstructed
}

void* TEnemyCoinUnit::getUnusedItem()
{
	// TODO: not yet reconstructed
	return 0;
}

TKukku::TKukku(const char* name)
    : TSmallEnemy(name)
    , mMushroom(nullptr)
{
}

void TKukku::init(TLiveManager* liveManager)
{
	mManager = liveManager;
	mManager->manageActor(this);

	mMActorKeeper = new TMActorKeeper(mManager, 4);
	mMActor = mMActorKeeper->createMActor("tori.bmd", 0);

	mSpine->initWith(&TNerveKukkuGraphWander::theNerve());

	mMushroom = TMapObjBaseManager::newAndRegisterObj("mushroom1upR");

	for (TKukkuBall** ball = mBall; ball != mBall + 3; ball++) {
		*ball = new TKukkuBall(mMActorKeeper->createMActor("torifun.bmd", 3));
		(*ball)->init();
	}

	initCollision();

	initAnmSound();

	initParticle();

	mCenterJointIndex
	    = getModel()->getModelData()->getJointName()->getIndex("center");

	reset();
}

void TKukku::initCollision()
{
	initHitActor(0x1000002E, 1, 0x80000000, 30.0f, 30.0f, 100.0f, 100.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);
}

void TKukku::initParticle()
{
	SMS_LoadParticle("/scene/tori/jpa/ms_cooc_ase.jpa", 0x18C);
	SMS_LoadParticle("/scene/tori/jpa/ms_cooc_hane.jpa", 0x18D);
}

void TKukku::reset()
{
	unk1A4            = 0;
	unk1AC            = 0;
	mGravity          = 0.0f;
	mDropCount        = 0;
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	mScaledBodyRadius = 75.0f;
}

BOOL TKukku::receiveMessage(THitActor* sender, u32 message)
{
	// the target tests only mLiveFlag bit 0 here (clrlwi r0, r0, 31)
	if (checkLiveFlag(LIVE_FLAG_DEAD)) {
		return FALSE;
	}

	// The target dispatches with a cmpwi/bge range check (msg >= 2 -> base,
	// msg >= 0 -> die, else base) which is a switch range cascade.
	switch ((THitMessageType)message) {
	case HIT_MESSAGE_TRAMPLE:
	case HIT_MESSAGE_HIP_DROP:
		mSpine->reset();
		mSpine->setNext(&TNerveSmallEnemyDie::theNerve());
		return TRUE;
	default:
		break;
	}

	return TSmallEnemy::receiveMessage(sender, message);
}

void TKukku::control()
{
	// The target re-loads unk1A4 for the decrement instead of reusing the
	// value from the compare (MWCC does not CSE it across the branch here).
	if (unk1A4 > 0) {
		unk1A4--;
	}
	TLiveActor::control();
}

void TKukku::calcRootMatrix()
{
	// Spelled out rather than isNerve(&theNerve()): the target loads the latest
	// nerve first and only calls TNerveSmallEnemyDie::theNerve() after the
	// branch, which is the reverse of the isNerve(&theNerve()) argument order.
	// TODO: TSpineBase::Nerve is a private typedef, so the local has to be
	// spelled as the expanded type; a `getLatestNerve()` helper on TKukku would
	// be cleaner but does not exist.
	const TNerveBase<TLiveActor>* latest = mSpine->getLatestNerve();

	if (latest == &TNerveSmallEnemyDie::theNerve()) {
		// While dying the chicken is laid on its side, following the plane it
		// landed on.
		JGeometry::TVec3<f32> dir(0.0f, 1.0f, 0.0f);

		if (mGroundPlane != nullptr) {
			dir = mGroundPlane->getNormal();
			dir.normalize();
		}

		JGeometry::TQuat4<f32> quat;
		quat.setRotate(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f), dir);

		JGeometry::TQuat4<f32> euler;
		euler.setEulerY(0.017453294f * mRotation.y);
		quat.mul(euler);

		JGeometry::TRotation3<JGeometry::TMatrix34<
		    JGeometry::SMatrix34C<f32> > >
		    mtx;
		mtx.setQuat(quat);
		mtx.ref(0, 3) = mPosition.x;
		mtx.ref(1, 3) = mPosition.y;
		mtx.ref(2, 3) = mPosition.z;

		PSMTXCopy(reinterpret_cast<MtxPtr>(&mtx),
		          getModel()->getBaseTRMtx());
		getModel()->setBaseScale(mScaling);
	} else {
		TSpineEnemy::calcRootMatrix();
	}

	// re-read, not the value from above: the target reloads the latest nerve
	// through the spine pointer again here.
	latest = mSpine->getLatestNerve();

	if (latest == &TNerveKukkuFall::theNerve()
	    || latest == &origPostFallNerve()) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x18D, getModel()->getAnmMtx(mCenterJointIndex), 1, this);
	}

	if (mMActor->checkCurAnm("tori_back", 0)) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x18C, getModel()->getAnmMtx(mCenterJointIndex), 1, this);
	}
}

void TKukku::bind()
{
	TLiveActor::bind();
}

void TKukku::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);

	// the target walks a pointer from &mBall[0] to &mBall[0] + 3 rather
	// than indexing, so the loop is spelled as a pointer walk.
	for (TKukkuBall** ball = mBall; ball != mBall + 3; ball++) {
		(*ball)->perform(cue, graphics);
	}
}

void TKukku::behaveToWater(THitActor* hitActor)
{
	// Water only matters while the chicken is on its way down; once it is
	// already climbing back to the graph there is nothing to interrupt.
	const TNerveBase<TLiveActor>* latest = mSpine->getLatestNerve();

	bool falling = latest == &TNerveKukkuFall::theNerve()
	               || latest == &origPostFallNerve();

	if (falling) {
		bool recovering
		    = mSpine->getLatestNerve() == &TNerveKukkuRecoverGraph::theNerve();

		if (!recovering) {
			mSpine->reset();
			mSpine->setNext(&TNerveKukkuFall::theNerve());
		}
	}
}

void TKukku::updateRotation()
{
	// diff = pathNodePoint - mPosition
	JGeometry::TVec3<f32> diff = unkF4.getPoint();
	diff -= mPosition;

	f32 dist = diff.length();

	if (dist < 100.0f)
		return;

	f32 marchSpeed = getSaveParams()->mMarchSpeed.get();
	f32 turnSpeed  = getSaveParams()->mTurnSpeed.get();

	if (dist <= 2.0f * calcMinimumTurnRadius(marchSpeed, turnSpeed))
		turnSpeed = calcTurnSpeedToReach(marchSpeed, 0.5f * dist);

	TAnimalBase::getRotationFlyToDir(&mRotation, diff, marchSpeed, turnSpeed);

	// Zero the pitch/roll unless we are in one of the falling nerves.
	{
		const TNerveBase<TLiveActor>* latest = mSpine->getLatestNerve();
		bool falling = latest == &origFallNerve()
		               || latest == &origPostFallNerve();
		mRotation.x *= falling ? 0.0f : 1.0f;
	}

	{
		const TNerveBase<TLiveActor>* latest = mSpine->getLatestNerve();
		bool falling = latest == &origFallNerve()
		               || latest == &origPostFallNerve();
		mRotation.z *= falling ? 0.0f : 1.0f;
	}
}

#pragma dont_inline on
void TKukku::calcMomentum(f32 speed)
{
	JGeometry::TQuat4<f32> quat = SMS_Eular2Quat(mRotation);
	JGeometry::TVec3<f32> v(0.0f, 0.0f, speed);
	quat.rotate(v, v);
	mLinearVelocity = v;
}
#pragma dont_inline off

void TKukku::dropCoins()
{
	// The target tests the count first and bails out above 10; the mushroom
	// only ever replaces the coin pile once the count has reached 10.
	if (mDropCount > 10) {
		return;
	}

	if (mDropCount == 10 && mMushroom != nullptr) {
		++mDropCount;
		mMushroom->appear();
		mMushroom->JSGSetTranslation(mPosition);
		mMushroom->mVelocity.setAll(0.0f);
		// NOTE: the target masks the live flags down to bits 3..5
		// (rlwinm r0,r0,0,28,26), i.e. it keeps only 0x38.
		mMushroom->mLiveFlag &= 0x38;
		return;
	}

	s32 index = (s32)(MsRandF() * 4.0f);
	if (index < 1) {
		index = 1;
	} else if (index > 3) {
		index = 3;
	}

	int num = cDropCoinNumTable[index];

	// Half of the full turn, so the coins fan out over half a circle.
	JGeometry::TQuat4<f32> spread;
	spread.setEulerY(6.2831855f / (f32)num);

	// Pitch the throw direction by the drop angle.
	JGeometry::TQuat4<f32> pitch;
	pitch.setEulerX(3.1415927f * getSaveParams()->mDropAngleX.get());

	f32 dropSpeed = getSaveParams()->mDropSpeed.get();

	JGeometry::TVec3<f32> dir;
	origVec3Set(&dir, dropSpeed * MsSin(mRotation.y), 0.0f,
	            dropSpeed * MsCos(mRotation.y));

	JGeometry::TVec3<f32> pos(dir);

	pitch.rotate(pos, pos);
	spread.rotate(pos, pos);

	for (s32 i = 0; i < num; i++) {
		TMapObjBase* coin = gpItemManager->makeObjAppear(0x2000E);
		if (coin == nullptr) {
			break;
		}

		coin->appear();
		coin->JSGSetTranslation(mPosition);
		coin->mVelocity.set(pos);
		coin->mLiveFlag &= 0x38;

		if (++mDropCount == 10) {
			break;
		}

		spread.rotate(pos, pos);
	}
}

void TKukku::setDeadAnm()
{
	getMActor()->setBck("tori_down");
	setCurAnmSound();
}

void TKukku::setAfterDeadEffect()
{
	TSmallEnemy::setAfterDeadEffect();
	gpPollution->stamp(getManager()->getUnk58(), mPosition.x, mPosition.y,
	                   mPosition.z, 1000.0f);
}

TKukkuParams::TKukkuParams(const char* path)
    : TSmallEnemyParams(path)
    , PARAM_INIT(mMarchSpeed, 3.0f)
    , PARAM_INIT(mTurnSpeed, 0.2f)
    , PARAM_INIT(mWaterPowerY, 12.0f)
    , PARAM_INIT(mShootSpeed, 1.0f)
    , PARAM_INIT(mShootInterval, 60)
    , PARAM_INIT(mSearchRange, 800.0f)
    , PARAM_INIT(mHabatakiTimer, 45)
    , PARAM_INIT(mAirFric, 0.97f)
    , PARAM_INIT(mUpperVelocityY, 0.0f)
    , PARAM_INIT(mDropSpeed, 5.0f)
    , PARAM_INIT(mDropAngleX, -0.25f)
{
	TParams::load(mPrmPath);
}

TKukkuManager::TKukkuManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TKukkuManager::load(JSUMemoryInputStream& stream)
{
	TKukkuParams* params = new TKukkuParams("/enemy/kukku.prm");

	unk38 = params;

	params->mSLAttackRadius.set(30);
	params->mSLAttackHeight.set(30);
	params->mSLDamageRadius.set(100);
	params->mSLDamageHeight.set(100);
	params->mSLBodyRadius.set(5.0f);

	TSmallEnemyManager::load(stream);
}

void TKukkuManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "tori.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};

	createModelDataArray(entry);
}

const char** TKukku::getBasNameTable() const
{
	return tori_bastable;
}

// ============= nerves =============
// execute() is a regular out-of-line member function (strong symbol), so its
// order is significant, unlike theNerve()/~dtor which are header-inline
// (weak) and compiler-ordered.

DEFINE_NERVE(TNerveKukkuGraphWander, TLiveActor)
{
	TKukku* kukku = (TKukku*)spine->getBody();

	if (spine->getTime() == 0) {
		JGeometry::TVec3<f32> zero;
		zero.zero();
		kukku->mVelocity = zero;

		kukku->getTracer()->reset();
		kukku->goToShortestNextGraphNode();

		if (kukku->mMActor->checkCurAnm("tori_back", 0)) {
			kukku->mMActor->setBck("tori_back");
			kukku->setCurAnmSound();
		} else {
			kukku->mMActor->setBck("tori_wait");
			kukku->setCurAnmSound();
		}
	}

	if (kukku->isReachedToGoal()) {
		kukku->goToRandomNextGraphNode();

		if (!kukku->mMActor->checkCurAnm("tori_wait", 0)) {
			kukku->mMActor->setBck("tori_wait");
			kukku->setCurAnmSound();
		}
	}

	JGeometry::TVec3<f32> toMario = *gpMarioPos;
	toMario -= kukku->mPosition;
	toMario.y = 0.0f;

	TKukkuParams* params = kukku->getSaveParams();

	// Target: `if (distSq < range*range) { ...shoot logic... }` -- the bge skips
	// the whole block, so the shoot logic is the positive branch.
	if (toMario.squared() < params->mSearchRange.get()
	                          * params->mSearchRange.get()) {
		if (kukku->unk1AC >= 0) {
			kukku->unk1AC = kukku->unk1AC - 1;
		} else {
			// Find the first ball that is not in use and shoot it at Mario.
			// The target tests unk6C *bit 31* (`clrlwi. r3, r3, 31`), not the
			// whole word, so the mask is spelled out rather than using a
			// truthiness test.
			// TODO: no THitFlagBits enumerator covers 0x80000000 (the nearest
			// are HIT_FLAG_UNK40000000 and ACTOR_TYPE_PLAYER), so the literal
			// mask is used until the flag's meaning is identified.
			TKukkuBall* ball = nullptr;
			for (TKukkuBall** it = kukku->mBall; it != kukku->mBall + 3;
			     it++) {
				TKukkuBall* b = *it;

				bool inUse = true;
				if ((b->unk6C & 0x80000000u) && b->unk7C == 0)
					inUse = false;

				if (inUse)
					continue;

				ball = b;
				break;
			}

			if (ball) {
				// dir = up, rescaled so that its length is the shot speed.
				// setLength is spelled out because the target calls dot() and
				// scale() out of line here (bl TVec3::dot / bl TVec3::scale),
				// while MWCC expands both.
				f32 shootSpeed = params->mShootSpeed.get();
				JGeometry::TVec3<f32> dir(0.0f, 1.0f, 0.0f);

				f32 lsq = origVec3Dot(&dir, &dir);
				if (lsq <= JGeometry::TUtil<f32>::epsilon()) {
					dir.zero();
				} else {
					origVec3Scale(&dir,
					              shootSpeed
					                  * JGeometry::TUtil<f32>::inv_sqrt(lsq),
					              dir);
				}

				// TODO: the joint name string is "null_osen" in the target,
				// which looks like a name that failed to resolve at build time
				kukku->getModel()
				    ->getModelData()
				    ->getJointName()
				    ->getIndex("null_osen");

				ball->mPosition = kukku->mPosition;
				ball->unk70     = dir;

				ball->mHitFlags &= ~HIT_FLAG_UNK40000000;
				ball->unk6C &= ~HIT_FLAG_UNK40000000;

				kukku->unk1AC = params->mShootInterval.get();

				if (gpMSound->gateCheck(MSD_SE_EN_TORI_SHIT)) {
					MSoundSESystem::MSoundSE::startSoundActor(
					    MSD_SE_EN_TORI_SHIT, (const Vec*)&kukku->mPosition, 0,
					    nullptr, 0, 4);
				}
			}
		}
	}

	kukku->updateRotation();

	// velocity = forward vector of mRotation * march speed
	f32 marchSpeed = kukku->getSaveParams()->mMarchSpeed.get();
	JGeometry::TQuat4<f32> quat = SMS_Eular2Quat(kukku->mRotation);
	JGeometry::TVec3<f32> v;
	origVec3Set(&v, 0.0f, 0.0f, marchSpeed);
	origQuatRotate(&quat, v, v);
	kukku->mLinearVelocity = v;

	return FALSE;
}

DEFINE_NERVE(TNerveKukkuHit, TLiveActor)
{
	// TODO: not yet reconstructed; UNUSED in the target (fully inlined at its
	// single call site)
	return FALSE;
}

DEFINE_NERVE(TNerveKukkuFall, TLiveActor)
{
	TKukku* kukku = (TKukku*)spine->getBody();

	if (spine->getTime() == 0) {
		kukku->getMActor()->setBck("tori_wait");
		kukku->setCurAnmSound();

		J3DFrameCtrl* ctrl = kukku->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setRate(2.0f * SMSGetAnmFrameRate());

		JGeometry::TVec3<f32> vel;
		vel.set(0.0f,
		        -kukku->getSaveParams()->mWaterPowerY.get(),
		        0.0f);
		kukku->mVelocity = vel;

		kukku->dropCoins();
	}

	// Once the chicken is no longer falling through the air it is simply
	// splattered, so bail out before touching the velocity.
	if (!kukku->checkLiveFlag(LIVE_FLAG_AIRBORNE)) {
		SMS_EasyEmitParticle<E_SMS_EFFECT_ONETIME_NORMAL>(
		    E_SMS_EFFECT_ONETIME_NORMAL(0xA1), &kukku->mPosition,
		    (const void*)nullptr, JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
		SMS_EasyEmitParticle<E_SMS_EFFECT_ONETIME_NORMAL>(
		    E_SMS_EFFECT_ONETIME_NORMAL(0xA2), &kukku->mPosition,
		    (const void*)nullptr, JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));

		spine->pushAfterCurrent(&TNerveSmallEnemyDie::theNerve());
		return TRUE;
	}

	JGeometry::TVec3<f32> vel(kukku->mVelocity);

	vel *= kukku->getSaveParams()->mAirFric.get();
	vel.y += kukku->getSaveParams()->mUpperVelocityY.get();

	bool landed = false;

	if (-0.1f < vel.y) {
		vel.y  = 0.0f;
		landed = true;
	}

	kukku->mVelocity = vel;

	if (landed) {
		spine->pushAfterCurrent(&TNerveKukkuRecoverGraph::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveKukkuPostFall, TLiveActor)
{
	TKukku* kukku = (TKukku*)spine->getBody();

	if (spine->getTime() == 0) {
		kukku->getMActor()->setBck("tori_back");
		kukku->setCurAnmSound();

		// mVelocity = (0,0,0); the target goes through a temporary (the
		// lwz/stw round trip), so a local + operator= is used.
		JGeometry::TVec3<f32> zero;
		zero.zero();
		kukku->mVelocity = zero;
	}

	if (kukku->checkCurAnmEnd(0)) {
		if (kukku->getSaveParams()->mHabatakiTimer.get() < spine->getTime()) {
			spine->pushAfterCurrent(&TNerveKukkuGraphWander::theNerve());
			return TRUE;
		}
	}

	return FALSE;
}

DEFINE_NERVE(TNerveKukkuRecoverGraph, TLiveActor)
{
	TKukku* kukku = (TKukku*)spine->getBody();

	if (spine->getTime() == 0) {
		kukku->getMActor()->setBck("tori_back");
		kukku->setCurAnmSound();

		JGeometry::TVec3<f32> zero;
		zero.zero();
		kukku->mVelocity = zero;
	}

	if (kukku->getSaveParams()->mHabatakiTimer.get() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveKukkuGraphWander::theNerve());
		return TRUE;
	}

	kukku->updateRotation();

	JGeometry::TVec3<f32> velocity;
	kukku->calcMomentum(kukku->getSaveParams()->mMarchSpeed.get());
	kukku->mLinearVelocity = velocity;

	return FALSE;
}
