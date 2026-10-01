#include <MoveBG/MapObjRicco.hpp>

#include <System/EmitterViewObj.hpp>
#include <System/MarDirector.hpp>
#include <MoveBG/ItemManager.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/MapObjBall.hpp>
#include <MoveBG/MapObjGeneral.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <PowerPC_EABI_Support/Msl/MSL_C/MSL_Common/string.h>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// Local statics of this TU. Both have a non-trivial constructor, so MWCC
// zero-allocates them in .bss and fills them in from __sinit_MapObjRicco_cpp.
// They must be declared before the MSound headers above so that they land
// ahead of the JALList smList objects in .bss, as in the retail object.
// TODO: submarineSetWtPos_forSound is initialised but never read anywhere in
// this TU; the original presumably kept it around for an unfinished feature.
static JGeometry::TVec3<f32> submarineCranePos_forSound(1956.0f, 1000.0f,
                                                        6425.0f);
static JGeometry::TVec3<f32> submarineSetWtPos_forSound(1956.0f, -100.0f,
                                                        6425.0f);

// NOTE: this unit is reverse_fn_order (per tools/validate-symbol-order.py):
// with -inline deferred, MWCC emits functions in the reverse of source
// order, so definitions below are ordered backwards from the retail
// address order on purpose. Keep new functions in that scheme.

// TCraneRotY ----------------------------------------------------------------

u32 TCraneRotY::mWaitTime = 120;

void TCraneRotY::calc() { setRootMtxRotY(); }

void TCraneRotY::control()
{
	TMapObjBase::control();

	switch (mState) {
	case STATE_ROTATE_UP:
		mRotation.y += unk144;

		if (mRotation.y > unk138 + unk140) {
			startStateTimer(mWaitTime);
			setState(STATE_WAIT_DOWN);
		}
		break;

	case STATE_WAIT_UP:
		if (!isStateTimerEngaged())
			setState(STATE_ROTATE_UP);
		break;

	case STATE_ROTATE_DOWN:
		mRotation.y -= unk144;

		if (mRotation.y < unk138 + unk13C) {
			startStateTimer(mWaitTime);
			setState(STATE_WAIT_UP);
		}
		break;

	case STATE_WAIT_DOWN:
		if (!isStateTimerEngaged())
			setState(STATE_ROTATE_DOWN);
		break;
	}

	// TODO: the rotated branch goes to STATE_WAIT_DOWN and the reversed one to
	// STATE_WAIT_UP, i.e. the state names above are certainly swapped relative
	// to whatever the original author called them.
	if (isState(STATE_ROTATE_UP) || isState(STATE_ROTATE_DOWN)) {
		u32 sound = unk148;

		if (gpMSound->gateCheck(sound))
			MSoundSESystem::MSoundSE::startSoundActor(sound, &mPosition, 0,
			                                         nullptr, 0, 4);
	}
}

void TCraneRotY::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);

	stream.read(&unk140, 4);

	unk138 = mRotation.y;

	// TODO: unverified random range constants (l, r)
	unk144 = MsRandF(0.0f, 2.0f);

	// TODO: unverified name comparison string
	if (!strcmp(getName(), "Ｙ軸回転クレーンＢ"))
		unk148 = MSD_SE_OBJ_CRANE_SIDEMOVE1;
	else
		unk148 = MSD_SE_OBJ_CRANE_SIDEMOVE2;

	mState = 0;
}

// TCraneCargo -----------------------------------------------------------

void TCraneCargo::control()
{
	unk158.z = 0.0f;
	unk158.y = 0.0f;
	unk158.x = 0.0f;
	TMapObjBase::control();
}

void TCraneCargo::calc()
{
	updateRootMtxTrans();
	calcLeanMtx(getModel()->getAnmMtx(1));
}

// TCraneUpDown -----------------------------------------------------------

f32 TCraneUpDown::mRotSpeed = 0.1f;
u32 TCraneUpDown::mWaitTime = 120;

void TCraneUpDown::control()
{
	TMapObjBase::control();

	switch (mState) {
	case STATE_WAIT_UP:
		if (!isStateTimerEngaged())
			setState(STATE_ROTATE_UP);
		break;

	case STATE_ROTATE_UP:
		mRotation.x += mRotSpeed;

		if (mRotation.x > unk140) {
			startStateTimer(mWaitTime);
			setState(STATE_WAIT_DOWN);
		}
		break;

	case STATE_WAIT_DOWN:
		if (!isStateTimerEngaged())
			setState(STATE_ROTATE_DOWN);
		break;

	case STATE_ROTATE_DOWN:
		mRotation.x -= mRotSpeed;

		if (mRotation.x < unk144) {
			startStateTimer(mWaitTime);
			setState(STATE_WAIT_UP);
		}
		break;
	}

	// The cargo hangs off the crane at a fixed local offset. That offset is
	// rotated by the crane's X and Y angles and then added to the crane's own
	// position, so the cargo swings along with it.
	// TODO: the matrix maths is a hand-rolled pair of rotations with no
	// translation; reconstructed straight from the asm and its intent is
	// not obvious. Neither sin/cos pair is reused between the two.
	unk138->mPosition.set(0.0f, 0.0f, 1500.0f);

	MtxPtr modelMtx = getModel()->getAnmMtx(0);

	Mtx mtx;
	MTXIdentity(mtx);

	s32 xIndex = static_cast<s32>(182.04445f * mRotation.x);
	xIndex = static_cast<u16>(xIndex) >> jmaSinShift;
	f32 xSin = jmaSinTable[xIndex];
	f32 xCos = jmaCosTable[xIndex];

	modelMtx[0][0] = 1.0f;
	modelMtx[0][1] = 0.0f;
	modelMtx[0][2] = 0.0f;
	modelMtx[0][3] = 0.0f;

	modelMtx[1][0] = 0.0f;
	modelMtx[1][1] = xCos;
	modelMtx[1][2] = -xSin;
	modelMtx[1][3] = 0.0f;

	modelMtx[2][0] = 0.0f;
	modelMtx[2][1] = xSin;
	modelMtx[2][2] = xCos;
	modelMtx[2][3] = 0.0f;

	s32 yIndex = static_cast<s32>(182.04445f * mRotation.y);
	yIndex = static_cast<u16>(yIndex) >> jmaSinShift;
	f32 ySin = jmaSinTable[yIndex];
	f32 yCos = jmaCosTable[yIndex];

	mtx[0][0] = yCos;
	mtx[0][1] = 0.0f;
	mtx[0][2] = ySin;
	mtx[0][3] = 0.0f;

	mtx[1][0] = 0.0f;
	mtx[1][1] = 1.0f;
	mtx[1][2] = 0.0f;
	mtx[1][3] = 0.0f;

	mtx[2][0] = -ySin;
	mtx[2][1] = 0.0f;
	mtx[2][2] = yCos;
	mtx[2][3] = 0.0f;

	MTXConcat(mtx, modelMtx, modelMtx);
	MTXMultVec(modelMtx, &unk138->mPosition, &unk138->mPosition);

	unk138->mPosition.x += mPosition.x;
	unk138->mPosition.y += mPosition.y - mYOffset + unk138->mYOffset;
	unk138->mPosition.z += mPosition.z;

	if (isState(STATE_ROTATE_DOWN) || isState(STATE_ROTATE_UP)) {
		u32 sound = unk13C;

		if (gpMSound->gateCheck(sound))
			MSoundSESystem::MSoundSE::startSoundActor(sound, &mPosition, 0,
			                                         nullptr, 0, 4);
	}
}

// The cargo hangs off the crane and is driven along the crane's X axis; the
// "craneUpDown 0" variant is the one that spawns the extra object.
// TODO: the object name and the sound IDs (0x3036/0x3037) were read straight
// off the .rodata / call sites, nothing in the binary names them.
void TCraneUpDown::initMapObj()
{
	TMapObjBase::initMapObj();

	mMapCollisionManager->getUnk8()->setAllActor(nullptr);

	unk138 = TMapObjBaseManager::newAndRegisterObj("craneCargoUpDown");
	unk138->appear();

	if (strcmp(getName(), "craneUpDown 0") == 0) {
		unk144 = -25.0f;
		unk140 = 45.0f;
		unk13C = 0x3036;
	} else {
		unk144 = -25.0f;
		unk140 = 30.0f;
		unk13C = 0x3037;
	}

	mRotation.x = MsRandF(unk144, unk140);
}

// TRiccoWatermill -----------------------------------------------------------

// Values read out of the retail .data, not chosen: dumped the target's bytes
// for each symbol with objdiff-cli and unpacked them big-endian.
f32 TRiccoWatermill::mRotAccel              = 1.0f;
f32 TRiccoWatermill::mRotSpeedMaxUp         = 3.0f;
f32 TRiccoWatermill::mRotSpeedMaxDown       = 1.0f;
f32 TRiccoWatermill::mRotDown               = 0.05f;
f32 TRiccoWatermill::mSubmarineMoveRate     = 0.5f;
f32 TRiccoWatermill::mSubmarineMaxTransY    = 750.0f;
f32 TRiccoWatermill::mSubmarineBottomTransY = -950.0f;
u32 TRiccoWatermill::mWaitTime              = 600;
f32 TRiccoWatermill::mSubmarineSurfaceTransY;

u32 TRiccoWatermill::touchWater(THitActor*)
{
	if (isState(STATE_STOPPED))
		return TRUE;

	unk140 = 5;

	if (isState(STATE_RISE))
		mPartner->setUpMapCollision(1);

	offMapObjFlag(MAP_OBJ_FLAG_UNK100);
	mPartner->offMapObjFlag(MAP_OBJ_FLAG_UNK100);

	if (mPartner->mPosition.y < mSubmarineMaxTransY) {
		unk138 += mRotAccel;

		if (unk138 > mRotSpeedMaxUp)
			unk138 = mRotSpeedMaxUp;

		mState = STATE_TOP_WAIT;
	} else {
		unk138 = 0.0f;
	}

	return TRUE;
}

void TRiccoWatermill::calc() { setRootMtxRotZ(); }

// States 2, 3 and 4 are the ones in which the mill actually turns: the mill
// spins by -unk138 and drags the partner upwards at mSubmarineMoveRate. The
// other three only react to the partner reaching one of its two end stops.
void TRiccoWatermill::control()
{
	TMapObjBase::control();

	if (isState(STATE_TOP_WAIT) || isState(STATE_TOP)
	    || isState(STATE_BOTTOM)) {
		if (unk138 != 0.0f) {
			mRotation.z -= unk138;

			if (gpMSound->gateCheck(0x3031))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x3031, &mPosition, nullptr, fabsf(unk138), 0, 0,
				    &unk14C, 0, 4);

			f32 rise = mSubmarineMoveRate * unk138;
			mPartner->mPosition.y += rise;

			if (gpMSound->gateCheck(0x3030))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x3030, &submarineCranePos_forSound, nullptr,
				    fabsf(rise), 0, 0, &unk150, 0, 4);

			if (gpMSound->gateCheck(0x3023))
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x3023, &submarineCranePos_forSound, nullptr,
				    fabsf(rise), 0, 0, &unk154, 0, 4);
		}

		// unk140 counts down a delay before the mill is allowed to slow back
		// down towards zero.
		if (unk140 == 0) {
			unk138 -= mRotDown;

			if (unk138 < -mRotSpeedMaxDown)
				unk138 = -mRotSpeedMaxDown;
		} else {
			unk140--;
		}
	}

	// The case labels are deliberately *not* in ascending state order: MWCC
	// emits the bodies in source order, and the retail object lays them out as
	// STATE_TOP_WAIT, STATE_TOP, STATE_BOTTOM.
	switch (mState) {
	case STATE_TOP_WAIT:
		if (mPartner->mPosition.y > mSubmarineMaxTransY) {
			mPartner->mPosition.y = mSubmarineMaxTransY;

			if (!isStateTimerEngaged()) {
				if (gpMSound->gateCheck(0x3832))
					MSoundSESystem::MSoundSE::startSoundActor(
					    0x3832, &mPartner->mPosition, 0, nullptr, 0, 4);

				// Only once per cycle the coin gets thrown out of the tank.
				if (!unk144) {
					throwObjToFrontFromPoint(
					    unk148,
					    JGeometry::TVec3<f32>(
					        2008.0f, mSubmarineMaxTransY + 500.0f, 7066.0f),
					    7066.0f, 20.0f);
					unk144 = 1;
				}
			}
		}

		if (unk138 < 0.0f)
			setState(unk144 ? STATE_TOP : STATE_BOTTOM);
		break;

	case STATE_STOPPED:
		break;

	case STATE_RISE:
		break;

	case STATE_TOP:
		if (mPartner->mPosition.y <= mSubmarineSurfaceTransY) {
			mPartner->mPosition.y = mSubmarineSurfaceTransY;
			mPartner->setUpMapCollision(0);
			unk138 = 0.0f;

			if (gpMSound->gateCheck(0x3832))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x3832, &mPartner->mPosition, 0, nullptr, 0, 4);

			startStateTimer(mWaitTime);
			setState(STATE_STOPPED);
		}
		break;

	case STATE_BOTTOM:
		if (mPartner->mPosition.y <= mSubmarineBottomTransY) {
			mPartner->mPosition.y = mSubmarineBottomTransY;
			mPartner->setUpMapCollision(0);
			unk138 = 0.0f;
			mPartner->onMapObjFlag(MAP_OBJ_FLAG_UNK100);
			onMapObjFlag(MAP_OBJ_FLAG_UNK100);

			if (gpMSound->gateCheck(0x3833))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x3833, &mPartner->mPosition, 0, nullptr, 0, 4);

			setState(STATE_RISE);
		}
		break;
	}
}


// The partner (a fake "blue coin", i.e. the submarine's payload marker) and the
// object's own shadow/animation partner are looked up by name at load time.
// The partner is parked below the map and its collision is rebuilt from
// scratch so it starts out hidden under the water.
void TRiccoWatermill::loadAfter()
{
	TMapObjBase::loadAfter();

	mPartner = (TMapObjBase*)JDrama::TNameRefGen::search("submarine");
	unk148 = (TMapObjBase*)JDrama::TNameRefGen::search("青コイン（潜水艦用）");

	unk148->makeObjDead();

	mPartner->mPosition.y = mSubmarineBottomTransY;
	mPartner->removeMapCollision();
	mPartner->setUpCurrentMapCollision();
}

TRiccoWatermill::TRiccoWatermill(const char* name)
    : TMapObjBase(name)
    , unk138(0.0f)
    , mPartner(0)
    , unk140(0)
    , unk144(0)
    , unk148(0)
    , unk14C(0)
    , unk150(0)
    , unk154(0)
{
}

// TFruitSwitch --------------------------------------------------------------

BOOL TFruitSwitch::receiveMessage(THitActor* sender, u32 message)
{
	if (message == 1) {
		startBck("riccoswitch");
		onHitFlag(HIT_FLAG_NO_COLLISION);

		if (mMapCollisionManager->unk8)
			mMapCollisionManager->unk8->remove();

		mLauncher->fireObj();
		return TRUE;
	}

	return FALSE;
}

TCraneUpDown::~TCraneUpDown() { }

TSurfGesoObj::~TSurfGesoObj() { }

void TSurfGesoObj::initMapObj()
{
	TMapObjBase::initMapObj();

	if (strcmp(getUnkF4(), "SurfGesoRed") == 0) {
		mTevColor.r = 0xFF;
		mTevColor.g = 0xB4;
		mTevColor.b = 0xFF;
		mTevColor.a = 0xFF;
	} else if (strcmp(getUnkF4(), "SurfGesoYellow") == 0) {
		mTevColor.r = 0xFF;
		mTevColor.g = 0xFF;
		mTevColor.b = 0x7D;
		mTevColor.a = 0xFF;
	} else if (strcmp(getUnkF4(), "SurfGesoGreen") == 0) {
		mTevColor.r = 0xB4;
		mTevColor.g = 0xFF;
		mTevColor.b = 0xB4;
		mTevColor.a = 0xFF;
	}

	SDLModelData* modelData = gpMapObjManager->mSurfGessoModelData;
	MActorAnmData* anmData = gpMapObjManager->getMActorAnmData();
	mMActor = SMS_MakeMActorFromSDLModelData(modelData, anmData, 3);
	TMapObjBase::initPacketMatColor(getModel(), GX_TEVREG1, &mTevColor);
	mMActor->setBck("surfgeso_run1");
}

// Values read out of the retail .sdata, not chosen.
f32 TFruitLauncher::mObjSpeedXZ     = 1.0f;
f32 TFruitLauncher::mObjSpeedY      = 20.0f;
u32 TFruitLauncher::mFruitLiveTime  = 4800;

// One shot attempt: rolls a single 0..100 value and picks the matching one of
// the five equally-sized bands, spawning that fruit kind at our own position.
// Returns null when the pool had nothing to hand out.
// TODO: the five 0x4000039x ids are read straight off the `lis r4, 0x4000` /
// `addi r4, r4, 0x390..0x394` pairs; nothing in the binary names them.
static TMapObjBase* pickFruit(const JGeometry::TVec3<f32>& pos)
{
	f32 roll = MsRandF() * 100.0f;

	u32 id;

	if (roll < 20.0f)
		id = 0x40000390;
	else if (roll < 40.0f)
		id = 0x40000391;
	else if (roll < 60.0f)
		id = 0x40000392;
	else if (roll < 80.0f)
		id = 0x40000393;
	else
		id = 0x40000394;

	return gpItemManager->makeObjAppear(pos.x, pos.y, pos.z, id, false);
}

// Picks one of the five fruit kinds at random and throws it out of the tank.
// Each attempt draws a fresh 0..100 roll; the five ids are laid out at
// 0x40000390 + n and cover 20% of the range each, the last one taking whatever
// is left over. A null result (nothing in the pool) simply rolls again, three
// times, before giving up.
// TODO: the five 0x4000039x ids are read straight off the `lis r4, 0x4000` /
// `addi r4, r4, 0x390..0x394` pairs; nothing in the binary names them. They are
// MSD-style ids but the low half has no enum entry yet.
void TFruitLauncher::fireObj()
{
	// Puffs at the mouth of the launcher and clunks twice as the fruit drops.
	gpMarioParticleManager->emitAndBindToPosPtr(0x11, &mPosition, 0, 0);

	if (gpMSound->gateCheck(MSD_SE_OBJ_AP_BUTTON))
		MSoundSESystem::MSoundSE::startSoundActor(
		    MSD_SE_OBJ_AP_BUTTON, &mPosition, 0, nullptr, 0, 4);

	if (gpMSound->gateCheck(MSD_SE_SMOKE_EFFECT))
		MSoundSESystem::MSoundSE::startSoundActor(
		    MSD_SE_SMOKE_EFFECT, &mPosition, 0, nullptr, 0, 4);

	// The two tank switches alternate: every shot fires from the other one.
	unk140 = unk140 ? 0 : 1;
	TMapObjBase* launcher = unk138[unk140];

	// Rewind the launcher's animation to its first frame and let it collide
	// again, so the fruit it just spat out can be hit.
	// TODO: the target stores 0.0f to offset 0x10 of the J3DFrameCtrl returned by
	// getFrameCtrl(0); J3DFrameCtrl is an incomplete type in this TU, so the store
	// is spelled out through the raw pointer for now.
	launcher->getMActor()->getFrameCtrl(0)->setFrame(0.0f);

	launcher->offHitFlag(HIT_FLAG_NO_COLLISION);

	// Re-pose the model after the rewind, then hand its node matrices to the
	// launcher so its collision follows the animation again.
	launcher->getModel()->calc();
	launcher->mMapCollisionManager->getUnk8()->setUpMtx(
	    launcher->getModel()->getAnmMtx(0));

	TMapObjBase* fruit = nullptr;

	if (!fruit)
		fruit = pickFruit(mPosition);

	if (!fruit)
		fruit = pickFruit(mPosition);

	if (!fruit)
		fruit = pickFruit(mPosition);

	if (!fruit)
		return;

	// Copied component-wise rather than as one TVec3 assignment: the target reads
	// and writes each float separately, which a struct copy does not reproduce.
	fruit->mPosition.x = mPosition.x;
	fruit->mPosition.y = mPosition.y;
	fruit->mPosition.z = mPosition.z;

	// Thrown up and out at a random angle around the vertical axis. Both rolls are
	// drawn before anything is stored, so the first feeds Z and the second X.
	// TODO: the target keeps the first roll alive in f30 across the second rand()
	// call, so its frame also spills f30 and is 0x28 bytes larger; nothing tried
	// here reproduces that extra callee-saved FP register.
	f32 vert  = mObjSpeedXZ * (MsRandF() * 0.5f - 0.5f);
	f32 horiz = mObjSpeedXZ * (MsRandF() * 0.5f - 0.5f);

	fruit->mVelocity.set(horiz, -mObjSpeedY, vert);
	fruit->offLiveFlag(4);

	TMarDirector* director = gpMarDirector;

	director->fireStartDemoCamera(
	    "riccoswitch", &fruit->mPosition, -1, 0.0f, true, nullptr, 0,
	    nullptr, JDrama::TFlagT<u16>(0));

	if (gpMSound->gateCheck(MSD_SE_SY_COLLECT_DELIGHT))
		MSoundSESystem::MSoundSE::startSoundSystemSE(
		    MSD_SE_SY_COLLECT_DELIGHT, 0, nullptr, 0);

	if (TMapObjBase::isFruit(fruit))
		((TResetFruit*)fruit)->killByTimer(mFruitLiveTime);
}

// The launcher spawns one of each of the five fruit models up front (as
// "infinite fruit" objects, flagged so they start out hidden) and then hands
// them out one at a time in fireObj(). The two tank switches are looked up by
// name; the launch animation is played on the first of them.
// TODO: the object/flag names are reconstructed, only "RiccoSwitch"/"riccoswitch"
// is a real string from the binary.
void TFruitLauncher::loadAfter()
{
	TMapObjBase::loadAfter();

	((TResetFruit*)TMapObjBaseManager::newAndRegisterObj("FruitCoconut"))
	    ->unk1A4 = 1;
	((TResetFruit*)TMapObjBaseManager::newAndRegisterObj("FruitDurian"))
	    ->unk1A4 = 1;
	((TResetFruit*)TMapObjBaseManager::newAndRegisterObj("FruitPapaya"))
	    ->unk1A4 = 1;
	((TResetFruit*)TMapObjBaseManager::newAndRegisterObj("FruitPine"))->unk1A4
	    = 1;
	((TResetFruit*)TMapObjBaseManager::newAndRegisterObj("FruitBanana"))
	    ->unk1A4 = 1;

	unk138[0] = (TMapObjBase*)JDrama::TNameRefGen::search("タンクスイッチＡ");
	// The ROM stores `this` into offset 0x138 of the resolved tank-switch
	// object, i.e. that object keeps a back-pointer to its owner.
	// TODO: TMapObjGeneral::unk138 is declared `const TBGCheckData*`, which
	// cannot hold a map object, so the real field type is still unverified.
	// Store through a void* rather than invent a header change here.
	*(void**)&((TMapObjGeneral*)unk138[0])->unk138 = this;

	unk138[1] = (TMapObjBase*)JDrama::TNameRefGen::search("タンクスイッチＢ");
	// same back-pointer write as above - see the TODO there
	*(void**)&((TMapObjGeneral*)unk138[1])->unk138 = this;

	unk140 = 1;

	unk138[0]->startBck("riccoswitch");
	((THitActor*)unk138[0])->onHitFlag(HIT_FLAG_NO_COLLISION);

	if (mMapCollisionManager->getUnk8())
		mMapCollisionManager->getUnk8()->remove();
}

TFruitLauncher::~TFruitLauncher() { }
