#include <Enemy/BathtubKiller.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/EffectObj.hpp>
#include <Enemy/KoopaJr.hpp>
#include <Map/Map.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjCorona.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/MarioStatus.hpp>
#include <Player/WaterGun.hpp>
#include <System/FlagManager.hpp>
#include <System/Particles.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <M3DUtil/MActor.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// Every site that asks "am I already dying?" goes through the BKIsDying chain
// below. The map records no symbol for it, so these are TU-local inlines, but
// the depth is pinned by the ROM: theNerve() is called at the comparisons
// (depth 4) and expanded at the push sites with the TNerveBase constructor
// left as a call (depth 2), and each comparison is materialised as a bool.

static const char* bathtubkiller_bastable[] = {
	"/scene/bathtubkiller/bas/bathtubdownkiller_down1.bas",
	nullptr,
	nullptr,
};

TBathtubKillerPersonality::TBathtubKillerPersonality() { }

void TBathtubKillerPersonality::makeFast(const TBathtubKillerParams* params)
{
	mAccelerationQuatRate = params->fastAccelerationQuatRate.get();
	mChaseAcceleration = params->fastChaseAcceleration.get();
	mChaseSpeed = params->fastChaseSpeed.get();
	mInitialSpeed = params->fastInitialSpeed.get();
	mDeadPeriod = params->fastDeadPeriod.get();
}

void TBathtubKillerPersonality::makeShine(const TBathtubKillerParams* params)
{
	mAccelerationQuatRate = params->shineAccelerationQuatRate.get();
	mChaseAcceleration = params->shineChaseAcceleration.get();
	mChaseSpeed = params->shineChaseSpeed.get();
	mInitialSpeed = params->shineInitialSpeed.get();
	mDeadPeriod = params->shineDeadPeriod.get();
}

void TBathtubKillerPersonality::makeNormal(const TBathtubKillerParams* params)
{
	mAccelerationQuatRate = params->mSLAccelerationQuatRate.get();
	mChaseAcceleration = params->mSLChaseAcceleration.get();
	mChaseSpeed = params->mSLChaseSpeed.get();
	mInitialSpeed = params->mSLInitialSpeed.get();
	mDeadPeriod = params->mSLDeadPeriod.get();
}

TBathtubKillerParams::TBathtubKillerParams(const char* prm)
    : TSmallEnemyParams(prm)
    , PARAM_INIT(fastAccelerationQuatRate, 0.05f)
    , PARAM_INIT(fastChaseAcceleration, 0.2f)
    , PARAM_INIT(fastChaseSpeed, 15.0f)
    , PARAM_INIT(fastInitialSpeed, 50.0f)
    , PARAM_INIT(fastDeadPeriod, 1800)
    , PARAM_INIT(shineAccelerationQuatRate, 0.05f)
    , PARAM_INIT(shineChaseAcceleration, 0.2f)
    , PARAM_INIT(shineChaseSpeed, 15.0f)
    , PARAM_INIT(shineInitialSpeed, 50.0f)
    , PARAM_INIT(shineDeadPeriod, 1800)
    , PARAM_INIT(mushroomProbability, 0.05f)
    , PARAM_INIT(mSLColorChangeRateDelta, 0.05f)
    , PARAM_INIT(mSLAccelerationQuatRate, 0.05f)
    , PARAM_INIT(mSLChaseAcceleration, 0.2f)
    , PARAM_INIT(mSLChaseSpeed, 15.0f)
    , PARAM_INIT(mSLInitialSpeed, 20.0f)
    , PARAM_INIT(mSLDeadPeriod, 1800)
    , PARAM_INIT(mSLStraightDistance, 50.0f)
    , PARAM_INIT(mSLChaseMinY, 50.0f)
    , PARAM_INIT(mSLChaseMaxY, 100.0f)
    , PARAM_INIT(mSLAboidDistanceY, 0.05f)
    , PARAM_INIT(mSLAboidDistance, 500.0f)
    , PARAM_INIT(mSLChaseDistanceY, 200.0f)
    , PARAM_INIT(mSLChaseDistance, 1000.0f)
    , PARAM_INIT(mSLTrampleVelocity, 100.0f)
    , PARAM_INIT(mSLFlyingSpeedMax, 200.0f)
    , PARAM_INIT(mSLFlyingGravityY, 0.1f)
    , PARAM_INIT(mSLBombRange, 300.0f)
    , PARAM_INIT(aboidAngle, 10.0f)
    , PARAM_INIT(mSLChaseStraightPeriod, 360)
    , PARAM_INIT(mSLSmokeInterval, 3)
    , PARAM_INIT(mSLLaunchingPeriod, 360)
    , PARAM_INIT(noCollisionAmongKillers, 360)
{
	TParams::load(mPrmPath);

	fastAccelerationQuatRate.set(0.03f);
	fastChaseAcceleration.set(0.1f);
	fastChaseSpeed.set(10.0f);
	fastInitialSpeed.set(14.0f);
	fastDeadPeriod.set(720);
	shineAccelerationQuatRate.set(0.03f);
	shineChaseAcceleration.set(0.03f);
	shineChaseSpeed.set(7.0f);
	shineInitialSpeed.set(12.0f);
	shineDeadPeriod.set(2400);
	mSLAccelerationQuatRate.set(0.03f);
	mSLChaseAcceleration.set(0.1f);
	mSLChaseSpeed.set(8.0f);
	mSLInitialSpeed.set(12.0f);
	mSLDeadPeriod.set(1440);
	mSLColorChangeRateDelta.set(0.16f);
	mSLStraightDistance.set(400.0f);
	mSLChaseMinY.set(50.0f);
	mSLChaseMaxY.set(100.0f);
	mSLAboidDistanceY.set(100.0f);
	mSLAboidDistance.set(100.0f);
	mSLChaseDistanceY.set(400.0f);
	mSLDamageRadius.set(120);
	mSLDamageHeight.set(100);
	mSLAttackRadius.set(100);
	mSLAttackHeight.set(90);
	mSLTrampleVelocity.set(500.0f);
	mSLFlyingSpeedMax.set(200.0f);
	mSLFlyingGravityY.set(0.06f);
	mSLBombRange.set(500.0f);
	aboidAngle.set(5.0f);
	mSLChaseStraightPeriod.set(420);
	mSLSmokeInterval.set(4);
	mSLLaunchingPeriod.set(240);
	noCollisionAmongKillers.set(480);
	mushroomProbability.set(0.3f);
}

TBathtubKiller::TBathtubKiller(const char* name)
    : TSmallEnemy(name)
    , unk1CC(nullptr)
{
}

void TBathtubKiller::init(TLiveManager* manager)
{
	TSmallEnemy::init(manager);
	mActorType = 0x08000024;
	unk150    = 0x11;
	onLiveFlag(LIVE_FLAG_UNK10);
	onLiveFlag(LIVE_FLAG_DEAD);
	onLiveFlag(LIVE_FLAG_UNK8);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	unk194 = 0;
	resetBathtubKiller();
}

// TODO: 87.6%, frame exact; the known-open this-vs-pool-base rotation
// (retail: string base r31, this r30, new/nose r27). Inert: u32/u16 indices,
// a named keeper local.
void TBathtubKiller::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 2);
	mMActor = mMActorKeeper->createMActor("bathtubkiller_model1.bmd", 0);
	mMActorKeeper->createMActor("bathtubdownkiller_model1.bmd", 3);
	s32 nose = getActorKeeper()->getMActor("bathtubkiller_model1.bmd")
	               ->getModel()
	               ->getModelData()
	               ->getMaterialName()
	               ->getIndex("_nosemat1");
	s32 eyes = getActorKeeper()->getMActor("bathtubkiller_model1.bmd")
	               ->getModel()
	               ->getModelData()
	               ->getMaterialName()
	               ->getIndex("_eyesmat1");
	s32 body = getActorKeeper()->getMActor("bathtubkiller_model1.bmd")
	               ->getModel()
	               ->getModelData()
	               ->getMaterialName()
	               ->getIndex("_body1");
	SMS_InitPacket_OneTevColor(
	    getActorKeeper()->getMActor("bathtubkiller_model1.bmd")->getModel(),
	    nose, GX_TEVREG0, &unk1E0);
	SMS_InitPacket_OneTevColor(
	    getActorKeeper()->getMActor("bathtubkiller_model1.bmd")->getModel(),
	    eyes, GX_TEVREG0, &unk1E8);
	SMS_InitPacket_OneTevColor(
	    getActorKeeper()->getMActor("bathtubkiller_model1.bmd")->getModel(),
	    body, GX_TEVREG0, &unk1D8);
	SMS_InitPacket_OneTevColor(
	    getActorKeeper()->getMActor("bathtubdownkiller_model1.bmd")->getModel(),
	    0, GX_TEVREG0, &unk1F0);
}

void TBathtubKiller::reset()
{
	TSmallEnemy::reset();
	offLiveFlag(LIVE_FLAG_DEAD);
	offLiveFlag(LIVE_FLAG_UNK8);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	offHitFlag(HIT_FLAG_CANNOT_ATTACK);
	offHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	resetBathtubKiller();
}

void TBathtubKiller::resetBathtubKiller()
{
	JGeometry::TVec3<f32> zero;

	mSpine->initWith(&TNerveBathtubKillerWander::theNerve());
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	mTimers[0] = 0;
	mTimers[1] = 0;
	mTimers[2] = 0;
	mTimers[3] = 0;
	mTimers[4] = 0;
	mQuat.set(0.0f, 0.0f, 0.0f, 1.0f);
	zero.set(0.0f, 0.0f, 0.0f);
	mVelocity.set(zero);
	mAcceleration.set(0.0f, 0.0f, 0.0f);
	unk21C = 0;
	unk1D4 = 0;

	if (unk194 == 1) {
		unk1D8.r = 50;
		unk1D8.g = 70;
		unk1D8.b = 160;
		unk1D8.a = 0;
		unk1E0 = unk1D8;
		unk1E8 = unk1D8;
		unk1F0 = unk1D8;
		mPersonality.makeShine(getSaveParam2());
	} else {
		unk1D8.r = 0;
		unk1D8.g = 0;
		unk1D8.b = 0;
		unk1D8.a = 0;
		unk1E0 = unk1D8;
		unk1E8 = unk1D8;
		unk1F0 = unk1D8;
		if (unk194 == 2) {
			mPersonality.makeFast(getSaveParam2());
		} else {
			mPersonality.makeNormal(getSaveParam2());
		}
	}

	unk1FC = 0.0f;
	unk1F8 = getSaveParam2()->mSLColorChangeRateDelta.get();
	mTimers[0] = mPersonality.mDeadPeriod;
	mTimers[1] = getSaveParam2()->mSLLaunchingPeriod.get();
	mTimers[3] = getSaveParam2()->noCollisionAmongKillers.get();
	unk200 = getSaveParam2()->mSLChaseMinY.get();
	unk204 = getSaveParam2()->mSLChaseMaxY.get();
	if (unk194 == 2) {
		int choice = 4.0f * MsRandF();
		f32 heightOffset = 0.0f;
		if (choice == 0)
			heightOffset = 120.0f;
		else if (choice == 1)
			heightOffset = 240.0f;
		unk200 += heightOffset;
		unk204 += heightOffset;
	}
}

void TBathtubKiller::generateItemBathtubKiller()
{
	if (unk194 == 1) {
		TMapObjBase* item = nullptr;
		TBathtubKillerManager* manager = (TBathtubKillerManager*)mManager;
		int lives = TFlagManager::getInstance()->getFlag(0x20001);
		if (SMS_GetMarioWaterGun()->getCurrentWater() == 0) {
			item = gpItemManager->makeObjAppear(
			    mPosition.x, mPosition.y, mPosition.z, 0x20000002, true);
		} else {
			if (manager->mInitialLives == lives
			    && manager->mMushroomDropCount < 7) {
				manager->generateMushroom(mPosition);
				++manager->mMushroomDropCount;
			} else if (lives <= manager->mInitialLives + 1
			           && unk1CC->getNumGripsDead() == 3
			           && !manager->mDroppedFinalMushroom) {
				manager->generateMushroom(mPosition);
				manager->mDroppedFinalMushroom = true;
			}
		}
		if (!item)
			item = gpItemManager->makeObjAppear(
			    mPosition.x, mPosition.y, mPosition.z, 0x20000002, true);
		if (item && item->getActorType() == 0x20000002) {
			JPABaseEmitter* emitter = gpMarioParticleManager->emit(
			    PARTICLE_MS_ENM_DISAP_A_W, &item->mPosition, 0, nullptr);
			if (emitter)
				emitter->setGlobalScale(item->getScaling());
			emitter = gpMarioParticleManager->emit(
			    PARTICLE_MS_ENM_DISAP_B, &item->mPosition, 0, nullptr);
			if (emitter)
				emitter->setGlobalScale(item->mScaling);
		}
	}
}

void TBathtubKiller::killBathtubKiller()
{
	unk21C = 0;
	onLiveFlag(LIVE_FLAG_DEAD);
	stopAnmSound();
}

// TODO: the map records 0x14c for this and for explodeBathtubKiller alike; ours
// are 0xd4 and 0xf8, while the inlined copies in the Break/Explosion nerves are
// instruction-exact. Explode with generateExplosion called rather than expanded
// would be 0xd4 too, so retail's two bodies are probably the same shape (setDead
// expanded, one bl, onHitFlag) plus 0x78 of common code the nerves never show
// (an effect or sound?); nothing in the binary pins it, so it is not guessed.
void TBathtubKiller::breakBathtubKiller()
{
	setDeadBathtubKillerAnm();
	generateItemBathtubKiller();
	onHitFlag(HIT_FLAG_NO_COLLISION);
}

void TBathtubKiller::explodeBathtubKiller()
{
	setDeadBathtubKillerAnm();
	generateExplosion();
	onHitFlag(HIT_FLAG_NO_COLLISION);
}

static inline bool BKIsExploding(TBathtubKiller* k)
{
	return k->mSpine->getCurrentNerve() == &TNerveBathtubKillerExplosion::theNerve();
}

static inline bool BKIsBreaking(TBathtubKiller* k)
{
	return k->mSpine->getCurrentNerve() == &TNerveBathtubKillerBreak::theNerve();
}

static inline bool BKIsDying(TBathtubKiller* k)
{
	return BKIsExploding(k) || BKIsBreaking(k);
}

static inline void BKPushBreak(TBathtubKiller* k)
{
	if (!BKIsDying(k))
		k->mSpine->pushNerve(&TNerveBathtubKillerBreak::theNerve());
}

static inline void BKPushExplosion(TBathtubKiller* k)
{
	if (!BKIsDying(k))
		k->mSpine->pushNerve(&TNerveBathtubKillerExplosion::theNerve());
}

// TODO: the final `nextPos - mPosition` temporary sits at 0x60, retail 0x4c.
// Inert or worse: copy-init nextPos, mVelocity = / .set(x, y, z), zero(),
// add(), sub(a, b), -= in place, a named copy of mPosition, velocity order.
// iro.py: retail creates the difference temporary after the five nerve/operator= pointer temps (@3084-@3099), i.e. at
// inline-expansion time, where ours is the parse-time return temporary @1136 of TVec3::operator-.
void TBathtubKiller::bind()
{
	JGeometry::TVec3<f32> nextPos = mPosition;
	nextPos += mLinearVelocity;
	nextPos += mVelocity;

	mVelocity += mAcceleration;

	if (!BKIsDying(this)) {
		mGroundHeight = gpMap->checkGround(nextPos.x, nextPos.y + mHeadHeight,
		                                   nextPos.z, &mGroundPlane);
		mGroundHeight += 1.0f;
		if (nextPos.y <= 0.05f + mGroundHeight) {
			BKPushExplosion(this);
			mAcceleration.set(0.0f, 0.0f, 0.0f);
			mVelocity.set(mAcceleration);
			nextPos.y = mGroundHeight;
		}
		if (gpMap->isTouchedOneWallAndMoveXZ(&nextPos.x,
		                                     nextPos.y + mHeadHeight,
		                                     &nextPos.z, mBodyRadius)) {
			BKPushExplosion(this);
		}
	}

	mLinearVelocity = nextPos - mPosition;
}

// TODO: 98.5%, frame exact. Ours holds &mPosition in r29 across the distance
// and the sound call where retail rematerialises it; every raw/accessor
// combination over the five mPosition sites is inert or shrinks the frame by
// 8/0x10.
// c-m16: raw `mPosition.distance(SMS_GetMarioPos())` plus `&mPosition` for
// the sound fixes the registers (98.6, only a load/fmuls swap left) but the
// frame is then 0x10 short; `*gpMarioPos` there is 0x18 short.
void TBathtubKiller::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);

	if (unk1CC == nullptr)
		unk1CC = (TBathtub*)JDrama::TNameRefGen::search2("バスタブ");

	if ((cue & CUE_MOVE) && !checkLiveFlag(LIVE_FLAG_DEAD)) {
		updateTimers();
		if (mTimers[0] <= 0) {
			BKPushExplosion(this);
		}
		if (!gpMap->isInArea(getPosition().x, getPosition().z))
			killBathtubKiller();
		if (unk1CC->unk29A != 0)
			killBathtubKiller();
	}

	if ((cue & CUE_CALC_ANIM) && !checkLiveFlag(LIVE_FLAG_DEAD)) {
		if (!BKIsDying(this)) {
			makeNoseColor();
			unk1D4++;
			if (unk1D4 >= getSaveParam2()->mSLSmokeInterval.get()) {
				unk1D4 = 0;
				unk220.setQT(mQuat, mPosition);
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    MAP_MAP_MS_KP_KILL_SMOKE, unk220, 1, this);
			}
			f32 distToMario = getPosition().distance(*gpMarioPos);
			gpMSound->startSoundActorWithInfo(MSD_SE_EN_KILLER_FLY,
			                                  getPosition(), nullptr,
			                                  distToMario, 0, 0, nullptr, 0,
			                                  4);
		}
	}
}

void TBathtubKiller::makeNoseColor()
{
	if (unk194 == 2) {
		unk1FC += unk1F8;
		if (unk1FC > 1.0f) {
			unk1FC = 1.0f;
			unk1F8 = -getSaveParam2()->mSLColorChangeRateDelta.get();
		}
		if (unk1FC < 0.0f) {
			unk1FC = 0.0f;
			unk1F8 = getSaveParam2()->mSLColorChangeRateDelta.get();
		}
		unk1E0.r = (u8)(255.0f * unk1FC);
	}
}

f32 TBathtubKiller::getBathtubY()
{
	return (*unk1CC->getRootJointMtx())[1][3];
}

void TBathtubKiller::makeInitialVelocity(JGeometry::TVec3<f32> velocity)
{
	f32 speed = velocity.length();
	f32 speedMax = getSaveParam2()->mSLFlyingSpeedMax.get();
	if (speed > speedMax) {
		velocity.normalize();
		velocity.scale(speedMax);
	}
	mVelocity.set(velocity);

	velocity.setLength(1.0f);
	JGeometry::TVec3<f32> forward;
	mQuat.getZDir(forward);
	JGeometry::TQuat4<f32> aim;
	aim.setRotate(forward, velocity, 1.0f);
	mQuat.mul(aim, mQuat);
}

// UNUSED (0x78), inlined in the Wander nerve: the gravity pull and a
// makeQuat with fixed rates is exactly 0x78 out of line, and inlining it moves
// the makeQuat argument copy below the nerve's isAttackable/canChase objects
// as in retail.
void TBathtubKiller::moveParabolic()
{
	mAcceleration.set(0.0f, -getGravityY(), 0.0f);
	makeQuat(mVelocity, 1.0f, 0.1f);
}

// TODO: every instruction present; frame 0x88 against retail 0xa8 and the
// normalised chase vector stays in f27-f29 where retail drops it to volatiles.
// A second vector for normalize(v), setLength(1.0f), operator- and
// normalising mAcceleration in place are all inert or worse. Of these,
// `unit.normalize(toTarget)` alone moves the result into volatiles as retail
// does, but frame 0x90 and FPR numbering (retail minY f29) stay off.
void TBathtubKiller::moveChasing()
{
	JGeometry::TVec3<f32> target = *gpMarioPos;
	f32 minY = unk200 + getBathtubY();
	f32 maxY = unk204 + getBathtubY();
	target.y = 0.5f * (minY + maxY);

	JGeometry::TVec3<f32> toTarget;
	toTarget.sub(target, mPosition);
	toTarget.normalize();
	mAcceleration.scale(mPersonality.mChaseAcceleration, toTarget);
	makeAccelerationQuat();

	JGeometry::TVec3<f32> dir;
	mQuat.getZDir(dir);
	dir.normalize();
	if (mPosition.y > maxY)
		dir.y = 0.0f >= dir.y ? dir.y : 0.0f;
	if (mPosition.y < minY)
		dir.y = 0.0f >= dir.y ? 0.0f : dir.y;
	mVelocity.scale(mPersonality.mChaseSpeed, dir);
}

// Retail calls makeQuat here directly: the argument copy then sits above
// `dir` (an inlined callee's temporaries are laid out last-first), where the
// makeVelocityQuat level put it below.
// The forward vector is written out rather than taken from TQuat4::getZDir:
// the header's named components leave dead words below `dir` and load
// 1.0f late, where retail's frame (0x48 in the Straight nerve) has neither.
void TBathtubKiller::moveStraight()
{
	JGeometry::TVec3<f32> dir;
	dir.set(mQuat.x * mQuat.z * 2.0f + mQuat.w * mQuat.y * 2.0f,
	        mQuat.y * mQuat.z * 2.0f - mQuat.w * mQuat.x * 2.0f,
	        1.0f - mQuat.x * mQuat.x * 2.0f - mQuat.y * mQuat.y * 2.0f);
	dir.y = 0.0f;
	dir.normalize();
	dir.scale(mPersonality.mChaseSpeed);
	mVelocity.set(dir);
	makeQuat(mVelocity, mPersonality.mAccelerationQuatRate, 0.1f);
}

void TBathtubKiller::makeVelocityQuat()
{
	makeQuat(mVelocity, mPersonality.mAccelerationQuatRate, 0.1f);
}

void TBathtubKiller::makeAccelerationQuat()
{
	makeQuat(mAcceleration, mPersonality.mAccelerationQuatRate, 0.1f);
}

// TODO: 90.8%, frame 0x218 against the ROM's 0x1e0. This is the open
// JGQuat4.hpp item already recorded in that header: an inlined
// TQuat4::setRotate keeps its two TQuat4 temporaries' stack homes at every
// expansion site, and this function has three of them.
// 2026-09-27 (95.05): every r1 slot is uniformly 0x10 low, so the missing object is one 16-byte inline temporary
// below all live ones (the tiltQuat.rotate expansion, as in Kazekun doAttackPose); the rest is f24/f25 param colouring.
void TBathtubKiller::makeQuat(JGeometry::TVec3<f32> axis, f32 moveAmountY,
                              f32 moveAmountX)
{
	JGeometry::TVec3<f32> normAxis = axis;
	normAxis.normalize();

	JGeometry::TVec3<f32> forward;
	mQuat.getZDir(forward);

	JGeometry::TVec3<f32> up;
	mQuat.getYDir(up);

	JGeometry::TQuat4<f32> steer;
	steer.setRotate(forward, normAxis, moveAmountY);
	mQuat.mul(steer, mQuat);

	// Y-axis rotation
	JGeometry::TVec3<f32> right;
	right.cross(forward, JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f));
	if (right.length() > 0.0f) {
		right.normalize();

		JGeometry::TQuat4<f32> tiltQuat;
		tiltQuat.setRotate(right, M_PI / 2.0f);

		JGeometry::TVec3<f32> curUp;
		tiltQuat.rotate(forward, curUp);

		steer.setRotate(up, curUp, moveAmountX);
		mQuat.mul(steer, mQuat);
	}

	mQuat.normalize();
}

// TODO: dead in the ROM (UNUSED 0x40c) and not reconstructed. The name and the
// size say it is makeQuat's sibling that spins the killer about its own forward
// axis instead of pitching it, but nothing in the TU inlines it, so there is no
// evidence for the body.
void TBathtubKiller::makeScrewQuat(JGeometry::TVec3<f32> axis, f32 moveAmountY,
                                   f32 moveAmountX)
{
	JGeometry::TVec3<f32> normAxis = axis;
	normAxis.normalize();

	JGeometry::TVec3<f32> forward;
	mQuat.getZDir(forward);

	JGeometry::TQuat4<f32> steer;
	steer.setRotate(forward, normAxis, moveAmountY);
	mQuat.mul(steer);

	JGeometry::TQuat4<f32> screw;
	screw.setRotate(forward, moveAmountX);
	mQuat.mul(screw);

	mQuat.normalize();
}

f32 TBathtubKiller::getGravityY() const
{
	return getSaveParam2()->mSLFlyingGravityY.get();
}

void TBathtubKiller::calcRootMatrix()
{
	TPosition3f mtx;
	mtx.setQT(mQuat, mPosition);
	getModel()->setBaseScale(mScaling);
	getModel()->setBaseTRMtx(mtx);
}

BOOL TBathtubKiller::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SUPER_HIP_DROP
	    || message <= HIT_MESSAGE_HIP_DROP) {
		BKPushBreak(this);
		return TRUE;
	}

	if (message == HIT_MESSAGE_UNKA) {
		BKPushExplosion(this);
		return TRUE;
	}

	if (message == HIT_MESSAGE_UNKD) {
		kill();
		return TRUE;
	}

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		behaveToWater(sender);
		return TRUE;
	}

	return FALSE;
}

void TBathtubKiller::attackToMario()
{
	if (!BKIsDying(this) && gpMarioPos->y < getPosition().y) {
		mSpine->pushNerve(&TNerveBathtubKillerExplosion::theNerve());
		SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
		SMS_ThrowMario(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f), 60.0f);
		unk21C = 1;
	}
}

bool TBathtubKiller::isCollidMove(THitActor* other)
{
	if (BKIsDying(this))
		return false;

	// Another killer bursting next to us.
	if (other->isActorType(0x08000029)) {
		BKPushExplosion(this);
		return true;
	}

	if (other->isActorType(0x08000021) || other->isActorType(0x0800002A)
	    || other->isActorType(0x0800002C)) {
		BKPushExplosion(this);
		other->receiveMessage(this, HIT_MESSAGE_ATTACK);
		return true;
	}

	// Two killers launched together are allowed to overlap for a while.
	if (other->isActorType(0x08000024) && mTimers[3] <= 0) {
		BKPushExplosion(this);
		return true;
	}

	return true;
}

void TBathtubKiller::behaveToWater(THitActor* water) { BKPushBreak(this); }

const char** TBathtubKiller::getBasNameTable() const
{
	return bathtubkiller_bastable;
}

void TBathtubKiller::setNormalBathtubKillerAnm()
{
	mMActor = mMActorKeeper->getMActor("bathtubkiller_model1.bmd");
	setBckAnm(1);
}

void TBathtubKiller::setChaseBathtubKillerAnm()
{
	mMActor = mMActorKeeper->getMActor("bathtubkiller_model1.bmd");
	setBckAnm(1);
}

void TBathtubKiller::setStraightBathtubKillerAnm()
{
	mMActor = mMActorKeeper->getMActor("bathtubkiller_model1.bmd");
	setBckAnm(2);
}

void TBathtubKiller::setDeadBathtubKillerAnm()
{
	mMActor = mMActorKeeper->getMActor("bathtubdownkiller_model1.bmd");
	setBckAnm(0);
	mQuat.set(0.0f, 0.0f, 0.0f, 1.0f);
	JGeometry::TVec3<f32>& accel = mAcceleration;
	accel.set(0.0f, 0.0f, 0.0f);
	mVelocity = JGeometry::TVec3<f32>(0, 0, 0);
	onLiveFlag(LIVE_FLAG_UNK8);
	unk1E0 = unk1D8;
}

// The ROM materialises each timer's address in its own register before the
// test, the unrolled-loop shape of an indexed array, and the map's 0x7c
// matches this loop exactly.
void TBathtubKiller::updateTimers()
{
	for (int i = 0; i < 5; ++i)
		if (mTimers[i] > 0)
			mTimers[i]--;
}

// TODO: out of line 0x1a4 against the map's 0x19c. Retail measures Mario's
// distance first and keeps bathtubPos.y/.z live in f5/f6 across both sqrt
// blocks; we reload them. Naming the Mario distance fixes the order but costs
// the Chase nerve 8 bytes of frame; flipped comparison, top declarations and
// a named reference to the tub position were inert. `f32 marioDist` named
// before the test makes the Wander nerve instruction-exact (block placement
// only) but costs Chase 8; `100.0f + mario < my` fixes the order, not the compare.
bool TBathtubKiller::isAttackable()
{
	if (!unk1CC->isKillerAttackable())
		return false;

	// A shine killer gives up once Mario is closer to the tub than it is.
	if (unk194 == 2) {
		JGeometry::TVec3<f32> marioPos = SMS_GetMarioPos();
		marioPos.y = 0.0f;
		JGeometry::TVec3<f32> myPos = mPosition;
		myPos.y = 0.0f;
		JGeometry::TVec3<f32> bathtubPos = unk1CC->mPosition;
		bathtubPos.y = 0.0f;
		// Retail computes Mario's distance first and keeps both results as
		// dead named words above the three vectors (Chase closed).
		f32 marioDist = marioPos.distance(bathtubPos);
		f32 myDist = myPos.distance(bathtubPos);
		if (myDist > 100.0f + marioDist)
			return false;
	}

	return true;
}

bool TBathtubKiller::isAboided()
{
	if (getPosition().y > 5.0f + (unk204 + getBathtubY()))
		return false;

	JGeometry::TVec3<f32> marioPos = SMS_GetMarioPos();
	JGeometry::TVec3<f32> myPos = getPosition();
	f32 heightDiff = fabsf(marioPos.y - myPos.y);
	marioPos.y = 0.0f;
	myPos.y = 0.0f;
	f32 diffY;
	f32 diffX;
	f32 diffZ = marioPos.z - myPos.z;
	diffX = marioPos.x - myPos.x;
	diffY = marioPos.y - myPos.y;
	f32 squaredX = diffX * diffX;
	f32 squaredY = diffY * diffY;
	f32 squaredZ = diffZ * diffZ;
	f32 distXZ = JGeometry::TUtil<f32>::sqrt(squaredX + squaredY + squaredZ);

	if (heightDiff > getSaveParam2()->mSLAboidDistanceY.get())
		if (distXZ <= getSaveParam2()->mSLAboidDistance.get())
			return true;

	if (distXZ > getSaveParam2()->mSLStraightDistance.get())
		return false;

	if (SMS_GetMarioStatus() == MARIO_STATUS_HANGING) {
		mTimers[4] = 240;
		onHitFlag(HIT_FLAG_NO_COLLISION);
		return true;
	}

	// The ROM holds the scalar deltas across the preceding tests, then
	// materialises this vector only for the direction calculation.
	JGeometry::TVec3<f32> toMario;
	toMario.set(diffX, diffY, diffZ);
	toMario.normalize();
	TDirectionCalc marioDir(toMario);

	JGeometry::TVec3<f32> forward;
	mQuat.getZDir(forward);
	TDirectionCalc myDir(forward);

	f32 direction = myDir.absDirection(marioDir.get());
	f32 aboidAngle = TDirectionCalc::d2r(getSaveParam2()->aboidAngle.get());
	if (direction > aboidAngle)
		return false;

	return true;
}

bool TBathtubKiller::canChase()
{
	if (mTimers[1] > 0)
		return false;

	TBathtubKillerParams* params = getSaveParam2();
	f32 chaseDistanceY = params->mSLChaseDistanceY.get();
	f32 minY = unk200 + getBathtubY();
	if (mPosition.y > minY + chaseDistanceY)
		return false;

	return true;
}

void TBathtubKiller::generateExplosion()
{
	TEffectExplosion* explosion
	    = (TEffectExplosion*)gpConductor->makeOneEnemyAppear(
	        mPosition, "エフェクト爆発マネージャー", 1);
	if (explosion)
		explosion->generate(mPosition, mScaling);
}

// TODO: frame exact; moveParabolic's makeQuat copy sits 8 above retail's
// 0x38. Retail creates two more words before it and two fewer after, as if
// the copy were one level deeper (below isAttackable's depth-2 TVec3 bindings);
// named gravity, a named velocity copy and a raw param read all add words.
DEFINE_NERVE(TNerveBathtubKillerWander, TLiveActor)
{
	TBathtubKiller* killer = (TBathtubKiller*)spine->getBody();
	if (spine->getTime() == 0)
		killer->setNormalBathtubKillerAnm();

	if (!killer->isAttackable()) {
		spine->pushAfterCurrent(&TNerveBathtubKillerStraight::theNerve());
		return TRUE;
	}

	if (killer->canChase()) {
		spine->pushAfterCurrent(&TNerveBathtubKillerChase::theNerve());
		return TRUE;
	}

	killer->moveParabolic();
	return FALSE;
}

DEFINE_NERVE(TNerveBathtubKillerChase, TLiveActor)
{
	TBathtubKiller* killer = (TBathtubKiller*)spine->getBody();
	if (spine->getTime() == 0)
		killer->setChaseBathtubKillerAnm();

	if (!killer->isAttackable()) {
		spine->pushAfterCurrent(&TNerveBathtubKillerStraight::theNerve());
		return TRUE;
	}

	if (killer->isAboided()) {
		if (killer->unk194 == 1)
			spine->pushAfterCurrent(
			    &TNerveBathtubKillerChaseStraight::theNerve());
		else
			spine->pushAfterCurrent(&TNerveBathtubKillerStraight::theNerve());
		return TRUE;
	}

	killer->moveChasing();
	return FALSE;
}

DEFINE_NERVE(TNerveBathtubKillerChaseStraight, TLiveActor)
{
	TBathtubKiller* killer = (TBathtubKiller*)spine->getBody();
	if (spine->getTime() == 0) {
		killer->setStraightBathtubKillerAnm();
		killer->mTimers[2]
		    = killer->getSaveParam2()->mSLChaseStraightPeriod.get();
	}

	if (!killer->isAttackable()) {
		spine->pushAfterCurrent(&TNerveBathtubKillerStraight::theNerve());
		return TRUE;
	}

	if (killer->mTimers[4] <= 0)
		killer->offHitFlag(HIT_FLAG_NO_COLLISION);

	if (killer->mTimers[2] <= 0) {
		spine->pushAfterCurrent(&TNerveBathtubKillerChase::theNerve());
		return TRUE;
	}

	killer->moveStraight();
	return FALSE;
}

DEFINE_NERVE(TNerveBathtubKillerStraight, TLiveActor)
{
	TBathtubKiller* killer = (TBathtubKiller*)spine->getBody();
	if (spine->getTime() == 0)
		killer->setStraightBathtubKillerAnm();

	if (killer->mTimers[4] <= 0)
		killer->offHitFlag(HIT_FLAG_NO_COLLISION);

	killer->moveStraight();
	return FALSE;
}

DEFINE_NERVE(TNerveBathtubKillerBreak, TLiveActor)
{
	TBathtubKiller* killer = (TBathtubKiller*)spine->getBody();
	if (spine->getTime() == 0)
		killer->breakBathtubKiller();
	if (killer->checkCurAnmEnd(0)) {
		killer->killBathtubKiller();
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBathtubKillerExplosion, TLiveActor)
{
	TBathtubKiller* killer = (TBathtubKiller*)spine->getBody();
	if (spine->getTime() == 0)
		killer->explodeBathtubKiller();
	if (killer->checkCurAnmEnd(0)) {
		killer->killBathtubKiller();
		return TRUE;
	}
	return FALSE;
}

TBathtubKillerManager::TBathtubKillerManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TBathtubKillerManager::load(JSUMemoryInputStream& stream)
{
	// Two discarded `getActiveObjNum()` calls: the ROM reads mParams and
	// compares it against null twice here, with no branch and no use of the
	// result, exactly as in loadAfter.
	getActiveObjNum();
	TSmallEnemyManager::load(stream);
	unk38 = new TBathtubKillerParams("/enemy/bathtubkiller.prm");
	getActiveObjNum();
}

void TBathtubKillerManager::loadAfter()
{
	TSmallEnemyManager::loadAfter();
	TMapObjBaseManager::newAndRegisterObj("mushroom1up");
	TMapObjBaseManager::newAndRegisterObj("mushroom1up");
	mInitialLives = TFlagManager::getInstance()->getFlag(0x20001);
	mMushroom = nullptr;
	mDroppedFinalMushroom = false;
	mMushroomDropCount = 0;
	// The ROM reads mParams and compares it against null here with no branch
	// and no use of the result. `getActiveObjNum()` opens with exactly that
	// test, so a discarded call to it is what is left of whatever used the
	// count; it also accounts for the whole 32-byte frame difference.
	getActiveObjNum();
	static const char* loopFilenames[] = {
		"/scene/map/map/ms_kp_kill_smoke.jpa",
	};
	for (int i = 0; i < 1; ++i)
		SMS_LoadParticle(loopFilenames[i], MAP_MAP_MS_KP_KILL_SMOKE + i);
}

void TBathtubKillerManager::generateMushroom(JGeometry::TVec3<f32> position)
{
	if (!mMushroom || mMushroom->checkLiveFlag(LIVE_FLAG_DEAD))
		mMushroom = gpItemManager->makeObjAppear(
		    position.x, position.y, position.z, 0x20000005, true);
}

int TBathtubKillerManager::countActiveKillers()
{
	int count = 0;
	for (int i = 0; i < getActiveObjNum(); ++i) {
		if (!getObj(i)->checkLiveFlag(LIVE_FLAG_DEAD))
			++count;
	}
	return count;
}

int TBathtubKillerManager::countActiveShineKillers()
{
	int count = 0;
	for (int i = 0; i < getActiveObjNum(); ++i) {
		if (!getObj(i)->checkLiveFlag(LIVE_FLAG_DEAD)
		    && ((TBathtubKiller*)getObj(i))->unk194 == 1)
			++count;
	}
	return count;
}

void TBathtubKillerManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "bathtubkiller_model1.bmd", 0x50230000, 0 },
		{ "bathtubdownkiller_model1.bmd", 0x50210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

TSpineEnemy* TBathtubKillerManager::createEnemyInstance()
{
	return new TBathtubKiller;
}
