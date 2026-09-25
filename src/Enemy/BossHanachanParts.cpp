#include <Enemy/BossHanachan.hpp>
#include <MarioUtil/MapUtil.hpp>

TLiveActor* TBossHanachanPartsBase::getSandActor_() const
{
	const TLiveActor* actor = SMS_GetGroundActor(mGroundPlane, 0x400000CD);
	if (actor == nullptr)
		actor = SMS_GetGroundActor(mGroundPlane, 0x400000CB);
	return const_cast<TLiveActor*>(actor);
}
