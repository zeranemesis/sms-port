#include <System/MarDirector.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JDrama/JDRCamera.hpp>
#include <System/Application.hpp>
#include <System/MSoundMainSide.hpp>
#include <System/MarioGamePad.hpp>
#include <System/PerformList.hpp>
#include <System/FlagManager.hpp>
#include <System/CardManager.hpp>
#include <Player/Mario.hpp>
#include <Player/WaterGun.hpp>
#include <Strategic/ObjHitCheck.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <THPPlayer/THPPlayer.h>
#include <GC2D/GCConsole2.hpp>
#include <GC2D/ConsoleStr.hpp>
#include <GC2D/ScrnFader.hpp>
#include <GC2D/PauseMenu2.hpp>
#include <GC2D/CardSave.hpp>
#include <GC2D/Guide.hpp>
#include <GC2D/SunGlass.hpp>
#include <GC2D/Talk2D2.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/MarioPositionObj.hpp>
#include <MoveBG/Item.hpp>
#include <MoveBG/MapObjDolpic.hpp>
#include <NPC/NpcBase.hpp>
#include <dolphin/gx.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
#include <Map/MapCollisionEntry.hpp>

#include <System/StageUtil.hpp>

extern OSThread gSetupThread;

// TODO: 99.7%, frame 0x158 vs retail 0x198 (TGraphics sits 0x40 higher, a
// dead low region) and r29/r30 swapped. Body from upstream (98.5 -> 99.7).
int TMarDirector::direct()
{
	int dt = 600 / (int)SMSGetVSyncTimesPerSec();

	if (!unk260) {
		if (!OSIsThreadTerminated(&gSetupThread))
			return 0;

		void* local_40;
		OSJoinThread(&gSetupThread, &local_40);
		if (local_40)
			return 4;

		setupObjects();
		unk260 = true;
	}

	u32 desiredAppState = TApplication::APP_STATE_DEFAULT;

	JDrama::TGraphics local_140;

	u8 prevSeGateMask = SMSGetMSound()->mSeGateMask;
	unk54 += dt;

	int i = 0;
	for (;;) {
		if (!checkFlag(DIRECTOR_FLAG_LAST_SIMULATION_TICK)) {
			++i;
			if (i == 1)
				onFlag(DIRECTOR_FLAG_FIRST_SIMULATION_TICK);
			unk54 -= 5;
			if (unk54 < 5)
				onFlag(DIRECTOR_FLAG_LAST_SIMULATION_TICK);

			// inline?
			u32 blacklistedCues = 0;
			u8 seGateMask       = prevSeGateMask;
			if (checkFlag(DIRECTOR_FLAG_LAST_SIMULATION_TICK)) {
				if (unk258)
					unk258->stageLoop();
			} else {
				blacklistedCues |= CUE_CALC_ANIM;
				seGateMask &= ~MSSeGate_Continuous;
			}
			SMSGetMSound()->mSeGateMask = seGateMask;

			switch (mState) {
			case STATE_PAUSE_MENU:
			case STATE_CARD_SAVE:
			case STATE_UNK12:
				blacklistedCues |= CUE_CALC_ANIM;
				blacklistedCues |= CUE_MOVE;
				break;

			case STATE_GUIDE:
				blacklistedCues |= CUE_CALC_ANIM;
				blacklistedCues |= CUE_MOVE;
				break;
			}

			if (!(blacklistedCues & CUE_MOVE))
				++unk58;
			++unk5C;
			if (checkFlag(DIRECTOR_FLAG_FIRST_SIMULATION_TICK)) {
				if (mState == STATE_UNK4 || mState == STATE_UNK7)
					SMSRumbleMgr->update();
			} else {
				for (int i = 0; i < 4; ++i) {
					TMarioGamePad* pad = unk18[i];
					pad->resetButtons();

					getGamePad(i)->updateMeaning();
					getGamePad(i)->offFlag(TMarioGamePad::PAD_FLAG_0x40);
				}
			}

			u32 tmp = 0;
			if (checkFlag(DIRECTOR_FLAG_FIRST_SIMULATION_TICK))
				tmp |= 1;
			if (checkFlag(DIRECTOR_FLAG_LAST_SIMULATION_TICK))
				tmp |= 2;
			local_140.unk0 = tmp;

			// inline
			bool bVar1 = true;
			if ((unk58 & 1) || (unk58 & 2))
				bVar1 = false;

			if (bVar1)
				gpObjHitCheck->checkActorsHit();
			else
				gpObjHitCheck->clearHitNum();

			u32 movementCue = ~blacklistedCues;
			if (unk58 & 1)
				movementCue &= ~CUE_MOVEMENT_GATE_A;
			if (unk58 & 2)
				movementCue &= ~CUE_MOVEMENT_GATE_B;
			if (checkDemoFlag(DEMO_FLAG_SHINE_GET_STOP_THE_WORLD))
				mShinePfLstMov->perform(movementCue, &local_140);
			else
				mPerformListMovement->perform(movementCue, &local_140);

			u32 someCue = 0;
			if (!checkFlag(DIRECTOR_FLAG_LAST_SIMULATION_TICK))
				someCue |= CUE_CALC_ANIM;
			u32 unk30Cue = ~someCue;
			unk30->perform(unk30Cue, &local_140);
			movement();
			if (!(blacklistedCues & CUE_CALC_ANIM)) {
				if (checkDemoFlag(DEMO_FLAG_SHINE_GET_STOP_THE_WORLD))
					mShinePfLstAnm->perform(~blacklistedCues, &local_140);
				else
					mPerformListCalcAnim->perform(~blacklistedCues, &local_140);
			}

			if (checkFlag(DIRECTOR_FLAG_LAST_SIMULATION_TICK)) {
				local_140.unk0 = 0;
				unk34->perform(CUE_ALL, &local_140);
				break;
			}
		} else {
			local_140.unk0 = 0;
			unk40->perform(CUE_ALL, &local_140);
			unk38->perform(CUE_ALL, &local_140);
			unk3C->perform(CUE_ALL, &local_140);
			mPerformListGX->perform(CUE_ALL, &local_140);
			if ((gpSilhouetteManager->unk48 > 0.0f ? true : false)
			    || gpCamera->unk2C8 != -1) {
				mPerformListSilhouette->perform(CUE_ALL, &local_140);
			}
			mPerformListGXPost->perform(CUE_ALL, &local_140);
			GXInvalidateTexAll();
		}
		desiredAppState = changeState();
		offFlag(DIRECTOR_FLAG_LAST_SIMULATION_TICK
		        | DIRECTOR_FLAG_FIRST_SIMULATION_TICK);
	}

	gpMSound->mSeGateMask = prevSeGateMask;
	return desiredAppState;
}

// TODO: 99.6%, instruction-exact; retail's frame is 8 bytes larger (one more
// stack temporary, the same open class as TApplication::TApplication's
// +0x10). Spelling the copy as `gpApplication.mNextArea = local_3C` or a
// direct three-argument `set` is worse (84%) and breaks the expanded copies
// in changeState/updateGameMode.
static void decideNextStage()
{
	TGameSequence local_3C;

	int stage = SMS_getShineStage(gpApplication.mCurrArea.getStage());
	switch (stage) {
	case 0:
		local_3C.set(1, 0xff, JDrama::TFlagT<u16>());
		break;
	case 1:
	case 2:
	default:
		local_3C.set(1, 0xff, JDrama::TFlagT<u16>());
		break;
	}
	SMSGetApplication()->setNextArea(local_3C);
}

// UNUSED, 0x10c.
// TODO: dead in the shipped game and 67 instructions in the map, with no call
// site and no literal of its own to read; the body is unrecovered.
static void decideNextStageOfMiss() { }

// UNUSED, 0x64: the shine flags of the seven shadow-Mario episodes, one per
// stage. Its `stages` array is the map's `stages$3013` .sdata object, which
// places it here in source order.
static bool checkDefeatShadowMarioAll()
{
	static u8 stages[] = { 0x6, 0x10, 0x1A, 0x24, 0x2E, 0x38, 0x42 };

	int i = 0;
	do {
		if (!TFlagManager::smInstance->getShineFlag(stages[i]))
			return false;
		++i;
	} while (i < 7);

	return true;
}

// TODO: instruction-exact; frame 0x18 vs retail 0x30 (5-7 dead words with no
// stack access, none of them in ours). The result variable gives retail's
// `li r3, 0` hoisted above the first branch and the separate final zero.
// Inert: TFlagManager::getInstance()/SMSGetFlagManager() receivers at every
// site (call receivers keep their register), a compared shadow-Mario result.
static int decideNextScenario(u8 param_1)
{
	int scenario = 0;
	switch (param_1) {
	case 1:
		if (TFlagManager::smInstance->getBool(0x103AE))
			scenario = 2;
		else if (checkDefeatShadowMarioAll())
			scenario = 9;
		else if (TFlagManager::smInstance->getBool(0x10389))
			scenario = 8;
		else if (TFlagManager::smInstance->getBool(0x10386)
		    && TFlagManager::smInstance->getBool(0x10387)) {
			if (TFlagManager::smInstance->getFlag(0x40000) >= 10)
				scenario = 7;
			else
				scenario = 6;
		} else if (TFlagManager::smInstance->getBool(0x10385))
			scenario = 5;
		else if (TFlagManager::smInstance->getBool(0x10384))
			scenario = 1;
		else
			scenario = 0;
		break;
	}
	return scenario;
}

// fabricated: retail forms &gpApplication.mNextArea as a pointer before the
// inlined TGameSequence::set (`stbu`/`addi r4, r3, 0x12`), which a
// reference-returning level reproduces.
static inline TGameSequence& getNextArea() { return gpApplication.mNextArea; }

// fabricated: a case helper retail expands in changeState. Its level puts
// decideNextStage at depth 2, over the budget there, so retail calls it from
// here while its other two sites expand it.
static inline void decideNextStageOfClear()
{
	const TGameSequence& curArea = gpApplication.mCurrArea;
	if (SMS_isExMap() || curArea.getStage() == 0 || curArea.getStage() == 60) {
		getNextArea().set(curArea.getStage(), 0, 0);
	} else {
		decideNextStage();
	}
}

// TODO: every instruction and register right, frame 0x148 against 0x168
// (research c-r29: `str` declared at the top fixed the r27/r29 swap in the
// STATE_UNK1 wipe test; getFader() and getGamePad() at every pad read moved
// the frame from 0x100). Body from upstream (its accessor levels put `this`
// in r31, 98.8 -> 99.9), with our decideNextStageOfClear in STATE_UNK7:
// retail inlines the SMS_isExMap test there.
int TMarDirector::changeState()
{
	TConsoleStr* str;
	int desiredAppState = TApplication::APP_STATE_DEFAULT;
	u8 nextState        = mState;
	switch (mState) {
	case STATE_UNK0:
		switch (SMSGetApplication()->mCurrArea.getStage()) {
		case 0xf:
			nextState = STATE_UNK4;
			onTransitionFlag(TRANSITION_FLAG_STAGE_BGM_STARTED);
			break;

		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x8:
		case 0x9:
		case 0x34:
			nextState = STATE_UNK1;
			onTransitionFlag(TRANSITION_FLAG_GO_BANNER_PENDING
			                 | TRANSITION_FLAG_SCENARIO_NAME_BANNER_PENDING);
			break;

		case 1:
			if (checkDemoFlag(DEMO_FLAG_CAMERA_DEMO_ON_START)) {
				nextState = STATE_UNK1;
				onTransitionFlag(
				    TRANSITION_FLAG_GO_BANNER_PENDING
				    | TRANSITION_FLAG_SCENARIO_NAME_BANNER_PENDING);
			} else {
				nextState = STATE_UNK2;
			}
			break;

		default:
			if (JKRGetResource("/scene/map/camera/startcamera.bck")) {
				nextState = STATE_UNK1;
				onTransitionFlag(TRANSITION_FLAG_GO_BANNER_PENDING
				                 | TRANSITION_FLAG_OPENING_WIPE_PENDING);
			} else {
				nextState = STATE_UNK4;
			}
			break;
		}
		break;

	case STATE_UNK1:
		if (checkDemoFlag(DEMO_FLAG_CAMERA_DEMO_WIPE_STARTED)) {
			if (getConsole()->unk94->unk2B8 == 4) {
				nextState = STATE_UNK3;
				offDemoFlag(DEMO_FLAG_CAMERA_DEMO_WIPE_STARTED);
			}
		} else {
			TGameSequence& curArea = SMSGetApplication()->mCurrArea;
			str                    = getConsole()->unk94;
			f32 iVar2              = gpCamera->getRestDemoFrames() / 120.0f;
			if (iVar2 <= str->getWipeCloseTime()
			    || ((curArea.getStage() != 1 || curArea.getScenario() != 1)
			        && (curArea.getStage() != 1 || curArea.getScenario() != 9)
			        && getGamePad()->checkFrameMeaning(
			            TMarioGamePad::MEANING_0x1
			            | TMarioGamePad::MEANING_0x20
			            | TMarioGamePad::MEANING_0x40))) {
				onDemoFlag(DEMO_FLAG_CAMERA_DEMO_WIPE_STARTED);
				getConsole()->unk94->startCloseWipe(
				    checkTransitionFlag(TRANSITION_FLAG_OPENING_WIPE_PENDING)
				    != FALSE);
				offTransitionFlag(TRANSITION_FLAG_OPENING_WIPE_PENDING);
			}
		}
		break;

	case STATE_UNK3:
		if (SMSGetApplication()->mCurrArea.getStage() == 1) {
			if (getConsole()->unk94->unk2B8 == 6)
				nextState = STATE_UNK2;
		} else if (getConsole()->unk94->unk2B8 == 6
		           && !gpMarioOriginal->checkStatusType(
		               MARIO_STATUS_FLAG_UNK1000)) {
			nextState = STATE_UNK4;
		}
		break;

	case STATE_UNK2:
		if (!unkE0->unk26
		    && !gpMarioOriginal->checkStatusType(MARIO_STATUS_FLAG_UNK1000))
			nextState = STATE_UNK4;
		break;

	case STATE_UNK4:
		nextState = updateGameMode();
		break;

	case STATE_PAUSE_MENU:
		switch (unkAC->getNextState()) {
		case 0:
			nextState = STATE_UNK4;
			break;
		case 1:
			unkE4     = 4;
			nextState = STATE_UNK12;
			unkB4     = TApplication::APP_STATE_DONE;
			break;
		case 5:
			decideNextStage();
			unk4C &= ~DIRECTOR_FLAG_MOVIE_PENDING;
			moveStage();
			unkE4     = 2;
			nextState = STATE_UNK9;
			break;
		}
		break;

	case STATE_GUIDE:
		if (unk78->unkC4 && SMSGetApplication()->getFader()->isFullyFadedIn())
			nextState = STATE_UNK4;
		break;

	case STATE_CARD_SAVE: {
		switch (unkAC->mCardSave->getNextState()) {
		case 0:
			if (unk261 == 7) {
				TFlagManager::getInstance()->restore();
				TFlagManager::getInstance()->setBool(true, 0x30001);
				if (!TFlagManager::getInstance()->getFlag(0x40000)) {
					SMSGetApplication()->mNextArea.set(0, 0, 0);
				} else {
					SMSGetApplication()->mNextArea.set(1, 0xff, 0);
				}
				unk4C &= ~DIRECTOR_FLAG_MOVIE_PENDING;
				moveStage();
				SMSGetApplication()->getFader()->setFadeStatus(
				    TSMSFader::FADE_STATUS_FULLY_FADED_OUT);
				desiredAppState = TApplication::APP_STATE_GAMEPLAY;
			} else {
				nextState = STATE_UNK4;
			}
			break;
		case 1:
			if (unk261 == 7) {
				SMSGetApplication()->getFader()->setFadeStatus(
				    TSMSFader::FADE_STATUS_FULLY_FADED_OUT);
				desiredAppState = TApplication::APP_STATE_DONE;
			} else {
				unkE4     = 4;
				nextState = STATE_UNK12;
				unkB4     = TApplication::APP_STATE_DONE;
			}
			break;
		}
		break;
	}

	case STATE_UNK7:
		if (SMSGetApplication()->getFader()->isFullyFadedOut()
		    && (MSBgm::getHandle(2) == 0 || unk5C - unk60 >= 1200)) {
			if (TFlagManager::getInstance()->getFlag(0x20001) >= 0) {
				TFlagManager::getInstance()->setBool(true, 0x30002);
				decideNextStageOfClear();
				unk4C &= ~DIRECTOR_FLAG_MOVIE_PENDING;
				moveStage();
				unkE4 = 0xf;
				SMSGetApplication()->getFader()->setColor(
				    JUtility::TColor(0, 0, 0, 0xff));
				nextState = STATE_UNK12;
			} else {
				SMSGetApplication()->getFader()->startWipe(0xE, 0.3f, 0.0f);
				SMSGetApplication()->getFader()->setColor(
				    JUtility::TColor(0, 0, 0, 0xff));
				unk261    = 7;
				nextState = STATE_CARD_SAVE;
				getConsole()->startDisappearStar();
				getConsole()->startDisappearCoin();
			}
		}
		break;

	case STATE_UNK9:
	case STATE_UNK12:
		if (SMSGetApplication()->getFader()->isFullyFadedOut()
		    && gpMSound->checkWaveOnAram(MS_WAVE_DEFAULT))
			desiredAppState = unkB4;
		break;
	}

	if (getGamePad()->isSomethingPushed()
	    && gpCardManager->getLastStatus() != CARD_RESULT_BUSY
	    && (unk4C & DIRECTOR_FLAG_LAST_SIMULATION_TICK)
	    && !checkTransitionFlag(TRANSITION_FLAG_RESET_HANDLED)) {
		nextState = STATE_UNK12;
		onTransitionFlag(TRANSITION_FLAG_RESET_HANDLED);
		unkE4 = 4;
		unkB4 = TApplication::APP_STATE_DONE;
	}

	if (nextState != mState) {
		currentStateFinalize(nextState);
		nextStateInitialize(nextState);
		mState = nextState;
	}

	return desiredAppState;
}

void TMarDirector::currentStateFinalize(u8 next_state)
{
	// TODO: frame 0xd0 vs retail 0x120 (instruction-exact otherwise; 0x90
	// before getFader(), getGamePad()->offFlag() and getStage(), c-r29).
	// MSMainProc dual-u8 args need scenario-then-stage named locals for RTL.
	switch (mState) {
	case STATE_UNK0:
		JDrama::TNameRefGen::search<JDrama::TViewObj>("Group 2D")
		    ->unkC.off(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
		JDrama::TNameRefGen::search<JDrama::TViewObj>("Guide")->unkC.on(
		    CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);

		gpApplication.getFader()->startWipe(unkE4, 0.4f, 0.0f);
		SMSRumbleMgr->reset();
		break;

	case STATE_UNK1:
		getGamePad()->offFlag(0x1);
		gpCamera->endDemoCamera();
		mConsole->unk94->startOpenWipe();
		u8 scenario = gpApplication.mCurrArea.getScenario();
		u8 stage    = gpApplication.mCurrArea.getStage();
		MSMainProc::endStageEntranceDemo(stage, scenario);
		break;

	case STATE_UNK4:
		if (unk124 == 0)
			OSStopStopwatch(&unkE8);
		getGamePad()->offFlag(0x2);
		break;

	case STATE_UNK5:
		getGamePad()->offFlag(0x1);
		SMSRumbleMgr->finishPause();
		if (gpApplication.mCurrArea.getStage() == 1)
			THPPlayerPlay();
		break;

	case STATE_UNK10:
		getGamePad()->offFlag(0x1);
		SMSRumbleMgr->finishPause();

		JDrama::TNameRefGen::search<JDrama::TViewObj>("Group 2D")
		    ->unkC.off(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
		JDrama::TNameRefGen::search<JDrama::TViewObj>("Guide")->unkC.on(
		    CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);

		SMSSwitch2DArchive("guide", gArBkConsole);
		if (gpApplication.mCurrArea.getStage() == 1)
			THPPlayerPlay();
		break;

	case STATE_UNK11:
		getGamePad()->offFlag(0x1);
		SMSRumbleMgr->finishPause();
		if (gpApplication.mCurrArea.getStage() == 1)
			THPPlayerPlay();
		switch (unk261) {
		case 3:
			mConsole->startAppearBalloon(0x4B, true);
			break;

		case 4:
			mConsole->startAppearBalloon(0x4C, true);
			break;
		}
		break;
	}
}

// TODO: every instruction and register matches; frame 0x78 vs retail 0xb0.
// Naming the previous area's stage as a u8 for SMS_getShineStage adds 8
// (c-hs6, one lone named value, not taken).
void TMarDirector::setMario()
{
	bool cVar4 = false;
	f32 fVar1;
	const JGeometry::TVec3<f32>* pos;
	int iVar9;
	TWaterGun* waterGun;
	if (TFlagManager::getInstance()->getBool(0x30006)) {
		TFlagManager::getInstance()->setBool(false, 0x30006);
		cVar4 = true;
	}

	u8 uVar10 = unkD0;

	TMarioPositionObj* marioSetPosition
	    = JDrama::TNameRefGen::search<TMarioPositionObj>("マリオセット位置");
	if (!marioSetPosition || marioSetPosition->unkD0 == 0)
		uVar10 = 0;

	switch (unkD1) {
	case 1: {
		fVar1 = 0.0f;

		pos = nullptr;

		if (uVar10) {
			fVar1 = marioSetPosition->getUnk70(uVar10 - 1).y;
			pos   = &marioSetPosition->getUnk10(uVar10 - 1);
		}
		gpMarioOriginal->rollingStart(pos, fVar1);
	} break;

	case 2: {
		iVar9 = 0;
		switch (SMS_getShineStage(gpApplication.mPrevArea.getStage())) {
		case 5:
		case 6:
		case 7:
			iVar9 = 1;
			break;
		case 8:
			iVar9 = 2;
			break;
		}
		fVar1 = 0.0f;

		pos = nullptr;

		if (uVar10) {
			fVar1 = marioSetPosition->getUnk70(uVar10 - 1).y;
			pos   = &marioSetPosition->getUnk10(uVar10 - 1);
		}
		gpMarioOriginal->returnStart(pos, fVar1, cVar4, iVar9);
	} break;

	case 4:
		gpMarioOriginal->toroccoStart();
		break;

	case 0:
	default:
		pos = nullptr;
		if (uVar10)
			pos = &marioSetPosition->getUnk10(uVar10 - 1);
		gpMarioOriginal->waitingStart(pos, 0.0f);
		break;
	}

	const TGameSequence& currArea = gpApplication.mCurrArea;

	// Every stage that is not one of the listed exceptions restores the
	// nozzle the save file remembers; the exception list is read off the
	// ROM's comparison tree (groups 1-6, 8-9, 0x10, 0x2C, 0x34, 0x39).
	if (gpMarioOriginal->checkFlag(MARIO_FLAG_HAS_FLUDD)) {
		switch (currArea.getStage()) {
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 8:
		case 9:
		case 0x10:
		case 0x2C:
		case 0x34:
		case 0x39:
			break;

		case 0x3C:
			gpMarioOriginal->getFludd()->changeNozzle(TWaterGun::Rocket, true);
			break;

		default: {
			waterGun = gpMarioOriginal->getFludd();
			waterGun->changeNozzle(
			    (TWaterGun::TNozzleType)TFlagManager::getInstance()->getFlag(
			        0x40004),
			    true);
			gpMarioOriginal->getFludd()->changeNozzle(TWaterGun::Spray, true);
		} break;
		}
	}

	u8 uVar6 = SMS_getShineIDofExStage(currArea.getStage());
	if (uVar6 != 0xff && TFlagManager::getInstance()->getShineFlag(uVar6) == 0)
		gpMarioOriginal->offFlag(MARIO_FLAG_HAS_FLUDD);
}

// TODO: every instruction matches except the saved-register rotation (retail
// r31 this, r30 pool, r29 currSeq, r28 camera name, r27 gpApplication) and the
// frame (retail 0x160, ours 0x130 with the header accessors getGamePad(),
// getStage(), getConsole() and getPortNum() at every site, research c-r29).
// Top-declared name, const/ref-to-app spellings were inert.
void TMarDirector::nextStateInitialize(u8 next_state)
{
	TGameSequence& currSeq = gpApplication.mCurrArea;

	switch (next_state) {
	case 1: {
		const char* pcVar8 = "startcamera";
		getGamePad()->onFlag(0x1);
		unk68 = 0;
		if (currSeq.getStage() == 1 && checkUnk4EFlag(2)) {
			if (currSeq.unk1 == 8) {
				switch (TFlagManager::smInstance->getFlag(0x60003)) {
				case 0:
					if (TFlagManager::smInstance->getFlag(0x40000) >= 0x14)
						pcVar8 = "mareopen_startcamera";
					break;
				case 1:
					pcVar8 = "yoshi_startcamera";
					break;
				case 2:
					pcVar8 = "turbo_startcamera";
					break;
				case 3:
					pcVar8 = "rocket_startcamera";
					break;
				}
			} else {
				if (TFlagManager::smInstance->getBool(0x50001)) {
					pcVar8 = "sinkricco";
				} else {
					if (TFlagManager::smInstance->getBool(0x50002))
						pcVar8 = "sinkmamma";
				}
			}
		}
		SMSGetCamera()->startDemoCamera(pcVar8, nullptr, -1, 0.0f, true);
		if (unk50 & 4) {
			getConsole()->unk94->startAppearScenario();
			unk50 &= ~0x4;
		}
		{
			u8 scenario = currSeq.unk1;
			u8 stage    = currSeq.getStage();
			MSMainProc::startStageEntranceDemo(stage, scenario);
		}
		break;
	}

	case 3:
		unk68 = 0;
		if (!(unk50 & 1)) {
			u8 scenario = currSeq.unk1;
			u8 stage    = currSeq.getStage();
			MSMainProc::startStageBGM(stage, scenario);
			setMario();
			unk50 |= 1;
		}
		break;

	case 2:
		if (!(unk50 & 1)) {
			u8 scenario = currSeq.unk1;
			u8 stage    = currSeq.getStage();
			MSMainProc::startStageBGM(stage, scenario);
			setMario();
			unk50 |= 1;
		}
		if (mMap != 0xf)
			getConsole()->unkC.off(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
		if (currSeq.getStage() == 1)
			THPPlayerPlay();
		break;

	case 4:
		if (mState <= STATE_UNK3 && mMap != 0xf)
			getConsole()->unkC.off(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
		if (unk50 & 2) {
			getConsole()->unk94->startAppearGo();
			unk50 &= ~0x2;
		}
		if (!(unk50 & 1)) {
			u8 scenario = currSeq.unk1;
			u8 stage    = currSeq.getStage();
			MSMainProc::startStageBGM(stage, scenario);
			setMario();
			unk50 |= 1;
		}
		if (!unk124)
			OSStartStopwatch(&unkE8);
		getGamePad()->onFlag(0x2);
		break;

	case 12:
		if (currSeq.getStage() == 1)
			THPPlayerStop();
	// !!!fallthrough!!!
	case 9: {
		gpApplication.getFader()->startWipe(unkE4, 0.4f, 0.0f);
		if (unkE4 == 8)
			SMSGetMSound()->startSoundSystemSE(MSD_SE_MA_INTO_DOKAN, 0, nullptr,
			                                   0);
		MSound* sound = gpMSound;
		sound->fadeOutAllSound(SMSGetVSyncTimesPerSec() * 0.4f);
		SMSRumbleMgr->reset();
		for (int i = 0; i < 4; ++i)
			JUTGamePad::CRumble::stopMotor(getGamePad(i)->getPortNum());
		break;
	}

	case 5:
		if (currSeq.getStage() == 1)
			THPPlayerPause();
		SMSRumbleMgr->startPause();
		unkAC->setDrawStart();
		for (int i = 0; i < 4; ++i)
			JUTGamePad::CRumble::stopMotor(getGamePad(i)->getPortNum());
		getGamePad()->onFlag(0x1);
		break;

	case 10:
		if (currSeq.getStage() == 1)
			THPPlayerPause();
		SMSRumbleMgr->startPause();
		for (int i = 0; i < 4; ++i)
			JUTGamePad::CRumble::stopMotor(getGamePad(i)->getPortNum());
		getGamePad()->onFlag(0x1);
		JDrama::TNameRefGen::search<JDrama::TViewObj>("Group 2D")
		    ->unkC.on(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
		JDrama::TNameRefGen::search<JDrama::TViewObj>("Guide")->unkC.off(
		    CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
		SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_WIPE_IN, 0, nullptr, 0);
		gpApplication.getFader()->startWipe(6, 1.0f, 0.0f);
		unk78->setup(nullptr);
		unk78->startMoveCursor();
		break;

	case 11:
		if (currSeq.getStage() == 1)
			THPPlayerPause();
		SMSRumbleMgr->startPause();
		unkAC->mCardSave->init(unk261);
		for (int i = 0; i < 4; ++i)
			JUTGamePad::CRumble::stopMotor(getGamePad(i)->getPortNum());
		switch (unk261) {
		case 3:
		case 4:
			SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_COLLECT_DELIGHT, 0,
			                                   nullptr, 0);
			break;
		}
		getGamePad()->onFlag(0x1);
		break;

	case 7:
		gpMarDirector->getConsole()->unk94->startAppearMiss();
		TFlagManager::smInstance->decFlag(0x20001, 1);
		unk60 = unk5C;
		gpApplication.getFader()->setColor(JUtility::TColor(0, 0, 0, 0xff));
		if (TFlagManager::smInstance->getFlag(0x20001) >= 0) {
			MSBgm::startBGM(MSD_BGM_MISS);
			if (checkUnk4EFlag(8))
				gpApplication.getFader()->startWipe(2, 0.0f, 2.0f);
			else
				gpApplication.getFader()->startWipe(10, 0.0f, 2.2f);
		} else {
			MSBgm::startBGM(MSD_BGM_GAMEOVER);
			gpApplication.getFader()->startWipe(0xD, 0.0f, 2.0f);
		}
		break;
	}
}

// TODO: instructions exact; frame 0xf0 vs retail 0x130 (0xa8 before
// getGamePad() at the pad reads and getTalkMode()/getTalkingNPC() at the
// talk reads, research c-r29; 0xe0 before getCurrentMap() and getConsole(),
// c-hs5; the NPC accessor as openTalkWindow's
// argument changes code). Body from upstream
// (its flag accessors fixed the r29/r30 swap, 99.8 -> 99.93), with the ROM's
// GET_SHINE fanfare, the `1` demo flag and the 0x40 clear kept from ours.
u8 TMarDirector::updateGameMode()
{
	u8 r29 = mState;

	switch (unk124) {
	case 0:
		if (!checkFlag(~(DIRECTOR_FLAG_SHINE_TAKEN
		                 | DIRECTOR_FLAG_LAST_SIMULATION_TICK
		                 | DIRECTOR_FLAG_FIRST_SIMULATION_TICK))) {
			if (SMS_CheckMarioFlag(MARIO_FLAG_GAME_OVER)) {
				unk4C |= DIRECTOR_FLAG_GAME_OVER_PENDING;
				break;
			}

			if (getCurrentMap() != 15) {
				if (getGamePad()->testTrigger(0x10)) {
					r29 = STATE_GUIDE;
					break;
				}

				if (getGamePad()->checkFrameMeaning(TMarioGamePad::MEANING_0x1)) {
					if (gpMarioOriginal->checkActionThing3()) {
						r29 = STATE_PAUSE_MENU;
						break;
					}

					SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_NOT_COLLECT, 0,
					                                   nullptr, 0);
				}
			}
		} else {
			if (checkFlag(DIRECTOR_FLAG_GAME_OVER_PENDING)) {
				offFlag(DIRECTOR_FLAG_GAME_OVER_PENDING);
				r29 = STATE_UNK7;
				TFlagManager::getInstance()->setFlag(0x40002, 0);
				break;
			}

			if (checkFlag(DIRECTOR_FLAG_SHINE_GET_PENDING)) {
				offFlag(DIRECTOR_FLAG_SHINE_GET_PENDING);
				unk126 = 3;

				TGCConsole2* console = gpMarDirector->getConsole();
				console->unk94->startAppearShineGet();
				console->unk47 = 1;
				MSBgm::startBGM(MSD_BGM_GET_SHINE);
				TFlagManager::getInstance()->setBool(true, 0x30006);
				TFlagManager::getInstance()->setShineFlag(unk25C->getEventId());
				f32 fVar3     = unkDC->mRate;
				f32 fadeInSec = 1.0f;
				unkDC->registFadeout(fadeInSec * fVar3, fVar3 * 5.3333333f);
				onFlag(DIRECTOR_FLAG_SHINE_TAKEN
				       | DIRECTOR_FLAG_CARD_SAVE_PENDING
				       | DIRECTOR_FLAG_STAGE_TRANSITION_PENDING);
				unk261 = 6;
				decideNextStage();
				break;
			}

			if (checkFlag(DIRECTOR_FLAG_DEMO_PENDING)) {
				unk126 = 3;
				break;
			}

			if (checkFlag(DIRECTOR_FLAG_CARD_SAVE_PENDING)) {
				offFlag(DIRECTOR_FLAG_CARD_SAVE_PENDING);
				r29 = STATE_CARD_SAVE;
				break;
			}

			if (checkFlag(DIRECTOR_FLAG_GATE_DEMO_STAGE_TRANSITION_PENDING)) {
				offFlag(DIRECTOR_FLAG_GATE_DEMO_STAGE_TRANSITION_PENDING);
				onFlag(DIRECTOR_FLAG_STAGE_TRANSITION_PENDING);
				unk126                        = 3;
				const TGameSequence& nextArea = SMSGetApplication()->mNextArea;
				if (nextArea.getStage() == 5) {
					fireStartDemoCamera("hodai_dpt_pinna1", nullptr, -1, 0.0f,
					                    false, nullptr, 0, nullptr, 0);
					if (unk254 != nullptr)
						unk254->startDemo();
					break;
				}

				if (nextArea.getStage() == 6) {
					fireStartDemoCamera("camera_sirena_gate_in", nullptr, -1,
					                    0.0f, false, nullptr, 0, nullptr, 0);
					break;
				}

				if (nextArea.getStage() == 8) {
					fireStartDemoCamera("camera_monte_gate_in", nullptr, -1,
					                    0.0f, false, nullptr, 0, nullptr, 0);
					break;
				}

				break;
			}

			if (checkFlag(DIRECTOR_FLAG_ACTOR_DEMO_STAGE_TRANSITION_PENDING)) {
				offFlag(DIRECTOR_FLAG_ACTOR_DEMO_STAGE_TRANSITION_PENDING);
				onFlag(DIRECTOR_FLAG_STAGE_TRANSITION_PENDING);
				unk126 = 3;
				fireStartDemoCamera(nullptr, nullptr, -1, 0.0f, false, nullptr,
				                    0, unk250, 1);
				break;
			}

			if (checkFlag(DIRECTOR_FLAG_STAGE_TRANSITION_PENDING)) {
				moveStage();
				r29 = STATE_UNK9;
				break;
			}
		}
		break;

	case 2:
		if (checkFlag(DIRECTOR_FLAG_DEMO_PENDING)) {
			unk126 = 4;
		} else {
			if (unkB0->getTalkMode() == 0)
				unk126 = 0;
		}
		break;

	case 3:
	case 4: {
		bool bVar5  = false;
		bool uVar15 = 0;
		if (checkFlag(DIRECTOR_FLAG_END_DEMO_PENDING)) {
			uVar15 = 1;
			bVar5  = true;
			offFlag(DIRECTOR_FLAG_END_DEMO_PENDING);
		} else {
			if (!gpCamera->getRestDemoFrames()) {
				if (!MSBgm::getHandle(2) || unk5C - unk60 >= 720) {
					bVar5  = true;
					uVar15 = unk12C[unk24D].unk10;
				}
			}
		}

		if (bVar5) {
			u32 prev = unk24D++;
			unk24D &= 0x7;
			TDemoInfo* info = &unk12C[prev];
			if (unk24D != unk24C) {
				gpCamera->endDemoCamera();
				if (info->unk14 != nullptr)
					(*info->unk14)(info->unk18, 1);

				TDemoInfo* next = &unk12C[unk24D];
				gpCamera->startDemoCamera(next->unk0, next->unk4, next->unk8,
				                          next->unkC, next->unk10);
				if (next->unk14 != nullptr)
					(*next->unk14)(next->unk18, 0);
			} else {
				offFlag(DIRECTOR_FLAG_DEMO_PENDING);
				unk126 = unk124 == 4 ? 2 : 0;
				if (uVar15 != 0)
					gpCamera->endDemoCamera();
				if (info->unk14 != nullptr)
					(*info->unk14)(info->unk18, 1);
			}
		}
	} break;
	}

	if (unk24D == unk24C)
		offFlag(DIRECTOR_FLAG_DEMO_PENDING);

	unk125 = unk124;

	if (unk124 != unk126) {
		switch (unk124) {
		case 2:
			if (unk126 == 0) {
				unkA0 = 0;
				unkA4 = 0;
				getGamePad()->offFlag(TMarioGamePad::PAD_FLAG_0x8);
				OSStartStopwatch(&unkE8);
			}
			break;

		case 3:
		case 4:
			if (unk124 == 4)
				MSMainProc::fromTalkingCameraDemo(unk124 == 4);
			else
				MSMainProc::fromInnerCameraDemo();
			getGamePad()->offFlag(TMarioGamePad::PAD_FLAG_0x10);
			OSStartStopwatch(&unkE8);
			break;
		}

		switch (unk126) {
		case 0:
			break;

		case 1:
			getTalkingNPC()->onLiveFlag(LIVE_FLAG_UNK40000);
			getTalkingNPC()->unkC.off(CUE_MOVE | CUE_CALC_ANIM);
			getGamePad()->onFlag(TMarioGamePad::PAD_FLAG_0x8);
			OSStopStopwatch(&unkE8);
			break;

		case 2:
			if (unk124 == 1)
				unkB0->openTalkWindow(unkA0);
			break;

		case 3:
		case 4:
			if (unk126 == 4)
				MSMainProc::toTalkingCameraDemo();
			else
				MSMainProc::toInnerCameraDemo();
			getGamePad()->onFlag(TMarioGamePad::PAD_FLAG_0x10);
			if ((int)unk12C[unk24D].unk20.get() == 1) {
				gpCamera->startGateDemoCamera(unk12C[unk24D].unk1C);
			} else {
				gpCamera->startDemoCamera(unk12C[unk24D].unk0,
				                          unk12C[unk24D].unk4,
				                          unk12C[unk24D].unk8,
				                          unk12C[unk24D].unkC,
				                          unk12C[unk24D].unk10);
				if (unk12C[unk24D].unk14 != nullptr)
					(*unk12C[unk24D].unk14)(
					    unk12C[unk24D].unk18, 0);
			}
			OSStopStopwatch(&unkE8);
			unk60 = unk5C;
			break;
		}

		unk124 = unk126;
	}

	if (unk128 & 0x1) {
		unk128 &= ~0x1;
		unk128 |= 0x2;
	} else {
		unk128 &= ~0x2;
	}

	return r29;
}

// Body from upstream, whose accessor levels (SMSGetApplication()->getFader(),
// getStage()) fixed the register assignment; the nozzle store keeps the
// ROM's `mr r5, r0`. The frame closed with the binder-shaped getFader(), raw
// `unk1` for the two scenario tests (as the stores beside them) and
// getFludd() for the nozzle read (research c-r29, hsearch).
void TMarDirector::moveStage()
{
	unkB4 = TApplication::APP_STATE_GAMEPLAY;
	unkE4 = 15;
	SMSGetApplication()->getFader()->setColor(JUtility::TColor(0, 0, 0, 0xff));

	u8 sVar4 = SMS_getShineStage(SMSGetApplication()->mNextArea.getStage());
	u8 sVar5 = SMS_getShineStage(SMSGetApplication()->mCurrArea.getStage());
	if (sVar4 != sVar5)
		TFlagManager::getInstance()->setFlag(0x40002, 0);

	TGameSequence& nextArea = SMSGetApplication()->mNextArea;

	if (nextArea.unk1 == 0xff)
		switch (nextArea.getStage()) {
		case 1:
			unkE4         = 2;
			nextArea.unk1 = decideNextScenario(nextArea.getStage());
			TFlagManager::getInstance()->setFlag(0x40003, 0);
			break;

		case 13: {
			unkE4     = 2;
			u32 thing = 0;
			switch (TFlagManager::getInstance()->getFlag(0x40003)) {
			case 0:
				thing = 0;
				break;
			case 2:
				thing = 1;
				break;
			case 4:
				thing = 2;
				break;
			case 5:
				thing = 3;
				break;
			case 6:
				thing = 4;
				break;
			case 7:
				thing = 5;
				break;
			}
			nextArea.unk1 = thing;
			break;
		}

		case 0x3A: {
			unkE4     = 2;
			u32 thing = 0;
			switch (TFlagManager::getInstance()->getFlag(0x40003)) {
			case 0:
				thing = 1;
				break;
			case 7:
				thing = 0;
				break;
			}
			nextArea.unk1 = thing;
			break;
		}

		case 7: {
			unkE4     = 2;
			u32 thing = 0;
			switch (TFlagManager::getInstance()->getFlag(0x40003)) {
			case 1:
				thing = 0;
				break;
			case 2:
				thing = 1;
				break;
			case 3:
			case 4:
				thing = 2;
				break;
			case 6:
				thing = 3;
				break;
			case 7:
				thing = 4;
				break;
			}
			nextArea.unk1 = thing;
			break;
		}

		case 14: {
			unkE4     = 2;
			u32 thing = 0;
			switch (TFlagManager::getInstance()->getFlag(0x40003)) {
			case 3:
				thing = 0;
				break;
			case 4:
				thing = 1;
				break;
			}
			nextArea.unk1 = thing;
			break;
		}

		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 8:
#if defined(VERSION_GMSE01)
			unkE4 = 8;
			unkB4 = TApplication::APP_STATE_TITLE;
#else
			unkE4 = 2;
			unkB4 = TApplication::APP_STATE_BOOT;
#endif
			break;

		case 9:
			SMSGetApplication()->getFader()->setColor(
			    JUtility::TColor(0xD2, 0xD2, 0xD2, 0xFF));
			unkB4 = TApplication::APP_STATE_TITLE;
			break;

		case 0x34:
			unkE4         = 8;
			nextArea.unk1 = 0;
			TFlagManager::getInstance()->setFlag(0x40003, 0);
			break;

		case 0:
			nextArea.unk1 = 0;
			TFlagManager::getInstance()->setFlag(0x40003, 0);
			break;

		default:
			unkE4         = 2;
			nextArea.unk1 = 0;
			break;
		}

	if (nextArea.unk1 != 0xff) {
		if (unk4C & DIRECTOR_FLAG_MOVIE_PENDING) {
			unkE4 = 15;
			SMSGetApplication()->getFader()->setColor(
			    JUtility::TColor(0, 0, 0, 0xff));
			unkB4 = TApplication::APP_STATE_MOVIE;
		} else {
			unkB4 = TApplication::APP_STATE_GAMEPLAY;
		}
	}

	if (gpMarioOriginal->checkFlag(MARIO_FLAG_HAS_FLUDD)) {
		int nozzle      = gpMarioOriginal->getFludd()->mSecondNozzle;
		s32 savedNozzle = nozzle;
		if (nozzle == 3)
			savedNozzle = 4;
		TFlagManager::getInstance()->setFlag(0x40004, savedNozzle);
	}
}

JStage::TObject* TMarDirector::JSGFindObject(const char* param_1,
                                             JStage::TEObject param_2) const
{
	if (strcmp("cam_int1", param_1) == 0) {
		TMarDirector* self = const_cast<TMarDirector*>(this);
		return (JDrama::TCamera*)self->search("camera 1");
	}

	if (strcmp("mario", param_1) == 0) {
		TMarDirector* self = const_cast<TMarDirector*>(this);
		return (JDrama::TActor*)self->search("マリオ");
	}

	return JDrama::TDirector::JSGFindObject(param_1, param_2);
}
