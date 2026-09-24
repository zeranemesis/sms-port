
#include <Enemy/Enemy.hpp>
#include <Strategic/Spine.hpp>

class TNerveWireTrapWait {
public:
	BOOL execute(TSpineBase<TLiveActor>*) const;
};

BOOL TNerveWireTrapWait::execute(TSpineBase<TLiveActor>* spine) const
{
	const int waitTime = *reinterpret_cast<const int*>(
	    reinterpret_cast<const char*>(spine->getBody()) + 0x174);
	if (waitTime < spine->getTime())
		return TRUE;
	return FALSE;
}
