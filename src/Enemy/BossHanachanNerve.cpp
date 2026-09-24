#include <Enemy/BossHanachan.hpp>
#include <Strategic/Spine.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <MSound/MSModBgm.hpp>
#include <MSound/BackgroundMusic.hpp>
#include <System/MarDirector.hpp>
#include <GC2D/GCConsole2.hpp>

const TNerveBossHanachanGraphWander& TNerveBossHanachanGraphWander::theNerve()
{
	static TNerveBossHanachanGraphWander instance;
	return instance;
}

BOOL TNerveBossHanachanGraphWander::execute(TSpineBase<TLiveActor>* spine) const
{
	TBossHanachan* self = (TBossHanachan*)spine->getBody();

	if (spine->getTime() == 0)
		self->setHeadAndBodyAnm(ANM_KIND_0, STOP_MOTION_BLEND_ON);

	self->execWalk(true);

	if (self->checkFallDecideAndSetup()) {
		spine->pushAfterCurrent(&TNerveBossHanachanTumble::theNerve());
		return TRUE;
	}

	return FALSE;
}

const TNerveBossHanachanTumble& TNerveBossHanachanTumble::theNerve()
{
	static TNerveBossHanachanTumble instance;
	return instance;
}

BOOL TNerveBossHanachanTumble::execute(TSpineBase<TLiveActor>* spine) const
{
	TBossHanachan* self = (TBossHanachan*)spine->getBody();

	if (spine->getTime() == 0)
		self->setTumbleAnm(STOP_MOTION_BLEND_ON);
	else
		self->considerSetAnm(NERVE_ANM_TUMBLE);

	self->execSlip();

	if (self->getMarchSpeed() == 0.0f && self->isTumbleCompletelyAllBody()) {
		gpMarDirector->getConsole()->startAppearBalloon(7, true);
		spine->pushAfterCurrent(&TNerveBossHanachanDown::theNerve());
		return TRUE;
	}

	return FALSE;
}

const TNerveBossHanachanDown& TNerveBossHanachanDown::theNerve()
{
	static TNerveBossHanachanDown instance;
	return instance;
}

BOOL TNerveBossHanachanDown::execute(TSpineBase<TLiveActor>* spine) const
{
	TBossHanachan* self = (TBossHanachan*)spine->getBody();

	self->considerSetAnm(NERVE_ANM_DOWN);

	if (spine->getTime() >= *(const s16*)(self->mUnk1C0 + 0x1a8)) {
		self->setAnmTimerWhenGetUp();
		spine->pushAfterCurrent(&TNerveBossHanachanGetUp::theNerve());
		return TRUE;
	}

	return FALSE;
}

const TNerveBossHanachanGetUp& TNerveBossHanachanGetUp::theNerve()
{
	static TNerveBossHanachanGetUp instance;
	return instance;
}

BOOL TNerveBossHanachanGetUp::execute(TSpineBase<TLiveActor>* spine) const
{
	TBossHanachan* self = (TBossHanachan*)spine->getBody();

	self->considerSetAnm(NERVE_ANM_GETUP);

	if (self->isFinishedGetUp()) {
		self->setRandomWeakBodyIndex();
		self->setAnmTimerWhenSnort();
		spine->pushAfterCurrent(&TNerveBossHanachanSnort::theNerve());
		return TRUE;
	}

	return FALSE;
}

const TNerveBossHanachanDamage& TNerveBossHanachanDamage::theNerve()
{
	static TNerveBossHanachanDamage instance;
	return instance;
}

BOOL TNerveBossHanachanDamage::execute(TSpineBase<TLiveActor>* spine) const
{
	TBossHanachan* self = (TBossHanachan*)spine->getBody();

	self->considerSetAnm(NERVE_ANM_DAMAGE);
	self->execSlip();

	if (self->getMarchSpeed() == 0.0f
	    && spine->getTime() >= *(const s16*)(self->mUnk1C0 + 0x1bc)) {
		self->setAnmTimerWhenGetUp();
		spine->pushAfterCurrent(&TNerveBossHanachanGetUp::theNerve());
		return TRUE;
	}

	return FALSE;
}

const TNerveBossHanachanSnort& TNerveBossHanachanSnort::theNerve()
{
	static TNerveBossHanachanSnort instance;
	return instance;
}

BOOL TNerveBossHanachanSnort::execute(TSpineBase<TLiveActor>* spine) const
{
	TBossHanachan* self = (TBossHanachan*)spine->getBody();

	if (spine->getTime() == 0xc8) {
		if (self->checkLiveFlag(LIVE_FLAG_UNK20000)) {
			self->offLiveFlag(LIVE_FLAG_UNK20000);
			MSBgm::startBGM(MSD_BGM_BOSSGESO_2DN3RD);
		}

		switch (self->getHitPoints()) {
		case 2:
			gpMSound->unk98->changeTempo(0, 1);
			break;
		case 1:
			gpMSound->unk98->changeTempo(1, 1);
			break;
		}
	}

	self->considerSetAnm(NERVE_ANM_SNORT);

	if (self->isAllBckAlreadyEnd(ANM_KIND_E)) {
		self->goToInitialRecoverGraphNode();
		spine->pushAfterCurrent(&TNerveBossHanachanGraphWander::theNerve());
		return TRUE;
	}

	return FALSE;
}

const TNerveBossHanachanDead& TNerveBossHanachanDead::theNerve()
{
	static TNerveBossHanachanDead instance;
	return instance;
}

BOOL TNerveBossHanachanDead::execute(TSpineBase<TLiveActor>* spine) const
{
	TBossHanachan* self = (TBossHanachan*)spine->getBody();

	self->considerSetAnm(NERVE_ANM_DEAD);

	if (!(self->mLiveFlag & LIVE_FLAG_UNK40000)) {
		if (self->isAllBckAlreadyEnd(ANM_KIND_F)) {
			self->mLiveFlag |= LIVE_FLAG_UNK40000;
			self->removeAllMapCollision();
		}
	}

	return FALSE;
}
