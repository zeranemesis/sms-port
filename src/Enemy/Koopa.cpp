
#include <Enemy/Enemy.hpp>

class TKoopaParts {
public:
	void control();
};

class TKoopaManager {
public:
	void* createEnemyInstance();
};

class TKoopaHand {
public:
	BOOL receiveMessage(THitActor*, u32);
};

void TKoopaParts::control() { }

void* TKoopaManager::createEnemyInstance() { return nullptr; }

BOOL TKoopaHand::receiveMessage(THitActor*, u32) { return TRUE; }
