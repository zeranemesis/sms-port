
#include <Enemy/SmallEnemy.hpp>

class TAmiNoko {
public:
	bool isCollidMove(THitActor*);
	const char** getBasNameTable() const;
};

class TAmiNokoManager {
public:
	TSpineEnemy* createEnemyInstance();
};

bool TAmiNoko::isCollidMove(THitActor*) { return false; }

TSpineEnemy* TAmiNokoManager::createEnemyInstance() { return nullptr; }

static const char* amiNoko_bastable[] = {
	nullptr,
	"/scene/amiNoko/bas/aminoko_flying1_start.bas",
	"/scene/amiNoko/bas/aminoko_hit1.bas",
	nullptr,
	"/scene/amiNoko/bas/aminoko_run1_loop.bas",
	nullptr,
	nullptr,
	"/scene/amiNoko/bas/aminoko_run2_loop.bas",
	nullptr,
	nullptr,
	"/scene/amiNoko/bas/aminoko_turn1_loop.bas",
	nullptr,
	nullptr,
	"/scene/amiNoko/bas/aminoko_turn2_loop.bas",
	nullptr,
	nullptr,
};

const char** TAmiNoko::getBasNameTable() const { return amiNoko_bastable; }
