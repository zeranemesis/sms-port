
#include <MoveBG/MapObjMonte.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
void TMapObjMonteRoot::initMapObj()
{
	TMapObjBase::initMapObj();
	mDamageHeight = 1400.0f * mScaling.y;
	calcEntryRadius();
	mPosition.y = mInitialPosition.y + mYOffset;
}

BOOL TJumpMushroom::receiveMessage(THitActor*, unsigned long)
{
	startAnim(1);
	return TRUE;
}

f32 TFluff::getRadiusAtY(f32) const { return 20.0f; }

f32 TGoalFlag::getRadiusAtY(f32) const { return 20.0f; }

void TGoalFlag::initMapObj() { TMapObjBase::initMapObj(); }
