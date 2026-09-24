
#include <Enemy/Enemy.hpp>
#include <Strategic/Spine.hpp>

class TNerveElecCarapaceWait {
public:
	int execute(TSpineBase<TLiveActor>*) const;
};

class TElecNokonoko {
public:
	const char** getBasNameTable() const;
};

static const char* dennoko_bastable[] = {
	"/scene/dennoko/bas/dennoko_catch1.bas",
	"/scene/dennoko/bas/dennoko_down1.bas",
	"/scene/dennoko/bas/dennoko_elec_down1.bas",
	"/scene/dennoko/bas/dennoko_hit1.bas",
	nullptr,
	nullptr,
	"/scene/dennoko/bas/dennoko_mogaki1_loop.bas",
	"/scene/dennoko/bas/dennoko_mogaki1_start.bas",
	nullptr,
	nullptr,
	"/scene/dennoko/bas/dennoko_run1_loop.bas",
	nullptr,
	"/scene/dennoko/bas/dennoko_shoot1.bas",
	"/scene/dennoko/bas/dennoko_supply1.bas",
	nullptr,
	"/scene/dennoko/bas/dennoko_turn1_loop.bas",
	nullptr,
	nullptr,
};

const char** TElecNokonoko::getBasNameTable() const { return dennoko_bastable; }

class TElecCarapace {
public:
	void rebirth();
	void recoverScale();
};

void TElecCarapace::rebirth() { }

void TElecCarapace::recoverScale() { }

int TNerveElecCarapaceWait::execute(TSpineBase<TLiveActor>* spine) const
{
	if (spine->getTime() > 60)
		return 1;
	return 0;
}
