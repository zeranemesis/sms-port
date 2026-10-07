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

// The 24 dead bytes are two rungs of the same chain: `getChild()` at the
// awaken site is +16 (at both sites it is +32) and the SMSGetMap() fork
// (promoted to Map.hpp in header round 18) is +4 per read site, i.e. +8 here.
// Both sites raw is 0x40, both through getChild() 0x60.
void TMapWarp::changeModel(int i)
{
	if (unk8 == i)
		return;

	// TODO: inlines
	SMSGetMap()->getModelManager()->getJointModel(0)->mChildren[unk8]
	    ->sleep();
	SMSGetMap()->getModelManager()->getJointModel(0)->getChild(i)->awake();
	unk8 = i;
}

void TMapWarp::warp(int) { }

// TODO: 99.8% (66.4% before closure batch 83). Three fixes got it there: the
// warp-point index is a named `int no = checkData->getData();` so that MWCC
// caches `no * 0x14` in a callee-saved register while re-reading the `unk4`
// member after each virtual call; `unk8` is the *left* operand of both
// inequality tests; and the warp destination is `SMS_GetMarioPos() + unk4[no]
// .unk8` bound to a named local (the by-value left operand of operator+ is
// retail's three-word copy in front of `bl TVec3::add`, and the named result is
// the second copy at 0xe8) rather than an in-place `+=`. The stream vector is
// zero-initialised and then has `.z` overwritten (`stfs` to the same slot
// twice), not built by the three-argument constructor.
// Residue: frame 0x170 vs 0x158, 24 dead bytes, plus `addi r5, r4, 0` where we
// emit `mr r5, r4` for MTXMultVec's duplicated out-pointer. The slots are not
// uniformly shifted: retail's order up the frame is [operator+ copy 0xa0,
// warpPos 0xe8, stream vector 0x100, mtx 0x10c, checkData 0x13c, the two
// double magics 0x148/0x158], ours is [copy 0xd4, stream vector 0xe0, mtx
// 0xec, warpPos 0x11c, checkData 0x12c, ...] -- so retail has 52 fewer low
// bytes and keeps both warp vectors at the bottom, below the stream vector and
// the matrix, where ours has `warpPos` in the named block. Making warpPos
// unnamed is much worse (80.2%) and an explicit `TVec3(...)` temporary bound
// to the `const&` parameter worse still (76.5%), so the two vectors are not
// plain temporaries either; the low region is the lead.
// cc28 (2026-09-22): 23 -> 11 mismatching slots, all r1 displacements. C-style
// top declarations `fVar8, checkData, mtx, vec2, warpPos` (vec2 via `.set`)
// put fVar8 (+4), checkData 0x13c, mtx 0x10c and vec2 0x100 exactly on
// retail's slots once the destination goes through the TU-local reference
// out-parameter level `MapWarpDest(warpPos, unk4[no])`. Left: warpPos 0xf4 vs
// 0xe8 and the operator+ copy 0xc8 vs 0xa0 -- retail has 12 bytes between vec2
// and warpPos and 0x1c more pool between warpPos and the copy, i.e. ~0x28 of
// inline pool created before the copy that ours creates after it. Tried, all
// worse or equal: helper returning by value (+8 frame), named-result helper
// (+0x10), the helper also issuing the request (same or +8), `.set(a + b)`
// (float copies), top-declared `TCubeStreamInfo* info`/`int no` (no slot),
// dead named `f32 angle` (no slot), every order of the five top declarations.
// c-m29: the warp block is the UNUSED `warp(int no)` (map 0x138): written as
// `int warp = unk4[no].unk0; if (unk8 != warp) {...}` with `TVec3 warpPos =
// SMS_GetMarioPos() + unk4[no].getUnk8();` and called here, it is 0x138 exact
// and watchToWarp keeps every instruction and frame 0x170 once fVar8 is
// unnamed, but 23 slots differ (99.89 < 99.94): retail has a dead 12-byte
// object at 0xf4 and the operator+ copy low at 0xa0, i.e. a by-value-returning
// operator+ with a local (frame-model 8d), not the header's fabricated one.
static inline s32 MapWarpGetStreamType(const TCubeStreamInfo* info)
{
	return info->unk38;
}

static inline f32 MapWarpGetStreamSpeed(const TCubeStreamInfo* info)
{
	return info->unk40;
}

static inline void MapWarpDest(JGeometry::TVec3<f32>& out,
                               TMapWarp::TMapWarpInfo& info)
{
	out = SMS_GetMarioPos() + info.getUnk8();
}

void TMapWarp::watchToWarp()
{
	f32 fVar8;
	const TBGCheckData* checkData;
	Mtx mtx;
	JGeometry::TVec3<f32> vec2;
	JGeometry::TVec3<f32> warpPos;
	fVar8 = gpMap->checkGroundExactY(gpMarioPos->x, gpMarioPos->y + 30.0f,
	                                 gpMarioPos->z, &checkData);

	if (checkData->isWarp()) {
		int no   = checkData->getData();
		int warp = unk4[no].unk0;
		if (unk8 != warp) {
			gpMap->getModelManager()->getJointModel(0)->getChild(unk8)->sleep();
			gpMap->getModelManager()->getJointModel(0)->getChild(warp)->awake();
			unk8 = unk4[no].unk0;

			MapWarpDest(warpPos, unk4[no]);
			SMS_MarioWarpRequest(warpPos,
			                     ((*gpMarioAngleY) * 180.0f) / 32768.0f);
		}
	}

	if (checkData->isMapChange()) {
		int no = checkData->getData();
		if (unk8 != no) {
			gpMap->getModelManager()->getJointModel(0)->getChild(unk8)->sleep();
			gpMap->getModelManager()->getJointModel(0)->getChild(no)->awake();

			unk8 = no;
		}
	}

	int no = gpCubeStream->getInCubeNo(SMS_GetMarioPos());
	if (no == -1)
		return;

	TCubeStreamInfo* info = (TCubeStreamInfo*)(*gpCubeStream->unk14)[no];
	MsMtxSetXYZRPH(mtx, 0.0f, 0.0f, 0.0f, info->unk18.x, info->unk18.y,
	               info->unk18.z);

	vec2.set(0.0f, 0.0f, 0.0f);
	vec2.z = 0.01f * MapWarpGetStreamSpeed(info);
	MTXMultVec(mtx, &vec2, &vec2);
	if ((MapWarpGetStreamType(info) == 0 ? true : false)
	    || (MapWarpGetStreamType(info) == 1 ? true : false))
		SMS_FlowMoveMario(vec2);
	else
		SMS_WindMoveMario(vec2);
}

// The 16 dead bytes are one `TJointObj::getChild()` level on the loop body's
// child fetch (`getChildrenNum()` on the bound instead is the same +16; both
// together overshoot to 0x60).
void TMapWarp::initModel()
{
	// TODO: inlines
	int num = gpMap->getModelManager()->getJointModel(0)->mChildrenNum;
	for (int i = 0; i < num; ++i)
		if (i != unk8)
			gpMap->getModelManager()
			    ->getJointModel(0)
			    ->getChild((u16)i)
			    ->sleep();
}

int getWarpPointNo(const char* name)
{
	// Fabricated struct; the entries themselves are the map's 0x98-byte
	// point_name_table$2630.
	struct NameTableEntry {
		const char* mName;
		u32 mNo;
	};
	static const NameTableEntry point_name_table[] = {
		{ "warpA1", 0 },  { "warpA0", 1 },  { "warpB1", 2 },  { "warpB0", 3 },
		{ "warpC1", 4 },  { "warpC0", 5 },  { "warpD1", 6 },  { "warpD0", 7 },
		{ "warpE1", 8 },  { "warpE0", 9 },  { "warpF1", 10 }, { "warpF0", 11 },
		{ "warpG1", 12 }, { "warpG0", 13 }, { "warpH1", 14 }, { "warpH0", 15 },
		{ "warpI1", 16 }, { "warpI0", 17 }, { nullptr, 0 },
	};

	u32 needle = 0;
	while (strcmp(point_name_table[needle].mName, name) != 0)
		++needle;
	return point_name_table[needle].mNo;
}

void loadWarpPointPos(JSUMemoryInputStream& stream, int num, Vec* positions)
{
	for (int i = 0; i < num; ++i) {
		const char* name = stream.readString();
		int no = getWarpPointNo(name);
		Vec& pos = positions[no];
		stream >> pos.x >> pos.y >> pos.z;

		u32 dummy;
		stream >> dummy >> dummy >> dummy;
		stream >> dummy >> dummy >> dummy;
	}
}

// TODO: 99.8%, frame and every instruction exact. The name loop is
// loadWarpPointPos's own body (its UNUSED map size 0x12c pins the loop, the
// readString and the getWarpPointNo lookup inside it; `name` and `no` named,
// `cnt` named at the call). The stack arrays go positions/warp/dest; the four
// pre-loop reads share the named `u32 data`; six chained `>>` continuations.
// Residue: the third loop's reused values (retail local_180[i] r19 /
// local_1d0[i] r12, ours swapped) and the negations' `add r8, r8, r5` (ours
// r9). All-raw negations fix the r8 but lose 0x10 of frame, which the two
// getUnk8() sites were filling; retail's 0x10 has another source (not the
// other accessor placements, nor named u32 copies of the two values).
// c-k5: regalloc.py names the swapped webs as the local_180[i]/local_1d0[i]
// CSE temporaries (@913/@914); named `warp`/`kind` copies in either order and
// reading the second pair back from unk4[2 * i] are all worse (97.8/94.4).
void TMapWarp::init(JSUMemoryInputStream& stream)
{
	u32 data;
	stream >> data;
	unk0 = data;
	if (!unk0)
		return;

	stream >> data;
	unk8 = data;
	unk4 = new TMapWarpInfo[unk0 * 2];

	JGeometry::TVec3<f32> local_130[20];
	u32 local_180[20];
	u32 local_1d0[20];

	for (int i = 0; i < unk0; ++i) {
		stream >> data;
		local_1d0[i] = data;
		stream >> data;
		local_180[i] = data;
	}

	int cnt = unk0 * 2;
	loadWarpPointPos(stream, cnt, local_130);

	for (int i = 0; i < unk0; ++i) {
		unk4[2 * i].unk8.x = local_130[2 * i].x - local_130[2 * i + 1].x;
		unk4[2 * i].unk8.y = local_130[2 * i].y - local_130[2 * i + 1].y;
		unk4[2 * i].unk8.z = local_130[2 * i].z - local_130[2 * i + 1].z;

		unk4[2 * i].unk0 = local_180[i];
		unk4[2 * i].unk4 = local_1d0[i];

		unk4[2 * i + 1].unk8.x = -unk4[2 * i].getUnk8().x;
		unk4[2 * i + 1].unk8.y = -unk4[2 * i].getUnk8().y;
		unk4[2 * i + 1].unk8.z = -unk4[2 * i].unk8.z;

		unk4[2 * i + 1].unk0 = local_180[i];
		unk4[2 * i + 1].unk4 = local_1d0[i];
	}

	if (SMSGetMarDirector()->mMap == 4) {
		unkC = 8.0f;
	}
}

TMapWarp::TMapWarp()
    : unk0(0)
    , unk4(0)
    , unk8(0)
    , unkC(3.0f)
{
}
