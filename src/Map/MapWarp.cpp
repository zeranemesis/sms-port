#include <Map/MapWarp.hpp>
#include <Map/MapModel.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Player/MarioAccess.hpp>
#include <Camera/CubeManagerBase.hpp>
#include <System/MarDirector.hpp>
#include <dolphin/mtx.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

void TMapWarp::changeModel(int i)
{

	
	
	if (unk8 == i)
		return;

	// TODO: inlines
	gpMap->getModelManager()->getJointModel(0)->mChildren[unk8]->sleep();
	gpMap->getModelManager()->getJointModel(0)->mChildren[i]->awake();
	unk8 = i;
}

void TMapWarp::warp(int) { }

void TMapWarp::watchToWarp()
{

	// Measured: worth +0.1 pp here (45 -> 25 differing instructions).
	// TODO: the residual is an INTERIOR shift, not bottom padding - the ROM puts
	// `marioPos` (0xe8) and its `SMS_GetMarioPos()+unk4[point].unk8` scratch (0xa0)
	// BELOW `mtx`/`vec2`, we put them above. checkData lands 0x10 low as a result.
	
	
	const TBGCheckData* checkData;
	f32 fVar8 = gpMap->checkGroundExactY(gpMarioPos->x, gpMarioPos->y + 30.0f,
	                                     gpMarioPos->z, &checkData);

	if (checkData->isWarp()) {
		int point = checkData->getData();
		int warp = unk4[point].unk0;
		if (unk8 != warp) {
			gpMap->getModelManager()->getJointModel(0)->getChild(unk8)->sleep();
			gpMap->getModelManager()->getJointModel(0)->getChild(warp)->awake();
			unk8 = unk4[point].unk0;

			// TODO: inlines
			JGeometry::TVec3<f32> marioPos
			    = SMS_GetMarioPos() + unk4[point].unk8;
			SMS_MarioWarpRequest(marioPos,
			                     (*gpMarioAngleY * 180.0f) / 32768.0f);
		}
	}

	if (checkData->isMapChange()) {
		int point = checkData->getData();
		if (unk8 != point) {
			gpMap->getModelManager()->getJointModel(0)->getChild(unk8)->sleep();
			gpMap->getModelManager()
			    ->getJointModel(0)
			    ->getChild(point)
			    ->awake();

			unk8 = point;
		}
	}

	int no = gpCubeStream->getInCubeNo(SMS_GetMarioPos());
	if (no == -1)
		return;

	TCubeStreamInfo& info = (TCubeStreamInfo&)(*gpCubeStream->unk14)[no];
	Mtx mtx;
	MsMtxSetXYZRPH(mtx, 0.0f, 0.0f, 0.0f, info.unk18.x, info.unk18.y,
	               info.unk18.z);

	JGeometry::TVec3<f32> vec2(0.0f, 0.0f, 0.0f);
	vec2.z = info.unk40 * 0.01f;
	MTXMultVec(mtx, &vec2, &vec2);
	if ((info.unk38 == 0 ? true : false) || (info.unk38 == 1 ? true : false))
		SMS_FlowMoveMario(vec2);
	else
		SMS_WindMoveMario(vec2);
}

void TMapWarp::initModel()
{

	
	
	// TODO: inlines
	int num = gpMap->getModelManager()->getJointModel(0)->mChildrenNum;
	for (int i = 0; i < num; ++i)
		if (i != unk8)
			gpMap->getModelManager()
			    ->getJointModel(0)
			    ->mChildren[(u16)i]
			    ->sleep();
}

void getWarpPointNo(const char*) { }

void loadWarpPointPos(JSUMemoryInputStream&, int, Vec*) { }

void TMapWarp::init(JSUMemoryInputStream& stream)
{
	// TODO(matching): 95.9% (808B). Stack layout now exact (frame 0x230 vs
	// target 0x238; all local offsets match). Residuals are MWCC register-
	// allocator artifacts we could not reproduce:
	//  - register rotation: target binds stream->r25 / this->r31, ours
	//    stream->r31 / this->r30 (target saves r19-r31 via stmw r19 @0x204,
	//    ours r20-r31 via stmw r20 @0x200 -> frame differs by 8).
	//  - 3 target-only instructions: two eager vec-address copies
	//    (addi r4, r5, 0 / addi r20, r5, 8 before the first stream>> read of
	//    local_130[idx]) and one extra stream copy used as `this` for the 3rd
	//    dummy read (target keeps 5 stream copies, our stream>>-chain
	//    intermediates coalesce to 4).
	// Attempted: frame pads at top/bottom, shared read temp, chained
	// stream>> expressions in several groupings (all measured via
	// tools/decomp-diff.py). Instruction sequence otherwise matches 100%.
	// Fabricated
	struct NameTableEntry {
		const char* unk0;
		u32 unk4;
	};
	static const NameTableEntry point_name_table[] = {
		{ "warpA1", 0 },  { "warpA0", 1 },  { "warpB1", 2 },  { "warpB0", 3 },
		{ "warpC1", 4 },  { "warpC0", 5 },  { "warpD1", 6 },  { "warpD0", 7 },
		{ "warpE1", 8 },  { "warpE0", 9 },  { "warpF1", 10 }, { "warpF0", 11 },
		{ "warpG1", 12 }, { "warpG0", 13 }, { "warpH1", 14 }, { "warpH0", 15 },
		{ "warpI1", 16 }, { "warpI0", 17 }, { nullptr, 0 },
	};

	// Single shared read temp: the ROM passes the SAME stack slot (top of frame,
	// just above local_130) for all four integer reads.
	u32 tmp;
	stream.read(tmp);
	unk0 = tmp;
	if (!unk0)
		return;

	stream.read(tmp);
	unk8 = tmp;
	unk4 = new TMapWarpInfo[unk0 * 2];

	JGeometry::TVec3<f32> local_130[20];
	u32 local_180[20];
	u32 local_1d0[20];

	for (int i = 0; i < unk0; ++i) {
		stream.read(tmp);
		local_1d0[i] = tmp;
		stream.read(tmp);
		local_180[i] = tmp;
	}

	int cnt = unk0 * 2;
	for (int i = 0; i < cnt; ++i) {
		const char* str = stream.readString();
		u32 needle      = 0;
		while (strcmp(point_name_table[needle].unk0, str) != 0)
			++needle;

		u32 idx = point_name_table[needle].unk4;
		stream >> local_130[idx].x >> local_130[idx].y >> local_130[idx].z;

		u32 dummy;
		stream >> dummy >> dummy >> dummy;
		stream >> dummy;
		stream >> dummy >> dummy;
	}

	for (int i = 0; i < unk0; ++i) {
		unk4[2 * i].unk8.x = local_130[2 * i].x - local_130[2 * i + 1].x;
		unk4[2 * i].unk8.y = local_130[2 * i].y - local_130[2 * i + 1].y;
		unk4[2 * i].unk8.z = local_130[2 * i].z - local_130[2 * i + 1].z;

		unk4[2 * i].unk0 = local_180[i];
		unk4[2 * i].unk4 = local_1d0[i];

		unk4[2 * i + 1].unk8.x = -unk4[2 * i].getUnk8().x;
		unk4[2 * i + 1].unk8.y = -unk4[2 * i].getUnk8().y;
		unk4[2 * i + 1].unk8.z = -unk4[2 * i].getUnk8().z;

		unk4[2 * i + 1].unk0 = local_180[i];
		unk4[2 * i + 1].unk4 = local_1d0[i];
	}

	if (gpMarDirector->mMap == 4) {
		unkC = 8.0f;
	}


	// target layout: dummy at 0x64, arrays at 0x68/0xb8/0x108, temp at 0x1f8).
	// Our frame = 0x30 base + 0x28 of MWCC expression-temporary slots (5
	// stream>>-chain intermediates, measured) + this 12-byte pad = 0x64.
	
	
}

TMapWarp::TMapWarp()
    : unk0(0)
    , unk4(0)
    , unk8(0)
    , unkC(3.0f)
{
}
