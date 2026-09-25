#include <Enemy/BossHanachan.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
void TBossHanachan::kill() { }

BOOL TBossHanachan::hasMapCollision() const { return true; }

BOOL TBossHanachanManager::hasMapCollision() const { return true; }
