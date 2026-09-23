#include <GC2D/Guide.hpp>
#include <GC2D/BoundPane.hpp>
#include <GC2D/ExPane.hpp>
#include <GC2D/MessageUtil.hpp>
#include <GC2D/ScrnFader.hpp>
#include <JSystem/J2D/J2DOrthoGraph.hpp>
#include <JSystem/J2D/J2DPicture.hpp>
#include <JSystem/J2D/J2DScreen.hpp>
#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/JDrama/JDRGraphics.hpp>
#include <JSystem/JGeometry/JGVec3.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <JSystem/JUtility/JUTResFont.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <MSound/MSound.hpp>
#include <Player/MarioAccess.hpp>
#include <System/Application.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/MarioGamePad.hpp>
#include <System/StageUtil.hpp>
#include <stdio.h>
#include <string.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <System/DummyStrings.hpp>

static u8 setup_wait;

static u32 scNormalStageTable[] = { 0, 1, 2, 3, 4, 13, 6, 8, 9, 10 };

TGuide::TGuide(const char* name)
    : JDrama::TViewObj(name)
    , unk10(8)
    , unkBC(nullptr)
    , unkC0(nullptr)
    , unkC4(0)
    , unkC5(0)
    , unk160(0xff)
    , unk164(1)
    , unk434(0, 0, 0, 0)
    , unk480(-1)
    , unk48C(0, 0, 0, 0)
{
}

void TGuide::load(JSUMemoryInputStream& stream)
{
	unkC5 = 0;
	JDrama::TNameRef::load(stream);

	JKRMemArchive* archive = gpMarDirector->unkD8;
	setup(archive);

	unkBC = new J2DSetScreen("guide_1.blo", (JKRArchive*)archive);
	((J2DTextBox*)unkBC->search('a_ic'))->setFont(gpSystemFont);
	((J2DTextBox*)unkBC->search('a_tx'))->setFont(gpSystemFont);
	((J2DTextBox*)unkBC->search('b_ic'))->setFont(gpSystemFont);
	((J2DTextBox*)unkBC->search('b_tx'))->setFont(gpSystemFont);

	unk124 = nullptr;
	unk124 = (J2DTextBox*)unkBC->search('s_mn');
	SMSMakeTextBuffer(unk124, 0x1a);
	unk124->setFont(gpSystemFont);

	for (int i = 0; i < 10; ++i) {
		char buffer[256];
		snprintf(buffer, 0xff, "/guide/timg/coin_number_%d.bti", i);
		unkC8[i] = new JUTTexture((const ResTIMG*)JKRGetResource(buffer));
	}

	unkF4 = unkBC->search('ss_i');
	for (int i = 0; i < 2; ++i)
		unkF8[i] = (J2DPicture*)unkBC->search('ss_1' + i);

	unk100 = unkBC->search('sq_i');
	for (int i = 0; i < 2; ++i)
		unk104[i] = unkBC->search('sq_1' + i);

	for (int i = 0; i < 3; ++i)
		unk10C[i] = (J2DPicture*)unkBC->search('sc_1' + i);

	unk118 = unkBC->search('sc_s');
	for (int i = 0; i < 2; ++i)
		unk11C[i] = (J2DPicture*)unkBC->search('sb_1' + i);

	JUTTexture* cursorTex = new JUTTexture(
	    (const ResTIMG*)JKRGetResource("/guide/timg/guide_cursor_2.bti"));
	for (int i = 0; i < 2; ++i) {
		unk128[i] = new TExPane(unkBC, 'cu_a' + i);
		J2DPicture* pic = (J2DPicture*)unk128[i]->getPane();
		pic->insert(cursorTex, pic->mTextureNum, 0.0f);
	}

	for (int i = 0; i < 13; ++i) {
		u32 tag = (i / 10 << 8) + i % 10 + '00';
		unk168[i] = unkBC->search(tag);
		unk1C0[i] = new TExPane(unkBC, (tag << 16) + '_0');
		unk218[i] = unk1C0[i]->getPane()->getBounds();
		unk378[i] = new TExPane(unkBC, (tag << 16) + '_1');
		((J2DTextBox*)unkBC->search((tag << 16) + '_3'))->setFont(gpSystemFont);
		((J2DTextBox*)unkBC->search((tag << 16) + '_5'))->setFont(gpSystemFont);
	}

	unk168[13] = unkBC->search('20');
	unk1C0[13] = new TExPane(unkBC, 'lwin');
	unk218[13] = unk1C0[13]->getPane()->getBounds();
	unk378[13] = new TExPane(unkBC, 'llin');

	for (int i = 0; i < 10; ++i)
		unk44C[i] = unkBC->search('pn00' + i);

	unk430 = unkBC->search('01mi');
	unk434 = unkBC->search('01_9')->getBounds();
	unk474 = JKRGetResource("/cmn2d/stagename.bmg");

	unk134 = (J2DPicture*)unkBC->search('10');
	// TODO: the ROM keeps a second copy of the new texture pointer here
	unk134->insert(new JUTTexture((const ResTIMG*)JKRGetResource(
	                   "/guide/timg/guide_draw_sun_2.bti")),
	               unk134->mTextureNum, 0.0f);
	unk138 = (J2DPicture*)unkBC->search('13');
	// TODO: the ROM keeps a second copy of the new texture pointer here
	unk138->insert(new JUTTexture((const ResTIMG*)JKRGetResource(
	                   "/guide/timg/guide_draw_ship_2.bti")),
	               unk138->mTextureNum, 0.0f);
	unk13C = (J2DPicture*)unkBC->search('16');
	// TODO: the ROM keeps a second copy of the new texture pointer here
	unk13C->insert(new JUTTexture((const ResTIMG*)JKRGetResource(
	                   "/guide/timg/guide_draw_palmtree_2.bti")),
	               unk13C->mTextureNum, 0.0f);
	unk140 = (J2DPicture*)unkBC->search('17');
	// TODO: the ROM keeps a second copy of the new texture pointer here
	unk140->insert(new JUTTexture((const ResTIMG*)JKRGetResource(
	                   "/guide/timg/guide_draw_palmtree_1.bti")),
	               unk140->mTextureNum, 0.0f);
	unk144 = (J2DPicture*)unkBC->search('18');
	// TODO: the ROM keeps a second copy of the new texture pointer here
	unk144->insert(new JUTTexture((const ResTIMG*)JKRGetResource(
	                   "/guide/timg/guide_draw_fish_2.bti")),
	               unk144->mTextureNum, 0.0f);

	unk148 = (J2DPicture*)unkBC->search('11');
	unk14C = (J2DPicture*)unkBC->search('14');
	unk150 = (J2DPicture*)unkBC->search('15');
	unk154 = unkBC->search('12');
	unk154->setBasePosition(J2DBasePosition_4);
	unk158 = unkBC->search('19');
	unk158->setBasePosition(J2DBasePosition_4);

	void* guideMessage = JKRGetResource("/guide/guidemess.bmg");
	for (int i = 0; i < 13; ++i) {
		u32 tag = (i / 10 << 24) + 0x30300000 + (i % 10 << 16);

		J2DTextBox* name = (J2DTextBox*)unkBC->search(tag + '_3');
		SMSMakeTextBuffer(name, 0x40);
		name->setFont(gpSystemFont);
		strncpy(name->getStringPtr(), SMSGetMessageData(guideMessage, i + 13),
		        0x40);

		J2DTextBox* text = (J2DTextBox*)unkBC->search(tag + '_5');
		SMSMakeTextBuffer(text, 0x200);
		text->setFont(gpSystemFont);
		strncpy(text->getStringPtr(), SMSGetMessageData(guideMessage, i),
		        0x200);
	}

	unk478 = new TExPane(unkBC, 'mark');
	unk444 = new TBoundPane(unkBC, '20');
	resetObjects();
	unkC5 = 1;
}

void TGuide::resetObjects()
{
	int total = 0;
	for (u32 i = 0; i < 13; ++i) {
		if (i >= 10)
			continue;

		unk14[i].unk0 = 0;

		int shineNum = 0;
		if (i != 0 && i != 1) {
			for (u32 j = 0; j < 8; ++j)
				if (SMS_isGetShine(i, j, false))
					++shineNum;
		}
		int clampedShineNum = shineNum < 100 ? shineNum : 99;
		unk14[i].mShineNum  = clampedShineNum;
		total += clampedShineNum;

		int etcShineNum = 0;
		if (i != 0 && i != 1) {
			if (SMS_isGetShine(i, 1, true))
				etcShineNum = 1;
			if (SMS_isGetShine(i, 2, true))
				++etcShineNum;
		}
		// TODO: target has an extra branch around both clamps below
		if (etcShineNum >= 10)
			etcShineNum = 9;
		unk14[i].mEtcShineNum = etcShineNum;
		total += etcShineNum;

		int coinNum
		    = (u16)TFlagManager::getInstance()->getFlag(0x20005 + i);
		unk14[i].mCoinNum = coinNum < 1000 ? coinNum : 999;

		unk14[i].mHundredCoinShine = SMS_isGetShine(i, 0, true);
		if (unk14[i].mHundredCoinShine)
			++total;

		int blueCoinNum = 0;
		if (i != 0) {
			for (u8 j = 0; j < 50; ++j)
				if (TFlagManager::getInstance()->getBlueCoinFlag(
				        scNormalStageTable[i], j))
					++blueCoinNum;
		}
		if (blueCoinNum >= 1000)
			blueCoinNum = 999;
		unk14[i].mBlueCoinNum = blueCoinNum;

		if (TFlagManager::getInstance()->getBool(0x103A5 + i)) {
			unk44C[i]->show();
			unk168[i]->show();
		} else {
			unk44C[i]->hide();
			unk168[i]->hide();
		}
	}

	unk14[9].unk0 = 1;
	u8 corona     = 0;
	for (u8 j = 0; j < 50; ++j)
		if (TFlagManager::getInstance()->getBlueCoinFlag(scNormalStageTable[9],
		                                                 j))
			++corona;
	unk14[9].mBlueCoinNum = corona;

	s16 extra = 0;
	if (TFlagManager::getInstance()->getBool(0x10056))
		extra = 1;
	if (TFlagManager::getInstance()->getBool(0x10058))
		++extra;
	unk14[0].mShineNum = extra;
	total += extra;
	unk14[1].mShineNum = TFlagManager::getInstance()->getFlag(0x40000) - total;

	changeBotStatus(-1);
	resetScore();
	unk128[0]->getPane()->show();
	unk128[1]->getPane()->show();
}

void TGuide::resetScore()
{
	u8 etcShineTotal = 0;
	int shineTotal   = 0;
	for (int i = 0; i < 10; ++i) {
		if (i == 9)
			continue;

		if (TFlagManager::getInstance()->getBool(0x103A5 + i))
			unkBC->search('0_mn' + (i << 24))->show();
		else
			unkBC->search('0_mn' + (i << 24))->hide();

		if ((u32)i > 1) {
			for (int j = 0; j < 8; ++j) {
				if (j < unk14[i].mShineNum)
					unkBC->search('0ss1' + (i << 24) + j)->show();
				else
					unkBC->search('0ss1' + (i << 24) + j)->hide();
			}

			J2DPane* etc1 = unkBC->search('0sq1' + (i << 24));
			etc1->hide();
			J2DPane* etc2 = unkBC->search('0sq2' + (i << 24));
			etc2->hide();
			if (unk14[i].mEtcShineNum != 0)
				etc1->show();
			if (unk14[i].mEtcShineNum > 1)
				etc2->show();

			etcShineTotal += unk14[i].mEtcShineNum;
			shineTotal += unk14[i].mShineNum;
		}
	}

	shineTotal += etcShineTotal;
	if (etcShineTotal == 0)
		unkBC->search('lqus')->hide();
	else
		unkBC->search('lqus')->show();

	for (int i = 1; i < 10; ++i) {
		unk3D0[i] = unkBC->search('mi00' + i);
		if (i == 9)
			continue;

		u16 coinNum = unk14[i].mCoinNum;
		u32 tag     = '0c_1' + (i << 24);
		if (coinNum > 999)
			coinNum = 999;

		J2DPicture* digit0 = (J2DPicture*)unkBC->search(tag);
		J2DPicture* digit1 = (J2DPicture*)unkBC->search(tag + 1);
		J2DPicture* digit2 = (J2DPicture*)unkBC->search(tag + 2);
		if (coinNum < 100) {
			digit0->hide();
			digit1->changeTexture(unkC8[coinNum / 10]->getTexInfo(), 0);
			digit2->changeTexture(unkC8[coinNum % 10]->getTexInfo(), 0);
		} else {
			digit0->show();
			int hundreds = coinNum / 100;
			digit0->changeTexture(unkC8[hundreds]->getTexInfo(), 0);
			coinNum -= hundreds * 100;
			digit1->changeTexture(unkC8[coinNum / 10]->getTexInfo(), 0);
			digit2->changeTexture(unkC8[coinNum % 10]->getTexInfo(), 0);
		}

		if (unk14[i].mHundredCoinShine) {
			unkBC->search('0c_s' + (i << 24))->show();
			++shineTotal;
		} else {
			unkBC->search('0c_s' + (i << 24))->hide();
		}
	}

	unk3D0[0] = unkBC->search('mi00');
	unk448    = unkBC->search('clic');

	s16 extra = 0;
	if (TFlagManager::getInstance()->getBool(0x10056))
		extra = 1;
	if (TFlagManager::getInstance()->getBool(0x10058))
		++extra;
	((J2DPicture*)unkBC->search('0s_1'))
	    ->changeTexture(unkC8[extra]->getTexInfo(), 0);

	shineTotal += extra;
	int shineNum = TFlagManager::getInstance()->getFlag(0x40000);
	u8 rest      = shineNum - (u8)shineTotal;
	if (rest > 99)
		rest = 99;
	((J2DPicture*)unkBC->search('1s_1'))
	    ->changeTexture(unkC8[rest / 10]->getTexInfo(), 0);
	((J2DPicture*)unkBC->search('1s_2'))
	    ->changeTexture(unkC8[rest % 10]->getTexInfo(), 0);

	if (shineNum > 999)
		shineNum = 999;
	J2DPicture* digit0 = (J2DPicture*)unkBC->search('lt_1');
	J2DPicture* digit1 = (J2DPicture*)unkBC->search('lt_2');
	J2DPicture* digit2 = (J2DPicture*)unkBC->search('lt_3');
	if (shineNum < 100) {
		digit0->hide();
		digit1->changeTexture(unkC8[shineNum / 10]->getTexInfo(), 0);
		digit2->changeTexture(unkC8[shineNum % 10]->getTexInfo(), 0);
	} else {
		digit0->show();
		int hundreds = shineNum / 100;
		digit0->changeTexture(unkC8[hundreds]->getTexInfo(), 0);
		shineNum -= hundreds * 100;
		digit1->changeTexture(unkC8[shineNum / 10]->getTexInfo(), 0);
		digit2->changeTexture(unkC8[shineNum % 10]->getTexInfo(), 0);
	}

	switch (gpApplication.mSaveFile) {
	case 0:
		unkBC->search('ld_a')->show();
		unkBC->search('ld_b')->hide();
		unkBC->search('ld_c')->hide();
		break;
	case 1:
		unkBC->search('ld_a')->hide();
		unkBC->search('ld_b')->show();
		unkBC->search('ld_c')->hide();
		break;
	case 2:
		unkBC->search('ld_a')->hide();
		unkBC->search('ld_b')->hide();
		unkBC->search('ld_c')->show();
		break;
	}

	unk47C = 255.0f
	         * (1.0f
	            - (TFlagManager::getInstance()->getFlag(0x40000) / 30) * 0.25f);
	unk478->getPane()->setAlpha(unk47C);
}

JKRMemArchive* TGuide::setup(JKRMemArchive* archive)
{
	if (archive)
		SMSMountAramArchive(archive, gArBkGuide);
	else
		setup_wait = 0x10;
	unkC4 = 0;
	return archive;
}

// TODO: body is a guess, size not verified
void TGuide::setup2(JKRMemArchive* archive)
{
	SMSMountAramArchive(archive, gArBkGuide);
	unkC4 = 0;
}

void TGuide::startMoveCursor()
{
	unk10  = 9;
	unk164 = 0;
}

void TGuide::startMoveCursor2()
{
	u8 stage   = gpMarDirector->mMap;
	u8 shineSt = SMS_getShineStage(stage);
	if (stage == 0x14)
		shineSt = 0;

	unk42C = shineSt;
	resetObjects();
	s16 current = shineSt;
	changeBotStatus(current);
	for (int i = 0; i < 10; ++i) {
		if (i == current)
			unk3D0[i]->show();
		else
			unk3D0[i]->hide();
	}
	unk164 = 0;
}

void TGuide::linkSelect()
{
	unkC0->mFlags |= TMarioGamePad::PAD_FLAG_GUIDE_INPUT;
	if (unkC0->checkFrameMeaning(TMarioGamePad::MEANING_MENU_B)
	    || (unkC0->mButton.mTrigger & 0x10))
		unk10 = 7;

	J2DPane* cursor = unk128[0]->getPane();
	// TODO: mCompSPos[8]/[9] are probably the stick values
	int x = cursor->getBounds().x1;
	int y = cursor->getBounds().y1;
	x += (s16)(3.2f * unkC0->mCompSPos[8]);
	y += (s16)(-3.2f * unkC0->mCompSPos[9]);
	if (x > 568)
		x = 568;
	if (x < 0)
		x = 0;
	if (y > 360)
		y = 360;
	if (y < 56)
		y = 56;
	cursor->move(x, y);
	unk128[1]->getPane()->move(x + 7, y + 4);

	int point = checkPoint(x - 2, y + 10);
	if (point != -1 && unkC0->checkMeaning(TMarioGamePad::MEANING_MENU_A))
		appearGuidePane(point);

	if (point != -1 && point < 10) {
		u8 current = unk44C[point]->getAlpha();
		int alpha;
		if (unk164)
			alpha = current + 4;
		else
			alpha = current - 4;

		if (alpha < 30) {
			unk164 = 1;
			alpha  = 30;
		} else if (alpha > 255) {
			unk164 = 0;
			alpha  = 255;
		}
		unk44C[point]->setAlpha(alpha);

		changePattern((J2DPicture*)unk128[0]->getPane(), 45, unkF0);
		changePattern((J2DPicture*)unk128[1]->getPane(), 45, unkF0);
	}

	if (unk480 != point) {
		changeBotStatus(point);
		if (unk480 != -1 && unk480 < 10)
			unk44C[unk480]->setAlpha(255);

		if (point == -1) {
			J2DPicture* cursor0 = (J2DPicture*)unk128[0]->getPane();
			cursor0->setBlendKonstColor(1.0f, 0.0f, 0.0f, 0.0f);
			cursor0->setBlendKonstAlpha(1.0f, 0.0f, 0.0f, 0.0f);
			J2DPicture* cursor1 = (J2DPicture*)unk128[1]->getPane();
			cursor1->setBlendKonstColor(1.0f, 0.0f, 0.0f, 0.0f);
			cursor1->setBlendKonstAlpha(1.0f, 0.0f, 0.0f, 0.0f);
		}
		unk164 = 0;
		unk480 = point;
	}

	changePattern(unk134, 90, unkF0);
	mirrorPattern(unk148, 90, unkF0);
	rotatePattern((J2DPicture*)unk154, 90, unkF0, 30);
	changePattern(unk138, 90, unkF0);
	mirrorPattern(unk14C, 90, unkF0);
	mirrorPattern(unk150, 90, unkF0);
	changePattern(unk13C, 90, unkF0);
	changePattern(unk140, 90, unkF0);
	changePattern(unk144, 90, unkF0);
	rotatePattern((J2DPicture*)unk158, 90, unkF0, -45);
	shinePattern(unk444, 90, unkF0);
	// NOTE: calling mmarkPattern here instead keeps setPaneAlpha out-of-line,
	// unlike the ROM, so this was probably written out by hand.
	TExPane* mark = unk478;
	u32 frame     = unkF0;
	if (frame % 270 == 0) {
		if (!((frame / 270) & 1))
			mark->setPaneAlpha(270, 0, unk47C);
		else
			mark->setPaneAlpha(270, unk47C, 0);
	}
	mark->update();

	if (unk15C) {
		unk160 += 3;
		if (unk160 > 300)
			unk15C = 0;
	} else {
		unk160 -= 3;
		if (unk160 < 30)
			unk15C = 1;
	}

	u8 alpha;
	if (unk160 < 30)
		alpha = 30;
	else if (unk160 > 255)
		alpha = 255;
	else
		alpha = unk160;

	for (int i = 0; i < 10; ++i)
		unk168[i]->setAlpha(alpha);

	++unkF0;
	if (unkF0 > 540)
		unkF0 = 0;
}

void TGuide::changePattern(J2DPicture* picture, s16 period, u32 frame)
{
	if (frame % period == 0) {
		if ((frame / period) & 1) {
			picture->setBlendKonstColor(1.0f, 0.0f, 0.0f, 0.0f);
			picture->setBlendKonstAlpha(1.0f, 0.0f, 0.0f, 0.0f);
		} else {
			picture->setBlendKonstColor(0.0f, 1.0f, 0.0f, 0.0f);
			picture->setBlendKonstAlpha(0.0f, 1.0f, 0.0f, 0.0f);
		}
	}
}

void TGuide::mirrorPattern(J2DPicture* picture, s16 period, u32 frame)
{
	if (frame % period == 0) {
		if ((frame / period) & 1)
			picture->mMirror = (J2DMirror)2;
		else
			picture->mMirror = (J2DMirror)0;
	}
}

void TGuide::rotatePattern(J2DPicture* picture, s16 period, u32 frame,
                           s16 angle)
{
	if (frame % period == 0) {
		if ((frame / period) & 1)
			picture->mRotation = angle;
		else
			picture->mRotation = 0.0f;
	}
}

void TGuide::shinePattern(TBoundPane* pane, s16 period, u32 frame)
{
	int phase = frame % period;
	if (phase == 0) {
		pane->setPanePosition(45, JUTPoint(0, 0), JUTPoint(0, -5),
		                      JUTPoint(0, 0));
	} else if (phase == 45) {
		pane->setPanePosition(45, JUTPoint(0, 0), JUTPoint(0, 5),
		                      JUTPoint(0, 0));
	}

	u8 alpha;
	if (frame % (period * 2) < 130)
		alpha = 255;
	else
		alpha = 0;
	unk448->setAlpha(alpha);
	pane->update();
}

void TGuide::mmarkPattern(TExPane* pane, s16 period, u32 frame)
{
	if (frame % period == 0) {
		if (!((frame / period) & 1))
			pane->setPaneAlpha(period, 0, unk47C);
		else
			pane->setPaneAlpha(period, unk47C, 0);
	}
	pane->update();
}

// TODO: body is unknown
void TGuide::searchNearPoint(s16*, s16*, s16, s16) { }

int TGuide::checkPoint(int x, int y)
{
	int result = -1;
	for (int i = 0; i < 14; ++i) {
		JUTRect rect(unk168[i]->getBounds());
		if (x > rect.x1 && x < rect.x2 && y > rect.y1 && y < rect.y2) {
			result = i;
			break;
		}
	}

	if (result == -1) {
		for (int i = 0; i < 10; ++i) {
			JUTRect rect(unk44C[i]->getBounds());
			if (x > rect.x1 && x < rect.x2 && y > rect.y1 && y < rect.y2) {
				result = i;
				break;
			}
		}
	}

	if (result >= 0 && result < 10 && !unk44C[result]->mVisible)
		result = -1;

	return result;
}

void TGuide::changeBotStatus(int stage)
{
	if (stage == -1 || stage >= 10) {
		unk124->hide();
		return;
	}

	if (unk14[stage].unk0 == 0) {
		unk124->show();
		unkF4->show();
		strncpy(unk124->getStringPtr(), SMSGetMessageData(unk474, stage),
		        0x1a);

		int shineNum = unk14[stage].mShineNum;
		if (shineNum < 0)
			shineNum = 0;
		if (shineNum > 99)
			shineNum = 99;
		if (shineNum < 10) {
			unkF8[1]->hide();
			unkF8[0]->changeTexture(unkC8[shineNum]->getTexInfo(), 0);
		} else {
			unkF8[1]->show();
			unkF8[0]->changeTexture(unkC8[shineNum / 10]->getTexInfo(), 0);
			unkF8[1]->changeTexture(unkC8[shineNum % 10]->getTexInfo(), 0);
		}

		if ((u32)stage <= 1 || unk14[stage].mEtcShineNum == 0) {
			unk100->hide();
			unk104[0]->hide();
			unk104[1]->hide();
		} else if (unk14[stage].mEtcShineNum == 1) {
			unk100->show();
			unk104[0]->show();
			unk104[1]->hide();
		} else {
			unk100->show();
			unk104[0]->show();
			unk104[1]->show();
		}

		int coinNum = unk14[stage].mCoinNum;
		if (coinNum < 0)
			coinNum = 0;
		if (coinNum > 999)
			coinNum = 999;
		if (coinNum < 100) {
			unk10C[2]->hide();
			unk10C[0]->changeTexture(unkC8[coinNum / 10]->getTexInfo(), 0);
			unk10C[1]->changeTexture(unkC8[coinNum % 10]->getTexInfo(), 0);
		} else {
			unk10C[2]->show();
			int hundreds = coinNum / 100;
			unk10C[0]->changeTexture(unkC8[hundreds]->getTexInfo(), 0);
			coinNum -= hundreds * 100;
			unk10C[1]->changeTexture(unkC8[coinNum / 10]->getTexInfo(), 0);
			unk10C[2]->changeTexture(unkC8[coinNum % 10]->getTexInfo(), 0);
		}

		if (unk14[stage].mHundredCoinShine)
			unk118->show();
		else
			unk118->hide();

		int blueCoinNum = unk14[stage].mBlueCoinNum;
		if (blueCoinNum < 0)
			blueCoinNum = 0;
		if (blueCoinNum > 99)
			blueCoinNum = 99;

		if (stage == 0) {
			unkBC->search('sb_i')->hide();
			unkBC->search('sc_t')->hide();
		} else {
			unkBC->search('sb_i')->show();
			unkBC->search('sc_t')->show();
		}

		if (blueCoinNum < 10) {
			unk11C[1]->hide();
			unk11C[0]->changeTexture(unkC8[blueCoinNum % 10]->getTexInfo(), 0);
		} else {
			unk11C[1]->show();
			unk11C[0]->changeTexture(unkC8[blueCoinNum / 10]->getTexInfo(), 0);
			unk11C[1]->changeTexture(unkC8[blueCoinNum % 10]->getTexInfo(), 0);
		}
	} else {
		unk124->show();
		unkF4->hide();

		int blueCoinNum = unk14[stage].mBlueCoinNum;
		if (blueCoinNum < 0)
			blueCoinNum = 0;
		if (blueCoinNum > 99)
			blueCoinNum = 99;
		if (blueCoinNum < 10) {
			unk11C[1]->hide();
			unk11C[0]->changeTexture(unkC8[blueCoinNum % 10]->getTexInfo(), 0);
		} else {
			unk11C[1]->show();
			unk11C[0]->changeTexture(unkC8[blueCoinNum / 10]->getTexInfo(), 0);
			unk11C[1]->changeTexture(unkC8[blueCoinNum % 10]->getTexInfo(), 0);
		}

		unkBC->search('sb_i')->show();
		unkBC->search('sc_t')->hide();
		unkBC->search('sq_i')->hide();
		strncpy(unk124->getStringPtr(), SMSGetMessageData(unk474, stage),
		        0x1a);
	}
}

void TGuide::placeMario()
{
	u8 stage = gpMarDirector->mMap;
	if (SMS_getShineStage(stage) != 1 || stage == 0x14) {
		unk430->hide();
		return;
	}

	// TODO: stack frame is 0x70 bytes too small, some inline is missing
	JGeometry::TVec3<f32> pos = *gpMarioPos;
	int mapW                  = unk434.getWidth();
	int mapH                  = unk434.getHeight();
	pos.x                     = pos.x * mapW / 25000.0f;
	pos.y                     = 0.0f;
	pos.z                     = pos.z * mapH / 21200.0f;

	J2DPane* marker = unk430;
	int w           = marker->getBounds().getWidth();
	int h           = marker->getBounds().getHeight();
	int x           = 0.5f * mapW + pos.x - 0.5f * w - 2.0f;
	int y           = 0.5f * mapH + pos.z + 0.5f * h;
	if (x > mapW - w)
		x = mapW - w;
	if (x < 0)
		x = 0;
	if (y > mapH - h)
		y = mapH - h;
	if (y < 0)
		y = 0;
	marker->show();
	unk430->move(x, y);

	for (int i = 2; i < 10; ++i) {
		if (TFlagManager::getInstance()->getBool(0x103A5 + i))
			unkBC->search('01g1' + (i - 2))->show();
		else
			unkBC->search('01g1' + (i - 2))->hide();
	}
}

void TGuide::appearGuidePane(int index)
{
	unk424 = unk1C0[index];
	unk428 = unk378[index];
	JUTRect paneRect(unk218[index]);
	JUTRect pointRect(unk168[index]->getBounds());

	unk424->getPane()->show();
	unk424->setCenteredSize(20, paneRect.getWidth(), paneRect.getHeight(), 0,
	                        0);
	unk424->setPaneOffset(20, 0, 0, pointRect.x1 - paneRect.x1,
	                      pointRect.y1 - paneRect.x1 - 40);

	unk428->getPane()->setAlpha(0);
	unk428->getPane()->show();
	unk428->setPaneAlpha(20, 255, 0);
	unk128[0]->setPaneAlpha(20, 0, 255);
	unk128[1]->setPaneAlpha(20, 0, 80);

	if (index == 1)
		placeMario();

	gpMSound->startSoundSystemSE(0x4804, 0, nullptr, 0);
	unk42C = index;
	unk10  = 1;
	if (index != -1 && index < 10) {
		unk164 = 0;
		unk44C[index]->setAlpha(255);
	}
}

void TGuide::disappearGuidePane(int index)
{
	unk428->setPaneAlpha(20, 0, 255);
	JUTRect paneRect(unk218[index]);
	JUTRect pointRect(unk168[index]->getBounds());
	gpMSound->startSoundSystemSE(0x4805, 0, nullptr, 0);
	unk424->setCenteredSize(20, 0, 0, paneRect.getWidth(),
	                        paneRect.getHeight());
	unk424->setPaneOffset(20, pointRect.x1 - paneRect.x1,
	                      pointRect.y1 - paneRect.x1 - 40, 0, 0);
	unk128[0]->setPaneAlpha(20, 255, 0);
	unk128[1]->setPaneAlpha(20, 80, 0);
	unk10 = 3;
}

// fabricated
// TODO: the ROM needs one more inline level between perform and
// disappearGuidePane (otherwise setPaneSize/setPaneAlpha/setPaneOffset get
// inlined into it), the real split of perform is unknown.
inline void TGuide::control()
{
	bool done = true;
	switch (unk10) {
	case 9: {
		if (unkC5 && gpApplication.mFader->mFadeStatus == 0) {
			gpApplication.mFader->startWipe(5, 1.0f, 0.0f);
			unk10 = 10;
		}
		u32 stage = gpMarDirector->mMap;
		if (stage == 0x14)
			stage = 0;
		JUTRect rect(unk168[SMS_getShineStage(stage)]->getBounds());
		unk128[0]->getPane()->move(rect.x1 + 6, rect.y1 - 1);
		unk128[1]->getPane()->move(rect.x1 + 6, rect.y1 - 1);
		break;
	}
	case 10:
		if (gpApplication.mFader->mFadeStatus == 1) {
			unk10  = 0;
			unk428 = nullptr;
			unk424 = nullptr;
			unk128[0]->getPane()->setAlpha(255);
			unk128[1]->getPane()->setAlpha(80);
		}
		break;
	case 0:
		linkSelect();
		break;
	case 1:
		done &= unk424->update();
		for (int i = 0; i < 2; ++i)
			done &= unk128[i]->update();
		if (done) {
			if (unk428->update())
				unk10 = 2;
		}
		break;
	case 2:
		if (unkC0->checkFrameMeaning(TMarioGamePad::MEANING_MENU_A
		                             | TMarioGamePad::MEANING_MENU_B)) {
			disappearGuidePane(unk42C);
		} else if (unkC0->mButton.mTrigger & 0x10) {
			unk10 = 7;
		}
		break;
	case 3:
		if (unk428->update()) {
			done &= unk424->update();
			for (int i = 0; i < 2; ++i)
				done &= unk128[i]->update();
			if (done) {
				unk424->getPane()->hide();
				unk428->getPane()->hide();
				unk10 = 0;
				unkF0 = 0;
			}
		}
		break;
	case 7:
		gpApplication.mFader->startWipe(6, 1.0f, 0.0f);
		unkC0->mFlags &= ~TMarioGamePad::PAD_FLAG_GUIDE_INPUT;
		gpMSound->startSoundSystemSE(0x4818, 0, nullptr, 0);
		unk10 = 11;
		break;
	case 11:
		if (gpApplication.mFader->mFadeStatus == 0) {
			gpApplication.mFader->startWipe(5, 1.0f, 0.0f);
			if (unk424 && unk424->getPane()->mVisible)
				unk424->getPane()->hide();
			if (unk428 && unk428->getPane()->mVisible)
				unk428->getPane()->hide();
			unkC4 = 1;
			unk10 = 8;
		}
		break;
	}
}

void TGuide::perform(u32 param_1, JDrama::TGraphics* param_2)
{
	if (setup_wait != 0) {
		--setup_wait;
		if (setup_wait == 0) {
			SMSSwitch2DArchive("game_6", gArBkGuide);
			unkC4 = 0;
			startMoveCursor2();
		} else {
			return;
		}
	}

	if ((param_1 & 8) && unk10 != 9 && unk10 != 8) {
		J2DOrthoGraph graph(param_2->getViewport());
		graph.setup2D();
		unkBC->draw(0, 0, &graph);
	}

	if (param_1 & 1)
		control();
}
