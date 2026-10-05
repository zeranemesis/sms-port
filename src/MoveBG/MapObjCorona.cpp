#include "MoveBG/MapObjCorona.hpp"


// rogue include: the original TU opens .rodata with the dummy string pair from
// System/DummyStrings.hpp plus the four MtxCalcType names; without them every
// string offset in this object is shifted.
#include <M3DUtil/InfectiousStrings.hpp>
#include "MoveBG/MapObjBase.hpp"
#include <math.h>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JMath.hpp>
#include <M3DUtil/MActor.hpp>
#include <Camera/CameraShake.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Mario.hpp>
#include <System/MarDirector.hpp>
#include <GC2D/GCConsole2.hpp>
#include <System/Particles.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Enemy/BathtubKiller.hpp>
#include <Enemy/Koopa.hpp>
#include <Player/WaterGun.hpp>
#include <Player/NozzleBase.hpp>
#include <Player/NozzleTrigger.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/JGeometry/JGMatrix33.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <stdio.h>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// The original calls JGeometry::TUtil<f32>::inv_sqrt(v) out-of-line here
// (bl sqrt__Q29JGeometry8TUtil<f>Ff), with the range guard inside the
// callee. JGUtil.hpp only offers the inline spelling, so MWCC always
// expands these sites and the call never appears.
// FABRICATED: the callee is orig_inv_sqrt, so the `bl` itself still shows as
// one mismatched instruction. Making JGUtil.hpp out-of-line instead was
// measured repo-wide at -32.2 points - see docs/AGENT_MATCHING_TIPS.md.
#pragma dont_inline on
static f32 orig_inv_sqrt(f32 v) {
	return JGeometry::TUtil<f32>::inv_sqrt(v);
}
#pragma dont_inline off


extern "C" u8 allowsLaunch__6TKoopaCFv(void*);
extern "C" void getDown__6TKoopaFv(void*);
extern "C" void stagger__6TKoopaFb(void*, bool);
extern "C" void fall__6TKoopaFv(void*);

// ---------------------------------------------------------------------------
// The whole file below is in the REVERSE of the address order the linker map
// records for MoveBG.a/MapObjCorona.cpp, because this TU is built with
// -inline deferred. tools/validate-symbol-order.py is the authority.
// ---------------------------------------------------------------------------

// ============================ GROUND TRUTH: the string table ================
// RESOLVED.  The ROM's .rodata for this TU is one contiguous block that starts
// at build/GMSP01/asm/MoveBG/MapObjCorona.s's @1490.  Until TBathtub::load,
// TBathtub::startDemo, TBathtub::allowsTumble and TBathtub::control were
// reconstructed, our object was missing everything between the four
// MtxCalcType names and the four .jpa names of loadAfter(), which made every
// string address this file computed 0x1C0 too small.  .rodata now matches the
// target byte for byte from +0x00 to +0x52F (the target's last 16 bytes are
// section padding).  Offsets relative to @1490, and the function that owns each
// string -- all of them are now referenced by code, none are dead:
//
//   +0x0f8 /scene/mapObj/bath_col_inside3.col   load(), switch case 0
//   +0x11c /scene/mapObj/bath_col_inside2.col   load(), case 1
//   +0x140 /scene/mapObj/bath_col_inside1.col   load(), case 2
//   +0x164 /scene/mapObj/bath_col_inside6.col   load(), case 3
//   +0x188 /scene/mapObj/bath_col_inside5.col   load(), case 4
//   +0x1ac /scene/mapObj/bath_col_inside4.col   load(), case 5
//   +0x1d0 scene/map/map/stand_effect           load()  (NO leading slash)
//   +0x1ec "壊れかけのバスタブの取っ手"            load()  (TBathtubGrip name)
//   +0x208 "submarin"                            load()
//   +0x214 /scene/map/map/shine                 load()
//   +0x22c /scene/map/map/shine/shine_3bai.bmd  load()
//   +0x250 "バスタブキラーマネージャー"            allowsTumble(), the killer
//                                                 manager looked up by name
//   +0x26c "stand_effect"                       startDemo(), TBathtubGrip::
//                                                 receiveMessage()
//   +0x27c "bath_overturn1"                     startDemo(), control()
//   +0x28c "koopa_last2"                        startDemo() (demo camera)
//   +0x298 "bath_overturn2"                     control()
//   +0x2a8 "bath_overturn3"                     control()
//   +0x2b8 .. +0x31c  the four loadAfter() .jpa names
//   +0x340 .. +0x3c4  TBathtubGrip's own names
//   +0x3e4 ..         /MapObj/bathtub.prm + param names
//
// Two traps worth recording:
//
//  * Strings at +0x250 and +0x298/+0x2A8 look unreferenced if you only scan for
//    `addi rX, r31, <off>`: they are addressed with a 32-bit
//    `lis r3, "@NNNN"@ha` + `addi r31, r3, "@NNNN"@l` pair because the offset
//    does not fit the 16-bit addi.  Grep for the symbol name, not the offset.
//  * The @NNNN literal numbering interleaves .rodata strings and .sdata2
//    constants in order of first use, and counts up in map order (= reverse
//    source order for this -inline deferred TU).  It is the cheapest way to
//    assign a string to an owning function: dump the .sdata2/.rodata object
//    table, find the neighbouring @NNNN numbers, and grep which function uses
//    them.  That is how +0x250 was pinned to allowsTumble() (@4385-@4388 are its
//    four float constants) and +0x298/+0x2A8 to control() (@4999-@5002 follow).
// ===========================================================================

// Not in the map: the TU-local helper both getNumKiller{Burstable,Launchable}
// use to ask the resident Koopa whether he may be launched. Dead in the target
// build, so it is dead-stripped and has no entry there either.
static bool bathtubKoopaAllowsLaunch()
{
	return allowsLaunch__6TKoopaCFv(JDrama::TNameRefGen::search("クッパ")) != 0;
}

// TODO (getNumKillerBurstable + getNumKillerLaunchable): the target tests
// unk248 in a *value* context -- an inlined `bool f(int) { return v >= 0; }`
// materialised as `li r0,0 / srawi r3,r0,31 / srwi r4,r5,31 / subfc r0,r5,r0
// / adde r0,r3,r4` and then merged into one `clrlwi. r0,r0,24 / bne`.  Writing
// the same comparison here (inline, through a helper, negated, or as a `&&`
// chain) always folds back to `cmpwi / bne|blt`, so the original form is still
// unknown.  The five `getNumGripsDead()` accesses after it are unrolled in
// both builds, and getNumKillerBurstable's switch also needs its `case 3` and
// `case 4` spelled out separately (MWCC picks 3 as the pivot for {1,2,3,4} but
// 2 for a merged `case 3: case 4:`).

// ---------------------------------------------------------------------------
// TBathtubParams (map position 67)
// ---------------------------------------------------------------------------

TBathtubParams::TBathtubParams()
    : TParams("/MapObj/bathtub.prm")
    , PARAM_INIT(resetGrip, 0)
    , PARAM_INIT(trampleRelease, 10)
    , PARAM_INIT(trampleRecover, 10)
    , PARAM_INIT(quakeRelease, 500)
    , PARAM_INIT(quakeRecover, 500)
    , PARAM_INIT(hipdropRelease, 35)
    , PARAM_INIT(hipdropRecover, 35)
    , PARAM_INIT(breakCount0, 750)
    , PARAM_INIT(breakCount1, 710)
    , PARAM_INIT(breakCount2, 685)
    , PARAM_INIT(breakCount3, 655)
    , PARAM_INIT(launchStopCount, 1000)
    , PARAM_INIT(animSpeed0, 0.15f)
    , PARAM_INIT(animSpeed1, 0.15f)
    , PARAM_INIT(animSpeed2, 0.15f)
    , PARAM_INIT(animSpeed3, 0.15f)
    , PARAM_INIT(animSpeed4, 0.22f)
    , PARAM_INIT(shake, 0.0f)
    , PARAM_INIT(watermark, 0.3f)
    , PARAM_INIT(maxAngle, 35.0f)
    , PARAM_INIT(angleVelDamp, 0.93f)
    , PARAM_INIT(rebound, 0.0005f)
    , PARAM_INIT(shakeDamp, 0.93f)
    , PARAM_INIT(marioWeight, 0.01f)
    , PARAM_INIT(marioDropWeight, 5.0f)
    , PARAM_INIT(outerHeight, 20.0f)
{
	TParams::load(mPrmPath);
}

// ---------------------------------------------------------------------------
// TBathtubGripParts (map positions 63-66)
// ---------------------------------------------------------------------------

// Unused
TBathtubGripParts::TBathtubGripParts(const char* name, int index,
                                     TBathtubGrip* grip)
    : TLiveActor(name)
    , mGrip(grip)
    , unkF8(index)
{
}

// Unused
TBathtubGripPartsFragile::TBathtubGripPartsFragile(int index,
                                                   TBathtubGrip* grip)
    : TBathtubGripParts("バスタブの足場の一部（弱点）", index, grip)
{
}

// Unused
TBathtubGripPartsHard::TBathtubGripPartsHard(int index, TBathtubGrip* grip)
    : TBathtubGripParts("バスタブの足場の一部（壊れない）", index, grip)
{
}

Mtx* TBathtubGripParts::getRootJointMtx() const
{
	// getAnmMtx() is `mNodeMatrices[idx]`, i.e. the same address the target
	// computes; only the return type differs (Mtx* vs MtxPtr). Note the model
	// belongs to the *grip*, not to this part actor: the target leaves mGrip in
	// r3 across the getModel() call.
	s32 index = mGrip->unk200[unkF8];
	
	
	return reinterpret_cast<Mtx*>(mGrip->getModel()->getAnmMtx(index));
}

// The sender is forwarded unchanged: the target never reloads r4, so the
// argument is this part actor itself rather than a null pointer.
BOOL TBathtubGripPartsFragile::receiveMessage(THitActor* sender, u32 message)
{
	return mGrip->receiveMessage(sender, message);
}

BOOL TBathtubGripPartsHard::receiveMessage(THitActor* sender, u32 message)
{
	if (message == 3)
		message = 1;
	return mGrip->receiveMessage(sender, message);
}

// ---------------------------------------------------------------------------
// TBathtubGrip (map positions 46-60)
// ---------------------------------------------------------------------------

void TBathtubGrip::kill()
{
	unk24A = 1;
	makeObjDead();

	for (s32 i = 0; i < 17; i++)
		unk164[i]->remove();
	for (s32 i = 0; i < 5; i++)
		unk150[i]->remove();
}

// Unused
void TBathtubGrip::reset() { }

// Between reset() and the ctor in source order: the object (which is the
// reverse of this file) needs it at map position 58, i.e. after the
// TBathtubGrip ctor and before reset(). The body is empty -- the 0x6C bytes
// the map gives it are the standard MWCC virtual-destructor prologue (__vt__
// store, the +0x20 adjusting entry, TLiveActor::~TLiveActor and the
// extsh./ble-guarded `operator delete`) plus the separate 8-byte @32@ thunk.
TBathtubGripParts::~TBathtubGripParts() { }

TBathtubGrip::TBathtubGrip(TBathtub* bathtub, f32 angle,
                           MActorAnmData* anmData, const char* name)
    : TMapObjBase(name)
{
	mStandMActor = new MActor(anmData);
	// The target evaluates the resource lookup *before* the `new J3DModel`
	// allocation (an argument produced by a call beats a plain size argument),
	// so the glb pointer has to be hoisted into a named local.
	void* glb = JKRFileLoader::getGlbResource(
	    "/scene/map/map/stand_effect/stand_effect.bmd");
	// The target keeps the 0x50050000 model id in a callee-saved register
	// across the three calls (lis r23,0x5005 / addi r4,r23,0 / ... / addi r5,
	// r23,0), so it has to be a plain named local -- `const` makes MWCC treat
	// it as a foldable literal and re-materialise it per use.
	u32 modelId = 0x50050000;	mStandMActor->setModel(
	    new J3DModel(J3DModelLoaderDataBase::load(glb, modelId), 0, 1), modelId);

	unk254  = 0;
	mBathtub = bathtub;
	unk24C  = angle;

	initAndRegister("stand_break");
	calcRootMatrix();
	getModel()->calc();

	JUTNameTab* nameTab = getModel()->getModelData()->getJointName();

	// 0x10, not 0x48: colName[0x100] at 0x38 + partName at 0x138 leaves the
	// saved-register area starting at 0x148, which is what makes the target's
	// frame exactly 0x180.
	char partName[0x10];
	char colName[0x100];

	for (s32 i = 0; i < 17; i++) {
		sprintf(partName, "c%d", i + 1);
		sprintf(colName, "/scene/mapObj/stand_break_%s.col", partName);

		unk200[i] = nameTab->getIndex(partName);
		unk164[i] = new TMapCollisionMove();
		unk1BC[i] = new TBathtubGripPartsHard(i, this);
		unk164[i]->init(colName, 0, unk1BC[i]);

		if (i < 5) {
			sprintf(partName, "b%d", i + 1);
			sprintf(colName, "/scene/mapObj/stand_break_%s.col", partName);

			unk150[i] = new TMapCollisionMove();
			unk1A8[i] = new TBathtubGripPartsFragile(i, this);
			unk150[i]->init(colName, 0, unk1A8[i]);
		}
	}

	offLiveFlag(LIVE_FLAG_DEAD);
	unk248 = 0;
	unk24A = 0;
	unk249 = 1;
	unk24B = 0;

	startAnim(0);

	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(0);
	if (ctrl != nullptr) {
		ctrl->setFrame(0.0f);
		ctrl->setRate(0.0f);
	}

	unk250 = 1.0f;
	unk258 = 100;
	unk260 = 0;
}

void TBathtubGrip::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TMapObjBase::perform(cue, graphics);

	if (unk260 != 0)
		return;

	if (cue & 1) {
		PSMTXCopy((MtxPtr)getRootJointMtx(),
		          mStandMActor->getModel()->getBaseTRMtx());
		if (unk254 > 0 || unk248 != 0) {
			if (mStandMActor->curAnmEndsNext(0, nullptr))
				unk260 = 1;
		}
	}

	mStandMActor->perform(cue, graphics);
}

// Unused
bool TBathtubGrip::isCracking() const
{
	return false;
}

// Unused
void TBathtubGrip::startCrack() { }

// Unused
void TBathtubGrip::startBreak(f32, int, f32) { }

// How many of the five grips are already dead -- that count drives both the
// demo trigger and which of the five shake speeds the survivor plays.
static s32 countLiveGrips(TBathtub* bath)
{
	s32 count = 0;
	for (s32 i = 0; i < 5; i++) {
		if (bath->unk168[i]->unk249 == 0)
			count++;
	}
	return count;
}

// Messages 2 and anything >= 4 are ignored. Message 3 is the "the tub is being
// pushed over" tick; 0 relays to the bathtub's own trample handling and 1 asks
// for a hipdrop. The sender is forwarded unchanged.
BOOL TBathtubGrip::receiveMessage(THitActor* sender, u32 message)
{
	// The target's dispatch chain is `cmpwi 2 / beq false`, then `bge` into the
	// message-3 test guarded by `cmpwi 4 / bge false`; 0 and 1 are reached by
	// falling through.
	if (message == 2 || message >= 4)
		return false;

	if (message == 3) {
		// Nothing happens once the grip is already falling or held.
		if (unk248 != 0)
			return false;
		if (unk249 == 0)
			return false;

		// The grab sound is anchored on Mario himself. Component by component: the
		// target interleaves lfs/stfs, which a three-argument .set() or a word
		// copy does not (the same rule load() documents).
		unk138[0].x = gpMarioPos->x;
		unk138[0].y = gpMarioPos->y;
		unk138[0].z = gpMarioPos->z;
		if (gpMSound->gateCheck(0x3821))
			MSoundSESystem::MSoundSE::startSoundActor(
			    0x3821, reinterpret_cast<Vec*>(&unk138[0]), 0, nullptr, 0, 4);

		// Four dead grips means Mario has knocked them all off: the demo.
		s32 dead = countLiveGrips(mBathtub);
		if (static_cast<u16>(dead) == 4) {
			mBathtub->startDemo();
			return true;
		}

		mBathtub->quake(sender->mPosition);

		// The shake speed and duration come from the param table, indexed by
		// how many grips are still standing.
		// Each arm re-fetches mBathtub->unk16C off `this` rather than reusing a named
		// pointer, which is what the target's per-case `lwz 0x244 / lwz 0x16c`
		// pairs are: a named local collapses them to one load.
		switch (dead) {
		case 1:
			unk258 = mBathtub->unk16C->breakCount0.get();
			unk250 = mBathtub->unk16C->animSpeed1.get();
			break;
		case 2:
			unk258 = mBathtub->unk16C->breakCount1.get();
			unk250 = mBathtub->unk16C->animSpeed2.get();
			break;
		case 3:
			unk258 = mBathtub->unk16C->breakCount2.get();
			unk250 = mBathtub->unk16C->animSpeed3.get();
			break;
		case 4:
			unk258 = mBathtub->unk16C->breakCount3.get();
			unk250 = mBathtub->unk16C->animSpeed4.get();
			break;
		default:
			unk258 = mBathtub->unk16C->breakCount0.get();
			unk250 = mBathtub->unk16C->animSpeed0.get();
			break;
		}
		unk254 = 1;

		mStandMActor->setBck("stand_effect");
		mStandMActor->setBtk("stand_effect");
		mStandMActor->setBrk("stand_effect");

		J3DFrameCtrl* ctrl = mStandMActor->getFrameCtrl(0);
		if (ctrl != nullptr)
			ctrl->setRate(0.5f * (12.0f * SMSGetAnmFrameRate()));

		unk248 = 1;
		// The surviving grip replays the fall animation from the start.
		s32 anim;
		if (dead == 0)
			anim = 1;
		else if (dead == 1)
			anim = 0;
		else
			anim = dead;
		startAnim(static_cast<u16>(anim));
		return true;
	}

	if (message == 0) {
		// The trample timers live on the bathtub, so this is a straight relay
		// of TBathtub::receiveMessage's case 0.
		TBathtub* bath = mBathtub;
		if (reinterpret_cast<const u8*>(bath)[0x29A] == 0) {
			const s32* timer = &bath->unk16C->trampleRelease.get();
			if (bath->unk250 <= bath->unk16C->trampleRelease.get()) {
				bath->unk250 = *timer;
				bath->unk258 = *reinterpret_cast<int*>(
				    reinterpret_cast<u8*>(bath->unk16C) + 0x40);
				bath->unk25C = *reinterpret_cast<int*>(
				    reinterpret_cast<u8*>(bath->unk16C) + 0x40);
				bath->unk254 = *reinterpret_cast<u32*>(
				    reinterpret_cast<u8*>(bath->unk16C) + 0x7C);
			}
		}
		return true;
	}

	mBathtub->hipdrop(sender->mPosition);
	return true;
}

Mtx* TBathtubGrip::getRootJointMtx() const
{
	return reinterpret_cast<Mtx*>(getModel()->getBaseTRMtx());
}

// The grip's own model matrix is rebuilt by baking the grip's stored angle
// (unk24C) into a rotation matrix and then multiplying the bathtub's root
// joint matrix by it.
void TBathtubGrip::calcRootMatrix()
{
	// The destination matrix is fetched before anything else: the target
	// keeps model+0x20 in r31 across the whole body.
	MtxPtr model = getModel()->getBaseTRMtx();

	Mtx mtx;
	MsMtxSetRotRPH(mtx, 0.0f, unk24C, 0.0f);
	mtx[0][3] = 0.0f;
	mtx[1][3] = 0.0f;
	mtx[2][3] = 0.0f;

	MtxPtr bathp = reinterpret_cast<MtxPtr>(mBathtub->getRootJointMtx());

	reinterpret_cast<JGeometry::SMatrix34C<f32>*>(model)->set(
	    // clang-format off
	    bathp[0][0] * mtx[0][0] + bathp[0][1] * mtx[1][0] + bathp[0][2] * mtx[2][0],
	    bathp[0][0] * mtx[0][1] + bathp[0][1] * mtx[1][1] + bathp[0][2] * mtx[2][1],
	    bathp[0][0] * mtx[0][2] + bathp[0][1] * mtx[1][2] + bathp[0][2] * mtx[2][2],
	    bathp[0][0] * mtx[0][3] + bathp[0][1] * mtx[1][3] + bathp[0][2] * mtx[2][3] + bathp[0][3],
	    bathp[1][0] * mtx[0][0] + bathp[1][1] * mtx[1][0] + bathp[1][2] * mtx[2][0],
	    bathp[1][0] * mtx[0][1] + bathp[1][1] * mtx[1][1] + bathp[1][2] * mtx[2][1],
	    bathp[1][0] * mtx[0][2] + bathp[1][1] * mtx[1][2] + bathp[1][2] * mtx[2][2],
	    bathp[1][0] * mtx[0][3] + bathp[1][1] * mtx[1][3] + bathp[1][2] * mtx[2][3] + bathp[1][3],
	    bathp[2][0] * mtx[0][0] + bathp[2][1] * mtx[1][0] + bathp[2][2] * mtx[2][0],
	    bathp[2][0] * mtx[0][1] + bathp[2][1] * mtx[1][1] + bathp[2][2] * mtx[2][1],
	    bathp[2][0] * mtx[0][2] + bathp[2][1] * mtx[1][2] + bathp[2][2] * mtx[2][2],
	    bathp[2][0] * mtx[0][3] + bathp[2][1] * mtx[1][3] + bathp[2][2] * mtx[2][3] + bathp[2][3]
	    // clang-format on
	);
}

// Unused
bool TBathtubGrip::marioIsOn() const
{
	return false;
}

// Unused
void TBathtubGrip::setupCollisions_() { }

void TBathtubGrip::control()
{
	// TODO: the target's frame is 0x50 against this one's (0x28 before the
	// pad), and its moveMtx/setUp loops keep the induction variable in r29
	// where MWCC picks r28 here. Nothing stack-resident explains the missing
	// 0x28 bytes, so the pad only pins the frame; the register split is not
	// reproducible from the source shape tried so far.
	
	

	// The target reaches calcRootMatrix() through the vtable (0xC0) and only
	// then makes the *direct* call to TMapObjBase::control().
	this->calcRootMatrix();
	TMapObjBase::control();

	if (unk24A != 0) {
		// Same unrolled `for` shape as kill(): the index is kept in r30 as a
		// byte offset so the array access is an lwzx off this.
		for (s32 i = 0; i < 17; i++)
			unk164[i]->remove();
		for (s32 i = 0; i < 5; i++)
			unk150[i]->remove();
		return;
	}

	mMActor->calcAnm();

	if (unk24B != 0) {
		for (s32 i = 0; i < 17; i++) {
			unk164[i]->moveMtx(
			    reinterpret_cast<MtxPtr>(unk1BC[i]->getRootJointMtx()));
			unk164[i]->setUp();
		}
		for (s32 i = 0; i < 5; i++) {
			unk150[i]->moveMtx(
			    reinterpret_cast<MtxPtr>(unk1A8[i]->getRootJointMtx()));
			unk150[i]->setUp();
		}
	}

	if (unk248 != 0) {
		if (animIsFinished()) {
			// The resetGrip tunable decides between re-arming the grip and
			// killing it outright.
			if (mBathtub->unk16C->resetGrip.get()) {
				offLiveFlag(LIVE_FLAG_DEAD);
				unk248 = 0;
				unk24A = 0;
				unk249 = 1;
				unk24B = 0;
				startAnim(0);
				J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(0);
				if (ctrl != nullptr) {
					ctrl->setFrame(0.0f);
					ctrl->setRate(0.0f);
				}
				unk250 = 1.0f;
				unk258 = 100;
				unk260 = 0;
			} else {
				kill();
			}
		} else {
			J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(0);
			if (ctrl != nullptr)
				ctrl->setRate(0.5f * (unk250 * SMSGetAnmFrameRate()));

			// The sound and the rumble are both anchored on the fragile part's
			// model matrix; the translation is read out into unk138[1] first.
			MtxPtr mtx = reinterpret_cast<MtxPtr>(
			    unk1A8[0]->getRootJointMtx());
			unk138[1].set(mtx[0][3], mtx[1][3], mtx[2][3]);

			if (gpMSound->gateCheck(0x300D))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x300D, reinterpret_cast<Vec*>(&unk138[1]), 0, nullptr, 0,
				    4);
			SMSRumbleMgr->start(8, reinterpret_cast<Vec*>(&unk138[0]));
		}
	} else {
		J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(0);
		ctrl->setRate(0.0f);
		if (unk254 > 0) {
			ctrl->setFrame(1.0f);
			// The pre-increment form: the target keeps the incremented value in
			// a register for the compare instead of re-reading the member.
			if (++unk254 > unk258) {
				unk248 = 1;
				unk254 = 0;
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0xF6, reinterpret_cast<MtxPtr>(getRootJointMtx()), 0,
				    this);
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0xF7, reinterpret_cast<MtxPtr>(getRootJointMtx()), 0,
				    this);
			}
		} else {
			ctrl->setFrame(0.0f);
		}
	}
}

// ---------------------------------------------------------------------------
// TBathtub (map positions 1-45)
// ---------------------------------------------------------------------------

void TBathtub::loadAfter()
{
	SMS_LoadParticle("/scene/map/map/ms_lkp_yuge1.jpa", 0x1BE);
	SMS_LoadParticle("/scene/map/map/ms_kp_funsui.jpa", 0x1BF);
	SMS_LoadParticle("/scene/map/map/ms_kp_break_a.jpa", 0x0F6);
	SMS_LoadParticle("/scene/map/map/ms_kp_break_b.jpa", 0x0F7);
}

// dont_inline: the target calls hipdrop out of line from receiveMessage, and
// with the named-member form below MWCC's inliner would otherwise expand it
// into that caller (which regresses receiveMessage from 88.8% to 0%).
#pragma dont_inline on
// The three TBathtubParams members the map records at +0x7c/+0x90/+0x7c are
// hipdropRelease / hipdropRecover / hipdropRelease (TParamT<T>::value sits at
// +0x10 inside the TParamRT, and the class starts at 0x08). Naming them turns
// the previous raw byte offsets back into real member accesses.
void TBathtub::hipdrop(const JGeometry::TVec3<f32>& position)
{
	if (unk29A != 0)
		return;

	if (unk250 > unk16C->hipdropRelease.get())
		return;

	// TODO: the target reads the bathtub's own centre at 0x10c / 0x114, which is
	// neither mPosition (0x10..0x18) nor anything named in the header. The
	// seed 0.0f is written on the left of the sum so MWCC keeps the ROM's
	// `0.0f + dx*dx + dz*dz` shape (see quake).
	f32 dx = position.x - *reinterpret_cast<f32*>(reinterpret_cast<u8*>(this) + 0x10C);
	f32 dz = position.z - *reinterpret_cast<f32*>(reinterpret_cast<u8*>(this) + 0x114);
	f32 distance = 0.0f + dx * dx + dz * dz;
	if (distance > 0.0f)
		orig_inv_sqrt(distance);

	// TODO: 94.9%, and the residue is the same register-allocation /
	// orig_inv_sqrt-relocation noise quake documents, not missing logic. The
	// 48-byte pad only pins the 0x98 frame.
	
	

	unk250 = unk16C->hipdropRelease.get();
	unk258 = unk16C->hipdropRecover.get();
	unk25C = unk16C->hipdropRecover.get();
	unk254 = unk16C->hipdropRelease.get();
	stagger__6TKoopaFb(JDrama::TNameRefGen::search("クッパ"), false);
}
#pragma dont_inline off

void TBathtub::quake(const JGeometry::TVec3<f32>& position)
{
	// TODO: 91.1%.  What is left, all of it register-allocation or relocation
	// noise rather than missing logic: (1) the ROM loads position.x and
	// this->0x10C *before* position.z and this->0x114 and keeps the seeded
	// 0.0f in f1 as an fmadds addend (`fmadds f1, f3, f3, f1`); this build
	// loads them the other way round and folds the seed into a plain fmuls.
	// Neither the three-statement `f32 d = 0.0f; d += ..` form nor swapping
	// the dx/dz declarations moves MWCC off that, so the original spelling is
	// still unknown.  (2) `orig_inv_sqrt` is an MWCC-emitted local copy of the
	// inline TUtil<f32>::inv_sqrt; the ROM calls the real out-of-line symbol.
	// Fixing either needs a libs/ change, which is off limits here.
	if (unk29A != 0)
		return;

	f32 dx = position.x - *reinterpret_cast<f32*>(reinterpret_cast<u8*>(this) + 0x10C);
	f32 dz = position.z - *reinterpret_cast<f32*>(reinterpret_cast<u8*>(this) + 0x114);
	// The seed 0.0f is written on the left of the sum so MWCC keeps the ROM's
	// `0.0f + dx*dx + dz*dz` shape; the three-statement `f32 d = 0.0f; d += ..`
	// form folds the seed away and costs a whole instruction.
	f32 distance = 0.0f + dx * dx + dz * dz;
	if (distance > 0.0000038146973f) {
		orig_inv_sqrt(distance);
	}

	unk24C = 300;
	u8* params = reinterpret_cast<u8*>(unk16C);
	unk250 = *reinterpret_cast<int*>(params + 0x54);
	unk258 = *reinterpret_cast<int*>(params + 0x68);
	unk25C = *reinterpret_cast<int*>(params + 0x68);
	unk254 = *reinterpret_cast<u32*>(params + 0x7C);
	unk248 = *reinterpret_cast<int*>(params + 0xF4);

	// The Koopa's TName is looked up *before* the two shakes, and the target
	// keeps the result in a callee-saved register all the way down to getDown().
	void* koopa = JDrama::TNameRefGen::search("クッパ");
	gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x25), 1.0f);
	gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x26), 1.0f);
	SMSRumbleMgr->start(4, static_cast<f32*>(nullptr));
	// The 20-byte pad below the throw vector is what pushes it to the ROM's
	// 0x74 slot; the 76-byte one above it makes up the rest of the 0xA0 frame.
	// See the TODO at the top for what is still missing.
	
	
	JGeometry::TVec3<f32> velocity(0.0f, 1.0f, 0.0f);
	SMS_ThrowMario(velocity, 10.0f);
	getDown__6TKoopaFv(koopa);
	
	
}

// The unrolled `for` form (not five spelled-out `if`s) is what reproduces the
// target: it keeps the counter in r5 and re-reads unk168 through r3 each step.
int TBathtub::getNumGripsDead() const
{
	int count = 0;
	for (int i = 0; i < 5; i++) {
		if (unk168[i]->unk249 == 0)
			count++;
	}
	return count;
}

// Unused
void TBathtub::trample(const JGeometry::TVec3<f32>&) { }

// Unused
void TBathtub::liftMario(const JGeometry::TVec3<f32>&) { }

// Both the cos and the -sin product are held in named locals: that is what
// stops MWCC from fusing each `+=` into an fmadds. The no-op write to unk1EC
// has to go through a volatile pointer to survive (and the target really does
// emit the lfs/fadds/stfs triple for it).
void TBathtub::tumble(f32 angle, f32 force)
{
	if (unk29A == 0) {
		f32 amount = 0.0001f;
		amount = force * amount;
		s32 index = static_cast<u16>(static_cast<s32>(182.04445f * angle)) >> jmaSinShift;
		f32 c = amount * jmaCosTable[index];
		f32 s = amount * -jmaSinTable[index];
		unk1E8 += c;
		*reinterpret_cast<volatile f32*>(&unk1EC) += JGeometry::TUtil<f32>::epsilon();
		unk1F0 += s;
		
		
	}
}

MtxPtr TBathtub::getTakingMtx()
{
	return mMActor->getModel()->getAnmMtx(mMarioJntIdx);
}

// Unused
MtxPtr TBathtub::getShineMtx() { return nullptr; }

// Unused
MtxPtr TBathtub::getShineEffectMtx() { return nullptr; }

// Unused
MtxPtr TBathtub::getWaterMtx(int) { return nullptr; }

MtxPtr TBathtub::getSubmarineMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mSubmarineJntIdx);
}

MtxPtr TBathtub::getPeachMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mDuckJntIdx);
}

// Unused
MtxPtr TBathtub::getKoopaMtxInDemo() { return nullptr; }

MtxPtr TBathtub::getKoopaJrMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mJuniorJntIdx);
}

BOOL TBathtub::receiveMessage(THitActor*, u32 message)
{
	switch (message) {
	case 1:
		hipdrop(*gpMarioPos);
		return true;
	case 3:
		hipdrop(*gpMarioPos);
		return true;
	case 0: {
		if (reinterpret_cast<const u8*>(this)[0x29A] == 0) {
			// The target materialises the address of the trampleRelease value
			// slot into r5 and re-reads through it for the store, while the
			// comparison reads the slot straight off unk16C.
			const s32* timer = &unk16C->trampleRelease.get();
			if (unk250 <= unk16C->trampleRelease.get()) {
				unk250 = *timer;
				unk258 = *reinterpret_cast<int*>(
				    reinterpret_cast<u8*>(unk16C) + 0x40);
				unk25C = *reinterpret_cast<int*>(
				    reinterpret_cast<u8*>(unk16C) + 0x40);
				unk254 = *reinterpret_cast<u32*>(
				    reinterpret_cast<u8*>(unk16C) + 0x7C);
			}
		}
		return true;
	}
	case 2:
	default:
		return false;
	}
}

Mtx* TBathtub::getRootJointMtx() const
{
    if (reinterpret_cast<const u8*>(this)[0x29A] != 0)
        return *reinterpret_cast<Mtx**>(reinterpret_cast<u8*>(getModel()) + 0x58);
    return reinterpret_cast<Mtx*>(reinterpret_cast<u8*>(getModel()) + 0x20);
}

void TBathtub::perform(u32 cue, JDrama::TGraphics* graphics) { }

// TODO: only the opening block is reconstructed.  The rest of the ROM's 0x4AC
// bytes is the emitter/sound/tipping-spring tail, and its shape could not be
// pinned down without inventing names: the two shine-body emitters use particle
// ids that no enum in include/System/Particles.hpp names, and the six
// `stfs 3.0f` the target writes into each returned emitter (at +0x154/+0x158/
// +0x15C and +0x174/+0x178/+0x17C) have no matching inline setter in
// libs/JSystem/JParticle/JPAEmitter.hpp -- only setGlobalScale(TVec3), which
// takes a vector.  It is here because the two animation names it uses are what
// puts .rodata+0x298 and +0x2A8 in place, which in turn is what un-shifts every
// string after them.
//
// Decoded shape, for whoever picks this up:
//   if (unk29A) { curAnmEndsNext -> switch (unk294++) { 0: bath_overturn2,
//     1: bath_overturn3 }; emit(0x128) on the shine body joint (unk29C) and
//     emit(0x129) on mShineBodyJntIdx, six 3.0f stores into each emitter;
//     unk200 = the water4 joint matrix translation + sound 0x81C1 at it;
//     calcRootMatrix(); calcBathtubData(); remove all 30 pieces;
//     if (mario->receiveMessage(this,4)) mHeldObject = mario;
//     gpMarioOriginal->mFaceAngle.y = 0x7FFF; if (unk290 > 0) unk290--; }
//   else { if (unk24C > 0) unk24C--; if (unk248 > 0) unk248--;
//     if (unk250 > params+0x7C && marioIsOn() && !mHeldObject) { ...tipping
//     spring driven by yDown and unk1E8/1EC/1F0... }
//     updatePosture_(); calcRootMatrix(); TMapObjBase::control();
//     calcBathtubData(); getModel()->calc();
//     if (a 3-term dot product over mBathtubData > 0.995f) emit(0x1BE, &unk1F4);
//     for (i<5) if (unk168[i]->unk248) emitAndBindToMtxPtr(0x1BF, mKoopaJnt
//     matrix, 1, this);
//     if (!mHeldObject) setupCollisions_(); else remove all 30 pieces; }
void TBathtub::control()
{
	if (unk29A != 0) {
		if (mMActor->curAnmEndsNext(0, nullptr)) {
			// The switch tests the pre-increment value: the target compares the
			// loaded unk294 and stores unk294+1.
			switch (unk294++) {
			case 0:
				startBck("bath_overturn2");
				break;
			case 1:
				startBck("bath_overturn3");
				break;
			}
		}
	}
}

// ---------------------------------------------------------------------------
// The bathtub's local "down" axis. The ROM keeps it in .bss (so it is *not*
// constant-initialised) and gives every caller its own one-byte lazy-init
// guard in .sbss -- calcBathtubData() and updatePosture_() each carry their own
// -- which is why the initialisations below are spelled out by hand instead of
// being left to MWCC's static-init machinery.
// ---------------------------------------------------------------------------
static JGeometry::TVec3<f32> yDown;
// calcBathtubData()'s half of the lazy init (updatePosture_ has its own). It is
// a signed byte because the ROM tests it with `extsb.` rather than `cmplwi`.
static s8 bathtubDownInited = 0;
// updatePosture_()'s half of the lazy init. Same signed-byte reason.
static s8 postureInited = 0;

// Rebuilds the tub's cached data from its own root-joint matrix: the basis
// vectors become mBathtubData::unk18, the pivot becomes mPos, and the model's
// up-axis (unk0C) is derived from the matrix. Then, unless the Koopa's tumble
// effect is running, the tub is levelled by rotating unk0C back onto +Y.
void TBathtub::calcBathtubData()
{
	// The target calls getRootJointMtx() once and reads the 3x3 block straight
	// out of the returned matrix pointer.
	MtxPtr mtx = reinterpret_cast<MtxPtr>(getRootJointMtx());

	// The 3x3 block is stored transposed: ref(0,j) = mtx[j][0]. Our unk18 is
	// still TMtx33f = TMatrix33<SMatrix33C<f32>> (see the TBathtub ctor note),
	// so the transposed ref() indices below spell the R convention directly.
	// clang-format off
	mBathtubData.unk18.ref(0, 0) = mtx[0][0];
	mBathtubData.unk18.ref(0, 1) = mtx[1][0];
	mBathtubData.unk18.ref(0, 2) = mtx[2][0];

	mBathtubData.unk18.ref(1, 0) = mtx[0][1];
	mBathtubData.unk18.ref(1, 1) = mtx[1][1];
	mBathtubData.unk18.ref(1, 2) = mtx[2][1];

	mBathtubData.unk18.ref(2, 0) = mtx[0][2];
	mBathtubData.unk18.ref(2, 1) = mtx[1][2];
	mBathtubData.unk18.ref(2, 2) = mtx[2][2];
	// clang-format on

	mBathtubData.mPos.x = mtx[0][3];
	mBathtubData.mPos.y = mtx[1][3];
	mBathtubData.mPos.z = mtx[2][3];

	// The "how full is it" height: clamped by the watermark param.
	f32 up = mBathtubData.unk18.ref(1, 1);
	f32 rise = 1.0f - up * up;
	if (rise > 0.0f)
		rise = 1.0f * JGeometry::TUtil<f32>::inv_sqrt(rise);
	// max(): the target keeps the watermark unless it is already below `rise`.
	f32 clamped = unk16C->watermark.get();
	if (!(clamped > rise))
		clamped = rise;

	mBathtubData.unk44 = mBathtubData.unk3C * clamped;
	mBathtubData.unk48 = unk16C->outerHeight.get();

	// unk0C is the middle column of unk18: the tub's own up-axis.
	mBathtubData.unk0C.x = mBathtubData.unk18.ref(1, 0);
	mBathtubData.unk0C.y = mBathtubData.unk18.ref(1, 1);
	mBathtubData.unk0C.z = mBathtubData.unk18.ref(1, 2);
	mBathtubData.unk58.zero();

	// The tumble effect locks the up-axis, so the levelling below is skipped
	// while it runs (and while the hipdrop hold timer is still counting).
	// TODO: the target looks the name up twice -- once through
	// calcKeyCode__Q26JDrama8TNameRefFPCc and then through the name table's
	// searchF(u32, const char*) at vtable+0x1C -- and our
	// JDrama::TNameRefGen only offers search(const char*), so the lookup is
	// spelled with the one-argument form here. The name (the .sdata2 string
	// "@3407", 0x834E8362 0x8370 = "クッパ") is confirmed; the double lookup
	// and its discarded result are not reproducible without a searchF overload.
	if (!reinterpret_cast<TKoopa*>(JDrama::TNameRefGen::search("クッパ"))
	         ->effectsTumble()
	    && unk24C <= 0) {
		mBathtubData.unk0C.x = 0.0f;
		mBathtubData.unk0C.y = 1.0f;
		mBathtubData.unk0C.z = 0.0f;
	} else {
		JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
		JGeometry::TVec3<f32> axis;
		axis.cross(up, mBathtubData.unk0C);

		f32 lenSq = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
		if (!(lenSq <= JGeometry::TUtil<f32>::epsilon())) {
			axis.scale(1.0f * orig_inv_sqrt(lenSq), axis);
			// A named local: the ROM keeps the product and the division as
			// separate instructions instead of fusing them.
			f32 angle = unk16C->maxAngle.get() * 6.2831855f / 360.0f;
			f32 half = 0.5f * angle;
			JGeometry::TVec3<f32> qv(axis.x * sinf(half),
			                        axis.y * sinf(half),
			                        axis.z * sinf(half));
			JGeometry::TQuat4<f32> q(qv.x, qv.y, qv.z, cosf(half));
			q.rotate(mBathtubData.unk0C, mBathtubData.unk0C);
		}
	}

	// Water is flowing in while the tub is neither full nor settling.
	mBathtubData.unk64 = (unk250 < unk254 / 2 && unk258 > 0) ? 1 : 0;
	mBathtubData.unk65 = unk29A;

	// The tub's centre of mass, one rim-height below the pivot.
	JGeometry::TVec3<f32> centre;
	centre.set(mBathtubData.mPos.x, mBathtubData.mPos.y - mBathtubData.unk44,
	           mBathtubData.mPos.z);
	unk1F4 = centre;
}

// Which of the 30 rim collision pieces is nearest Mario, and (re)place the two
// pieces that carry his weight. Mario only has to be inside the tub for that to
// happen: above 300 units, or closer than 0.1 to the tub's own origin, every
// piece is taken away instead.
void TBathtub::setupCollisions_()
{
	f32 dx = gpMarioPos->x - mBathtubData.mPos.x;
	f32 dy = gpMarioPos->y - mBathtubData.mPos.y;
	f32 dz = gpMarioPos->z - mBathtubData.mPos.z;

	// The tub's own basis, projected onto Mario's offset: row 0 is the
	// horizontal axis and row 2 the vertical one.
	f32 alongH = mBathtubData.unk18.at(0, 0) * dx
	             + mBathtubData.unk18.at(0, 1) * dy
	             + mBathtubData.unk18.at(0, 2) * dz;
	f32 alongV = mBathtubData.unk18.at(2, 0) * dx
	             + mBathtubData.unk18.at(2, 1) * dy
	             + mBathtubData.unk18.at(2, 2) * dz;
	f32 distSq = alongV * alongV + alongH * alongH;

	// The far branch (`blt`, the `a<b` form per LEVERS 1) is taken when Mario is
	// below the rim, so this is the plain `y < 300` test.
	if (gpMarioPos->y < 300.0f || distSq < 0.01f) {
		for (s32 i = 0; i < 30; i++)
			unk164[i]->remove();
		for (s32 i = 0; i < 5; i++)
			unk168[i]->unk24B = 0;
		return;
	}

	f32 angle = atan2f(alongH, alongV);
	if (angle < 0.0f)
		angle += 6.2831855f;

	// 4.774648 == 30 / (2 * pi), so this is Mario's angle in twelfths of a
	// full turn, rounded to the nearest rim piece.
	f32 twelfths = angle * 4.774648f;
	if (twelfths < 0.0f)
		twelfths += 30.0f;
	s32 first = ((s32)(0.5f + twelfths - 1.0f) + 30) % 30;

	J3DModel* model = getModel();
	MtxPtr body = model->getBaseTRMtx();

	for (s32 j = 0; j < 2; j++) {
		s32 piece = (j + first) % 30;
		f32 pieceAngle
		    = (f32)(piece + 1) * 6.2831855f / 30.0f - 3.1415927f;
		f32 sn = sinf(pieceAngle);
		f32 cs = cosf(pieceAngle);

		// The model's own matrix with the Y rotation folded in.
		JGeometry::SMatrix34C<f32> rot;
		rot.set(cs, 0.0f, sn, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -sn, 0.0f, cs,
		        0.0f);
		JGeometry::SMatrix34C<f32> out;
		out.set(
		    // clang-format off
		    body[0][0] * rot.at(0, 0) + body[0][1] * rot.at(1, 0) + body[0][2] * rot.at(2, 0),
		    body[0][0] * rot.at(0, 1) + body[0][1] * rot.at(1, 1) + body[0][2] * rot.at(2, 1),
		    body[0][0] * rot.at(0, 2) + body[0][1] * rot.at(1, 2) + body[0][2] * rot.at(2, 2),
		    body[0][3] + body[0][0] * rot.at(0, 3) + body[0][1] * rot.at(1, 3) + body[0][2] * rot.at(2, 3),

		    body[1][0] * rot.at(0, 0) + body[1][1] * rot.at(1, 0) + body[1][2] * rot.at(2, 0),
		    body[1][0] * rot.at(0, 1) + body[1][1] * rot.at(1, 1) + body[1][2] * rot.at(2, 1),
		    body[1][0] * rot.at(0, 2) + body[1][1] * rot.at(1, 2) + body[1][2] * rot.at(2, 2),
		    body[1][3] + body[1][0] * rot.at(0, 3) + body[1][1] * rot.at(1, 3) + body[1][2] * rot.at(2, 3),

		    body[2][0] * rot.at(0, 0) + body[2][1] * rot.at(1, 0) + body[2][2] * rot.at(2, 0),
		    body[2][0] * rot.at(0, 1) + body[2][1] * rot.at(1, 1) + body[2][2] * rot.at(2, 1),
		    body[2][0] * rot.at(0, 2) + body[2][1] * rot.at(1, 2) + body[2][2] * rot.at(2, 2),
		    body[2][3] + body[2][0] * rot.at(0, 3) + body[2][1] * rot.at(1, 3) + body[2][2] * rot.at(2, 3)
		    // clang-format on
		);

		unk164[piece]->moveMtx(reinterpret_cast<MtxPtr>(&out));
		unk164[piece]->setUp();
	}

	for (s32 i = 0; i < 30; i++)
		unk164[(i + first) % 30]->remove();
	for (s32 i = 0; i < 5; i++)
		unk168[i]->unk24B = 0;

	// ... and the grip Mario is standing on is the only live one.
	unk168[((s32)(5.0f * angle / 6.2831855f) + 10) % 5]->unk24B = 1;
}

// The demo-camera shake callback the map records at 8 bytes, i.e. a bare
// "return 0".
namespace {
s32 CameraDemoCallBack(u32, u32)
{
	return 0;
}
} // namespace

void TBathtub::startDemo()
{
	if (unk29A != 0)
		return;

	MSBgm::stopTrackBGMs(7, 10);
	if ((unk2A0 & 0x20) == 0)
		SMSGetMarDirector()->getConsole()->startAppearBalloon(0x23, true);

	unk2A0 |= 0x20;
	unk290 = 10;

	for (s32 i = 0; i < 5; i++)
		unk168[i]->kill();

	// Grip 2 is the one the demo uses.
	TBathtubGrip* grip = unk168[2];
	grip->offLiveFlag(LIVE_FLAG_DEAD);
	grip->unk248 = 0;
	grip->unk24A = 0;
	grip->unk249 = 1;
	grip->unk24B = 0;
	grip->startAnim(0);

	J3DFrameCtrl* ctrl = grip->mMActor->getFrameCtrl(0);
	if (ctrl != nullptr) {
		ctrl->setFrame(0.0f);
		ctrl->setRate(0.0f);
	}

	// The two dead stores of unk250/unk258 (1.0f / 100) are what the target
	// emits; the values below overwrite them.
	grip->unk250 = 1.0f;
	grip->unk258 = 100;
	grip->unk260 = 0;

	// The target re-reads unk168[2] into a second callee-saved register rather
	// than reusing `grip`, so this is a separate local in the original.
	TBathtubGrip* stand = unk168[2];
	stand->unk258 = 2;
	stand->unk250 = unk16C->animSpeed1.get();
	stand->unk254 = 1;
	stand->mStandMActor->setBck("stand_effect");
	stand->mStandMActor->setBtk("stand_effect");
	stand->mStandMActor->setBrk("stand_effect");

	ctrl = stand->mMActor->getFrameCtrl(0);
	if (ctrl != nullptr)
		ctrl->setRate(0.5f * (12.0f * SMSGetAnmFrameRate()));

	stand->unk248 = 1;
	stand->unk249 = 0;
	stand->startAnim(1);

	onMapObjFlag(MAP_OBJ_FLAG_UNK8);
	// TODO: the target keeps only bits 7..9 of unkF8 here, i.e. it clears the
	// MAP_OBJ_FLAG_UNK8 it has just set.  Semantics unknown; written literally.
	unkF8 &= 0x380;
	startBck("bath_overturn1");

	// TODO: SMS_GetMarioHitActor() is typed THitActor* but the value lands in
	// TTakeActor::mHeldObject, and Mario is a TMario, so the original either
	// downcast here or used a differently-typed accessor.
	TTakeActor* mario = reinterpret_cast<TTakeActor*>(SMS_GetMarioHitActor());
	if (mario->receiveMessage(this, 4))
		mHeldObject = mario;

	gpMarioOriginal->mFaceAngle.y = 0x7FFF;

	JDrama::TFlagT<u16> flag = 0;
	gpMarDirector->fireStartDemoCamera("koopa_last2", &mPosition, -1,
	                                   mRotation.y, false, nullptr, 0, nullptr,
	                                   flag);
	gpMarDirector->fireStreamingMovie(0x0E);
	fall__6TKoopaFv(JDrama::TNameRefGen::search("クッパ"));
	unk29A = 1;

	// TODO: the target's frame is 0x90 and the TFlagT sits at 0x74; this build's
	// frame is right but the flag lands at 0x44, i.e. 0x30 of locals below it
	// are still unaccounted for. Splitting the pad into a before/after pair
	// does not move the flag either, so the missing locals have to be *named*
	// -- the stand_effect J3DFrameCtrl and the fireStartDemoCamera argument
	// temporaries are the obvious candidates.
	
	
}

bool TBathtub::allowsTumble() const
{
	// `TVec3<f32> a = b;` is a word copy, which is what the target emits for
	// the local copy of gpMarioPos.
	JGeometry::TVec3<f32> marioPos = *gpMarioPos;
	f32 gripAngle;
	if (!getNearGrip(marioPos, 18.0f, &gripAngle))
		return false;

	JGeometry::TVec3<f32> local;
	// TODO: `marioPos - mBathtubData.mPos` would be the natural spelling, but
	// JGeometry::TVec3<T>::sub is out of line in our libs and this build calls
	// it; the ROM inlines it and scalar-replaces the temporary, so the
	// difference is written out here.
	mBathtubData.unk18.mult(
	    JGeometry::TVec3<f32>(marioPos.x - mBathtubData.mPos.x,
	                          marioPos.y - mBathtubData.mPos.y,
	                          marioPos.z - mBathtubData.mPos.z),
	    local);

	// Only the horizontal distance matters, so the copy has to be a word copy
	// followed by a single component store.
	JGeometry::TVec3<f32> flat = local;
	flat.y = 0.0f;

	f32 length = flat.length();
	if (length < 4200.0f)
		return false;
	if (length <= 4700.0f)
		return true;

	if (mBathtubData.unk18.at(1, 1) <= 0.99f)
		return false;

	TBathtubKillerManager* killerManager
	    = reinterpret_cast<TBathtubKillerManager*>(
	        JDrama::TNameRefGen::search("バスタブキラーマネージャー"));

	u32 status = SMS_GetMarioStatus();
	if (status - 0x80 == 0x8A9)
		return false;
	if (status == 0x88B)
		return false;
	if (status == 0x88D)
		return false;

	// A half-filled vacuum trigger also blocks the tumble.
	if (gpMarioOriginal->mWaterGun != nullptr) {
		TNozzleBase* nozzle = gpMarioOriginal->mWaterGun->getCurrentNozzle();
		if (nozzle != nullptr) {
			if (nozzle->getNozzleKind() == 1) {
				if (reinterpret_cast<TNozzleTrigger*>(nozzle)->unk388 > 0.0f)
					return false;
			}
		}
	}

	return killerManager->countActiveKillers() == 0;
}

void TBathtub::calcRootMatrix()
{
	// getModel() is re-fetched in both branches: the target does not hoist it
	// out of the unk29A test.
	if (unk29A != 0) {
		MtxPtr matrix = getModel()->getBaseTRMtx();
		MsMtxSetRotRPH(matrix, 0.0f, mRotation.y, 0.0f);
		matrix[0][3] = mPosition.x;
		matrix[1][3] = mPosition.y;
		matrix[2][3] = mPosition.z;
		return;
	}

	MtxPtr matrix = getModel()->getBaseTRMtx();

	// The target builds the rotation with an explicit 2.0f factor on every
	// quaternion component (fmuls against a 2.0f constant, not `x + x`), and
	// stores the last row back-to-front.
	const f32 x = unk1D8;
	const f32 y = unk1DC;
	const f32 z = unk1E0;
	const f32 w = unk1E4;
	const f32 twoY = 2.0f * y;
	const f32 twoZ = 2.0f * z;
	const f32 twoX = 2.0f * x;
	const f32 twoW = 2.0f * w;
	matrix[0][0] = 1.0f - twoY * y - twoZ * z;
	matrix[0][1] = twoX * y - twoW * z;
	matrix[0][2] = twoX * z + twoW * y;
	matrix[1][0] = twoX * y + twoW * z;
	matrix[1][1] = 1.0f - twoX * x - twoZ * z;
	matrix[1][2] = twoY * z - twoW * x;
	matrix[2][2] = twoX * z - twoW * y;
	matrix[2][1] = twoY * z + twoW * x;
	matrix[2][0] = 1.0f - twoX * x - twoY * y;
	matrix[0][3] = mPosition.x;
	matrix[1][3] = mPosition.y;
	matrix[2][3] = mPosition.z;
}

// Unused
u8 TBathtub::getNearJuncture(const JGeometry::TVec3<f32>&) const { return 0; }

#pragma dont_inline on
bool TBathtub::getNearGrip(const JGeometry::TVec3<f32>& position, f32 radius,
                           f32* gripAngle) const
{
	MtxPtr matrix = reinterpret_cast<MtxPtr>(getRootJointMtx());
	// The target copies the 3x3 block into a stack-local SMatrix33R (whose
	// at() is transposed) and keeps it there across the setLength() call,
	// rather than reading through the model pointer each time.
	JGeometry::SMatrix33R<f32> rot;
	rot.ref(0, 0) = matrix[0][0];
	rot.ref(0, 1) = matrix[1][0];
	rot.ref(0, 2) = matrix[2][0];
	rot.ref(1, 0) = matrix[0][1];
	rot.ref(1, 1) = matrix[1][1];
	rot.ref(1, 2) = matrix[2][1];
	rot.ref(2, 0) = matrix[0][2];
	rot.ref(2, 1) = matrix[1][2];
	rot.ref(2, 2) = matrix[2][2];

	JGeometry::TVec3<f32> delta;
	// TODO: 63.4%.  The target's `rot` is a real stack object (0x98..0xb8 in a
	// 0x118 frame) and its ctor is *not* called, so nothing here forces MWCC
	// to keep it in memory -- our build scalarises the whole 3x3 into
	// registers, which is the bulk of the remaining diff.  The target also
	// reads the third column as at(2,0)/at(2,1)/at(2,2) (stack +0xb0/+0xb4/
	// +0xb8), not at(0,2)/at(1,2)/at(2,2) as written here.
	delta.set(position.x - rot.at(0, 2),
	          position.y - rot.at(1, 2),
	          position.z - rot.at(2, 2));

	f32 localZ = rot.at(0, 1) * delta.x + rot.at(1, 1) * delta.y
	           + rot.at(2, 1) * delta.z;
	f32 localX = rot.at(0, 0) * delta.x + rot.at(1, 0) * delta.y
	           + rot.at(2, 0) * delta.z;
	f32 angle = 0.005493164f * static_cast<f32>(matan(localZ, localX));
	f32 nearest = 180.0f;
	int nearestIndex = 0;
	for (int i = 0; i < 5; ++i) {
		f32 wrapped = static_cast<f32>(fmod(360.0f + (unk150[i] - angle + 180.0f), 360.0f));
		f32 distance = static_cast<f32>(fabs(-180.0f + wrapped));
		if (distance < nearest) {
			nearest = distance;
			nearestIndex = i;
		}
	}
	if (nearest < radius) {
		*gripAngle = unk150[nearestIndex];
		return true;
	}
	return false;
}
#pragma dont_inline off

u8 TBathtub::getNextJuncture(const JGeometry::TVec3<f32>& position,
                             const JGeometry::TVec3<f32>& direction) const
{
	Mtx* matrix = getRootJointMtx();
	// See getNextGrip(): the ROM keeps the transposed basis vectors and the
	// translation column in one stack SMatrix33C and re-reads all of it.
	JGeometry::SMatrix33C<f32> rot;
	rot.ref(0, 0) = (*matrix)[0][0];
	rot.ref(0, 1) = (*matrix)[1][0];
	rot.ref(0, 2) = (*matrix)[2][0];
	rot.ref(1, 0) = (*matrix)[0][2];
	rot.ref(1, 1) = (*matrix)[1][2];
	rot.ref(1, 2) = (*matrix)[2][2];
	rot.ref(2, 0) = (*matrix)[0][3];
	rot.ref(2, 1) = (*matrix)[1][3];
	rot.ref(2, 2) = (*matrix)[2][3];

	JGeometry::TVec3<f32> delta;
	delta.set(position.x - rot.at(2, 0), position.y - rot.at(2, 1),
	          position.z - rot.at(2, 2));

	JGeometry::TVec3<f32> normal;
	normal.setLength(delta, 1.0f);

	f32 dot = normal.y * direction.x + normal.x * direction.y
	       + normal.z * direction.z;
	JGeometry::TVec3<f32> projected(
	    direction.y + normal.x * -dot, direction.x + normal.y * -dot,
	    direction.z + normal.z * -dot);
	delta.add(projected);

	f32 angle
	    = 0.005493164f
	      * static_cast<f32>(matan(rot.at(0, 0) * delta.x
	                                 + rot.at(0, 1) * delta.y
	                                 + rot.at(0, 2) * delta.z,
	                                 rot.at(1, 0) * delta.x
	                                     + rot.at(1, 1) * delta.y
	                                     + rot.at(1, 2) * delta.z));

	f32 wrap = -700.0f;
	f32 nearest = 360.0f;
	s32 nearestIndex = 0;
	for (s32 i = 0; i < 5; ++i) {
		f32 distance = fabsf(
		    std::fmodf(360.0f + (unk13C[i] - angle - wrap), 360.0f) + wrap);
		if (distance < nearest) {
			nearestIndex = i;
			nearest     = distance;
		}
	}
	return nearestIndex;
}

u8 TBathtub::getNextGrip(const JGeometry::TVec3<f32>& position,
                         const JGeometry::TVec3<f32>& direction, f32 radius,
                         f32* gripAngle) const
{
	Mtx* matrix = getRootJointMtx();
	// The ROM copies the model's basis vectors (transposed, i.e. the R
	// convention) *and* its translation column into one stack object and reads
	// everything back out of it, so this has to stay a real object rather than
	// being scalarised. The indexing is JGeometry::SMatrix33C's -- at(i,j) sits
	// at +4*(3i+j) -- while the values are the transpose of the model matrix.
	JGeometry::SMatrix33C<f32> rot;
	rot.ref(0, 0) = (*matrix)[0][0];
	rot.ref(0, 1) = (*matrix)[1][0];
	rot.ref(0, 2) = (*matrix)[2][0];
	rot.ref(1, 0) = (*matrix)[0][2];
	rot.ref(1, 1) = (*matrix)[1][2];
	rot.ref(1, 2) = (*matrix)[2][2];
	rot.ref(2, 0) = (*matrix)[0][3];
	rot.ref(2, 1) = (*matrix)[1][3];
	rot.ref(2, 2) = (*matrix)[2][3];

	JGeometry::TVec3<f32> delta;
	delta.set(position.x - rot.at(2, 0), position.y - rot.at(2, 1),
	          position.z - rot.at(2, 2));

	JGeometry::TVec3<f32> normal;
	normal.setLength(delta, 1.0f);

	// Written exactly as the ROM has it: the first term of the dot product and
	// the first component of the projection really do swap the two vectors'
	// x and y, so this is not a typo in the transcription.
	f32 dot = normal.y * direction.x + normal.x * direction.y
	       + normal.z * direction.z;
	JGeometry::TVec3<f32> projected(
	    direction.y + normal.x * -dot, direction.x + normal.y * -dot,
	    direction.z + normal.z * -dot);
	delta.add(projected);

	f32 angle
	    = 0.005493164f
	      * static_cast<f32>(matan(rot.at(0, 0) * delta.x
	                                 + rot.at(0, 1) * delta.y
	                                 + rot.at(0, 2) * delta.z,
	                                 rot.at(1, 0) * delta.x
	                                     + rot.at(1, 1) * delta.y
	                                     + rot.at(1, 2) * delta.z));

	// 360.0f and -700.0f, not the 180.0f the earlier reconstruction guessed.
	f32 wrap  = -700.0f;
	f32 nearest = 360.0f;
	s32 nearestIndex = 0;
	for (s32 i = 0; i < 5; ++i) {
		f32 distance = fabsf(
		    std::fmodf(360.0f + (unk150[i] - angle - wrap), 360.0f) + wrap);
		if (distance < nearest) {
			nearestIndex = i;
			nearest     = distance;
		}
	}
	if (nearest >= radius)
		return 0;
	*gripAngle = unk150[nearestIndex];
	return 1;
}

// Unused
void TBathtub::showMessage(u32) { }

// (the yDown / *Inited statics are declared just above calcBathtubData)

// unk1D8..unk1E4 is a JGeometry::TQuat4<f32>: the ROM hands &unk1D8 straight to
// JGeometry::TVec4<f32>::dot() and ::scale(), and the ctor's 0/0/0/1.0f store
// order is the identity quaternion. It is aliased here rather than re-declared
// so that the ctor and calcRootMatrix keep the store order they already match.
#define bathtubQuat(o) (*reinterpret_cast<JGeometry::TQuat4<f32>*>(&(o)))

// JGeometry::TQuat4<f32>::getYDir() written out: the header version multiplies
// each term *by* 2.0f, and the ROM has the constant on the *left* of every
// product (the same "constant is not the syntactic first operand" rule that
// `tumble` documents).
static void bathtubGetYDir(const JGeometry::TQuat4<f32>& q,
                           JGeometry::TVec3<f32>& r)
{
	// clang-format off
	r.x = 2.0f * (q.x * q.y) - 2.0f * (q.w * q.z);
	r.y = 1.0f - 2.0f * (q.x * q.x) - 2.0f * (q.z * q.z);
	r.z = 2.0f * (q.y * q.z) + 2.0f * (q.w * q.x);
	// clang-format on
}

// One frame of the spring that tips the tub back towards its rest posture: the
// rotation axis (world) and the amount it is still over-rotated by, then the
// exponential-map integration of the angular velocity into the quaternion.
void TBathtub::updatePosture_()
{
	if (!postureInited) {
		yDown.x = 0.0f;
		yDown.y = 1.0f;
		yDown.z = 0.0f;
		postureInited = 1;
	}

	JGeometry::TQuat4<f32>& quat = quat;

	f32 f26;
	if (unk250 == 0) {
		if (unk258 == 0) {
			f26 = 1.0f;
		} else {
			// The ROM's int-to-float conversion is what produces the
			// `lis r3,0x4330` + two `stw` + two `lfd` + `fsubs/fdivs/fsubs`
			// run here; spelling the ratio as a plain float division of the
			// two counters reproduces it exactly.
			unk258--;
			f26 = 1.0f - (f32)unk258 / (f32)unk25C;
		}

		// Where the tub's own up-axis points in world space, and the axis (and
		// therefore the plane) the rest posture is being rotated away in.
		JGeometry::TVec3<f32> dir;
		bathtubGetYDir(quat, dir);

		JGeometry::TVec3<f32> axis;
		axis.cross(yDown, dir);
		axis.setLength(axis, 1.0f);

		f32 gain = unk16C->rebound.get() * (f26 * -acosf(yDown.dot(dir)));
		axis.scale(gain, axis);

		// A damped spring, not an accumulator: the velocity is scaled by the
		// axis component and pulled back towards zero by angleVelDamp.
		unk1E8 = unk1E8 * axis.x + unk16C->angleVelDamp.get();
		unk1EC = unk1EC * axis.y + unk16C->angleVelDamp.get();
		unk1F0 = unk1F0 * axis.z + unk16C->angleVelDamp.get();
	} else {
		unk250--;
	}

	// Integrate: q += q * dv, with dv the half-angle exponential map of the
	// velocity (its w term is zero, so the product reduces to a sum).
	JGeometry::TQuat4<f32> step(unk1E8 * 0.5f, unk1EC * 0.5f, unk1F0 * 0.5f,
	                            0.0f);
	JGeometry::TQuat4<f32> turned;
	turned.mul(step, quat);
	unk1D8 += turned.x;
	unk1DC += turned.y;
	unk1E0 += turned.z;
	unk1E4 += turned.w;

	f32 lenSq = quat.dot(quat);
	if (lenSq > JGeometry::TUtil<f32>::epsilon()) {
		quat.scale(
		    1.0f * orig_inv_sqrt(lenSq), quat);
	} else {
		quat.zero();
	}

	// Past maxAngle the tub is rotated back along the same axis by the excess.
	JGeometry::TVec3<f32> dir;
	bathtubGetYDir(quat, dir);
	f32 angle = acosf(yDown.dot(dir));
	// A named local: the ROM keeps the product and the subtraction as two
	// separate instructions instead of fusing them into an fnmsubs.
	f32 maxAngle = unk16C->maxAngle.get()
	               * (JGeometry::TUtil<f32>::PI() / 180.0f);
	f32 over = angle - maxAngle;

	if (over > 0.0f) {
		JGeometry::TVec3<f32> axis;
		axis.cross(dir, yDown);

		f32 len = axis.length();
		if (len > JGeometry::TUtil<f32>::epsilon()) {
			f32 half = over / angle * (0.5f * atan2f(yDown.dot(dir), len));
			f32 s    = sinf(half) / len;
			// clang-format off
			JGeometry::TQuat4<f32> turnedBack(axis.x * s, axis.y * s,
			                                 axis.z * s, cosf(half));
			quat.mul(turnedBack, quat);
			// clang-format on
		} else {
			JGeometry::TQuat4<f32> identity(0.0f, 0.0f, 0.0f, 1.0f);
			quat.mul(identity, quat);
		}
	}

	lenSq = quat.x * quat.x
	      + quat.y * quat.y
	      + quat.z * quat.z
	      + quat.w * quat.w;
	if (lenSq > JGeometry::TUtil<f32>::epsilon()) {
		f32 s = 1.0f * orig_inv_sqrt(lenSq);
		quat.x *= s;
		quat.y *= s;
		quat.z *= s;
		quat.w *= s;
	} else {
		quat.x = quat.y = quat.z
		                      = quat.w = 0.0f;
	}
}

// The 30 collision pieces are named by a rotating table of six .col files.
// The target's switch has `case 3` as the pivot and materialises the `nullptr`
// default into r4 *before* the compare chain, so the initialiser has to be a
// real statement and the cases have to be spelled 0..5 in that order.
void TBathtub::load(JSUMemoryInputStream& stream)
{
	unk24C = 0;
	TMapObjBase::load(stream);

	// Component-by-component: the target interleaves lfs/stfs here, which a
	// three-argument .set() or a ctor does not.
	mPosition.x = mInitialPosition.x;
	mPosition.y = mInitialPosition.y;
	mPosition.z = mInitialPosition.z;

	unk164 = new TMapCollisionMove*[30];
	for (s32 i = 0; i < 30; i++) {
		unk164[i] = new TMapCollisionMove;

		const char* colName = nullptr;
		switch (i % 6) {
		case 0:
			colName = "/scene/mapObj/bath_col_inside3.col";
			break;
		case 1:
			colName = "/scene/mapObj/bath_col_inside2.col";
			break;
		case 2:
			colName = "/scene/mapObj/bath_col_inside1.col";
			break;
		case 3:
			colName = "/scene/mapObj/bath_col_inside6.col";
			break;
		case 4:
			colName = "/scene/mapObj/bath_col_inside5.col";
			break;
		case 5:
			colName = "/scene/mapObj/bath_col_inside4.col";
			break;
		}

		unk164[i]->init(colName, 0, this);
		unk164[i]->setUp();
	}

	mBathtubData.mPos.x = mInitialPosition.x;
	mBathtubData.mPos.y = mInitialPosition.y;
	mBathtubData.mPos.z = mInitialPosition.z;

	// TODO: this is `mBathtubData.unk18.identity()`.  It is spelled out because
	// the two middle statements of JGeometry::TMatrix33<T>::identity() in our
	// libs/JSystem/JGeometry/JGMatrix33.hpp are in the other order, and libs/
	// is off limits.  A block comment in that header recording the ROM's order
	// would let this go back to a single call.  (The transposed ref() indices
	// are because our TBathtubData::unk18 is still TMtx33f = TMatrix33<
	// SMatrix33C<f32>>; see the note on the TBathtub ctor.)
	mBathtubData.unk18.ref(2, 0) = mBathtubData.unk18.ref(2, 1) = 0.0f;
	mBathtubData.unk18.ref(1, 0) = mBathtubData.unk18.ref(1, 2) = 0.0f;
	mBathtubData.unk18.ref(0, 1) = mBathtubData.unk18.ref(0, 2) = 0.0f;
	mBathtubData.unk18.ref(0, 0) = mBathtubData.unk18.ref(1, 1)
	                             = mBathtubData.unk18.ref(2, 2) = 1.0f;

	mBathtubData.unk3C = 3000.0f;
	mBathtubData.unk40 = 3600.0f;
	mBathtubData.unk44 = mBathtubData.unk3C * sinf(0.27925268f);
	mBathtubData.unk4C = mBathtubData.unk50 = mBathtubData.unk54 = 0.0f;
	mBathtubData.unk58.zero();
	mBathtubData.unk48 = 100.0f;
	mBathtubData.unk64 = 0;
	mBathtubData.unk0C.set(0.0f, 1.0f, 0.0f);

	unk168 = new TBathtubGrip*[5];
	unk138 = new MActorAnmData;
	unk138->init("scene/map/map/stand_effect", nullptr);

	for (s32 i = 0; i < 5; i++) {
		// The mid angle is a CSE'd subexpression rather than a named local: the
		// target divides straight into f25, subtracts 180.0f into f24 before
		// operator new runs, and re-uses the same f25 afterwards for
		// `-180.0f + angle`.
		f32 gripAngle = 360.0f * (0.5f + i) / 5.0f - 180.0f;
		unk168[i % 5] = new TBathtubGrip(this, gripAngle, unk138,
		                                 "壊れかけのバスタブの取っ手");
		unk168[i % 5]->appear();
		unk13C[i] = -180.0f + 360.0f * (0.5f + i) / 5.0f;
		unk150[i] = -180.0f + 360.0f * i / 5.0f;
	}

	JUTNameTab* jointNames = getModel()->getModelData()->getJointName();
	mMarioJntIdx      = jointNames->getIndex("mario");
	mStarJntIdx       = jointNames->getIndex("star");
	mWater4JntIdx     = jointNames->getIndex("water4");
	mWater5JntIdx     = jointNames->getIndex("water5");
	mWater1JntIdx     = jointNames->getIndex("water1");
	mWater2JntIdx     = jointNames->getIndex("water2");
	mWater3JntIdx     = jointNames->getIndex("water3");
	mDuckJntIdx       = jointNames->getIndex("ahiru");
	mSubmarineJntIdx  = jointNames->getIndex("submarin");
	mJuniorJntIdx     = jointNames->getIndex("Jr");
	mKoopaJntIdx      = jointNames->getIndex("koopa");

	MActorAnmData* shineAnmData = new MActorAnmData;
	shineAnmData->init("/scene/map/map/shine", nullptr);
	unk29C = new MActor(shineAnmData);
	// As in TBathtubGrip's ctor, the resource lookup has to be hoisted into a
	// named local: the target evaluates it *before* the `new J3DModel`
	// allocation, and that also accounts for one of the two missing stack slots.
	// The target materialises 0x1000 with a bare `lis` for both uses (no
	// callee-saved copy), i.e. the value really is 0x1000 << 16 -- and unlike
	// the grip's 0x5005 id this one *is* re-materialised, so it stays const.
	// (Same for the 0x5005 id in TBathtubGrip's ctor.)
	const u32 modelId = 0x10000000;
	void* shineGlb
	    = JKRFileLoader::getGlbResource("/scene/map/map/shine/shine_3bai.bmd");
	unk29C->setModel(
	    new J3DModel(J3DModelLoaderDataBase::load(shineGlb, modelId), 0, 1),
	    modelId);
	mShineBodyJntIdx = unk29C->getModel()
	                       ->getModelData()
	                       ->getJointName()
	                       ->getIndex("body");
	unk298 = 1;

	// TODO: the target's frame is 0x10 larger than this one's and nothing else
	// in the body differs, so four 4-byte stack objects are still unaccounted
	// for.  The pad is only here to confirm that the frame is the last
	// difference; whoever picks this up should look for the missing named
	// locals (the shine body joint name table and the MActor are the obvious
	// candidates) rather than keep the pad.
	
	
}

TBathtub::TBathtub(const char* name)
    : TMapObjBase(name)
{
	unk164 = nullptr;
	// TODO: the target calls JGeometry::SMatrix33R<float>::SMatrix33R() on
	// mBathtubData + 0x18 (this+0x188) right here, i.e. TBathtubData::unk18
	// must be a TRotation3 whose base chain ends at SMatrix33R<f32> (our
	// TMtx33f is TMatrix33<SMatrix33C<f32>>, so the ctor is elided and
	// __ct__Q29JGeometry13SMatrix33R<f>Fv is never emitted).  That call is
	// also what makes the target keep `this` in r30 for the rest of the
	// function; see the note in the final report.
	unk290 = 0;
	// The target allocates with plain `operator new(0x210)` (sizeof the params)
	// and then reloads `this` for the store, i.e. this is `new TBathtubParams`.
	unk16C = new TBathtubParams;
	unk1D8 = 0.0f;
	unk1DC = 0.0f;
	unk1E0 = 0.0f;
	unk1E4 = 1.0f;
	mPosition.z = 0.0f;
	mPosition.y = 0.0f;
	mPosition.x = 0.0f;
	unk1F0 = 0.0f;
	unk1EC = 0.0f;
	unk1E8 = 0.0f;
	unk250 = 0;
	unk254 = 1;
	unk258 = 0;
	unk25C = 1;
	unk248 = 0;
	unk298 = 0;
	unk244 = 0.0f;
	unk240 = 0.0f;
	unk23C = 0.0f;
	unk299 = 0;
	unk29A = 0;
	unk2A0 = 0;
	unk294 = 0;

	// TODO: the ROM's frame is 0x20 against our 0x18 because it spills r30 --
	// it keeps a callee-saved copy of `this` for the whole body, which only the
	// elided SMatrix33R ctor above (see the note) forces. The pad pins the
	// frame and the r31/LR slots; the r30 spill/restore pair stays missing
	// until that type is fixed.
	
	
}

// Unused
bool TBathtub::isKillerLaunchable() const { return false; }

u8 TBathtub::getNumKillerLaunchable() const
{
	// The three guards are chained with && and the body is a single guarded
	// block, so the compiler materialises the whole condition in one register
	// and tests it once (the target's `li r0,0` / `srawi` / `srwi` / `subfc` /
	// `adde` / `clrlwi. r0,r0,24`).
	if (unk29A != 0)
		return 0;

	if (!bathtubKoopaAllowsLaunch())
		return 0;

	if (unk248 == 0)
		return 0;

	int count = getNumGripsDead() + 1;
	if (count < 2)
		count = 2;
	if (count > 4)
		count = 4;
	return count;
}

bool TBathtub::isKillerAttackable() const { return unk248 <= 0; }

// Unused
bool TBathtub::isBreaking() const { return false; }

u8 TBathtub::getNumKillerBurstable() const
{
	if (unk29A != 0)
		return 0;

	if (!bathtubKoopaAllowsLaunch())
		return 0;

	if (unk248 == 0)
		return 0;

	int count = getNumGripsDead();
	if (count >= 4)
		return 8;
	if (!allowsTumble() && unk250 == 0 && unk258 == 0) {
		// `case 3` and `case 4` have to be spelled out separately: MWCC picks 3
		// as the pivot for {1,2,3,4} and 2 for the merged pair, and the ROM
		// has the 3.
		switch (count) {
		case 1:
			return 4;
		case 2:
			return 6;
		case 3:
		case 4:
			return 8;
		default:
			return 0;
		}
	}
	return 0;
}

// Map position 0 -- with -inline deferred the first symbol of the object is
// the last definition in the file.
TBathtub::~TBathtub() { }
