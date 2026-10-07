#include <System/GCLogoDir.hpp>
#include <dolphin/vi.h>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JDrama/JDRDStageGroup.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <JSystem/JDrama/JDRDStage.hpp>
#include <JSystem/JDrama/JDREfbCtrl.hpp>
#include <JSystem/JDrama/JDRCamera.hpp>
#include <JSystem/JDrama/JDRScreen.hpp>
#include <System/Application.hpp>
#include <System/MarioGamePad.hpp>
#include <System/Resolution.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <GC2D/ScrnFader.hpp>
#include <GC2D/ProgSelect.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <System/DummyMactorString.hpp>
#include <System/DummyStrings.hpp>

void TNintendo2D::perform(u32 cue, JDrama::TGraphics*)
{

	if (cue & CUE_DRAW) {
		GXSetCullMode(GX_CULL_BACK);
		Mtx afStack_3c;
		MTXTrans(afStack_3c, 0.0, 0.0, 0.0);
		GXLoadPosMtxImm(afStack_3c, GX_PNMTX0);
		GXSetCurrentMtx(GX_PNMTX0);
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S8, 0);
		GXClearVtxDesc();
		GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
		GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
		GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		GXSetNumChans(1);
		GXSetNumTexGens(1);
		GXSetNumTevStages(1);
		unk10->load(GX_TEXMAP0);
		GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c,
		                  GX_FALSE, 0x7d);
		GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
		GXSetTevOp(GX_TEVSTAGE0, GX_MODULATE);

		GXBegin(GX_QUADS, GX_VTXFMT0, 4);
		GXPosition3s16(unk14.x1, unk14.y1, 0);
		GXColor1u32(unk24);
		GXTexCoord2s8(0, 0);
		GXPosition3s16(unk14.x2, unk14.y1, 0);
		GXColor1u32(unk24);
		GXTexCoord2s8(1, 0);
		GXPosition3s16(unk14.x2, unk14.y2, 0);
		GXColor1u32(unk24);
		GXTexCoord2s8(1, 1);
		GXPosition3s16(unk14.x1, unk14.y2, 0);
		GXColor1u32(unk24);
		GXTexCoord2s8(0, 1);
		GXEnd();
	}
}

TGCLogoDir::TGCLogoDir()
    : JDrama::TDirector()
    , mOverallState(0)
    , mState(0)
{
	f32 sync       = SMSGetVSyncTimesPerSec();
	mLogoShowTimer = 0;
	mRefreshRate   = sync;
	unk3C          = 0;
	unk44          = 0;
}

void TGCLogoDir::setup(JDrama::TDisplay* param_1, TMarioGamePad* param_2)
{
	unk14    = new JDrama::TDStageGroup(param_1);
	mGamePad = param_2;
	getGamePad()->setFlag(1);

	JDrama::TViewObjPtrListT<JDrama::TViewObj>* rootViewObjs
	    = new JDrama::TViewObjPtrListT<JDrama::TViewObj>("root View Objs");
	unk10 = rootViewObjs;

	JDrama::TViewObjPtrListT<JDrama::TViewObj>* group2d
	    = new JDrama::TViewObjPtrListT<JDrama::TViewObj>("Group 2D");
	rootViewObjs->getChildren().push_back(group2d);

	unk34 = new JUTTexture(
	    (const ResTIMG*)JKRGetResource("/nintendo/timg/nintendo_376x104.bti"));
	unk30 = new JUTTexture(
	    (const ResTIMG*)JKRGetResource("/nintendo/timg/title_dolby_mark.bti"));
	unk20 = new TNintendo2D(unk34);

	group2d->getChildren().push_back(unk20);

	mProgSelect           = new TProgSelect(0);
	mProgSelect->mGamePad = getGamePad();
	mProgSelect->unkC.on(0xffff);
	group2d->getChildren().push_back(mProgSelect);

	JDrama::TDStageDisp* stageDisp = new JDrama::TDStageDisp("<DStageDisp>", 0);
	unk14->getChildren().push_back(stageDisp);
	JDrama::TRect logoRendArea(0, 0, SMSGetGCLogoRenderWidth(),
	                           SMSGetGCLogoRenderHeight());
	stageDisp->getEfbCtrlDisp()->TEfbCtrl::setSrcRect(logoRendArea);

	f32 h     = logoRendArea.getHeight() * 1.09375f;
	f32 fVar3 = (640 - logoRendArea.getWidth()) / 2;
	f32 fVar2 = (480 - h) / 2;

	JDrama::TOrthoProj* proj = new JDrama::TOrthoProj(
	    -1.0f, 1.0f, fVar3, fVar2, fVar2 + h, fVar3 + logoRendArea.getWidth());
	group2d->getChildren().push_back(proj);

	JDrama::TScreen* screen = new JDrama::TScreen(logoRendArea, "Screen 2D");
	stageDisp->getUnk14()->getChildren().push_back(screen);
	screen->assignCamera(proj);
	screen->assignViewObj(group2d);

	gpApplication.mFader->setColor(JUtility::TColor(0, 0, 0, 0));
	gpApplication.getFader()->startWipe(14, 0.4f, 0.0f);
}

TGCLogoDir::~TGCLogoDir() { getGamePad()->offFlag(0x1); }

// TODO: frame exact (research c-r29: getFader(), getProgSelect() and one
// getNintendo2D() read; the binder at a second TNintendo2D read overshoots
// by 8, and any single one of the three gives the same layout). Retail has
// one more word created between the rect and colour temporaries (0x58/0x50)
// where ours has it as alignment above the rect; named rect or colour
// locals, TColor(u32) and a `.get()` colour are worse.
int TGCLogoDir::direct()
{
	int desiredAppState = TApplication::APP_STATE_DEFAULT;

	JDrama::TDirector::direct();

	int nextState = mOverallState;
	switch (mOverallState) {
	case 0:
		if (direct_nlogo()) {
			if (getGamePad()->isSomethingPushed()) {
				desiredAppState = TApplication::APP_STATE_DONE;
				break;
			}

			getProgSelect()->unkC.on(0xffff);
			getNintendo2D()->unk10 = unk30;
			unk20->unk14 = JUTRect(254, 201, 404, 271);
			unk20->unk24 = JUtility::TColor(255, 255, 255, 255);
			gpApplication.getFader()->startWipe(14, 0.4f, 0.0f);
			nextState = 1;
		}
		break;

	case 1:
		if (direct_dolby())
			desiredAppState = TApplication::APP_STATE_DONE;
		break;
	}

	if (nextState != mOverallState) {
		mOverallState = nextState;
		mState        = 0;
	}

	return desiredAppState;
}

// Cases 0 and 1 of direct_nlogo share this block; expanding it as a helper
// gives retail's register reuse of the zero `cancelled` for the stores.
static inline bool GCLogoCancelProgressive(TGCLogoDir* dir)
{
	bool cancelled = false;

	if ((int)dir->getProgSelect()->unkC.mValue != 0 && VIGetTvFormat() == 0
	    && VIGetDTVStatus() == 1) {
		if (OSGetProgressiveMode() == 1) {
			dir->getProgSelect()->unkC = 0;
			cancelled              = true;
		} else if (dir->getGamePad()->getButton() & JUTGamePad::B) {
			dir->unk44 += 1;
			if (dir->unk44 / SMSGetVSyncTimesPerSec() > 1.0f) {
				dir->getProgSelect()->unkC = 0;
				cancelled              = true;
			}
		} else {
			dir->unk44 = 0;
		}
	}

	return cancelled;
}

// TODO: frame exact with getFader() and getProgSelect() (research c-r29);
// retail still reloads mState after the isSomethingPushed test (see
// direct_dolby).
bool TGCLogoDir::direct_nlogo()
{
	bool ended    = false;
	int nextState = mState;
	switch (mState) {
	case 0:
		if (gpApplication.getFader()->isFullyFadedIn()) {
			nextState = (int)getProgSelect()->unkC.mValue == 0 ? 3 : 1;

			SMSGetMSound()->startSoundSystemSE(MSD_SE_MV_CHAO, 0, nullptr, 0);
			mLogoShowTimer = 0;
		} else {
			GCLogoCancelProgressive(this);
		}
		break;

	case 1:
		mLogoShowTimer += 1;
		if (mLogoShowTimer / mRefreshRate >= 0.8f) {
			nextState = 2;
			OSSetProgressiveMode(0);
		} else {
			if (GCLogoCancelProgressive(this)) {
				mLogoShowTimer = 0;
				nextState      = 3;
			}
		}
		break;

	case 3:
		if (getProgSelect()->mHideTextBoxes) {
			mLogoShowTimer = 0;
			nextState      = 4;
		}
		break;

	case 4:
		mLogoShowTimer += 1;
		if (mLogoShowTimer / mRefreshRate >= 2.0f)
			nextState = 2;
		break;

	case 2:
		if (gpApplication.getFader()->isFullyFadedOut())
			ended = true;
		break;
	}

	if (mState != 2 && mGamePad->isSomethingPushed())
		nextState = 2;

	if (nextState != mState) {
		if (nextState != 2)
			(void)nextState; // assert?
		else
			gpApplication.getFader()->startWipe(15, 0.4f, 0.0f);
		mState = nextState;
	}

	return ended;
}

// TODO: frame exact with getFader(), getGamePad() and the BOOL
// isSomethingPushed() (research c-r29); what is left is retail's reload of
// mState after the isSomethingPushed test, where ours reuses the load from
// the `!= 2` compare (a frontend CSE temporary). The frame bound leaves no
// room for a word, so whatever blocks the CSE there adds no object.
// c-h18 inert for the reload: nested ifs, a state-change helper (by
// pointer or `int&`), a comparison helper and raw mResetFlag.check; c-r29
// inert: an inline mState reader at the test or the tail, `mState !=
// nextState`, a named or `!= false` pad result.
bool TGCLogoDir::direct_dolby()
{
	bool ended    = false;
	int nextState = mState;
	switch (mState) {
	case 0:
		if (gpApplication.getFader()->isFullyFadedIn()) {
			mLogoShowTimer = 0;
			nextState      = 1;
		}
		break;

	case 1:
		mLogoShowTimer += 1;
		if (mLogoShowTimer / mRefreshRate >= 0.8f)
			nextState = 2;
		break;

	case 2:
		if (gpApplication.getFader()->isFullyFadedOut())
			ended = true;
		break;
	}

	if (mState != 2 && getGamePad()->isSomethingPushed())
		nextState = 2;

	if (nextState != mState) {
		if (nextState != 2)
			(void)nextState; // assert?
		else
			gpApplication.getFader()->startWipe(15, 0.4f, 0.0f);
		mState = nextState;
	}

	return ended;
}
