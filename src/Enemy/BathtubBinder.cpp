#include <Enemy/BathtubBinder.hpp>


// rogue include: the original TU opens .rodata with the four
// MActorMtxCalcType_* names plus the dummy string pair from
// System/DummyStrings.hpp; without them every string offset in this object
// is shifted.
#include <M3DUtil/InfectiousStrings.hpp>
#include <Strategic/LiveActor.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <Map/BathWaterManager.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MoveBG/MapObjCorona.hpp>

// The three `radius = sqrt(unk3C * unk3C - unk44 * unk44)` sites below are
// out-of-line calls in the original (bl sqrt__Q29JGeometry8TUtil<f>Ff, with
// the `mag <= 0` guard inside the callee), while every inv_sqrt in this
// function is expanded inline. JGUtil.hpp only offers the inline spelling,
// so the calls go through this TU-local non-inlined wrapper.
// FABRICATED: the callee is orig_sqrt, not sqrt__Q29JGeometry8TUtil<f>Ff, so
// the `bl` still shows as one mismatched instruction. Making JGUtil.hpp
// out-of-line instead was measured repo-wide at -32.2 points over 50 units -
// see docs/AGENT_MATCHING_TIPS.md.
#pragma dont_inline on
static f32 orig_sqrt(f32 v) { return JGeometry::TUtil<f32>::sqrt(v); }
#pragma dont_inline off

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
	unk4  = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
	unk8  = (TBathWaterManager*)JDrama::TNameRefGen::search("バスタブの水");
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

// Rolls Mario (and only Mario) around inside the bathtub: the two ends of
// him are pushed back inside the tub and clamped against its rim, his height
// is pulled towards the average of the water height at both ends and his
// pitch is tilted to match the slope of the water surface.
void TBathtubBinder::float_(TLiveActor* actor)
{
	if (unk8 == 0)
		return;

	Mtx mtx;
	MsMtxSetRotRPH(mtx, actor->mRotation.x, actor->mRotation.y,
	               actor->mRotation.z);

	TBathtub* bathtub = unk4;

	// front end of Mario (offset unkC along his facing direction)
	f32 frontX = actor->mPosition.x + mtx[0][2] * unkC;
	f32 frontZ = actor->mPosition.z + mtx[2][2] * unkC;

	f32 dx;
	f32 dz;
	f32 limit;

	if (bathtub != nullptr) {
		const TBathtubData* data = &bathtub->getBathtubData();
		JGeometry::TVec3<f32> center = data->getThing();
		f32 radius = orig_sqrt(
		    data->unk3C * data->unk3C - data->unk44 * data->unk44);
		limit = radius - unk10;
		dz    = frontZ - center.z;
		dx    = frontX - center.x;
		if (dx * dx + dz * dz > limit * limit) {
			f32 t = limit * JGeometry::TUtil<f32>::inv_sqrt(dx * dx + dz * dz);
			frontX = center.x + dx * t;
			frontZ = center.z + dz * t;
		}
	}

	f32 waterFront = unk20 + unk8->getWaterHeight(frontX, frontZ);

	// back end of Mario (offset unk14 the other way)
	f32 backX = actor->mPosition.x + mtx[0][2] * -unk14;
	f32 backZ = actor->mPosition.z + mtx[2][2] * -unk14;

	if (bathtub != nullptr) {
		const TBathtubData* data = &bathtub->getBathtubData();
		JGeometry::TVec3<f32> center = data->getThing();
		f32 radius = orig_sqrt(
		    data->unk3C * data->unk3C - data->unk44 * data->unk44);
		limit = radius - unk18;
		dz    = backZ - center.z;
		dx    = backX - center.x;
		if (dx * dx + dz * dz > limit * limit) {
			f32 t = limit * JGeometry::TUtil<f32>::inv_sqrt(dx * dx + dz * dz);
			backX = center.x + dx * t;
			backZ = center.z + dz * t;
		}
	}

	f32 waterBack = unk20 + unk8->getWaterHeight(backX, backZ);

	dx = frontX - backX;
	dz = frontZ - backZ;
	f32 dy = waterFront - waterBack;
	f32 targetY = unk1C * dy + waterBack;
	f32 oldY    = actor->mPosition.y;
	actor->mPosition.y = oldY + 0.2f * (targetY - oldY);

	if (dx * dx + dz * dz + dy * dy <= 0.000003814697265625f)
		return;

	f32 pitch = (360.0f / 65536.0f) * -matan(
	    JGeometry::TUtil<f32>::inv_sqrt(dx * dx + dz * dz), dy);
	pitch = JGeometry::TUtil<f32>::clamp(pitch, -15.0f, 15.0f);
	actor->mRotation.x = actor->mRotation.x
	                   + 0.1f * (pitch - actor->mRotation.x);
	actor->mRotation.z = 0.0f;

	f32 midLimit = 0.5f * (unk10 + unk18);

	if (bathtub != nullptr) {
		const TBathtubData* data = &bathtub->getBathtubData();
		JGeometry::TVec3<f32> center = data->getThing();
		f32 radius = orig_sqrt(
		    data->unk3C * data->unk3C - data->unk44 * data->unk44);
		limit = radius - midLimit;
		dz     = actor->mPosition.z - center.z;
		dx     = actor->mPosition.x - center.x;
		if (dx * dx + dz * dz > limit * limit) {
			f32 t = limit * JGeometry::TUtil<f32>::inv_sqrt(dx * dx + dz * dz);
			actor->mPosition.x = center.x + dx * t;
			actor->mPosition.z = center.z + dz * t;
		}
		if (actor->mPosition.y < center.y)
			actor->mPosition.y = center.y;
	}
}
