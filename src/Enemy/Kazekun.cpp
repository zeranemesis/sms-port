#include <Enemy/Kazekun.hpp>
#include <M3DUtil/MActor.hpp>
#include <Strategic/Spine.hpp>

// TODO: this TU is only partially decompiled. The nerve state machine
// (execute() bodies besides Wait), TKazekun::init/calcRootMatrix/
// attackToMario/behaveToWater, TKazekunParams, TKazekunManager::load and
// ::createModelData, flyAroundMario and doAttackPose all still need
// reconstruction. See build/GMSP01/asm/Enemy/Kazekun.s.
//
// NOTE: this file uses -inline deferred, under which the compiler emits
// function bodies in the REVERSE of source order (see
// docs/AGENT_MATCHING_TIPS.md / validate-symbol-order.py). Functions below
// are intentionally ordered so the emitted .text matches mario.MAP.

TKazekun::TKazekun(const char* name)
    : TSmallEnemy(name)
{
	unk1B0 = 0;
	onLiveFlag(LIVE_FLAG_UNK10);
}

void TKazekun::bind()
{
	mLinearVelocity += mVelocity;
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

bool TKazekun::isCollidMove(THitActor*)
{
	return false;
}

void TKazekun::setDeadAnm()
{
	mMActor->getFrameCtrl(0)->init(1);
	mMActor->getFrameCtrl(0)->setFrame(0.0f);
}

TKazekunManager::TKazekunManager(const char* name)
    : TSmallEnemyManager(name)
{
}

DEFINE_NERVE(TNerveKazekunWait, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->onLiveFlag(LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8);
		self->setAnmSound(nullptr);
	}

	if (self->unk1B0 < spine->getTime()) {
		// TODO: TNerveKazekunSearch::theNerve() is inlined at this callsite
		// in the target (deferred inlining), which also changes this
		// function's stack frame size. Revisit once TNerveKazekunSearch's
		// execute() is decompiled and defined nearby in this TU.
		spine->pushAfterCurrent(&TNerveKazekunSearch::theNerve());
		return true;
	}

	return false;
}
