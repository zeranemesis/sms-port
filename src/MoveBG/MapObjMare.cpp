#include <MoveBG/MapObjMare.hpp>

u32 TMuddyBoat::getSDLModelFlag() const { return 0; }

void TMuddyBoat::calcRootMatrix() { }

void TMareEventPoint::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);
	initHitActor(0x40000236, 0, 0, 0.0f, 0.0f, 300.0f, 600.0f);
}
