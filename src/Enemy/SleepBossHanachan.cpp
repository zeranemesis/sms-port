#include <Enemy/SleepBossHanachan.hpp>

// symbol confirmed by reading the disassembly (li r3, sleepBossHanachan_bastable@sda21)
extern const char* sleepBossHanachan_bastable[];

void TSleepBossHanachan::calcRootMatrix() {}

const char** TSleepBossHanachan::getBasNameTable() const
{
	return sleepBossHanachan_bastable;
}

// TODO: the remaining 12 functions in this unit (TSleepBossHanachan::init,
// ::startFall, ::~TSleepBossHanachan, TSleepBossHanachanManager and its
// ::createModelData/::~TSleepBossHanachanManager, and the two nerve classes
// TNerveSBH_Fall / TNerveSBH_SleepContinue with their theNerve()/execute()/
// dtors) were not attempted in the time budget available for this pass.
