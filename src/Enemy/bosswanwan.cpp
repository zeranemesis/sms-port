
#include <dolphin/mtx.h>

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
