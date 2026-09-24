
#include <Enemy/Enemy.hpp>

class TFruitsBoat {
public:
	BOOL receiveMessage(THitActor*, u32);
};

BOOL TFruitsBoat::receiveMessage(THitActor*, u32) { return FALSE; }
