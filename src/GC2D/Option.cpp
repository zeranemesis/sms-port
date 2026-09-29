#include <GC2D/Option.hpp>
#include <macros.h>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/J2D/J2DScreen.hpp>
#include <JSystem/J2D/J2DPicture.hpp>
#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/J2D/J2DOrthoGraph.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <JSystem/JAudio/JAInterface/JAIGlobalParameter.hpp>
#include <System/Application.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/MarioGamePad.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MSound/MSound.hpp>
#include <GC2D/ExPane.hpp>
#include <GC2D/MessageUtil.hpp>
#include <Camera/CameraOption.hpp>
#include <Player/MarioAccess.hpp>
#include <stdio.h>
#include <string.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// TODO: get rid of this
static const char* dummyMactorStringValue1 = "\0\0\0\0\0\0\0\0\0\0\0";
static const char* SMS_NO_MEMORY_MESSAGE   = "メモリが足りません\n";

namespace {

void tag_to_string(char*, u32) { }

void print_pane_tree(J2DPane*, int) { }

const TPatternAnmControl::TAnmChunk cRumbleAnm[] = {
	{ 'cnt0', 0.03 }, { 'cnt1', 0.03 }, { 'cnt2', 0.03 }, { 'cnt1', 0.03 },
	{ 'cnt2', 0.03 }, { 'cnt1', 0.03 }, { 'cnt2', 0.03 }, { 'cnt1', 0.03 },
	{ 'cnt2', 0.03 }, { 'cnt0', 2.0 },
};

const TPatternAnmControl::TAnmChunk cStopRumbleAnm[] = {
	{ 'cnt0', 1.0f },
};

const TPatternAnmControl::TAnmChunk cSurMonteAnm[] = {
	{ 'fa_8', 0.3f }, { 'fa_9', 0.3f }, { 'fa_8', 0.3f }, { 'fa_9', 0.3f },
	{ 'fa_6', 0.3f }, { 'fa_7', 0.3f }, { 'fa_6', 0.3f }, { 'fa_7', 0.3f },
};

const TPatternAnmControl::TAnmChunk cSurLTSpeakerAnm[] = {
	{ 'sp2b', 0.3f }, { 'sp2a', 0.3f }, { 'sp2b', 0.3f },
	{ 'sp2a', 0.3f }, { 'sp2a', 1.2f },
};

const TPatternAnmControl::TAnmChunk cSurRTSpeakerAnm[] = {
	{ 'sp2c', 0.3f }, { 'sp2d', 0.3f }, { 'sp2c', 0.3f },
	{ 'sp2d', 0.3f }, { 'sp2c', 1.2f },
};

const TPatternAnmControl::TAnmChunk cSurLBSpeakerAnm[] = {
	{ 'sp2e', 1.2f }, { 'sp2f', 0.3f }, { 'sp2e', 0.3f },
	{ 'sp2f', 0.3f }, { 'sp2e', 0.3f },
};

const TPatternAnmControl::TAnmChunk cSurRBSpeakerAnm[] = {
	{ 'sp2g', 1.2f }, { 'sp2g', 0.3f }, { 'sp2h', 0.3f },
	{ 'sp2g', 0.3f }, { 'sp2h', 0.3f },
};

const char* const cOptionLanguageDirectories[] = { "", "ge/", "fr/", "sp/",
	                                              "it/" };
const char* const cOptionMessageMarkerA =
	"\033" "GM[0]\033CC[64ff64]\033FX[26]\033FY[26]\033SH[3]\033CD[4]@"
	"\033GM[0]\033CC\033FX\033FY\033SH\033CU[4]";
const char* const cOptionMessageMarkerB =
	"\033" "GM[0]\033CC[dcdcdc]\033FX[26]\033FY[26]\033SH[3]\033CD[4]*"
	"\033GM[0]\033CC\033FX\033FY\033SH\033CU[4]";
const char* const cRumbleModeTextures[] = { "select_on.bti", "select_off.bti" };
const char* const cSoundModeTextures[] = { "select_stereo.bti", "select_mono.bti",
	                                      "select_surround.bti" };
const char* const cSubtitleTextures[] = { "select_on.bti", "select_off.bti" };
const u32 cSubtitleToggleItems[] = { 'sel6', 'sel5' };
const u32 cLanguageToggleItems[] = { 'lan0', 'lan1', 'lan2', 'lan3', 'lan4' };

int replaceOptionPictureTexture(J2DScreen* screen, u32 tag, JUTTexture* texture)
{
	J2DPicture* picture = (J2DPicture*)screen->search(tag);
	int halfWidth = (texture->getWidth() - picture->getWidth()) / 2;
	picture->changeTexture(texture->getTexInfo(), 0);
	JUTRect bounds = picture->getBounds();
	bounds.reform(-halfWidth, 0, halfWidth, 0);
	picture->setBounds(bounds);
	return picture->getWidth();
}

inline void resizeOptionBubble(TPaneScalingControl* bubble, int contentWidth)
{
	J2DPane* pane = bubble->mPane;
	int width = contentWidth + 0x1E;
	int halfWidth = (pane->getWidth() - width) / 2;
	const_cast<JUTRect&>(pane->getBounds()).reform(-halfWidth, 0, halfWidth, 0);
	JUTRect bounds = pane->getBounds();
	int baseWidth = bounds.getWidth();
	int baseHeight = bounds.getHeight();
	f32 progress = bubble->mFrameCtrl.getFrame() / bubble->mFrameCtrl.getEnd();
	f32 scale = bubble->mAmplitude * MsSin(RAD_TO_DEG(progress * (2.0f * M_PI)));
	int widthDelta = scale * baseWidth;
	int heightDelta = scale * baseHeight;
	bounds.move(bounds.x1 - widthDelta / 2, bounds.y1 - heightDelta / 2);
	bounds.resize(widthDelta + baseWidth, heightDelta + baseHeight);
	pane->setBounds(bounds);
	bubble->mFrameCtrl.update();
}

const TPatternAnmControl::TAnmChunk cSteMonteAnm[] = {
	{ 'fa_4', 0.3f }, { 'fa_5', 0.3f }, { 'fa_4', 0.3f }, { 'fa_5', 0.3f },
	{ 'fa_2', 0.3f }, { 'fa_3', 0.3f }, { 'fa_2', 0.3f }, { 'fa_3', 0.3f },
};

const TPatternAnmControl::TAnmChunk cSteLSpeakerAnm[] = {
	{ 'sp0b', 0.3f }, { 'sp0a', 0.3f }, { 'sp0b', 0.3f },
	{ 'sp0a', 0.3f }, { 'sp0a', 1.2f },
};

const TPatternAnmControl::TAnmChunk cSteRSpeakerAnm[] = {
	{ 'sp1a', 1.2f }, { 'sp1b', 0.3f }, { 'sp1a', 0.3f },
	{ 'sp1b', 0.3f }, { 'sp1a', 0.3f },
};

const TPatternAnmControl::TAnmChunk cMonoMonteAnm[] = {
	{ 'fa_0', 0.3f },
	{ 'fa_1', 0.3f },
};

const TPatternAnmControl::TAnmChunk cMonoSpeakerAnm[] = {
	{ 'sp0a', 0.3f },
	{ 'sp0b', 0.3f },
};

const u32 cRumbleToggleItems[] = {
	'sel1',
	'sel0',
};

const u32 cSoundToggleItems[] = {
	'sel3',
	'sel2',
	'sel4',
};

} // namespace

#pragma dont_inline on
void TArrowControl::loadLanguageTextures()
{
	char path[0xFF];
	for (int language = 0; language < ARRAY_COUNT(cOptionLanguageDirectories);
	     ++language) {
		snprintf(path, sizeof(path), "/option/timg/%s%s",
		         cOptionLanguageDirectories[language], "select_quit.bti");
		JUTTexture* texture = new JUTTexture();
		if (texture)
			texture->storeTIMG((const ResTIMG*)JKRFileLoader::getGlbResource(path));
		mLanguageTextures[language] = texture;
	}
}

int TArrowControl::replaceTexture(u32 tag, JUTTexture* texture)
{
	J2DPicture* picture = (J2DPicture*)mPane->search(tag);
	int widthDelta = texture->getWidth() - picture->getWidth();
	picture->changeTexture(texture->getTexInfo(), 0);
	JUTRect bounds = picture->getBounds();
	bounds.reform(0, 0, widthDelta, 0);
	picture->setBounds(bounds);
	return picture->getWidth();
}

void TOptionRumbleUnit::changeTexture(int language)
{
	int maxWidth = replaceOptionPictureTexture(mScreen, 'm_1',
	                                           mLanguageTextures[language]);
	for (int state = 0; state < ARRAY_COUNT(mStateTextures); ++state) {
		int width = replaceOptionPictureTexture(
		    mScreen, 'sel0' + state, mStateTextures[state][language]);
		if (width > maxWidth)
			maxWidth = width;
	}
	resizeOptionBubble(mSelectionBubble, maxWidth);
}

void TOptionSoundUnit::changeTexture(int language)
{
	int maxWidth = replaceOptionPictureTexture(mScreen, 'm_2',
	                                           mLanguageTextures[language]);
	for (int mode = 0; mode < ARRAY_COUNT(mModeTextures); ++mode) {
		int width = replaceOptionPictureTexture(
		    mScreen, 'sel2' + mode, mModeTextures[mode][language]);
		if (width > maxWidth)
			maxWidth = width;
	}
	resizeOptionBubble(mSelectionBubble, maxWidth);
}

void TOptionSubtitleUnit::changeTexture(int language)
{
	int maxWidth = replaceOptionPictureTexture(mScreen, 'm_3',
	                                           mLanguageTextures[language]);
	for (int state = 0; state < ARRAY_COUNT(mStateTextures); ++state) {
		int width = replaceOptionPictureTexture(
		    mScreen, 'sel5' + state, mStateTextures[state][language]);
		if (width > maxWidth)
			maxWidth = width;
	}
	resizeOptionBubble(mSelectionBubble, maxWidth);
}

void TOptionLanguageUnit::changeTexture(int language)
{
	int maxWidth = replaceOptionPictureTexture(mScreen, 'm_4',
	                                           mLanguageTextures[language]);
	for (int i = 0; i < mSelectionText->mItems.size(); ++i) {
		J2DPane* pane = mScreen->search('lan0' + i);
		int width = pane->getWidth();
		if (width > maxWidth)
			maxWidth = width;
	}
	resizeOptionBubble(mSelectionBubble, maxWidth);
}

int TOptionLanguageUnit::replaceTexture(u32 tag, JUTTexture* texture)
{
	return replaceOptionPictureTexture(mScreen, tag, texture);
}
#pragma dont_inline off

const TOptionSoundUnit::FabricatedSoundSettings
    TOptionSoundUnit::cSoundSettings[]
    = {
	      { MSD_SE_SY_SOUT_MONO, 0 },
	      { MSD_SE_SY_SOUT_STEREO, 1 },
	      { MSD_SE_SY_SOUT_SURROUND, 2 },
      };

const TOptionSoundUnit::FabricatedFlagInfo TOptionSoundUnit::cFlagInfos[] = {
	{ TOptionSoundUnit::SOUND_TYPE_MONO, 0 },
	{ TOptionSoundUnit::SOUND_TYPE_STEREO, 1 },
	{ TOptionSoundUnit::SOUND_TYPE_SURROUND, 2 },
};

void TArrowControl::update()
{
	updateAlpha();
	if (mPane->getAlpha() != 0) {
		updateScale();
	}
}

void TArrowControl::updateAlpha()
{
	int iVar3 = unk14 != 0 ? 1 : -1;
	mPane->setAlpha(
	    JGeometry::TUtil<s32>::clamp(iVar3 * 8 + mPane->getAlpha(), 0, 255));
}

// incorrect
void TArrowControl::updateScale()
{
	int move = calcMoveX(mPhase);
	mPane->setBounds(
	    JUTRect(mBounds.x1 - move, mBounds.y1, mBounds.x2, mBounds.y2));

	mPhase = JGeometry::TUtil<int>::mod(mPhase + 101, 100);
}

int TArrowControl::calcMoveX(int phase) const
{
	int iVar3 = phase < 50 ? phase : 100 - phase;
	f32 fVar1 = iVar3 / 50.0f;
	f32 fVar2 = 1.0f - fVar1;
	return fVar1 * 8.0f * fVar1 + fVar2 * -8.0f * fVar2 + fVar1 * fVar2;
}

TPaneScalingControl::TPaneScalingControl(J2DPane* pane)
    : mPane(pane)
{
	mInitialBounds = pane->getBounds();
}

void TPaneScalingControl::setupAnm(f32 amplitude, f32 speed)
{
	mAmplitude = amplitude;
	mFrameCtrl.init(0x78);
	mFrameCtrl.setAttribute(J3DFrameCtrl::ATTR_LOOP);
	mFrameCtrl.setRate(speed);
}

void TPaneScalingControl::startAnm() { mFrameCtrl.setRate(1.0f); }

void TPaneScalingControl::stopAnm()
{
	mFrameCtrl.setRate(0.0f);
	mFrameCtrl.reset();
}

void TPaneScalingControl::update()
{
	int iVar10 = mInitialBounds.getWidth();
	int iVar5  = mInitialBounds.getHeight();

	f32 progress = (f32)mFrameCtrl.getFrame() / (f32)mFrameCtrl.getEnd();
	f32 fVar2    = mAmplitude * MsSin(RAD_TO_DEG(progress * (2 * M_PI)));

	int uVar6 = fVar2 * iVar10;
	int uVar1 = fVar2 * iVar5;

	JUTRect local_5c = mInitialBounds;
	local_5c.move(mInitialBounds.x1 - uVar6 / 2, mInitialBounds.y1 - uVar1 / 2);
	local_5c.resize(uVar6 + mInitialBounds.getWidth(),
	                uVar1 + mInitialBounds.getHeight());
	mPane->mBounds = local_5c;
	mFrameCtrl.update();
}

TPatternAnmControl::TPatternAnmControl(J2DScreen* screen)
    : mScreen(screen)
{
}

void TPatternAnmControl::set(const TPatternAnmControl::TAnmChunk* chunks,
                             int num_chunks)
{
	mChunks.set(chunks, num_chunks);
	hide();
}

void TPatternAnmControl::setupAnm()
{
	mCurrentChunk = mChunks.begin();

	// Kahan compensated summation via Fast2Sum
	f32 sum   = 0.0f;
	f32 error = 0.0f;
	for (const TAnmChunk *it = mChunks.begin(), *e = mChunks.end(); it != e;
	     ++it) {
		f32 lastSum = sum;
		f32 next    = error + it->mDuration;
		sum += next;
		error = next - (sum - lastSum);
	}

	mFrameCtrl.init(120.0f * sum + 1.0f);
	mFrameCtrl.setAttribute(J3DFrameCtrl::ATTR_LOOP);
	mFrameCtrl.setRate(1.0f);
	show();
	mNextTriggerFrame = mCurrentChunk->mDuration * 120.0f;
}

void TPatternAnmControl::update()
{
	if (mFrameCtrl.checkPass(mNextTriggerFrame)
	    || mFrameCtrl.checkState(J3DFrameCtrl::STATE_LOOPED_ONCE)) {
		mScreen->search(mCurrentChunk->mTag)->hide();
		if (++mCurrentChunk == mChunks.end()
		    || mFrameCtrl.checkState(J3DFrameCtrl::STATE_LOOPED_ONCE)) {
			mCurrentChunk     = mChunks.begin();
			mNextTriggerFrame = 0.0f;
		}
		mScreen->search(mCurrentChunk->mTag)->show();
		mNextTriggerFrame += mCurrentChunk->mDuration * 120.0f;
	}
	mFrameCtrl.update();
}

void TPatternAnmControl::show()
{
	hide();
	mScreen->search(mCurrentChunk->mTag)->show();
}

void TPatternAnmControl::hide()
{
	for (const TAnmChunk* it = mChunks.begin(); it != mChunks.end(); ++it)
		mScreen->search(it->mTag)->hide();
}

TToggleControl::TToggleControl(J2DScreen* screen)
    : mScreen(screen)
{
}

void TToggleControl::setupToggle(const u32* tags, int num_tags)
{
	mItems.set(tags, num_tags);
	for (const u32* it = mItems.begin(); it != mItems.end(); ++it)
		mScreen->search(*it)->hide();
	mCurItem = mItems.begin();
	mScreen->search(*mCurItem)->show();
}

void TToggleControl::toggle()
{
	mScreen->search(*mCurItem)->hide();
	if (++mCurItem == mItems.end())
		mCurItem = mItems.begin();
	mScreen->search(*mCurItem)->show();
}

s32 TToggleControl::getNumber() const { return mCurItem - mItems.begin(); }

void TToggleControl::setNumber(int num)
{
	mScreen->search(*mCurItem)->hide();
	mCurItem = mItems.begin() + num;
	mScreen->search(*mCurItem)->show();
}

TOptionRumbleUnit::TOptionRumbleUnit(J2DScreen* screen)
    : mScreen(screen)
    , mShouldRumble(false)
{
	mParentPane   = new TExPane(mScreen, 'oya1');
	mInitialAlpha = mParentPane->getPane()->getAlpha();

	// The speech bubble around the on/off text that pulsates
	// when this setting is selected.
	mSelectionBubble = new TPaneScalingControl(mScreen->search('me_0'));
	mSelectionBubble->setupAnm(0.05f, 1.0f);
	mSelectionBubble->stopAnm();

	// The image of a gamepad that either shakes occasionally or not based on
	// whether rumble is enabled in the options.
	mGamepadIcon[1] = new TPatternAnmControl(mScreen);
	mGamepadIcon[1]->set(cRumbleAnm, ARRAY_COUNT(cRumbleAnm));
	mGamepadIcon[1]->setupAnm();
	mGamepadIcon[1]->hide();

	mGamepadIcon[0] = new TPatternAnmControl(mScreen);
	mGamepadIcon[0]->set(cStopRumbleAnm, ARRAY_COUNT(cStopRumbleAnm));
	mGamepadIcon[0]->setupAnm();
	mGamepadIcon[0]->hide();

	// The text that says on/off for rumble in the options menu.
	mSelectionText = new TToggleControl(mScreen);
	mSelectionText->setupToggle(cRumbleToggleItems,
	                            ARRAY_COUNT(cRumbleToggleItems));

	char path[0xFF];
	for (int language = 0; language < ARRAY_COUNT(cOptionLanguageDirectories);
	     ++language) {
		snprintf(path, sizeof(path), "/option/timg/%s%s",
		         cOptionLanguageDirectories[language], "select_rumble.bti");
		JUTTexture* texture = new JUTTexture();
		if (texture)
			texture->storeTIMG((const ResTIMG*)JKRFileLoader::getGlbResource(path));
		mLanguageTextures[language] = texture;
	}
	for (int state = 0; state < ARRAY_COUNT(cRumbleModeTextures); ++state) {
		for (int language = 0; language < ARRAY_COUNT(cOptionLanguageDirectories);
		     ++language) {
			snprintf(path, sizeof(path), "/option/timg/%s%s",
			         cOptionLanguageDirectories[language],
			         cRumbleModeTextures[state]);
			JUTTexture* texture = new JUTTexture();
			if (texture)
				texture->storeTIMG(
				    (const ResTIMG*)JKRFileLoader::getGlbResource(path));
			mStateTextures[state][language] = texture;
		}
	}

	setState(STATE_INACTIVE);
}

#pragma dont_inline on
void TOptionRumbleUnit::update()
{
	switch (mState) {
	case STATE_DEACTIVATING:
		mParentPane->update();
		// fade-out animation is done
		if (mParentPane->getPane()->getAlpha() == 150)
			setState(STATE_INACTIVE);
		break;

	case STATE_ACTIVE:
		mParentPane->update();
		mSelectionBubble->update();
		mGamepadIcon[getValue()]->update();
		checkRumble();
		break;

	case STATE_INACTIVE:
		break;
	}
}
#pragma dont_inline off

#pragma dont_inline on
void TOptionRumbleUnit::checkRumble()
{
	if (mShouldRumble) {
		if (mGamepadIcon[mSelectionText->getNumber()]->checkCompletedOnce()) {
			mShouldRumble = false;
			SMSRumbleMgr->stop();
		} else {
			switch (mGamepadIcon[mSelectionText->getNumber()]
			            ->getCurrentPaneTag()) {
			case 'cnt0':
				SMSRumbleMgr->stop();
				break;

			case 'cnt1':
			case 'cnt2':
				SMSRumbleMgr->start(8, (float*)nullptr);
				break;
			}
		}
	}
}

#pragma dont_inline off

void TOptionRumbleUnit::toggle()
{
	mShouldRumble = true;
	mSelectionText->toggle();
	adjust();
	SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_SELECT_COMMON, 0, nullptr, 0);
	SMSRumbleMgr->start(8, (float*)nullptr);
}

void TOptionRumbleUnit::adjust()
{
	bool b = mSelectionText->getNumber() == 1 ? true : false;
	SMSRumbleMgr->setActive(b);
	adjustView();
}

void TOptionRumbleUnit::adjustView()
{
	switch (mSelectionText->getNumber()) {
	case 0:
		mGamepadIcon[1]->hide();
		mGamepadIcon[0]->setupAnm();
		break;
	case 1:
		mGamepadIcon[0]->hide();
		mGamepadIcon[1]->setupAnm();
		break;
	}
}

void TOptionRumbleUnit::show() { }

void TOptionRumbleUnit::hide() { }

void TOptionRumbleUnit::deactivate(bool force)
{
	SMSRumbleMgr->stop();
	if (force)
		setState(TOptionRumbleUnit::STATE_INACTIVE);
	else
		setState(TOptionRumbleUnit::STATE_DEACTIVATING);
}

void TOptionRumbleUnit::activate()
{
	setState(TOptionRumbleUnit::STATE_ACTIVE);
}

void TOptionRumbleUnit::setValue(TOptionRumbleUnit::RumbleType type)
{
	mSelectionText->setNumber(type);
	adjust();
}

void TOptionRumbleUnit::setState(TOptionRumbleUnit::State state)
{
	mState = state;
	switch (state) {
	case STATE_INACTIVE:
		mParentPane->getPane()->setAlpha(150);
		setInfluencedAlphaRecursive(mParentPane->getPane(), true);
		mSelectionBubble->stopAnm();
		mShouldRumble = false;
		break;

	case STATE_DEACTIVATING:
		mParentPane->setPaneAlpha(30, 150, mInitialAlpha);
		setInfluencedAlphaRecursive(mParentPane->getPane(), true);
		mShouldRumble = false;
		break;

	case STATE_ACTIVE:
		mParentPane->getPane()->setAlpha(mInitialAlpha);
		setInfluencedAlphaRecursive(mParentPane->getPane(), false);
		mSelectionBubble->startAnm();
		adjustView();
		break;
	}
}

void TOptionRumbleUnit::setInfluencedAlphaRecursive(J2DPane* pane, bool flag)
{
	for (JSUTreeIterator<J2DPane> it = pane->getPaneTree()->getFirstChild();
	     it != pane->getPaneTree()->getEndChild(); ++it) {
		it->setInfluenceAlpha(flag);
		setInfluencedAlphaRecursive(it.getObject(), flag);
	}
}

TOptionSoundUnit::TOptionSoundUnit(J2DScreen* screen)
    : mScreen(screen)
{
	mParentPane   = new TExPane(mScreen, 'oya2');
	mInitialAlpha = mParentPane->getPane()->getAlpha();

	// The speech bubble around the mono/stereo/surround text that pulsates
	// when this setting is selected.
	mSelectionBubble = new TPaneScalingControl(mScreen->search('me_1'));
	mSelectionBubble->setupAnm(0.05f, 1.0f);
	mSelectionBubble->stopAnm();

	// These 3 are for the animation of a pianta (monte) vibing to the speakers
	initMonoAnm();
	initSteleoAnm();
	initSurroundAnm();

	// The toggle for switching between mono/stereo/surround.
	mSelectionText = new TToggleControl(mScreen);
	mSelectionText->setupToggle(cSoundToggleItems,
	                            ARRAY_COUNT(cSoundToggleItems));
	mMusicFrameCtrl.init(289);
	mMusicFrameCtrl.setAttribute(J3DFrameCtrl::ATTR_LOOP);
	mMusicFrameCtrl.setRate(1.0f);
	mMusic = nullptr;
	char path[0xFF];
	for (int language = 0; language < ARRAY_COUNT(cOptionLanguageDirectories);
	     ++language) {
		snprintf(path, sizeof(path), "/option/timg/%s%s",
		         cOptionLanguageDirectories[language], "select_sound.bti");
		JUTTexture* texture = new JUTTexture();
		if (texture)
			texture->storeTIMG((const ResTIMG*)JKRFileLoader::getGlbResource(path));
		mLanguageTextures[language] = texture;
	}
	for (int mode = 0; mode < ARRAY_COUNT(cSoundModeTextures); ++mode) {
		for (int language = 0; language < ARRAY_COUNT(cOptionLanguageDirectories);
		     ++language) {
			snprintf(path, sizeof(path), "/option/timg/%s%s",
			         cOptionLanguageDirectories[language],
			         cSoundModeTextures[mode]);
			JUTTexture* texture = new JUTTexture();
			if (texture)
				texture->storeTIMG(
				    (const ResTIMG*)JKRFileLoader::getGlbResource(path));
			mModeTextures[mode][language] = texture;
		}
	}
	setState(STATE_INACTIVE);
	adjustView();
}

TOptionSubtitleUnit::TOptionSubtitleUnit(J2DScreen* screen)
    : mScreen(screen)
{
	mParentPane   = new TExPane(mScreen, 'txp2');
	mInitialAlpha = mParentPane->getPane()->getAlpha();
	mSelectionBubble = new TPaneScalingControl(mScreen->search('me_2'));
	mSelectionBubble->setupAnm(0.05f, 1.0f);
	mSelectionBubble->stopAnm();
	mSelectionText = new TToggleControl(mScreen);
	mSelectionText->setupToggle(cSubtitleToggleItems,
	                            ARRAY_COUNT(cSubtitleToggleItems));

	char path[0xFF];
	for (int language = 0; language < ARRAY_COUNT(cOptionLanguageDirectories);
	     ++language) {
		snprintf(path, sizeof(path), "/option/timg/%s%s",
		         cOptionLanguageDirectories[language], "select_subtitles.bti");
		JUTTexture* texture = new JUTTexture();
		if (texture)
			texture->storeTIMG((const ResTIMG*)JKRFileLoader::getGlbResource(path));
		mLanguageTextures[language] = texture;
	}
	for (int state = 0; state < ARRAY_COUNT(cSubtitleTextures); ++state) {
		for (int language = 0; language < ARRAY_COUNT(cOptionLanguageDirectories);
		     ++language) {
			snprintf(path, sizeof(path), "/option/timg/%s%s",
			         cOptionLanguageDirectories[language], cSubtitleTextures[state]);
			JUTTexture* texture = new JUTTexture();
			if (texture)
				texture->storeTIMG(
				    (const ResTIMG*)JKRFileLoader::getGlbResource(path));
			mStateTextures[state][language] = texture;
		}
	}
	setState(STATE_INACTIVE);
}

TOptionLanguageUnit::TOptionLanguageUnit(J2DScreen* screen)
    : mScreen(screen)
{
	mParentPane   = new TExPane(mScreen, 'txp3');
	mInitialAlpha = mParentPane->getPane()->getAlpha();
	mSelectionBubble = new TPaneScalingControl(mScreen->search('me_3'));
	mSelectionBubble->setupAnm(0.05f, 1.0f);
	mSelectionBubble->stopAnm();
	mSelectionText = new TToggleControl(mScreen);
	mSelectionText->setupToggle(cLanguageToggleItems,
	                            ARRAY_COUNT(cLanguageToggleItems));

	char path[0xFF];
	for (int language = 0; language < ARRAY_COUNT(cOptionLanguageDirectories);
	     ++language) {
		snprintf(path, sizeof(path), "/option/timg/%s%s",
		         cOptionLanguageDirectories[language], "select_language.bti");
		JUTTexture* texture = new JUTTexture();
		if (texture)
			texture->storeTIMG((const ResTIMG*)JKRFileLoader::getGlbResource(path));
		mLanguageTextures[language] = texture;
	}
	setState(STATE_INACTIVE);
}

void TOptionSubtitleUnit::setInfluencedAlphaRecursive(J2DPane* pane, bool flag)
{
	for (JSUTreeIterator<J2DPane> it = pane->getPaneTree()->getFirstChild();
	     it != pane->getPaneTree()->getEndChild(); ++it) {
		it->setInfluenceAlpha(flag);
		setInfluencedAlphaRecursive(it.getObject(), flag);
	}
}

void TOptionLanguageUnit::setInfluencedAlphaRecursive(J2DPane* pane, bool flag)
{
	for (JSUTreeIterator<J2DPane> it = pane->getPaneTree()->getFirstChild();
	     it != pane->getPaneTree()->getEndChild(); ++it) {
		it->setInfluenceAlpha(flag);
		setInfluencedAlphaRecursive(it.getObject(), flag);
	}
}

#pragma dont_inline on
void TOptionSubtitleUnit::setState(State state)
{
	mState = state;
	switch (state) {
	case STATE_INACTIVE:
		mParentPane->getPane()->setAlpha(150);
		setInfluencedAlphaRecursive(mParentPane->getPane(), true);
		mSelectionBubble->stopAnm();
		break;
	case STATE_DEACTIVATING:
		mParentPane->setPaneAlpha(30, 150, mInitialAlpha);
		setInfluencedAlphaRecursive(mParentPane->getPane(), true);
		break;
	case STATE_ACTIVE:
		mParentPane->getPane()->setAlpha(mInitialAlpha);
		setInfluencedAlphaRecursive(mParentPane->getPane(), false);
		mSelectionBubble->startAnm();
		break;
	}
}

void TOptionLanguageUnit::setState(State state)
{
	mState = state;
	switch (state) {
	case STATE_INACTIVE:
		mParentPane->getPane()->setAlpha(150);
		setInfluencedAlphaRecursive(mParentPane->getPane(), true);
		mSelectionBubble->stopAnm();
		break;
	case STATE_DEACTIVATING:
		mParentPane->setPaneAlpha(30, 150, mInitialAlpha);
		setInfluencedAlphaRecursive(mParentPane->getPane(), true);
		break;
	case STATE_ACTIVE:
		mParentPane->getPane()->setAlpha(mInitialAlpha);
		setInfluencedAlphaRecursive(mParentPane->getPane(), false);
		mSelectionBubble->startAnm();
		break;
	}
}
#pragma dont_inline off

void TOptionSubtitleUnit::update()
{
	if (mState == STATE_DEACTIVATING) {
		mParentPane->update();
		if (mParentPane->getPane()->getAlpha() == 150)
			setState(STATE_INACTIVE);
	} else if (mState == STATE_ACTIVE) {
		mParentPane->update();
		mSelectionBubble->update();
	}
}

void TOptionLanguageUnit::update()
{
	if (mState == STATE_DEACTIVATING) {
		mParentPane->update();
		if (mParentPane->getPane()->getAlpha() == 150)
			setState(STATE_INACTIVE);
	} else if (mState == STATE_ACTIVE) {
		mParentPane->update();
		mSelectionBubble->update();
	}
}

void TOptionSoundUnit::initMonoAnm()
{
	TPatternAnmControl** ary = mMonoAnimations;

	ary[0] = new TPatternAnmControl(mScreen);
	ary[0]->set(cMonoMonteAnm, ARRAY_COUNT(cMonoMonteAnm));
	ary[0]->setupAnm();

	ary[1] = new TPatternAnmControl(mScreen);
	ary[1]->set(cMonoSpeakerAnm, ARRAY_COUNT(cMonoSpeakerAnm));
	ary[1]->setupAnm();

	mMonteIcons[0].set(mMonoAnimations, ARRAY_COUNT(mMonoAnimations));
}

void TOptionSoundUnit::initSteleoAnm()
{
	TPatternAnmControl** ary = mStereoAnimations;

	ary[0] = new TPatternAnmControl(mScreen);
	ary[0]->set(cSteMonteAnm, ARRAY_COUNT(cSteMonteAnm));
	ary[0]->setupAnm();

	ary[1] = new TPatternAnmControl(mScreen);
	ary[1]->set(cSteRSpeakerAnm, ARRAY_COUNT(cSteRSpeakerAnm));
	ary[1]->setupAnm();

	ary[2] = new TPatternAnmControl(mScreen);
	ary[2]->set(cSteLSpeakerAnm, ARRAY_COUNT(cSteLSpeakerAnm));
	ary[2]->setupAnm();

	mMonteIcons[1].set(mStereoAnimations, ARRAY_COUNT(mStereoAnimations));
}

void TOptionSoundUnit::initSurroundAnm()
{
	TPatternAnmControl** ary = mSurroundAnimations;

	ary[0] = new TPatternAnmControl(mScreen);
	ary[0]->set(cSurMonteAnm, ARRAY_COUNT(cSurMonteAnm));
	ary[0]->setupAnm();

	ary[1] = new TPatternAnmControl(mScreen);
	ary[1]->set(cSurRTSpeakerAnm, ARRAY_COUNT(cSurRTSpeakerAnm));
	ary[1]->setupAnm();

	ary[2] = new TPatternAnmControl(mScreen);
	ary[2]->set(cSurLTSpeakerAnm, ARRAY_COUNT(cSurLTSpeakerAnm));
	ary[2]->setupAnm();

	ary[3] = new TPatternAnmControl(mScreen);
	ary[3]->set(cSurRBSpeakerAnm, ARRAY_COUNT(cSurRBSpeakerAnm));
	ary[3]->setupAnm();

	ary[4] = new TPatternAnmControl(mScreen);
	ary[4]->set(cSurLBSpeakerAnm, ARRAY_COUNT(cSurLBSpeakerAnm));
	ary[4]->setupAnm();

	mMonteIcons[2].set(mSurroundAnimations, ARRAY_COUNT(mSurroundAnimations));
}

#pragma dont_inline on
void TOptionSoundUnit::update()
{
	switch (mState) {
	case STATE_DEACTIVATING:
		mParentPane->update();
		// fade-out animation is done
		if (mParentPane->getPane()->getAlpha() == 150)
			setState(STATE_INACTIVE);
		break;

	case STATE_ACTIVE:
		mParentPane->update();
		mSelectionBubble->update();
		updatePatternAnm();
		break;

	case STATE_INACTIVE:
		break;
	}
}
#pragma dont_inline off

void TOptionSoundUnit::updatePatternAnm()
{
	ArrayWrapper<TPatternAnmControl*>& ary
	    = mMonteIcons[mSelectionText->getNumber()];

	for (TPatternAnmControl** it = ary.begin(); it != ary.end(); ++it)
		(*it)->update();

	mMusicFrameCtrl.update();
	if (mMusicFrameCtrl.checkState(J3DFrameCtrl::STATE_LOOPED_ONCE))
		adjustSound();
}

void TOptionSoundUnit::foreachPatternAnm(ArrayWrapper<TPatternAnmControl*>& ary,
                                         void (TPatternAnmControl::*ptmf)())
{
	for (TPatternAnmControl** it = ary.mData; it != ary.mData + ary.mSize; ++it)
		((*it)->*ptmf)();
}

void TOptionSoundUnit::toggle()
{
	mSelectionText->toggle();
	adjust();
	adjustSound();
}

void TOptionSoundUnit::adjust()
{
	adjustView();
	const FabricatedSoundSettings& setting
	    = cSoundSettings[mSelectionText->getNumber()];
	JAIGlobalParameter::setParamSoundOutputMode(setting.mOutputMode);
}

void TOptionSoundUnit::show() { }

void TOptionSoundUnit::hide() { }

void TOptionSoundUnit::deactivate(bool force)
{
	if (force)
		setState(TOptionSoundUnit::STATE_INACTIVE);
	else
		setState(TOptionSoundUnit::STATE_DEACTIVATING);
}

void TOptionSoundUnit::activate() { setState(TOptionSoundUnit::STATE_ACTIVE); }

void TOptionSoundUnit::setValue(int value)
{
	mSelectionText->setNumber(flagToType(value));
	adjust();
}

int TOptionSoundUnit::getValue() const
{
	return typeToFlag((SoundType)mSelectionText->getNumber());
}

void TOptionSoundUnit::stopSound()
{
	if (mMusic)
		mMusic->stop(1);
}

TOptionSoundUnit::SoundType TOptionSoundUnit::flagToType(int flag)
{
	for (const FabricatedFlagInfo* it = cFlagInfos;
	     it != cFlagInfos + ARRAY_COUNT(cFlagInfos); ++it) {
		if (it->mFlag == flag)
			return it->mSoundType;
	}

	return SOUND_TYPE_MONO;
}

int TOptionSoundUnit::typeToFlag(TOptionSoundUnit::SoundType type)
{
	for (const FabricatedFlagInfo* it = cFlagInfos;
	     it != cFlagInfos + ARRAY_COUNT(cFlagInfos); ++it) {
		if (it->mSoundType == type)
			return it->mFlag;
	}

	return 0;
}

void TOptionSoundUnit::setState(TOptionSoundUnit::State state)
{
	mState = state;
	switch (state) {
	case STATE_INACTIVE:
		mParentPane->getPane()->setAlpha(150);
		setInfluencedAlphaRecursive(mParentPane->getPane(), true);
		mSelectionBubble->stopAnm();
		stopSound();
		break;

	case STATE_DEACTIVATING:
		mParentPane->setPaneAlpha(30, 150, mInitialAlpha);
		setInfluencedAlphaRecursive(mParentPane->getPane(), true);
		break;

	case STATE_ACTIVE:
		mParentPane->getPane()->setAlpha(mInitialAlpha);
		setInfluencedAlphaRecursive(mParentPane->getPane(), false);
		mSelectionBubble->startAnm();
		adjustView();
		adjustSound();
		break;
	}
}

void TOptionSoundUnit::adjustView()
{
	switch (mSelectionText->getNumber()) {
	case SOUND_TYPE_MONO:
		foreachPatternAnm(mMonteIcons[1], &TPatternAnmControl::hide);
		foreachPatternAnm(mMonteIcons[2], &TPatternAnmControl::hide);
		foreachPatternAnm(mMonteIcons[0], &TPatternAnmControl::show);
		foreachPatternAnm(mMonteIcons[0], &TPatternAnmControl::setupAnm);
		break;
	case SOUND_TYPE_STEREO:
		foreachPatternAnm(mMonteIcons[0], &TPatternAnmControl::hide);
		foreachPatternAnm(mMonteIcons[2], &TPatternAnmControl::hide);
		foreachPatternAnm(mMonteIcons[1], &TPatternAnmControl::show);
		foreachPatternAnm(mMonteIcons[1], &TPatternAnmControl::setupAnm);
		break;
	case SOUND_TYPE_SURROUND:
		foreachPatternAnm(mMonteIcons[0], &TPatternAnmControl::hide);
		foreachPatternAnm(mMonteIcons[1], &TPatternAnmControl::hide);
		foreachPatternAnm(mMonteIcons[2], &TPatternAnmControl::show);
		foreachPatternAnm(mMonteIcons[2], &TPatternAnmControl::setupAnm);
		break;
	}
}

void TOptionSoundUnit::adjustSound()
{
	stopSound();

	const FabricatedSoundSettings& setting
	    = cSoundSettings[mSelectionText->getNumber()];
	SMSGetMSound()->startSoundSystemSE(setting.mSoundSystemSE, 0, &mMusic, 0);

	mMusicFrameCtrl.setFrame(0.0f);
}

void TOptionSoundUnit::setInfluencedAlphaRecursive(J2DPane* pane, bool flag)
{
	for (JSUTreeIterator<J2DPane> it = pane->getPaneTree()->getFirstChild();
	     it != pane->getPaneTree()->getEndChild(); ++it) {
		it->setInfluenceAlpha(flag);
		setInfluencedAlphaRecursive(it.getObject(), flag);
	}
}

void TOptionControl::load()
{
	JKRArchive* optionArch = (JKRArchive*)JKRFileLoader::getVolume("option");

	mScreen = new J2DSetScreen("option.blo", optionArch);
	mScreen->setCullBack(GX_CULL_BACK);
	mOptionTextA = (J2DTextBox*)mScreen->search('m_0a');
	mOptionTextB = (J2DTextBox*)mScreen->search('m_0b');
	mOptionTextA->setFont((JUTFont*)gpSystemFont);
	mOptionTextB->setFont((JUTFont*)gpSystemFont);
	SMSMakeTextBuffer(mOptionTextA, 0x200);
	SMSMakeTextBuffer(mOptionTextB, 0x200);
	static const char* const messagePaths[] = {
	    "/option/loadmessage_en.bmg", "/option/loadmessage_ge.bmg",
	    "/option/loadmessage_fr.bmg", "/option/loadmessage_sp.bmg",
	    "/option/loadmessage_it.bmg",
	};
	for (int i = 0; i < ARRAY_COUNT(messagePaths); ++i)
		mLocalizedMessageResources[i] =
		    JKRFileLoader::getGlbResource(messagePaths[i]);
	mBackArrow    = new TArrowControl(mScreen->search('yaji'));
	mBackArrow->loadLanguageTextures();
	mRumbleOption = new TOptionRumbleUnit(mScreen);
	mSoundOption  = new TOptionSoundUnit(mScreen);
	mSubtitleOption = new TOptionSubtitleUnit(mScreen);
	mLanguageOption = new TOptionLanguageUnit(mScreen);
	setType(SELECT_TYPE_RUMBLE_OPTION, true);
	loadSetting();
	mWasJumping = false;
	unk41       = true;
}

#pragma dont_inline on
void TOptionControl::loadSetting()
{
	switch (TFlagManager::getInstance()->getFlag(0xA0000)) {
	case 0:
		mSoundOption->setValue(0);
		break;
	case 1:
		mSoundOption->setValue(1);
		break;
	case 2:
		mSoundOption->setValue(2);
		break;
	}

	switch (TFlagManager::getInstance()->getFlag(0x90000)) {
	case 0:
		mRumbleOption->setValue(TOptionRumbleUnit::RUMBLE_TYPE_UNK0);
		break;
	case 1:
		mRumbleOption->setValue(TOptionRumbleUnit::RUMBLE_TYPE_UNK1);
		break;
	}

	int subtitle = TFlagManager::getInstance()->getFlag(0x90001);
	switch (subtitle) {
	case 0:
	case 1:
		mSubtitleOption->setValue(subtitle);
		break;
	}

	int language = TFlagManager::getInstance()->getFlag(0xA0001);
	switch (language) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
		mLanguageOption->setValue(language);
		break;
	}

	resetChangedSetting();
}
#pragma dont_inline off

void TOptionControl::movementCommon() { }

void TOptionControl::draw(J2DOrthoGraph* graph) { mScreen->draw(0, 0, graph); }

// mario walks from the card select screen to the options screen
bool TOptionControl::movementCard2Option()
{
	if (gpCameraOption->unk12 == 0) {
		mRumbleOption->mShouldRumble = false;
		mScreen->search('txp2')->show();
		mScreen->search('txp3')->show();
		mScreen->search('oya0')->show();
		mScreen->search('oya1')->show();
		mScreen->search('oya2')->show();
		mWasJumping = false;
		setType(mSelectedOption, true);
		int language = mLanguageOption->getValue();
		mBackArrow->replaceTexture('s_1', mBackArrow->mLanguageTextures[language]);
		mRumbleOption->changeTexture(language);
		mSoundOption->changeTexture(language);
		mSubtitleOption->changeTexture(language);
		int width = mLanguageOption->replaceTexture(
		    'm_4', mLanguageOption->mLanguageTextures[language]);
		ArrayWrapper<const u32>& items = mLanguageOption->mSelectionText->mItems;
		int count = items.size();
		int maxWidth = width;
		for (int i = 0; i < count; ++i) {
			J2DPane* pane = mLanguageOption->mScreen->search('lan0' + i);
			int itemWidth = pane->getWidth();
			if (itemWidth > maxWidth)
				maxWidth = itemWidth;
		}
		resizeOptionBubble(mLanguageOption->mSelectionBubble, maxWidth);
		changeTopMessage(language);
		return true;
	}

	return false;
}

#pragma dont_inline on
void TOptionControl::changeTopMessage(int language)
{
	if (language < 0 || language >= ARRAY_COUNT(mLocalizedMessageResources))
		language = 0;
	const char* message = (const char*)SMSGetMessageData(mLocalizedMessageResources[language], 0x1C);
	char buffer[0x200];
	char* out = buffer;
	while (*message) {
		if (*message == '@') {
			strncpy(out, cOptionMessageMarkerA, 0x44);
			out += 0x44;
		} else if (*message == '*') {
			strncpy(out, cOptionMessageMarkerB, 0x44);
			out += 0x44;
		} else {
			*out++ = *message;
		}
		++message;
	}
	*out = '\0';
	strncpy(mOptionTextA->getStringPtr(), buffer, 0x200);
	strncpy(mOptionTextB->getStringPtr(), buffer, 0x200);
}
#pragma dont_inline off

bool TOptionControl::movementOption()
{
	mBackArrow->update();
	mRumbleOption->update();
	mSoundOption->update();
	mSubtitleOption->update();
	mLanguageOption->update();

	checkInput();
	writeValue();

	if (gpCameraOption->unk0 & 1) {
		mSoundOption->stopSound();
		return true;
	}

	return false;
}

static inline void fake(TOptionSoundUnit* unit) { int v = unit->getValue(); }

// mario walks back from the options screen to the card select screen
bool TOptionControl::movementOption2Card()
{
	if (gpCameraOption->unk12 == 0) {
		mScreen->search('oya0')->hide();
		mScreen->search('oya1')->hide();
		mScreen->search('oya2')->hide();

		TOptionRumbleUnit* rumbleOption = mRumbleOption;
		if (mInitialRumbleValue == rumbleOption->getValue())
			fake(mSoundOption);

		return true;
	}

	return false;
}

void TOptionControl::setType(TOptionControl::SelectType type,
                             bool initial_options_entry)
{
	if (mSelectedOption != type || initial_options_entry) {
		mSelectedOption = type;
		switch (type) {
		case SELECT_TYPE_RUMBLE_OPTION:
			mRumbleOption->activate();
			mSoundOption->deactivate(initial_options_entry);
			mSubtitleOption->setState(initial_options_entry
			                              ? TOptionSubtitleUnit::STATE_INACTIVE
			                              : TOptionSubtitleUnit::STATE_DEACTIVATING);
			mLanguageOption->setState(initial_options_entry
			                              ? TOptionLanguageUnit::STATE_INACTIVE
			                              : TOptionLanguageUnit::STATE_DEACTIVATING);
			break;
		case SELECT_TYPE_SOUND_OPTION:
			mRumbleOption->deactivate(initial_options_entry);
			mSoundOption->activate();
			mSubtitleOption->setState(initial_options_entry
			                              ? TOptionSubtitleUnit::STATE_INACTIVE
			                              : TOptionSubtitleUnit::STATE_DEACTIVATING);
			mLanguageOption->setState(initial_options_entry
			                              ? TOptionLanguageUnit::STATE_INACTIVE
			                              : TOptionLanguageUnit::STATE_DEACTIVATING);
			break;
		case SELECT_TYPE_SUBTITLE_OPTION:
			mSubtitleOption->setState(TOptionSubtitleUnit::STATE_ACTIVE);
			mRumbleOption->deactivate(initial_options_entry);
			mSoundOption->deactivate(initial_options_entry);
			mLanguageOption->setState(initial_options_entry
			                              ? TOptionLanguageUnit::STATE_INACTIVE
			                              : TOptionLanguageUnit::STATE_DEACTIVATING);
			break;
		case SELECT_TYPE_LANGUAGE_OPTION:
			mLanguageOption->setState(TOptionLanguageUnit::STATE_ACTIVE);
			mSubtitleOption->setState(initial_options_entry
			                               ? TOptionSubtitleUnit::STATE_INACTIVE
			                               : TOptionSubtitleUnit::STATE_DEACTIVATING);
			mRumbleOption->deactivate(initial_options_entry);
			mSoundOption->deactivate(initial_options_entry);
			break;
		}

		if (!initial_options_entry)
			SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_CURSOR_COMMON, 0,
			                                   nullptr, 0);
	}
}

void TOptionControl::toggleCurType()
{
	switch (mSelectedOption) {
	case SELECT_TYPE_RUMBLE_OPTION:
		mRumbleOption->toggle();
		break;
	case SELECT_TYPE_SOUND_OPTION:
		mSoundOption->toggle();
		break;
	case SELECT_TYPE_SUBTITLE_OPTION:
		mSubtitleOption->toggle();
		break;
	case SELECT_TYPE_LANGUAGE_OPTION:
		mLanguageOption->toggle();
		break;
	}
}

void TOptionControl::checkInput()
{
	f32 fVar1 = gpMarDirector->unk18[0]->getMainStickInDir(0.0f, 1.0f);
	if (fVar1 >= 0.75f) {
		if (unk41) {
			unk41 = false;
		switch (mSelectedOption) {
		case SELECT_TYPE_RUMBLE_OPTION:
		case SELECT_TYPE_SOUND_OPTION:
			setType(SELECT_TYPE_RUMBLE_OPTION, false);
			break;
		case SELECT_TYPE_SUBTITLE_OPTION:
			setType(SELECT_TYPE_SOUND_OPTION, false);
			break;
		case SELECT_TYPE_LANGUAGE_OPTION:
			setType(SELECT_TYPE_SUBTITLE_OPTION, false);
			break;
		}
		}
	} else if (fVar1 <= -0.75f) {
		if (unk41) {
			unk41 = false;
			switch (mSelectedOption) {
		case SELECT_TYPE_RUMBLE_OPTION:
			setType(SELECT_TYPE_SOUND_OPTION, false);
			break;
		case SELECT_TYPE_SOUND_OPTION:
			setType(SELECT_TYPE_SUBTITLE_OPTION, false);
			break;
		case SELECT_TYPE_SUBTITLE_OPTION:
		case SELECT_TYPE_LANGUAGE_OPTION:
			setType(SELECT_TYPE_LANGUAGE_OPTION, false);
			break;
		}
		}
	} else {
		unk41 = true;
	}

	bool jumping = SMS_IsMarioStatusTypeJumping();
	if (!mWasJumping && jumping)
		toggleCurType();
	mWasJumping = jumping;
}

void TOptionControl::writeValue()
{
	TFlagManager::getInstance()->setFlag(0x90000, mRumbleOption->getValue());
	TFlagManager::getInstance()->setFlag(0xA0000, mSoundOption->getValue());
	TFlagManager::getInstance()->setFlag(0x90001, mSubtitleOption->getValue());
	TFlagManager::getInstance()->setFlag(0xA0001, mLanguageOption->getValue());
}

bool TOptionControl::isChangedSetting() const
{
	bool result = true;

	if (mInitialRumbleValue == mRumbleOption->getValue()
	    && mInitialSoundValue == mSoundOption->getValue()
	    && mInitialSubtitleValue == mSubtitleOption->getValue()
	    && mInitialLanguageValue == mLanguageOption->getValue())
		result = false;

	return result;
}

void TOptionControl::resetChangedSetting()
{
	mInitialRumbleValue = mRumbleOption->getValue();
	mInitialSoundValue  = mSoundOption->getValue();
	mInitialSubtitleValue = mSubtitleOption->getValue();
	mInitialLanguageValue = mLanguageOption->getValue();
}
