#include <Enemy/killer.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/EffectObj.hpp>
#include <Camera/CameraShake.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/MapData.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjBlock.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DCluster.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <dolphin/mtx.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* killer_bastable[] = {
	"/scene/killer/bas/downkiller_down1.bas", nullptr, nullptr,
	"/scene/killer/bas/killer_search1.bas",   nullptr,
};

bool TKiller::mSerialBomb        = true;
bool TKiller::mTrampleDie        = true;
f32 TFlyEnemy::mTestSp           = 2.5f;
int TFlyEnemy::mInvalidTime      = 200;
f32 TFlyEnemy::mTestMarioSpMax   = 12.0f;
static TKiller* gpCurKiller;
bool TKiller::mRollSw;

TFlyEnemyParams::TFlyEnemyParams(const char* path)
    : TWalkerEnemyParams(path)
    , PARAM_INIT(mSLNormalFlyGravityY, 0.2f)
    , PARAM_INIT(mSLNormalFlySpeed, 10.0f)
    , PARAM_INIT(mSLChaseFlyGravityY, 0.1f)
    , PARAM_INIT(mSLChaseDist, 2000.0f)
    , PARAM_INIT(mSLForceGravityY, 0.1f)
{
	TParams::load(mPrmPath);
}

void TFlyEnemy::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	unk19C = (TFlyEnemyParams*)getSaveParam();
}

f32 TFlyEnemy::getGravityY() const
{
	if (mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve())
		return unk194;
	return unk19C->mSLNormalFlyGravityY.get();
}

void TFlyEnemy::reset()
{
	TWalkerEnemy::reset();
	unk1A0 = 0;
	unk198 = 1;
	unk1A4 = 0;
	unk194 = unk19C->mSLNormalFlyGravityY.get();
	unk1A5 = 0;
}

void TFlyEnemy::fly()
{
	JGeometry::TVec3<f32> pos = mPosition;
	pos.add(mLinearVelocity);

	JGeometry::TVec3<f32> vel = mVelocity;
	vel.x += *gpMarioSpeedX / mTestMarioSpMax;
	vel.z += *gpMarioSpeedZ / mTestMarioSpMax;

	pos.add(vel);
	pos.y += unk194;

	mGroundHeight = gpMap->checkGround(pos.x, pos.y + mHeadHeight, pos.z,
	                                   &mGroundPlane);
	mGroundHeight += 1.0f;

	if (pos.y <= mGroundHeight) {
		if (unk1A0 > mInvalidTime) {
			offLiveFlag(LIVE_FLAG_AIRBORNE);
			mVelocity.set(0.0f, 0.0f, 0.0f);
			pos.y = mGroundHeight;
		}

		// TODO: the 0x4000..0x400A actor types are unidentified
		if (mGroundPlane->mActor != nullptr
		    && ((u32)(mGroundPlane->mActor->getActorType() - 0x4000) <= 10
		        ? true
		        : false))
			const_cast<TLiveActor*>(mGroundPlane->mActor)->kill();

		if (mGroundPlane->isIllegalData())
			kill();
	} else {
		onLiveFlag(LIVE_FLAG_AIRBORNE);
	}

	mLinearVelocity = pos - mPosition;
}

void TFlyEnemy::calcChaseParam()
{
	JGeometry::TVec3<f32> dir;
	dir.sub(*gpMarioPos, mPosition);
	dir.x *= 1.1f;
	dir.z *= 1.1f;

	JGeometry::TVec3<f32> tgt;
	tgt.add(mPosition, dir);

	if (unk1A5) {
		TPathNode node(tgt);
		unkF4      = node;
		unk104     = node;
		unk114.clear();
	}

	if (dir.y > 100.0f || fabsf(dir.y) >= 100.0f) {
		if (unk1A5) {
			if (unk198 != 2 && mSprayedByWaterCooldown == 0)
				unk194 = unk19C->mSLChaseFlyGravityY.get();
			unk198 = 2;
		} else {
			mPosition.y -= 1.0f;
		}
	} else {
		unk194 = unk19C->mSLNormalFlyGravityY.get();

		JGeometry::TVec3<f32> vel(0.0f, 0.0f, 0.0f);
		if (unk198 != 2 || dir.y > 150.0f) {
			unk198 = 0;
			MsVECNormalize(dir, dir);
			vel.x   = dir.x * unk19C->mSLNormalFlySpeed.get();
			vel.z   = dir.z * unk19C->mSLNormalFlySpeed.get();
			unk194 = unk19C->mSLForceGravityY.get();
		} else {
			mPosition.y -= 3.0f;
		}

		// TODO: these zero out the speed that was just computed; probably
		// not what the original wrote.
		vel.zero();
		mVelocity = vel;
	}
}

void TFlyEnemy::bind()
{
	if (mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve()
	    || unk1A0 < mInvalidTime)
		fly();
	else
		TLiveActor::bind();
}

void TFlyEnemy::flyMove()
{
	// UNUSED in marioEU.MAP at 0x2BC (700 B): dead-stripped, never emitted into
	// the DOL.  There is NO ground-truth assembly for it anywhere in this
	// repo, `decomp-diff.py` can only print our own side, and objdiff scores
	// it as `extra` - which is not in the unit's `fuzzy_match_percent` sum at
	// all.  Measured A/B on one tree: a 4-byte stub and a 700-byte body both
	// give `fuzzy_match_percent = 96.376860`, with a per-function JSON diff of
	// the two reports showing zero differences across all 54 functions.
	//
	// A size-exact placeholder body WAS written here and has been reverted.
	// It silenced `validate-symbol-order.py`'s SIZE warning and nothing else,
	// while adding 175 instructions whose statement order and constants were
	// all unverified.  AGENTS.md is explicit that a TODO beats a fakematch.
	//
	// What is actually worth knowing, and the reason to reconstruct this at
	// all: the map entry has a SIZE, so the linker compiled the body and then
	// dropped the out-of-line copy because nothing referenced it as a symbol.
	// In practice that means the original declared the function and every call
	// site inlined it.  0x2BC is therefore a hard constraint on the body, and
	// the win would be the CALLER's register allocation and frame - never this
	// symbol.  If a reconstruction compiles to exactly 700 B *and* still
	// inlines at the call sites, that is almost certainly the original; if it
	// does not inline, it is the wrong body.  Do not let the out-of-line copy
	// survive.
	//
	// Likely shape, unverified: `fly()` (real, 0x244) followed by a facing +
	// body-scale tail, i.e. the pre-split ancestor of `fly()` and
	// `calcChaseParam()` before the pair of nerve `execute()`s replaced it.
}

DEFINE_NERVE(TNerveFlyEnemyNormalFly, TLiveActor)
{
	TKiller* self = (TKiller*)spine->getBody();

	if (spine->getTime() == 0)
		self->setNormalFlyAnm();

	// TODO: unk1A4/unk1A0 are unidentified counters
	if (self->unk1A4 && self->unk1A0 > 500) {
		self->updateSquareToMario();
		if (self->mDistToMarioSquared
		    < self->unk19C->mSLChaseDist.get() * self->unk19C->mSLChaseDist.get()) {
			spine->pushAfterCurrent(&TNerveFlyEnemyChaseFly::theNerve());
			return true;
		}
	}

	if (!self->unk1A6 && self->isFindMario(1.0f) && self->unk1A0 > 100) {
		self->updateSquareToMario();
		if (self->mDistToMarioSquared
		    < self->unk19C->mSLChaseDist.get() * self->unk19C->mSLChaseDist.get()) {
			spine->pushAfterCurrent(&TNerveFlyEnemyChaseFly::theNerve());
			return true;
		}
	}

	JGeometry::TVec3<f32> vel(self->mVelocity);
	self->mRotation.x = MsGetRotFromZaxis(vel).x;

	f32 scale = 1.05f;
	scale *= self->mScaling.x;
	if (scale > self->mBodyScale) {
		scale = self->mBodyScale;
	} else if (scale < 0.0f) {
		scale = 0.0f;
	}
	self->mScaling.x = self->mScaling.y = self->mScaling.z = scale;

	return false;
}

// TODO: the ROM's `killer.o` emits out-of-line copies of four MathUtil /
// JGeometry inlines -- `MsWrap<f32>` (0x48 B, local), `MsCos` / `MsSin`
// (0x38 B each, weak) and `TVec3<f32>::set<f32>` (0x10 B, local) -- and
// `TNerveFlyEnemyChaseFly::execute` reaches all four through `bl`. We inline
// every one of them instead. Three `Ms*Killer` fakematches below force three
// of the four calls, and they are kept for now because deleting all three
// measured 18.8 points off `execute` (95.6 % -> 76.8 %, on the file state as of
// 2026-09-30; re-measure before trusting the exact figure). The matching bytes
// are worth far more than the wrong symbol name costs. See
// docs/AGENT_MATCHING_TIPS.md, "WHY a JGeometry inline is sometimes a `bl` in
// the ROM and inlined for us", and the open question recorded there about
// reproducing `MsCos__Ff` / `MsSin__Ff` / `MsWrap<f>__Ffff` under their real
// names.
//
// A fourth wrapper, `MsVec3SetKiller`, forces the `set<f32>` call. Measured
// 2026-09-30, both ways, in this exact file state:
//   - deleting it: `execute` 98.0 % -> 95.6 %, unit 87.14 % -> 86.90 % (-0.24).
//     So it is KEPT for now; the matching bytes beat the 16 B `extra` symbol.
//   - it does NOT reproduce `set<f>__Q29JGeometry8TVec3<f>Ffff` under any name,
//     so `set<f32>` stays `missing` in this TU either way;
//   - it also does NOT force `sub__Q29JGeometry8TVec3<f>…`,
//     `sqrt__Q29JGeometry8TUtil<f>Ff` or `moveRequest__10TTakeActorFRCQ29J…`
//     out of line -- those are `extra` in this object with *and* without it.
// TODO: the honest fix is a missing inline layer, not a wrapper.
// `TFlyEnemy::flyMove` (map size 0x2BC) is no longer an empty stub - it is
// reconstructed to exactly the map size - but it is still the one UNUSED
// function here that could plausibly be the missing inline layer: if the
// `orbit` store really went through an inlined `flyMove`, that would put the
// `set<f32>` at inline depth >= 2, which is where the JGeometry inliner
// refuses it (see the tips doc).

#pragma dont_inline on
static void MsVec3SetKiller(JGeometry::TVec3<f32>& v, f32 x, f32 y, f32 z)
{
	v.set(x, y, z);
}
#pragma dont_inline off

// TODO: the original MathUtil.hpp emitted MsWrap<f32> as a TU-local function
// (see the `MsWrap<f>__Ffff` symbol), so it is not inlined at every call site.
#pragma dont_inline on
static f32 MsWrapKiller(f32 t, f32 l, f32 r)
{
	if (l >= r)
		return l;

	while (t >= r)
		t -= r - l;
	while (t < l)
		t += r - l;

	return t;
}
#pragma dont_inline off

// The original MathUtil.hpp emitted MsCos/MsSin as TU-local out-of-line
// functions too (see the `MsCos__Ff` / `MsSin__Ff` symbols), so the ChaseFly
// nerve calls them instead of folding the sine table lookup in.
#pragma dont_inline on
static f32 MsCosKiller(f32 v) { return MsCos(v); }
static f32 MsSinKiller(f32 v) { return MsSin(v); }
#pragma dont_inline off

DEFINE_NERVE(TNerveFlyEnemyChaseFly, TLiveActor)
{
	TKiller* self = (TKiller*)spine->getBody();

	if (spine->getTime() == 0) {
		self->unk1A8 = self->mVelocity;
		self->calcChaseParam();
		self->setChaseFlyAnm();
	}

	f32 searchRange = 1.0f;
	if (self->unk1A5) {
		f32 bodyR2 = self->mScaledBodyRadius * self->mScaledBodyRadius;
		self->unk1A8.y = 0.1f;
		searchRange    = (self->unk1A8.length() / bodyR2) * TFlyEnemy::mTestSp;
	} else if (self->unk198 == 0) {
		searchRange = 2.0f;
	}

	self->walkBehavior(2, searchRange);

	if ((self->unk1A5 && self->isReachedToGoalXZ())
	    || (self->unk198 == 0 && self->mPosition.y < 100.0f + self->mGroundHeight))
		self->calcChaseParam();

	switch (self->unk198) {
	case 0:
	case 1:
		self->walkBehavior(3, 1.0f);
		break;

	case 2: {
		JGeometry::TVec3<f32> vel = self->mVelocity;
		vel.scale(0.9f);
		self->mVelocity = vel;

		const JGeometry::TVec3<f32>& pt = self->unkF4.getPoint();
		JGeometry::TVec3<f32> delta(pt);
		delta.sub(self->mPosition);

		// TODO: the result of this call is discarded in the original
		VECMag(&delta);

		f32 angle;
		if (delta.z == 0.0f) {
			if (delta.x >= 0.0f) {
				angle = 90.0f;
			} else {
				angle = -90.0f;
			}
		} else if (delta.z >= 0.0f) {
			angle = (360.0f / 65536.0f) * matan(delta.z, delta.x);
		} else {
			f32 t = (360.0f / 65536.0f) * matan(-delta.z, delta.x);
			angle = 180.0f - t;
		}

		f32 a = MsWrapKiller(angle, 0.0f, 360.0f);
		f32 d = a - MsWrapKiller(self->mRotation.y, a - 180.0f, a + 180.0f);
		if (d > 0.0f) {
			if (d > self->mTurnSpeed)
				d = self->mTurnSpeed;
		} else {
			if (d < -self->mTurnSpeed)
				d = -self->mTurnSpeed;
		}
		self->mRotation.y = MsWrapKiller(self->mRotation.y + d, 0.0f, 360.0f);

		JGeometry::TVec3<f32> lin = self->mLinearVelocity;
		f32 rot = self->mRotation.y;
		f32 r   = self->mScaledBodyRadius;
		JGeometry::TVec3<f32> orbit;
		f32 cy = r * MsCosKiller(rot);
		f32 cz = r * MsSinKiller(rot);
		MsVec3SetKiller(orbit, 0.0f, cy, cz);
		lin.add(orbit);
		self->mLinearVelocity = lin;
		break;
	}
	}

	if (self->unk198 != 2 && self->mPosition.y < 200.0f + self->mGroundHeight) {
		JGeometry::TVec3<f32> pos;
		pos.set(self->mScaledBodyRadius, -40.0f * self->getGravityY(),
		        self->mScaledBodyRadius);
		self->mRotation.x = MsGetRotFromZaxis(pos).x;

		JGeometry::TVec3<f32> up = self->mLinearVelocity;
		up.y = self->getGravityY();
		self->mRotation.x = MsGetRotFromZaxis(up).x;
	} else {
		self->mRotation.x *= 0.99f;
	}

	self->flyBehavior();

	f32 scale = 1.1f;
	scale *= self->mScaling.x;
	if (scale > self->mBodyScale) {
		scale = self->mBodyScale;
	} else if (scale < 0.0f) {
		scale = 0.0f;
	}
	self->mScaling.x = self->mScaling.y = self->mScaling.z = scale;

	return false;
}

TKillerSaveLoadParams::TKillerSaveLoadParams(const char* path)
    : TFlyEnemyParams(path)
    , PARAM_INIT(mSLWaterAddGravityY, 1.0f)
    , PARAM_INIT(mSLChaseTimer, 1000)
    , PARAM_INIT(mSLBombRange, 300.0f)
{
	TParams::load(mPrmPath);
}

TKillerManager::TKillerManager(const char* name)
    : TSmallEnemyManager(name)
{
	gpCurKiller = nullptr;
}

void TKillerManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TKillerSaveLoadParams("/enemy/killer.prm");
}

void TKillerManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "killer_model1.bmd", 0x10220000, 0 },
		{ "downkiller_model1.bmd", 0x10220000, 0 },
		{ nullptr, 0, 0 },
	};

	createModelDataArray(entry);
}

TSpineEnemy* TKillerManager::createEnemyInstance()
{
	// TODO: 79.6%. The ROM calls `JGeometry::TMatrix34<SMatrix34C<f32>>::TMatrix34()`
	// out of line (`bl`, 4 bytes, `weak`, alive at DOL 0x80006D88 - see
	// `__ct__Q29JGeometry38TMatrix34<...>Fv` in config/GMSP01/symbols.txt), and
	// that call is what forces the `stw r31, 0xc(r1)` plus the three
	// `lwz r31, 0xc(r1)` spill-reloads around it. We inline the empty ctor
	// instead, so all five instructions vanish. Fixing it means making
	// libs/JSystem/include/JSystem/JGeometry/JGMatrix34.hpp stop defining
	// `TMatrix34() { }` in-class - a libs/ edit, so it is left alone.
	return new TKiller;
}

static BOOL KillerBodyCallback(J3DNode* node, BOOL param_2)
{
	// TODO: 79.2%, the worst function in this TU. Three separate things are
	// wrong, and they are not the same problem as the frame deltas elsewhere:
	//  - frame 0xc8 vs our 0xb8. The stack area *above* the two matrices is
	//    identical on both sides (same lowest live slot, same shape); the ROM
	//    just reserves 12 more never-referenced bytes below it plus 4 for the
	//    8-byte-aligned `fctiwz` temp. That is the MWCC stack-padding bug, not
	//    a missing call - see docs/AGENT_MATCHING_TIPS.md.
	//  - the ROM re-loads `gpCurKiller` for the post-`bl` uses (r3 for the
	//    `getModel()` receiver, r7 for `mBodyScale`/`unk1B8`) instead of
	//    reusing the callee-saved r31 that already holds `self`, and loads it
	//    straight into r31 in the first place (no `mr r31, r0`). That looks
	//    like CSE being defeated on the global; not reproduced from source yet.
	//  - the `&&` chain's tail is a real bool in the ROM (`li r0,1 / b /
	//    li r0,0 / clrlwi / bne`, 7 instructions we optimise away). The
	//    `bool x = ...` trick in the tips emits `li 0` up front, which the ROM
	//    does not have, so the right spelling is still unknown.
	if (param_2 == 0) {
		TKiller* self = gpCurKiller;

		if (self != nullptr && TKiller::mRollSw
		    && self->mSpine->getCurrentNerve()
		           == &TNerveFlyEnemyChaseFly::theNerve()
		    && self->isBckAnm(1)) {
			MtxPtr mtx = self->getModel()
			                 ->getAnmMtx(((J3DJoint*)node)->getJntNo());

			Mtx rot;
			Mtx scl;

			scl[0][0] = self->mBodyScale;
			scl[0][1] = 0.0f;
			scl[0][2] = 0.0f;
			scl[1][0] = 0.0f;
			scl[1][1] = self->mBodyScale;
			scl[1][2] = 0.0f;
			scl[2][0] = 0.0f;
			scl[2][1] = 0.0f;
			scl[2][2] = self->mBodyScale;
			scl[0][3] = 0.0f;
			scl[1][3] = 0.0f;
			scl[2][3] = 0.0f;

			f32 s = MsSin(self->unk1B8);
			f32 c = MsCos(self->unk1B8);

			rot[0][0] = c;
			rot[0][1] = -s;
			rot[0][2] = 0.0f;
			rot[0][3] = 0.0f;
			rot[1][0] = s;
			rot[1][1] = c;
			rot[1][2] = 0.0f;
			rot[1][3] = 0.0f;
			rot[2][0] = 0.0f;
			rot[2][1] = 0.0f;
			rot[2][2] = 1.0f;
			rot[2][3] = 0.0f;

			PSMTXConcat(mtx, rot, mtx);
			PSMTXConcat(mtx, scl, mtx);
			PSMTXConcat(J3DSys::mCurrentMtx, rot, J3DSys::mCurrentMtx);
			PSMTXConcat(J3DSys::mCurrentMtx, scl, J3DSys::mCurrentMtx);
		}
	}

	return TRUE;
}

TKiller::TKiller(const char* name)
    : TFlyEnemy(name)
{
}

void TKiller::init(TLiveManager* manager)
{
	TFlyEnemy::init(manager);
	mActorType    = 0x1000001F;
	unk150        = 0x11;
	mKillerParams = (TKillerSaveLoadParams*)getSaveParam();
	mSpine->initWith(&TNerveFlyEnemyNormalFly::theNerve());
	onLiveFlag(LIVE_FLAG_UNK400);
	offLiveFlag(LIVE_FLAG_UNK800);
	onHitFlag(HIT_FLAG_UNK40000000);

	// TODO: frame is 8 bytes too small
	J3DModel* model = getMActor()->getModel();
	if (!model->getSkinDeform()) {
		J3DSkinDeform* deform = new J3DSkinDeform;
		model->setSkinDeform(deform, J3D_DEFORM_ATTACH_FLAG_UNK_1);
	}
	getMActor()->resetDL();

	if (mInstanceIndex == 0) {
		for (u8 i = 0; i < getModel()->getModelData()->getJointNum(); ++i)
			;
	}

	getMActor()->setJointCallback(1, KillerBodyCallback);
	unk188 = 0.0f;
}

void TKiller::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 2);
	mMActor       = mMActorKeeper->createMActor("killer_model1.bmd", 3);
	mMActorKeeper->createMActor("downkiller_model1.bmd", 3);

	s32 noseMatIdx = getActorKeeper()
	                     ->getMActor("killer_model1.bmd")
	                     ->getModel()
	                     ->getModelData()
	                     ->getMaterialName()
	                     ->getIndex("_nosemat1");
	s32 eyesMatIdx = getActorKeeper()
	                     ->getMActor("killer_model1.bmd")
	                     ->getModel()
	                     ->getModelData()
	                     ->getMaterialName()
	                     ->getIndex("_eyesmat1");
	s32 bodyMatIdx = getActorKeeper()
	                     ->getMActor("killer_model1.bmd")
	                     ->getModel()
	                     ->getModelData()
	                     ->getMaterialName()
	                     ->getIndex("_body1");

	SMS_InitPacket_OneTevColor(getMActor()->getModel(), noseMatIdx, GX_TEVREG0,
	                           &mNoseColor);
	SMS_InitPacket_OneTevColor(getMActor()->getModel(), eyesMatIdx, GX_TEVREG0,
	                           &mEyesColor);
	SMS_InitPacket_OneTevColor(getMActor()->getModel(), bodyMatIdx, GX_TEVREG0,
	                           &mBodyColor);
	SMS_InitPacket_OneTevColor(
	    getActorKeeper()->getMActor("downkiller_model1.bmd")->getModel(),
	    bodyMatIdx, GX_TEVREG0, &mBaseColor);
}

void TKiller::behaveToWater(THitActor* water)
{
	if (mSpine->getCurrentNerve() == &TNerveFlyEnemyNormalFly::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve())
		genEventCoin();

	if (mSpine->getCurrentNerve() != &TNerveKillerExplosion::theNerve()) {
		mSpine->pushNerve(&TNerveKillerExplosion::theNerve());
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
	}
}

void TKiller::genEventCoin()
{
	int num = 2;
	if (unk1A6)
		num = 8;

	for (int i = 0; i < num; ++i) {
		JGeometry::TVec3<f32> offset(0.0f, 0.0f, 30.0f);
		Mtx mtx;
		MsMtxSetRotY(mtx, 360.0f * (1.0f / num) * (i + 1));
		MTXMultVec(mtx, &offset, &offset);
		TMapObjBase* coin = gpItemManager->makeObjAppear(
		    mPosition.x + offset.x, mPosition.y, mPosition.z + offset.z,
		    0x2000000E, true);
		if (coin) {
			coin->mPosition.y = mPosition.y;
			MsVECNormalize(&offset, &offset);
			coin->mVelocity.set(3.0f * offset.x, 20.0f, 3.0f * offset.z);
			coin->offLiveFlag(LIVE_FLAG_UNK10);
		}
	}
}

bool TKiller::isHitValid(u32 message)
{
	if (message == HIT_MESSAGE_UNKB) {
		onLiveFlag(LIVE_FLAG_DEAD);
		onHitFlag(HIT_FLAG_NO_COLLISION);
		genEventCoin();
		return false;
	}

	if (mTrampleDie)
		unk194 -= 12.0f;
	return false;
}

void TKiller::setDeadAnm()
{
	mMActor = getActorKeeper()->getMActor("downkiller_model1.bmd");
	setBckAnm(0);
	TEffectExplosion* effect
	    = (TEffectExplosion*)gpConductor->makeOneEnemyAppear(
	        mPosition, "エフェクト爆発マネージャー", 1);
	if (effect != nullptr)
		effect->generate(mPosition, mScaling);
	gpCameraShake->startShake(CAM_SHAKE_MODE_UNK6, 1.0f);
	SMSRumbleMgr->start(0x15, 5, (f32*)nullptr);
}

void TKiller::attackToMario()
{
	if (SMS_GetMarioPos().y < mPosition.y) {
		if (mSpine->getCurrentNerve() != &TNerveKillerExplosion::theNerve()) {
			mSpine->pushNerve(&TNerveKillerExplosion::theNerve());
			sendAttackMsgToMario();
			return;
		}
		SMS_SendMessageToMario(this, HIT_MESSAGE_UNKA);
	}
}

const char** TKiller::getBasNameTable() const { return killer_bastable; }

void TKiller::setNormalFlyAnm()
{
	mMActor = getActorKeeper()->getMActor("killer_model1.bmd");
	setBckAnm(2);
	unk1B8 = 0.0f;
	unk1A0 = 0;
}

void TKiller::setChaseFlyAnm() { setBckAnm(3); }

bool TKiller::isRollFly()
{
	if (mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve()
	    && isBckAnm(1))
		return true;
	return false;
}

bool TKiller::isCollidMove(THitActor* other)
{
	if (other->isActorType(0x4000000A))
		((TLiveActor*)other)->kill();

	if (mSerialBomb
	    && mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve())
		mSpine->pushNerve(&TNerveKillerExplosion::theNerve());

	return true;
}

void TKiller::flyBehavior()
{
	mTurnSpeed = mKillerParams->mSLTurnSpeedLow.get();
	if (mSpine->getTime() > mKillerParams->mSLChaseTimer.get())
		unk194 -= mKillerParams->mSLWaterAddGravityY.get();

	if (checkCurAnmEnd(0) && isBckAnm(3))
		setBckAnm(1);

	unk1B8 += 2.5f;
}

void TKiller::changeOut()
{
	SMSGetMSound()->startSoundActor(MSD_SE_EN_TELSA_RECOVER, &mPosition, 0,
	                                nullptr, 0, 4);
	onLiveFlag(LIVE_FLAG_DEAD);
	genEventCoin();
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mPosition = mJuiceBlock->getPosition();
	gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_TLS_CHANGE,
	                                            &mPosition, 0, nullptr);
	getMActor()->setFrameRate(SMSGetAnmFrameRate(), 0);
	mJuiceBlock->kill();
	mJuiceBlock = nullptr;
}

void TKiller::reset()
{
	gpCurKiller = this;
	TFlyEnemy::reset();

	TMsRange<f32> range(0.0f, 1.0f);
	mBodyColor.r = mBodyColor.g = mBodyColor.b = mBaseColor.r = mBaseColor.g
	    = mBaseColor.b                         = 0;
	unk1A6                                     = 0;

	if (range.rand() < 0.05f) {
		unk1A6       = 1;
		mBodyColor.r = 200;
		mBodyColor.g = 185;
		mBodyColor.b = 0;
		mBaseColor.r = 255;
		mBaseColor.g = 225;
		mBaseColor.b = 70;
	}
}

void TKiller::setColorType()
{
	if (unk1A6)
		unk1A5 = 0;

	if (unk1A5) {
		mBodyColor.r = 70;
		mBodyColor.g = 20;
		mBodyColor.b = 70;
		mBaseColor.r = 70;
		mBaseColor.g = 20;
		mBaseColor.b = 70;
	}
}

void TKiller::bind()
{
	if (mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve()
	    || unk1A0 < TFlyEnemy::mInvalidTime) {
		fly();
	} else {
		TLiveActor::bind();
	}

	unk1A0++;

	if (mSpine->getCurrentNerve() != &TNerveKillerExplosion::theNerve()) {
		if (!checkLiveFlag2(0x01000000) && unk1A0 > TFlyEnemy::mInvalidTime) {
			mSpine->pushNerve(&TNerveKillerExplosion::theNerve());
		} else if (unk1A0 > TFlyEnemy::mInvalidTime) {
			TBGWallCheckRecord rec(mPosition.x, mPosition.y + mHeadHeight,
			                      mPosition.z, 2.0f * mBodyRadius, 1, 0);

			if (gpMap->isTouchedWallsAndMoveXZ(&rec)) {
				const TLiveActor* la = rec.mResultWalls[0]->mActor;

				if (la != nullptr) {
					if (((u32)(la->getActorType() - 0x4000) <= 10) ? true
					                                            : false)
						((TSmallEnemy*)la)->kill();
				}
			}

			mSpine->pushNerve(&TNerveKillerExplosion::theNerve());
		}
	}

	if (checkLiveFlag2(0x01000000)) {
		if (gpMSound->gateCheck(0x20A9)) {
			MSoundSESystem::MSoundSE::startSoundActor(0x20A9, &mPosition, 0,
			                                          nullptr, 0, 4);
		}

		f32 rot = mRotation.x;
		if (rot > 90.0f) {
			rot = 90.0f;
		} else if (rot < -25.0f) {
			rot = -25.0f;
		}
		mRotation.x = rot;

		MsMtxSetXYZRPH((MtxPtr)&unk1BC, mPosition.x, mPosition.y, mPosition.z,
		               mRotation.x, mRotation.y, mRotation.z);

		gpMarioParticleManager->emitAndBindToMtxPtr(0x174, (MtxPtr)&unk1BC, 1,
		                                            this);
	}
}

void TKiller::calcRootMatrix()
{
	if (gpMarDirector->mFlags & 0xF) {
		onLiveFlag(1);
		onHitFlag(1);
	}

	if (isBckAnm(2)) {
		if (unk1A6) {
			mEyesColor.r = 0xAA;
			mNoseColor.r = 0xAA;
			mEyesColor.g = 0x8C;
			mNoseColor.g = 0x8C;
			mEyesColor.b = 0;
			mNoseColor.b = 0;
		} else {
			mEyesColor.r = mEyesColor.g = mEyesColor.b = 0;
			mNoseColor.r = mNoseColor.g = mNoseColor.b = 0;
		}
	}

	if (isBckAnm(3)) {
		mNoseColor.b = 0;
		mNoseColor.g = 0;
		mEyesColor.r   = 0;

		if (mSpine->getTime() % 10 < 5) {
			mNoseColor.r = 0;
			mEyesColor.b = 0;
			mEyesColor.g = 0;
		}

		if (unk1A6) {
			if (mSpine->getTime() % 10 < 5) {
				mNoseColor.r = 0xAA;
				mNoseColor.g = 0x8C;
				mNoseColor.b = 0;
			} else {
				mNoseColor.r = 0xB4;
				mNoseColor.g = 0x8C;
				mNoseColor.b = 0x96;
			}
		}

		if (unk1A5) {
			if (mSpine->getTime() % 10 < 5) {
				mEyesColor.r = 0xC8;
				mNoseColor.r = 0xC8;
				mNoseColor.g = 0;
				mNoseColor.b = 0;
			} else {
				mNoseColor.r = 0x46;
				mNoseColor.g = 0x14;
				mNoseColor.b = 0x46;
			}
		}
	}

	if (isBckAnm(1)) {
		mEyesColor.r = 0;
		mEyesColor.b = 0;
		mEyesColor.g = 0;
		mNoseColor.b = 0;
		mNoseColor.g = 0;

		f32 s = fabs(MsSin((360.0f * mSpine->getTime()) / 120.0f));

		if (unk1A6) {
			mBodyColor.r = 0xAA;
			mBodyColor.g = 0x8C;
			mBodyColor.b = 0;
			// The original narrows these to 8 bits before storing them.
			mNoseColor.r = (u8)(160.0f + 10.0f * s);
			mNoseColor.g = (u8)(140.0f + 30.0f * s);
			mNoseColor.b = (u8)(150.0f * s);
		}

		if (unk1A5) {
			mBodyColor.r = 70;
			mBodyColor.g = 20;
			mBodyColor.b = 70;
			mBaseColor.r = 70;
			mBaseColor.g = 20;
			mBaseColor.b = 70;
			mNoseColor.r = (u8)(70.0f + 130.0f * s);
			mNoseColor.g = (u8)(20.0f - 20.0f * s);
			mNoseColor.b = (u8)(70.0f - 70.0f * s);
		}
	}

	gpCurKiller = this;

	f32 rot = mRotation.x;
	if (rot > 90.0f) {
		rot = 90.0f;
	} else if (rot < -25.0f) {
		rot = -25.0f;
	}
	mRotation.x = rot;

	TSpineEnemy::calcRootMatrix();
}

bool TKiller::isFindMario(f32 param_1)
{
	TSmallEnemyParams* prms = getSaveParams();

	f32 searchHeight = prms->mSLSearchHeight.get();

	if (fabs(SMS_GetMarioY() - mPosition.y) < searchHeight) {
		JGeometry::TVec3<f32> marioPos(SMS_GetMarioX(), SMS_GetMarioY(),
		                               SMS_GetMarioZ());

		f32 searchLength = prms->mSLSearchLength.get();
		f32 searchAngle  = prms->mSLSearchAngle.get();
		f32 searchAware  = prms->mSLSearchAware.get();

		if (isInSight(marioPos, searchLength * param_1, searchAngle * param_1,
		              searchAware * param_1))
			return true;
		else
			return false;
	}

	return false;
}

DEFINE_NERVE(TNerveKillerExplosion, TLiveActor)
{
	TKiller* self = (TKiller*)spine->getBody();

	if (spine->getTime() == 0) {
		self->unk20C = ((TKillerSaveLoadParams*)self->getSaveParam())
		                   ->mSLBombRange.get()
		               * self->getBodyScale() / self->mAttackRadius;
		self->mRotation.x = 0.0f;
		self->setDeadAnm();
		if (!self->isAirborne()) {
			if (self->getGroundPlane()->isWaterSurface()) {
				TEffectBombColumWater* water
				    = (TEffectBombColumWater*)gpConductor->makeOneEnemyAppear(
				        self->mPosition, "エフェクト爆発水柱マネージャー", 1);
				if (water) {
					JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
					water->generate(self->mPosition, scale);
				}
			}
			if (self->getGroundPlane()->isSand()) {
				TEffectColumSand* sand
				    = (TEffectColumSand*)gpConductor->makeOneEnemyAppear(
				        self->mPosition, "エフェクト砂柱マネージャー", 1);
				if (sand) {
					JGeometry::TVec3<f32> scale(0.6f, 0.9f, 0.6f);
					sand->generate(self->mPosition, scale);
				}
			}
		}
		SMSRumbleMgr->start(0x13, &self->mPosition);
	}

	if (self->unk190 < self->unk20C) {
		self->unk190 *= 1.3f;
	} else {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		if (self->checkCurAnmEnd(0)) {
			self->onLiveFlag(LIVE_FLAG_DEAD);
			self->onLiveFlag(LIVE_FLAG_UNK8);
			self->offLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
			self->mHolder = nullptr;
			self->stopAnmSound();
			spine->reset();
			spine->setNext(&TNerveSmallEnemyDie::theNerve());
			spine->pushAfterCurrent(spine->getDefault());
			self->mPosition.y -= 200.0f;
			return true;
		}
	}

	self->expandCollision();
	return false;
}
