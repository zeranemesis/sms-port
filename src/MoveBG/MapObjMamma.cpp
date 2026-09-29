#include <MoveBG/MapObjMamma.hpp>

// rogue include: dummy string pair, needed to match the .rodata prologue
#include <System/DummyStrings.hpp>
#include <MoveBG/MapObjBall.hpp>
#include <MoveBG/ItemManager.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <MSound/MSound.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <System/MarDirector.hpp>
#include <M3DUtil/MActor.hpp>

// NOTE: this TU uses -inline deferred, so definitions are emitted in reverse
// source order; keep them in reverse of marioEU.MAP address order.

f32 TMapObjBall::getDepthAtFloating() { return unk18C; }

bool TSandBase::withering()
{
	mScaling.y -= unk13C;
	if (mScaling.y < mScaleMin)
		mScaling.y = mScaleMin;

	SMSGetMSound()->startSoundActor(0x2099, &unk144->mPosition, 0, nullptr, 0,
	                                4);
	// the ROM materialises this comparison into a bool instead of using the
	// CR bit, so the ternary form has to be spelled out
	return mScaling.y <= mScaleMin ? true : false;
}

TSandBase::TSandBase(const char* name)
	: TMapObjBase(name)
	, unk138(0.0f)
	, unk13C(0.0f)
	, unk144(nullptr)
{
}

void TSandBomb::makeObjAppeared()
{
	TMapObjBase::makeObjAppeared();
	startControlAnim(1);
	startControlAnim(2);
}

u32 TSandBomb::getSDLModelFlag() const
{
	return 0;
}

void TSandBomb::initMapObj() { TMapObjBase::initMapObj(); }

void TSandBombBase::withered()
{
	mStateTimer = unk140;
	mState      = 3;
	unk144->sleep();
}

void TSandBombBase::waitBeforeExplode()
{
	mState      = 6;
	mStateTimer = unk148;
}

void TSandBombBase::grow() { mState = 5; }

TSandBombBase::TSandBombBase(const char* name)
	: TSandBase(name)
	, unk148(0)
	, unk14C(1.0f)
	, unk150(0.0f)
	, unk154(0.0f)
{
}

void TSandCastle::calcRootMatrix()
{
	// The ROM materialises the state test into a bool before branching, so
	// this has to go through the isState() inline, not a plain compare.
	if (isState(STATE_SHRINKING))
		return;
	TMapObjBase::calcRootMatrix();
}

// TODO: the demo-camera shake callback body is not reconstructed; the map
// records it at 8 bytes, i.e. a bare "return 0".
static s32 startCameraShakeSE(u32 unused1, u32 unused2)
{
	return 0;
}

u32 TLeanMirror::getSDLModelFlag() const
{
	return 0;
}

TSandBird::TSandBird(const char* name)
	: TJointCoin(name)
	, unk150(0)
	, unk151(0)
{
}

u32 TWatermelonStatic::touchWater(THitActor* hit_actor)
{
	return 1;
}

void TGoalWatermelon::touchActor(THitActor* hit_actor)
{
	// TODO: the BCK/BRK/demo names used here are guesses -- the string table
	// around the "シャイン（お化けスイカ用）" literal in .rodata has not been
	// decoded yet, so only the surrounding code shape is trustworthy.
	// Both tests are nested ifs, not an && chain: the ROM materialises two
	// separate bools and tests them in sequence.
	if (isState(STATE_HIDDEN)) {
		if ((u32)(hit_actor->mActorType - 0x4000) <= 0xD0) {
			unk13C = static_cast<TMapObjBase*>(hit_actor);
			unk13C->getMActor()->setBck("watermelon_shrink");
			unk13C->getMActor()->setBtk("watermelon_shrink");
			unk13C->offMapObjFlag(MAP_OBJ_FLAG_UNK8);
			unk13C->setVelocity(JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f));
			JDrama::TFlagT<u16> flag = 0;
			gpMarDirector->fireStartDemoCamera(
			    "スイカシャインカメラ", &unk13C->mPosition, -1, 0.0f, true, 0,
			    0, 0, flag);
			mState = 2;
		}
	}
}

void TGoalWatermelon::control()
{
	TMapObjBase::control();
	switch (mState) {
	case 0:
	case 1:
		break;
	case 2:
		if (unk13C->animIsFinished()) {
			gpItemManager->makeShineAppearWithDemoOffset(
			    "シャイン（お化けスイカ用）", "スイカシャインカメラ", 0.0f,
			    0.0f, 0.0f);
			mState = 3;
		}
		break;
	default:
		break;
	}
}

void TGoalWatermelon::loadAfter()
{
	TMapObjBase::loadAfter();
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	unk138 = static_cast<TLiveActor*>(
	    JDrama::TNameRefGen::search("シャイン（お化けスイカ用）"));
	// TODO: the ROM interleaves the loads and the stores here (and reloads
	// the shine only once), which the plain TVec3::set() does not reproduce.
	unk138->mPosition.set(unk140, unk144, unk148);
	unk138->calcRootMatrix();
}

void TGoalWatermelon::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	char name[0x20];
	stream.readString(name, sizeof(name));
	stream.read(&unk140, sizeof(unk140));
	stream.read(&unk144, sizeof(unk144));
	stream.read(&unk148, sizeof(unk148));
}

TGoalWatermelon::TGoalWatermelon(const char* name)
	: TMapObjBase(name)
	, unk138(nullptr)
	, unk13C(0)
{
	unk148 = 0.0f;
	unk144 = 0.0f;
	unk140 = 0.0f;
}

u32 TSandEgg::getSDLModelFlag() const
{
	return 0;
}

TSandBird::~TSandBird() { }

TGoalWatermelon::~TGoalWatermelon() { }

TWatermelonStatic::~TWatermelonStatic() { }
