#include <Enemy/EnemyAttachment.hpp>
#include <Enemy/Conductor.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/SharedParts.hpp>
#include <System/MarDirector.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <M3DUtil/MActor.hpp>
#include <Player/MarioAccess.hpp>
#include <Camera/Camera.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/PollutionManager.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

TEnemyAttachment::TEnemyAttachment(const char* name)
    : TSpineEnemy(name)
    , unk154(0)
    , unk158(0)
    , unk160(nullptr)
    , unk164(1.0f)
    , unk168(0)
{
}

void TEnemyAttachment::loadInit(TSpineEnemy* param_1, const char* param_2)
{
	unk160        = param_1;
	mMActorKeeper = new TMActorKeeper(unk160->getManager(), 1);
	mMActor       = mMActorKeeper->createMActor(param_2, 3);
}

void TEnemyAttachment::behaveToHitGround()
{
	unk168 = 1;
	offLiveFlag(LIVE_FLAG_AIRBORNE);
	mVelocity.x = 0.0f;
	mVelocity.y = -0.3f;
	mVelocity.z = 0.0f;
	rebirth();
}

void TEnemyAttachment::behaveToHitWall(const TBGCheckData*) { rebirth(); }

void TEnemyAttachment::forceKill()
{
	if (!mGroundPlane->isPool() && gpMap->isInArea(mPosition.x, mPosition.z))
		return;

	kill();

	if (unk160->checkLiveFlag(LIVE_FLAG_HIDDEN)) {
		unk160->kill();
		unk160->onLiveFlag(LIVE_FLAG_UNK20000);
	}
}

void TEnemyAttachment::recoverScale()
{
	if (mScaling.x < unk164) {
		VECScale(&mScaling, &mScaling, 2.0f);
	} else {
		mScaling.x = mScaling.y = mScaling.z = unk164;
	}
}

void TEnemyAttachment::bind()
{
	JGeometry::TVec3<f32> local_1C = mPosition;
	local_1C += mLinearVelocity;
	local_1C += mVelocity;
	recoverScale();
	mVelocity.y -= getNowGravity();
	if (mVelocity.y < mVelocityMinY)
		mVelocity.y = mVelocityMinY;
	if (!unk168) {
		mGroundHeight = gpMap->checkGround(local_1C.x, local_1C.y + mHeadHeight,
		                                   local_1C.z, &mGroundPlane);
		mGroundHeight += 1.0f;
	}

	f32 nextY = local_1C.y;
	if (nextY + mVelocity.y <= mGroundHeight)
		behaveToHitGround();
	else
		onLiveFlag(LIVE_FLAG_AIRBORNE);

	TBGWallCheckRecord local_48(local_1C.x, nextY + mHeadHeight, local_1C.z,
	                            2.0f * mBodyRadius, 1, 0);
	if (gpMap->isTouchedWallsAndMoveXZ(&local_48)) {
		const TBGCheckData* wall = local_48.mResultWalls[0];
		behaveToHitWall(wall);
	}

	mPosition = local_1C;
	// Retail really does subtract the position it has just written, so the
	// linear velocity always ends up zero here. The by-value left operand of
	// operator- is what keeps TVec3::sub a `bl`.
	// TODO: the operator- temporary lands at 0x24 where retail puts it at 0x10
	// (frame and every instruction otherwise exact); ours allocates ~20 bytes
	// of low region ahead of it.
	// c-m28 (iro.py): our copy is a parse-time temp created before the wall
	// record's three ctor bindings and line 103's three P temps; retail creates
	// it after them. One extra inline level over the statement moves it 0x24 ->
	// 0x1c only; set()/functional-cast/named spellings are worse or inert.
	mLinearVelocity = local_1C - mPosition;

	setBehavior();
	forceKill();

	mPosition += mLinearVelocity;
}

void TEnemyAttachment::rebirth()
{
	unk150 = 0;
	unk158 = 0;
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mVelocity.y = 0.0f;
}

void TEnemyAttachment::kill()
{
	unk150 = 0;
	unk158 = 0;
	onHitFlag(HIT_FLAG_NO_COLLISION);
}

void TEnemyAttachment::set()
{
	mPosition = unk160->getPosition();
	mRotation = unk160->getRotation();
}

void TEnemyAttachment::moveObject()
{
	if (unk150 == 1) {
		set();
		setBehavior();
	} else {
		calcRideMomentum();
		sendMessage();
		bind();
		calcRidePos();
	}
}

void TEnemyAttachment::sendMessage()
{
	for (int i = 0; i < mColCount; ++i) {
		if (mCollisions[i]->isActorType(0x80000001)) {
			SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
			continue;
		}

		if (mCollisions[i] != unk160) {
			kill();
		}
	}
}

void TEnemyAttachment::calcRootMatrix()
{
	MsMtxSetXYZRPH(getMActor()->getModel()->getBaseTRMtx(), mPosition.x,
	               mPosition.y, mPosition.z, mRotation.x, mRotation.y,
	               mRotation.z);
	getMActor()->getModel()->setBaseScale(mScaling);
}

// Binding level worth +8 of low region, landing TEnemyAttachment::perform's
// frame at 0x30 (batch 121).
static inline bool EnemyAttachmentIsDemoModeNow(const TMarDirector* p)
{
	bool demoModeNow = p->isDemoModeNow();
	return demoModeNow;
}

void TEnemyAttachment::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (unk150 == 0) {
		if (cue & CUE_CALC_ANIM)
			behaveToHost();
		return;
	}

	if (!EnemyAttachmentIsDemoModeNow(SMSGetMarDirector())
	    && SMSGetMarDirector()->isTalkModeNow()) {
		performOnlyDraw(cue, graphics);
		return;
	}

	if (cue & CUE_MOVE)
		moveObject();

	if (cue & CUE_CALC_ANIM) {
		calcRootMatrix();
		getMActor()->calcAnm();
	}

	if (cue & CUE_CALC_VIEW)
		getMActor()->viewCalc();

	if (cue & CUE_ENTRY)
		getMActor()->entry();
}

void TEnemyPolluteModelManager::init(TLiveActor* param_1)
{
	unk18 = new TEnemyPolluteModel*[unk14];
}

// The far-clip fetch reads `unk84` directly and goes through the params
// class's own wrapper: the frame is a three-rung ladder and only this
// combination lands retail's 0x60 with the argument loads in order.
// Measured, each against the same baseline: `getCondParams()` over raw
// `unk84` +0x10, `mEnemyFarClip.get()` over `getEnemyFarClip()` -8, a named
// `farClip` +8, and `gpCamera->getFovy()`/`getAspect()` +8 each. Every other
// pair that lands 0x60 misplaces exactly one of the four argument loads, and
// the 100.0f has to be a named local (a literal argument inside the loop is
// reloaded per iteration where retail does `fmr f1, f31`).
void TEnemyPolluteModelManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_CALC_ANIM) {
		f32 f31     = 100.0f;
		f32 farClip = gpConductor->unk84.getEnemyFarClip();
		SetViewFrustumClipCheckPerspective(gpCamera->getFovy(),
		                                   gpCamera->getAspect(),
		                                   graphics->getNearPlane(), farClip);

		for (int i = 0; i < unk14; ++i) {
			if (unk18[i]->unk5D) {
				if (ViewFrustumClipCheck(graphics, &unk18[i]->unk44, f31))
					unk18[i]->unk5C = false;
				else
					unk18[i]->unk5C = true;
			}
		}
	}

	for (int i = 0; i < unk14; ++i)
		unk18[i]->perform(cue, graphics);
}

// The ground check lives in `generate` (c-k5): an inlined callee's locals are
// created last-declared first, which is the only way retail's `check` (0x3c)
// sits directly below `generate`'s matrix (0x40), and with the check there
// the out-of-line `generate` is exactly the map's UNUSED 0x178.
// c-k13: the shared parts' actor is read through `getMActor()`; that
// accessor's receiver binding and the header `isIllegalData()` give the three
// dead words retail has below `check` (the fabricated checkFlag binder made
// up only two of them).
void TEnemyPolluteModelManager::generatePolluteModel(
    JGeometry::TVec3<f32>& param_1, JGeometry::TVec3<f32>& param_2)
{
	unk18[unk10]->generate(param_1, param_2);

	++unk10;
	if (unk10 >= unk14)
		unk10 = 0;
}

TEnemyPolluteModel::TEnemyPolluteModel(TLiveActor* param_1, int param_2,
                                       SDLModelData* param_3, const char* name)
    : JDrama::TViewObj(name)
    , unk5C(1)
    , unk5D(0)
{
	unk10 = new TSharedParts(param_1, param_2, param_3, 3);
}

// Each site reads the actor through TSharedParts::getMActor(), and the anim
// test is the no-argument curAnmEndsNext() wrapper (c-k13); together they
// give the dead frame the old per-site binders made up (0x80).
void TEnemyPolluteModel::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (!unk5D || unk5C)
		return;

	if (cue & CUE_CALC_ANIM) {
		if (unk10->getMActor()->curAnmEndsNext()) {
			unk5D = false;
			return;
		}

		unk10->getMActor()->getModel()->setBaseTRMtx(unk14);
		unk10->getMActor()->getModel()->setBaseScale(unk50);
		unk10->getMActor()->calcAnm();
	}

	if (cue & CUE_ENTRY)
		gpPollution->stampModel(unk10->getMActor()->getModel());
}

void TEnemyPolluteModel::generate(JGeometry::TVec3<f32>& param_1,
                                  JGeometry::TVec3<f32>& param_2)
{
	const TBGCheckData* check;
	gpMap->checkGround(param_1, &check);
	if (check->isIllegalData() || check->isWaterSurface())
		return;

	unk44 = param_1;
	unk50 = param_2;

	TPosition3f TStack_58;
	TStack_58.translation(param_1.x, param_1.y, param_1.z);
	unk14.translation(param_1.x, param_1.y, param_1.z);
	unk10->getMActor()->getModel()->setBaseTRMtx(TStack_58);
	unk5D = true;
	unk5C = false;
	setAnm();
}
