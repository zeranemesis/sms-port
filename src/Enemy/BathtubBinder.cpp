#include <Enemy/BathtubBinder.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <Strategic/LiveActor.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <Map/BathWaterManager.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MoveBG/MapObjCorona.hpp>

TBathtubBinder::TBathtubBinder()
{
	unk4 = nullptr;
	unk8 = 0;
}

TBathtubBinder::~TBathtubBinder() {}

void TBathtubBinder::bind(TLiveActor* actor)
{
	if (unk4 == nullptr || *(u8*)((u8*)unk4 + 0x29a) == 0) {
		float_(actor);
	}
}

bool TBathtubBinder::init(f32 a, f32 b, f32 c, f32 d, f32 e)
{
	unk4  = JDrama::TNameRefGen::search("バスタブ");
	unk8  = (s32)JDrama::TNameRefGen::search("バスタブの栓");
	unk20 = e;
	unkC  = a;
	unk10 = b;
	unk14 = c;
	unk18 = d;
	unk1C = unk18 / (unk10 + unk18);
	if (unk4 == nullptr)
		unk8 = 0;
	return unk4 != nullptr;
}

void TBathtubBinder::float_(TLiveActor* actor)
{
	if (unk8 == 0)
		return;

	TBathtub* bathtub = (TBathtub*)unk4;
	TBathWaterManager* water = (TBathWaterManager*)unk8;
	Mtx rotation;
	MsMtxSetRotRPH(rotation, actor->mRotation.x, actor->mRotation.y,
	               actor->mRotation.z);

	const TBathtubData* data =
	    bathtub != nullptr ? &bathtub->getBathtubData() : nullptr;
	f32 radius = 0.0f;
	if (data != nullptr)
		radius = JGeometry::TUtil<f32>::sqrt(data->unk3C * data->unk3C
		                                    - data->unk44 * data->unk44);

	JGeometry::TVec3<f32> point(actor->mPosition.x + rotation[0][2] * unkC,
	                            actor->mPosition.y,
	                            actor->mPosition.z + rotation[2][2] * unkC);
	f32 dx;
	f32 dz;
	f32 limit;
	f32 distanceSquared;
	if (data != nullptr) {
		dx = point.x - data->mPos.x;
		dz = point.z - data->mPos.z;
		limit = radius - unk10;
		distanceSquared = dx * dx + dz * dz;
		if (distanceSquared > limit * limit) {
			f32 distance = JGeometry::TUtil<f32>::sqrt(distanceSquared);
			point.x = data->mPos.x + dx * (limit / distance);
			point.z = data->mPos.z + dz * (limit / distance);
		}
	}
	f32 waterA = water->getWaterHeight(point.x, point.z) + unk20;

	JGeometry::TVec3<f32> other(actor->mPosition.x - rotation[0][2] * unk14,
	                            actor->mPosition.y,
	                            actor->mPosition.z - rotation[2][2] * unk14);
	if (data != nullptr) {
		dx = other.x - data->mPos.x;
		dz = other.z - data->mPos.z;
		limit = radius - unk18;
		distanceSquared = dx * dx + dz * dz;
		if (distanceSquared > limit * limit) {
			f32 distance = JGeometry::TUtil<f32>::sqrt(distanceSquared);
			other.x = data->mPos.x + dx * (limit / distance);
			other.z = data->mPos.z + dz * (limit / distance);
		}
	}
	f32 waterB = water->getWaterHeight(other.x, other.z) + unk20;

	f32 dy = waterA - waterB;
	f32 targetY = waterB + unk1C * dy;
	f32 oldY = actor->mPosition.y;
	actor->mPosition.y = oldY + 0.2f * (targetY - oldY);
	dx = point.x - other.x;
	dz = point.z - other.z;
	if (dx * dx + dz * dz + dy * dy < 0.000003814697265625f)
		return;

	f32 pitch = matan(JGeometry::TUtil<f32>::sqrt(dx * dx + dz * dz), dy)
	            * 0.0054931640625f;
	if (pitch < -15.0f)
		pitch = -15.0f;
	else if (pitch > 15.0f)
		pitch = 15.0f;
	actor->mRotation.x += 0.1f * (pitch - actor->mRotation.x);
	actor->mRotation.z = 0.0f;

	if (data != nullptr) {
		f32 centerOffset = 0.5f * (unk10 + unk18);
		JGeometry::TVec3<f32> foot(actor->mPosition.x - rotation[0][2] * unk14,
		                           actor->mPosition.y,
		                           actor->mPosition.z - rotation[2][2] * unk14);
		dx = foot.x - data->mPos.x;
		dz = foot.z - data->mPos.z;
		limit = radius - centerOffset;
		distanceSquared = dx * dx + dz * dz;
		if (distanceSquared > limit * limit) {
			f32 distance = JGeometry::TUtil<f32>::sqrt(distanceSquared);
			actor->mPosition.x = data->mPos.x + dx * (limit / distance)
			                     + rotation[0][2] * unk14;
			actor->mPosition.z = data->mPos.z + dz * (limit / distance)
			                     + rotation[2][2] * unk14;
		}

		f32 floor = data->mPos.y - data->unk44;
		if (actor->mPosition.y < floor)
			actor->mPosition.y = floor;
	}
}
