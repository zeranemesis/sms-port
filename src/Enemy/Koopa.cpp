
#include <Enemy/Enemy.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
class TKoopaParts {
public:
	void control();
};

class TKoopa {
public:
	BOOL receiveMessage(THitActor*, u32);
};

class TKoopaManager {
public:
	void* createEnemyInstance();
};

class TKoopaHand {
public:
	BOOL receiveMessage(THitActor*, u32);
};

void TKoopaParts::control() { }

BOOL TKoopa::receiveMessage(THitActor* sender, u32 message)
{
	return reinterpret_cast<TSpineEnemy*>(this)->TSpineEnemy::receiveMessage(
	    sender, message);
}

void* TKoopaManager::createEnemyInstance() { return nullptr; }

BOOL TKoopaHand::receiveMessage(THitActor*, u32) { return TRUE; }
