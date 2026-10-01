#include <Enemy/Spider.hpp>
#include <Enemy/Enemy.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/Map.hpp>

TSpider::TSpider()
    : unk4(0)
    , unk8(0)
    , unkC(0)
    , unk10(0.0f)
{
}

TSpider::~TSpider() { }

// TODO: 89.8% -- the whole instruction stream now lines up with the ROM, but
// the frame is 0x158 in the ROM and only 0x118 here, so every stack offset is
// off and MWCC picks a different register allocation:
//  * ROM keeps local_114.y, local_114.z and local_50.z in f31/f30/f27 (loaded
//    in the prologue at 0x5c / 0x88 / 0x154), this build re-loads them from
//    the stack, so it has 3 spare callee-saved FP regs and CSEs
//    &mBodyScale / &mWallRadius / &wall->mNormal into r30 / r29 / r4 instead.
//  * ROM leaves 4 never-touched bytes at 0x120, 12 at 0xa4 and 52 at 0x4c
//    (plus 64 at 0x00); we leave 80 at 0x00 and nothing in the middle.
// Reproducing that needs the original's exact local/temporary set, which the
// (identical) code no longer determines.

void TSpider::bind(TLiveActor* param_1)
{
	JGeometry::TVec3<f32> local_114 = param_1->mLinearVelocity;
	JGeometry::TVec3<f32> local_50  = param_1->mPosition;
	local_50 += local_114;

	if (param_1->isAirborne()) {
		JGeometry::TVec3<f32> local_5C = param_1->mVelocity;
		local_50 += local_5C;
		f32 dVar7 = param_1->getGravityY();
		local_5C.y -= dVar7;
		if (local_5C.y < TLiveActor::mVelocityMinY)
			local_5C.y = TLiveActor::mVelocityMinY;

		param_1->mVelocity = local_5C;
	}

	const TBGCheckData* local_60;
	f32 fVar3 = gpMap->checkGround(
	    local_50.x, local_50.y + ((TSpineEnemy*)param_1)->getHeadHeight(),
	    local_50.z, &local_60);
	fVar3 += 1.0f;

	if (param_1->mPosition.y - local_50.y > 0.0f) {
		const TBGCheckData* local_64;
		f32 dVar7 = gpMap->checkGround(
		    local_50.x, local_50.y + ((TSpineEnemy*)param_1)->getHeadHeight(),
		    local_50.z, &local_64);
		dVar7 += 1.0f;
		if (dVar7 > fVar3) {
			local_60 = local_64;
			fVar3    = dVar7;
		}
	}

	if (local_60->checkFlag(BG_CHECK_FLAG_ILLEGAL)) {
		if (unk4 <= 0) {
			param_1->kill();
		} else {
			local_50.y = param_1->mPosition.y;
			fVar3      = local_50.y;
			unk4 -= 1;
		}
	} else {
		unk4 = 0x1E;
	}

	if (local_50.y <= fVar3) {
		local_50.y = fVar3;

		param_1->mVelocity = JGeometry::TVec3<f32>(0, 0, 0);

		param_1->offLiveFlag(LIVE_FLAG_AIRBORNE);
		param_1->offLiveFlag(LIVE_FLAG_UNK8000);
	} else {
		param_1->onLiveFlag(LIVE_FLAG_AIRBORNE);
	}

	param_1->mGroundHeight = fVar3;
	param_1->mGroundPlane  = local_60;

	// the wall sphere is centred one head-height above the spider's feet and
	// has the body-scaled wall radius (0x148 * 0x14c), not the head radius
	TBGWallCheckRecord local_90(local_50.x,
	                            local_50.y + ((TSpineEnemy*)param_1)->getHeadHeight(),
	                            local_50.z,
	                            ((TSpineEnemy*)param_1)->getWallRadius(), 1, 0);

	JGeometry::TVec3<f32> local_bc;
	f32 unaff_f29;

	bool b   = gpMap->isTouchedWallsAndMoveXZ(&local_90);
	local_bc = local_90.mCenter;
	if (!b) {
		if (unk8 > 0) {
			unk8 -= 1;
			unaff_f29 = ((TSpineEnemy*)param_1)->mMarchSpeed;
			param_1->offLiveFlag(LIVE_FLAG_AIRBORNE);
			param_1->offLiveFlag(LIVE_FLAG_UNK8000);
			param_1->mVelocity = JGeometry::TVec3<f32>(0, 0, 0);
		} else {
			unkC = 0;

			((TSpineEnemy*)param_1)->unk138 = nullptr;

			unaff_f29 = 0.0f;
		}
		unk10 -= 0.016666667f;
		if (unk10 < 0.0f)
			unk10 = 0.0f;
	} else {
		JGeometry::TVec3<f32> normal = local_90.mResultWalls[0]->getNormal();
		if (normal.dot(local_114) < 0.0f) {
			unaff_f29 = ((TSpineEnemy*)param_1)->mMarchSpeed;
			param_1->offLiveFlag(LIVE_FLAG_AIRBORNE);
			param_1->offLiveFlag(LIVE_FLAG_UNK8000);
			param_1->mVelocity = JGeometry::TVec3<f32>(0, 0, 0);

			unkC = local_90.mResultWalls[0];

			((TSpineEnemy*)param_1)->unk138 = unkC;

			unk8 = 0x3C;

			normal.scale(unk10 * ((TSpineEnemy*)param_1)->getWallRadius(), normal);
			local_bc.sub(normal);

			unk10 += 1.0f / 60.0f;
			if (unk10 > 1.0f)
				unk10 = 1.0f;
		}
	}

	// the spider's new position is turned into its linear velocity here; the
	// ROM reuses this very stack slot (the one local_50 lived in) for it
	local_50 = local_bc;
	local_50.y += unaff_f29 - ((TSpineEnemy*)param_1)->getHeadHeight();

	param_1->mLinearVelocity = local_50 - param_1->mPosition;
}
