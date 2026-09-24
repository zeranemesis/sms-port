
#include <Enemy/Enemy.hpp>

class TSamboHead;
TSamboHead* gpCurSamboHead;

class THanaSambo {
public:
	void behaveToWater(THitActor*);
	bool isCollidMove(THitActor*);
};

class TSamboFlower {
public:
	void control();
};

class TSamboHead {
public:
	void calcRootMatrix();
};

void THanaSambo::behaveToWater(THitActor*) { }

bool THanaSambo::isCollidMove(THitActor*) { return false; }

void TSamboFlower::control() { }

void TSamboHead::calcRootMatrix()
{
	gpCurSamboHead = this;
	reinterpret_cast<TSpineEnemy*>(this)->TSpineEnemy::calcRootMatrix();
}
