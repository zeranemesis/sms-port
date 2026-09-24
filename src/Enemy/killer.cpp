
class TFlyEnemy {
public:
	void flyBehavior();
	void setChaseFlyAnm();
	void setNormalFlyAnm();
	void setAfterDeadEffect();
};

class TKiller {
public:
	void forceKill();
	const char** getBasNameTable() const;
};

static const char* killer_bastable[] = {
	"/scene/killer/bas/downkiller_down1.bas",
	nullptr,
	nullptr,
	"/scene/killer/bas/killer_search1.bas",
	nullptr,
};

const char** TKiller::getBasNameTable() const { return killer_bastable; }

void TFlyEnemy::flyBehavior() { }

void TFlyEnemy::setChaseFlyAnm() { }

void TFlyEnemy::setNormalFlyAnm() { }

void TFlyEnemy::setAfterDeadEffect() { }

void TKiller::forceKill() { }
