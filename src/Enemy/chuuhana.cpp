
#include <Enemy/Enemy.hpp>
#include <Strategic/Spine.hpp>

class JPABaseEmitter;
class JPABaseParticle;

class TChuuHana {
public:
	bool isFindMario(float);
	const char** getBasNameTable() const;
};

static const char* tyuhana_bastable[] = {
	"/scene/tyuhana/bas/tyuhana_chance_end.bas",
	nullptr,
	"/scene/tyuhana/bas/tyuhana_chance_start.bas",
	"/scene/tyuhana/bas/tyuhana_jump.bas",
	"/scene/tyuhana/bas/tyuhana_push.bas",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/tyuhana/bas/tyuhana_walk.bas",
};

const char** TChuuHana::getBasNameTable() const { return tyuhana_bastable; }

class TChuuHanaAseParCallback {
public:
	void draw(JPABaseEmitter*, JPABaseParticle*);
};

class TNerveChuuHanaObject {
public:
	BOOL execute(TSpineBase<TLiveActor>*) const;
};

bool TChuuHana::isFindMario(float) { return false; }

void TChuuHanaAseParCallback::draw(JPABaseEmitter*, JPABaseParticle*) { }

BOOL TNerveChuuHanaObject::execute(TSpineBase<TLiveActor>*) const
{
	return FALSE;
}
