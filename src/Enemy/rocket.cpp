
#include <Enemy/Rocket.hpp>

static const char* rocket_bastable[] = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};

const char** TRocket::getBasNameTable() const { return rocket_bastable; }

TRocket::TRocket(const char* name)
    : TSmallEnemy(name)
    , unk1A0(0)
    , unk1A1(0)
{
}
