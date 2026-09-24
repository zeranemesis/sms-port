
#include <Enemy/Enemy.hpp>

class TBossTelesa {
public:
	BOOL receiveMessage(THitActor*, u32);
};

class TTelesaSlot {
public:
	bool touchWater(THitActor*);
	void initNeonMatColor();
};

BOOL TBossTelesa::receiveMessage(THitActor*, u32) { return FALSE; }

bool TTelesaSlot::touchWater(THitActor*) { return false; }

void TTelesaSlot::initNeonMatColor() { }
