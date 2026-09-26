
#include <Enemy/Enemy.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjManager.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

class TBossTelesa {
public:
	BOOL receiveMessage(THitActor*, u32);
	void loadAfter();

private:
	// PAL accesses these references at 0x178..0x184. The named fields and
	// remaining TBossTelesa layout are not yet reconstructed.
	u8 mLoadAfterPadding[0x178];
	THitActor* mStageSlotObjects[3];
	THitActor* mStageOwnerObject;
};

class TTelesaSlot {
public:
	bool touchWater(THitActor*);
	void initNeonMatColor();
};

BOOL TBossTelesa::receiveMessage(THitActor*, u32) { return FALSE; }

void TBossTelesa::loadAfter()
{
	// PAL first gathers the stage's actor-type 0x4000019A objects into the
	// three references at this+0x178, then finds actor type 0x400001A6 and
	// gives it a back-reference to this boss at object offset 0x1A0.
	const u32 slotActorType = 0x4000019A;
	if (gpMapObjManager->getObjNumWithActorType(slotActorType) != 0) {
		int foundIndex = 0;
		for (int i = 0; i < gpMapObjManager->getObjNum(); i++) {
			TMapObjBase* obj = gpMapObjManager->getObj(i);
			if (obj->getActorType() == slotActorType)
				mStageSlotObjects[foundIndex++] = obj;
		}
	}

	const u32 ownerActorType = 0x400001A6;
	if (gpMapObjManager->getObjNumWithActorType(ownerActorType) != 0) {
		for (int i = 0; i < gpMapObjManager->getObjNum(); i++) {
			TMapObjBase* obj = gpMapObjManager->getObj(i);
			if (obj->getActorType() == ownerActorType) {
				mStageOwnerObject = obj;
				*(TBossTelesa**)((u8*)obj + 0x1A0) = this;
			}
		}
	}

	// TODO: PAL continues by creating/registering the roulette props and
	// initializing their per-object position/rotation/scale data. That part
	// is intentionally left untouched until its fields and resources are
	// identified from the complete method.
}

bool TTelesaSlot::touchWater(THitActor*) { return false; }

void TTelesaSlot::initNeonMatColor() { }
