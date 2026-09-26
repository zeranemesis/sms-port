#include <Animal/BeeHive.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <Animal/boid.hpp>
#include <Strategic/ObjManager.hpp>
#include <MSound/MSound.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

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

void TBeeHive::controlSound()
{
	if (mBeeNum == 0)
		return;

	f32 x = 0.0f;
	f32 y = x;
	f32 z = x;
	int count = 0;
	for (int i = 0; i < mBeeNum; ++i) {
		TRealoidActor* bee = getRealoid(i);
		if (!(bee->mFlags & TRealoidActor::FLAG_UNK2_OR_UNK4)) {
			x += bee->mPosition.x;
			y += bee->mPosition.y;
			z += bee->mPosition.z;
			++count;
		}
	}

	if (count == 0)
		return;

	f32 inv = 1.0f / count;
	mSoundPos.x = x * inv;
	mSoundPos.y = y * inv;
	mSoundPos.z = z * inv;
	gpMSound->startBeeSe((Vec*)&mSoundPos, count);
}

void TBeeHive::controlCollision()
{
	int idx = mCollisionIdx;
	int num = mBeeNum;

	TRealoidActor* bee = getRealoid(idx);
	bee->checkHitActors();
	bee->onHitFlag(HIT_FLAG_CANNOT_ATTACK);

	// TODO: this first wrap is dead but reproduces a dead compare in the
	// target (and keeps control() from inlining us); probably an inline
	// "next index" helper in the original.
	int next = idx + 1;
	if (num <= next)
		next = 0;

	if (num <= ++mCollisionIdx)
		mCollisionIdx = 0;

	next = mCollisionIdx;
	if (num <= next)
		next = 0;

	TRealoidActor* nextBee = getRealoid(next);
	if (!(nextBee->mFlags & TRealoidActor::FLAG_UNK2_OR_UNK4))
		nextBee->offHitFlag(HIT_FLAG_CANNOT_ATTACK);
}

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
