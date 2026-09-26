#include <Enemy/BossHanachan.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
void TBossHanachan::kill() { }

BOOL TBossHanachan::hasMapCollision() const { return true; }

BOOL TBossHanachanManager::hasMapCollision() const { return true; }
