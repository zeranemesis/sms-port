#include <Animal/BeeHive.hpp>
#include <Animal/boid.hpp>
#include <Strategic/ObjManager.hpp>

// TODO: this entire translation unit is freshly scaffolded from mario.MAP
// and vtable data. Only trivial destructors are matched so far; the rest
// are placeholder stubs. TBeeHive is inferred to extend TRealoid (like
// TFishoid) based on the createRealoidActor(MActor*) override in the
// vtable; this has not been cross-checked in a debugger/Ghidra.

DEFINE_NERVE(TNerveBeeHiveReset, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBeeHiveWait, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBeeHiveMarioWaterIn, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBeeHiveAttack, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBeeHiveBreak, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveBeeHiveFall, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

void TBeeHiveManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "bee_body.bmd", 0x10210000, 0 },
		{ "bee_nest.bmd", 0x10210000, 0 },
		{ "bee_nest_break.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TBeeHiveManager::load(JSUMemoryInputStream& stream)
{
	// TODO: not yet decompiled
}

TBeeHiveManager::TBeeHiveManager(const char* name)
    : TEnemyManager(name)
{
	// TODO: not yet decompiled
}

void TBeeHive::getCenterOfGravity() const
{
	// TODO: not yet decompiled
}

void TBeeHive::appearBee(int index)
{
	TRealoidActor* bee = getRealoid(index);
	if (!(bee->mFlags & TRealoidActor::FLAG_UNK4)
	    && (bee->mFlags & TRealoidActor::FLAG_UNK2)) {
		bee->offFlag(TRealoidActor::FLAG_UNK2);
		bee->offHitFlag(HIT_FLAG_NO_COLLISION);
		unk150->getBoid(index)->mPosition = mPosition;
	}
}

void TBeeHive::doWait()
{
	// TODO: not yet decompiled
}

void TBeeHive::calcRootMatrix()
{
	// TODO: not yet decompiled
}

// TODO: TBeeHive::controlSound() not yet decompiled (left undefined so control() does not inline an empty stub)

// TODO: TBeeHive::controlCollision() not yet decompiled (left undefined so control() does not inline an empty stub)

void TBeeHive::bind()
{
	// TODO: not yet decompiled
}

void TBeeHive::control()
{
	controlCollision();
	TLiveActor::control();
	controlSound();
}

void TBeeHive::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TRealoid::perform(cue, graphics);
	TSpineEnemy::perform(cue, graphics);
}

BOOL TBeeHive::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: not yet decompiled
	return FALSE;
}

TRealoidActor* TBeeHive::createRealoidActor(MActor* actor)
{
	// TODO: not yet decompiled
	return 0;
}

void TBeeHive::load(JSUMemoryInputStream& stream)
{
	// TODO: not yet decompiled
}

void TBeeHive::receiveMessageFromChild(TBee* bee)
{
	// TODO: not yet decompiled
}

void TBeeHive::reset()
{
	// TODO: not yet decompiled
}

void TBeeHive::init(TLiveManager* liveManager)
{
	// TODO: not yet decompiled
}

TBeeHive::TBeeHive(const char* name)
    : TRealoid(name)
{
	// TODO: not yet decompiled
}

TBee::TBee(MActor* actor)
    : TRealoidActor(actor)
{
	// TODO: not yet decompiled
}

BOOL TBee::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: not yet decompiled
	return FALSE;
}

void TBee::init()
{
	// TODO: not yet decompiled
}

TBeeHiveManager::~TBeeHiveManager()
{
	// TODO: not yet decompiled
}

TBeeHive::~TBeeHive()
{
	// TODO: not yet decompiled
}

TBee::~TBee()
{
	// TODO: not yet decompiled
}
