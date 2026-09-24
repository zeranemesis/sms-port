#include <MoveBG/MapObjRicco.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <PowerPC_EABI_Support/Msl/MSL_C/MSL_Common/string.h>

// NOTE: this unit is reverse_fn_order (per tools/validate-symbol-order.py):
// with -inline deferred, MWCC emits functions in the reverse of source
// order, so definitions below are ordered backwards from the retail
// address order on purpose. Keep new functions in that scheme.

// TCraneRotY ----------------------------------------------------------------

void TCraneRotY::calc() { setRootMtxRotY(); }

void TCraneRotY::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);

	stream.read(unk140);

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
	if (mState == 5)
		return TRUE;

	unk140 = 5;

	if (mState == 1)
		mPartner->setUpMapCollision(1);

	offMapObjFlag(MAP_OBJ_FLAG_UNK100);
	mPartner->offMapObjFlag(MAP_OBJ_FLAG_UNK100);

	if (mPartner->mPosition.y < mSubmarineMaxTransY) {
		unk138 += mRotAccel;

		if (unk138 > mRotSpeedMaxUp)
			unk138 = mRotSpeedMaxUp;

		mState = 2;
	} else {
		unk138 = 0.0f;
	}

	return TRUE;
}

void TRiccoWatermill::calc() { setRootMtxRotZ(); }

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
		// TODO: unverified animation name
		startBck("FruitSwitchPush");
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

TFruitLauncher::~TFruitLauncher() { }
