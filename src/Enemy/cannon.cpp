
#include <Enemy/Enemy.hpp>

class TCannon {
public:
	bool isCollidMove(THitActor*);
	bool isHitVallid(u32);
};

bool TCannon::isCollidMove(THitActor*) { return false; }

bool TCannon::isHitVallid(u32) { return false; }
