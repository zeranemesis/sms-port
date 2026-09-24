
#include <Enemy/Enemy.hpp>
#include <Strategic/Spine.hpp>

class JPABaseEmitter;
class JPABaseParticle;

class TChuuHana {
public:
	bool isFindMario(float);
};

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
