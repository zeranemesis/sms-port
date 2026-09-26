#include <Enemy/effectEnemy.hpp>
#include <System/Particles.hpp>
#include <Player/MarioAccess.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Strategic/ObjModel.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>

// rogue includes needed for matching sinit & bss: JALList<T>::smList is a
// template static data member, so pulling in MSBgm/MSSetSound instantiates
// the 15 JSUList objects that end up in this TU's .bss (0xB4 bytes) and the
// __sinit_effectEnemy_cpp block that registers them (0x2FC bytes).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// NOTE: definition order is the reverse of marioEU.MAP's .text layout,
// because this TU is compiled with -inline deferred.

void TEffectEnemyManager::load(JSUMemoryInputStream& input)
{
	TSmallEnemyManager::load(input);
	unk38 = new TWalkerEnemyParams("/enemy/moveFireEffect.prm");
}

void TEffectEnemyManager::loadAfter() { JDrama::TNameRef::loadAfter(); }

TSpineEnemy* TEffectEnemyManager::createEnemyInstance()
{
	return new TEffectEnemy("エフェクト敵");
}

void TEffectEnemyManager::initSetEnemies() {}

TEffectEnemy::TEffectEnemy(const char* name)
    : TWalkerEnemy(name)
    , unk194(0)
{
}

void TEffectEnemy::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000005;
}

void TEffectEnemy::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor = mMActorKeeper->createMActor("default.bmd", 3);
}

void TEffectEnemy::kill()
{
	setDeadAnm();
	onLiveFlag(LIVE_FLAG_DEAD);
	onHitFlag(HIT_FLAG_NO_COLLISION);
}

void TEffectEnemy::forceKill()
{
	if (!(mGroundPlane->checkFlag(BG_CHECK_FLAG_ILLEGAL)
	      || (!mGroundPlane->isDeathPlane() && !mGroundPlane->isPool()
	          && !mGroundPlane->isWaterSurface())
	      || isAirborne() || checkLiveFlag(LIVE_FLAG_UNK10))
	    || !gpMap->isInArea(mPosition.x, mPosition.z)) {
		kill();
	}
}

void TEffectEnemy::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return;

	if (cue & CUE_MOVE)
		TWalkerEnemy::moveObject();

	if ((cue & CUE_CALC_ANIM) && !checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		Vec local_1c;
		VECScale(&mScaling, &local_1c, mHitPoints / getMaxHitPoints());

		gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_C,
		                                            &mPosition, 3, this);
		gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_A,
		                                            &mPosition, 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_B,
		                                            &mPosition, 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_D,
		                                            &mPosition, 1, this);
	}

	THitActor::perform(cue, graphics);
}

void TEffectEnemy::reset() { TWalkerEnemy::reset(); }

void TEffectEnemy::behaveToWater(THitActor* actor)
{
	if (mHitPoints > 1)
		TSmallEnemy::behaveToWater(actor);
	else
		kill();
}

void TEffectEnemy::sendAttackMsgToMario()
{
	switch (unk194) {
	case 0:
		SMS_SendMessageToMario(this, HIT_MESSAGE_UNKA);
		kill();
		break;
	case 1:
		SMS_SendMessageToMario(this, HIT_MESSAGE_UNK9);
		break;
	default:
		SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
		break;
	}
}

// TODO: 99.8% - every instruction matches, but marioEU.dol allocates a
// 0x20 stack frame while we allocate 0x18 (target: stwu r1, -0x20(r1) /
// stw r31, 0x1c(r1) / lwz r0, 0x24(r1)). The 8 reserved bytes produce no
// code at all, so they come from a spill slot or a local that the compiler
// keeps alive; no source-level lever for it has been found yet. Same class
// of problem as the stack-frame deltas in TNerveSBH_Fall::execute.
void TEffectEnemy::setDeadAnm()
{
	gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_OFF,
	                                            &mPosition, 0, nullptr);
	if (gpMSound->gateCheck(0x28C5))
		MSoundSESystem::MSoundSE::startSoundActor(0x28C5, &mPosition, 0, nullptr,
		                                          0, 4);
	onLiveFlag(LIVE_FLAG_UNK20000);
}

// Nothing in marioEU.dol calls this and it is not in the vtable, so the
// linker deadstripped it: marioEU.MAP records it as UNUSED, size 0xF4. Its
// size therefore is the only check available, and the body below compiles to
// exactly 244 bytes (see tools/validate-symbol-order.py: "UNUSED symbol sizes
// match"). It is the particle block that ::perform contains inline, which
// makes it a strongly corroborated guess, but it can never be verified
// instruction by instruction. TODO: re-examine if a call site ever turns up.
void TEffectEnemy::emitEffect()
{
	Vec local_1c;
	VECScale(&mScaling, &local_1c, mHitPoints / getMaxHitPoints());

	gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_C,
	                                            &mPosition, 3, this);
	gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_A,
	                                            &mPosition, 1, this);
	gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_B,
	                                            &mPosition, 1, this);
	gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_D,
	                                            &mPosition, 1, this);
}
