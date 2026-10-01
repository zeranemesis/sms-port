#include <dolphin/mtx.h>
#include <Enemy/BossWanwan.hpp>
#include <Enemy/Graph.hpp>
#include <System/Particles.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/MtxUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
// J3DModel.hpp only forward-declares J3DSkinDeform; the full definition (a
// 0x14-byte object) lives in J3DCluster.hpp, which init() needs for `new`.
#include <JSystem/J3D/J3DGraphAnimator/J3DCluster.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <System/Application.hpp>
#include <System/EmitterViewObj.hpp>
#include <math.h>
#include <MSound/MSound.hpp>
#include <Camera/CameraShake.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <Player/MarioAccess.hpp>
#include <System/MarDirector.hpp>
#include <Player/Mario.hpp>
#include <GC2D/GCConsole2.hpp>
#include <MSound/MSoundSE.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <Enemy/Conductor.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <MoveBG/ItemManager.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Enemy/EffectObj.hpp>
// rogue include: TEnemyNameRefGroup lives in popo.hpp
#include <Enemy/popo.hpp>

// rogue include: dummy string pair, needed to match the .rodata prologue
#include <System/DummyStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

static JGeometry::TVec3<f32> BW_BATH_POS(-1000.0f, 4.5f, -6217.2f);
static JGeometry::TVec3<f32> BW_PICKET_START(6012.84f, 0.0f, 7323.15f);
static JGeometry::TVec3<f32> BW_HEAD_START(5741.72f, -100.0f, 6311.62f);

static const char* bwanwan_bastable[7] = {
	"/scene/bwanwan/bas/bwanwan_bark.bas",
	nullptr,
	"/scene/bwanwan/bas/bwanwan_shake.bas",
	nullptr,
	"/scene/bwanwan/bas/bwanwan_wait.bas",
	"/scene/bwanwan/bas/bwanwan_wait2.bas",
	nullptr,
};

TBWParams::TBWParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLMarchSpeed, 6.0f)
    , PARAM_INIT(mSLTurnSpeed, 1.0f)
    , PARAM_INIT(mSLLeashNodeLen, 120.0f)
    , PARAM_INIT(mSLPicketHeight, 100.0f)
    , PARAM_INIT(mSLPicketRadius, 100.0f)
    , PARAM_INIT(mSLChainHitHeight, 100.0f)
    , PARAM_INIT(mSLChainHitRadius, 100.0f)
    , PARAM_INIT(mSLChainGroundRadius, 60.0f)
    , PARAM_INIT(mSLPullLimit, 1.0f)
    , PARAM_INIT(mSLAttackSpeed, 10.0f)
    , PARAM_INIT(mSLStunTimer, 4000)
    , PARAM_INIT(mSLSearchLength, 10000.0f)
    , PARAM_INIT(mSLSearchAngle, 60.0f)
    , PARAM_INIT(mSLBWHitPointMax, 255)
    , PARAM_INIT(mSLHeadGap, 150.0f)
    , PARAM_INIT(mSLShakeLengthMax, 3000.0f)
    , PARAM_INIT(mSLShakeLengthMaxHP0, 2000.0f)
{
	TParams::load(mPrmPath);
}

void TBWLeashNode::calcTemperature()
{
	if (mIndex == 0)
		return;

	int prev = mIndex - 1;
	f32 diff = mLeash->mNodes[prev]->mTemperature - mTemperature;

	f32 delta;
	if (diff < 0.0f) {
		if (diff < -0.1f)
			delta = -0.02f;
		else
			delta = -0.005f;
	} else {
		if (diff > 0.1f)
			delta = 0.02f;
		else
			delta = 0.005f;
	}

	mTemperature += delta;
	if (mTemperature < 0.0f)
		mTemperature = 0.0f;
	if (mTemperature > 1.0f)
		mTemperature = 1.0f;
}

void TBWLeashNode::calcMatrix()
{
	TPosition3f* mtx    = (TPosition3f*)mMActor->mModel->getBaseTRMtx();
	TRope* rope          = mLeash->mRope;
	TRopePoint* points   = rope->mPoints;
	JGeometry::TVec3<f32> pos = points[mIndex].unkC;
	JGeometry::TVec3<f32> dir;

	if (mIndex < (int)rope->mNumPoints - 1) {
		dir = points[mIndex + 1].unkC;
		dir.sub(pos);
	} else {
		dir = points[mIndex - 1].unkC;
		dir.sub(pos);
		dir.negate();
	}

	PSVECNormalize(&dir, &dir);

	JGeometry::TVec3<f32> up;
	up.set(0.0f, 1.0f, 0.0f);

	JGeometry::TVec3<f32> v1;
	v1.cross(up, dir);
	PSVECNormalize(&v1, &v1);

	JGeometry::TVec3<f32> v2;
	v2.cross(dir, v1);
	PSVECNormalize(&v2, &v2);

	mtx->setZDir(dir);

	if (mIndex & 0x80000000) {
		mtx->setXDir(v1);
		mtx->setYDir(v2);
	} else {
		mtx->setXDir(v2);
		mtx->setYDir(v1);
	}

	mtx->setTrans(pos.x, pos.y + 0.55f, pos.z);

	mPosition = pos;
}

void TBWLeashNode::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_MOVE) {
		calcTemperature();
		calcMatrix();

		if (mLeash->mOwner->mHitPoints != 0 && mIndex < 8) {
			for (int i = 0; i < (int)mColCount; ++i) {
				THitActor* other = mCollisions[i];
				if (other->mActorType == 0x80000001)
					other->receiveMessage(this, 10);
			}
		}
	}

	if (cue & (1 << 30)) {
		J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(5);
		if (ctrl) {
			f32 len = mTemperature * ((f32)(ctrl->getEnd() - 1));
			f32 scale;

			if (mIndex < 5) {
				scale = (f32)mIndex * 0.25f
				      + (f32)mIndex
				            / (f32)mLeash->mOwner->getBWParams()
				                  ->mSLBWHitPointMax.value;
			} else {
				int count = mLeash->mRope->mNumPoints;

				if (mIndex < count - 10)
					scale = 1.0f;
				else
					scale = (f32)(count - mIndex) / 10.0f;
			}

			if (scale > 1.0f)
				scale = 1.0f;
			else if (scale < 0.0f)
				scale = 0.0f;

			ctrl->setFrame(len * scale);
			ctrl->setRate(0.0f);
		}
	}

	if (mIndex < (int)mLeash->mRope->mNumPoints - 1)
		mMActor->perform(cue, graphics);
}

TBWLeash::TBWLeash(TBossWanwan* owner, int count, const char* name)
    : TViewObj(name)
    , mOwner(owner)
    , mRope(nullptr)
    , mNodes(nullptr)
{
	// TODO: the two rope scalars are read back out of the save params at
	// 0xe0 / 0x144 in the ROM, i.e. mSLLeashNodeLen / mSLChainGroundRadius.
	// The param field names are themselves guesses, so treat them as such.
	TBWParams* p1 = (TBWParams*)owner->getSaveParam();
	f32 a         = p1->mSLLeashNodeLen.get();
	TBWParams* p2 = (TBWParams*)owner->getSaveParam();
	f32 b         = p2->mSLChainGroundRadius.get();
	mRope         = new TRope(count, mOwner->mPosition, a, b, 0.7f, -2.0f);
	mNodes        = new TBWLeashNode*[count];

	for (int i = 0; i < count; ++i) {
		mNodes[i] = new TBWLeashNode(this, i, "鎖部品");

		mNodes[i]->mMActor
		    = mOwner->mMActorKeeper->createMActor("bwanwan_chain.bmd", 0);
		mNodes[i]->mMActor->setBrkFromIndex(2);

		TBWParams* p3 = (TBWParams*)owner->getSaveParam();
		f32 r          = p3->mSLChainHitRadius.get();
		TBWParams* p4 = (TBWParams*)owner->getSaveParam();
		f32 h          = p4->mSLChainHitHeight.get();
		mNodes[i]->initHitActor(0x8000080c, 1, 0x8000, 1.2f * r, 1.2f * h, r,
		                        h);
	}

	TEnemyNameRefGroup* group
	    = (TEnemyNameRefGroup*)JDrama::TNameRef::search("敵グループ");

	for (int i = 0; i < count; ++i) {
		group->mObjects.insert(group->mObjects.end(), mNodes[i]);

		if (i < count - 2)
			mNodes[i]->offHitFlag(HIT_FLAG_NO_COLLISION);
		else
			mNodes[i]->onHitFlag(HIT_FLAG_NO_COLLISION);

		if (i < 2)
			mRope->mPoints[i].unk28 |= 1;
		else
			mRope->mPoints[i].unk28 &= ~1u;
	}
}

// TODO: mOwner/mRope/mNodes field names and the two cue bits are still guesses.
void TBWLeash::perform(u32 cue, JDrama::TGraphics* graphics)
{
	JGeometry::TVec3<f32> head;
	JGeometry::TVec3<f32> diff;
	JGeometry::TVec3<f32> pos;
	JGeometry::TVec3<f32> head2;
	JGeometry::TVec3<f32> diff2;
	JGeometry::TVec3<f32> result;

	if (cue & CUE_MOVE) {
		mOwner->getJointTransByIndex(5, &head);
		mRope->moveHead(head);

		mOwner->unk188 = 0;

		if (mOwner->unk17C) {
			result = mRope->mPoints[0].unkC;
			mRope->constraintTail(mOwner->unk158->mPosition);
			result.sub(mRope->mPoints[0].unkC);
			result.x = -result.x;
			result.y = -result.y;
			result.z = -result.z;
			mOwner->unk15C = result;

			diff   = mRope->mPoints[0].unkC;
			pos    = mOwner->mPosition;
			pos.y += 500.0f;
			diff.sub(pos);

			if (PSVECMag(&diff) > 650.0f) {
				PSVECNormalize(&diff, &diff);
				diff.x *= 20.0f;
				diff.y *= 20.0f;
				diff.z *= 20.0f;
				diff.y = 0.0f;

				mOwner->mPosition.add(diff);
				mOwner->unk188 = 1;
			}

			f32 yaw;
			if (diff.z == 0.0f) {
				if (diff.x >= 0.0f)
					yaw = 90.0f;
				else
					yaw = -90.0f;
			} else if (diff.z >= 0.0f) {
				s16 ang = matan(diff.z, diff.x);
				yaw     = (360.0f / 65536.0f) * (f32)ang;
			} else {
				s16 ang  = matan(-diff.z, diff.x);
				f32 theta = (360.0f / 65536.0f) * (f32)ang;
				yaw       = 180.0f - theta;
			}

			yaw = 180.0f + yaw;
			while (yaw >= 360.0f)
				yaw -= 360.0f;
			while (yaw < 0.0f)
				yaw += 360.0f;

			f32 rot    = mOwner->mRotation.y;
			f32 diff3  = yaw - MsWrap(yaw - 180.0f, rot, 180.0f + yaw);
			if (diff3 > 0.0f)
				diff3 = MsMin(diff3, 1.5f * mOwner->mTurnSpeed);
			else
				diff3 = MsMax(diff3, 1.5f * -mOwner->mTurnSpeed);

			f32 target = rot + diff3;
			while (target >= 360.0f)
				target -= 360.0f;
			while (target < 0.0f)
				target += 360.0f;
			mOwner->mRotation.y = target;
		}
	}

	if (cue & 0x2) {
		mOwner->getJointTransByIndex(1, &head2);

		for (int i = 0; i < 15; ++i) {
			diff2 = mRope->mPoints[i].unkC;
			diff2.sub(head2);

			if (PSVECMag(&diff2) < 500.0f) {
				diff2 *= 500.0f / PSVECMag(&diff2);
				diff2.add(head2);

				mRope->mPoints[i].unk18.zero();
				mRope->mPoints[i].unkC   = diff2;
				mRope->mPoints[i].unk0   = mRope->mPoints[i].unkC;
			}
		}
	}

	for (int i = 0; i < (int)mRope->mNumPoints; ++i)
		mNodes[i]->testPerform(cue, graphics);
}

BOOL TBWPicket::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->mActorType == 0x80000001) {
		if (message == 1) {
			TBossWanwan* owner = mOwner;
			owner->unk17C      = 1;
			owner->unk184      = 0;
			if (gpMSound->gateCheck(0x28c0))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x28c0, &mPosition, 0, nullptr, 0, 4);
			return TRUE;
		}
		if (message == 4) {
			TBossWanwan* owner = mOwner;
			if (owner->unk17C) {
				JPABaseEmitter* emitter = gpMarioParticleManager->emit(
				    0xae, &owner->unk158->mPosition, 0, nullptr);
				if (emitter)
					emitter->setGlobalScale(
					    JGeometry::TVec3<f32>(0.3f, 0.5f, 0.3f));
			}
			owner->unk194 = 0;
			owner->unk17C = 0;
			mHolder       = static_cast<TTakeActor*>(sender);
			return TRUE;
		}
		if (message == 7 || message == 8) {
			mHolder = nullptr;
			return TRUE;
		}
	}
	return FALSE;
}

// TODO: fabricated wrapper; the ROM calls TSpineBase<TLiveActor>::getLatestNerve()
// out of line here instead of inlining it, which we cannot express without touching
// Strategic/Spine.hpp.
static const TNerveBase<TLiveActor>* getLatestNerveOutOfLine(
    TSpineBase<TLiveActor>* spine)
{
	return spine->getLatestNerve();
}

BOOL TBWPicket::moveRequest(const JGeometry::TVec3<f32>& where_to)
{
	TBossWanwan* owner = mOwner;

	const TNerveBase<TLiveActor>* nerve = getLatestNerveOutOfLine(owner->mSpine);
	if (nerve == &TNerveBWJumpToBath::theNerve()
	    || nerve == &TNerveBWDie::theNerve())
		return FALSE;

	if (owner->mHitPoints != 0)
		return FALSE;

	TRope* rope = owner->mLeash->mRope;
	JGeometry::TVec3<f32> result = rope->mPoints[0].unkC;
	rope->constraintTail(where_to);
	result -= rope->mPoints[0].unkC;
	result.x = -result.x;
	result.y = -result.y;
	result.z = -result.z;
	owner->unk15C = result;
	return TRUE;
}

MtxPtr TBWPicket::getTakingMtx()
{
	return reinterpret_cast<MtxPtr>(reinterpret_cast<char*>(this) + 0x74);
}

void TBWPicket::perform(u32, JDrama::TGraphics*)
{
	// TODO: not decompiled
}

BOOL TBWHit::receiveMessage(THitActor* sender, u32 message)
{
	return mOwner->receiveMessage(sender, message);
}

void TBWHit::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_MOVE) {
		if (mJointIndex >= 0)
			mOwner->getJointTransByIndex(mJointIndex, &mPosition);

		for (int i = 0; i < mColCount; ++i) {
			THitActor* other = mCollisions[i];
			if (mOwner->mHitPoints != 0 && other->mActorType == 0x80000001)
				other->receiveMessage(mOwner, 10);
		}
	}
	THitActor::perform(cue, graphics);
}

void TBWBinder::bind(TLiveActor* actor)
{
	// Movement integration: the actor's linear velocity is added to its
	// position, and while it is airborne the (gravity-decayed) velocity is
	// added on top of that as well.
	TBossWanwan* self = (TBossWanwan*)actor;
	JGeometry::TVec3<f32> vel = actor->mLinearVelocity;
	JGeometry::TVec3<f32> pos = actor->mPosition;
	pos += vel;

	if (actor->isAirborne()) {
		JGeometry::TVec3<f32> v = actor->mVelocity;
		pos += v;

		v.y -= actor->getGravityY();
		if (v.y < TLiveActor::mVelocityMinY)
			v.y = TLiveActor::mVelocityMinY;
		actor->mVelocity = v;
	}

	// While the boss is being carried to the bath or is already dead it does
	// not collide with anything, it just keeps its horizontal drift.
	if (actor->mSpine->getLatestNerve() == &TNerveBWJumpToBath::theNerve()
	    || actor->mSpine->getLatestNerve() == &TNerveBWDie::theNerve()) {
		actor->mLinearVelocity = pos - actor->mPosition;
		return;
	}

	if (actor->isAirborne()) {
		const TBGCheckData* data;
		f32 ground = gpMap->checkGround(
		                pos.x,
		                pos.y + ((TBossWanwan*)actor)->getHeadHeight(), pos.z,
		                &data)
		             + 1.0f;

		// Also probe straight up from the current position: if that finds
		// higher ground (e.g. a ledge) prefer it.
		if (actor->mPosition.y > pos.y
		    && (data->mBGType
		              == BG_TYPE_EVERYTHING_BUT_MAP_OBJECTS_PHASE_THROUGH
		        || data->mBGType
		               == BG_TYPE_ONLY_ENEMIES_PHASE_THROUGH)) {
			const TBGCheckData* data2;
			f32 ground2 = gpMap->checkGround(
			                pos.x,
			                actor->mPosition.y
			                    + ((TBossWanwan*)actor)->getHeadHeight(),
			                pos.z, &data2)
			              + 1.0f;

			if (ground2 > ground) {
				ground = ground2;
				data   = data2;
			}
		}

		// Land only on walk-through-free ground the actor is allowed to
		// stand on; otherwise stay airborne.
		// TODO: 0x20 is an unnamed bit of TBGCheckData::mFlags.
		if (pos.y <= ground && (data->mFlags & 0x20)
		    && (data->mBGType
		            == BG_TYPE_EVERYTHING_BUT_MAP_OBJECTS_PHASE_THROUGH
		        || data->mBGType == BG_TYPE_ONLY_ENEMIES_PHASE_THROUGH)) {
			pos.y = ground;
			actor->mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
			// TODO: the two cleared masks are not named anywhere yet.
			actor->offLiveFlag(0x03800000);
			actor->offLiveFlag(0x1C000);
		} else {
			actor->onLiveFlag(LIVE_FLAG_AIRBORNE);
		}

		actor->mGroundHeight = ground;
		actor->mGroundPlane  = data;
	}

	// From here on `dir` is the horizontal displacement produced by this
	// frame; the slot originally held `vel`.
	JGeometry::TVec3<f32> dir = pos - actor->mPosition;

	if (!actor->isAirborne()) {
		// Look ahead along the graph: clamp how far along the current graph
		// link we are allowed to walk in one frame.
		TGraphTracer* tracer = self->unk124;
		if (tracer->getGraph() != nullptr
		    && tracer->getCurGraphIndex() >= 0 && tracer->getPrevIndex() >= 0
		    && tracer->getCurGraphIndex() != tracer->getPrevIndex()) {
			JGeometry::TVec3<f32> pathDir;
			JGeometry::TVec3<f32> prevDir;
			tracer->getGraph()
			    ->getGraphNode(tracer->getCurGraphIndex())
			    .getPoint(&pathDir);
			tracer->getGraph()
			    ->getGraphNode(tracer->getPrevIndex())
			    .getPoint(&prevDir);

			pathDir -= prevDir;
			PSVECNormalize(&pathDir, &pathDir);

			f32 len2 = pathDir.squared();
			f32 t    = 0.0f;
			if (len2 != 0.0f)
				t = dir.dot(pathDir) / len2;

			// Never walk less than three nodes per frame while moving
			// along the link, and never more than the raw projection.
			if (t > 0.0f) {
				if (t < 3.0f)
					t = 3.0f;
			} else if (t < 0.0f) {
				if (t > -3.0f)
					t = -3.0f;
			}

			dir      = pathDir;
			dir *= t;
		}
	}

	if (!actor->isAirborne()) {
		// Turn towards the direction of travel; the sharper the turn the
		// closer to the leash head, so the boss keeps looking at Mario.
		f32 mag = PSVECMag(&dir);
		if (mag != 0.0f) {
			s16 idx = (s16)(65536.0f / 360.0f * actor->mRotation.y);

			JGeometry::TVec3<f32> facing;
			// The redundant 1.0f factors are what the ROM emits.
			facing.set(1.0f * JMASSin(idx), 0.0f, 1.0f * JMASCos(idx));

			f32 turn = 360.0f * (mag / (2.0f * 3.1415927f * 500.0f));
			if (dir.dot(facing) < 0.0f)
				turn = -turn;
			turn *= 2.0f;

			if (self->unk16C != 0) {
				self->unk168 = MsWrap(self->unk168 + turn, 0.0f, 360.0f);
			} else if (self->unk168 != 0.0f) {
				f32 wrapped = self->unk168 + turn;
				if (wrapped > 360.0f)
					wrapped = 0.0f;
				self->unk168 = wrapped;
			}
		}
	}

	if (self->unk17C != 0) {
		// The leash is taut: yank the boss back to a fixed distance from
		// the leash head.
		JGeometry::TVec3<f32> ropePos
		    = self->mLeash->mRope->mPoints[3].unkC;
		JGeometry::TVec3<f32> here = actor->mPosition;
		JGeometry::TVec3<f32> from = actor->mPosition;

		from += dir;
		from -= ropePos;

		if (PSVECMag(&from) > 860.0f) {
			PSVECNormalize(&from, &from);
			from *= 860.0f;
			from += ropePos;
			from -= here;
		}

		dir = from;
	}

	if (!actor->isAirborne()) {
		// While walking, snap the boss onto the graph link it is on.
		TGraphTracer* tracer = self->unk124;
		JGeometry::TVec3<f32> pathDir;
		JGeometry::TVec3<f32> prevDir;
		tracer->getGraph()
		    ->getGraphNode(tracer->getCurGraphIndex())
		    .getPoint(&pathDir);
		tracer->getGraph()
		    ->getGraphNode(tracer->getPrevIndex())
		    .getPoint(&prevDir);

		JGeometry::TVec3<f32> snap
		    = MsPerpendicFootToLineR(prevDir, pathDir, actor->mPosition);

		pathDir -= prevDir;
		snap.sub(actor->mPosition);

		f32 len = PSVECMag(&snap);
		if (len > 10.0f)
			len = 10.0f;
		if (len < 0.0001f) {
			snap.x = 0.0f;
			snap.y = 0.0f;
			snap.z = 0.0f;
		} else {
			PSVECNormalize(&snap, &snap);
			snap *= len;
		}

		dir += snap;
	}

	actor->mLinearVelocity = dir;
}

void TBossWanwanMtxCalc::joinAnm(int index)
{
	M3UMtxCalcSIAnmBlendQuat::joinAnm(
	    mOwner->getActorKeeper()->getMActorAnmData()->getUnk2C()->getAnmPtr(
	        index));
}

void TBossWanwanMtxCalc::calc(u16 index)
{
	// Joint 0 is the root, so it is driven by the live transform info instead
	// of the animation - but only while the boss is off the ground.
	if (index == 0 && mOwner->isAirborne()) {
		// The conversion from TBossWanwanMtxCalc* to J3DMtxCalc* goes through
		// the +0x68 virtual base, which is why the ROM reloads the offset word
		// at +0x00 here instead of storing `this`.
		j3dSys.mCurrentMtxCalc = this;

		J3DTransformInfo info;
		if (mNewAnm) {
			mNewAnm->getTransform(index, &info);
		} else {
			info = j3dSys.getModel()
			           ->getModelData()
			           ->getJointNodePointer(index)
			           ->getTransformInfo();
		}

		// The root joint carries no translation. Note that the three stores
		// the else branch already made into info.mTranslate are dead: they are
		// immediately overwritten here. The ROM keeps them, so do not let the
		// compiler drop them.
		info.mTranslate.x = 0.0f;
		info.mTranslate.y = 0.0f;
		info.mTranslate.z = 0.0f;

		// Virtual: dispatches through the +0x4C subobject vtable.
		M3UMtxCalcSIAnmBlendQuat* calc = this;
		calc->calcTransform(index, info);
	} else {
		M3UMtxCalcSIAnmBlendQuat::calc(index);

		// TODO: the ROM's `index == 1` tail is not written. It zero-fills a
		// 0x30-byte matrix, then looks up a Y rotation built from
		// mOwner->unk168 via the `jmaSinShift` / `jmaSinTable` / `jmaCosTable`
		// globals (no header under include/ or libs/ declares those), and
		// finishes with
		//   PSMTXConcat(mOwner->getModel()->mNodeMatrices[index], rot, same);
		//   PSMTXCopy(same, J3DSys::mCurrentMtx);
		// The angle scale is the float constant 182.04444885253906f
		// (= 65536/360), and f0/f2/f3 are 0.0f/1.0f/0.0f.
	}
}

TBossWanwan::TBossWanwan(const char* name)
    : TSpineEnemy(name)
    , mMtxCalc(nullptr)
    , mLeash(nullptr)
    , unk158(nullptr)
    , unk168(0.0f)
    , unk16C(0)
    , unk17C(0)
    , unk180(0)
    , unk184(0)
    , unk188(0)
    , unk18C(false)
    , unk18D(false)
    , unk190(0)
    , unk194(1)
    , unk195(false)
    , unk198(0)
    , unk19C(0)
    , unk1A0(false)
    , unk1A4()
    , unk1B0(0)
    , unk1B4(0)
{
	mBinder = new TBWBinder;
}

void TBossWanwan::init(TLiveManager* manager)
{
	mManager = manager;
	// The ROM re-reads mManager from the field for both of these instead of
	// reusing the incoming parameter register.
	mManager->manageActor(this);

	mMActorKeeper = new TMActorKeeper(mManager, 0x11);
	mMActor      = mMActorKeeper->createMActor("bwanwan_body.bmd", 0);

	TGraphWeb* graph = gpConductor->getGraphByName("bwanwan");
	graph->initGoalIndex(BW_BATH_POS);
	// TODO: TGraphTracer's graph pointer (its +0x00 word) has no public
	// setter in include/Enemy/Graph.hpp, so write it through the object
	// header. `getGraph()` reads the same word back.
	*reinterpret_cast<TGraphWeb**>(unk124) = graph;

	mSpine->initWith(&TNerveBWGraphWander::theNerve());

	mMarchSpeed = getBWParams()->mSLMarchSpeed.get();
	mTurnSpeed  = getBWParams()->mSLTurnSpeed.get();
	mPosition   = BW_HEAD_START;

	reset();

	mLeash = new TBWLeash(this, 0xF, "ボスワンワン鎖");

	// The four radius/height arguments each re-evaluate the virtual
	// getSaveParam() on the picket's owner, and the compiler never
	// common-subexpressions it, so do not hoist it into a local here.
	TBWPicket* picket = new TBWPicket(this, "ボスワンワンつかみ");
	picket->initHitActor(0x0800000D, 1, 0x80000000,
	                     picket->mOwner->getBWParams()->mSLPicketRadius.get(),
	                     picket->mOwner->getBWParams()->mSLPicketHeight.get(),
	                     picket->mOwner->getBWParams()->mSLPicketRadius.get(),
	                     picket->mOwner->getBWParams()->mSLPicketHeight.get());

	TEnemyNameRefGroup* group
	    = (TEnemyNameRefGroup*)JDrama::TNameRef::search("敵グループ");
	group->mObjects.insert(group->mObjects.end(), picket);
	picket->offHitFlag(HIT_FLAG_NO_COLLISION);
	picket->mPicketMActor
	    = picket->mOwner->mMActorKeeper->createMActor("bwanwan_picket.bmd", 0);

	unk158          = picket;
	picket->mPosition = BW_PICKET_START;
	unk17C            = 1;
	unk184            = 0;
	goToRandomNextGraphNode();

	initHitActor(0x0800000B, 1, 0x80000000, 0.0f, 0.0f, 0.0f, 0.0f);

	unk170[0] = new TBWHit(this, 3, "ボスワンワンヒット");
	unk170[0]->initHitActor(0x0800000B, 3, 0xA0000000, 500.0f, 500.0f, 450.0f,
	                        500.0f);

	unk170[1] = new TBWHit(this, -1, "ボスワンワンヒット");
	unk170[1]->initHitActor(0x0800000B, 3, 0xA0000000, 300.0f, 500.0f, 270.0f,
	                        500.0f);

	TEnemyNameRefGroup* group2
	    = (TEnemyNameRefGroup*)JDrama::TNameRef::search("敵グループ");
	for (int i = 0; i < 2; ++i) {
		group2->mObjects.insert(group2->mObjects.end(), unk170[i]);
		unk170[i]->offHitFlag(HIT_FLAG_NO_COLLISION);
	}

	initAnmSound();
	mScaledBodyRadius = 500.0f;

	mMtxCalc = new TBossWanwanMtxCalc(this);

	mMActor->setCalcForBck(mMtxCalc);

	mMActor->calc();

	// TODO: no name known for this mask; it is a single `rlwinm`, i.e. an
	// "and with a constant" rather than an offLiveFlag() of a named bit.
	mLiveFlag &= 0x01C0000;

	mMtxCalc->joinAnm(4);
	mMActor->setFrameCtrlForBck(4);

	unk178 = 10.0f / mMActor->getFrameCtrl(0)->getEnd();

	setAnmSound(bwanwan_bastable[4]);

	mMActor->setBrkFromIndex(0);
	mMActor->setLightType(1);

	if (!mMActor->getModel()->getSkinDeform())
		mMActor->getModel()->setSkinDeform(new J3DSkinDeform(),
		                                    (J3DDeformAttachFlag)1);

	mHitPoints = getBWParams()->mSLBWHitPointMax.get();
	unk15C.set(0.0f, 0.0f, 0.0f);

	mMapCollisionManager = new TMapCollisionManager(1, "/scene/bwanwan", this);
	mMapCollisionManager->init("bwanwan_ofuro_col.col", 2, nullptr);

	Mtx mtx;
	MsMtxSetTRS(mtx, mPosition.x, mPosition.y, mPosition.z, mRotation.x,
	            mRotation.y, mRotation.z, mScaling.x, mScaling.y, mScaling.z);
	mMapCollisionManager->getUnk8()->setUpMtx(mtx);
	if (mMapCollisionManager->getUnk8())
		mMapCollisionManager->getUnk8()->remove();
}

void TBossWanwan::shakeCamera(int mode)
{
	if (!SMS_IsMarioTouchGround4cm())
		return;

	f32 dist = MsSqrtf(mDistToMarioSquared);

	f32 lengthMax    = getBWParams()->mSLShakeLengthMax.get();
	f32 lengthMaxHP0 = getBWParams()->mSLShakeLengthMaxHP0.get();

	f32 ratio;
	if (getMActor()->checkCurBckFromIndex(0))
		ratio = 1.0f;
	else
		ratio = (f32)mHitPoints / (f32)getBWParams()->mSLBWHitPointMax.get();

	f32 length = lengthMax * ratio + lengthMaxHP0 * (1.0f - ratio);
	f32 rest   = length - dist;
	if (rest < 0.0f)
		return;

	f32 power = rest / length;
	if (power > 1.0f)
		power = 1.0f;

	gpCameraShake->startShake((EnumCamShakeMode)mode, power * ratio);
	SMSRumbleMgr->start(8, &mPosition);
}

BOOL TBossWanwan::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->mActorType == 0x80000001)
		return FALSE;

	if (sender->mActorType == 0x1000001) {
		if (unk18C)
			return TRUE;

		gpMarioParticleManager->emit(0xe7, &sender->mPosition, 0, nullptr);

		if (mHitPoints == 0) {
			if (gpMSound->gateCheck(0x28d1))
				MSoundSESystem::MSoundSE::startSoundActor(0x28d1,
				    &mPosition, 0, nullptr, 0, 4);
		} else if (mHitPoints == 1) {
			gpMarioParticleManager->emitAndBindToMtxPtr(
			    0xb0, getModel()->getAnmMtx(1), 0, nullptr);
			if (gpMSound->gateCheck(0x28c5))
				MSoundSESystem::MSoundSE::startSoundActor(0x28c5,
				    &mPosition, 0, nullptr, 0, 4);
		} else {
			if (gpMSound->gateCheck(0x28be))
				MSoundSESystem::MSoundSE::startSoundActor(0x28be,
				    &mPosition, 0, nullptr, 0, 4);
		}

		if (mHitPoints)
			mHitPoints -= 1;

		unk190 += 1;
		return TRUE;
	}

	// TODO: name these actor types properly
	BOOL inRange;
	if ((u32)(sender->mActorType - 0x40000000) <= 0x5A)
		inRange = 1;
	else
		inRange = 0;
	if (inRange) {
		sender->receiveMessage(this, 1);
		mHitPoints = 0;
		unk190 += 1;
		if (!unk1A0)
			unk1A0 = unk1A0 + 1;

		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0xb0, getModel()->getAnmMtx(1), 0, nullptr);
		if (gpMSound->gateCheck(0x28c5))
			MSoundSESystem::MSoundSE::startSoundActor(0x28c5, &mPosition,
			                                          0, nullptr, 0, 4);
	}

	return TSpineEnemy::receiveMessage(sender, message);
}

void TBossWanwan::changeBck(int index)
{
	mMtxCalc->joinAnm(index);
	getMActor()->setFrameCtrlForBck(index);
	unk178 = 10.0f / getMActor()->getFrameCtrl(0)->getEnd();
	setAnmSound(bwanwan_bastable[index]);
}

void TBossWanwan::calcRootMatrix()
{
	getModel()->setBaseScale(mScaling);
	MsMtxSetXYZRPH(getModel()->getBaseTRMtx(), mPosition.x,
	               500.0f + mPosition.y, mPosition.z, mRotation.x, mRotation.y,
	               mRotation.z);
}

void TBossWanwan::slideToCurPathNode(f32 speed, f32 turn_speed)
{
	JGeometry::TVec3<f32> diff = getUnkF4().getPoint();
	diff -= mPosition;

	f32 dist = PSVECMag(&diff);

	// Same "wrap the direction into [0, 360) degrees then steer towards it"
	// scheme as TBossWanwan::control.
	f32 yaw;
	if (diff.z == 0.0f) {
		if (diff.x >= 0.0f)
			yaw = 90.0f;
		else
			yaw = -90.0f;
	} else if (diff.z >= 0.0f) {
		s16 ang = matan(diff.z, diff.x);
		yaw     = (360.0f / 65536.0f) * (f32)ang;
	} else {
		s16 ang  = matan(-diff.z, diff.x);
		f32 theta = (360.0f / 65536.0f) * (f32)ang;
		yaw       = 180.0f - theta;
	}

	while (yaw >= 360.0f)
		yaw -= 360.0f;
	while (yaw < 0.0f)
		yaw += 360.0f;

	f32 diff2 = yaw - MsWrap(yaw - 180.0f, mRotation.y, 180.0f + yaw);
	if (diff2 > 0.0f) {
		if (diff2 > turn_speed)
			diff2 = turn_speed;
	} else {
		if (diff2 <= -turn_speed)
			diff2 = -turn_speed;
	}

	f32 target = mRotation.y + diff2;
	while (target >= 360.0f)
		target -= 360.0f;
	while (target < 0.0f)
		target += 360.0f;
	mRotation.y = target;

	JGeometry::TVec3<f32> velocity = mLinearVelocity;
	if (dist > 0.0f) {
		f32 ratio = speed / dist;
		diff *= ratio;

		velocity += diff;
		mLinearVelocity = velocity;
	}
}

void TBossWanwan::control()
{
	TLiveActor::control();

	if (unk17C != 0 || unk158->isTaken()) {
		TBWParams* params = (TBWParams*)getSaveParam();
		f32 sq            = unk15C.squared();
		// TODO: the ROM materialises this into a `bge`-based bool here; ours
		// comes out as `cror eq,gt,eq / bne`. Spelling is not yet found.
		BOOL far;
		if (sq >= params->mSLPullLimit.get())
			far = 1;
		else
			far = 0;
		if (far) {
			mLinearVelocity.x += unk15C.x;
			mLinearVelocity.y += unk15C.y;
			mLinearVelocity.z += unk15C.z;

			JGeometry::TVec3<f32> diff = mPosition;
			// TODO: index 3 (and unkC) is read off the ROM's +0x90 displacement;
			// the leash end node is the most likely intent, but it is unverified.
			diff -= mLeash->mRope->mPoints[3].unkC;

			f32 yaw;
			if (diff.z == 0.0f) {
				if (diff.x >= 0.0f)
					yaw = 90.0f;
				else
					yaw = -90.0f;
			} else if (diff.z >= 0.0f) {
				s16 ang = matan(diff.z, diff.x);
				yaw     = (360.0f / 65536.0f) * (f32)ang;
			} else {
				s16 ang  = matan(-diff.z, diff.x);
				f32 theta = (360.0f / 65536.0f) * (f32)ang;
				yaw       = 180.0f - theta;
			}

			while (yaw >= 360.0f)
				yaw -= 360.0f;
			while (yaw < 0.0f)
				yaw += 360.0f;

			f32 rot    = mRotation.y;
			f32 diff2  = yaw - MsWrap(yaw - 180.0f, rot, 180.0f + yaw);
			if (diff2 > 0.0f)
				diff2 = MsMin(diff2, 4.0f * mTurnSpeed);
			else
				diff2 = MsMax(diff2, -mTurnSpeed * 4.0f);

			f32 target = rot + diff2;
			while (target >= 360.0f)
				target -= 360.0f;
			while (target < 0.0f)
				target += 360.0f;
			mRotation.y = target;
		}
	}

	unk15C.zero();
	updateSquareToMario();
}

void TBossWanwan::emitEffects()
{
	int flag = 0;
	if (getMActor()->checkCurBckFromIndex(4)
	    || getMActor()->checkCurBckFromIndex(5)) {
		if (getMActor()->checkBckPass(50.0f))
			flag = 1;
	} else if (getMActor()->checkCurBckFromIndex(2)) {
		if (getMActor()->checkBckPass(200.0f))
			flag = 1;
	}

	if (flag) {
		gpMarioParticleManager->emit(0xad, &mPosition, 0, nullptr);
		gpMarioParticleManager->emit(0xae, &mPosition, 0, nullptr);

		if (mHitPoints == 0) {
			const Vec* pos = &mLeash->mRope->mPoints[0].unkC;
			if (gpMSound->gateCheck(0x2975))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x2975, pos, 0, nullptr, 0, 4);
			pos = &unk158->mPosition;
			if (gpMSound->gateCheck(0x2976))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x2976, pos, 0, nullptr, 0, 4);
		} else {
			const Vec* pos = &mLeash->mRope->mPoints[0].unkC;
			if (gpMSound->gateCheck(0x2973))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x2973, pos, 0, nullptr, 0, 4);
			pos = &unk158->mPosition;
			if (gpMSound->gateCheck(0x2974))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x2974, pos, 0, nullptr, 0, 4);
		}
	}

	int flag2 = 0;
	if (getMActor()->checkCurBckFromIndex(0)) {
		if (getMActor()->checkBckPass(0.0f))
			flag2 = 1;
	} else if (getMActor()->checkCurBckFromIndex(4)
	           || getMActor()->checkCurBckFromIndex(5)) {
		if (getMActor()->checkBckPass(8.0f) || getMActor()->checkBckPass(48.0f))
			flag2 = 1;
	} else if (getMActor()->checkCurBckFromIndex(2)) {
		if (getMActor()->checkBckPass(72.0f))
			flag2 = 1;
	}

	if (flag2) {
		MtxPtr mtx = getModel()->getAnmMtx(1);
		gpMarioParticleManager->emitAndBindToMtxPtr(0xaf, mtx, 0, this);
	}

	if (getMActor()->checkCurBckFromIndex(4)
	    || getMActor()->checkCurBckFromIndex(5)) {
		if (getMActor()->checkBckPass(1.0f))
			shakeCamera(0x16);
	}

	if (getMActor()->checkCurBckFromIndex(0)) {
		J3DFrameCtrl* ctrl = getMActor()->getFrameCtrl(0);
		if (ctrl->checkPass(6.0f) || ctrl->checkPass(12.0f)) {
			shakeCamera(0x16);
			gpMarioParticleManager->emit(0xad, &mPosition, 0, nullptr);
			gpMarioParticleManager->emit(0xae, &mPosition, 0, nullptr);
		}
		if (ctrl->checkPass(4.0f)) {
			shakeCamera(0x17);
			gpMarioParticleManager->emit(0xad, &mPosition, 0, nullptr);
			gpMarioParticleManager->emit(0xae, &mPosition, 0, nullptr);
		}
	}

	if (getMActor()->checkCurBckFromIndex(2)) {
		if (getMActor()->checkBckPass(0.6f))
			shakeCamera(0x16);
	}

	if (mHitPoints != 0) {
		MtxPtr mtx = getModel()->getAnmMtx(1);
		gpMarioParticleManager->emitAndBindToMtxPtr(0x1ee, mtx, 3, this);
	}

	if (unk190 != 0 && mHitPoints != 0) {
		MtxPtr mtx = getModel()->getAnmMtx(1);
		gpMarioParticleManager->emitAndBindToMtxPtr(0x167, mtx, 1, this);
		unk190 = 0;
	}
}

void TBossWanwan::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (unk18C != 0) {
		unk170[0]->perform(cue, graphics);
		TSpineEnemy::perform(cue, graphics);

		if (cue & CUE_MOVE)
			mMtxCalc->advanceMotionBlend(-unk178);

		if (cue & CUE_ENTRY) {
			J3DFrameCtrl* ctrl = getMActor()->getFrameCtrl(5);
			if (ctrl != nullptr
			    && ctrl->getFrame() > 0.5f * (f32)ctrl->getEnd()) {
				unk1A4   = mPosition;
				unk1A4.y += 500.0f;
				gpMarioParticleManager->emitAndBindToPosPtr(
				    0x168, &unk1A4, 1, this);
			}
		}
	} else {
		if (cue & CUE_CALC_ANIM) {
			J3DFrameCtrl* ctrl = getMActor()->getFrameCtrl(5);
			f32 temperature
			    = (f32)mHitPoints
			      / (f32)getBWParams()->mSLBWHitPointMax.get();
			ctrl->setFrame(temperature * (ctrl->getEnd() - 1));
			ctrl->setRate(0.0f);
			if (mHitPoints == 0
			    || mHitPoints == getBWParams()->mSLBWHitPointMax.get()) {
				mLeash->mNodes[0]->mTemperature = temperature;
			}
			emitEffects();
		}

		unk170[0]->perform(cue, graphics);

		if (cue & CUE_MOVE) {
			unk170[1]->mPosition.set(unk170[0]->mPosition.x,
			                         unk170[0]->mPosition.y - 500.0f,
			                         unk170[0]->mPosition.z);
		}

		unk170[1]->perform(cue, graphics);

		if (cue & CUE_MOVE) {
			if (unk194 == 0) {
				unk19C++;
				if (unk19C < 0)
					unk19C = 1;
				if (unk19C > 0x258) {
					if (!(unk198 & 1)) {
						gpMarDirector->getConsole()->startAppearBalloon(
						    0x1a, true);
					}
					unk198 |= 1;
				}
			}

			if (unk1A0 == 0 && gpMarDirector->mMoveTickCount >= 0x7080) {
				if (!(unk198 & 8)) {
					gpMarDirector->getConsole()->startAppearBalloon(0x1d,
					                                               true);
				}
				unk198 |= 8;
			}
		}

		if (cue & CUE_MOVE) {
			if (!mSpine->isNerve(&TNerveBWDie::theNerve())
			    && !mSpine->isNerve(&TNerveBWJumpToBath::theNerve())) {
				if (mHitPoints != 0) {
					if (unk17C != 0 && unk194 == 0) {
						unk184++;
						if (unk184 > 0x258) {
							mSpine->pushNerve(
							    &TNerveBWShake::theNerve());
							unk184 = 0;
						}
					}
				} else {
					unk184 = 0;
				}

				if (mHitPoints == 0
				    && !mSpine->isNerve(&TNerveBWBark::theNerve())) {
					unk180++;
					if (unk180 > 0x960) {
						if (!mSpine->isNerve(
						        &TNerveBWBark::theNerve())) {
							mSpine->setNext(
							    &TNerveBWBark::theNerve());
						}
					}
				} else {
					if (gpMarDirector->mMoveTickCount % 20 == 0) {
						if (mHitPoints
						    < getBWParams()
						          ->mSLBWHitPointMax
						          .get()) {
							mHitPoints++;
						}
					}
					unk180 = 0;
				}
			}

			if (mSpine->isNerve(&TNerveBWGraphWander::theNerve())
			    && unk158->isTaken()) {
				f32 length     = unk15C.length();
				const Vec* pos = &mLeash->mRope->mPoints[6].unkC;
				if (gpMSound->gateCheck(0x20d2)) {
					MSoundSESystem::MSoundSE::startSoundActorWithInfo(
					    0x20d2, pos, nullptr, length, 0, 0, nullptr, 0, 4);
				}
			}
		}
	}

	TSpineEnemy::perform(cue, graphics);
	mLeash->testPerform(cue, graphics);
	unk158->testPerform(cue, graphics);

	if (cue & CUE_MOVE)
		mMtxCalc->advanceMotionBlend(-unk178);
}

TBossWanwanManager::TBossWanwanManager(const char* name)
    : TEnemyManager(name)
{
}

void TBossWanwanManager::initJParticle()
{
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_jump_rock.jpa", 0xad);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_jump_smoke.jpa", 0xae);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_downyuge.jpa", 0xb0);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_hibana.jpa", 0xaf);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_deadyuge.jpa", 0xb1);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_yugami.jpa", 0x1ee);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_hityuge.jpa", 0x167);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_kira.jpa", 0x168);
}

TSpineEnemy* TBossWanwanManager::createEnemyInstance()
{
	return new TBossWanwan("ボスワンワン");
}

void TBossWanwanManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "bwanwan_body.bmd", 0x10220000, 0 },
		{ "bwanwan_chain.bmd", 0x10220000, 0 },
		{ "bwanwan_picket.bmd", 0x10220000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TBossWanwanManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TBWParams("/enemy/bosswanwan.prm");
	TEnemyManager::load(stream);

	initJParticle();
}

DEFINE_NERVE(TNerveBWGraphWander, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		self->unk16C = 0;

		// The ROM tests bck 4 twice in a row here; kept verbatim.
		if (!self->getMActor()->checkCurBckFromIndex(4)
		    && !self->getMActor()->checkCurBckFromIndex(4)) {
			if (self->mHitPoints == 0)
				self->changeBck(5);
			else
				self->changeBck(4);

			J3DFrameCtrl* ctrl = self->getMActor()->getFrameCtrl(0);
			ctrl->setFrame(0.0f);
			ctrl->setRate(SMSGetAnmFrameRate());
		}

		self->getMActor()->setBtpFromIndex(0);
		J3DFrameCtrl* btpCtrl = self->getMActor()->getFrameCtrl(3);
		btpCtrl->setFrame(0.0f);
		btpCtrl->setRate(0.0f);
	}

	if (self->getMActor()->curAnmEndsNext(0, nullptr) && self->mHitPoints == 0
	    && self->getMActor()->checkCurBckFromIndex(5)) {
		self->changeBck(5);
		J3DFrameCtrl* ctrl = self->getMActor()->getFrameCtrl(0);
		ctrl->setFrame(0.0f);
		ctrl->setRate(SMSGetAnmFrameRate());
	}

	// Being dragged by the picket: steer towards Mario instead of the graph.
	if (self->unk17C == 0 && self->unk158->isTaken()) {
		TBWParams* params = (TBWParams*)self->getSaveParam();
		f32 sq            = self->unk15C.squared();
		BOOL inRange;
		if (sq >= params->mSLPullLimit.get())
			inRange = 1;
		else
			inRange = 0;
		if (inRange) {
			TGraphTracer* tracer = self->getTracer();
			int prev             = tracer->getPrevIndex();

			JGeometry::TVec3<f32> point;
			tracer->getGraph()->getGraphNode(prev).getPoint(&point);
			point -= self->mPosition;

			if (PSVECMag(&point) < 400.0f
			    && prev == tracer->getGraph()->unk10) {
				spine->pushAfterCurrent(&TNerveBWJumpToBath::theNerve());
				return TRUE;
			}

			if (PSVECMag(&point) < 400.0f) {
				JGeometry::TVec3<f32> marioPos = *gpMarioPos;
				marioPos -= self->mPosition;

				tracer->mPrevIdx = tracer->getGraph()->getAimToDirNextIndex(
				    prev, tracer->getCurGraphIndex(), marioPos, self->mPosition,
				    -1);
				tracer->mCurrIdx = prev;
				self->setGoalPathFromGraph();
				self->unk128 = 0;
				self->unk12C = 0.0f;
			}
		}

		// March speed scales with how healthy the boss is.
		f64 dHitPoints, dMaxHitPoints;
		*(u32*)&dHitPoints = (u32)self->mHitPoints;
		*(u32*)&dMaxHitPoints
		    = (u32)self->getBWParams()->mSLBWHitPointMax.get();
		f32 health = ((f32)dHitPoints - (f32)4503599627370496.0)
		           / ((f32)dMaxHitPoints - (f32)4503599627370496.0);

		self->slideToCurPathNode(3.0f * (health * self->getMarchSpeed()),
		                         self->getTurnSpeed());
		return FALSE;
	}

	if (self->isReachedToGoal()) {
		// The ROM pushes the wander nerve on *both* sides of this test and
		// only adds the jump nerve on the "found a node" side; kept verbatim.
		if (self->jumpToNextGraphNode() >= 0) {
			spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
			spine->pushAfterCurrent(&TNerveBWJump::theNerve());
		} else {
			spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		}

		TGraphTracer* tracer = self->getTracer();
		int prev             = tracer->getPrevIndex();

		if (prev >= 0
		    && tracer->getGraph()->getGraphNode(prev).unkC >= 2
		    && gpMarDirector->mMoveTickCount >= 0x3840) {
			if ((self->unk198 & 4) == 0)
				gpMarDirector->getConsole()->startAppearBalloon(0x1c, true);
			self->unk198 |= 4;
		}

		// Pick a neighbouring node to flee to, biased away from the way we
		// are currently facing.
		s16 idx = (s16)(65536.0f / 360.0f * self->mRotation.y);
		JGeometry::TVec3<f32> facing;
		// The redundant 1.0f factors are what the ROM emits.
		facing.set(1.0f * JMASSin(idx), 0.0f, 1.0f * JMASCos(idx));

		tracer->moveTo(tracer->getGraph()->getEscapeDirLimited(
		    tracer->getCurGraphIndex(), prev, facing, self->mPosition, 100.0f,
		    0xffffffff));
		self->setGoalPathFromGraph();
		self->unk128 = 0;
		self->unk12C = 0.0f;
		return TRUE;
	}

	self->unk16C = 0;

	f64 dHitPoints, dMaxHitPoints;
	*(u32*)&dMaxHitPoints
	    = (u32)self->getBWParams()->mSLBWHitPointMax.get();
	*(u32*)&dHitPoints = (u32)self->mHitPoints;
	f32 health = ((f32)dMaxHitPoints - (f32)4503599627370496.0)
	           / ((f32)dHitPoints - (f32)4503599627370496.0);

	if (self->unk158->isTaken()) {
		// Look towards where the boss is heading, biased by how hard Mario
		// is pulling on the cap.
		f64 biased;
		*((u32*)&biased + 1) = 0x43300000;
		*(u32*)&biased = (u32)(gpMarioOriginal->mIntendedYaw ^ 0x8000);

		s16 idx = (s16)((f32)(biased - 4503601774854144.0)
		                * (360.0f / 65536.0f) * (65536.0f / 360.0f));

		JGeometry::TVec3<f32> facing;
		// The redundant 1.0f factors are what the ROM emits.
		facing.set(1.0f * JMASSin(idx), 0.0f, 1.0f * JMASCos(idx));

		JGeometry::TVec3<f32> toMario = self->mPosition;
		toMario.sub(gpMarioOriginal->mPosition);
		PSVECNormalize(&toMario, &toMario);

		f32 pull = (f32)(0.75 * (f64)(gpMarioOriginal->mIntendedMag * 0.03125f));
		f32 turn = 1.0f - pull * -toMario.dot(facing);
		if (turn < 0.0f)
			turn = 0.0f;
		else if (turn > 1.5f)
			turn = 1.5f;

		self->slideToCurPathNode(turn * (health * self->getMarchSpeed()),
		                         self->getTurnSpeed());
	} else {
		self->slideToCurPathNode(health * self->getMarchSpeed() + 0.2f,
		                         self->getTurnSpeed());
	}

	return FALSE;
}

DEFINE_NERVE(TNerveBWRoll, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		J3DFrameCtrl* ctrl = self->getMActor()->getFrameCtrl(0);
		ctrl->setFrame(0.0f);
		ctrl->setRate(0.0f);
		self->unk16C = 1;
	}

	if (self->isReachedToGoal()) {
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		J3DFrameCtrl* ctrl = self->getMActor()->getFrameCtrl(0);
		ctrl->setRate(SMSGetAnmFrameRate());
		return TRUE;
	}

	f32 speed = self->getBWParams()->mSLAttackSpeed.get();
	self->walkToCurPathNode(speed, self->getTurnSpeed(), 0.0f);
	return FALSE;
}

DEFINE_NERVE(TNerveBWBark, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		self->changeBck(0);
		self->unk16C = 0;
		self->unk168 = 0.0f;
		if (self->unk194 == 0) {
			if (self->unk17C) {
				JPABaseEmitter* emitter = gpMarioParticleManager->emit(
				    0xae, &self->unk158->mPosition, 0, nullptr);
				if (emitter)
					emitter->setGlobalScale(
					    JGeometry::TVec3<f32>(0.3f, 0.5f, 0.3f));
			}
			self->unk194   = 0;
			self->unk17C   = 0;
			const Vec* pos = &self->unk158->mPosition;
			if (gpMSound->gateCheck(0x2966))
				MSoundSESystem::MSoundSE::startSoundActor(0x2966, pos, 0,
				                                          nullptr, 0, 4);
		}
	}

	if (spine->getTime() == 0x118)
		self->mHitPoints = self->getBWParams()->mSLBWHitPointMax.get();

	if (self->getMActor()->curAnmEndsNext(0, nullptr)) {
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		if ((self->unk198 & 2) == 0)
			gpMarDirector->getConsole()->startAppearBalloon(0x1b, true);
		self->unk198 |= 2;
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBWJump, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		const JGeometry::TVec3<f32>& goal = self->getUnk104().getPoint();
		f32 speed                         = self->unk124->unkC;
		self->mVelocity
		    = self->calcVelocityToJumpToY(goal, speed, self->getGravityY());
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->unk16C = 0;
	}

	if (self->isReachedToGoal()) {
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		return TRUE;
	}

	self->walkToCurPathNode(0.0f, self->getTurnSpeed(), 0.0f);
	return FALSE;
}

DEFINE_NERVE(TNerveBWStun, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	BOOL taken = self->unk158->isTaken();
	if (taken) {
		TBWParams* params = (TBWParams*)self->getSaveParam();
		f32 sq            = self->unk15C.squared();
		BOOL inRange;
		if (sq >= params->mSLPullLimit.get())
			inRange = 1;
		else
			inRange = 0;
		if (inRange) {
			TGraphTracer* tracer = self->getTracer();
			TGraphWeb* graph     = tracer->getGraph();
			int prev             = tracer->getPrevIndex();

			JGeometry::TVec3<f32> point;
			graph->getGraphNode(prev).getPoint(&point);
			point -= self->mPosition;

			if (PSVECMag(&point) < 5.0f) {
				if (prev == graph->unk10) {
					spine->pushAfterCurrent(&TNerveBWJumpToBath::theNerve());
					return TRUE;
				}

				JGeometry::TVec3<f32> marioPos = *gpMarioPos;
				marioPos -= self->mPosition;

				tracer->mPrevIdx = graph->getAimToDirNextIndex(
				    prev, tracer->mCurrIdx, marioPos, self->mPosition, -1);
				tracer->mCurrIdx = prev;
				self->setGoalPathFromGraph();
				self->unk128 = 0;
				self->unk12C = 100.0f;
			}
		}
		return FALSE;
	}

	TBWParams* params = (TBWParams*)self->getSaveParam();
	if (spine->getTime() > params->mSLStunTimer.get()) {
		self->mHitPoints = params->mSLBWHitPointMax.get();
		spine->pushAfterCurrent(&TNerveBWWakeup::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBWWakeup, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		self->changeBck(6);
		self->getMActor()->setBtpFromIndex(2);
		self->unk16C = 0;
		self->unk168 = 0.0f;
	}

	if (self->getMActor()->curAnmEndsNext(0, nullptr)) {
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBWJumpToBath, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		JGeometry::TVec3<f32> velocity = self->calcVelocityToJumpToY(
		    BW_BATH_POS, 10.0f, self->getGravityY());
		self->setGoalPath(TPathNode(BW_BATH_POS));
		self->mVelocity = velocity;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->unk16C = 0;
	}

	// TODO: unk195 is a "the water column effect has already been spawned"
	// latch; the name is a guess from the single use here.
	if (spine->getTime() > 0x78 && !self->unk195
	    && self->mPosition.y < 10.0f + BW_BATH_POS.y) {
		TEffectColumWater* enemy
		    = (TEffectColumWater*)gpConductor->makeOneEnemyAppear(
		        self->mPosition, "エフェクト水柱マネージャー", 1);
		if (enemy) {
			JGeometry::TVec3<f32> scale(5.0f, 5.0f, 5.0f);
			JGeometry::TVec3<f32> pos;
			pos.set(self->mPosition.x,
			        500.0f + self->mPosition.y - 30.0f, self->mPosition.z);
			enemy->generate(pos, scale);
			self->unk195 = 1;
		}

		if (gpMSound->gateCheck(0x2917))
			MSoundSESystem::MSoundSE::startSoundActor(0x2917, &self->mPosition,
			                                          0, nullptr, 0, 4);

		JGeometry::TVec3<f32> bath = BW_BATH_POS;
		JGeometry::TVec3<f32> diff = bath;
		diff -= self->mPosition;

		if (diff.squared() < 10000.0f) {
			if (self->mPosition.y < BW_BATH_POS.y) {
				self->mPosition = bath;
				spine->pushAfterCurrent(&TNerveBWDie::theNerve());
				return TRUE;
			}
		}
	}

	self->walkToCurPathNode(0.0f, self->getTurnSpeed(), 0.0f);
	return FALSE;
}

DEFINE_NERVE(TNerveBWDie, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		JGeometry::TVec3<f32> zero;
		zero.zero();
		self->mVelocity       = zero;
		self->mLinearVelocity = zero;
		self->onLiveFlag(LIVE_FLAG_UNK10);
		// TODO: the ROM masks mLiveFlag down to these three bits with a single
		// rlwinm; it is most likely a run of offLiveFlag() calls that MWCC
		// merged, but the individual flags are not recoverable from here.
		self->mLiveFlag &= 0x07000000;
		gpMarioParticleManager->emit(0xb1, &self->mPosition, 0, nullptr);
	}

	if (self->mHitPoints != 0) {
		self->mHitPoints -= 1;
		self->getMActor()->getFrameCtrl(0)->setFrame(0.0f);
		self->getMActor()->getFrameCtrl(0)->setRate(0.0f);
		spine->pushAfterCurrent(&TNerveBWDie::theNerve());
		return TRUE;
	}

	if (spine->getTime() == 0) {
		gpMarDirector->fireStartDemoCamera("bwanwan_down_camera.", nullptr, -1,
		                                   0.0f, true, nullptr, 0,
		                                   nullptr, JDrama::TFlagT<u16>(0));
		self->unk16C = 0;
		self->unk168 = 0.0f;
		self->unk18C = 1;
		self->mPosition = BW_BATH_POS;

		JGeometry::TVec3<f32> pos = self->mPosition;
		pos.y += 500.0f;
		JGeometry::TVec3<f32> scale = self->mScaling;
		scale *= 1.1f;

		Mtx mtx;
		MsMtxSetTRS(mtx, pos.x, pos.y, pos.z, self->mRotation.x,
		            self->mRotation.y, self->mRotation.z, scale.x, scale.y,
		            scale.z);
		TMapCollisionBase* collision = self->mMapCollisionManager->getUnk8();
		collision->setMtx(mtx);
		collision->setUp();

		self->unk170[0]->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->unk170[1]->onHitFlag(HIT_FLAG_NO_COLLISION);
		TRope* rope = self->mLeash->mRope;
		for (int i = 0; i < rope->mNumPoints; ++i)
			self->mLeash->mNodes[i]->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->unk158->onHitFlag(HIT_FLAG_NO_COLLISION);

		self->changeBck(1);
		self->getMActor()->setBtpFromIndex(0);
		self->getMActor()->setBrkFromIndex(1);
	}

	if (spine->getTime() > 0x3c && !self->unk18D
	    && gpMarDirector->unk124 != 3) {
		gpItemManager->makeShineAppearWithDemo("シャイン（ボス用）",
		                                       "ボスシャインカメラ",
		                                       self->mPosition.x,
		                                       self->mPosition.y,
		                                       self->mPosition.z);
		self->unk18D = 1;
	}

	if (self->getMActor()->curAnmEndsNext(5, nullptr))
		self->getMActor()->getFrameCtrl(5)->setRate(0.0f);

	return FALSE;
}

DEFINE_NERVE(TNerveBWJumpAway, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		JGeometry::TVec3<f32> velocity = self->calcVelocityToJumpToY(
		    BW_HEAD_START, 40.0f, self->getGravityY());
		self->setGoalPath(TPathNode(BW_HEAD_START));
		self->mVelocity = velocity;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->unk16C = 0;

		// TODO: unk188 gates the hand-off to the fall nerve; its meaning is
		// still unknown (set from TBossWanwan::init, cleared nowhere here).
		if (self->unk188 != 0) {
			spine->pushAfterCurrent(&TNerveBWFall::theNerve());
			return TRUE;
		}
	}

	if (self->isReachedToGoal()) {
		self->mPosition = BW_HEAD_START;
		self->unk124->reset();
		self->unk124->reset2();
		self->goToShortestNextGraphNode();
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		return TRUE;
	}

	self->walkToCurPathNode(0.0f, self->getTurnSpeed(), 0.0f);
	return FALSE;
}

DEFINE_NERVE(TNerveBWShake, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();
	MActor* actor     = self->getMActor();

	if (spine->getTime() == 0)
		self->changeBck(2);

	if (actor->curAnmEndsNext(0, nullptr)) {
		if (self->unk17C) {
			JPABaseEmitter* emitter = gpMarioParticleManager->emit(
			    0xae, &self->unk158->mPosition, 0, nullptr);
			if (emitter)
				emitter->setGlobalScale(
				    JGeometry::TVec3<f32>(0.3f, 0.5f, 0.3f));
		}
		self->unk194 = false;
		self->unk17C = 0;
		const Vec* pos = &self->unk158->mPosition;
		if (gpMSound->gateCheck(0x2967))
			MSoundSESystem::MSoundSE::startSoundActor(0x2967, pos, 0, nullptr,
			                                          0, 4);
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBWFall, TLiveActor)
{
	TBossWanwan* self = (TBossWanwan*)spine->getBody();

	if (spine->getTime() == 0) {
		const JGeometry::TVec3<f32>& target = self->unk158->mPosition;
		JGeometry::TVec3<f32> velocity
		    = self->calcVelocityToJumpToY(target, 5.0f, self->getGravityY());
		self->setGoalPath(TPathNode(self->unk158->mPosition));
		self->mVelocity = velocity;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->unk16C = 0;
	}

	if (self->isReachedToGoal()) {
		self->mPosition = self->unk158->mPosition;
		self->unk124->reset();
		self->unk124->reset2();
		self->goToShortestNextGraphNode();
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		return TRUE;
	}
	return FALSE;
}
