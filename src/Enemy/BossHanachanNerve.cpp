#include <Enemy/BossHanachan.hpp>
#include <Enemy/BossHanachanChangeSaveParams.hpp>
#include <GC2D/GCConsole2.hpp>
#include <MSound/MSModBgm.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <Strategic/Spine.hpp>
#include <System/MarDirector.hpp>

// TODO: __sinit_BossHanachanNerve_cpp is nonmatching -- calling MSBgm::startBGM
// here pulls in static-local JALList<T> registration objects for many
// unrelated sound-group types, and their sinit emission order does not yet
// match the target. Needs deeper investigation into MSBgm/MSound sinit
// ordering, not specific to this TU's own code.

DEFINE_NERVE(TNerveBossHanachanDead, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	boss->considerSetAnm(BH_NERVE_ANM_DEAD);
	if (!boss->checkLiveFlag(LIVE_FLAG_UNK40000)
	    && boss->isAllBckAlreadyEnd(BH_ANM_KIND_UNKF)) {
		boss->onLiveFlag(LIVE_FLAG_UNK40000);
		boss->removeAllMapCollision();
	}
	return false;
}

DEFINE_NERVE(TNerveBossHanachanSnort, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	if (spine->getTime() == 200 && boss->checkLiveFlag(LIVE_FLAG_UNK20000)) {
		boss->offLiveFlag(LIVE_FLAG_UNK20000);
		MSBgm::startBGM(0x80010029);
		switch (boss->mHitPoints) {
		case 2:
			SMSGetMSound()->unk98->changeTempo(0, 1);
			break;
		case 1:
			SMSGetMSound()->unk98->changeTempo(1, 1);
			break;
		}
	}

	boss->considerSetAnm(BH_NERVE_ANM_SNORT);
	if (boss->isAllBckAlreadyEnd(BH_ANM_KIND_UNKE)) {
		boss->goToInitialRecoverGraphNode();
		spine->pushAfterCurrent(&TNerveBossHanachanGraphWander::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBossHanachanDamage, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	boss->considerSetAnm(BH_NERVE_ANM_DAMAGE);
	boss->execSlip();
	if (boss->mMarchSpeed == 0.0f
	    && spine->getTime() >= boss->mChangeSaveParams->mSLDamageFrames.get()) {
		boss->setAnmTimerWhenGetUp();
		spine->pushAfterCurrent(&TNerveBossHanachanGetUp::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBossHanachanGetUp, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	boss->considerSetAnm(BH_NERVE_ANM_GET_UP);
	if (boss->isFinishedGetUp()) {
		boss->setRandomWeakBodyIndex();
		boss->setAnmTimerWhenSnort();
		spine->pushAfterCurrent(&TNerveBossHanachanSnort::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBossHanachanDown, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	boss->considerSetAnm(BH_NERVE_ANM_DOWN);
	if (spine->getTime() >= boss->mChangeSaveParams->mSLDownFrames.get()) {
		boss->setAnmTimerWhenGetUp();
		spine->pushAfterCurrent(&TNerveBossHanachanGetUp::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBossHanachanTumble, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	if (spine->getTime() == 0)
		boss->setTumbleAnm(BH_STOP_MOTION_BLEND_ON);
	else
		boss->considerSetAnm(BH_NERVE_ANM_TUMBLE);

	boss->execSlip();
	if (boss->mMarchSpeed == 0.0f && boss->isTumbleCompletelyAllBody()) {
		gpMarDirector->getConsole()->startAppearBalloon(7, true);
		spine->pushAfterCurrent(&TNerveBossHanachanDown::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBossHanachanGraphWander, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	if (spine->getTime() == 0)
		boss->setHeadAndBodyAnm(BH_ANM_KIND_UNK0, BH_STOP_MOTION_BLEND_ON);

	boss->execWalk(true);
	if (boss->checkFallDecideAndSetup()) {
		spine->pushAfterCurrent(&TNerveBossHanachanTumble::theNerve());
		return true;
	}
	return false;
}
