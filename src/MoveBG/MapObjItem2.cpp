#include <algorithm>
#include <MoveBG/MapObjItem2.hpp>
#include <M3DUtil/MActor.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <Player/MarioAccess.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <System/EmitterViewObj.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
// The .rodata order fixes these three: retail's blob is the DummyStrings zero
// object and no-memory message, then setUpTrans's (Vec){0,0,0}/(Vec){1,1,1}
// literals from MapCollisionEntry.hpp, then the four mtx-calc names (c-k29).
#include <System/DummyMactorString.hpp>
#include <System/DummyStrings.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// Parked: a binding level over TMarDirector's frame counter. The map shows no
// such accessor, so it stays TU-local; `TMushroom1up::perform` needs exactly
// this one binding expansion (+16 of low region) on top of the two
// `getStateTimer()` reads (+8 together) to reach retail's 0x40 frame. Header
// batch item: a binding `TMarDirector::getFrameCounter()` would serve it.
static inline int MapObjItem2GetDirectorFrame(TMarDirector* director)
{
	int frame = director->unk58;
	return frame;
}

TMushroom1up::TMushroom1up(int param_1, const char* name)
    : TMapObjBase(name)
    , unk138(0)
    , unk139(param_1)
    , unk13A(0)
    , unk13C(0)
{
}

void TMushroom1up::touchPlayer(THitActor* param_1)
{
	if (unk13A == 1)
		return;

	if (!param_1->receiveMessage(this, HIT_MESSAGE_ATTACK))
		return;

	unk13C = 0;
	unk13A = 1;
	SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_1UP, 0, nullptr, 0);
	mGroundPlane = TMap::getIllegalCheckData();
}

void TMushroom1up::makeObjAppeared()
{
	TMapObjBase::makeObjAppeared();
	mStateTimer = 1200;
	unk138      = 0;
	unk13A      = 0;
	if (unk139 != 2)
		SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_1UP_APPEAR, 0, nullptr, 0);

	JPABaseEmitter* emitter = gpMarioParticleManager->emit(
	    PARTICLE_MS_ENM_DISAP_A_W, &mPosition, 0, nullptr);
	if (emitter)
		emitter->setGlobalScale(getScaling());

	emitter = gpMarioParticleManager->emit(PARTICLE_MS_ENM_DISAP_B, &mPosition,
	                                       0, nullptr);
	if (emitter)
		emitter->setGlobalScale(getScaling());
}

void TMushroom1up::initMapObj()
{
	TMapObjBase::initMapObj();
	mGravity = 0.35f;
	offLiveFlag(LIVE_FLAG_AIRBORNE | LIVE_FLAG_UNK10 | LIVE_FLAG_HIDDEN
	            | LIVE_FLAG_DEAD);
	if (unk139 == 2) {
		onLiveFlag(LIVE_FLAG_UNK10);
		makeObjAppeared();
	}
	mScaling.set(1.5f, 1.5f, 1.5f);
}

void TMushroom1up::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	offLiveFlag(LIVE_FLAG_AIRBORNE | LIVE_FLAG_UNK10);
}

// The C-style `s`/`deg` declarations at the top of the follow block and
// getPosition() in the `diff -= ...` line give retail's 0x88 frame and slots.
void TMushroom1up::control()
{
	TMapObjBase::control();

	if (unk13A == 1) {
		f32 s;
		f32 deg;
		int t = 180 - unk13C;
		if (t < 0) {
			kill();
			return;
		}

		JGeometry::TVec3<f32> pos = SMS_GetMarioPos();
		deg = 5.0f * t;
		// The y offset comes first: it owns the lower literal id
		// (@3312) than the 1.5f of the x/z lines.
		pos.y += 200.0f;
		pos.x += 1.5f * (50.0f * JMACos(deg));
		s = JMASin(deg);
		pos.z += 1.5f * (50.0f * s);
		mPosition = pos;

		mScaling.set(1.5f, 1.5f, 1.5f);
		mLinearVelocity.zero();
		mVelocity.zero();
		unk13C++;
		return;
	}

	unk13C++;
	if (unk139 == 2) {
		mLinearVelocity.zero();
		mVelocity.zero();
		return;
	}

	if (unk138 == 0) {
		if (isAirborne())
			return;
		unk138 = 1;
	}

	JGeometry::TVec3<f32> diff = SMS_GetMarioPos();
	diff -= getPosition();
	diff.y = 0.0f;
	if (diff.isZero())
		diff.x = 1.0f;
	if (unk139 == 1)
		diff.negate();

	f32 angle = MsGetRotFromZaxisY(diff);

	f32 delta = MsAngleDiff(angle, mRotation.y);
	f32 step;
	// std::min/std::max rather than MsClamp: the const-reference
	// parameters are what put 1.0f and -1.0f in .sdata, not .sdata2.
	if (delta > 0.0f)
		step = std::min(delta, 1.0f);
	else
		step = std::max(delta, -1.0f);

	mRotation.y = MsWrap(mRotation.y + step, 0.0f, 360.0f);

	VECNormalize(&diff, &diff);
	diff.scale(3.8f);
	mLinearVelocity.add(diff);
}

void TMushroom1up::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (unk139 != 2 && getStateTimer() < 240 && (cue & CUE_ENTRY)
	    && MapObjItem2GetDirectorFrame(gpMarDirector) % 6 > 2)
		cue &= ~CUE_ENTRY;

	if ((cue & CUE_MOVE) && unk13A == 0 && unk139 != 2 && getStateTimer() <= 0)
		kill();

	TMapObjBase::perform(cue, graphics);
}

TJumpBase::TJumpBase(const char* name)
    : TMapObjBase(name)
{
	unk138 = 2;
}

void TJumpBase::initMapObj()
{
	TMapObjBase::initMapObj();
	if (mMapCollisionManager) {
		TMapCollisionBase* base = mMapCollisionManager->getUnk8();
		base->setAllBGType(7);
		base->setAllActor(this);
		base->setAllData(0x2710);
	}
	unkE8 = 0;
}

void TJumpBase::ensureTakeSituation()
{
	if (mHeldObject && mHeldObject->mHolder != this)
		mHeldObject = nullptr;
	if (mHolder && mHolder->mHeldObject != this)
		mHolder = nullptr;
}

BOOL TJumpBase::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->isActorType(0x80000001)) {
		if (message == HIT_MESSAGE_TAKE) {
			if (unk138 == 0) {
				mHolder = (TTakeActor*)sender;
				onHitFlag(HIT_FLAG_NO_COLLISION);
				if (mMapCollisionManager && mMapCollisionManager->unk8)
					mMapCollisionManager->unk8->remove();
				return TRUE;
			}
		} else if (message == HIT_MESSAGE_UNK8) {
			mHolder = nullptr;
			unk13C  = 0;
			unk138  = 2;
			return TRUE;
		} else if (message == HIT_MESSAGE_PUT) {
			mHolder = nullptr;
			unk13C  = 0;
			unk138  = 2;
			return TRUE;
		} else if (message == HIT_MESSAGE_THROWN) {
			mHolder = nullptr;
			unk13C  = 0;
			unk138  = 5;
			return TRUE;
		} else if (message == HIT_MESSAGE_TRAMPLE) {
			unk13C = 0;
			unk138 = 4;
			return TRUE;
		}
	}

	if (sender->isActorType(0x1000001) && unk138 == 3) {
		unk13C = 0;
		unk138 = 1;
		return TRUE;
	}

	return FALSE;
}

Mtx* TJumpBase::getRootJointMtx() const
{
	return (Mtx*)mMActor->getModel()->getAnmMtx(0);
}

void TJumpBase::calcRootMatrix()
{
	if (getHolder() != nullptr) {
		J3DModel* model = getModel();
		MtxPtr mtx      = getHolder()->getTakingMtx();
		model->setBaseTRMtx(mtx);
		model->setBaseScale(mScaling);
		mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);
		return;
	}
	TMapObjBase::calcRootMatrix();
}

// TODO: 99.9%, every instruction and register exact; only the frame is left
// (0x90 against 0x88): retail's (f32)getEnd() conversion slot is at 0x70
// (ours 0x78) and its copy of mVelocity is an unnamed 12-byte object at 0x54,
// right below case 5's vector temporary (0x60, which matches), with one 4-byte
// object between the conversion slot and the vector; our named `v2` sits in
// the named region at 0x6c. Measured on this body: an unnamed
// `TVec3<f32>(mVelocity)` or `add(TVec3(mVelocity))` (0x90, both vectors 0xc
// high: SMS_GetMarioAngleY's inlined trig leaves four dead 4-byte F/P
// temporaries at 0x18-0x24 where retail has room for one), a named or unnamed
// copy of getVelocity() (0x98), and `+= getVelocity()` (7 instructions short).
// Debugger layout (c-k4): `int angle = SMS_GetMarioAngleY();`, then
// `mVelocity = TVec3<f32>(JMASSin(angle), 0.0f, JMASCos(angle));` and
// `mPosition += TVec3<f32>(mVelocity);` gives retail's frame and every slot
// exactly (conversion 0x70, vector 0x60, copy 0x54; the accessor makes
// `angle` an IR temporary with no named slot, and the trig inlines then leave
// one dead word where two accessor calls leave four) -- but the IR optimiser
// merges the two index trees again (99.3, retail's second `sraw`/`slwi`
// missing). So retail's source has the one-read slot layout *and* two index
// derivations. Inert or worse on that: `s16`/`u16`/`const int`/`f32 angle`,
// named `c`/`s` results in either order (instructions exact with two accessor
// calls, but +8 frame from their split slots), one named result, mixed
// accessor/raw arguments, and a named `vel` vector.
// History:
// (1) `this`/pool-base swap (r31/r29, retail r29/r31): closed by c-k4 with raw
//     `mMActor` at the thirteen getMActor() sites. Debugger reading: `this`
//     and `prevState` reach the second simplify sweep at remaining degree 30,
//     15 of it coalesced ghost webs (each getMActor() result bound into r3),
//     so both are blocked and coloured before the pool base; with two fewer
//     ghosts they are pushed ahead of it and the pool base takes r31.
//     Earlier trials: `prevState` below the isAirborne block (96.5), a
//     function-scope `ctrl` (inert).
// (2) case 5's sine/cosine index: retail derives it twice (two `sraw` from one
//     `lha`/`clrlwi`). Closed by c-k4 with `SMS_GetMarioAngleY()` at both
//     calls, as TMapObjGeneral does: two inline results are not merged by the
//     IR optimiser and the backend CSE merges only the loads and the `clrlwi`.
//     A named `int`/`s16 angle` or `*gpMarioAngleY` twice merges the whole
//     index (95.9-97.2); TU-local JMASSin/JMASCos wrappers and
//     `mVelocity.set(...)` were inert or worse.
// Closure batch 128 restored the ground-plane `unk13C = 0; unk138 = 2;` pair
// at the end of the function (not case 5).
// c-k29: case 5 is byte-exact (frame 0x88, every slot on retail's) with
//   s16 sinAngle = SMS_GetMarioAngleY();
//   s16 cosAngle = SMS_GetMarioAngleY();
//   mVelocity = TVec3<f32>(JMASSin(sinAngle), 0.0f, JMASCos(cosAngle));
//   mPosition += TVec3<f32>(mVelocity);
// hsearch dbg: the two s16 locals are homed and packed into the one word
// retail has between the conversion slot and the vector (0x6c-0x70), and two
// separate reads keep the two index derivations that one named angle merges.
// With it the unit links (DOL SHA-1 unchanged, symbol order PASS). Not
// applied: two locals holding the same angle may read as a dummy copy, so it
// waits for an owner decision. One named angle, a reference or pointer to
// gpMarioAngleY, or a named angle mixed with one accessor are all worse.
void TJumpBase::control()
{
	int prevState = unk138;
	if (!isAirborne())
		onLiveFlag(LIVE_FLAG_UNK10);

	switch (unk138) {
	case 0:
		if (unk13C == 0) {
			mMActor->setBck("jumpbase_shrink");
			J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
			if (ctrl) {
				ctrl->setFrame((f32)ctrl->getEnd());
				ctrl->setRate(0.0f);
			}
			mScaledBodyRadius = 50.0f;
		}
		break;

	case 3:
		if (unk13C == 0) {
			offHitFlag(HIT_FLAG_NO_COLLISION);
			if (mMapCollisionManager)
				mMapCollisionManager->getUnk8()->setUp();

			mMActor->setBck("jumpbase_set");
			J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
			if (ctrl) {
				ctrl->setFrame((f32)ctrl->getEnd());
				ctrl->setRate(0.0f);
			}
		}
		break;

	case 2:
		if (unk13C == 0) {
			mMActor->setBck("jumpbase_set");
			J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
			if (ctrl) {
				ctrl->setFrame(0.0f);
				ctrl->setRate(SMSGetAnmFrameRate());
			}
			offLiveFlag(LIVE_FLAG_UNK10);
			mScaledBodyRadius = 100.0f;
		}
		if (mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
			unk13C = 0;
			unk138 = 3;
		}
		break;

	case 1:
		if (unk13C == 0) {
			if (mMapCollisionManager && mMapCollisionManager->getUnk8())
				mMapCollisionManager->getUnk8()->remove();

			mMActor->setBck("jumpbase_shrink");
			J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
			if (ctrl) {
				ctrl->setFrame(0.0f);
				ctrl->setRate(SMSGetAnmFrameRate());
			}
		}
		if (mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
			unk13C = 0;
			unk138 = 0;
		}
		break;

	case 4:
		if (unk13C == 0) {
			mMActor->setBck("jumpbase_jump");
			J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
			if (ctrl) {
				ctrl->setFrame(0.0f);
				ctrl->setRate(SMSGetAnmFrameRate());
			}
		}
		if (mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
			unk13C = 0;
			unk138 = 3;
		}
		break;

	case 5:
		if (unk13C == 0) {
			onLiveFlag(LIVE_FLAG_AIRBORNE);
			mVelocity = JGeometry::TVec3<f32>(JMASSin(SMS_GetMarioAngleY()), 0.0f,
			                                  JMASCos(SMS_GetMarioAngleY()));
			JGeometry::TVec3<f32> v2 = mVelocity;
			mPosition += v2;
			offLiveFlag(LIVE_FLAG_UNK10);
		}
		if (!isAirborne()) {
			unk13C = 0;
			unk138 = 2;
		}
		break;
	}

	if (unk138 == prevState) {
		unk13C++;
		if (unk13C == 0)
			unk13C = 1;
	}

	TMapObjBase::control();

	if (mGroundPlane) {
		if (mGroundPlane->isIllegalData() || mGroundPlane->isWaterSurface()) {
			makeObjDead();
			makeObjDefault();
			makeObjAppeared();
			unk13C = 0;
			unk138 = 2;
		}
	}

	if (mHolder) {
		mGroundPlane  = SMS_GetMarioGroundPlane();
		mGroundHeight = SMS_GetMarioPos().y;
	}
}
