#include <MoveBG/MapObjMare.hpp>

#include <M3DUtil/MActor.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

TCogwheelScale::TCogwheelScale(const char* name)
	: TMapObjBase(name)
	, unk138(0.0f)
	, unk13C(0.0f)
	, unk140(0.0f)
	, unk144(0.0f)
	, unk148(0.0f)
	, unk14C(0.01f)
	, unk150(5.0f)
	, unk154(0)
	, unk158(nullptr)
{
}

u32 TCogwheelScale::touchWater(THitActor* param_1)
{
	// TODO: unconfirmed
	return 0;
}

BOOL TCogwheelScale::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: unconfirmed
	return FALSE;
}

void TCogwheelScale::touchPlayer(THitActor* param_1)
{
	// TODO: unconfirmed
}

void TCogwheelScale::control()
{
	// TODO: unconfirmed
	TMapObjBase::control();
}

TCogwheel::TCogwheel(const char* name) : TMapObjBase(name) { }

void TCogwheel::initMapObj()
{
	// TODO: unconfirmed
	TMapObjBase::initMapObj();
}

void TCogwheel::control()
{
	// TODO: unconfirmed
	TMapObjBase::control();
}

void TCogwheel::calc()
{
	// TODO: unconfirmed
	TMapObjBase::calc();
}

void TCogwheel::draw() const
{
	// TODO: unconfirmed
}

void TCogwheel::initDraw() const
{
	// TODO: unconfirmed
}

void TCogwheel::rebound()
{
	// TODO: unconfirmed
}

TMapObjElasticCode::TMapObjElasticCode(const char* name) : TMapObjBase(name)
{
}

void TMapObjElasticCode::initMapObj()
{
	// TODO: unconfirmed
	TMapObjBase::initMapObj();
}

void TMapObjElasticCode::control()
{
	// TODO: unconfirmed
	TMapObjBase::control();
}

void TMapObjElasticCode::draw() const
{
	// TODO: unconfirmed
}

TMapObjGrowTree::TMapObjGrowTree(const char* name) : TMapObjBase(name) { }

void TMapObjGrowTree::initMapObj()
{
	// TODO: unconfirmed
	TMapObjBase::initMapObj();
}

void TMapObjGrowTree::loadAfter()
{
	// TODO: unconfirmed
	TMapObjBase::loadAfter();
}

void TMapObjGrowTree::control()
{
	// TODO: unconfirmed
	TMapObjBase::control();
}

u32 TMapObjGrowTree::touchWater(THitActor* param_1)
{
	// TODO: unconfirmed
	return 0;
}

void TMapObjGrowTree::updateHeight()
{
	// TODO: unconfirmed
}

void TMapObjGrowTree::getGrowHeightFromRate(float param_1) const
{
	// TODO: unconfirmed
}

TWireBell::TWireBell(const char* name) : TMapObjBase(name) { }

void TWireBell::loadAfter()
{
	// TODO: unconfirmed
	TMapObjBase::loadAfter();
}

void TWireBell::control()
{
	// TODO: unconfirmed
	TMapObjBase::control();
}

void TWireBell::draw() const
{
	// TODO: unconfirmed
}

void TWireBell::initDraw() const
{
	// TODO: unconfirmed
}

TMapObjPuncher::TMapObjPuncher(const char* name) : TMapObjBase(name) { }

void TMapObjPuncher::load(JSUMemoryInputStream& stream)
{
	// TODO: unconfirmed
	TMapObjBase::load(stream);
}

void TMapObjPuncher::control()
{
	// TODO: unconfirmed
	TMapObjBase::control();
}

void TMapObjPuncher::touchPlayer(THitActor* param_1)
{
	// TODO: unconfirmed
}

TMuddyBoat::TMuddyBoat(const char* name) : TMapObjBase(name) { }

void TMuddyBoat::initMapObj()
{
	// TODO: unconfirmed
	TMapObjBase::initMapObj();
}

u32 TMuddyBoat::getSDLModelFlag() const
{
	// TODO: unconfirmed value
	return 0;
}

void TMuddyBoat::calc()
{
	// TODO: unconfirmed
	TMapObjBase::calc();
}

void TMuddyBoat::control()
{
	// TODO: unconfirmed
	TMapObjBase::control();
}

void TMuddyBoat::bind()
{
	// TODO: unconfirmed
}

void TMuddyBoat::kill()
{
	// TODO: unconfirmed
	TMapObjBase::kill();
}

void TMuddyBoat::calcRootMatrix() { }

void TMuddyBoat::moveByWater()
{
	// TODO: unconfirmed
}

void TMuddyBoat::touchWall(JGeometry::TVec3<float>* param_1,
                            const TBGWallCheckRecord& param_2)
{
	// TODO: unconfirmed
}

void TMuddyBoat::bindToWall(const JGeometry::TVec3<float>& param_1,
                             float param_2, JGeometry::TVec3<float>* param_3)
{
	// TODO: unconfirmed
}

TMareFall::TMareFall(const char* name) : TMapObjBase(name) { }

void TMareFall::calc()
{
	// TODO: unconfirmed
	TMapObjBase::calc();
}

void TMareFall::load(JSUMemoryInputStream& stream)
{
	// TODO: unconfirmed
	TMapObjBase::load(stream);
}

TMareCork::TMareCork(const char* name) : TMapObjBase(name) { }

void TMareCork::loadAfter()
{
	// TODO: unconfirmed
	TMapObjBase::loadAfter();
}

void TMareCork::moveObject()
{
	// TODO: unconfirmed
}

void TMareCork::calcRootMatrix()
{
	// TODO: unconfirmed
}

MtxPtr TMareCork::getTakingMtx()
{
	// mNodeMatrices[2] corresponds to the cork joint
	return mMActor->getModel()->getAnmMtx(2);
}

void TMareCork::drawObject(JDrama::TGraphics* param_1)
{
	// TODO: unconfirmed
}

TMareEventPoint::TMareEventPoint(const char* name) : THitActor(name) { }

BOOL TMareEventPoint::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: unconfirmed
	return FALSE;
}

void TMareEventPoint::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);
	initHitActor(0x40000236, 0, 0, 0.0f, 0.0f, 300.0f, 600.0f);
}
