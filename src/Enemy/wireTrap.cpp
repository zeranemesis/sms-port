
#include <Enemy/Enemy.hpp>
#include <Enemy/WireBinder.hpp>
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

class TWireTrap : public TSpineEnemy {
public:
	TWireTrap(const char*);
	TWireBinder* getWireBinder() const;
	JGeometry::TVec3<f32>* getWireDir() const;
};

TWireBinder* TWireTrap::getWireBinder() const
{
	return (TWireBinder*)mBinder;
}

JGeometry::TVec3<f32>* TWireTrap::getWireDir() const
{
	return (JGeometry::TVec3<f32>*)((char*)mBinder + 8);
}
