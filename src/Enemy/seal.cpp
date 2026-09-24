
#include <Enemy/EnemyManager.hpp>

class TSealManager : public TEnemyManager {
public:
	void load(JSUMemoryInputStream&);
};

void TSealManager::load(JSUMemoryInputStream& stream) { TEnemyManager::load(stream); }
