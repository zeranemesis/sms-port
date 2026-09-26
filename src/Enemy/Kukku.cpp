#include <Enemy/Kukku.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/ObjModel.hpp>
#include <M3DUtil/MActor.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// TODO: this whole TU is a from-scratch scaffold (no header existed before).
// Field offsets, base classes and most function bodies were reconstructed
// from m2c drafts of build/GMSP01/asm/Enemy/Kukku.s and have not yet been
// matched against the target with decomp-diff. Treat everything except the
// functions confirmed matching via decomp-diff (see git log) as unverified.
//
// Source order below matches tools/validate-symbol-order.py (this TU is
// -inline deferred, so the non-weak/out-of-line function order is the
// *reverse* of mario.MAP's .text layout -- TKukkuBall/TEnemyCoinUnit come
// first, the nerves' execute() bodies come last).

void TKukkuBall::init()
{
	// TODO: not yet reconstructed
}

void TKukkuBall::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// TODO: not yet reconstructed
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
    , unk1A0(0)
{
}

void TKukku::init(TLiveManager* liveManager)
{
	// TODO: not yet reconstructed
	TSmallEnemy::init(liveManager);
}

void TKukku::reset()
{
	unk1A4            = 0;
	unk1AC            = 0;
	mGravity          = 0.0f;
	unk1B0            = 0;
	onLiveFlag(0x80);
	mScaledBodyRadius = 75.0f;
}

BOOL TKukku::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: not yet reconstructed
	return TSmallEnemy::receiveMessage(sender, message);
}

void TKukku::control()
{
	s32 timer = unk1A4;
	if (timer > 0) {
		unk1A4 = timer - 1;
	}
	TLiveActor::control();
}

void TKukku::calcRootMatrix()
{
	// TODO: not yet reconstructed
	TSpineEnemy::calcRootMatrix();
}

void TKukku::bind()
{
	TLiveActor::bind();
}

void TKukku::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// TODO: not yet reconstructed
	TSmallEnemy::perform(cue, graphics);
}

void TKukku::behaveToWater(THitActor* hitActor)
{
	// TODO: not yet reconstructed
	TSmallEnemy::behaveToWater(hitActor);
}

void TKukku::updateRotation()
{
	// TODO: not yet reconstructed
}

void TKukku::calcMomentum(f32 param_1)
{
	// TODO: not yet reconstructed
}

void TKukku::dropCoins()
{
	// TODO: not yet reconstructed
}

void TKukku::setDeadAnm()
{
	mMActor->setBck("tori_down");
	setCurAnmSound();
}

void TKukku::setAfterDeadEffect()
{
	TSmallEnemy::setAfterDeadEffect();
	// TODO: gpPollution->stamp(getManager()->..., pos.x, pos.y, pos.z, 1000.0f)
}

TKukkuManager::TKukkuManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TKukkuManager::load(JSUMemoryInputStream& stream)
{
	// TODO: not yet reconstructed (constructs TKukkuParams via PARAM_INIT)
	TSmallEnemyManager::load(stream);
}

void TKukkuManager::createModelData()
{
	// TODO: not yet reconstructed
}

const char** TKukku::getBasNameTable() const
{
	extern const char* tori_bastable[];
	return tori_bastable;
}

// ============= nerves =============
// execute() is a regular out-of-line member function (strong symbol), so its
// order is significant, unlike theNerve()/~dtor which are header-inline
// (weak) and compiler-ordered.

DEFINE_NERVE(TNerveKukkuGraphWander, TLiveActor)
{
	// TODO: not yet reconstructed
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
	// TODO: not yet reconstructed
	return FALSE;
}

DEFINE_NERVE(TNerveKukkuPostFall, TLiveActor)
{
	// TODO: not yet reconstructed
	return FALSE;
}

DEFINE_NERVE(TNerveKukkuRecoverGraph, TLiveActor)
{
	// TODO: not yet reconstructed
	return FALSE;
}
