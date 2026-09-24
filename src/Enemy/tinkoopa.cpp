
class TTinKoopa {
public:
	bool hasMapCollision() const;
	const char** getBasNameTable() const;
};

static const char* tinkoopa_bastable[] = {
	"/scene/tinkoopa/bas/tinkoopa_break1.bas",
	"/scene/tinkoopa/bas/tinkoopa_break2.bas",
	"/scene/tinkoopa/bas/tinkoopa_break3.bas",
	"/scene/tinkoopa/bas/tinkoopa_break4.bas",
	nullptr,
	"/scene/tinkoopa/bas/tinkoopa_damage1.bas",
	"/scene/tinkoopa/bas/tinkoopa_damage2.bas",
	"/scene/tinkoopa/bas/tinkoopa_damage3.bas",
	"/scene/tinkoopa/bas/tinkoopa_damage4.bas",
	nullptr,
	nullptr,
	nullptr,
	"/scene/tinkoopa/bas/tinkoopa_wait1.bas",
	"/scene/tinkoopa/bas/tinkoopa_wait2.bas",
	"/scene/tinkoopa/bas/tinkoopa_wait3.bas",
	"/scene/tinkoopa/bas/tinkoopa_wait4.bas",
	"/scene/tinkoopa/bas/tinkoopa_wait5.bas",
};

const char** TTinKoopa::getBasNameTable() const { return tinkoopa_bastable; }

class TTinKoopaManager {
public:
	bool hasMapCollision() const;
	void* createEnemyInstance();
};

bool TTinKoopaManager::hasMapCollision() const { return true; }

bool TTinKoopa::hasMapCollision() const { return true; }

void* TTinKoopaManager::createEnemyInstance() { return nullptr; }
