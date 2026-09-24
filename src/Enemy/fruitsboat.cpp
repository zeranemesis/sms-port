
#include <Enemy/Enemy.hpp>
#include <M3DUtil/MActor.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

class TFruitsBoat {
public:
	BOOL receiveMessage(THitActor*, u32);
	Mtx* getRootJointMtx() const;
};

BOOL TFruitsBoat::receiveMessage(THitActor*, u32) { return FALSE; }

Mtx* TFruitsBoat::getRootJointMtx() const
{
	// The full TFruitsBoat hierarchy is not declared in this TU.
	return (Mtx*)((TLiveActor*)this)->getMActor()->getModel()->getAnmMtx(0);
}
