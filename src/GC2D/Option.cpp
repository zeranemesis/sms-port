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

// The five language variants of the option screen artwork. The array itself
// lives in .data (the pointers are not const), the individual directory
// strings are emitted into .sdata2.
static const char* langArray[] = { "", "ge/", "fr/", "sp/", "it/" };

namespace {

void tag_to_string(char*, u32) { }

void print_pane_tree(J2DPane*, int) { }

const TPatternAnmControl::TAnmChunk cRumbleAnm[] = {
	{ 'cnt0', 0.03 }, { 'cnt1', 0.03 }, { 'cnt2', 0.03 }, { 'cnt1', 0.03 },
	{ 'cnt2', 0.03 }, { 'cnt1', 0.03 }, { 'cnt2', 0.03 }, { 'cnt1', 0.03 },
	{ 'cnt2', 0.03 }, { 'cnt0', 2.0 },
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

const char cOptionMessageMarkerA[] =
	"\033" "GM[0]\033CC[64ff64]\033FX[26]\033FY[26]\033SH[3]\033CD[4]@"
	"\033GM[0]\033CC\033FX\033FY\033SH\033CU[4]";
const char cOptionMessageMarkerB[] =
	"\033" "GM[0]\033CC[dcdcdc]\033FX[26]\033FY[26]\033SH[3]\033CD[4]*"
	"\033GM[0]\033CC\033FX\033FY\033SH\033CU[4]";
const char* const cRumbleModeTextures[] = { "select_on.bti", "select_off.bti" };
const char* const cSoundModeTextures[] = { "select_surround.bti", "select_mono.bti",
	                                      "select_stereo.bti" };
const char* const cSubtitleTextures[] = { "select_on.bti", "select_off.bti" };
const u32 cSubtitleToggleItems[] = { 'sel6', 'sel5' };
const u32 cLanguageToggleItems[] = { 'lan0', 'lan1', 'lan2', 'lan3', 'lan4' };

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

void loadTexture(JUTTexture** textures, const char* name)
{
	char path[0xFF];
	for (int i = 0; i < ARRAY_COUNT(langArray); ++i) {
		snprintf(path, sizeof(path), "/option/timg/%s%s", langArray[i], name);
		JUTTexture* texture = new JUTTexture();
		if (texture)
			texture->storeTIMG((const ResTIMG*)JKRFileLoader::getGlbResource(path));
		textures[i] = texture;
	}
}

} // namespace

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

TArrowControl::TArrowControl(J2DScreen* screen, J2DPicture* picture)
    : mScreen(screen)
    , mPicture(picture)
    , unk18(true)
    , mPhase(0)
{
	mBounds = mPicture->mBounds;
	loadTexture(mLanguageTextures, "select_quit.bti");
}

#pragma dont_inline on
int TArrowControl::replaceTexture(u32 tag, JUTTexture* texture)
{
	J2DPicture* picture = (J2DPicture*)mScreen->search(tag);
	int widthDelta = texture->getWidth() - picture->getWidth();
	picture->changeTexture(texture->getTexInfo(), 0);
	JUTRect bounds = picture->getBounds();
	bounds.reform(0, 0, widthDelta, 0);
	picture->setBounds(bounds);
	return picture->getWidth();
}
#pragma dont_inline off

void TArrowControl::changeTexture(int language)
{
	replaceOptionPictureTexture(mScreen, 's_1', mLanguageTextures[language]);
}

void TArrowControl::update()
{
	updateAlpha();
	if (mPicture->getAlpha() != 0) {
		updateScale();
	}
}

void TArrowControl::updateAlpha()
{
	int iVar3 = unk18 ? 1 : -1;
	mPicture->setAlpha(
	    JGeometry::TUtil<s32>::clamp(iVar3 * 8 + mPicture->getAlpha(), 0, 255));
}

void TArrowControl::updateScale()
{
	int move = calcMoveX(mPhase);
	mPicture->setBounds(
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

void TPaneScalingControl::startAnm() { mFrameCtrl.setRate(0.75f); }

void TPaneScalingControl::stopAnm()
{
	mFrameCtrl.setRate(1.0f);
	mFrameCtrl.setFrame(-(f32)mFrameCtrl.getStart());
	startAnm();
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

void TPaneScalingControl::resize(int content_width)
{
	int width      = content_width + 0x1E;
	int half_width = (width - mInitialBounds.getHeight()) / 2;
	mInitialBounds.reform(-half_width, 0, half_width, 0);
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
	mSelectionBubble->setupAnm(0.05f, 0.75f);
	mSelectionBubble->stopAnm();

	// The text that says on/off for rumble in the options menu.
	mSelectionText = new TToggleControl(mScreen);
	mSelectionText->setupToggle(cRumbleToggleItems,
	                            ARRAY_COUNT(cRumbleToggleItems));

	loadTexture(mLanguageTextures, "select_rumble.bti");

	// TODO: the gamepad icons are never created here in the original; the
	// members stay uninitialized and are filled in somewhere else.
	for (int state = 0; state < ARRAY_COUNT(cRumbleModeTextures); ++state) {
		char path[0xFF];
		for (int language = 0; language < ARRAY_COUNT(langArray); ++language) {
			snprintf(path, sizeof(path), "/option/timg/%s%s", langArray[language],
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

int TOptionRumbleUnit::replaceTexture(u32 tag, JUTTexture* texture)
{
	return replaceOptionPictureTexture(mScreen, tag, texture);
}

#pragma dont_inline on
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
	mSelectionBubble->resize(maxWidth);
	mSelectionBubble->update();
}
#pragma dont_inline off

void TOptionRumbleUnit::checkRumble() { }

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
		break;

	case STATE_INACTIVE:
		break;
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
	SMSRumbleMgr->setActive(mSelectionText->getNumber() == 1);
}

void TOptionRumbleUnit::adjustView() { }

void TOptionRumbleUnit::hide() { }

void TOptionRumbleUnit::show() { }

void TOptionRumbleUnit::deactivate(bool force)
{
	if (force)
		setState(TOptionRumbleUnit::STATE_INACTIVE);
	else
		setState(TOptionRumbleUnit::STATE_DEACTIVATING);
}

void TOptionRumbleUnit::activate() { setState(TOptionRumbleUnit::STATE_ACTIVE); }

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

TOptionSubtitleUnit::TOptionSubtitleUnit(J2DScreen* screen)
    : mScreen(screen)
{
	mParentPane   = new TExPane(mScreen, 'txp2');
	mInitialAlpha = mParentPane->getPane()->getAlpha();
	mSelectionBubble = new TPaneScalingControl(mScreen->search('me_2'));
	mSelectionBubble->setupAnm(0.05f, 0.75f);
	mSelectionBubble->stopAnm();
	mSelectionText = new TToggleControl(mScreen);
	mSelectionText->setupToggle(cSubtitleToggleItems,
	                            ARRAY_COUNT(cSubtitleToggleItems));

	loadTexture(mLanguageTextures, "select_subtitles.bti");

	for (int state = 0; state < ARRAY_COUNT(cSubtitleTextures); ++state) {
		char path[0xFF];
		for (int language = 0; language < ARRAY_COUNT(langArray); ++language) {
			snprintf(path, sizeof(path), "/option/timg/%s%s", langArray[language],
			         cSubtitleTextures[state]);
			JUTTexture* texture = new JUTTexture();
			if (texture)
				texture->storeTIMG(
				    (const ResTIMG*)JKRFileLoader::getGlbResource(path));
			mStateTextures[state][language] = texture;
		}
	}

	setState(STATE_INACTIVE);
}

int TOptionSubtitleUnit::replaceTexture(u32 tag, JUTTexture* texture)
{
	return replaceOptionPictureTexture(mScreen, tag, texture);
}

#pragma dont_inline on
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
	mSelectionBubble->resize(maxWidth);
	mSelectionBubble->update();
}
#pragma dont_inline off

void TOptionSubtitleUnit::toggle()
{
	mSelectionText->toggle();
	SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_SELECT_COMMON, 0, nullptr, 0);
}

void TOptionSubtitleUnit::adjust() { }

void TOptionSubtitleUnit::show() { }

void TOptionSubtitleUnit::hide() { }

void TOptionSubtitleUnit::deactivate(bool force)
{
	if (force)
		setState(TOptionSubtitleUnit::STATE_INACTIVE);
	else
		setState(TOptionSubtitleUnit::STATE_DEACTIVATING);
}

void TOptionSubtitleUnit::activate()
{
	setState(TOptionSubtitleUnit::STATE_ACTIVE);
}

void TOptionSubtitleUnit::setValue(int value) { mSelectionText->setNumber(value); }

void TOptionSubtitleUnit::setState(TOptionSubtitleUnit::State state)
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

#pragma dont_inline on
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
#pragma dont_inline off

void TOptionSubtitleUnit::setInfluencedAlphaRecursive(J2DPane* pane, bool flag)
{
	for (JSUTreeIterator<J2DPane> it = pane->getPaneTree()->getFirstChild();
	     it != pane->getPaneTree()->getEndChild(); ++it) {
		it->setInfluenceAlpha(flag);
		setInfluencedAlphaRecursive(it.getObject(), flag);
	}
}

TOptionLanguageUnit::TOptionLanguageUnit(J2DScreen* screen)
    : mScreen(screen)
{
	mParentPane   = new TExPane(mScreen, 'txp3');
	mInitialAlpha = mParentPane->getPane()->getAlpha();
	mSelectionBubble = new TPaneScalingControl(mScreen->search('me_3'));
	mSelectionBubble->setupAnm(0.05f, 0.75f);
	mSelectionBubble->stopAnm();
	mSelectionText = new TToggleControl(mScreen);
	mSelectionText->setupToggle(cLanguageToggleItems,
	                            ARRAY_COUNT(cLanguageToggleItems));

	loadTexture(mLanguageTextures, "select_language.bti");

	setState(STATE_INACTIVE);
}

#pragma dont_inline on
int TOptionLanguageUnit::replaceTexture(u32 tag, JUTTexture* texture)
{
	return replaceOptionPictureTexture(mScreen, tag, texture);
}
#pragma dont_inline off

void TOptionLanguageUnit::changeTexture(int language)
{
	int maxWidth = replaceOptionPictureTexture(mScreen, 'm_4',
	                                           mLanguageTextures[language]);
	J2DPane* pane = mScreen->search('lan0' + mSelectionText->getNumber());
	int width     = pane->getWidth();
	if (width > maxWidth)
		maxWidth = width;
	mSelectionBubble->resize(maxWidth);
	mSelectionBubble->update();
}

void TOptionLanguageUnit::toggle()
{
	mSelectionText->toggle();
	SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_SELECT_COMMON, 0, nullptr, 0);
}

void TOptionLanguageUnit::adjust() { }

void TOptionLanguageUnit::show() { }

void TOptionLanguageUnit::hide() { }

void TOptionLanguageUnit::deactivate(bool force)
{
	if (force)
		setState(TOptionLanguageUnit::STATE_INACTIVE);
	else
		setState(TOptionLanguageUnit::STATE_DEACTIVATING);
}

void TOptionLanguageUnit::activate()
{
	setState(TOptionLanguageUnit::STATE_ACTIVE);
}

void TOptionLanguageUnit::setValue(int value)
{
	mSelectionText->setNumber(value);
}

#pragma dont_inline on
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
#pragma dont_inline off

void TOptionLanguageUnit::setState(TOptionLanguageUnit::State state)
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

void TOptionLanguageUnit::setInfluencedAlphaRecursive(J2DPane* pane, bool flag)
{
	for (JSUTreeIterator<J2DPane> it = pane->getPaneTree()->getFirstChild();
	     it != pane->getPaneTree()->getEndChild(); ++it) {
		it->setInfluenceAlpha(flag);
		setInfluencedAlphaRecursive(it.getObject(), flag);
	}
}

TOptionSoundUnit::TOptionSoundUnit(J2DScreen* screen)
    : mScreen(screen)
    , mMusicFrameCtrl(0.0f)
{
	mParentPane   = new TExPane(mScreen, 'oya2');
	mInitialAlpha = mParentPane->getPane()->getAlpha();

	// The speech bubble around the mono/stereo/surround text that pulsates
	// when this setting is selected.
	mSelectionBubble = new TPaneScalingControl(mScreen->search('me_1'));
	mSelectionBubble->setupAnm(0.05f, 0.75f);
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
	mMusicFrameCtrl.setRate(0.75f);
	mMusic = nullptr;
	loadTexture(mLanguageTextures, "select_sound.bti");

	for (int mode = 0; mode < ARRAY_COUNT(cSoundModeTextures); ++mode) {
		char path[0xFF];
		for (int language = 0; language < ARRAY_COUNT(langArray); ++language) {
			snprintf(path, sizeof(path), "/option/timg/%s%s", langArray[language],
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

int TOptionSoundUnit::replaceTexture(u32 tag, JUTTexture* texture)
{
	return replaceOptionPictureTexture(mScreen, tag, texture);
}

#pragma dont_inline on
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
	mSelectionBubble->resize(maxWidth);
	mSelectionBubble->update();
}
#pragma dont_inline off

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

void TOptionSoundUnit::updatePatternAnm(){
	ArrayWrapper<TPatternAnmControl*>& ary = mMonteIcons[mSelectionText->getNumber()];

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

void TOptionSoundUnit::show() { }

void TOptionSoundUnit::hide()
{
	foreachPatternAnm(mMonteIcons[0], &TPatternAnmControl::hide);
	foreachPatternAnm(mMonteIcons[1], &TPatternAnmControl::hide);
	foreachPatternAnm(mMonteIcons[2], &TPatternAnmControl::hide);
}

void TOptionSoundUnit::adjust()
{
	adjustView();
	JAIGlobalParameter::setParamSoundOutputMode(
	    cSoundSettings[mSelectionText->getNumber()].mOutputMode);
}

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

void TOptionSoundUnit::adjustSound()
{
	stopSound();

	SMSGetMSound()->startSoundSystemSE(
	    cSoundSettings[mSelectionText->getNumber()].mSoundSystemSE, 0, &mMusic, 0);

	mMusicFrameCtrl.setRate(1.0f);
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
	mLocalizedMessageResources[0] = JKRFileLoader::getGlbResource("/option/loadmessage_en.bmg");
	mLocalizedMessageResources[1] = JKRFileLoader::getGlbResource("/option/loadmessage_ge.bmg");
	mLocalizedMessageResources[2] = JKRFileLoader::getGlbResource("/option/loadmessage_fr.bmg");
	mLocalizedMessageResources[3] = JKRFileLoader::getGlbResource("/option/loadmessage_sp.bmg");
	mLocalizedMessageResources[4] = JKRFileLoader::getGlbResource("/option/loadmessage_it.bmg");
	mBackArrow    = new TArrowControl(mScreen, (J2DPicture*)mScreen->search('yaji'));
	mRumbleOption = new TOptionRumbleUnit(mScreen);
	mSoundOption  = new TOptionSoundUnit(mScreen);
	mSubtitleOption = new TOptionSubtitleUnit(mScreen);
	mLanguageOption = new TOptionLanguageUnit(mScreen);
	setType(SELECT_TYPE_RUMBLE_OPTION, true);
	loadSetting();
	mWasJumping = false;
	unk41       = true;
}

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
		// TODO: the original really does call adjustView() here and only
		// here; the semantics of the (empty) adjustView are unknown.
		mRumbleOption->setValue(TOptionRumbleUnit::RUMBLE_TYPE_UNK1);
		mRumbleOption->adjustView();
		break;
	}

	switch (TFlagManager::getInstance()->getFlag(0x90001)) {
	case 0:
		mSubtitleOption->setValue(0);
		break;
	case 1:
		mSubtitleOption->setValue(1);
		break;
	}

	switch (TFlagManager::getInstance()->getFlag(0xA0001)) {
	case 0:
		mLanguageOption->setValue(0);
		break;
	case 1:
		mLanguageOption->setValue(1);
		break;
	case 2:
		mLanguageOption->setValue(2);
		break;
	case 3:
		mLanguageOption->setValue(3);
		break;
	case 4:
		mLanguageOption->setValue(4);
		break;
	}

	resetChangedSetting();
}

void TOptionControl::draw(J2DOrthoGraph* graph) { mScreen->draw(0, 0, graph); }

// TODO: the original body of movementCommon() is 2080 bytes once inlined;
// unknown, so left empty.
void TOptionControl::movementCommon() { }

#pragma dont_inline on
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
		const ArrayWrapper<const u32>& items = mLanguageOption->mSelectionText->mItems;
		J2DPane* pane = mLanguageOption->mScreen->search(
		    'lan0' + (mLanguageOption->mSelectionText->mCurItem - items.begin()) / 4);
		int itemWidth = pane->getWidth();
		if (itemWidth > width)
			width = itemWidth;
		mLanguageOption->mSelectionBubble->resize(width);
		mLanguageOption->mSelectionBubble->update();
		changeTopMessage(language);
		return true;
	}

	return false;
}
#pragma dont_inline off

#pragma dont_inline on
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
#pragma dont_inline off

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

#pragma dont_inline on
void TOptionControl::setType(TOptionControl::SelectType type,
                             bool initial_options_entry)
{
	if (mSelectedOption != type || initial_options_entry) {
		mSelectedOption = type;
		switch (type) {
		case SELECT_TYPE_RUMBLE_OPTION:
			mRumbleOption->activate();
			mSoundOption->deactivate(initial_options_entry);
			mSubtitleOption->deactivate(initial_options_entry);
			mLanguageOption->deactivate(initial_options_entry);
			break;
		case SELECT_TYPE_SOUND_OPTION:
			mRumbleOption->deactivate(initial_options_entry);
			mSoundOption->activate();
			mSubtitleOption->deactivate(initial_options_entry);
			mLanguageOption->deactivate(initial_options_entry);
			break;
		case SELECT_TYPE_SUBTITLE_OPTION:
			mSubtitleOption->activate();
			SMSRumbleMgr->stop();
			mRumbleOption->deactivate(initial_options_entry);
			mSoundOption->deactivate(initial_options_entry);
			mLanguageOption->deactivate(initial_options_entry);
			break;
		case SELECT_TYPE_LANGUAGE_OPTION:
			mLanguageOption->activate();
			mSubtitleOption->deactivate(initial_options_entry);
			SMSRumbleMgr->stop();
			mRumbleOption->deactivate(initial_options_entry);
			mSoundOption->deactivate(initial_options_entry);
			break;
		}

		if (!initial_options_entry)
			SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_CURSOR_COMMON, 0,
			                                   nullptr, 0);
	}
}
#pragma dont_inline off

void TOptionControl::changeTexture(int language)
{
	mBackArrow->replaceTexture('s_1', mBackArrow->mLanguageTextures[language]);
	mRumbleOption->changeTexture(language);
	mSoundOption->changeTexture(language);
	mSubtitleOption->changeTexture(language);
	mLanguageOption->changeTexture(language);
	changeTopMessage(language);
}

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
		changeTexture(mLanguageOption->getValue());
		break;
	}
}

void TOptionControl::writeValue()
{
	TFlagManager::getInstance()->setFlag(0x90000, mRumbleOption->getValue());
	TFlagManager::getInstance()->setFlag(0xA0000, mSoundOption->getValue());
	TFlagManager::getInstance()->setFlag(0x90001, mSubtitleOption->getValue());
	TFlagManager::getInstance()->setFlag(0xA0001, mLanguageOption->getValue());
}

void TOptionControl::checkInput()
{
	f32 fVar1 = gpMarDirector->unk18[0]->getMainStickInDir(1.0f, 0.75f);
	// Read once: both switch arms below use it, and the ROM hoists the load
	// ahead of the float compare.
	SelectType selected = mSelectedOption;
	// NOTE: the comparison is spelled with the constant on the left because
	// the ROM emits `fcmpo cr0, f0, f1` (constant first) here.
	if (-0.75f <= fVar1) {
		if (unk41) {
			unk41 = false;
		switch (selected) {
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
	} else if (fVar1 <= 50.0f) {
		if (unk41) {
			unk41 = false;
			switch (selected) {
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

bool TOptionControl::isChangedSetting() const
{
	// NOTE: written as a chain of guarded assignments rather than one big
	// `&&` so that each stage can bail out early.
	bool changed_by_rumble_and_sound = true;
	bool changed_by_subtitle         = true;
	bool changed_by_language         = true;

	if (mInitialRumbleValue == mRumbleOption->getValue()
	    && mInitialSoundValue == mSoundOption->getValue())
		changed_by_rumble_and_sound = false;

	if (!changed_by_rumble_and_sound
	    && mInitialSubtitleValue == mSubtitleOption->getValue())
		changed_by_subtitle = false;

	if (!changed_by_subtitle
	    && mInitialLanguageValue == mLanguageOption->getValue())
		changed_by_language = false;

	return changed_by_language;
}

void TOptionControl::resetChangedSetting()
{
	mInitialRumbleValue = mRumbleOption->getValue();
	mInitialSoundValue  = mSoundOption->getValue();
	mInitialSubtitleValue = mSubtitleOption->getValue();
	mInitialLanguageValue = mLanguageOption->getValue();
}
