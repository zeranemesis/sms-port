
#include <dolphin/types.h>
#include <Animal/fishoid.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/LiveActor.hpp>
#include <Strategic/ObjManager.hpp>

struct TBeeHiveActorState {
	u8 pad0[0x64];
	u32 unk64;
	u8 pad68[0x0C];
	u32 unk74;
};

struct TBeeHiveSpawnData {
	u8 pad0[0x14];
	u8* entries;
};

struct TBeeHiveSpawnEntry {
	void* unk0;
	void* unk4;
	void* unk8;
	u8 padC[0x44];
};

class TBeeHive {
public:
	u8 pad0[0x10];
	void* unk10;
	void* unk14;
	void* unk18;
	u8 pad1C[0x134];
	TBeeHiveSpawnData* spawnData;
	TBeeHiveActorState** actors;

	void appearBee(int index);
	void controlCollision();
	void controlSound();
	void control();
	void perform(u32 cue, JDrama::TGraphics* graphics);
};

void TBeeHive::appearBee(int index)
{
	TBeeHiveActorState* actor = actors[index];
	if (actor->unk74 & 4)
		return;
	if (!(actor->unk74 & 2))
		return;

	actor->unk74 &= ~2u;
	actor->unk64 &= ~1u;

	TBeeHiveSpawnEntry* entry
	    = reinterpret_cast<TBeeHiveSpawnEntry*>(spawnData->entries + index * 0x50);
	entry->unk0 = unk10;
	entry->unk4 = unk14;
	entry->unk8 = unk18;
}

void TBeeHive::control()
{
	controlCollision();
	reinterpret_cast<TLiveActor*>(this)->TLiveActor::control();
	controlSound();
}

void TBeeHive::perform(u32 cue, JDrama::TGraphics* graphics)
{
	reinterpret_cast<TRealoid*>(this)->TRealoid::perform(cue, graphics);
	reinterpret_cast<TSpineEnemy*>(this)->TSpineEnemy::perform(cue, graphics);
}

class TBeeHiveManager : public TEnemyManager {
public:
	TBeeHiveManager(const char*);
	virtual void createModelData();
};

void TBeeHiveManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
	    {"bee_body.bmd", 0x10210000, 0},
	    {"bee_nest.bmd", 0x10210000, 0},
	    {"bee_nest_break.bmd", 0x10210000, 0},
	    {nullptr, 0, 0},
	};
	createModelDataArray(entry);
}

TBeeHiveManager::TBeeHiveManager(const char* name)
	: TEnemyManager(name)
{
}
