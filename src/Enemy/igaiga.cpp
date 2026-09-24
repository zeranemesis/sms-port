
#include <Enemy/Igaiga.hpp>

static const char* igaiga_bastable[] = {
	"/scene/igaiga/bas/igaiga_down1.bas",
	"/scene/igaiga/bas/igaiga_down2.bas",
	nullptr,
	nullptr,
	"/scene/igaiga/bas/igaiga_shoot1.bas",
	"/scene/igaiga/bas/igaiga_waterdown1.bas",
	"/scene/igaiga/bas/igaiga_waterhit1.bas",
	nullptr,
};

const char** TIgaiga::getBasNameTable() const { return igaiga_bastable; }

static const char* gorogoro_bastable[] = { nullptr, nullptr, nullptr, nullptr };

const char** TGorogoro::getBasNameTable() const { return gorogoro_bastable; }

void TRollEnemy::bound() { }
