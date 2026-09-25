
#include <Enemy/Igaiga.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <Player/MarioAccess.hpp>

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

bool TRollEnemy::isRolling() { return false; }

void TRollEnemy::rollSE() { }

void TRollEnemy::boundSE() { }

void TRollEnemy::setAfterDeadEffect() { }

void TRollEnemy::attackToMario()
{
	SMS_SendMessageToMario(this, 0xE);
}
