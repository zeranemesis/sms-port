#include <Enemy/WireTrap.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <Strategic/Spine.hpp>

// TODO: TWireTrapManager::createModelData, ::load, ctor and most of TWireTrap
// are not yet reconstructed. Only the small accessor functions below have
// been verified against the target assembly so far.

// TODO: execute() bodies below are placeholders -- their real logic has not
// been reverse-engineered yet. They exist only so theNerve() can be emitted
// and inlined at TWireTrap::getNerveFromMode's call sites, matching the
// target's control flow there. Do not treat these as matching nerve bodies.
DEFINE_NERVE(TNerveWireTrapWait, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();
	if (self->mWaitTime < spine->getTime())
		return TRUE;
	return FALSE;
}

DEFINE_NERVE(TNerveWireTrapSearch, TLiveActor)
{
	return FALSE;
}

DEFINE_NERVE(TNerveWireTrapOnewayMove, TLiveActor)
{
	return FALSE;
}

DEFINE_NERVE(TNerveWireTrapReturnMove, TLiveActor)
{
	return FALSE;
}

TWireBinder* TWireTrap::getWireBinder() const
{
	return (TWireBinder*)mBinder;
}

TNerveBase<TLiveActor>* TWireTrap::getNerveFromMode(int mode)
{
	switch (mode) {
	case 0:
		return (TNerveBase<TLiveActor>*)&TNerveWireTrapReturnMove::theNerve();
	case 1:
		return (TNerveBase<TLiveActor>*)&TNerveWireTrapOnewayMove::theNerve();
	case 2:
		return (TNerveBase<TLiveActor>*)&TNerveWireTrapSearch::theNerve();
	default:
		return nullptr;
	}
}

const JGeometry::TVec3<f32>& TWireTrap::getWireDir() const
{
	return getWireBinder()->getDir();
}
