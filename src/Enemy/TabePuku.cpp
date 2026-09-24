
#include <Enemy/Enemy.hpp>

class TTabePuku {
public:
	void forceKill();
	void behaveToWater(THitActor*);
	void reset();

	char mPadding[0xB8];
	float mResetValue;
};

void TTabePuku::forceKill() { }

void TTabePuku::behaveToWater(THitActor*) { }

void TTabePuku::reset() { mResetValue = 130.0f; }
