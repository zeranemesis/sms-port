
#include <Enemy/Enemy.hpp>

class TKazekun {
public:
	bool isCollidMove(THitActor*);
	const char** getBasNameTable() const;
};

bool TKazekun::isCollidMove(THitActor*) { return false; }

static const char* Kazekun_bastable[] = {
	"/scene/Kazekun/bas/kazekun_appear.bas",
	"/scene/Kazekun/bas/kazekun_attack.bas",
	nullptr,
	"/scene/Kazekun/bas/kazekun_vanish.bas",
	"/scene/Kazekun/bas/kazekun_wait.bas",
};

const char** TKazekun::getBasNameTable() const { return Kazekun_bastable; }
