#include "MoveBG/MapObjCorona.hpp"
#include "MoveBG/MapObjBase.hpp"
#include <JSystem/JMath.hpp>
#include <M3DUtil/MActor.hpp>
#include <System/Particles.hpp>

void TBathtub::loadAfter()
{
	SMS_LoadParticle("/scene/map/map/ms_lkp_yuge1.jpa", 0x1BE);
	SMS_LoadParticle("/scene/map/map/ms_kp_funsui.jpa", 0x1BF);
	SMS_LoadParticle("/scene/map/map/ms_kp_break_a.jpa", 0x0F6);
	SMS_LoadParticle("/scene/map/map/ms_kp_break_b.jpa", 0x0F7);
}

void TBathtub::hipdrop(const JGeometry::TVec3<f32>&) { }

void TBathtub::quake(const JGeometry::TVec3<f32>&) { }

int TBathtub::getNumGripsDead() const
{
	int count = 0;
	if (reinterpret_cast<const u8*>(unk168[0])[0x249] == 0)
		count = 1;
	if (reinterpret_cast<const u8*>(unk168[1])[0x249] == 0)
		count += 1;
	if (reinterpret_cast<const u8*>(unk168[2])[0x249] == 0)
		count += 1;
	if (reinterpret_cast<const u8*>(unk168[3])[0x249] == 0)
		count += 1;
	if (reinterpret_cast<const u8*>(unk168[4])[0x249] == 0)
		count += 1;
	return count;
}

void TBathtub::tumble(f32 angle, f32 force)
{
	if (reinterpret_cast<const u8*>(this)[0x299] == 0) {
		f32 amount = force * 0.0001f;
		s32 index = static_cast<u16>(static_cast<s32>(182.04445f * angle)) >> jmaSinShift;
		unk1E8 += amount * jmaCosTable[index];
		*reinterpret_cast<volatile f32*>(&unk1EC) += 0.0f;
		unk1F0 += amount * -jmaSinTable[index];
	}
}

MtxPtr TBathtub::getTakingMtx()
{
	return mMActor->getModel()->getAnmMtx(mMarioJntIdx);
}

MtxPtr TBathtub::getSubmarineMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mSubmarineJntIdx);
}

MtxPtr TBathtub::getPeachMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mDuckJntIdx);
}

MtxPtr TBathtub::getKoopaJrMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mJuniorJntIdx);
}

BOOL TBathtub::receiveMessage(THitActor* sender, u32 message) { return false; }

Mtx* TBathtub::getRootJointMtx() const
{
    if (reinterpret_cast<const u8*>(this)[0x299] != 0)
        return *reinterpret_cast<Mtx**>(reinterpret_cast<u8*>(getModel()) + 0x58);
    return reinterpret_cast<Mtx*>(reinterpret_cast<u8*>(getModel()) + 0x20);
}

void TBathtub::perform(u32 cue, JDrama::TGraphics* graphics) { }

void TBathtub::control() { }

void TBathtub::calcBathtubData() { }

void TBathtub::setupCollisions_() { }

void TBathtub::removeCollisions_() { } // Unused

void TBathtub::startDemo() { }

bool TBathtub::allowsTumble() const { return false; }

void TBathtub::calcRootMatrix() { }

bool TBathtub::getNearGrip(const JGeometry::TVec3<f32>&, f32, f32*) const
{
	return false;
}

u8 TBathtub::getNextJuncture(const JGeometry::TVec3<f32>&,
                             const JGeometry::TVec3<f32>&) const
{
	return 0;
}

u8 TBathtub::getNextGrip(const JGeometry::TVec3<f32>&,
                         const JGeometry::TVec3<f32>&, f32, f32*) const
{
	return 0;
}

void TBathtub::updatePosture_() { }

TBathtub::TBathtub(const char* name)
    : TMapObjBase(name)
{
}

void TBathtub::load(JSUMemoryInputStream&) { }

u8 TBathtub::getNumKillerLaunchable() const { return 0; }

bool TBathtub::isKillerAttackable() const { return unk248 <= 0; }

u8 TBathtub::getNumKillerBurstable() const { return 0; }

// Unused
bool TBathtub::isBreaking() const { return false; }

// Unused
bool TBathtub::isKillerLaunchable() const { return false; }

// Unused
void TBathtub::showMessage(u32) { }

// Unused
u8 TBathtub::getNearJuncture(const JGeometry::TVec3<f32>&) const { return 0; }

// Unused
MtxPtr TBathtub::getKoopaMtxInDemo() { return nullptr; }

// Unused
MtxPtr TBathtub::getWaterMtx(s32) { return nullptr; }

// Unused
MtxPtr TBathtub::getShineEffectMtx() { return nullptr; }

// Unused
MtxPtr TBathtub::getShineMtx() { return nullptr; }

// Unused
void TBathtub::liftMario(const JGeometry::TVec3<f32>&) { }

// Unused
void TBathtub::trample(const JGeometry::TVec3<f32>&) { }
