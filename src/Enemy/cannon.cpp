#include <Enemy/Cannon.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <System/Application.hpp>
#include <System/Particles.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MAnmSound.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <Player/MarioAccess.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// NOTE: this TU is -inline deferred, so the out-of-line functions below are
// defined in the *reverse* order of mario.MAP's .text layout.

static const char* cannon_bastable[] = {
	nullptr,
	nullptr,
	"/scene/cannon/bas/CannonDom_break.bas",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/cannon/bas/tyorobe_appear1.bas",
	"/scene/cannon/bas/tyorobe_damage1.bas",
	"/scene/cannon/bas/tyorobe_down1.bas",
	"/scene/cannon/bas/tyorobe_hyde1.bas",
	"/scene/cannon/bas/tyorobe_throw1.bas",
	nullptr,
	nullptr,
	nullptr,
};

// TODO: UNUSED statics also listed in the map: mTestAngY, mShootAngVal
// (.sdata) and mThrowOffsetY, mCannonOffset (.sbss); their values are
// unknown.
u8 TCannon::mChorobeiJntIdx     = 4;
u8 TCannon::mChorobeiHandJntIdx = 4;
f32 TCannon::mVelocityRate      = 0.62f;
f32 TCannon::mSearchRate        = 0.02f;

TCannonSaveLoadParams::TCannonSaveLoadParams(const char* path)
    : TSmallEnemyParams(path)
    , PARAM_INIT(mSLHideDist, 300.0f)
    , PARAM_INIT(mSLBombDist, 2000.0f)
    , PARAM_INIT(mSLKillerDist, 10000.0f)
    , PARAM_INIT(mSLBombInterval, 100)
    , PARAM_INIT(mSLKillerInterval, 50)
    , PARAM_INIT(mSLShootInterval, 100)
    , PARAM_INIT(mSLChorobeiAttackRadius, 100.0f)
    , PARAM_INIT(mSLChorobeiAttackHeight, 100.0f)
    , PARAM_INIT(mSLChorobeiDamageRadius, 100.0f)
    , PARAM_INIT(mSLChorobeiDamageHeight, 100.0f)
    , PARAM_INIT(mSLKillerTransYOffset, -50.0f)
    , PARAM_INIT(mSLBombHeiGenerateRate, 0.7f)
    , PARAM_INIT(mSLThrowXZSpeed, 12.0f)
{
	TParams::load(mPrmPath);
}

TCannonManager::TCannonManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TCannonManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TCannonSaveLoadParams("/enemy/cannon.prm");
}

TSmallEnemy* TCannonManager::createEnemyInstance()
{
	return new TCannon("砲台");
}

// ============= TChorobei =============

TChorobei::TChorobei(TCannon* cannon, int jointIndex, const char* name)
    : THitActor(name)
    , mCannon(cannon)
    , mParts(nullptr)
    , unk70(0.0f)
    , mAnmSound(nullptr)
    , unk78(0)
    , unk7C(300.0f)
{
	mParts = new TSharedParts(mCannon, jointIndex,
	                          "/scene/cannon/tyorobe_model1.bmd", 0x10020000, 3);
	if (mAnmSound)
		return;

	mAnmSound = new MAnmSound(gpMSound);
	mAnmSound->initAnmSound(nullptr, 1, 0.0f);
}

void TChorobei::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// TODO: not yet reconstructed
	THitActor::perform(cue, graphics);
}

void TChorobei::setBckAnm(int)
{
	// TODO: UNUSED in the target (0xAC bytes), not yet reconstructed
}

void TChorobei::checkHit()
{
	// TODO: not yet reconstructed
}

BOOL TChorobei::receiveMessage(THitActor* sender, u32 message) { return FALSE; }

void TChorobei::isUpEnd()
{
	// TODO: UNUSED in the target (0x70 bytes), not yet reconstructed
}

void TChorobei::isDownEnd()
{
	// TODO: UNUSED in the target (0x70 bytes), not yet reconstructed
}

// ============= TCannonDom =============

TCannonDom::TCannonDom(TLiveActor* owner, int jointIndex,
                       SDLModelData* modelData, u32 flags, const char* name)
    : TSharedParts(owner, jointIndex, modelData, flags, name)
    , mAnmSound(nullptr)
    , unk20(0)
    , unk24(0)
    , unk28(0.0f)
    , unk2C(0.0f)
    , unk30(0.0f)
{
	unk30 = TMsRange<f32>(0.0f, 360.0f).rand();
	if (mAnmSound)
		return;

	mAnmSound = new MAnmSound(gpMSound);
	mAnmSound->initAnmSound(nullptr, 1, 0.0f);
}

void TCannonDom::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// TODO: not yet reconstructed
	TSharedParts::perform(cue, graphics);
}

void TCannonDom::setBckAnm(int)
{
	// TODO: UNUSED in the target (0xA8 bytes), not yet reconstructed
}

// ============= TCannon =============

TCannon::TCannon(const char* name)
    : TSmallEnemy(name)
    , unk1A0(nullptr)
    , unk1A8(nullptr)
    , unk1B8(nullptr)
    , unk1E0(nullptr)
    , unk214(0)
    , unk218(0)
    , unk21C(0)
    , unk220(0.0f)
    , unk230(1)
    , unk238(0)
    , unk239(1)
    , unk254(nullptr)
    , unk258(nullptr)
    , unk290(0)
    , unk2AC(0.0f)
{
	unk224.set(0.0f, 0.0f, 0.0f);
}

void TCannon::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	reset();
	mHitPoints = getMaxHitPoints();
	unk230     = gpApplication.mCurrArea.unk0;
	unk23C     = mPosition;
}

void TCannon::loadAfter()
{
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_a.jpa", 0xE8);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_b.jpa", 0xE9);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_c.jpa", 0xEA);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_d.jpa", 0xEB);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_e.jpa", 0xEC);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_smoke.jpa", 0x166);
	if (unk230 == 5) {
		unk254 = (TLiveActor*)JDrama::TNameRefGen::search("efMareGate");
		unk254->kill();
	}
}

void TCannon::init(TLiveManager* manager)
{
	// TODO: not yet reconstructed
	TSmallEnemy::init(manager);
}

void TCannon::reset()
{
	TSmallEnemy::reset();
	mHitPoints = getMaxHitPoints();
	onHitFlag(HIT_FLAG_NO_COLLISION);
	unk214      = 1;
	mHeadHeight = 40.0f;
	onLiveFlag(LIVE_FLAG_UNK10);
	unk2AC = mRotation.y;
	if (unk230 == 9) {
		unk239 = 0;
		setGoalPath(TPathNode(JGeometry::TVec3<f32>(-565.0f, 8500.0f, 7675.0f)));
	}
}

void TCannon::moveObject()
{
	// TODO: not yet reconstructed
	TSmallEnemy::moveObject();
}

BOOL TCannon::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->getActorType() == 0x40000235 && message == HIT_MESSAGE_TAKE
	    && mHolder == nullptr) {
		mHolder = (TTakeActor*)sender;
		return TRUE;
	}

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER)
		return TRUE;

	return FALSE;
}

void TCannon::calcObjCollision()
{
	// TODO: UNUSED in the target (0x12C bytes), not yet reconstructed
}

void TCannon::entryObjCollision()
{
	// TODO: UNUSED in the target (0x58 bytes), not yet reconstructed
}

const char** TCannon::getBasNameTable() const { return cannon_bastable; }

void TCannon::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// TODO: not yet reconstructed
	TSmallEnemy::perform(cue, graphics);
}

void TCannon::calcRootMatrix()
{
	// TODO: not yet reconstructed
	TSpineEnemy::calcRootMatrix();
}

MtxPtr TCannon::getTakingMtx()
{
	// TODO: not yet reconstructed
	return nullptr;
}

void TCannon::bombSet()
{
	// TODO: not yet reconstructed
}

void TCannon::bombShoot()
{
	// TODO: not yet reconstructed
}

void TCannon::bombScaleUp()
{
	// TODO: UNUSED in the target (0x5C bytes), not yet reconstructed
}

void TCannon::hitHead(TBombHei*)
{
	// TODO: UNUSED in the target (0x154 bytes), not yet reconstructed
}

void TCannon::updateAttachPos()
{
	// TODO: UNUSED in the target (0x1C0 bytes), not yet reconstructed
}

void TCannon::killerShoot()
{
	// TODO: not yet reconstructed
}

void TCannon::endKillerShoot()
{
	// TODO: UNUSED in the target (0x24 bytes), not yet reconstructed
}

void TCannon::damage()
{
	// TODO: UNUSED in the target (0x118 bytes), not yet reconstructed
}

void TCannon::setKillerGoalPoint()
{
	// TODO: not yet reconstructed
}

void TCannon::deadCannon()
{
	// TODO: UNUSED in the target (0x20 bytes), not yet reconstructed
}

void TCannon::startDemo() { }

void TCannon::startMarioDemo() { }

void TCannon::turnToGoal()
{
	// TODO: UNUSED in the target (0x2C bytes), not yet reconstructed
}

bool TCannon::isObject()
{
	if (isBckAnm(4) && checkCurAnmEnd(0))
		return true;

	return false;
}

void TCannon::killShootAct()
{
	// TODO: UNUSED in the target (0x3C bytes), not yet reconstructed
}

void TCannon::gateOpen()
{
	// TODO: UNUSED in the target (0xD0 bytes), not yet reconstructed
}

void TCannon::startChorobeiShout() { }

// ============= nerves =============

DEFINE_NERVE(TNerveCannonOpen, TLiveActor)
{
	// TODO: not yet reconstructed
	return FALSE;
}

DEFINE_NERVE(TNerveCannonSearch, TLiveActor)
{
	// TODO: not yet reconstructed
	return FALSE;
}

DEFINE_NERVE(TNerveCannonShoot, TLiveActor)
{
	// TODO: not yet reconstructed
	return FALSE;
}

DEFINE_NERVE(TNerveCannonForceBombShoot, TLiveActor)
{
	// TODO: not yet reconstructed
	return FALSE;
}

DEFINE_NERVE(TNerveCannonClose, TLiveActor)
{
	// TODO: not yet reconstructed
	return FALSE;
}

DEFINE_NERVE(TNerveCannonDamage, TLiveActor)
{
	// TODO: not yet reconstructed
	return FALSE;
}

DEFINE_NERVE(TNerveCannonDamageDemo, TLiveActor)
{
	// TODO: not yet reconstructed
	return FALSE;
}

DEFINE_NERVE(TNerveCannonObject, TLiveActor)
{
	// TODO: not yet reconstructed
	return FALSE;
}
