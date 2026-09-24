#include <MoveBG/MapObjPinna.hpp>
#include <Player/MarioAccess.hpp>
#include <System/EmitterViewObj.hpp>

void TMerrygoround::draw() const {}
u32 TAmiKing::touchWater(THitActor*) { return 1; }

void TChangeStageMerrygoround::calc()
{
	if (unk13C) {
		gpMarioParticleManager->emitAndBindToPosPtr(0x100, gpMarioPos, 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(0x101, gpMarioPos, 1, this);
	}
}

TPinnaCoaster::TPinnaCoaster(const char* name)
    : TMapObjBase(name)
    , unk138(0)
{
	unk148 = unk144 = unk140 = 0.0f;
}
