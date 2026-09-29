
#include <Enemy/Koopa.hpp>
#include <Player/MarioAccess.hpp>
#include <Camera/CameraShake.hpp>
#include <MoveBG/MapObjCorona.hpp>
#include <Strategic/Strategy.hpp>
#include <MSound/MSound.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MAnmSound.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <Strategic/Spine.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

static const char* koopa_bastable[] = {
	"/scene/koopa/bas/koopa_down.bas",
	nullptr,
	"/scene/koopa/bas/koopa_fall.bas",
	"/scene/koopa/bas/koopa_fire_end.bas",
	"/scene/koopa/bas/koopa_fire_loop.bas",
	"/scene/koopa/bas/koopa_fire_start.bas",
	"/scene/koopa/bas/koopa_first.bas",
	"/scene/koopa/bas/koopa_getup.bas",
	"/scene/koopa/bas/koopa_hipdrop.bas",
	"/scene/koopa/bas/koopa_stagger.bas",
	"/scene/koopa/bas/koopa_turn_l.bas",
	"/scene/koopa/bas/koopa_turn_r.bas",
	"/scene/koopa/bas/koopa_wait.bas",
	nullptr,
	"/scene/koopa/bas/koopa_waterhit.bas",
};

// NOTE: this TU is -inline deferred, so the out-of-line functions below are
// defined in the *reverse* order of the map's .text layout.

// ============= nerves =============

// TODO: nonmatching stubs. Decoded from the target: wrap `unk150 - rotation.y`
// into [-180, 180) via std::fmodf, clamp it to +-turnSpeed, play bck 10/11
// (scaled by turnAnim), add it to rotation.y (wrapped through
// JGeometry::TUtil<f32>::mod, which JGUtil.hpp cannot express) and return TRUE
// once the target angle is reached. The target calls std::fmodf and
// TKoopa::changeAnm out of line here.
BOOL TNerveKoopaTurnL::execute(TSpineBase<TLiveActor>* spine) const
{
	return FALSE;
}

BOOL TNerveKoopaTurnR::execute(TSpineBase<TLiveActor>* spine) const
{
	return FALSE;
}

// TODO: not yet reconstructed
BOOL TNerveKoopaWait::execute(TSpineBase<TLiveActor>* spine) const { return FALSE; }

BOOL TNerveKoopaTumble::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	self->changeAnm(8, 0, self->getParams()->tumbleSpeed.get());
	self->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
	if (spine->getTime() == 190) {
		gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x27), 1.0f);
		TLiveActor* bathtub
		    = (TLiveActor*)JDrama::TNameRefGen::search("バスタブ");
		gpMarioParticleManager->emitAndBindToMtx(
		    0xf5, *bathtub->getRootJointMtx(), 0, this);
		if (SMS_IsMarioTouchGround4cm())
			SMSRumbleMgr->start(1, static_cast<f32*>(nullptr));
	}
	if (self->getMActor()->curAnmEndsNext())
		return TRUE;
	return FALSE;
}

BOOL TNerveKoopaFall::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	f32 rate     = self->getParams()->fallSpeed.get();
	self->changeAnm(2, 0, rate);
	return FALSE;
}

// TODO: not yet reconstructed
BOOL TNerveKoopaFlame::execute(TSpineBase<TLiveActor>* spine) const { return FALSE; }

BOOL TNerveKoopaProvoke::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	self->changeAnm(6, 0, 2.0f);
	if (self->getMActor()->curAnmEndsNext()) {
		spine->setNext(&TNerveKoopaWait::theNerve());
		return FALSE;
	}
	return FALSE;
}

BOOL TNerveKoopaStagger::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	self->changeAnm(9, 0, self->getParams()->staggerSpeed.get());
	if (self->getMActor()->curAnmEndsNext())
		return TRUE;
	return FALSE;
}

BOOL TNerveKoopaGetShowered::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	self->changeAnm(0xe, 0, self->getParams()->waterhitSpeed.get());
	if (self->getMActor()->curAnmEndsNext())
		return TRUE;
	return FALSE;
}

BOOL TNerveKoopaGetDown::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	switch (self->getMActor()->getCurAnmIdx(ANM_TYPE_BCK)) {
	case 0:
		if (self->getMActor()->curAnmEndsNext())
			self->changeAnm(1, 0, self->getParams()->downSpeed.get());
		break;
	case 1: {
		TBathtub* bathtub
		    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
		f32 step = self->getParams()->downStep.get();
		if (!((f32)(spine->getTime() * (bathtub->getNumGripsDead() + 2)) < step)
		    && self->getMActor()->curAnmEndsNext())
			self->changeAnm(7, 0, self->getParams()->downSpeed.get());
		break;
	}
	case 7:
		if (self->getMActor()->curAnmEndsNext())
			return TRUE;
		break;
	default:
		self->changeAnm(0, 0, self->getParams()->downSpeed.get());
		TLiveActor* bathtub
		    = (TLiveActor*)JDrama::TNameRefGen::search("バスタブ");
		gpMarioParticleManager->emitAndBindToMtx(
		    0xf5, *bathtub->getRootJointMtx(), 0, this);
		break;
	}
	return FALSE;
}

// ============= TKoopaParts =============

TKoopaParts::TKoopaParts(const char* name, u32 actorType, TKoopa* owner,
                         f32 radius)
    : THitActor(name)
    , mOwner(owner)
{
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(this);
	initHitActor(actorType, 5, 0x88000000, radius, radius, radius, radius);
	onHitFlag(HIT_FLAG_CANNOT_ATTACK);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_UNK10000000);
	onHitFlag(HIT_FLAG_UNK8000000);
}

void TKoopaParts::perform(u32 cue, JDrama::TGraphics* graphics)
{
	THitActor::perform(cue, graphics);
	if (cue & 1) {
		control();
		for (int i = 0; i < mColCount; ++i)
			attack_(mCollisions[i]);
	}
}

void TKoopaFlame::control()
{
	if (!(unk8C < unk88)) {
		onHitFlag(HIT_FLAG_CANNOT_ATTACK);
		onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
		onHitFlag(HIT_FLAG_NO_COLLISION);
		return;
	}
	unk8C = unk8C + unk84;
	f32 x      = unk6C.x + unk78.x * unk8C;
	f32 y      = unk6C.y + unk78.y * unk8C;
	f32 z      = unk6C.z + unk78.z * unk8C;
	f32 height = unk94;
	f32 radius = unk90;
	if (height <= 0.0f)
		height = 2.0f * radius;
	mPosition.x = x;
	mPosition.y = y;
	mPosition.z = z;
	offHitFlag(HIT_FLAG_CANNOT_ATTACK);
	offHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	mAttackRadius = radius;
	mAttackHeight = height;
	mDamageRadius = radius;
	mDamageHeight = height;
	calcEntryRadius();
}

BOOL TKoopaFlame::receiveMessage(THitActor*, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER:
		return FALSE;
	default:
		return TRUE;
	}
}

void TKoopaFlame::attack_(THitActor* sender)
{
	if (sender->receiveMessage(this, HIT_MESSAGE_UNKA)
	    && sender == gpMarioAddress) {
		SMS_ThrowMario(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f),
		               mOwner->getParams()->flameJump.get());
		mOwner->unk155 = 1;
		mOwner->changeAnm(3, 0, mOwner->getParams()->fireSpeed.get());
		mOwner->unk19C = 240;
	}
}

BOOL TKoopaHand::receiveMessage(THitActor*, u32) { return TRUE; }

void TKoopaHand::attack_(THitActor* sender)
{
	sender->receiveMessage(this, HIT_MESSAGE_ATTACK);
}

BOOL TKoopaHead::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER:
		if (mOwner->getShowered()) {
			gpMarioParticleManager->emit(0xe7, &sender->mPosition, 0, nullptr);
			gpMSound->startSoundSet(0x6802, &mOwner->mPosition, 0, 0.0f, 0, 0,
			                        4);
		}
		break;
	case HIT_MESSAGE_ATTACK:
		if (sender->getActorType() == 0x8000024) {
			mOwner->stagger(false);
		}
		break;
	}
	return TRUE;
}

void TKoopaHead::attack_(THitActor* sender)
{
	if (sender->receiveMessage(this, HIT_MESSAGE_ATTACK)
	    && sender == SMS_GetMarioHitActor()) {
		SMS_ThrowMario(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f), 60.0f);
	}
}

BOOL TKoopaBody::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER:
		break;
	case HIT_MESSAGE_ATTACK:
		if (sender->getActorType() == 0x8000024) {
			mOwner->stagger(false);
		}
		break;
	}
	return TRUE;
}

void TKoopaBody::attack_(THitActor* sender)
{
	if (sender->receiveMessage(this, HIT_MESSAGE_ATTACK)
	    && sender == SMS_GetMarioHitActor()) {
		SMS_ThrowMario(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f), 60.0f);
	}
}

f32 TKoopa::getFlameDirDegree() const
{
	f32 rate = getFlameDirRate() * getParams()->flameNeckRange.get();
	return mRotation.y + (unk154 ? -rate : rate);
}

namespace {
// TODO: not yet reconstructed
int KoopaNeckCallBack(J3DNode* node, int param) { return TRUE; }
}

void TKoopa::setUpHitActors()
{
	if (isFlameStart()) {
		int slot      = -1;
		bool tooClose = false;
		for (int i = 0; i < 10; ++i) {
			TKoopaFlame* flame = unk164[i];
			if (!(flame->unk8C < flame->unk88))
				slot = i;
			else if (flame->unk8C < 2.0f * getParams()->flameRadius.get())
				tooClose = true;
		}
		if (!tooClose && slot >= 0) {
			MtxPtr mtx = mMActor->getModel()->getAnmMtx(unk1A0);
			JGeometry::TVec3<f32> dir(mtx[0][0], 0.0f, mtx[2][0]);
			JGeometry::TVec3<f32> pos;
			pos.x = mtx[0][3];
			pos.y = mtx[1][3] - 500.0f;
			pos.z = mtx[2][3];
			dir.normalize();
			unk164[slot]->launch(pos, dir, getParams()->flameHeight.get(),
			                     getParams()->flameRadius.get(),
			                     getParams()->flameVelocity.get());
		}
	} else {
		for (int i = 0; i < 10; ++i) {
			unk164[i]->unk88 = 0.0f;
			unk164[i]->unk8C = 1.0f;
		}
	}

	MtxPtr mtx = mMActor->getModel()->getAnmMtx(unk1A8);
	f32 radius = getParams()->headRadius.get();
	f32 height = 2.0f * radius;
	unk194->mPosition.x = mtx[0][3];
	unk194->mPosition.y = mtx[1][3] - 200.0f;
	unk194->mPosition.z = mtx[2][3];
	unk194->offHitFlag(HIT_FLAG_CANNOT_ATTACK);
	unk194->offHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	unk194->offHitFlag(HIT_FLAG_NO_COLLISION);
	unk194->mAttackRadius = radius;
	unk194->mAttackHeight = height;
	unk194->mDamageRadius = radius;
	unk194->mDamageHeight = height;
	unk194->calcEntryRadius();

	unk198->mPosition.x = mPosition.x;
	unk198->mPosition.y = mPosition.y;
	unk198->mPosition.z = mPosition.z;
	unk198->offHitFlag(HIT_FLAG_CANNOT_ATTACK);
	unk198->offHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	unk198->offHitFlag(HIT_FLAG_NO_COLLISION);
	unk198->mAttackRadius = 800.0f;
	unk198->mAttackHeight = 2000.0f;
	unk198->mDamageRadius = 800.0f;
	unk198->mDamageHeight = 2000.0f;
	unk198->calcEntryRadius();
}

void TKoopa::changeAnm(int bck, int btp, f32 rate)
{
	changeBck(bck);
	changeBtp(btp);
	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
	ctrl->setRate(0.5f * (rate * SMSGetAnmFrameRate()));
}

f32 TKoopa::getFlameDirRate() const
{
	f32 frame  = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	f32 end    = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getEnd();
	f32 range  = getParams()->flameOverStart.get();
	int start  = getParams()->flameFocusStartStep.get();
	int finish = getParams()->flameFocusEndStep.get();
	int time   = mSpine->getTime();
	int idx    = mMActor->getCurAnmIdx(ANM_TYPE_BCK);
	switch (idx) {
	case 5:
		return -(frame * range / end);
	case 3:
	case 4: {
		f32 t;
		if (mSpine->getTime() <= start)
			t = -range;
		else if (time > finish)
			t = 1.0f;
		else
			t = (1.0f + range) * (f32)(time - start) / (f32)(finish - start)
			    - range;
		if (idx == 3)
			t *= 1.0f - frame / end;
		return t;
	}
	default:
		return 0.0f;
	}
}

BOOL TKoopa::isFlaming() const
{
	switch (mMActor->getCurAnmIdx(0)) {
	case 3:
	case 4:
	case 5:
		return TRUE;
	default:
		return FALSE;
	}
}

f32 TKoopa::getNeckFocus() const
{
	int idx            = mMActor->getCurAnmIdx(ANM_TYPE_BCK);
	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
	f32 end            = ctrl->getEnd();
	f32 frame          = ctrl->getFrame();
	f32 focus          = 1.0f;
	switch (idx) {
	case 6:
		focus = 0.0f;
		if (frame >= 164.0f)
			focus = (frame - 164.0f) / (end - 164.0f);
		break;
	case 2:
		focus = 0.0f;
		break;
	case 0:
		if (frame <= 40.0f)
			focus = 1.0f - frame / 40.0f;
		else
			focus = 0.0f;
		break;
	case 1:
		focus = 0.0f;
		break;
	case 7:
		if (frame <= 125.0f)
			focus = 0.0f;
		else
			focus = (frame - 125.0f) / (end - 125.0f);
		break;
	case 9:
		if (frame <= 30.0f)
			focus = 1.0f - frame / 30.0f;
		else if (frame <= 65.0f)
			focus = 0.0f;
		else
			focus = (frame - 65.0f) / (end - 65.0f);
		break;
	case 8:
		if (frame <= 30.0f)
			focus = 1.0f - frame / 30.0f;
		else if (frame <= 170.0f)
			focus = 0.0f;
		else
			focus = (frame - 170.0f) / (end - 170.0f);
		break;
	case 14:
		if (frame <= 20.0f)
			focus = 1.0f - frame / 20.0f;
		else if (frame <= 40.0f)
			focus = 0.0f;
		else
			focus = (frame - 40.0f) / (end - 40.0f);
		break;
	case 4:
		focus = 0.0f;
		break;
	case 5:
		if (frame <= 103.0f)
			focus = 1.0f - frame / 103.0f;
		else
			focus = 0.0f;
		break;
	case 3:
		focus = frame / end;
		break;
	case 12:
		if (frame <= 200.0f) {
		} else if (frame <= 255.0f)
			focus = 1.0f - (frame - 200.0f) / 55.0f;
		else if (frame <= 330.0f)
			focus = 0.0f;
		else if (frame <= 390.0f)
			focus = (frame - 330.0f) / 60.0f;
		else if (frame <= 440.0f) {
		} else if (frame <= 480.0f)
			focus = 1.0f - (frame - 440.0f) / 40.0f;
		else if (frame <= 555.0f)
			focus = 0.0f;
		else if (frame <= 615.0f)
			focus = (frame - 555.0f) / 60.0f;
		break;
	}
	return focus;
}

BOOL TKoopa::allowsLaunch() const
{
	return &TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve() ? FALSE : TRUE;
}

void TKoopa::getDown()
{
	if (&TNerveKoopaFall::theNerve() == mSpine->getCurrentNerve())
		return;
	if (&TNerveKoopaProvoke::theNerve() == mSpine->getCurrentNerve())
		return;
	if (&TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve())
		return;
	if (&TNerveKoopaStagger::theNerve() == mSpine->getCurrentNerve())
		mSpine->setNext(&TNerveKoopaGetDown::theNerve());
	if (&TNerveKoopaGetShowered::theNerve() == mSpine->getCurrentNerve())
		mSpine->setNext(&TNerveKoopaGetDown::theNerve());
	mSpine->pushNerve(&TNerveKoopaGetDown::theNerve());
}

BOOL TKoopa::effectsTumble() const
{
	if (&TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve()) {
		s32 time = mSpine->getTime();
		if (time < 900 && time > 190)
			return TRUE;
	}
	return FALSE;
}

bool TKoopa::getShowered()
{
	if (&TNerveKoopaFall::theNerve() == mSpine->getCurrentNerve())
		return false;
	if (&TNerveKoopaProvoke::theNerve() == mSpine->getCurrentNerve())
		return false;
	if (&TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve())
		return false;
	if (&TNerveKoopaGetDown::theNerve() == mSpine->getCurrentNerve())
		return false;
	if (&TNerveKoopaGetShowered::theNerve() == mSpine->getCurrentNerve())
		return true;
	if (&TNerveKoopaStagger::theNerve() == mSpine->getCurrentNerve()) {
		mSpine->setNext(&TNerveKoopaGetShowered::theNerve());
		return true;
	}
	if (&TNerveKoopaFlame::theNerve() == mSpine->getCurrentNerve()) {
		mSpine->setNext(&TNerveKoopaWait::theNerve());
		return false;
	}
	mSpine->pushNerve(&TNerveKoopaGetShowered::theNerve());
	return true;
}

void TKoopa::stagger(bool ignoreFlame)
{
	if (&TNerveKoopaFall::theNerve() != mSpine->getCurrentNerve()
	    && &TNerveKoopaProvoke::theNerve() != mSpine->getCurrentNerve()
	    && (ignoreFlame
	        || &TNerveKoopaFlame::theNerve() != mSpine->getCurrentNerve())
	    && &TNerveKoopaTumble::theNerve() != mSpine->getCurrentNerve()
	    && &TNerveKoopaGetDown::theNerve() != mSpine->getCurrentNerve()
	    && &TNerveKoopaGetShowered::theNerve() != mSpine->getCurrentNerve())
		mSpine->pushNerve(&TNerveKoopaStagger::theNerve());
}

f32 TKoopa::getTargetDir(const JGeometry::TVec3<f32>& pos) const
{
	TLiveActor* bathtub = (TLiveActor*)JDrama::TNameRefGen::search("バスタブ");
	Mtx* m              = bathtub->getRootJointMtx();
	JGeometry::TVec3<f32> origin((*m)[0][3], (*m)[1][3], (*m)[2][3]);
	JGeometry::TVec3<f32> d;
	JGeometry::TVec3<f32> axisX((*m)[0][0], (*m)[1][0], (*m)[2][0]);
	JGeometry::TVec3<f32> axisZ((*m)[0][2], (*m)[1][2], (*m)[2][2]);
	JGeometry::TVec3<f32> axisY((*m)[0][1], (*m)[1][1], (*m)[2][1]);
	d.sub(pos, origin);
	f32 z = axisZ.dot(d);
	f32 x = axisX.dot(d);
	return 0.005493164f * matan(z, x);
}

void TKoopa::fall()
{
	mRotation.y = 180.0f;
	mSpine->setNext(&TNerveKoopaFall::theNerve());
}

void TKoopa::updateAnmSound()
{
	if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) == 8) {
		unk158.x = mPosition.x;
		unk158.y = mPosition.y;
		unk158.z = mPosition.z;
	} else {
		MtxPtr mtx = mMActor->getModel()->getAnmMtx(unk1A0);
		JGeometry::TVec3<f32> pos;
		pos.z = mtx[2][3];
		pos.y = mtx[1][3];
		pos.x = mtx[0][3];
		unk158.set(pos);
	}
	if (mAnmSound && mAnmSoundPath) {
		J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
		mAnmSound->animeLoop(&unk158, ctrl->getFrame(), ctrl->getRate(), 0, 4);
	}
}

// ============= TKoopa =============

TKoopa::TKoopa(const char* name)
    : TSpineEnemy(name)
{
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	offLiveFlag(LIVE_FLAG_UNK100);
	onLiveFlag(LIVE_FLAG_UNK10);
}

void TKoopa::load(JSUMemoryInputStream& stream) { TSpineEnemy::load(stream); }

void TKoopa::loadAfter()
{
	JDrama::TNameRef::loadAfter();
	for (int i = 0; i < 10; ++i)
		unk164[i] = new TKoopaFlame("クッパの吐く炎", 0x8000029, this, 100.0f);
	for (int i = 0; i < 2; ++i)
		unk18C[i] = new TKoopaHand("クッパの手", 0x800002b, this, 100.0f);
	unk194 = new TKoopaHead("クッパの頭", 0x800002a, this, 100.0f);
	unk198 = new TKoopaBody("クッパの体", 0x800002a, this, 100.0f);
}

void TKoopa::init(TLiveManager* manager)
{
	mBodyRadius = 800.0f;
	mHeadHeight = 2000.0f;
	TSpineEnemy::init(manager);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	offHitFlag(HIT_FLAG_CANNOT_ATTACK);
	mSpine->initWith(&TNerveKoopaProvoke::theNerve());
	changeAnm(12, 1, 2.0f);
	changeAnm(6, 0, 2.0f);
	mMActor->initSimpleMotionBlend(16);
	unk150 = getTargetDir(*gpMarioPos);
	initAnmSound();
	reset();
	JUTNameTab* names = getModel()->getModelData()->getJointName();
	unk1A8            = names->getIndex("ago");
	unk1A0            = names->getIndex("head");
	unk1A4            = names->getIndex("neck");
	J3DJoint* head    = getModel()->getModelData()->getJointNodePointer(unk1A0);
	head->setCallBack(KoopaNeckCallBack);
	head->setCallBackUserData(this);
	unk1B8 = 1.0f;
	unk155 = 0;
}

const char** TKoopa::getBasNameTable() const { return koopa_bastable; }

void TKoopa::reset()
{
	TSpineEnemy::reset();
	changeAnm(0xc, 1, getParams()->waitSpeed.get());
	unk19C = 600;
}

void TKoopa::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & 1) {
		unk1B8 = getNeckFocus();
		if (unk19C > 0)
			unk19C = unk19C - 1;
	}
	TSpineEnemy::perform(cue, graphics);
	for (int i = 0; i < 10; ++i)
		unk164[i]->perform(cue, graphics);
	unk194->perform(cue, graphics);
	unk18C[0]->perform(cue, graphics);
	unk18C[1]->perform(cue, graphics);
	unk198->perform(cue, graphics);
	if (cue & 1) {
		f32 frame = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
		bool afterStart = false;
		bool beforeEnd  = false;
		if (&TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve()
		    && frame >= getParams()->tumbleStartFrame.get())
			afterStart = true;
		if (afterStart && frame <= getParams()->tumbleEndFrame.get())
			beforeEnd = true;
		if (beforeEnd) {
			TBathtub* bathtub
			    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
			bathtub->tumble(mRotation.y, getParams()->tumbleWeight.get());
		}
		setUpHitActors();
	}
	if (cue & 2) {
		if (!isFlameStart() && !isFlameEnd())
			return;
		mMActor->calc();
		f32 scale = getParams()->flameScale.get();
		JGeometry::TVec3<f32> scaleVec;
		scaleVec.set(scale, scale, scale);
		JPABaseEmitter* emitter;
		emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    KOOPA_JPA_MS_KP_FIRE_E, mMActor->getModel()->getAnmMtx(unk1A0), 3,
		    this);
		if (emitter)
			emitter->setGlobalScale(scaleVec);
		emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    KOOPA_JPA_MS_KP_FIRE_D, mMActor->getModel()->getAnmMtx(unk1A0), 1,
		    this);
		if (emitter)
			emitter->setGlobalScale(scaleVec);
		emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    KOOPA_JPA_MS_KP_FIRE_C, mMActor->getModel()->getAnmMtx(unk1A0), 1,
		    this);
		if (emitter)
			emitter->setGlobalScale(scaleVec);
		emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    KOOPA_JPA_MS_KP_FIRE_B, mMActor->getModel()->getAnmMtx(unk1A0), 1,
		    this);
		if (emitter)
			emitter->setGlobalScale(scaleVec);
		emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    KOOPA_JPA_MS_KP_FIRE_A, mMActor->getModel()->getAnmMtx(unk1A0), 1,
		    this);
		if (emitter)
			emitter->setGlobalScale(scaleVec);
	}
}

BOOL TKoopa::receiveMessage(THitActor* sender, u32 message)
{
	return TSpineEnemy::receiveMessage(sender, message);
}

TKoopaManager::TKoopaManager(const char* name)
    : TEnemyManager(name)
{
}

TSpineEnemy* TKoopaManager::createEnemyInstance() { return nullptr; }

void TKoopaManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "koopa_model.bmd", 0x14240000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TKoopaManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
	unk38 = new TKoopaParams("/enemy/koopa.prm");
}

void TKoopaManager::loadAfter()
{
	TEnemyManager::loadAfter();
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_a.jpa", 0x1c0);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_b.jpa", 0x1c1);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_c.jpa", 0x1c2);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_d.jpa", 0x1c3);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_hipdrop.jpa", 0xf5);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_e.jpa", 0x1f3);
}
