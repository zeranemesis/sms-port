#include <GC2D/ProgSelect.hpp>
#include <stdio.h>
#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/JUtility/JUTResFont.hpp>
#include <JSystem/J2D/J2DOrthoGraph.hpp>
#include <JSystem/J2D/J2DPrint.hpp>
#include <System/Application.hpp>
#include <System/MarioGamePad.hpp>
#include <GC2D/MessageUtil.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>

void TProgSelect::setLang(s32 lang)
{
	static const char* filename[] = {
	    "/nintendo/progmessage_en.bmg", "/nintendo/progmessage_ge.bmg",
	    "/nintendo/progmessage_fr.bmg", "/nintendo/progmessage_sp.bmg",
	    "/nintendo/progmessage_it.bmg",
	};

	const char* fileName = filename[lang];
	unk130 = JKRFileLoader::getGlbResource(fileName);
	snprintf(unk1C, 0x100, SMSGetMessageData(unk130, 0));
	snprintf(unk120[0]->getStringPtr(), 0x20, SMSGetMessageData(unk130, 4));
	snprintf(unk120[1]->getStringPtr(), 0x20, SMSGetMessageData(unk130, 1));
}

TProgSelect::TProgSelect(u8 param_1, const char* name)
    : JDrama::TViewObj(name)
    , mPulsingTimer(255)
    , mSelection(param_1)
    , mIncreasePulsing(false)
    , mHideTextBoxes(false)
{
	f32 sync     = SMSGetVSyncTimesPerSec();
	unk128       = 0;
	mRefreshRate = sync;
	unk130       = nullptr;
	char* yesText = new char[0x20];
	unk120[0] = new J2DTextBox(gpSystemFont->getResFont(), yesText);
	char* noText = new char[0x20];
	unk120[1] = new J2DTextBox(gpSystemFont->getResFont(), noText);
	setLang(0);

	unk120[0]->setFontSize(28, 28);
	unk120[1]->setFontSize(28, 28);
	if (!mSelection) {
		unk120[0]->setBlackWhite(0x00ff0000, 0x00ff00ff);
		unk120[1]->setBlackWhite(0x7f7f7f00, 0x7f7f7fff);
	} else {
		unk120[1]->setBlackWhite(0x00ff0000, 0x00ff00ff);
		unk120[0]->setBlackWhite(0x7f7f7f00, 0x7f7f7fff);
	}
}

void TProgSelect::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_MOVE) {
		if (mIncreasePulsing) {
			mPulsingTimer += 16;
			if (mPulsingTimer > 255) {
				mPulsingTimer    = 255;
				mIncreasePulsing = false;
			}
		} else {
			mPulsingTimer -= 16;
			if (mPulsingTimer < 0) {
				mPulsingTimer    = 0;
				mIncreasePulsing = true;
			}
		}

		u32 prevSelection = mSelection;
		if (mGamePad->checkFrameMeaning(TMarioGamePad::MEANING_MENU_LEFT)) {
			if (mSelection) {
				mSelection = 0;
				unk128     = 0;
			}
		} else if (mGamePad->checkFrameMeaning(
		               TMarioGamePad::MEANING_MENU_RIGHT)) {
			if (mSelection != 1) {
				mSelection = 1;
				unk128     = 0;
			}
		} else if (mGamePad->checkFrameMeaning(TMarioGamePad::MEANING_MENU_A)
		           || thing()) {
			{
				if (!mSelection) {
					snprintf(unk1C, 256, SMSGetMessageData(unk130, 3));
					OSSetEuRgb60Mode(1);
				} else {
					snprintf(unk1C, 256, SMSGetMessageData(unk130, 2));
					OSSetEuRgb60Mode(0);
				}
				mHideTextBoxes = true;
			}
		}

		if (prevSelection != mSelection) {
			mPulsingTimer = 255;
			unk120[mSelection]->setBlackWhite(0x00ff0000, 0x00ff00ff);
			unk120[prevSelection]->setBlackWhite(0x7f7f7f00, 0x7f7f7fff);
			unk120[prevSelection]->setAlpha(255);
		}

		unk120[mSelection]->setBlackWhite(0x00ff0000,
		                                  mPulsingTimer + 0x00ff0000);
	}

	if (cue & CUE_DRAW) {
		J2DOrthoGraph local_110(graphics->getViewport());
		local_110.setup2D();
		J2DPrint JStack_174(gpSystemFont, 0);
		JStack_174.setUnk50(32);
		JStack_174.printReturn(unk1C, 300, 160, HBIND_CENTER, VBIND_TOP, 175,
		                       300, 255);
		if (!mHideTextBoxes) {
			unk120[0]->draw(240, 400);
			unk120[1]->draw(340, 400);
		}
	}

	char trahs[0x10];
}
