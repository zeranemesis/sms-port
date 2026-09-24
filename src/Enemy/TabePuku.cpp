
#include <Enemy/Enemy.hpp>
#include <Enemy/SmallEnemy.hpp>

class TTabePuku {
public:
	bool isFindMario(float);
	void forceKill();
	void behaveToWater(THitActor*);
	void reset();

	char mPadding[0xB8];
	float mResetValue;
};

void TTabePuku::forceKill() { }

bool TTabePuku::isFindMario(float distance)
{
	return reinterpret_cast<TSmallEnemy*>(this)->isFindMarioFromParam(distance);
}

void TTabePuku::behaveToWater(THitActor*) { }

void TTabePuku::reset() { mResetValue = 130.0f; }
