#include "MoveBG/MapObjCorona.hpp"


// rogue include: the original TU opens .rodata with the dummy string pair from
// System/DummyStrings.hpp plus the four MtxCalcType names; without them every
// string offset in this object is shifted.
#include <M3DUtil/InfectiousStrings.hpp>
#include "MoveBG/MapObjBase.hpp"
#include <math.h>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JMath.hpp>
#include <M3DUtil/MActor.hpp>
#include <Camera/CameraShake.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Player/MarioAccess.hpp>
#include <System/Particles.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/JGeometry/JGMatrix33.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <stdio.h>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

extern "C" u8 allowsLaunch__6TKoopaCFv(void*);
extern "C" void getDown__6TKoopaFv(void*);
extern "C" void stagger__6TKoopaFb(void*, bool);
extern "C" void* __ct__14TBathtubParamsFv(void*);

// ---------------------------------------------------------------------------
// The whole file below is in the REVERSE of the address order the linker map
// records for MoveBG.a/MapObjCorona.cpp, because this TU is built with
// -inline deferred. tools/validate-symbol-order.py is the authority.
// ---------------------------------------------------------------------------

// Not in the map: the TU-local helper both getNumKiller{Burstable,Launchable}
// use to ask the resident Koopa whether he may be launched. Dead in the target
// build, so it is dead-stripped and has no entry there either.
static bool bathtubKoopaAllowsLaunch()
{
	return allowsLaunch__6TKoopaCFv(JDrama::TNameRefGen::search("クッパ")) != 0;
}

// ---------------------------------------------------------------------------
// TBathtubParams (map position 67)
// ---------------------------------------------------------------------------

TBathtubParams::TBathtubParams()
    : TParams("/MapObj/bathtub.prm")
    , PARAM_INIT(resetGrip, 0)
    , PARAM_INIT(trampleRelease, 10)
    , PARAM_INIT(trampleRecover, 10)
    , PARAM_INIT(quakeRelease, 500)
    , PARAM_INIT(quakeRecover, 500)
    , PARAM_INIT(hipdropRelease, 35)
    , PARAM_INIT(hipdropRecover, 35)
    , PARAM_INIT(breakCount0, 750)
    , PARAM_INIT(breakCount1, 710)
    , PARAM_INIT(breakCount2, 685)
    , PARAM_INIT(breakCount3, 655)
    , PARAM_INIT(launchStopCount, 1000)
    , PARAM_INIT(animSpeed0, 0.15f)
    , PARAM_INIT(animSpeed1, 0.15f)
    , PARAM_INIT(animSpeed2, 0.15f)
    , PARAM_INIT(animSpeed3, 0.15f)
    , PARAM_INIT(animSpeed4, 0.22f)
    , PARAM_INIT(shake, 0.0f)
    , PARAM_INIT(watermark, 0.3f)
    , PARAM_INIT(maxAngle, 35.0f)
    , PARAM_INIT(angleVelDamp, 0.93f)
    , PARAM_INIT(rebound, 0.0005f)
    , PARAM_INIT(shakeDamp, 0.93f)
    , PARAM_INIT(marioWeight, 0.01f)
    , PARAM_INIT(marioDropWeight, 5.0f)
    , PARAM_INIT(outerHeight, 20.0f)
{
	TParams::load(mPrmPath);
}

// ---------------------------------------------------------------------------
// TBathtubGripParts (map positions 63-66)
// ---------------------------------------------------------------------------

// Unused
TBathtubGripParts::TBathtubGripParts(const char* name, int index,
                                     TBathtubGrip* grip)
    : TLiveActor(name)
    , mGrip(grip)
    , unkF8(index)
{
}

// Unused
TBathtubGripPartsFragile::TBathtubGripPartsFragile(int index,
                                                   TBathtubGrip* grip)
    : TBathtubGripParts("バスタブの足場の一部（弱点）", index, grip)
{
}

// Unused
TBathtubGripPartsHard::TBathtubGripPartsHard(int index, TBathtubGrip* grip)
    : TBathtubGripParts("バスタブの足場の一部（壊れない）", index, grip)
{
}

Mtx* TBathtubGripParts::getRootJointMtx() const
{
	s32 index = mGrip->unk200[unkF8];
	// getAnmMtx() is `mNodeMatrices[idx]`, i.e. the same address the target
	// computes; only the return type differs (Mtx* vs MtxPtr).
	return reinterpret_cast<Mtx*>(getModel()->getAnmMtx(index));
}

BOOL TBathtubGripPartsFragile::receiveMessage(THitActor*, u32 message)
{
	return mGrip->receiveMessage(nullptr, message);
}

BOOL TBathtubGripPartsHard::receiveMessage(THitActor*, u32 message)
{
	if (message == 3)
		message = 1;
	return mGrip->receiveMessage(nullptr, message);
}

// ---------------------------------------------------------------------------
// TBathtubGrip (map positions 46-60)
// ---------------------------------------------------------------------------

void TBathtubGrip::kill()
{
	unk24A = 1;
	makeObjDead();

	for (s32 i = 0; i < 17; i++)
		unk164[i]->remove();
	for (s32 i = 0; i < 5; i++)
		unk150[i]->remove();
}

// Unused
void TBathtubGrip::reset() { }

// Between reset() and the ctor in source order: the object (which is the
// reverse of this file) needs it at map position 58, i.e. after the
// TBathtubGrip ctor and before reset().
TBathtubGripParts::~TBathtubGripParts() { }

TBathtubGrip::TBathtubGrip(TBathtub* bathtub, f32 angle,
                           MActorAnmData* anmData, const char* name)
    : TMapObjBase(name)
{
	mStandMActor = new MActor(anmData);
	mStandMActor->setModel(
	    new J3DModel(J3DModelLoaderDataBase::load(
	                     JKRFileLoader::getGlbResource(
	                         "/scene/map/map/stand_effect/stand_effect.bmd"),
	                     0x5005),
	                 0, 1),
	    0x5005);

	unk254  = 0;
	mBathtub = bathtub;
	unk24C  = angle;

	initAndRegister("stand_break");
	calcRootMatrix();
	getModel()->calc();

	JUTNameTab* nameTab = getModel()->getModelData()->getJointName();

	char partName[0x48];
	char colName[0x100];

	for (s32 i = 0; i < 17; i++) {
		sprintf(partName, "c%d", i + 1);
		sprintf(colName, "/scene/mapObj/stand_break_%s.col", partName);

		unk200[i] = nameTab->getIndex(partName);
		unk164[i] = new TMapCollisionMove();
		unk1BC[i] = new TBathtubGripPartsHard(i, this);
		unk164[i]->init(colName, 0, unk1BC[i]);

		if (i < 5) {
			sprintf(partName, "b%d", i + 1);
			sprintf(colName, "/scene/mapObj/stand_break_%s.col", partName);

			unk150[i] = new TMapCollisionMove();
			unk1A8[i] = new TBathtubGripPartsFragile(i, this);
			unk150[i]->init(colName, 0, unk1A8[i]);
		}
	}

	offLiveFlag(LIVE_FLAG_DEAD);
	unk248 = 0;
	unk24A = 0;
	unk249 = 1;
	unk24B = 0;

	startAnim(0);

	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(0);
	if (ctrl != nullptr) {
		ctrl->setFrame(0.0f);
		ctrl->setRate(0.0f);
	}

	unk250 = 1.0f;
	unk258 = 100;
	unk260 = 0;
}

void TBathtubGrip::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TMapObjBase::perform(cue, graphics);

	if (unk260 != 0)
		return;

	if (cue & 1) {
		PSMTXCopy((MtxPtr)getRootJointMtx(),
		          mStandMActor->getModel()->getBaseTRMtx());
		if (unk254 > 0 || unk248 != 0) {
			if (mStandMActor->curAnmEndsNext(0, nullptr))
				unk260 = 1;
		}
	}

	mStandMActor->perform(cue, graphics);
}

// Unused
bool TBathtubGrip::isCracking() const
{
	return false;
}

// Unused
void TBathtubGrip::startCrack() { }

// Unused
void TBathtubGrip::startBreak(f32, int, f32) { }

// TODO: not reconstructed yet (0x324 in the map).
BOOL TBathtubGrip::receiveMessage(THitActor*, u32)
{
	return false;
}

Mtx* TBathtubGrip::getRootJointMtx() const
{
	return reinterpret_cast<Mtx*>(getModel()->getBaseTRMtx());
}

// The grip's own model matrix is rebuilt by baking the grip's stored angle
// (unk24C) into a rotation matrix and then multiplying the bathtub's root
// joint matrix by it.
void TBathtubGrip::calcRootMatrix()
{
	// The destination matrix is fetched before anything else: the target
	// keeps model+0x20 in r31 across the whole body.
	MtxPtr model = getModel()->getBaseTRMtx();

	Mtx mtx;
	MsMtxSetRotRPH(mtx, 0.0f, unk24C, 0.0f);
	mtx[0][3] = 0.0f;
	mtx[1][3] = 0.0f;
	mtx[2][3] = 0.0f;

	MtxPtr bathp = reinterpret_cast<MtxPtr>(mBathtub->getRootJointMtx());

	reinterpret_cast<JGeometry::SMatrix34C<f32>*>(model)->set(
	    // clang-format off
	    bathp[0][0] * mtx[0][0] + bathp[0][1] * mtx[1][0] + bathp[0][2] * mtx[2][0],
	    bathp[0][0] * mtx[0][1] + bathp[0][1] * mtx[1][1] + bathp[0][2] * mtx[2][1],
	    bathp[0][0] * mtx[0][2] + bathp[0][1] * mtx[1][2] + bathp[0][2] * mtx[2][2],
	    bathp[0][0] * mtx[0][3] + bathp[0][1] * mtx[1][3] + bathp[0][2] * mtx[2][3] + bathp[0][3],
	    bathp[1][0] * mtx[0][0] + bathp[1][1] * mtx[1][0] + bathp[1][2] * mtx[2][0],
	    bathp[1][0] * mtx[0][1] + bathp[1][1] * mtx[1][1] + bathp[1][2] * mtx[2][1],
	    bathp[1][0] * mtx[0][2] + bathp[1][1] * mtx[1][2] + bathp[1][2] * mtx[2][2],
	    bathp[1][0] * mtx[0][3] + bathp[1][1] * mtx[1][3] + bathp[1][2] * mtx[2][3] + bathp[1][3],
	    bathp[2][0] * mtx[0][0] + bathp[2][1] * mtx[1][0] + bathp[2][2] * mtx[2][0],
	    bathp[2][0] * mtx[0][1] + bathp[2][1] * mtx[1][1] + bathp[2][2] * mtx[2][1],
	    bathp[2][0] * mtx[0][2] + bathp[2][1] * mtx[1][2] + bathp[2][2] * mtx[2][2],
	    bathp[2][0] * mtx[0][3] + bathp[2][1] * mtx[1][3] + bathp[2][2] * mtx[2][3] + bathp[2][3]
	    // clang-format on
	);
}

// Unused
bool TBathtubGrip::marioIsOn() const
{
	return false;
}

// Unused
void TBathtubGrip::setupCollisions_() { }

// TODO: not reconstructed yet (0x394 in the map).
void TBathtubGrip::control() { }

// ---------------------------------------------------------------------------
// TBathtub (map positions 1-45)
// ---------------------------------------------------------------------------

void TBathtub::loadAfter()
{
	SMS_LoadParticle("/scene/map/map/ms_lkp_yuge1.jpa", 0x1BE);
	SMS_LoadParticle("/scene/map/map/ms_kp_funsui.jpa", 0x1BF);
	SMS_LoadParticle("/scene/map/map/ms_kp_break_a.jpa", 0x0F6);
	SMS_LoadParticle("/scene/map/map/ms_kp_break_b.jpa", 0x0F7);
}

void TBathtub::hipdrop(const JGeometry::TVec3<f32>& position)
{
	if (unk29A != 0)
		return;

	u8* params = reinterpret_cast<u8*>(unk16C);
	if (unk250 > *reinterpret_cast<int*>(params + 0x7C))
		return;

	f32 dx = position.x - *reinterpret_cast<f32*>(reinterpret_cast<u8*>(this) + 0x10C);
	f32 dz = position.z - *reinterpret_cast<f32*>(reinterpret_cast<u8*>(this) + 0x114);
	f32 distance = dz * dz + (dx * dx + 0.0f);
	if (distance > 0.0000038146973f)
		JGeometry::TUtil<f32>::inv_sqrt(distance);

	unk250 = *reinterpret_cast<int*>(params + 0x7C);
	unk258 = *reinterpret_cast<int*>(params + 0x90);
	unk25C = *reinterpret_cast<int*>(params + 0x90);
	unk254 = *reinterpret_cast<u32*>(params + 0x7C);
	stagger__6TKoopaFb(JDrama::TNameRefGen::search("クッパ"), false);
}

void TBathtub::quake(const JGeometry::TVec3<f32>& position)
{
	if (unk29A != 0)
		return;

	f32 dx = position.x - *reinterpret_cast<f32*>(reinterpret_cast<u8*>(this) + 0x10C);
	f32 dz = position.z - *reinterpret_cast<f32*>(reinterpret_cast<u8*>(this) + 0x114);
	f32 distance = dz * dz + (dx * dx + 0.0f);
	if (distance > 0.0000038146973f) {
		JGeometry::TUtil<f32>::inv_sqrt(distance);
	}

	unk24C = 300;
	u8* params = reinterpret_cast<u8*>(unk16C);
	unk250 = *reinterpret_cast<int*>(params + 0x54);
	unk258 = *reinterpret_cast<int*>(params + 0x68);
	unk25C = *reinterpret_cast<int*>(params + 0x68);
	unk254 = *reinterpret_cast<u32*>(params + 0x7C);
	unk248 = *reinterpret_cast<int*>(params + 0xF4);

	gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x25), 1.0f);
	gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x26), 1.0f);
	SMSRumbleMgr->start(4, static_cast<f32*>(nullptr));
	JGeometry::TVec3<f32> velocity(0.0f, 1.0f, 0.0f);
	SMS_ThrowMario(velocity, 10.0f);
	getDown__6TKoopaFv(JDrama::TNameRefGen::search("クッパ"));
}

int TBathtub::getNumGripsDead() const
{
	int count = 0;
	if (unk168[0]->unk249 == 0)
		count = 1;
	if (unk168[1]->unk249 == 0)
		count += 1;
	if (unk168[2]->unk249 == 0)
		count += 1;
	if (unk168[3]->unk249 == 0)
		count += 1;
	if (unk168[4]->unk249 == 0)
		count += 1;
	return count;
}

// Unused
void TBathtub::trample(const JGeometry::TVec3<f32>&) { }

// Unused
void TBathtub::liftMario(const JGeometry::TVec3<f32>&) { }

void TBathtub::tumble(f32 angle, f32 force)
{
	if (unk29A == 0) {
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

// Unused
MtxPtr TBathtub::getShineMtx() { return nullptr; }

// Unused
MtxPtr TBathtub::getShineEffectMtx() { return nullptr; }

// Unused
MtxPtr TBathtub::getWaterMtx(int) { return nullptr; }

MtxPtr TBathtub::getSubmarineMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mSubmarineJntIdx);
}

MtxPtr TBathtub::getPeachMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mDuckJntIdx);
}

// Unused
MtxPtr TBathtub::getKoopaMtxInDemo() { return nullptr; }

MtxPtr TBathtub::getKoopaJrMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mJuniorJntIdx);
}

BOOL TBathtub::receiveMessage(THitActor*, u32 message)
{
	switch (message) {
	case 1:
		hipdrop(*gpMarioPos);
		return true;
	case 3:
		hipdrop(*gpMarioPos);
		return true;
	case 0: {
		if (reinterpret_cast<const u8*>(this)[0x29A] == 0) {
			const u8* params = reinterpret_cast<const u8*>(unk16C);
			int timer = *reinterpret_cast<const int*>(params + 0x2C);
			if (unk250 <= timer) {
				unk250 = timer;
				unk258 = *reinterpret_cast<const int*>(params + 0x40);
				unk25C = *reinterpret_cast<const int*>(params + 0x40);
				unk254 = *reinterpret_cast<const u32*>(params + 0x7C);
			}
		}
		return true;
	}
	case 2:
	default:
		return false;
	}
}

Mtx* TBathtub::getRootJointMtx() const
{
    if (reinterpret_cast<const u8*>(this)[0x29A] != 0)
        return *reinterpret_cast<Mtx**>(reinterpret_cast<u8*>(getModel()) + 0x58);
    return reinterpret_cast<Mtx*>(reinterpret_cast<u8*>(getModel()) + 0x20);
}

void TBathtub::perform(u32 cue, JDrama::TGraphics* graphics) { }

void TBathtub::control() { }

void TBathtub::calcBathtubData() { }

void TBathtub::setupCollisions_() { }

// The demo-camera shake callback the map records at 8 bytes, i.e. a bare
// "return 0".
namespace {
s32 CameraDemoCallBack(u32, u32)
{
	return 0;
}
} // namespace

void TBathtub::startDemo() { }

bool TBathtub::allowsTumble() const
{
	f32 gripY = 0.0f;
	if (gpMarioPos == nullptr || !getNearGrip(*gpMarioPos, 18.0f, &gripY))
		return false;

	const u8* self = reinterpret_cast<const u8*>(this);
	f32 dx = gpMarioPos->x - *reinterpret_cast<const f32*>(self + 0x170);
	f32 dy = gpMarioPos->y - *reinterpret_cast<const f32*>(self + 0x174);
	f32 dz = gpMarioPos->z - *reinterpret_cast<const f32*>(self + 0x178);
	f32 x = *reinterpret_cast<const f32*>(self + 0x188) * dx
	       + *reinterpret_cast<const f32*>(self + 0x18C) * dy
	       + *reinterpret_cast<const f32*>(self + 0x190) * dz;
	f32 y = *reinterpret_cast<const f32*>(self + 0x194) * dx
	       + *reinterpret_cast<const f32*>(self + 0x198) * dy
	       + *reinterpret_cast<const f32*>(self + 0x19C) * dz;
	f32 z = *reinterpret_cast<const f32*>(self + 0x1A0) * dx
	       + *reinterpret_cast<const f32*>(self + 0x1A4) * dy
	       + *reinterpret_cast<const f32*>(self + 0x1A8) * dz;
	f32 magnitude = JGeometry::TUtil<f32>::sqrt(x * x + y * y + z * z);
	if (magnitude < 4200.0f || magnitude > 4700.0f)
		return false;
	return true;
}

void TBathtub::calcRootMatrix()
{
	// getModel() is re-fetched in both branches: the target does not hoist it
	// out of the unk29A test.
	if (unk29A != 0) {
		MtxPtr matrix = getModel()->getBaseTRMtx();
		MsMtxSetRotRPH(matrix, 0.0f, mRotation.y, 0.0f);
		matrix[0][3] = mPosition.x;
		matrix[1][3] = mPosition.y;
		matrix[2][3] = mPosition.z;
		return;
	}

	MtxPtr matrix = getModel()->getBaseTRMtx();

	// The target builds the rotation with an explicit 2.0f factor on every
	// quaternion component (fmuls against a 2.0f constant, not `x + x`), and
	// stores the last row back-to-front.
	const f32 x = unk1D8;
	const f32 y = unk1DC;
	const f32 z = unk1E0;
	const f32 w = unk1E4;
	const f32 twoY = 2.0f * y;
	const f32 twoZ = 2.0f * z;
	const f32 twoX = 2.0f * x;
	const f32 twoW = 2.0f * w;
	matrix[0][0] = 1.0f - twoY * y - twoZ * z;
	matrix[0][1] = twoX * y - twoW * z;
	matrix[0][2] = twoX * z + twoW * y;
	matrix[1][0] = twoX * y + twoW * z;
	matrix[1][1] = 1.0f - twoX * x - twoZ * z;
	matrix[1][2] = twoY * z - twoW * x;
	matrix[2][2] = twoX * z - twoW * y;
	matrix[2][1] = twoY * z + twoW * x;
	matrix[2][0] = 1.0f - twoX * x - twoY * y;
	matrix[0][3] = mPosition.x;
	matrix[1][3] = mPosition.y;
	matrix[2][3] = mPosition.z;
}

// Unused
u8 TBathtub::getNearJuncture(const JGeometry::TVec3<f32>&) const { return 0; }

#pragma dont_inline on
bool TBathtub::getNearGrip(const JGeometry::TVec3<f32>& position, f32 radius,
                           f32* gripAngle) const
{
	MtxPtr matrix = reinterpret_cast<MtxPtr>(getRootJointMtx());
	// The target copies the 3x3 block into a stack-local SMatrix33R (whose
	// at() is transposed) and keeps it there across the setLength() call,
	// rather than reading through the model pointer each time.
	JGeometry::SMatrix33R<f32> rot;
	rot.ref(0, 0) = matrix[0][0];
	rot.ref(0, 1) = matrix[1][0];
	rot.ref(0, 2) = matrix[2][0];
	rot.ref(1, 0) = matrix[0][1];
	rot.ref(1, 1) = matrix[1][1];
	rot.ref(1, 2) = matrix[2][1];
	rot.ref(2, 0) = matrix[0][2];
	rot.ref(2, 1) = matrix[1][2];
	rot.ref(2, 2) = matrix[2][2];

	JGeometry::TVec3<f32> delta;
	delta.set(position.x - rot.at(0, 2),
	          position.y - rot.at(1, 2),
	          position.z - rot.at(2, 2));

	f32 localZ = rot.at(0, 1) * delta.x + rot.at(1, 1) * delta.y
	           + rot.at(2, 1) * delta.z;
	f32 localX = rot.at(0, 0) * delta.x + rot.at(1, 0) * delta.y
	           + rot.at(2, 0) * delta.z;
	f32 angle = 0.005493164f * static_cast<f32>(matan(localZ, localX));
	f32 nearest = 180.0f;
	int nearestIndex = 0;
	for (int i = 0; i < 5; ++i) {
		f32 wrapped = static_cast<f32>(fmod(360.0f + (unk150[i] - angle + 180.0f), 360.0f));
		f32 distance = static_cast<f32>(fabs(-180.0f + wrapped));
		if (distance < nearest) {
			nearest = distance;
			nearestIndex = i;
		}
	}
	if (nearest < radius) {
		*gripAngle = unk150[nearestIndex];
		return true;
	}
	return false;
}
#pragma dont_inline off

u8 TBathtub::getNextJuncture(const JGeometry::TVec3<f32>& position,
                             const JGeometry::TVec3<f32>& direction) const
{
	Mtx* matrix = getRootJointMtx();
	JGeometry::TVec3<f32> delta(position.x - (*matrix)[0][3],
	                            position.y - (*matrix)[1][3],
	                            position.z - (*matrix)[2][3]);
	JGeometry::TVec3<f32> normal;
	normal.setLength(delta, 1.0f);

	const f32 dot = normal.x * direction.x + normal.y * direction.y
	              + normal.z * direction.z;
	JGeometry::TVec3<f32> projected(
	    delta.x + direction.x - normal.x * dot,
	    delta.y + direction.y - normal.y * dot,
	    delta.z + direction.z - normal.z * dot);
	f32 localX = (*matrix)[0][0] * projected.x
	            + (*matrix)[1][0] * projected.y
	            + (*matrix)[2][0] * projected.z;
	f32 localZ = (*matrix)[0][2] * projected.x
	            + (*matrix)[1][2] * projected.y
	            + (*matrix)[2][2] * projected.z;
	f32 angle = 0.005493164f * static_cast<f32>(matan(localZ, localX));

	f32 nearest = 180.0f;
	u8 nearestIndex = 0;
	for (u8 i = 0; i < 5; ++i) {
		f32 wrapped = static_cast<f32>(fmod(360.0f + (unk13C[i] - angle + 180.0f),
		                                    360.0f));
		f32 distance = static_cast<f32>(fabs(-180.0f + wrapped));
		if (distance < nearest) {
			nearestIndex = i;
			nearest = distance;
		}
	}
	return nearestIndex;
}

u8 TBathtub::getNextGrip(const JGeometry::TVec3<f32>& position,
                         const JGeometry::TVec3<f32>& direction, f32 radius,
                         f32* gripAngle) const
{
	Mtx* matrix = getRootJointMtx();
	JGeometry::TVec3<f32> delta(position.x - (*matrix)[0][3],
	                            position.y - (*matrix)[1][3],
	                            position.z - (*matrix)[2][3]);
	JGeometry::TVec3<f32> normal;
	normal.setLength(delta, 1.0f);
	const f32 dot = normal.x * direction.x + normal.y * direction.y
	              + normal.z * direction.z;
	JGeometry::TVec3<f32> projected(
	    delta.x + direction.x - normal.x * dot,
	    delta.y + direction.y - normal.y * dot,
	    delta.z + direction.z - normal.z * dot);
	f32 localX = (*matrix)[0][0] * projected.x
	            + (*matrix)[1][0] * projected.y
	            + (*matrix)[2][0] * projected.z;
	f32 localZ = (*matrix)[0][2] * projected.x
	            + (*matrix)[1][2] * projected.y
	            + (*matrix)[2][2] * projected.z;
	f32 angle = 0.005493164f * static_cast<f32>(matan(localZ, localX));

	f32 nearest = 180.0f;
	u8 nearestIndex = 0;
	for (u8 i = 0; i < 5; ++i) {
		f32 wrapped = static_cast<f32>(fmod(360.0f + (unk150[i] - angle + 180.0f),
		                                    360.0f));
		f32 distance = static_cast<f32>(fabs(-180.0f + wrapped));
		if (distance < nearest) {
			nearestIndex = i;
			nearest = distance;
		}
	}
	if (nearest >= radius)
		return 0;
	*gripAngle = unk150[nearestIndex];
	return 1;
}

// Unused
void TBathtub::showMessage(u32) { }

void TBathtub::updatePosture_() { }

void TBathtub::load(JSUMemoryInputStream&) { }

TBathtub::TBathtub(const char* name)
    : TMapObjBase(name)
{
	unk164 = nullptr;
	unk290 = 0;
	unk16C = reinterpret_cast<TBathtubParams*>(
	    __ct__14TBathtubParamsFv(new u8[0x210]));
	unk1D8 = 0.0f;
	unk1DC = 0.0f;
	unk1E0 = 0.0f;
	unk1E4 = 1.0f;
	mPosition.x = 0.0f;
	mPosition.y = 0.0f;
	mPosition.z = 0.0f;
	unk1E8 = 0.0f;
	unk1EC = 0.0f;
	unk1F0 = 0.0f;
	unk250 = 0;
	unk254 = 1;
	unk258 = 0;
	unk25C = 1;
	unk248 = 0;
	unk298 = 0;
	unk244 = 0.0f;
	unk240 = 0.0f;
	unk23C = 0.0f;
	unk299 = 0;
	unk29A = 0;
	unk2A0 = 0;
	unk294 = 0;
}

// Unused
bool TBathtub::isKillerLaunchable() const { return false; }

u8 TBathtub::getNumKillerLaunchable() const
{
	if (unk29A != 0)
		return 0;

	if (!bathtubKoopaAllowsLaunch())
		return 0;

	if (unk248 == 0)
		return 0;

	int count = getNumGripsDead() + 1;
	if (count < 2)
		count = 2;
	if (count > 4)
		count = 4;
	return static_cast<u8>(count);
}

bool TBathtub::isKillerAttackable() const { return unk248 <= 0; }

// Unused
bool TBathtub::isBreaking() const { return false; }

u8 TBathtub::getNumKillerBurstable() const
{
	if (unk29A != 0)
		return 0;

	if (!bathtubKoopaAllowsLaunch())
		return 0;

	if (unk248 == 0)
		return 0;

	int count = getNumGripsDead();
	if (count >= 4)
		return 8;
	if (!allowsTumble() && unk250 == 0 && unk258 == 0) {
		switch (count) {
		case 1:
			return 4;
		case 2:
			return 6;
		case 3:
		case 4:
			return 8;
		default:
			return 0;
		}
	}
	return 0;
}

// Map position 0 -- with -inline deferred the first symbol of the object is
// the last definition in the file.
TBathtub::~TBathtub() { }
