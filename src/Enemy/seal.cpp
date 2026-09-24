
#include <Enemy/EnemyManager.hpp>

class TSealManager : public TEnemyManager {
public:
	TSealManager(const char*);
	void createModelData();
	void load(JSUMemoryInputStream&);
};

TSealManager::TSealManager(const char* name)
    : TEnemyManager(name)
{
}

void TSealManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
	    { "gene_orange_model1.bmd", 0x11210000, 0 },
	    { nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TSealManager::load(JSUMemoryInputStream& stream) { TEnemyManager::load(stream); }
