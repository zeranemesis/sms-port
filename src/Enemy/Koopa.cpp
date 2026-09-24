
#include <Enemy/Enemy.hpp>

class TKoopaParts {
public:
	void control();
};

class TKoopa {
public:
	BOOL receiveMessage(THitActor*, u32);
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

BOOL TKoopa::receiveMessage(THitActor* sender, u32 message)
{
	return reinterpret_cast<TSpineEnemy*>(this)->TSpineEnemy::receiveMessage(
	    sender, message);
}

void* TKoopaManager::createEnemyInstance() { return nullptr; }

BOOL TKoopaHand::receiveMessage(THitActor*, u32) { return TRUE; }
