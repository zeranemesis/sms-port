
#include <Enemy/Enemy.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <Enemy/SmallEnemy.hpp>

class TSamboHead;
TSamboHead* gpCurSamboHead;

class THanaSambo {
public:
	void behaveToWater(THitActor*);
	bool isCollidMove(THitActor*);
	BOOL isHitValid(u32);
};

class TSamboFlower {
public:
	void control();
};

class TSamboHead {
public:
	void calcRootMatrix();
	void setDeadAnm();
};

void THanaSambo::behaveToWater(THitActor*) { }

bool THanaSambo::isCollidMove(THitActor*) { return false; }

BOOL THanaSambo::isHitValid(u32 message)
{
	if (message == 0xB) {
		reinterpret_cast<TLiveActor*>(this)->onLiveFlag(LIVE_FLAG_HIDDEN);
		return TRUE;
	}
	return FALSE;
}

void TSamboFlower::control() { }

void TSamboHead::calcRootMatrix()
{
	gpCurSamboHead = this;
	reinterpret_cast<TSpineEnemy*>(this)->TSpineEnemy::calcRootMatrix();
}

void TSamboHead::setDeadAnm()
{
	reinterpret_cast<TSmallEnemy*>(this)->setBckAnm(3);
}
