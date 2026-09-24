
#include <Enemy/EnemyManager.hpp>
#include <MSound/MSoundSE.hpp>
#include <Strategic/Nerve.hpp>
#include <Strategic/ObjManager.hpp>

class TAnimalBird {
public:
	const char** getBasNameTable() const;
};

static const char* bird_bastable[] = {
	nullptr,
	"/scene/bird/bas/bird_fly.bas",
	"/scene/bird/bas/bird_open.bas",
	nullptr,
	nullptr,
	"/scene/bird/bas/bird_start.bas",
	"/scene/bird/bas/bird_stop.bas",
	nullptr,
	nullptr,
};

const char** TAnimalBird::getBasNameTable() const { return bird_bastable; }

class TAnimalBirdManager : public TEnemyManager {
public:
	TAnimalBirdManager(const char*);
	virtual void loadAfter();
	virtual void createModelData();
};

TAnimalBirdManager::TAnimalBirdManager(const char* name)
	: TEnemyManager(name)
{
}

void TAnimalBirdManager::loadAfter()
{
	TEnemyManager::loadAfter();
	MSoundSESystem::MSRandPlay::createRandPlayVec(0x3869, getObjNum());
	MSoundSESystem::MSRandPlay::createRandPlayVec(0x3870, getObjNum());
}

void TAnimalBirdManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
	    {"bird_man.bmd", 0x10210000, 0}, {nullptr, 0, 0}};
	createModelDataArray(entry);
}

class TLiveActor;
template <class T> class TSpineBase;

class TNerveAnimalBirdLanding : public TNerveBase<TLiveActor> {
public:
	virtual ~TNerveAnimalBirdLanding();
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
};

TNerveAnimalBirdLanding::~TNerveAnimalBirdLanding() { }
