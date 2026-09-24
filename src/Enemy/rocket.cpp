
#include <Enemy/Rocket.hpp>

class TRocketManager : public TSmallEnemyManager {
public:
	void loadAfter();
};

void TRocketManager::loadAfter()
{
	JDrama::TNameRef::loadAfter();
}

static const char* rocket_bastable[] = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};

const char** TRocket::getBasNameTable() const { return rocket_bastable; }

void TRocket::behaveToWater(THitActor*) { attackToMario(); }

TRocket::TRocket(const char* name)
    : TSmallEnemy(name)
    , unk1A0(0)
    , unk1A1(0)
{
}
