#include <Enemy/KoopaJr.hpp>
#include <Enemy/BathtubBinder.hpp>
#include <Enemy/BathtubKiller.hpp>
#include <Enemy/Koopa.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <System/Particles.hpp>
#include <System/FlagManager.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Strategy.hpp>
#include <Strategic/Spine.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MoveBG/MapObjCorona.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/WaterGun.hpp>
#include <math.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// The original calls JGeometry::TUtil<f32>::inv_sqrt(v) out-of-line here
// (bl sqrt__Q29JGeometry8TUtil<f>Ff), with the range guard inside the
// callee. JGUtil.hpp only offers the inline spelling, so MWCC always
// expands these sites and the call never appears.
// FABRICATED: the callee is orig_inv_sqrt, so the `bl` itself still shows as
// one mismatched instruction. Making JGUtil.hpp out-of-line instead was
// measured repo-wide at -32.2 points - see docs/AGENT_MATCHING_TIPS.md.
#pragma dont_inline on
static f32 orig_inv_sqrt(f32 v) {
	return JGeometry::TUtil<f32>::inv_sqrt(v);
}
#pragma dont_inline off


// NOTE: this TU is -inline deferred, so the out-of-line functions below are
// defined in the *reverse* order of mario.MAP's .text layout.

static const char* koopajr_bastable[] = {
	"/scene/koopajr/bas/koopajr_damage.bas",
	"/scene/koopajr/bas/koopajr_shoot.bas",
	nullptr,
	"/scene/koopajr/bas/koopajr_yahoo.bas",
};

static const char* TKoopaJr_jointNameTable[] = {
	"KoopaJr_null", "killer_null00", "killer_null03",
	"killer_null01", "killer_null04",
};

static int TKoopaJr_jointIndexTable[5];

static const char* koopajrsubmarine_bastable[1] = { nullptr };

// ============= TDirectionCalc =============

TDirectionCalc::TDirectionCalc()
    : mDirection(0.0f)
{
}

TDirectionCalc::TDirectionCalc(f32 direction)
    : mDirection(direction)
{
}

TDirectionCalc::TDirectionCalc(JGeometry::TVec3<f32> vec)
{
	makeDirection(vec);
}

void TDirectionCalc::normalize()
{
	mDirection = 0.0f + std::fmodf(6.2831855f + (mDirection - 0.0f), 6.2831855f);
}

#pragma dont_inline on
f32 TDirectionCalc::calcNearerDirection(f32 target)
{
	normalize();
	if (target >= mDirection) {
		if (6.2831855f - (target - mDirection) < target - mDirection)
			target -= 6.2831855f;
	} else {
		if (6.2831855f - (mDirection - target) < mDirection - target)
			target += 6.2831855f;
	}
	return target;
}
#pragma dont_inline off

void TDirectionCalc::sub(f32)
{
	// TODO: UNUSED in the target (0xA0 bytes), not yet reconstructed
}

#pragma dont_inline on
f32 TDirectionCalc::calcTurnDirection(f32 target, f32 speed)
{
	normalize();
	mDirection = 0.0f
	              + std::fmodf(6.2831855f + (mDirection - 0.0f), 6.2831855f);
	if (target >= mDirection) {
		if (6.2831855f - (target - mDirection) < target - mDirection)
			target -= 6.2831855f;
	} else {
		if (6.2831855f - (mDirection - target) < mDirection - target)
			target += 6.2831855f;
	}

	if (target > mDirection) {
		if (target - mDirection < speed)
			speed = target - mDirection;
		return mDirection + speed;
	} else {
		if (mDirection - target < speed)
			speed = mDirection - target;
		return mDirection - speed;
	}
}
#pragma dont_inline off

void TDirectionCalc::makeDirection(JGeometry::TVec3<f32> vec)
{
	mDirection = atan2f(vec.x, vec.z);
}

void TDirectionCalc::calcDirectionVector()
{
	// TODO: UNUSED in the target (0x64 bytes), not yet reconstructed
}

f32 TDirectionCalc::absDirection(f32 target)
{
	mDirection = 0.0f
	              + std::fmodf(6.2831855f + (mDirection - 0.0f), 6.2831855f);
	if (target >= mDirection) {
		if (6.2831855f - (target - mDirection) < target - mDirection)
			target -= 6.2831855f;
	} else {
		if (6.2831855f - (mDirection - target) < mDirection - target)
			target += 6.2831855f;
	}
	return fabsf(mDirection - target);
}

// TODO: operand order of the fmuls is swapped
f32 TDirectionCalc::d2r(f32 degrees) { return degrees * 3.1415927f / 180.0f; }

f32 TDirectionCalc::r2d(f32 radians) { return radians * 180.0f / 3.1415927f; }

// ============= params =============

TKoopaJrParams::TKoopaJrParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLLaunchKillerLimit, 10000.0f)
    , PARAM_INIT(mSLDamageRadius, 1000.0f)
    , PARAM_INIT(mSLDamageHeight, 4000.0f)
    , PARAM_INIT(mSLKoopaJrScale, 1.6f)
    , PARAM_INIT(mSLFastLaunchDistance, 10000.0f)
    , PARAM_INIT(mSLDamagePeriod, 360)
    , PARAM_INIT(mSLLaunchKillerPeriod, 1200)
    , PARAM_INIT(mSLLaunchKillerPeriodFast, 360)
{
	TParams::load(mPrmPath);

	// NOTE: the values loaded from the .prm are overridden right away
	mSLLaunchKillerLimit.set(4200.0f);
	mSLDamageRadius.set(100.0f);
	mSLDamageHeight.set(300.0f);
	mSLKoopaJrScale.set(2.0f);
	mSLFastLaunchDistance.set(4600.0f);
	mSLDamagePeriod.set(240);
	mSLLaunchKillerPeriodFast.set(360);
	mSLLaunchKillerPeriod.set(840);
}

TKoopaJrSubmarineParams::TKoopaJrSubmarineParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(killerTargetDistanceMin, 500.0f)
    , PARAM_INIT(killerTargetDistance, 500.0f)
    , PARAM_INIT(bottomHeight, 0.0f)
    , PARAM_INIT(centerZ, 0.0f)
    , PARAM_INIT(aboidKoopaFlameAngle, 0.31415927f)
    , PARAM_INIT(traceMarioAngle, 0.31415927f)
    , PARAM_INIT(mSLWavePhaseVelocity, 0.31415927f)
    , PARAM_INIT(mSLWaveAmplitudeMin, 0.37699112f)
    , PARAM_INIT(mSLWaveAmplitudeMaxLaunch, 0.37699112f)
    , PARAM_INIT(mSLWaveAmplitudeMax, 0.37699112f)
    , PARAM_INIT(mSLSwingPhaseVelocity, 0.31415927f)
    , PARAM_INIT(mSLSwingAmplitudeMin, 0.37699112f)
    , PARAM_INIT(mSLSwingAmplitudeMax, 0.37699112f)
    , PARAM_INIT(mSLRoundAngleVelocity, 0.05f)
    , PARAM_INIT(mSLRoundDistance, 1.0f)
    , PARAM_INIT(mSLAcceleration, 1.0f)
    , PARAM_INIT(mSLRotationSpeed, 1.0f)
    , PARAM_INIT(mSLSpeedMax, 8.0f)
    , PARAM_INIT(mSLKoopaJrSubmarineScale, 1.6f)
    , PARAM_INIT(mSLDamageRadius, 1000.0f)
    , PARAM_INIT(mSLDamageHeight, 4000.0f)
    , PARAM_INIT(shineKillerProbability0, 0.0f)
    , PARAM_INIT(shineKillerProbability1, 0.0f)
    , PARAM_INIT(mSLKillerIntervalFast, 30)
    , PARAM_INIT(mSLKillerInterval, 30)
{
	TParams::load(mPrmPath);

	// NOTE: the values loaded from the .prm are overridden right away.
	// TODO: the angles are plain sdata2 constants in the target (folded
	// multiples of pi) while the other values go through set()'s reference
	// and land in sdata; the exact original spelling is unknown.
	killerTargetDistanceMin.set(500.0f);
	killerTargetDistance.set(700.0f);
	bottomHeight.set(0.0f);
	centerZ.set(200.0f);
	aboidKoopaFlameAngle.value      = 0.2f * 3.1415927f;
	traceMarioAngle.value           = 0.1f * 3.1415927f;
	mSLWavePhaseVelocity.value      = 0.03f * 3.1415927f;
	mSLWaveAmplitudeMin.value       = 0.04f * 3.1415927f;
	mSLWaveAmplitudeMaxLaunch.value = 0.12f * 3.1415927f;
	mSLWaveAmplitudeMax.value       = 0.1f * 3.1415927f;
	mSLSwingPhaseVelocity.value     = 0.06f * 3.1415927f;
	mSLSwingAmplitudeMin.value      = 0.04f * 3.1415927f;
	mSLSwingAmplitudeMax.value      = 0.18f * 3.1415927f;
	mSLRoundAngleVelocity.set(0.12f);
	mSLRoundDistance.set(2000.0f);
	mSLAcceleration.set(1.0f);
	mSLRotationSpeed.set(1.0f);
	mSLSpeedMax.set(5.0f);
	mSLKoopaJrSubmarineScale.set(2.0f);
	mSLDamageRadius.set(240.0f);
	mSLDamageHeight.set(120.0f);
	shineKillerProbability0.set(0.5f);
	shineKillerProbability1.set(0.125f);
	mSLKillerInterval.set(90);
	mSLKillerIntervalFast.set(30);
}

static int TKoopaJr_getJointIndex(int index)
{
	return TKoopaJr_jointIndexTable[index];
}

// ============= TCallbackHitActor =============

TCallbackHitActor::TCallbackHitActor(const char* name, u32 actorType,
                                     f32 radius, f32 height, THitActor* owner)
    : THitActor(name)
    , mOwner(owner)
{
	initHitActor(actorType, 0, 0, 0.0f, 0.0f, radius, height);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(this);
}

BOOL TCallbackHitActor::receiveMessage(THitActor* sender, u32 message)
{
	return mOwner->receiveMessage(sender, message);
}

// ============= TKoopaJr =============

TKoopaJr::TKoopaJr(const char* name)
    : TSpineEnemy(name)
    , unk15C(nullptr)
    , unk160(nullptr)
    , unk164(nullptr)
    , unk168(nullptr)
    , unk16C(nullptr)
{
	onLiveFlag(LIVE_FLAG_UNK10);
	offLiveFlag(LIVE_FLAG_UNK100);
}

void TKoopaJr::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("koopajr_model.bmd", 0);
	mMActor->setLightType(1);
	initAnmSound();
	f32 height = getParams()->mSLDamageHeight.get();
	initHitActor(0x08000028, 1, 0, 0.0f, 0.0f,
	             getParams()->mSLDamageRadius.get(), height);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	mSpine->initWith(&TNerveKoopaJrWait::theNerve());
	unk16C = (TEnemyManager*)JDrama::TNameRefGen::search(
	    "バスタブキラーマネージャー");
	if (unk168 == nullptr)
		unk168 = (TKoopaJrSubmarineManager*)JDrama::TNameRefGen::search(
		    "クッパジュニアサブマリンマネージャー");
	f32 scale = getParams()->mSLKoopaJrScale.get();
	mScaling.set(scale, scale, scale);
	resetKoopaJr();
}

void TKoopaJr::reset()
{
	// TODO: frame padding. The ROM's frame is 8 bytes larger than ours with an
	// identical instruction stream (MWCC stack-padding bug); no natural source
	// spelling has been found that reproduces it.
	
	
	TSpineEnemy::reset();
	resetKoopaJr();
}

void TKoopaJr::resetKoopaJr()
{
	mSpine->reset();
	unk150 = 0;
	unk154 = 0;
	unk158 = 0;
	unk154 = getParams()->mSLLaunchKillerPeriod.get();
	unk158 = 0;
}

// Reconstructed from the ROM at 0x801141AC.
//
// On the first frame the three cross-references are resolved (the submarine is
// taken from its manager, Bowser from the koopa manager, and the bathtub by
// name), on the `2` cue the damage nerve is armed once the submarine's swing
// amplitude reaches half of the wave amplitude limit, and on the `1` cue the
// three timers tick, the demo nerve is pushed and the facing is turned towards
// Mario.
void TKoopaJr::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (unk164 == nullptr) {
		unk164 = (TKoopaJrSubmarine*)unk168->getObj(0);
		unk164->unk1A0 = this;
	}

	if (unk160 == nullptr) {
		unk160 = (TKoopa*)((TEnemyManager*)JDrama::TNameRefGen::search(
		                       "クッパマネージャー"))
		             ->getObj(0);
	}

	if (unk15C == nullptr) {
		unk15C = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
	}

	if (cue & 2) {
		f32 limit = 0.5f * unk164->getParams()->mSLSwingAmplitudeMax.value;
		if (!(unk164->unk190 < limit))
			startDamageNerve();
	}

	if (cue & 1) {
		if (unk150 > 0)
			--unk150;
		if (unk154 > 0)
			--unk154;
		if (unk158 > 0)
			--unk158;

		// TODO: @hack. The ROM branches on the same flag twice here and both
		// branches are taken, so the demo nerve push below is dead code there.
		// `if (flag && !flag)` is the only spelling found so far that emits
		// the same pair of branches around the block.
		if (unk15C->unk29A && !unk15C->unk29A) {
			if (mSpine->getCurrentNerve() != &TNerveKoopaJrDemo::theNerve())
				mSpine->pushNerve(&TNerveKoopaJrDemo::theNerve());
		}

		if (mSpine->getCurrentNerve() == &TNerveKoopaJrWait::theNerve()) {
			checkNerveKillerLaunchNormal();
			checkNerveKillerLaunchFast();
			checkNerveKillerHit();
		}

		// The ROM materialises the difference into one stack TVec3 and then
		// copies it into a second before the atan2f, so both are named here.
		JGeometry::TVec3<f32> diff;
		diff.x = gpMarioPos->x - mPosition.x;
		diff.y = gpMarioPos->y - mPosition.y;
		diff.z = gpMarioPos->z - mPosition.z;
		diff.y = 0.0f;

		JGeometry::TVec3<f32> toMario = diff;
		mRotation.y = 180.0f * atan2f(toMario.x, toMario.z) / 3.1415927f;
	}

	TSpineEnemy::perform(cue, graphics);
}

void TKoopaJr::calcRootMatrix()
{
	J3DModel* model = getModel();
	if (unk15C->unk29A) {
		MtxPtr mtx = unk15C->getKoopaJrMtxInDemo();
		MTXCopy(mtx, getModel()->getBaseTRMtx());
	} else {
		unk164->getJointTransByIndex(TKoopaJr_getJointIndex(0), &mPosition);
		MsMtxSetXYZRPH(model->getBaseTRMtx(), mPosition.x, mPosition.y,
		               mPosition.z, mRotation.x, mRotation.y, mRotation.z);
	}
	model->setBaseScale(mScaling);
}

void TKoopaJr::startKoopaJrMessage(u32)
{
	// TODO: UNUSED in the target (0x2C bytes), not yet reconstructed
}

void TKoopaJr::emitKoopaJrEffects() { }

void TKoopaJr::updateTimers()
{
	// TODO: UNUSED in the target (0x4C bytes), not yet reconstructed
}

const char** TKoopaJr::getBasNameTable() const { return koopajr_bastable; }

BOOL TKoopaJr::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: frame padding. The ROM's frame is 8 bytes larger than ours with an
	// otherwise identical instruction stream (MWCC stack-padding bug).
	
	
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		gpMarioParticleManager->emit(0xE7, &sender->mPosition, 0, nullptr);
		gpMSound->startSoundSet(0x6802, &mPosition, 0, 0.0f, 0, 0, 4);
		damageKoopaJr();
		return TRUE;
	}

	return FALSE;
}

void TKoopaJr::damageKoopaJr() { startDamageNerve(); }

void TKoopaJr::checkSubmarineSwing()
{
	// TODO: UNUSED in the target (0x274 bytes), not yet reconstructed
}

void TKoopaJr::startDamageNerve()
{
	unk150 = getParams()->mSLDamagePeriod.get();
	if (mSpine->getCurrentNerve() == &TNerveKoopaJrWait::theNerve())
		mSpine->pushNerve(&TNerveKoopaJrDamage::theNerve());

	if (mSpine->getCurrentNerve() == &TNerveKoopaJrLaunch::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveKoopaJrYahoo::theNerve())
		mSpine->setNext(&TNerveKoopaJrDamage::theNerve());
}

void TKoopaJr::checkNerve()
{
	// TODO: UNUSED in the target (0x4A0 bytes), not yet reconstructed
}

void TKoopaJr::checkNerveKillerLaunchNormal()
{
	if (unk154 > 0)
		return;

	s32 n = unk15C->getNumKillerLaunchable();
	if (n == 0)
		return;

	if (n > 8)
		n = 8;

	TKoopaJrSubmarine* sub = unk164;
	sub->unk180 = 0;
	sub->unk184 = n;
	for (int i = 0; i < sub->unk184; ++i)
		sub->unk178[i] = 0;

	if (sub->appearShineKiller(sub->unk184))
		sub->unk178[sub->unk184 - 1] = 1;

	mSpine->pushNerve(&TNerveKoopaJrLaunch::theNerve());
	sub->mSpine->pushNerve(
	    &TNerveKoopaJrSubmarineCannonOpenClose::theNerve());
	sub->setAnimationIndex(0);
}

void TKoopaJr::checkNerveKillerLaunchFast()
{
	if (unk158 > 0)
		return;

	s32 n = unk15C->getNumKillerBurstable();
	if (n == 0)
		return;

	if (n > 8)
		n = 8;

	TKoopaJrSubmarine* sub = unk164;
	sub->unk180 = 0;
	sub->unk184 = n;
	for (int i = 0; i < sub->unk184; ++i)
		sub->unk178[i] = 2;

	if (sub->appearShineKiller(sub->unk184))
		sub->unk178[sub->unk184 - 1] = 1;

	mSpine->pushNerve(&TNerveKoopaJrLaunch::theNerve());
	sub->mSpine->pushNerve(
	    &TNerveKoopaJrSubmarineCannonOpenClose::theNerve());
	sub->setAnimationIndex(0);
}

// dont_inline: the ROM reaches this out of line from perform (bl at
// 0x801145E0); letting MWCC inline the whole enemy scan shifts perform's frame
// and hoists the manager load out of the branch.
#pragma dont_inline on
void TKoopaJr::checkNerveKillerHit()
{
	for (int i = 0; i < unk16C->getActiveObjNum(); ++i) {
		TBathtubKiller* killer = (TBathtubKiller*)unk16C->getObj(i);
		if (killer->unk21C == 1) {
			mSpine->pushNerve(&TNerveKoopaJrYahoo::theNerve());
			break;
		}
	}
}
#pragma dont_inline off

void TKoopaJr::getBathtubY()
{
	// TODO: UNUSED in the target (0x34 bytes), not yet reconstructed
}

DEFINE_NERVE(TNerveKoopaJrWait, TLiveActor)
{
	TKoopaJr* self = (TKoopaJr*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setAnimationIndex(2);
		self->unk154 = self->getParams()->mSLLaunchKillerPeriod.get();
		self->unk158 = self->getParams()->mSLLaunchKillerPeriodFast.get();
	}

	return FALSE;
}

DEFINE_NERVE(TNerveKoopaJrDamage, TLiveActor)
{
	TKoopaJr* self = (TKoopaJr*)spine->getBody();

	if (spine->getTime() == 0)
		self->setAnimationIndex(0);

	if (self->unk150 <= 0 && self->getMActor()->isCurAnmAlreadyEnd(0))
		return TRUE;

	return FALSE;
}

DEFINE_NERVE(TNerveKoopaJrDemo, TLiveActor)
{
	TKoopaJr* self = (TKoopaJr*)spine->getBody();

	if (spine->getTime() == 0)
		self->setAnimationIndex(0);

	return FALSE;
}

DEFINE_NERVE(TNerveKoopaJrLaunch, TLiveActor)
{
	TKoopaJr* self = (TKoopaJr*)spine->getBody();

	if (spine->getTime() == 0)
		self->setAnimationIndex(1);

	if (self->getMActor()->isCurAnmAlreadyEnd(0))
		return TRUE;

	return FALSE;
}

DEFINE_NERVE(TNerveKoopaJrYahoo, TLiveActor)
{
	TKoopaJr* self = (TKoopaJr*)spine->getBody();

	if (spine->getTime() == 0)
		self->setAnimationIndex(3);

	if (self->getMActor()->isCurAnmAlreadyEnd(0))
		return TRUE;

	return FALSE;
}

// ============= TKoopaJrManager =============

TKoopaJrManager::TKoopaJrManager(const char* name)
    : TEnemyManager(name)
{
}

void TKoopaJrManager::createModelData()
{
	// TODO: flags not decoded into J3DMLF_* yet
	static const TModelDataLoadEntry entry[] = {
		{ "koopajr_model.bmd", 0x54220000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TKoopaJrManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
	unk38 = new TKoopaJrParams("/enemy/koopajr.prm");
}

void TKoopaJrManager::loadAfter()
{
	static const char* onetimeFilenames[1] = {
		"/scene/koopajr/jpa/ms_koopajr_killer.jpa",
	};
	for (int i = 0; i < 1; ++i)
		SMS_LoadParticle(onetimeFilenames[i], 0xEF + i);
}

TSpineEnemy* TKoopaJrManager::createEnemyInstance() { return nullptr; }

// ============= TKoopaJrSubmarine =============

// dont_inline: the ROM calls this ctor out of line from
// TKoopaJrSubmarineManager::createEnemyInstance instead of inlining it.
#pragma dont_inline on
TKoopaJrSubmarine::TKoopaJrSubmarine(const char* name)
    : TSpineEnemy(name)
    , unk164(0.0f)
    , unk16C(0.0f)
    , unk188(0.0f)
    , unk1A0(nullptr)
{
	offLiveFlag(LIVE_FLAG_UNK10);
	offLiveFlag(LIVE_FLAG_UNK100);
}
#pragma dont_inline off

void TKoopaJrSubmarine::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	initAnmSound();
	initHitActor(0x08000020, 0, 0, 0.0f, 0.0f,
	             getParams()->mSLDamageRadius.get(),
	             getParams()->mSLDamageHeight.get());
	onHitFlag(HIT_FLAG_NO_COLLISION);
	unk1A4 = new TCallbackHitActor("サブマリンリアボディ", 0x0800002D,
	                               getParams()->mSLDamageRadius.get(),
	                               getParams()->mSLDamageHeight.get(), this);
	unk1A8 = new TCallbackHitActor("サブマリンフロントボディ", 0x0800002D,
	                               getParams()->mSLDamageRadius.get(),
	                               getParams()->mSLDamageHeight.get(), this);
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor = mMActorKeeper->createMActor("LastKoopaJrSubmarine.bmd", 0);
	mMActor->setLightType(1);
	mSpine->initWith(&TNerveKoopaJrSubmarineWait::theNerve());

	JUTNameTab* jointNames = getModel()->getModelData()->getJointName();
	for (int i = 0; i < 5; ++i)
		TKoopaJr_jointIndexTable[i]
		    = jointNames->getIndex(TKoopaJr_jointNameTable[i]);

	f32 scale = getParams()->mSLKoopaJrSubmarineScale.get();
	mScaling.set(scale, scale, scale);
	unk174  = new TBathtubBinder;
	mBinder = unk174;
	resetKoopaJrSubmarine();
}

void TKoopaJrSubmarine::reset()
{
	TSpineEnemy::reset();
	resetKoopaJrSubmarine();
}

void TKoopaJrSubmarine::resetKoopaJrSubmarine()
{
	// TODO: frame padding. The ROM's frame is 8 bytes larger than ours with an
	// otherwise identical instruction stream (MWCC stack-padding bug).
	
	
	mSpine->reset();
	unk150 = 0;
	setAnimationIndex(0);
	unk188 = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getRate();
	unk180 = 0;
	unk184 = 0;
	for (int i = 0; i < 8; ++i)
		unk178[i] = 0;
	unk154                = 0.0f;
	unk158                = 0.0f;
	unk15C                = 0.0f;
	unk160                = 1.0f;
	unk16C                = 0.0f;
	unk164.mDirection     = 0.0f;
	unk170                = 0;
	unk18C                = 0;
	unk190                = 0.0f;
	unk194                = 0.0f;
	unk198                = 0.0f;
	unk19C                = 0.0f;
	unk174->init(150.0f, 100.0f, 150.0f, 100.0f,
	             getParams()->bottomHeight.value);
}

void TKoopaJrSubmarine::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & 2) {
		if (mSpine->getCurrentNerve()
		        == &TNerveKoopaJrSubmarineLaunchKiller::theNerve()
		    && unk180 < unk184 && unk150 <= 0) {
			launchKiller();
			if (unk178[unk180] == 2)
				unk150 = getParams()->mSLKillerIntervalFast.get();
			else
				unk150 = getParams()->mSLKillerInterval.get();
			++unk180;
		}
		makeCollisionPositions();
		moveSwing();
	}

	if (cue & 1) {
		if (unk150 > 0)
			--unk150;
		checkNerve();
	}

	TSpineEnemy::perform(cue, graphics);
	unk1A4->perform(cue, graphics);
	unk1A8->perform(cue, graphics);
}

// dont_inline: the ROM calls this out of line from
// TKoopaJrSubmarine::perform (bl at 0x80112CBC) rather than inlining the whole
// loop body into perform's frame.
#pragma dont_inline on
void TKoopaJrSubmarine::makeCollisionPositions()
{
	// TODO: frame padding. The ROM's frame is 40 bytes larger than ours with an
	// identical instruction stream (MWCC stack-padding bug).
	
	
	JGeometry::TVec3<f32> pos(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < 2; ++i) {
		MtxPtr mtx = getModel()->getAnmMtx(TKoopaJr_getJointIndex(i + 3));
		pos.x += mtx[0][3];
		pos.y += mtx[1][3];
		pos.z += mtx[2][3];
	}
	pos.scale(0.5f);
	unk1A4->mPosition.set(pos);
	getJointTransByIndex(TKoopaJr_getJointIndex(0), &unk1A8->mPosition);
}

#pragma dont_inline off

#pragma dont_inline on
void TKoopaJrSubmarine::moveSwing()
{
	if (unk18C) {
		unk18C = 0;
		unk190 = unk190 + 0.06283186f;
		f32 limA = getParams()->mSLSwingAmplitudeMax.value;
		unk190 = (limA >= unk190) ? unk190 : limA;
	}

	unk190 = unk190 - 0.009424779f;
	{
		f32 limB = getParams()->mSLSwingAmplitudeMin.value;
		unk190 = (limB >= unk190) ? unk190 : limB;
	}

	if (unk190 <= 0.0f)
		unk194 = 0.0f;

	unk194 = 0.0f
	        + std::fmodf(6.2831855f
	                         + (unk194 + getParams()->mSLSwingPhaseVelocity.value
	                            - 0.0f),
	                     6.2831855f);

	f32 len = JGeometry::TUtil<f32>::sqrt(mVelocity.z * mVelocity.z
	                                     + (mVelocity.x * mVelocity.x
	                                        + mVelocity.y * mVelocity.y));
	f32 rate = len / getParams()->mSLSpeedMax.value;

	if (unk150 > 0) {
		unk198 = unk198 + 0.03141593f;
		f32 limC = getParams()->mSLWaveAmplitudeMaxLaunch.value;
		unk198 = (limC >= unk198) ? unk198 : limC;
	}

	if (rate > 0.5f) {
		unk198 = unk198 + 0.03141593f;
		f32 limD = getParams()->mSLWaveAmplitudeMax.value;
		unk198 = (limD >= unk198) ? unk198 : limD;
	}

	unk198 = unk198 - 0.018849557f;
	{
		f32 limE = getParams()->mSLWaveAmplitudeMin.value;
		unk198 = (limE >= unk198) ? unk198 : limE;
	}

	if (unk198 <= 0.0f)
		unk19C = 0.0f;

	unk19C = 0.0f
	        + std::fmodf(6.2831855f
	                         + (unk19C + getParams()->mSLWavePhaseVelocity.value
	                            - 0.0f),
	                     6.2831855f);
}

void TKoopaJrSubmarine::getSwingAngle()
{
	// TODO: UNUSED in the target (0x38 bytes), not yet reconstructed
}

void TKoopaJrSubmarine::getWaveAngle()
{
	// TODO: UNUSED in the target (0x38 bytes), not yet reconstructed
}

void TKoopaJrSubmarine::bind()
{
	JGeometry::TVec3<f32> nextPos = mPosition;
	nextPos += mLinearVelocity;
	nextPos += mVelocity;
	mLinearVelocity = nextPos - mPosition;
	unk174->bind(this);
}

void TKoopaJrSubmarine::calcRootMatrix()
{
	// TODO: not yet reconstructed
	TSpineEnemy::calcRootMatrix();
}

BOOL TKoopaJrSubmarine::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		gpMarioParticleManager->emit(0xE7, &sender->mPosition, 0, nullptr);
		gpMSound->startSoundSet(0x6802, &mPosition, 0, 0.0f, 0, 0, 4);
		damageKoopaJrSubmarine();
		return TRUE;
	}

	return FALSE;
}

void TKoopaJrSubmarine::prepareKillerLaunch(int)
{
	// TODO: UNUSED in the target (0x80 bytes), not yet reconstructed
}

void TKoopaJrSubmarine::prepareKillerLaunchFast(int)
{
	// TODO: UNUSED in the target (0x84 bytes), not yet reconstructed
}

#pragma dont_inline on
int TKoopaJrSubmarine::appearShineKiller(int)
{
	// TODO: frame padding. The ROM's frame is 48 bytes larger than ours with an
	// identical instruction stream (MWCC stack-padding bug).
	
	
	f32 chance;
	if (SMS_GetMarioWaterGun()->mCurrentWater == 0) {
		chance = 0.5f;
	} else if (*(s8*)((u8*)unk1A0->unk16C + 0x60)
	           == TFlagManager::smInstance->getFlag(0x20001)) {
		chance = 0.5f;
	} else {
		s32 denom = *(s32*)((u8*)((const TWaterGun*)SMS_GetMarioWaterGun())
		                        ->getCurrentNozzle()
		                    + 0xCC);
		s32 numer = SMS_GetMarioWaterGun()->mCurrentWater;
		f32 prob0 = getParams()->shineKillerProbability0.value;
		f32 prob1 = getParams()->shineKillerProbability1.value;
		chance = prob0 + ((f32)numer / (f32)denom) * (prob1 - prob0);
	}

	s32 ret = 0;
	if (rand() * 0.000030517578f < chance)
		ret = 1;
	return ret;
}
#pragma dont_inline off

#pragma dont_inline on
void TKoopaJrSubmarine::checkKillerLaunch()
{
	// TODO: UNUSED in the target (0x100 bytes), not yet reconstructed
}

#pragma dont_inline off

#pragma dont_inline on
void TKoopaJrSubmarine::launchKiller()
{
	// TODO: frame padding. The ROM's frame is 32 bytes larger than ours with an
	// identical instruction stream (MWCC stack-padding bug); no natural source
	// spelling has been found that reproduces it.
	
	
	s32 slot = unk180 % 4;
	TBathtubKiller* killer = (TBathtubKiller*)unk1A0->unk16C->getDeadEnemy();
	if (killer) {
		killer->unk194 = unk178[unk180];
		killer->reset();

		s32 joint = TKoopaJr_jointIndexTable[slot + 1];
		MtxPtr mtx = getModel()->getAnmMtx(joint);
		killer->mPosition.x = mtx[0][3];
		killer->mPosition.y = mtx[1][3];
		killer->mPosition.z = mtx[2][3];
		JGeometry::TVec3<f32> dir;
		dir.x = mtx[0][2];
		dir.y = mtx[1][2];
		dir.z = mtx[2][2];
		makeKillerVelocity(killer, dir);

		gpMSound->startSoundActor(0x285D, (const Vec*)&killer->mPosition, 0,
		                          nullptr, 0, 4);
	}
}
#pragma dont_inline off

#pragma dont_inline on
void TKoopaJrSubmarine::makeKillerVelocity(TBathtubKiller*,
                                           JGeometry::TVec3<f32>)
{
	// TODO: not yet reconstructed
}
#pragma dont_inline off

void TKoopaJrSubmarine::emitKoopaJrSubmarineEffects() { }

void TKoopaJrSubmarine::updateTimers()
{
	// TODO: UNUSED in the target (0x1C bytes), not yet reconstructed
}

void TKoopaJrSubmarine::setKoopaJr(TKoopaJr* koopaJr) { unk1A0 = koopaJr; }

const char** TKoopaJrSubmarine::getBasNameTable() const
{
	return koopajrsubmarine_bastable;
}

#pragma dont_inline on
void TKoopaJrSubmarine::makeRelativeAngle()
{
	// `flameDir` has to survive the calcNearerDirection() call in a
	// callee-saved register: the ROM reads it back afterwards to build the
	// "flame + PI" heading.
	f32 flameDir = TDirectionCalc::d2r(unk1A0->unk160->getFlameDirDegree());
	f32 flame = unk164.calcNearerDirection(flameDir);
	f32 flameDiff = fabsf(unk164.mDirection - flame);

	JGeometry::TVec3<f32> diff;
	{
		const JGeometry::TVec3<f32>& bath = unk1A0->unk15C->mPosition;
		diff.x = gpMarioPos->x - bath.x;
		diff.y = gpMarioPos->y - bath.y;
		diff.z = gpMarioPos->z - bath.z;
		diff.y = 0.0f;
	}

	JGeometry::TVec3<f32> mario = diff;
	f32 marioAng = atan2f(mario.x, mario.z);
	f32 marioDir = unk164.calcNearerDirection(marioAng);
	f32 marioDiff = fabsf(unk164.mDirection - marioDir);

	f32 result = unk164.mDirection;
	if (unk1A0->unk160->isFlaming()
	    && flameDiff <= getParams()->aboidKoopaFlameAngle.value)
		result = flameDir + 3.1415927f;
	else if (marioDiff > getParams()->traceMarioAngle.value)
		result = marioAng;

	f32 step = 0.017453294f * getParams()->mSLRoundAngleVelocity.value;
	unk164.mDirection = 0.0f + std::fmodf(6.2831855f + (unk164.mDirection - 0.0f),
	                                     6.2831855f);
	f32 target = unk164.calcNearerDirection(result);
	if (target > unk164.mDirection) {
		if (target - unk164.mDirection < step)
			step = target - unk164.mDirection;
		unk164.mDirection += step;
	} else {
		if (unk164.mDirection - target < step)
			step = unk164.mDirection - target;
		unk164.mDirection -= step;
	}
}
#pragma dont_inline off

#pragma dont_inline on
void TKoopaJrSubmarine::makeRoundVelocity()
{
	f32 dx, dz;
	{
		JGeometry::TVec3<f32> v;
		v.set(sinf(unk164.mDirection), 0.0f, cosf(unk164.mDirection));
		JGeometry::TVec3<f32> dir = v;
		dir.scale(unk168);
		dx = (unk1A0->unk15C->mPosition.x + dir.x) - mPosition.x;
		dz = (unk1A0->unk15C->mPosition.z + dir.z) - mPosition.z;
	}

	f32 dy      = 0.0f;
	f32 squared = dz * dz + (dx * dx + dy);
	f32 len     = JGeometry::TUtil<f32>::sqrt(squared);

	if (len < 100.0f) {
		unk170 = 1;
		return;
	}

	unk170 = 0;

	f32 vx = dx;
	f32 vy = dy;
	f32 vz = dz;
	if (squared > JGeometry::TUtil<f32>::epsilon()) {
		f32 inv = 1.0f * orig_inv_sqrt(squared);
		vx *= inv;
		vy *= inv;
		vz *= inv;
	}

	f32 acc = getParams()->mSLAcceleration.value;
	mVelocity.x += vx * acc;
	mVelocity.y += vy * acc;
	mVelocity.z += vz * acc;

	f32 spd = JGeometry::TUtil<f32>::sqrt(mVelocity.z * mVelocity.z
	                                      + (mVelocity.x * mVelocity.x
	                                         + mVelocity.y * mVelocity.y));
	f32 max = getParams()->mSLSpeedMax.value;
	if (spd > max) {
		f32 lsq = mVelocity.z * mVelocity.z
		          + (mVelocity.x * mVelocity.x + mVelocity.y * mVelocity.y);
		if (lsq <= JGeometry::TUtil<f32>::epsilon()) {
			mVelocity.zero();
		} else {
			mVelocity.scale(1.0f * orig_inv_sqrt(lsq),
			                mVelocity);
		}
		mVelocity.x *= max;
		mVelocity.y *= max;
		mVelocity.z *= max;
	}
}
#pragma dont_inline off

void TKoopaJrSubmarine::makeDirection()
{
	// TODO: UNUSED in the target (0x18C bytes), not yet reconstructed
}

#pragma dont_inline on
void TKoopaJrSubmarine::checkNerve()
{
	if (unk1A0->mSpine->getCurrentNerve() == &TNerveKoopaJrWait::theNerve()) {
		makeRelativeAngle();
		unk168 = getParams()->mSLRoundDistance.value;
		makeRoundVelocity();
	}

	mVelocity.x *= 0.95f;
	mVelocity.y *= 0.95f;
	mVelocity.z *= 0.95f;

	if (!unk170) {
		JGeometry::TVec3<f32> vel = mVelocity;
		f32 lsq = vel.dot(vel);
		if (lsq <= JGeometry::TUtil<f32>::epsilon())
			vel.zero();
		else
			vel.scale(1.0f * orig_inv_sqrt(lsq), vel);

		JGeometry::TVec3<f32> dir = vel;
		f32 angle = atan2f(dir.x, dir.z);
		unk164.mDirection
		    = unk164.calcTurnDirection(
		        angle, TDirectionCalc::d2r(getParams()->mSLRotationSpeed.value));
	}

	if (mSpine->getCurrentNerve() == &TNerveKoopaJrSubmarineWait::theNerve())
		return;

	if (mSpine->getCurrentNerve()
	    == &TNerveKoopaJrSubmarineCannonOpenClose::theNerve()) {
		if (unk180 == 0) {
			J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
			if (ctrl->checkPass(30.0f)) {
				ctrl->setRate(0.0f);
				mSpine->setNext(
				    &TNerveKoopaJrSubmarineLaunchKiller::theNerve());
			}
		}
	} else {
		mSpine->setNext(&TNerveKoopaJrSubmarineLaunchKiller::theNerve());
	}
}
#pragma dont_inline off

DEFINE_NERVE(TNerveKoopaJrSubmarineWait, TLiveActor)
{
	TKoopaJrSubmarine* self = (TKoopaJrSubmarine*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setAnimationIndex(0);
		self->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setRate(0.0f);
	}

	return FALSE;
}

DEFINE_NERVE(TNerveKoopaJrSubmarineCannonOpenClose, TLiveActor)
{
	TKoopaJrSubmarine* self = (TKoopaJrSubmarine*)spine->getBody();

	if (spine->getTime() == 0) {
		J3DFrameCtrl* ctrl = self->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setRate(self->unk188);
	}

	if (self->getMActor()->isCurAnmAlreadyEnd(0))
		return TRUE;

	return FALSE;
}

DEFINE_NERVE(TNerveKoopaJrSubmarineLaunchKiller, TLiveActor)
{
	TKoopaJrSubmarine* self = (TKoopaJrSubmarine*)spine->getBody();

	// TODO: frame padding. The ROM's frame is 8 bytes larger than ours with an
	// identical instruction stream (MWCC stack-padding bug); no natural source
	// spelling has been found that reproduces it.
	
	

	if (self->unk180 == self->unk184 && self->unk150 <= 0) {
		J3DFrameCtrl* ctrl = self->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setRate(self->unk188);
		return TRUE;
	}

	return FALSE;
}

// ============= TKoopaJrSubmarineManager =============

TKoopaJrSubmarineManager::TKoopaJrSubmarineManager(const char* name)
    : TEnemyManager(name)
{
}

void TKoopaJrSubmarineManager::createModelData()
{
	// TODO: flags not decoded into J3DMLF_* yet
	static const TModelDataLoadEntry entry[] = {
		{ "LastKoopaJrSubmarine.bmd", 0x54220000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TKoopaJrSubmarineManager::load(JSUMemoryInputStream& stream)
{
	// TODO: @hack. The ROM compares unk38 against 0 (a dead compare whose
	// flags are immediately overwritten) on entry and again after the
	// assignment. No natural source spelling has been found; this ternary is
	// the only construct found so far that emits the bare cmplwi.
	(void)(unk38 ? unk38 : unk38);
	// TODO: frame padding (MWCC stack-padding bug): the ROM's frame is 64 bytes
	// larger than ours.
	
	
	TEnemyManager::load(stream);
	unk38 = new TKoopaJrSubmarineParams("/enemy/koopajrsubmarine.prm");
	(void)(unk38 ? unk38 : unk38);
}

void TKoopaJrSubmarineManager::loadAfter()
{
	// TODO: frame padding (MWCC stack-padding bug), see ::load.
	
	
	JDrama::TNameRef::loadAfter();
	// TODO: @hack, see TKoopaJrSubmarineManager::load.
	(void)(unk38 ? unk38 : unk38);
}

TSpineEnemy* TKoopaJrSubmarineManager::createEnemyInstance()
{
	return new TKoopaJrSubmarine("クッパジュニアサブマリン");
}
