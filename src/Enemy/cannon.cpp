
#include <Enemy/Enemy.hpp>

class TCannon : public TSpineEnemy {
public:
	TCannon(const char*);
	bool isCollidMove(THitActor*);
	bool isHitVallid(u32);
	bool isObject();
	bool isInhibitedForceMove();
	void startChorobeiShout();

private:
	u8 unk150[0xC];
	s32 unk15C;
};

bool TCannon::isCollidMove(THitActor*) { return false; }

bool TCannon::isHitVallid(u32) { return false; }

bool TCannon::isObject()
{
	bool canBeObject;
	if (unk15C == 4) {
		canBeObject = true;
	} else {
		canBeObject = false;
	}
	if (canBeObject && checkCurAnmEnd(0))
		return true;
	return false;
}

bool TCannon::isInhibitedForceMove() { return true; }

void TCannon::startChorobeiShout() { }
