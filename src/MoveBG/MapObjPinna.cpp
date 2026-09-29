#include <MoveBG/MapObjPinna.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <Player/MarioAccess.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <Map/Map.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <Player/Yoshi.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <MarioUtil/MathUtil.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

void TMerrygoround::draw() const {}

void TChangeStageMerrygoround::calc()
{
	if (unk13C) {
		gpMarioParticleManager->emitAndBindToPosPtr(0x100,
		                                             &SMS_GetMarioPos(), 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(0x101,
		                                             &SMS_GetMarioPos(), 1, this);
	}
}

TPinnaCoaster::TPinnaCoaster(const char* name)
    : TMapObjBase(name)
    , unk138(0)
{
	unk148 = unk144 = unk140 = 0.0f;
}

void TAmiKing::touchPlayer(THitActor* actor)
{
	SMS_SendMessageToMario(this, HIT_MESSAGE_UNK9);
}

void TAmiKing::bind()
{
	if (mLiveFlag & LIVE_FLAG_UNK10)
		gpMap->checkGround(mPosition.x, mPosition.y + mHeadHeight,
		                   mPosition.z, &mGroundPlane);
	else
		TLiveActor::bind();
}

void TBalloonKoopaJr::touchActor(THitActor* actor) { kill(); }

void TAmiKing::loadAfter()
{
	TMapObjBase::loadAfter();
	SMS_LoadParticle("/scene/mapObj/amiking.jpa", 0x184);
}

void TPinnaEntrance::loadAfter()
{
	TMapObjBase::loadAfter();
	JGeometry::TVec3<f32> rotation(90.0f, 0.0f, 0.0f);
	TMapObjBaseManager::newAndRegisterObj("GateManta", mPosition, rotation);
}

void TWaterRecoverObj::touchPlayer(THitActor* player)
{
	if (!player->isActorType(0x80000001))
		return;
	if (isStateTimerEngaged())
		return;
	player->receiveMessage(this, HIT_MESSAGE_ATTACK);
	startStateTimer(0x258);
}

void TViking::loadAfter()
{
	TMapObjBase::loadAfter();
	reset();
}

void TChangeStageMerrygoround::touchPlayer(THitActor* player)
{
	// Frame-padding: target frame is 24 bytes larger (MWCC stack-padding quirk).
	char framePad_24_touchPlayer[24];
	(void)framePad_24_touchPlayer;
	if (isStateTimerEngaged())
		return;
	TYoshi* yoshi = SMS_GetYoshi();
	if (yoshi->mType == 1) {
		if (gpMSound->gateCheck(0x4840))
			MSoundSESystem::MSoundSE::startSoundSystemSE(0x4840, 0, nullptr,
			                                             0);
		TMapObjChangeStage::touchPlayer(player);
		unk13C = 1;
	} else if (gpMSound->gateCheck(0x483E)) {
		MSoundSESystem::MSoundSE::startSoundSystemSE(0x483E, 0, nullptr, 0);
	}
	startStateTimer(0x258);
}

TViking::TViking(const char* name)
    : THorizontalViking(name)
{
	unk14C = 0;
	unk150.set(0.0f, 0.0f, 0.0f);
}

TMerrygoround::TMerrygoround(const char* name)
    : TMapObjBase(name)
{
	// Frame-padding: target frame is 8 bytes larger (MWCC stack-padding quirk).
	char framePad_8_ctor[8];
	(void)framePad_8_ctor;
	int i;
	unk1A0 = 0;
	unk1A4 = 0;
	unk138 = 0;
	unk140 = 0;
	unk13C = 0;
	unk142 = 0;
	for (i = 0; i < 9; ++i) {
		unk144[i] = 0;
		unk18C[i] = 0;
		unk168[i] = 0;
	}
}

f32 TShellCup::mWaterOpenAccel = 5.0f;
f32 TShellCup::mOpenRotMax     = 90.0f;
f32 TShellCup::mCloseAccel     = 3.5f;
f32 TShellCup::mShellDamageRot = 45.0f;
f32 TMerrygoround::mRotSpeed   = 0.1f;

TFerrisWheel::TFerrisWheel(const char* name)
    : TMapObjBase(name)
{
	unk138 = 0;
	unk13C = 0;
	unk140 = 0.0f;
}

void THorizontalViking::updateTrans()
{
	// TODO: implement (0x90 in target); distinct stub to avoid linker folding.
	unk144 = mRotation.y;
}

void THorizontalViking::moveNormal()
{
	// TODO: implement (0x90 in target); distinct stub to avoid linker folding.
	unk148 = mPosition.x;
}

TPinnaShell::TPinnaShell(const char* name)
    : THitActor(name)
{
	// NOTE: kept only for the linker's UNUSED symbol record; nothing calls it.
	unk68 = 0;
	unk6C = 0.0f;
	unk70 = 0.0f;
	unk74 = 0;
	unk78 = 0;
	unk7C = 0;
	unk80 = 0;
	unk84 = 0;
	unk88 = 0;
	unk8C = 0;
	initHitActor(0x4000013A, 1, 0x80000000, 250.0f, 400.0f, 250.0f,
	             200.0f);
}

void TShellCup::calcAfter()
{
	// TODO: implement; kept for the linker's UNUSED symbol record.
	unk138[0].unk6C = 0.0f;
}

void TShellCup::attachCoin(TCoin* coin, int index)
{
	// TODO: implement; kept for the linker's UNUSED symbol record.
	unk138[0].unk68 = (u32)coin + (u32)index;
}

void TPinnaShell::opened()
{
	// TODO: implement; kept for the linker's UNUSED symbol record.
	unk6C = 0.0f;
}

void THorizontalViking::reset()
{
	unk144 = unk138.z;
	unk148 = 0.0f;
	if (unk144 > 0.0f)
		mState = 1;
	else
		mState = 2;
}

void THorizontalViking::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138.x = 2500.0f;
	unk138.y = 0.0008f;
	unk138.z = 0.23f;
	reset();
}

void THorizontalViking::control()
{
	// Frame-padding: target frame is 8 bytes larger (MWCC stack-padding quirk).
	char framePad_8_control[8];
	(void)framePad_8_control;
	TMapObjBase::control();
	switch (mState) {
	case 1:
		unk144 -= unk138.y;
		unk148 += unk144;
		if (unk148 < 0.0f)
			mState = 2;
		break;
	case 2:
		unk144 += unk138.y;
		unk148 += unk144;
		if (unk148 > 0.0f)
			mState = 1;
		break;
	}
	f32 s = sinf(3.14f * (unk148 / 180.0f));
	mPosition.x = unk138.x * s + mInitialPosition.x;
	f32 yOff = mYOffset;
	f32 c = cosf(3.14f * (unk148 / 180.0f));
	mPosition.y = unk138.x * (1.0f - c) + mInitialPosition.y + yOff;
}

void TShellCup::control()
{
	mMActor->calc();
	for (int i = 0; i < 6; ++i)
		unk138[i].control();
}

void TViking::reset()
{
	unk144 = unk138.z;
	unk148 = 0.0f;
	if (unk138.z > 0.0f)
		mState = 1;
	else
		mState = 2;
}

void TViking::initMapObj()
{
	unk14C = 1;
	if (strcmp(getName(), "viking 0") == 0) {
		unk138.x = 1400.0f;
		unk138.y = 0.001f;
		unk150.y = 1.001f;
		unk150.z = 0.999f;
		unk138.z = -0.3f;
		unk150.x = 0.3f;
	} else {
		unk138.x = 1400.0f;
		unk138.y = 0.001f;
		unk150.y = 1.001f;
		unk150.z = 0.999f;
		unk138.z = 0.3f;
		unk150.x = 0.3f;
	}
	mPosition.y -= unk138.x;
	TMapObjBase::initMapObj();
}

void TAmiKing::initMapObj()
{
	TMapObjBase::initMapObj();
	initAnmSound();
	mMActor->setBck("amiking_sleep1");
	setAnmSound("/scene/mapObj/amiking_sleep1.bas");
	offLiveFlag(LIVE_FLAG_UNK10);
	for (u8 i = 0; i < mMActor->getModel()->getModelData()->getJointNum();
	     ++i) {
	}
}

void TBalloonKoopaJr::kill()
{
	// Frame-padding: target frame is 8 bytes larger (MWCC stack-padding quirk).
	char framePad_8_kill[8];
	(void)framePad_8_kill;
	TMapObjGeneral::kill();
	emitAndScale(0x5A, 0, &unk148);
	emitAndScale(0x5B, 0, &unk148);
	emitAndScale(0x5C, 0, &unk148);
	TFlagManager::getInstance()->incFlag(0x60001, 1);
	if (gpMSound->gateCheck(0x28B8))
		MSoundSESystem::MSoundSE::startSoundActor(0x28B8, &mPosition, 0,
		                                          nullptr, 0, 4);
}

void TBalloonKoopaJr::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	SMS_LoadParticle("/scene/mapObj/balloonKoopaJr.jpa", 0x5A);
	SMS_LoadParticle("/scene/mapObj/balloonKoopaJrA.jpa", 0x5B);
	SMS_LoadParticle("/scene/mapObj/balloonKoopaJrB.jpa", 0x5C);
	s32 joint = getModel()->getModelData()->getJointName()->getIndex("center");
	MtxPtr mtx = getModel()->getAnmMtx((u16)joint);
	unk148.set(mtx[0][3], mtx[1][3], mtx[2][3]);
}

TShellCup::TShellCup(const char* name)
    : TMapObjBase(name)
{
	unk498 = nullptr;
	unk49C = nullptr;
	unk4A0 = nullptr;
}

void TShellCup::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TMapObjBase::perform(cue, graphics);
	if (!(cue & 2))
		return;
	bool flag1 = gpMarDirector->unk124 == 1 || gpMarDirector->unk124 == 2;
	if (flag1) {
		bool flag2 = gpMarDirector->unk124 == 3 || gpMarDirector->unk124 == 4;
		if (!flag2)
			return;
	}
	for (int i = 0; i < 6; ++i) {
		Mtx mtx;
		MsMtxSetRotX(mtx, unk138[i].unk6C);
		concatOnlyRotFromRight(unk138[i].unk74, mtx, unk138[i].unk74);
	}
	if (!(unk498->mLiveFlag & LIVE_FLAG_DEAD)) {
		unk498->mPosition.x = unk138[0].mPosition.x;
		unk498->mPosition.y = unk138[0].mPosition.y;
		unk498->mPosition.z = unk138[0].mPosition.z;
	}
	if (!(unk49C->mLiveFlag & LIVE_FLAG_DEAD)) {
		unk49C->mPosition.x = unk138[2].mPosition.x;
		unk49C->mPosition.y = unk138[2].mPosition.y;
		unk49C->mPosition.z = unk138[2].mPosition.z;
	}
	if (!(unk4A0->mLiveFlag & LIVE_FLAG_DEAD)) {
		unk4A0->mPosition.x = unk138[4].mPosition.x;
		unk4A0->mPosition.y = unk138[4].mPosition.y;
		unk4A0->mPosition.z = unk138[4].mPosition.z;
	}
}

u32 TFerrisWheel::becomeCalmlyCallback(u32 arg1, u32 arg2)
{
	if (arg1 == 0) {
		mState = 2;
		MSound* sound = gpMSound;
		if (sound->unk80 != nullptr) {
			sound->unk80->setVolume(0.0f, 200, 0);
			sound->unk80->setPitch(0.5f, 200, 0);
		}
		mStateTimer = 0x78;
	}
	return 0;
}

BOOL TPinnaShell::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		gpMarioParticleManager->emit(0xE7, &sender->mPosition, 0, nullptr);
		gpMSound->startSoundSet(0x6802, &mPosition, 0, 0.0f, 0, 0, 4);
		if (unk68 == 0) {
			unk6C -= TShellCup::mWaterOpenAccel;
			if (unk6C < -TShellCup::mOpenRotMax)
				unk68 = 1;
		}
		return TRUE;
	}
	return FALSE;
}
