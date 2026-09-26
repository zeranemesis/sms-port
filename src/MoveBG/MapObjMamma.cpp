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

// NOTE: this TU uses -inline deferred, so definitions are emitted in
// reverse source order; keep them in reverse of marioEU.MAP address order
// (TSandEgg < TLeanMirror < TSandBomb in the map).

f32 TMapObjBall::getDepthAtFloating() { return unk18C; }

f32 TSandBase::mScaleMin = 0.00001f;

u32 TSandBomb::getSDLModelFlag() const
{
	return 0;
}

void TSandBomb::initMapObj() { TMapObjBase::initMapObj(); }

void TSandBomb::makeObjAppeared()
{
	TMapObjBase::makeObjAppeared();
	startControlAnim(1);
	startControlAnim(2);
}

TSandBase::TSandBase(const char* name)
	: TMapObjBase(name)
	, unk138(0.0f)
	, unk13C(0.0f)
	, unk144(nullptr)
{
}

bool TSandBase::withering()
{
	mScaling.y -= unk13C;
	if (mScaling.y < mScaleMin)
		mScaling.y = mScaleMin;

	SMSGetMSound()->startSoundActor(0x2099, &unk144->mPosition, 0, nullptr, 0,
	                                4);
	return mScaling.y <= mScaleMin;
}

TSandBombBase::TSandBombBase(const char* name)
	: TSandBase(name)
	, unk148(0)
	, unk14C(1.0f)
	, unk150(0.0f)
	, unk154(0.0f)
{
}

void TSandBombBase::waitBeforeExplode()
{
	mState      = 6;
	mStateTimer = unk148;
}

void TSandBombBase::grow() { mState = 5; }

void TSandBombBase::withered()
{
	mStateTimer = unk140;
	mState      = 3;
	unk144->sleep();
}

void TSandCastle::calcRootMatrix()
{
	if (mState == 2)
		return;
	TMapObjBase::calcRootMatrix();
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
