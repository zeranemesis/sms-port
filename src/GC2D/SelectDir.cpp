#include <System/SelectDir.hpp>

#include <dolphin/os.h>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JKernel/JKRMemArchive.hpp>
#include <JSystem/JDrama/JDRDStageGroup.hpp>
#include <JSystem/JDrama/JDRDStage.hpp>
#include <JSystem/JDrama/JDRCamera.hpp>
#include <JSystem/JDrama/JDREfbCtrl.hpp>
#include <JSystem/JDrama/JDRScreen.hpp>
#include <JSystem/JDrama/JDRViewObjPtrList.hpp>
#include <JSystem/JParticle/JPAEmitterManager.hpp>
#include <JSystem/JParticle/JPAResourceManager.hpp>
#include <GC2D/ScrnFader.hpp>
#include <GC2D/SelectMenu.hpp>
#include <GC2D/SelectShine2.hpp>
#include <MSound/MSound.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <System/Application.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/FlagManager.hpp>
#include <System/MarioGamePad.hpp>
#include <System/Resolution.hpp>
#include <System/DummyMactorString.hpp>
#include <System/DummyStrings.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <System/DummyStrings.hpp>

extern OSThread gSetupThread;
extern u8* gpSetupThreadStack;

TSelectDir::TSelectDir()
    : unk20(0)
    , unk24(0)
    , unk28(0)
    , unk2C(0)
    , unk30(0)
    , unk34(0)
    , unk38(false)
    , unk3C(0)
    , unk40(0)
    , unk44(0)
    , unk48(0)
    , unk4C(false)
{
}

// Closure batch 128: exact. The 8 bytes batch 123 could only find as unnamed
// padding are one binding level over the gamepad member -- the shape a
// `TMarioGamePad* TSelectDir::getGamePad()` accessor that binds its result
// would have. Parked TU-local below; header batch item. (A binding level over
// the `JKRFileLoader::getVolume` cast closes it identically, so the byte is
// certain and its owner is not; the gamepad accessor is the spelling the rest
// of the tree uses.)
TSelectDir::~TSelectDir()
{
	JKRMemArchive* arc = (JKRMemArchive*)JKRFileLoader::getVolume("select");
	if (arc)
		arc->unmountFixed();

	getGamePad()->offFlag(1);
}

void TSelectDir::setup(JDrama::TDisplay* display, TMarioGamePad* gamePad,
                       unsigned char stage)
{
	unk1C = display;
	unk18 = gamePad;
	SMSRumbleMgr->reset();
	unk40 = stage;

	OSCreateThread(&gSetupThread, &setupThreadFunc, this,
	               gpSetupThreadStack + 0x10000, 0x10000, 0x11, 0);
	OSResumeThread(&gSetupThread);
}

void* TSelectDir::setupThreadFunc(void* param_1)
{
	((TSelectDir*)param_1)->rsetup();
}

int TSelectDir::rsetup()
{
	// TODO: 99.6% with 370 operand-only markers and *no* structural marker;
	// GMSE01 frame is 0x610 against retail's 0x648, with list/constructor
	// temporary offsets and LookAtCamera vector ordering/register differences
	// remaining. Closure batch 128 classified the residue: of the 233 markers
	// that carry exactly one `r1` displacement on each side, the retail-minus-
	// ours delta is *not* uniform -- 40 bytes at 61 slots, 56 at 59, 52 at 46,
	// 48 at 33, 36 at 11, 44 at 9, 32 at 7, 64 at 4, 28 at 2 and one slot 56
	// *lower*. So this is not one dead carrier under everything (contrast
	// `direct()`, where every slot shifts by the same 0x80) but several
	// separately misplaced objects inside a 0x600-byte local area, and the
	// total gap is only 56 bytes. A per-object slot map (grouping the deltas
	// by the source construct each slot belongs to) is the prerequisite for
	// any lever work here; do not spend lever trials on it before that.
	// Closure re-pass (batch 161) built that map and the residue is the
	// known-open JGadget pool word.  Sorting the 233 single-displacement
	// markers by *our* offset, the delta rises monotonically in 4-byte steps
	// through the low pool -- our slots run 0x204/0x208, 0x20c-0x21c,
	// 0x220-0x230, 0x234-0x244, 0x248-0x258, 0x25c... at a stride of 0x14,
	// where retail's run 0x220/0x224, 0x22c-0x23c, 0x244-0x254, 0x25c-0x26c,
	// 0x274-0x284, 0x28c... at a stride of 0x18.  Each group is one
	// `getChildren().push_back()` expansion: the iterator/insert pair stores
	// five words on both sides, but retail reserves six (0x14 rounded up to
	// 8), so every expansion costs us four bytes.  The `.28`/`.32` cluster of
	// deltas is just how many expansions precede a slot; the remaining bands
	// (0x34 and 0x38 near the top) are the same accumulation after the last
	// expansion.  RULES.md lists this as the JGadget pool-word class, so no
	// lever trials here until that class is solved for the tree.
	// Closure 217 rebuilt the slot map per instruction (all 1096 align, 0
	// structural markers) and split the 0x38 into three pieces: +28 below
	// our first referenced pool slot (0x204 against retail's 0x220), +20 of
	// pad inside the pool (retail's five `+4` steps, one per expansion
	// sub-block, ours contiguous), and +8 between the pool top and the
	// second temp block, whose stride is 0x14 on both sides but whose
	// retail groups alternate 0x10/0x14.  So it is not one knob.
	// Research 211's receiver knob was then tried in the inverse direction
	// it predicts, and every spelling is worse: dropping `root` for
	// `((TViewObjPtrListT<TViewObj>*)unk10)->getChildren().push_back(x)` at
	// the four root sites is +9 instructions (97.3%, the member is reloaded
	// per site) and still leaves the pool contiguous; a TU-local binder
	// returning the cast is the same +9 at frame 0x630; `root->insert(x)`
	// at the four root sites rewrites the whole map (frame 0x5a8, pool
	// [60]); `insert` at all twenty sites is 85.1% and +107 instructions;
	// and keeping `root` but initialising it from `unk10` after the member
	// store costs one reload for frame 0x608.  The pool stays [48]
	// contiguous in every packing variant, so retail's five pad words are
	// not the receiver's naming here -- the padded sub-blocks are the first
	// five expansions only, and whatever distinguishes them is upstream of
	// the call site.
	// cc42: a TU-local named-reference `getChildren()` binder at the first
	// five push sites (the four root pushes and group2D's) lands frame 0x648
	// exactly, but the per-expansion stride of the last six expansions is
	// untouched (their low blocks still 4 short each, their upper blocks 4
	// long each); descent over binder/pointer/pointer-then-reference/named-
	// group forks at all twenty sites finds nothing better. Not applied.
	// c-k31 (dbg slot map): retail's three depth-1 words between the
	// seventh and eighth push are the getGamePad()/getSelectMenu() reads of
	// the gamepad stores, and the TLookAtCamera vectors are created in
	// position/up/target order (see JDRCamera.hpp): gap 34 -> 14, webs 2 -> 0,
	// frame 0x638. The rest is the `stageDisp->getUnk14()->getChildren()`
	// site: retail binds the list's getChildren() one level below the
	// TDStageDisp receiver (one word moves from depth 2 to depth 3 at all
	// five screen pushes). A TDStageDisp accessor returning
	// `((TViewObjPtrListT<TViewObj>*)unk14)->getChildren()` lands it (gap 4,
	// 263 markers) and moves the same boundary in MenuDir, MovieDirector and
	// GCLogoDir, but GCLogoDir::setup then drops 29 -> 62 markers; parked
	// for the owner (docs/catalog/frame-gaps.md, closure c-k31). Still open
	// after it: one word above group3D, one depth-2 word between the
	// seventh and eighth push, two depth-2 words after the last push.
	void* arcData = SMSLoadArchive("/data/select.arc", 0, 0, 0);

	JKRMemArchive* archive = new JKRMemArchive;
	if (!archive->mountFixed(arcData, MBF_0))
		return 1;

	unk2C = archive;

	JDrama::TViewObjPtrListT<JDrama::TViewObj>* root
	    = new JDrama::TViewObjPtrListT<JDrama::TViewObj>("root View Objs");
	unk10 = root;

	unk14 = new JDrama::TDStageGroup(unk1C);
	unk20 = new TSelectMenu("<TSelectMenu>");
	unk28 = new TSelectShineManager("<SelectShineManger>");
	unk24 = new TSelectGrad("<TSelectGrad>");
	unk24->setStageColor(unk40);

	JDrama::TViewObjPtrListT<JDrama::TViewObj>* group3D
	    = new JDrama::TViewObjPtrListT<JDrama::TViewObj>("Group 3D");
	JDrama::TViewObjPtrListT<JDrama::TViewObj>* group2D
	    = new JDrama::TViewObjPtrListT<JDrama::TViewObj>("Group 2D");
	JDrama::TViewObjPtrListT<JDrama::TViewObj>* groupGrad
	    = new JDrama::TViewObjPtrListT<JDrama::TViewObj>("Group Grad");
	JDrama::TViewObjPtrListT<JDrama::TViewObj>* group2DParticle
	    = new JDrama::TViewObjPtrListT<JDrama::TViewObj>("Group 2D Particle");

	root->getChildren().push_back(group2D);
	root->getChildren().push_back(group3D);
	root->getChildren().push_back(groupGrad);
	root->getChildren().push_back(group2DParticle);

	group2D->getChildren().push_back(unk20);
	group3D->getChildren().push_back(unk28);
	groupGrad->getChildren().push_back(unk24);

	getGamePad()->mFlags      = 1;
	getSelectMenu()->mGamePad = getGamePad();

	JPAResourceManager* resourceManager2D = new JPAResourceManager(9, 0x200, 0);
	JPAResourceManager* resourceManager3D = new JPAResourceManager(9, 0x200, 0);

	resourceManager2D->load("/select/particle/ms_2d_shine_promi.jpa", 0);
	resourceManager2D->load("/select/particle/ms_2d_shine_senko.jpa", 1);
	resourceManager2D->load("/select/particle/ms_2d_shine_kira.jpa", 2);
	resourceManager2D->load("/select/particle/ms_2d_shine_kira_em.jpa", 3);
	resourceManager3D->load("/select/particle/ms_2d_scsel_on_a.jpa", 4);
	resourceManager3D->load("/select/particle/ms_2d_scsel_on_a2.jpa", 5);
	resourceManager3D->load("/select/particle/ms_2d_scsel_on_b.jpa", 6);
	resourceManager3D->load("/select/particle/ms_2d_scsel_on_c.jpa", 7);
	resourceManager3D->load("/select/particle/ms_2d_scsel_on_d.jpa", 8);

	unk30 = new JPAEmitterManager(resourceManager2D, 0x400, 0x80, 0x100, 0);
	unk34 = new JPAEmitterManager(resourceManager3D, 0x400, 0x80, 0x100, 0);

	TEmitterViewObj* emitterView2D = new TEmitterViewObj(unk30);
	group3D->getChildren().push_back(emitterView2D);

	TEmitterViewObj* emitterView3D = new TEmitterViewObj(unk34);
	group2DParticle->getChildren().push_back(emitterView3D);

	JDrama::TDStageDisp* stageDisp = new JDrama::TDStageDisp("<DStageDisp>", 0);
	unk14->getChildren().push_back(stageDisp);

	JDrama::TRect rect(0, 0, SMSGetTitleRenderWidth(),
	                   SMSGetTitleRenderHeight());
	stageDisp->getEfbCtrlDisp()->TEfbCtrl::setSrcRect(rect);

	JDrama::TOrthoProj* gradCamera
	    = new JDrama::TOrthoProj(-100.0f, 100.0f, 0.0f, 16.0f, 464.0f, 600.0f);
	groupGrad->getChildren().push_back(gradCamera);

	JDrama::TScreen* gradScreen = new JDrama::TScreen(rect, "Screen Grad");
	stageDisp->getUnk14()->getChildren().push_back(gradScreen);
	gradScreen->assignCamera(gradCamera);
	gradScreen->assignViewObj(groupGrad);

	JDrama::TOrthoProj* screen2DCamera
	    = new JDrama::TOrthoProj(-100.0f, 100.0f, 0.0f, 16.0f, 464.0f, 600.0f);
	group2D->getChildren().push_back(screen2DCamera);

	JDrama::TScreen* screen2D = new JDrama::TScreen(rect, "Screen 2D");
	stageDisp->getUnk14()->getChildren().push_back(screen2D);
	screen2D->assignCamera(screen2DCamera);
	screen2D->assignViewObj(group2D);
	unk48 = screen2D;

	JDrama::TLookAtCamera* camera3D = new JDrama::TLookAtCamera(
	    JGeometry::TVec3<f32>(300.0f, 240.0f, 1300.0f),
	    JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f),
	    JGeometry::TVec3<f32>(300.0f, 240.0f, 0.0f), 30.0f, 1.3333334f,
	    "<TLookAtCamera>");
	group3D->getChildren().push_back(camera3D);

	JDrama::TScreen* screen3D = new JDrama::TScreen(rect, "Screen 3D");
	stageDisp->getUnk14()->getChildren().push_back(screen3D);
	screen3D->assignCamera(camera3D);
	screen3D->assignViewObj(group3D);

	JDrama::TOrthoProj* screen2DCamera2
	    = new JDrama::TOrthoProj(-100.0f, 100.0f, 0.0f, 16.0f, 464.0f, 600.0f);
	group2D->getChildren().push_back(screen2DCamera2);

	JDrama::TScreen* screen2D2 = new JDrama::TScreen(rect, "Screen 2D");
	stageDisp->getUnk14()->getChildren().push_back(screen2D2);
	screen2D2->assignCamera(screen2DCamera2);
	screen2D2->assignViewObj(group2D);
	unk44 = screen2D2;

	JDrama::TOrthoProj* particleCamera
	    = new JDrama::TOrthoProj(-500.0f, 500.0f, 0.0f, 16.0f, 464.0f, 600.0f);
	group2DParticle->getChildren().push_back(particleCamera);

	JDrama::TScreen* particleScreen = new JDrama::TScreen(rect, "Screen Grad");
	stageDisp->getUnk14()->getChildren().push_back(particleScreen);
	particleScreen->assignCamera(particleCamera);
	particleScreen->assignViewObj(group2DParticle);

	unk44->unkC.off(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
	unk48->unkC.on(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);

	return 0;
}

void TSelectDir::changeOrder()
{
	unk44->unkC.on(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
	unk48->unkC.off(CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW);
}

// The carriers the batch-128/cc32 ladders counterfeited with TU-local levels
// are accessor levels (research c-r29): the binder-shaped
// TApplication::getFader() at the fader sites and getSelectMenu() at six of
// the seven menu reads. The last two named words are a result local declared
// first (MenuDir's direct has the same `DEFAULT` result) and the fader status
// named for the switch (c-k24).
int TSelectDir::direct()
{
	int ret = TApplication::APP_STATE_DEFAULT;

	if (!unk38) {
		if (!OSIsThreadTerminated(&gSetupThread))
			return TApplication::APP_STATE_WAIT;

		void* res;
		OSJoinThread(&gSetupThread, &res);
		if (res != 0)
			return TApplication::APP_STATE_GAMEPLAY;

		unk38 = true;
		getSelectMenu()->initData(unk40, unk2C, unk28, this);
		getSelectMenu()->startMove();
		getSelectMenu()->startOpenWindow();

		SMSGetApplication()->getFader()->startWipe(0xe, 0.4f, 0.0f);
		SMSGetApplication()->getFader()->setColor(
		    unk40 == 9 ? JUtility::TColor(0xff, 0xff, 0xff, 0xff)
		               : JUtility::TColor(0, 0, 0, 0xff));
		gpMSound->initSound();
		return TApplication::APP_STATE_WAIT;
	}

	JDrama::TDirector::direct();

	int status = SMSGetApplication()->getFader()->mFadeStatus;
	switch (status) {
	case TSMSFader::FADE_STATUS_FULLY_FADED_IN:
	case TSMSFader::FADE_STATUS_FADING_IN:
		if (getSelectMenu()->unk14B)
			return TApplication::APP_STATE_DONE;

		if (unk20->mCloseMenu) {
			SMSGetApplication()->mNextArea.unk1 = getSelectMenu()->mSelectedShine;
			TFlagManager::smInstance->setFlag(0x40003, getSelectMenu()->mSelectedShine);
			SMSGetApplication()->getFader()->startWipe(0xf, 1.0f, 0.0f);
			SMSGetApplication()->getFader()->setColor(
			    JUtility::TColor(0xff, 0xff, 0xff, 0xff));
			SMSGetMSound()->fadeOutAllSound(SMSGetVSyncTimesPerSec());
		}
		break;
	}

	if (unk18->isSomethingPushed() && !unk4C) {
		unk4C = true;
		SMSGetApplication()->getFader()->startWipe(4, 1.0f, 0.0f);
		SMSGetMSound()->fadeOutAllSound(SMSGetVSyncTimesPerSec() * 0.4f);
		unk10->unkC.on(CUE_MOVE | CUE_CALC_ANIM);
	}

	if (SMSGetApplication()->getFader()->mFadeStatus
	    == TSMSFader::FADE_STATUS_FULLY_FADED_OUT) {
		gpMSound->stopAllSound();
		if (unk18->isSomethingPushed())
			return TApplication::APP_STATE_DONE;
		else
			return TApplication::APP_STATE_GAMEPLAY;
	}

	return ret;
}
