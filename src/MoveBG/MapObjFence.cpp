#include <MoveBG/MapObjFence.hpp>

void TFenceWater::changeStatusToWait()
{
	u8* self = reinterpret_cast<u8*>(this);
	*reinterpret_cast<f32*>(self + 0x140) = 0.0f;
	*reinterpret_cast<f32*>(self + 0x13C) = 0.0f;
	mState = 1;
}

void TFenceWaterH::changeStatusToWait()
{
	u8* self = reinterpret_cast<u8*>(this);
	*reinterpret_cast<f32*>(self + 0x140) = 0.0f;
	*reinterpret_cast<f32*>(self + 0x13C) = 0.0f;
	mState = 1;
	setUpMapCollision(0);
}

TFenceWater::~TFenceWater() { }

TFenceWaterH::~TFenceWaterH() { }

void TFenceWater::draw() const {}

void TRailFence::initMapCollisionData()
{
	TMapObjBase::initMapCollisionData();
}
