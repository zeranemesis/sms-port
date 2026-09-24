
#include <MoveBG/MapObjMonte.hpp>

BOOL TJumpMushroom::receiveMessage(THitActor*, unsigned long)
{
	startAnim(1);
	return TRUE;
}

f32 TFluff::getRadiusAtY(f32) const { return 20.0f; }

f32 TGoalFlag::getRadiusAtY(f32) const { return 20.0f; }

void TGoalFlag::initMapObj() { TMapObjBase::initMapObj(); }
