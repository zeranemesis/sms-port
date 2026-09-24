
#include <Enemy/SmallEnemy.hpp>

class TPopo {
public:
	const char** getBasNameTable() const;
};

static const char* popo_bastable[] = {
	"/scene/popo/bas/popo_chase.bas",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/popo/bas/popo_jump.bas",
	"/scene/popo/bas/popo_wait.bas",
};

const char** TPopo::getBasNameTable() const { return popo_bastable; }
