#include <Enemy/KoopaJr.hpp>
#include <Enemy/BathtubBinder.hpp>
#include <Enemy/BathtubKiller.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <System/Particles.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Strategy.hpp>
#include <Strategic/Spine.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MoveBG/MapObjCorona.hpp>
#include <Player/MarioAccess.hpp>
#include <math.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

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

void TDirectionCalc::sub(f32)
{
	// TODO: UNUSED in the target (0xA0 bytes), not yet reconstructed
}

f32 TDirectionCalc::calcTurnDirection(f32 target, f32 speed)
{
	// TODO: not yet reconstructed; normalizes twice (once through
	// std::fmodf, once through JGeometry::TUtil<f32>::mod) before clamping
	// the turn to `speed`.
	return mDirection;
}

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
	// TODO: not yet reconstructed; needs JGeometry::TUtil<f32>::mod, whose
	// float specialization is emitted weak into this TU but is not declared
	// in JGUtil.hpp yet.
	return target;
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

void TKoopaJr::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// TODO: not yet reconstructed
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

void TKoopaJr::setAnimationIndex(int index)
{
	mMActor->setBckFromIndex(index);
	setAnmSound(getBas(index));
}

void TKoopaJr::updateTimers()
{
	// TODO: UNUSED in the target (0x4C bytes), not yet reconstructed
}

const char** TKoopaJr::getBasNameTable() const { return koopajr_bastable; }

BOOL TKoopaJr::receiveMessage(THitActor* sender, u32 message)
{
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
	// TODO: not yet reconstructed
}

void TKoopaJr::checkNerveKillerLaunchFast()
{
	// TODO: not yet reconstructed
}

void TKoopaJr::checkNerveKillerHit()
{
	// TODO: not yet reconstructed
}

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

TKoopaJrSubmarine::TKoopaJrSubmarine(const char* name)
    : TSpineEnemy(name)
    , unk164(0.0f)
    , unk188(0.0f)
    , unk1A0(nullptr)
{
	offLiveFlag(LIVE_FLAG_UNK10);
	offLiveFlag(LIVE_FLAG_UNK100);
}

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
	unk16C.mDirection     = 0.0f;
	unk164                = 0.0f;
	unk170                = 0;
	unk18C                = 0;
	unk190                = 0.0f;
	unk194                = 0.0f;
	unk198                = 0.0f;
	unk19C                = 0.0f;
	unk174->init(150.0f, 100.0f, 150.0f, 100.0f,
	             getParams()->bottomHeight.get());
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

void TKoopaJrSubmarine::makeCollisionPositions()
{
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

void TKoopaJrSubmarine::moveSwing()
{
	// TODO: not yet reconstructed
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

void TKoopaJrSubmarine::damageKoopaJrSubmarine() { unk18C = 1; }

void TKoopaJrSubmarine::setAnimationIndex(int index)
{
	mMActor->setBckFromIndex(index);
	setAnmSound(getBas(index));
}

void TKoopaJrSubmarine::prepareKillerLaunch(int)
{
	// TODO: UNUSED in the target (0x80 bytes), not yet reconstructed
}

void TKoopaJrSubmarine::prepareKillerLaunchFast(int)
{
	// TODO: UNUSED in the target (0x84 bytes), not yet reconstructed
}

bool TKoopaJrSubmarine::appearShineKiller(int)
{
	// TODO: not yet reconstructed
	return false;
}

void TKoopaJrSubmarine::checkKillerLaunch()
{
	// TODO: UNUSED in the target (0x100 bytes), not yet reconstructed
}

void TKoopaJrSubmarine::launchKiller()
{
	// TODO: not yet reconstructed
}

void TKoopaJrSubmarine::makeKillerVelocity(TBathtubKiller*,
                                           JGeometry::TVec3<f32>)
{
	// TODO: not yet reconstructed
}

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

void TKoopaJrSubmarine::makeRelativeAngle()
{
	// TODO: not yet reconstructed
}

void TKoopaJrSubmarine::makeRoundVelocity()
{
	// TODO: not yet reconstructed
}

void TKoopaJrSubmarine::makeDirection()
{
	// TODO: UNUSED in the target (0x18C bytes), not yet reconstructed
}

void TKoopaJrSubmarine::checkNerve()
{
	// TODO: not yet reconstructed
}

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
	TEnemyManager::load(stream);
	unk38 = new TKoopaJrSubmarineParams("/enemy/koopajrsubmarine.prm");
}

void TKoopaJrSubmarineManager::loadAfter() { JDrama::TNameRef::loadAfter(); }

TSpineEnemy* TKoopaJrSubmarineManager::createEnemyInstance()
{
	return new TKoopaJrSubmarine("クッパジュニアサブマリン");
}
