
#include <dolphin/mtx.h>

// rogue include: dummy string pair, needed to match the .rodata prologue
#include <System/DummyStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

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
