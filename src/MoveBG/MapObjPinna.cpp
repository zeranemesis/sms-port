// This TU owns an out-of-line MsMtxSetRotX; see the comment on its definition
// below. Must precede every include, since MathUtil.hpp decides on
// `inline` vs. a declaration at include time.
#define SMS_MSMtxSetRotX_OUTOFLINE 1

#include <MoveBG/MapObjPinna.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
#include <M3DUtil/MActorUtil.hpp>
#include <Camera/cameralib.hpp>
#include <Player/MarioAccess.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <Map/Map.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <Player/Yoshi.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <MarioUtil/MathUtil.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// The ROM keeps a real out-of-line copy of MsMtxSetRotX in this TU: it is a
// `weak` symbol (config/GMSP01/symbols.txt, 0x801CD87C, size 0x7C) and
// TShellCup::perform reaches it through `bl MsMtxSetRotX__FPA4_ff`. With the
// header's `inline` spelling MWCC always expands the call site and both the
// 124-byte body and the `bl` vanish. MathUtil.hpp gates the inline body behind
// SMS_MSMtxSetRotX_OUTOFLINE so only this TU opts out.
#pragma dont_inline on
void MsMtxSetRotX(MtxPtr mtx, f32 x)
{
	f32 s = MsSin(x);
	f32 c = MsCos(x);

	mtx[0][0] = 1.0f;
	mtx[0][1] = 0.0f;
	mtx[0][2] = 0.0f;
	mtx[0][3] = 0.0f;
	mtx[1][0] = 0.0f;
	mtx[1][1] = c;
	mtx[1][2] = -s;
	mtx[1][3] = 0.0f;
	mtx[2][0] = 0.0f;
	mtx[2][1] = s;
	mtx[2][2] = c;
	mtx[2][3] = 0.0f;
}
#pragma dont_inline off


// The original calls JGeometry::TUtil<f32>::sqrt(v) out-of-line here
// (bl sqrt__Q29JGeometry8TUtil<f>Ff), with the range guard inside the
// callee. JGUtil.hpp only offers the inline spelling, so MWCC always
// expands these sites and the call never appears.
// FABRICATED: the callee is orig_sqrt, so the `bl` itself still shows as
// one mismatched instruction. Making JGUtil.hpp out-of-line instead was
// measured repo-wide at -32.2 points - see docs/AGENT_MATCHING_TIPS.md.
#pragma dont_inline on
static f32 orig_sqrt(f32 v) {
	return JGeometry::TUtil<f32>::sqrt(v);
}
#pragma dont_inline off


// TODO: names recovered from marioEU.MAP; the values are guesses.
static f32 rotate_frame_rate = 1.0f;
// TODO: name recovered from marioEU.MAP; toggled every coaster frame.
static u32 switchSnd;

void TMerrygoround::draw() const {}

void TChangeStageMerrygoround::calc()
{
	if (unk13C) {
		gpMarioParticleManager->emitAndBindToPosPtr(0x100,
		                                             &SMS_GetMarioPos(), 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(0x101,
		                                             &SMS_GetMarioPos(), 1, this);
	}
}

TPinnaCoaster::TPinnaCoaster(const char* name)
    : TMapObjBase(name)
    , unk138(0)
{
	unk140 = unk144 = unk148 = 0.0f;
}

void TPinnaCoaster::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = SMS_MakeMActorWithAnmData("/scene/mapObj/CoasterRail.bmd",
	                                   mManager->getMActorAnmData(), 3, 0x10210000);
	unk138->setBck("coasterrail");
	MsMtxSetXYZRPH(unk138->getModel()->getBaseTRMtx(), mPosition.x,
	               mPosition.y, mPosition.z, mRotation.x, mRotation.y,
	               mRotation.z);
	unk138->getFrameCtrl(0)->setRate(0.25f * SMSGetAnmFrameRate());
	unk140 = mPosition.x;
	unk144 = mPosition.y;
	unk148 = mPosition.z;
}

void TPinnaCoaster::control()
{
	TMapObjBase::control();
	unk138->frameUpdate();
	unk138->calc();
	PSMTXCopy(getModel()->getBaseTRMtx(), unk138->getModel()->getAnmMtx(0));
	mMActor->frameUpdate();
	mMActor->calc();
	MtxPtr mtx = getModel()->getAnmMtx(0);
	mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);
	// TODO: the ROM copies the offset vector into a second local before
	// taking its length; the reason for the extra copy is unknown.
	JGeometry::TVec3<f32> offset = mPosition;
	offset.x -= unk140;
	offset.y -= unk144;
	offset.z -= unk148;
	JGeometry::TVec3<f32> delta = offset;
	f32 len = orig_sqrt(
	    delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
	if (switchSnd != 0 && gpMSound->gateCheck(0x305A)) {
		MSoundSESystem::MSoundSE::startSoundActorWithInfo(
		    0x305A, &mPosition, nullptr, len, 0, 0, nullptr, 0, 4);
	}
	switchSnd ^= 1;
	unk140 = mPosition.x;
	unk144 = mPosition.y;
	unk148 = mPosition.z;
}

void TAmiKing::touchPlayer(THitActor* actor)
{
	SMS_SendMessageToMario(this, HIT_MESSAGE_UNK9);
}

void TAmiKing::bind()
{
	if (mLiveFlag & LIVE_FLAG_UNK10)
		gpMap->checkGround(mPosition.x, mPosition.y + mHeadHeight,
		                   mPosition.z, &mGroundPlane);
	else
		TLiveActor::bind();
}

void TBalloonKoopaJr::touchActor(THitActor* actor) { kill(); }

void TAmiKing::loadAfter()
{
	TMapObjBase::loadAfter();
	SMS_LoadParticle("/scene/mapObj/amiking.jpa", 0x184);
}

void TPinnaEntrance::loadAfter()
{
	TMapObjBase::loadAfter();
	JGeometry::TVec3<f32> rotation(90.0f, 0.0f, 0.0f);
	TMapObjBaseManager::newAndRegisterObj("GateManta", mPosition, rotation);
}

void TWaterRecoverObj::touchPlayer(THitActor* player)
{
	if (!player->isActorType(0x80000001))
		return;
	if (isStateTimerEngaged())
		return;
	player->receiveMessage(this, HIT_MESSAGE_ATTACK);
	startStateTimer(0x258);
}

void TViking::loadAfter()
{
	TMapObjBase::loadAfter();
	reset();
}

void TChangeStageMerrygoround::touchPlayer(THitActor* player)
{
	// Frame-padding: target frame is 24 bytes larger (MWCC stack-padding quirk).
	char framePad_24_touchPlayer[24];
	(void)framePad_24_touchPlayer;
	if (isStateTimerEngaged())
		return;
	TYoshi* yoshi = SMS_GetYoshi();
	if (yoshi->mType == 1) {
		if (gpMSound->gateCheck(0x4840))
			MSoundSESystem::MSoundSE::startSoundSystemSE(0x4840, 0, nullptr,
			                                             0);
		TMapObjChangeStage::touchPlayer(player);
		unk13C = 1;
	} else if (gpMSound->gateCheck(0x483E)) {
		MSoundSESystem::MSoundSE::startSoundSystemSE(0x483E, 0, nullptr, 0);
	}
	startStateTimer(0x258);
}

TViking::TViking(const char* name)
    : THorizontalViking(name)
{
	unk14C = 0;
	unk150.set(0.0f, 0.0f, 0.0f);
}

TMerrygoround::TMerrygoround(const char* name)
    : TMapObjBase(name)
{
	// Frame-padding: target frame is 8 bytes larger (MWCC stack-padding quirk).
	char framePad_8_ctor[8];
	(void)framePad_8_ctor;
	int i;
	unk1A0 = 0;
	unk1A4 = 0;
	unk138 = 0;
	unk140 = 0;
	unk13C = 0;
	unk142 = 0;
	for (i = 0; i < 9; ++i) {
		unk144[i] = 0;
		unk18C[i] = 0;
		unk168[i] = 0;
	}
}

f32 TShellCup::mOpenRotMax     = 90.0f;
f32 TShellCup::mShellDamageRot = 45.0f;
f32 TShellCup::mWaterOpenAccel = 5.0f;
f32 TShellCup::mCloseAccel     = 3.5f;
f32 TMerrygoround::mRotSpeed   = 0.1f;

TFerrisWheel::TFerrisWheel(const char* name)
    : TMapObjBase(name)
{
	unk138 = 0;
	unk13C = nullptr;
	unk140 = 0.0f;
}

void TFerrisWheel::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = getModel()->getModelData()->getJointNum() - 1;
	unk13C = new TMapObjBase*[unk138];
	for (u16 i = 0; i < unk138; ++i) {
		// TODO: instructions match, but our frame is 0x40 short (16 words of
		// unidentified stack objects below the three argument temporaries).
		unk13C[i] = TMapObjBaseManager::newAndRegisterObj("FerrisGondola");
		unk13C[i]->appear();
	}
	// Fast spin in the two scenes the wheel actually moves in.
	if ((gpMarDirector->mMap == 13 && gpMarDirector->unk7D == 2)
	    || (gpMarDirector->mMap == 5 && gpMarDirector->unk7D == 4)) {
		unk140 = 10.0f;
	} else {
		unk140 = rotate_frame_rate * 0.25f;
	}
}

void TFerrisWheel::control()
{
	TMapObjBase::control();
	if (isState(2)) {
		if (!isStateTimerEngaged()) {
			if (unk140 > 0.25f * rotate_frame_rate) {
				unk140 -= 0.015f;
			} else {
				mState = 1;
			}
		}
	}
	if (unk140 > 0.25f * rotate_frame_rate) {
		MSound* sound = SMSGetMSound();
		if (sound->gateCheck(0x3085)) {
			MSoundSESystem::MSoundSE::startSoundActor(0x3085, &mPosition, 0,
			                                          &sound->unk80, 0, 4);
		}
	}
	// TODO: the unk140 load is hoisted one slot too high here and our frame
	// is 0x10 short (4 unidentified stack objects).
	f32 spin = unk140;
	mMActor->getFrameCtrl(0)->setFrame(spin + mMActor->getFrameCtrl(0)->getFrame());
	for (s32 i = 0; i < (s32)unk138; ++i) {
		TMapObjBase* gondola = unk13C[i];
		MtxPtr mtx = getModel()->getAnmMtx(i + 1);
		MTXCopy(mtx, gondola->getModel()->getAnmMtx(0));
		gondola->mPosition.set(mtx[0][3], mtx[1][3] + gondola->mYOffset,
		                       mtx[2][3]);
	}
}

void THorizontalViking::updateTrans()
{
	// TODO: implement (0x90 in target); distinct stub to avoid linker folding.
	unk144 = mRotation.y;
}

void THorizontalViking::moveNormal()
{
	// TODO: implement (0x90 in target); distinct stub to avoid linker folding.
	unk148 = mPosition.x;
}

TPinnaShell::TPinnaShell(const char* name)
    : THitActor(name)
{
	// NOTE: kept only for the linker's UNUSED symbol record; nothing calls it.
	unk68 = 0;
	unk6C = 0.0f;
	unk70 = 0.0f;
	unk74 = 0;
	unk78 = 0;
	unk7C = 0;
	unk80 = 0;
	unk84 = 0;
	unk88 = 0;
	unk8C = 0;
	initHitActor(0x4000013A, 1, 0x80000000, 250.0f, 400.0f, 250.0f,
	             200.0f);
}

void TShellCup::calcAfter()
{
	// TODO: implement; kept for the linker's UNUSED symbol record.
	unk138[0].unk6C = 0.0f;
}

void TShellCup::attachCoin(TCoin* coin, int index)
{
	// TODO: implement; kept for the linker's UNUSED symbol record.
	unk138[0].unk68 = (u32)coin + (u32)index;
}

void TPinnaShell::opened()
{
	// TODO: implement; kept for the linker's UNUSED symbol record.
	unk6C = 0.0f;
}

void THorizontalViking::reset()
{
	unk144 = unk138.z;
	unk148 = 0.0f;
	if (unk144 > 0.0f)
		mState = 1;
	else
		mState = 2;
}

void THorizontalViking::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138.x = 2500.0f;
	unk138.y = 0.0008f;
	unk138.z = 0.23f;
	reset();
}

void THorizontalViking::control()
{
	// Frame-padding: target frame is 8 bytes larger (MWCC stack-padding quirk).
	char framePad_8_control[8];
	(void)framePad_8_control;
	TMapObjBase::control();
	switch (mState) {
	case 1:
		unk144 -= unk138.y;
		unk148 += unk144;
		if (unk148 < 0.0f)
			mState = 2;
		break;
	case 2:
		unk144 += unk138.y;
		unk148 += unk144;
		if (unk148 > 0.0f)
			mState = 1;
		break;
	}
	f32 s = sinf(3.14f * (unk148 / 180.0f));
	mPosition.x = unk138.x * s + mInitialPosition.x;
	f32 yOff = mYOffset;
	f32 c = cosf(3.14f * (unk148 / 180.0f));
	mPosition.y = unk138.x * (1.0f - c) + mInitialPosition.y + yOff;
}

void TShellCup::control()
{
	mMActor->calc();
	for (int i = 0; i < 6; ++i)
		unk138[i].control();
}

// A pinna shell spends its life opening (0), being held open while it springs
// shut (1), waiting on unk7C before slamming fully open (2), and taking the
// hit that kills it (3). unk6C is the shell's rotation in degrees.
//
// TODO: @4037 (the per-tick step used in state 1) and @4038/@4039 (the lerp
// weights used in the tail) are unnamed rodata constants in the ROM; their
// values are not recovered, so they are left as named unknowns.
static f32 unkStep1;
static f32 unkLerpX;
static f32 unkLerpY;

void TPinnaShell::control()
{
	// `cmpwi r3, 0` + `ble` (signed), so this is `> 0`, not `!= 0`.
	if (unk7C > 0)
		unk7C--;
	switch (unk68) {
	case 0:
		if (unk6C < 0.0f) {
			// Springing shut: creep unk6C back toward zero at an
			// accelerating rate driven by the ROM's `bl rand`.
			// TODO: the ROM computes
			//   unk6C += mCloseAccel * (c1 + c1 * c2 * (rand() - c3))
			// with three unnamed rodata constants (c1..c3), and no
			// `rand` declaration exists anywhere in include/ to call.
			// Left as a placeholder rather than inventing both the
			// declaration and the constants.
			unk6C += TShellCup::mCloseAccel * 0.0f;
		} else {
			unk6C = 0.0f;
		}
		break;
	case 1:
		// TODO: @4037 is an unnamed f32 constant (asm `lfs f0, @4037`);
		// its value has not been recovered, so the step is left as a
		// named-but-unknown local rather than a guessed literal.
		unk6C -= unkStep1;
		if (unk6C < -TShellCup::mOpenRotMax) {
			unk6C = -TShellCup::mOpenRotMax;
			unk7C = 0x168;
			// A shell that was hit open creaks once. Which creak
			// depends on what the shell is holding (THitActor::
			// mActorType, at 0x4C).
			if (unk80 != nullptr
			    && !(unk80->mLiveFlag & LIVE_FLAG_DEAD)) {
				if ((unk80->mActorType - 0x2000) <= 0x10) {
					if (gpMSound->gateCheck(0x483F))
						MSoundSESystem::MSoundSE::startSoundSystemSE(
						    0x483F, 0, nullptr, 0);
				} else if (gpMSound->gateCheck(0x4813)) {
					MSoundSESystem::MSoundSE::startSoundSystemSE(0x4813,
					                                           0, nullptr, 0);
				}
			} else if (gpMSound->gateCheck(0x483D)) {
				MSoundSESystem::MSoundSE::startSoundSystemSE(0x483D, 0,
				                                           nullptr, 0);
			}
			unk68 = 2;
		}
		break;
	case 2:
		if (unk7C <= 0) {
			unk68 = 3;
			if (gpMSound->gateCheck(0x389F))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x389F, &mPosition, 0, nullptr, 0, 4);
		}
		break;
	case 3:
		unk6C += unk70;
		if (unk6C <= -TShellCup::mShellDamageRot)
			unk88->mLiveFlag &= ~1u;
		if (unk6C >= 0.0f) {
			unk6C = 0.0f;
			unk68 = 0;
			unk88->mLiveFlag |= 1u;
		}
		break;
	}

	// Tail: slide the shell between its rest position (unk8C) and wherever
	// unk74's joint transform put it, then copy mPosition onto the shell it
	// is riding (unk88).
	// TODO: @4038 (the horizontal weight) and @4039 (the vertical weight)
	// are unnamed rodata constants; both are used as
	// `mPosition.axis = saved + weight * (joint.axis - saved)`, so the
	// structure is right but the two weights are still unknown.
	MtxPtr jnt = unk74->getModel()->getAnmMtx(0);
	f32 ox = unk8C[0][3];
	f32 oy = unk8C[1][3];
	f32 oz = unk8C[2][3];
	mPosition.x = ox + unkLerpX * (jnt[0][3] - ox);
	mPosition.y = jnt[1][3] - unkLerpY;
	mPosition.z = oz + unkLerpX * (jnt[2][3] - oz);
	unk88->mPosition = mPosition;
	// When unk48 is set the ROM builds an X-rotation matrix from unk6C on
	// the stack and calls the virtual at vtable + 0x14 on unk84 with it.
	// TODO: vtable + 0x14 is a single-MtxPtr virtual. TMapObjBase has two
	// candidates (changeObjMtx / setModelMtx) and this TU's __vt__ dump
	// does not disambiguate them, so the call is left out rather than
	// guessed -- emitting the wrong one would mismatch just as badly.
	if (unk48 != 0) {
		Mtx mtx;
		MsMtxSetRotX(mtx, unk6C);
		unk74->concatOnlyRotFromRight(unk74->getModel()->getAnmMtx(0), mtx,
		                              unk74->getModel()->getAnmMtx(0));
		// TODO: unk84-><vtable + 0x14>(&mtx);
	}
}

void TViking::roll()
{
	// The viking ride swings between the two ends of its arc; every state
	// accelerates unk144, and crossing an end plays the creak sound once.
	switch (mState) {
	case 1:
		unk144 *= unk150.y;
		unk144 -= unk138.y;
		unk148 += unk144;
		if (unk148 < 0.0f) {
			f32 speed = fabsf(unk144);
			if (gpMSound->gateCheck(0x38A0))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x38A0, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
			mState = 2;
		}
		if (unk148 > 180.0f) {
			unk148 -= 360.0f;
			f32 speed = fabsf(unk144);
			if (gpMSound->gateCheck(0x38A0))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x38A0, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
			mState = 4;
		}
		break;
	case 2:
		unk144 *= unk150.y;
		unk144 += unk138.y;
		unk148 += unk144;
		if (unk148 > 0.0f) {
			f32 speed = fabsf(unk144);
			if (gpMSound->gateCheck(0x38A0))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x38A0, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
			mState = 1;
		}
		if (unk148 < -180.0f) {
			unk148 += 360.0f;
			f32 speed = fabsf(unk144);
			if (gpMSound->gateCheck(0x38A0))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x38A0, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
			mState = 3;
		}
		break;
	case 3:
		unk144 *= unk150.z;
		unk144 -= unk138.y;
		unk148 += unk144;
		if (unk148 < 0.0f) {
			f32 speed = fabsf(unk144);
			if (gpMSound->gateCheck(0x38A0))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x38A0, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
			if (unk144 > -unk150.x)
				mState = 2;
			else
				mState = 4;
		}
		break;
	case 4:
		unk144 *= unk150.z;
		unk144 += unk138.y;
		unk148 += unk144;
		if (unk148 > 0.0f) {
			f32 speed = fabsf(unk144);
			if (gpMSound->gateCheck(0x38A0))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x38A0, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
			if (unk144 < unk150.x)
				mState = 1;
			else
				mState = 3;
		}
		break;
	}
}

void TViking::control()
{
	// unk14C picks the drive mode: 1 lets the ride swing freely, 0 just
	// parks it at whichever end it stopped on.
	switch (unk14C) {
	case 0:
		switch (mState) {
		case 1:
			unk144 -= unk138.y;
			unk148 += unk144;
			if (unk148 < 0.0f)
				mState = 2;
			break;
		case 2:
			unk144 += unk138.y;
			unk148 += unk144;
			if (unk148 > 0.0f)
				mState = 1;
			break;
		}
		break;
	case 1:
		roll();
		break;
	}
	f32 s = sinf(3.14f * (unk148 / 180.0f));
	mPosition.x = unk138.x * s + mInitialPosition.x;
	f32 yOff = mYOffset;
	f32 c = cosf(3.14f * (unk148 / 180.0f));
	mPosition.y = unk138.x * (1.0f - c) + mInitialPosition.y + yOff;
	mRotation.z = unk148;
	updateObjMtx();
}

void TAmiKing::calc()
{
	gpMarioParticleManager->emitAndBindToMtxPtr(0x184, getModel()->getAnmMtx(0),
	                                            1, this);
	if (unk138 == 0) {
		MtxPtr jointMtx = mMActor->getModel()->getAnmMtx(6);
		unk13C.set(jointMtx[0][3], jointMtx[1][3], jointMtx[2][3]);
		JGeometry::TVec3<f32> offset(0.0f, 0.0f, 200.0f);
		Mtx mtx;
		MsMtxSetRotRPH(mtx, 0.0f, mRotation.y, 0.0f);
		PSMTXMultVec(mtx, &offset, &offset);
		unk13C.x = unk13C.x + offset.x;
		unk13C.y = unk13C.y + offset.y;
		unk13C.z = unk13C.z + offset.z;
		JPABaseEmitter* emitter =
		    gpMarioParticleManager->emitAndBindToPosPtr(0x124, &unk13C, 1,
		                                              this);
		if (emitter) {
			emitter->mGlobalDynamicsScale.set(2.0f, 2.0f, 2.0f);
			emitter->mGlobalParticleScale.set(2.0f, 2.0f, 2.0f);
		}
		if (gpMSound->gateCheck(0x214F))
			MSoundSESystem::MSoundSE::startSoundActor(0x214F, &mPosition, 0,
			                                          nullptr, 0, 4);
	} else {
		if (gpMSound->gateCheck(0x2120))
			MSoundSESystem::MSoundSE::startSoundActor(0x2120, &mPosition, 0,
			                                          nullptr, 0, 4);
	}
}

void TViking::reset()
{
	unk144 = unk138.z;
	unk148 = 0.0f;
	if (unk138.z > 0.0f)
		mState = 1;
	else
		mState = 2;
}

void TViking::initMapObj()
{
	unk14C = 1;
	if (strcmp(getName(), "viking 0") == 0) {
		unk138.x = 1400.0f;
		unk138.y = 0.001f;
		unk150.y = 1.001f;
		unk150.z = 0.999f;
		unk138.z = -0.3f;
		unk150.x = 0.3f;
	} else {
		unk138.x = 1400.0f;
		unk138.y = 0.001f;
		unk150.y = 1.001f;
		unk150.z = 0.999f;
		unk138.z = 0.3f;
		unk150.x = 0.3f;
	}
	mPosition.y -= unk138.x;
	TMapObjBase::initMapObj();
}

void TAmiKing::initMapObj()
{
	TMapObjBase::initMapObj();
	initAnmSound();
	mMActor->setBck("amiking_sleep1");
	setAnmSound("/scene/mapObj/amiking_sleep1.bas");
	offLiveFlag(LIVE_FLAG_UNK10);
	for (u8 i = 0; i < mMActor->getModel()->getModelData()->getJointNum();
	     ++i) {
	}
}

void TBalloonKoopaJr::kill()
{
	// Frame-padding: target frame is 8 bytes larger (MWCC stack-padding quirk).
	char framePad_8_kill[8];
	(void)framePad_8_kill;
	TMapObjGeneral::kill();
	emitAndScale(0x5A, 0, &unk148);
	emitAndScale(0x5B, 0, &unk148);
	emitAndScale(0x5C, 0, &unk148);
	TFlagManager::getInstance()->incFlag(0x60001, 1);
	if (gpMSound->gateCheck(0x28B8))
		MSoundSESystem::MSoundSE::startSoundActor(0x28B8, &mPosition, 0,
		                                          nullptr, 0, 4);
}

void TBalloonKoopaJr::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	SMS_LoadParticle("/scene/mapObj/balloonKoopaJr.jpa", 0x5A);
	SMS_LoadParticle("/scene/mapObj/balloonKoopaJrA.jpa", 0x5B);
	SMS_LoadParticle("/scene/mapObj/balloonKoopaJrB.jpa", 0x5C);
	s32 joint = getModel()->getModelData()->getJointName()->getIndex("center");
	MtxPtr mtx = getModel()->getAnmMtx((u16)joint);
	unk148.set(mtx[0][3], mtx[1][3], mtx[2][3]);
}

TShellCup::TShellCup(const char* name)
    : TMapObjBase(name)
{
	unk498 = nullptr;
	unk49C = nullptr;
	unk4A0 = nullptr;
}

void TShellCup::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TMapObjBase::perform(cue, graphics);
	if (!(cue & 2))
		return;
	bool flag1 = gpMarDirector->unk124 == 1 || gpMarDirector->unk124 == 2;
	if (flag1) {
		bool flag2 = gpMarDirector->unk124 == 3 || gpMarDirector->unk124 == 4;
		if (!flag2)
			return;
	}
	for (int i = 0; i < 6; ++i) {
		Mtx mtx;
		MsMtxSetRotX(mtx, unk138[i].unk6C);
		// The ROM does `lwz r3, 0x74(r29)` and passes r3 straight
		// through as the concatOnlyRotFromRight receiver/argument, i.e.
		// the shell's stored pointer is handed over verbatim.
		concatOnlyRotFromRight(*(MtxPtr*)&unk138[i].unk74, mtx,
		                       *(MtxPtr*)&unk138[i].unk74);
	}
	if (!(unk498->mLiveFlag & LIVE_FLAG_DEAD)) {
		unk498->mPosition.x = unk138[0].mPosition.x;
		unk498->mPosition.y = unk138[0].mPosition.y;
		unk498->mPosition.z = unk138[0].mPosition.z;
	}
	if (!(unk49C->mLiveFlag & LIVE_FLAG_DEAD)) {
		unk49C->mPosition.x = unk138[2].mPosition.x;
		unk49C->mPosition.y = unk138[2].mPosition.y;
		unk49C->mPosition.z = unk138[2].mPosition.z;
	}
	if (!(unk4A0->mLiveFlag & LIVE_FLAG_DEAD)) {
		unk4A0->mPosition.x = unk138[4].mPosition.x;
		unk4A0->mPosition.y = unk138[4].mPosition.y;
		unk4A0->mPosition.z = unk138[4].mPosition.z;
	}
}

u32 TFerrisWheel::becomeCalmlyCallback(u32 arg1, u32 arg2)
{
	if (arg1 == 0) {
		mState = 2;
		MSound* sound = gpMSound;
		if (sound->unk80 != nullptr) {
			sound->unk80->setVolume(0.0f, 200, 0);
			sound->unk80->setPitch(0.5f, 200, 0);
		}
		mStateTimer = 0x78;
	}
	return 0;
}

BOOL TPinnaShell::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		gpMarioParticleManager->emit(0xE7, &sender->mPosition, 0, nullptr);
		gpMSound->startSoundSet(0x6802, &mPosition, 0, 0.0f, 0, 0, 4);
		if (unk68 == 0) {
			unk6C -= TShellCup::mWaterOpenAccel;
			if (unk6C < -TShellCup::mOpenRotMax)
				unk68 = 1;
		}
		return TRUE;
	}
	return FALSE;
}
