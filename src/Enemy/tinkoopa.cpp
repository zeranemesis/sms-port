
class TTinKoopa {
public:
	bool hasMapCollision() const;
};

class TTinKoopaManager {
public:
	void* createEnemyInstance();
};

bool TTinKoopa::hasMapCollision() const { return true; }

void* TTinKoopaManager::createEnemyInstance() { return nullptr; }
