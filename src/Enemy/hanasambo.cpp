
#include <Enemy/Enemy.hpp>

class THanaSambo {
public:
	void behaveToWater(THitActor*);
	bool isCollidMove(THitActor*);
};

class TSamboFlower {
public:
	void control();
};

void THanaSambo::behaveToWater(THitActor*) { }

bool THanaSambo::isCollidMove(THitActor*) { return false; }

void TSamboFlower::control() { }
