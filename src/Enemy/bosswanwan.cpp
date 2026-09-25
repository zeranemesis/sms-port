
#include <dolphin/mtx.h>

// rogue include: dummy string pair, needed to match the .rodata prologue
#include <System/DummyStrings.hpp>

class TBossWanwan {
public:
	void kill();
};

class TBWPicket {
public:
	MtxPtr getTakingMtx();
};

void TBossWanwan::kill() { }

MtxPtr TBWPicket::getTakingMtx()
{
	return reinterpret_cast<MtxPtr>(reinterpret_cast<char*>(this) + 0x74);
}
