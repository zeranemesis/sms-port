
#include <Enemy/WalkerEnemy.hpp>

class THauntLeg : public TWalkerEnemy {
public:
	THauntLeg(const char*);
	virtual void setRunAnm();
	virtual void setWalkAnm();
	virtual void setWaitAnm();
	virtual void setGenerateAnm();
};

void THauntLeg::setRunAnm() { setBckAnm(1); }
void THauntLeg::setWalkAnm() { setBckAnm(1); }
void THauntLeg::setWaitAnm() { setBckAnm(2); }
void THauntLeg::setGenerateAnm() { setBckAnm(0); }
