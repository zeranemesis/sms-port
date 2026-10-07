// Definitions follow the map's .text layout reversed, this TU being
// -inline deferred: all of TMapObjBall from touchRoof down to its
// constructor, then TResetFruit from checkGroundCollision down to its
// constructor (the four UNUSED bodies pick/living/waitEffect/rotting sit
// between makeObjLiving and breaking, where the map's symbol closure puts
// them), then TRandomFruit, TCoverFruit and TBigWatermelon from
// touchWaterSurface down to its constructor. Do not resort them by class or
// by hand; validate-symbol-order.py checks this.
#include <MoveBG/MapObjBall.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <Player/ModelWaterManager.hpp>
#include <MoveBG/ItemManager.hpp>
#include <Camera/CubeManagerBase.hpp>
#include <Enemy/PoiHana.hpp>
#include <MoveBG/Item.hpp>
#include <JSystem/JGeometry.hpp>
#include <MarioUtil/MapUtil.hpp>
#include <string.h>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <stdio.h>
#include <string.h>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/PollutionManager.hpp>
#include <Player/MarioAccess.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

void TMapObjBall::touchRoof(JGeometry::TVec3<f32>* param_1)
{
	if (param_1->y > unk140)
		param_1->y = unk140;

	calcReflectingVelocity(unk13C, mMapObjData->mPhysical->unk4->unk4,
	                       &mVelocity);
}

void TMapObjBall::touchWall(JGeometry::TVec3<f32>* param_1,
                            TBGWallCheckRecord* param_2)
{
	// Hitting a wall while rolling on the ground pops the ball up a little,
	// scaled by how fast it was going. The watermelon is too heavy for that.
	if (!isAirborne()) {
		if (!isActorType(0x400000D0)) {
			mVelocity.y
			    += unk184 * JGeometry::TVec3<f32>(mVelocity).length();
		}
	}

	for (int i = 0; i < param_2->mResultWallsNum; ++i) {
		const TBGCheckData* wall = param_2->mResultWalls[i];

		JGeometry::TVec3<f32> vel(mVelocity);
		f32 into = vel.dot(wall->getNormal());
		if (into < 0.0f) {
			// Push the ball back out to exactly one radius from the plane.
			// Both products are TVec3::dot() (c-k15: the written-out sums let
			// MWCC reuse the normal's loads, 98.0 -> 99.8).
			// TODO: every instruction and register matches; the frame is
			// 0xf0 against 0x120 (the velocity copies sit 0x1c-0x38 low).
			f32 dist = param_1->dot(wall->getNormal()) + wall->getPlaneDistance();
			param_1->x += (mBodyRadius - dist) * wall->getNormal().x;
			param_1->z += (mBodyRadius - dist) * wall->getNormal().z;

			f32 bounce = into * -(1.0f + mMapObjData->getPhysicalData()->unk8);
			mVelocity.x += bounce * wall->getNormal().x;
			mVelocity.z += bounce * wall->getNormal().z;

			if (isActorType(0x400000D0)) {
				if (mScaling.y >= 5.0f) {
					SMSGetMSound()->startSoundActorWithInfo(
					    MSD_SE_OBJ_WATERMELON_BROLL, &mPosition, nullptr,
					    abs(JGeometry::TVec3<f32>(mVelocity).length()), 0, 0,
					    nullptr, 0, 4);
				} else {
					SMSGetMSound()->startSoundActorWithInfo(
					    MSD_SE_OBJ_WATERMELON_SROLL, &mPosition, nullptr,
					    abs(JGeometry::TVec3<f32>(mVelocity).length()), 0, 0,
					    nullptr, 0, 4);
				}
			} else {
				u32 sound = mMapObjData->mSound->unk4->unk0[4];
				SMSGetMSound()->startSoundActorWithInfo(
				    sound, &mPosition, (Vec*)&mVelocity, 0.0f, 0, 0, nullptr,
				    0, 4);
			}
		}
	}
}

void TMapObjBall::touchPollution() { kill(); }

void TMapObjBall::touchWaterSurface() { kill(); }

// Binding level over the sound singleton, sized inside the body that
// TBigWatermelon::rebound pastes as well.
static inline MSound* MapObjBallBounceSound()
{
	MSound* sound = SMSGetMSound();
	return sound;
}

void TMapObjBall::rebound(JGeometry::TVec3<f32>* param_1)
{
	calcReflectingVelocity(mGroundPlane, mMapObjData->mPhysical->unk4->unk4,
	                       &mVelocity);
	param_1->y = mGroundHeight;
	onLiveFlag(LIVE_FLAG_AIRBORNE);

	if (isActorType(0x400000D0)) {
		// The watermelon has a big and a small bounce sample, chosen by how
		// far it has been scaled up.
		if (mScaling.y >= 5.0f) {
			MapObjBallBounceSound()->startSoundActorWithInfo(
			    MSD_SE_OBJ_WATERMELON_BBUND, &mPosition, nullptr,
			    abs(getGroundPlane()->mNormal.y), 0, 0, nullptr, 0, 4);
		} else {
			MapObjBallBounceSound()->startSoundActorWithInfo(
			    MSD_SE_OBJ_WATERMELON_SBUND, &mPosition, nullptr,
			    abs(getGroundPlane()->mNormal.y), 0, 0, nullptr, 0, 4);
		}
	} else {
		u32 sound = mMapObjData->mSound->unk4->unk0[4];
		MapObjBallBounceSound()->startSoundActorWithInfo(sound, &mPosition,
		                                        (Vec*)&mVelocity, 0.0f, 0, 0,
		                                        nullptr, 0, 4);
	}
}

void TMapObjBall::touchGround(JGeometry::TVec3<f32>* param_1)
{
	f32 speed = abs(JGeometry::TVec3<f32>(getVelocity()).length());
	if (speed > 0.05f) {
		if (isActorType(0x400000D0)) {
			// Big and small rolling samples, same split as rebound().
			if (mScaling.y >= 5.0f) {
				SMSGetMSound()->startSoundActorWithInfo(
				    MSD_SE_OBJ_WATERMELON_BROLL, &mPosition, nullptr, speed, 0,
				    0, nullptr, 0, 4);
			} else {
				SMSGetMSound()->startSoundActorWithInfo(
				    MSD_SE_OBJ_WATERMELON_SROLL, &mPosition, nullptr, speed, 0,
				    0, nullptr, 0, 4);
			}
		}
	}

	if (mGroundPlane->isWaterSurface()) {
		touchWaterSurface();
		param_1->set(mPosition);
		return;
	}

	if (gpPollution->isPolluted(param_1->x, param_1->y, param_1->z)) {
		touchPollution();
		param_1->set(mPosition);
		return;
	}

	// A slow enough impact settles instead of bouncing.
	if (mVelocity.y > -unk188) {
		offLiveFlag(LIVE_FLAG_AIRBORNE);
		mVelocity.y = 0.0f;
		param_1->y  = getGroundHeight();
	} else {
		rebound(param_1);
	}

	// Rolling downhill: the ground normal drags the ball along.
	if (!isAirborne()) {
		mVelocity.x += unk180 * getGroundPlane()->getNormal().x;
		mVelocity.z += unk180 * getGroundPlane()->getNormal().z;
	}

	mVelocity.x *= getMapObjData()->mPhysical->unk4->unk10;
	mVelocity.z *= getMapObjData()->mPhysical->unk4->unk10;
}

void TMapObjBall::put()
{
	TMapObjGeneral::put();
	calcCurrentMtx();
}

// Consumed const-ref binding under the unnamed TVec3 copy: +4 of pool
// so the copy sits at retail's 0x20 and the frame stays 0x38, while the
// unnamed temporary still keeps TUtil<f32>::sqrt out of line.
static inline f32 MapObjBallHoldSpeed(const JGeometry::TVec3<f32>& vel)
{
	return JGeometry::TVec3<f32>(vel).length();
}

void TMapObjBall::hold(TTakeActor* param_1)
{
	// A ball still moving fast cannot be picked up. The unnamed temporary
	// is what keeps JGeometry::TUtil<f32>::sqrt out of line, as the ROM has
	// it (weak from boid.cpp): a named copy puts sqrt one level shallower
	// and expands it.
	if (MapObjBallHoldSpeed(mVelocity) > 10.0f)
		return;

	TMapObjGeneral::hold(param_1);
	mVelocity.zero();
}

void TMapObjBall::kicked()
{
	// Only a downward or level kick does anything.
	if (JGeometry::TVec3<f32>(mVelocity).y > 0.0f)
		return;

	if (JGeometry::TVec3<f32>(mVelocity).y == 0.0f) {
		mVelocity.y = unk178;
	} else {
		mVelocity.y = unk174 * SMS_GetMarioSpeedY()
		    - unk160 * JGeometry::TVec3<f32>(mVelocity).y;
	}

	mVelocity.x += unk170 * SMS_GetMarioSpeedX();
	mVelocity.z += unk170 * SMS_GetMarioSpeedZ();

	// A ball kicked straight down would otherwise sit still, so give it a
	// random nudge in XZ.
	f32 minSpeed = mMapObjData->getPhysicalData()->unkC;
	if (abs(mVelocity.x) < minSpeed && abs(mVelocity.z) < minSpeed) {
		mVelocity.x = 2.0f * MsRandF() - 1.0f;
		mVelocity.z = 2.0f * MsRandF() - 1.0f;
	}

	unk194 = 10;
	offLiveFlag(LIVE_FLAG_UNK10);
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	// Spelled out rather than through SMS_SendMessageToMario(): retail calls
	// SMS_GetMarioHitActor() and then dispatches receiveMessage through the
	// vtable here, where the helper is a real `bl` in every other TU.
	//
	// TODO: every instruction now matches; the frame is 0xb8 against our 0x78.
	// Retail parks the first TVec3 copy at 0xc and then a descending block of
	// three 12-byte temporaries from 0x90, i.e. 0x40 bytes of low region we do
	// not reserve between the two groups; ours are contiguous at 0x30-0x5f.
	SMS_GetMarioHitActor()->receiveMessage(this, HIT_MESSAGE_ATTACK);

	if (!isActorType(0x400000D0)) {
		SMSGetMSound()->startSoundActor(MSD_SE_MA_KICK_DRIAN, &mPosition, 0,
		                                nullptr, 0, 4);
	}
}

u32 TMapObjBall::touchWater(THitActor* param_1)
{
	if (isState(STATE_HOLDING) || isState(STATE_APPEARING))
		return 1;

	// The current drags the ball along, scaled by the per-kind unk17C.
	JGeometry::TVec3<f32> pushed;
	JGeometry::TVec3<f32> vel(mVelocity);
	pushed.set(vel);

	const JGeometry::TVec3<f32>& flow = getWaterSpeed(param_1);
	// TODO: retail loads flow.x before the drag factor; every spelling tried
	// (scaleAdd, the accessor at each site, the raw member, a named flow.x)
	// loads the drag first. The per-site accessor loads flow first but drops
	// CSE of drag, swaps the fmadds operands, and grows the frame +8. The
	// raw member is also 8 short of the frame: the drag was read through an
	// accessor.
	f32 drag = getUnk17C();
	pushed.x += flow.x * drag;
	pushed.y += flow.y * drag;
	pushed.z += flow.z * drag;
	mVelocity = pushed;

	offLiveFlag(LIVE_FLAG_UNK10);
	return 1;
}

// Reference binding over the physical-parameter chain, used in
// TMapObjBall::boundByActor's kick-up level (a value copy orders its slots
// worse). c-k15: the two direct tests there read
// TMapObjData::getPhysicalData() (code-identical); at the kick-up site the
// accessor, a named `const f32&` over it or the raw chain are all worse.
static inline f32 MapObjBallMinBoundSpeed(const TMapObjBall* p)
{
	const f32& min = p->mMapObjData->mPhysical->unk4->unkC;
	return min;
}

// Binding level over the sound singleton, +8 of low region per site in
// TMapObjBall::boundByActor.
static inline MSound* MapObjBallBoundSound()
{
	MSound* sound = SMSGetMSound();
	return sound;
}

// Mario walking into the ball nudges it harder than standing on it. Holding
// minSpeed in a callee (not in boundByActor) puts its dead word below the
// unnamed TVec3 copies, as in retail.
static inline void MapObjBallKickUp(TMapObjBall* p)
{
	f32 minSpeed = MapObjBallMinBoundSpeed(p);
	if (abs(SMS_GetMarioSpeedX()) > minSpeed
	    || abs(SMS_GetMarioSpeedZ()) > minSpeed) {
		p->mVelocity.y += p->unk150;
		if (!p->isActorType(0x400000D0)) {
			MapObjBallBoundSound()->startSoundActor(MSD_SE_MA_KICK_DRIAN,
			                                &p->mPosition, 0, nullptr, 0, 4);
		}
	} else {
		p->mVelocity.y += p->unk154;
	}
}

void TMapObjBall::boundByActor(THitActor* param_1)
{
	JGeometry::TVec3<f32> away;
	away.set(param_1->mPosition.x - mPosition.x, 0.0f,
	         param_1->mPosition.z - mPosition.z);

	f32 reach;
	if (isActorType(0x400000D0))
		reach = mAttackRadius + param_1->mDamageRadius;
	else
		reach = mDamageRadius;

	if (reach * reach < away.x * away.x + away.z * away.z)
		return;

	if (away.x != 0.0f && away.z != 0.0f)
		MsVECNormalize(away, away);

	if (param_1->isActorType(0x80000001)) {
		if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK2000000)) {
			MapObjBallKickUp(this);

			mVelocity.x += unk148 * SMS_GetMarioSpeedX() - away.x * unk14C;
			mVelocity.z += unk148 * SMS_GetMarioSpeedZ() - away.z * unk14C;
			param_1->receiveMessage(this, HIT_MESSAGE_ATTACK);
		}
	} else {
		f32 into = JGeometry::TVec3<f32>(mVelocity).dot(away);

		if (into >= 0.0f
		    && abs(JGeometry::TVec3<f32>(mVelocity).x)
		        > mMapObjData->getPhysicalData()->unkC
		    && abs(JGeometry::TVec3<f32>(mVelocity).z)
		        > mMapObjData->getPhysicalData()->unkC) {
			mVelocity.x = -((1.0f + unk16C) * (away.x * into) - mVelocity.x);
			mVelocity.y += unk168;
			mVelocity.z = -((1.0f + unk16C) * (away.z * into) - mVelocity.z);
			param_1->receiveMessage(this, HIT_MESSAGE_UNK10);

			if (!isActorType(0x400000D0)) {
				MapObjBallBoundSound()->startSoundActor(MSD_SE_IT_DRIAN_BOUND,
				                                &mPosition, 0, nullptr, 0, 4);
			}
		} else {
			mVelocity.x = -(away.x * unk164 - mVelocity.x);
			mVelocity.y += unk168;
			mVelocity.z = -(away.z * unk164 - mVelocity.z);
		}
	}

	// A falling ball that lands on Mario's head bounces off him.
	if (param_1->isActorType(0x80000001)
	    && !checkMapObjFlag(MAP_OBJ_FLAG_UNK2000000)) {
		if (JGeometry::TVec3<f32>(mVelocity).y < 0.0f
		    && 130.0f + SMS_GetMarioPos().y < mPosition.y + mBodyRadius) {
			mVelocity.y = unk160 * -JGeometry::TVec3<f32>(mVelocity).y;
			mVelocity.x += unk158 * SMS_GetMarioSpeedX();
			mVelocity.y += unk15C * SMS_GetMarioSpeedY();
			mVelocity.z += unk158 * SMS_GetMarioSpeedZ();

			if (!isActorType(0x400000D0)) {
				MapObjBallBoundSound()->startSoundActor(MSD_SE_MA_KICK_DRIAN,
				                                &mPosition, 0, nullptr, 0, 4);
			}
		}
	}

	unk194 = 10;
	offLiveFlag(LIVE_FLAG_UNK10);
	onLiveFlag(LIVE_FLAG_AIRBORNE);
}

void TMapObjBall::touchActor(THitActor* param_1)
{
	// unk194 is a short cooldown after a kick, so one kick cannot chain.
	if (unk194 != 0 || isState(STATE_HOLDING) || isHideObj(param_1)
	    || param_1->isActorType(0x08000083)
	    || param_1->isActorType(0x400000CA)
	    || param_1->isActorType(0x400000CC))
		return;

	if (param_1->isActorType(0x80000001)) {
		if (!isActorType(0x400000D0) && SMS_GetMarioSpeedY() != 0.0f) {
			kicked();
			return;
		}
	}

	boundByActor(param_1);
}

// Horizontal speed, one copy of the velocity per read. Retail rounds z*z and
// fuses x*x into the sum (fma(x, x, z*z)); this spelling does the same.
static inline f32 MapObjBallXZSpeed(const JGeometry::TVec3<f32>& v)
{
	return JGeometry::TUtil<f32>::sqrt(
	    JGeometry::TVec3<f32>(v).x * JGeometry::TVec3<f32>(v).x
	    + JGeometry::TVec3<f32>(v).z * JGeometry::TVec3<f32>(v).z);
}

// TODO: 96.1%. The speed spelled as two arguments to a TU-local
// `x * x + z * z` helper matched every instruction (99.3%, frame 0x48 short)
// but rounded x*x and fused z*z, the reverse of retail (tools/expr-diff.py):
// up to 1 ulp in the roll angle. Swapping that helper's arguments or addends,
// or naming a square, did not reverse the fusion; the plain four-copy sum
// above does, at the cost of the velocity temporaries' order.
// c-k15: the four settle/roll tests read TMapObjData::getPhysicalData(); each
// accessor's receiver binding and forced load land the frame at retail's
// 0x1c0 (96.1 -> 96.3, 105 -> 59 markers). What is left is the speed: retail
// makes two velocity copies (x from the first, z from the second) where the
// four-copy sum makes four; the two-level SqXZ helper is 0x18 short with it.
void TMapObjBall::calcCurrentMtx()
{
	TPosition3f rot;
	rot.identity();

	// Settle a nearly-stopped ball on flat ground so it does not creep.
	if (abs(JGeometry::TVec3<f32>(mVelocity).x)
	    < mMapObjData->getPhysicalData()->unkC) {
		if (abs(JGeometry::TVec3<f32>(mVelocity).z)
		        < mMapObjData->getPhysicalData()->unkC
		    && mGroundPlane->mNormal.y == 1.0f) {
			mVelocity.x = 0.0f;
			mVelocity.z = 0.0f;
		}
	}

	if (abs(JGeometry::TVec3<f32>(mVelocity).x)
	        > mMapObjData->getPhysicalData()->unkC
	    || abs(JGeometry::TVec3<f32>(mVelocity).z)
	        > mMapObjData->getPhysicalData()->unkC) {
		// Roll about the horizontal axis square to the direction of travel,
		// by the arc length the ball has covered over its own radius.
		JGeometry::TVec3<f32> axis;
		getVerticalVecToTargetXZ(
		    mPosition.x + JGeometry::TVec3<f32>(mVelocity).x,
		    mPosition.z + JGeometry::TVec3<f32>(mVelocity).z, &axis);

		f32 rolled = 2.0f * (MapObjBallXZSpeed(mVelocity) / mBodyRadius);
		rot.setRotate(axis, rolled);
	}

	TPosition3f cur;
	cur.set(getModel()->getAnmMtx(0));
	cur.ref(0, 3) = 0.0f;
	cur.ref(1, 3) = 0.0f;
	cur.ref(2, 3) = 0.0f;
	MTXConcat(rot, cur, rot);

	rot.ref(0, 3) = mPosition.x;
	rot.ref(1, 3) = mPosition.y + mBodyRadius;
	rot.ref(2, 3) = mPosition.z;

	if (isActorType(0x40000394) && rot.at(1, 1) > 0.0f)
		rot.ref(1, 3) = -(50.0f * rot.at(1, 1) - rot.at(1, 3));

	if (isActorType(0x40000392))
		rot.ref(1, 3) = -(10.0f * (1.0f - rot.at(1, 1)) - rot.at(1, 3));

	getModel()->setAnmMtx(0, rot);
}

static inline const TMapObjPhysicalInfo* MapObjBallPhysical(const TMapObjBall* p)
{
	return p->mMapObjData->mPhysical;
}

static inline u32 MapObjBallWallCheckFlags(const TMapObjBall* p)
{
	u32 flags = MapObjBallPhysical(p)->mWallCheckFlags;
	return flags;
}

// TODO: 99.7%. Every instruction matches and the frame is exact; retail
// puts `centre` (0x28) below the check record (0x34) where ours puts it
// above. mBodyRadius read inside the sum gives retail's y-then-radius load
// order. Declaring the record first and filling it field by field
// reorders the slots but reloads the radius (four instructions).
void TMapObjBall::checkWallCollision(JGeometry::TVec3<f32>* param_1)
{
	JGeometry::TVec3<f32> centre;
	centre.x = param_1->x;
	centre.y = param_1->y + mBodyRadius;
	centre.z = param_1->z;

	TBGWallCheckRecord check(centre, mBodyRadius, 4,
	                         MapObjBallWallCheckFlags(this));

	if (gpMap->isTouchedWallsAndMoveXZ(&check)) {
		unk138   = check.mResultWalls[0];
		param_1->x = centre.x;
		param_1->z = centre.z;
		touchWall(param_1, &check);
		return;
	}

	unk138 = nullptr;
}

void TMapObjBall::makeObjDefault()
{
	TMapObjBase::makeObjDefault();

	MtxPtr mtx  = getModel()->getAnmMtx(0);
	mtx[0][3] = getPosition().x;
	mtx[1][3] = mPosition.y + mBodyRadius;
	mtx[2][3] = getPosition().z;
}

void TMapObjBall::makeObjAppeared()
{
	TMapObjBase::makeObjAppeared();
	calcCurrentMtx();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	mtx[0][3] = getPosition().x;
	mtx[1][3] = mPosition.y + mBodyRadius;
	mtx[2][3] = getPosition().z;

	if (isActorType(0x40000394)) {
		if (mtx[1][1] > 0.0f)
			mtx[1][3] = -(50.0f * mtx[1][1] - mtx[1][3]);
	}

	if (isActorType(0x40000392))
		mtx[1][3] = -(10.0f * (1.0f - mtx[1][1]) - mtx[1][3]);

	unkE8 = 0;
}

void TMapObjBall::control()
{
	TMapObjGeneral::control();

	// The three named locals below are what keeps this body at fifteen
	// statements, which is one over MWCC's depth-1 inline budget; without
	// them TResetFruit::control's LIVING and HOLDING arms expand this
	// function instead of calling it, as the ROM does. The register
	// evidence agrees: the countdown is loaded into a register and tested
	// there, and both matrix pointers are fetched into their own registers.
	int timer = unk194;
	if (timer != 0)
		unk194 = timer - 1;

	if (isState(STATE_HOLDING)) {
		// While carried the ball rides the holder's matrix, lifted clear of
		// the hand by unk190.
		Mtx mtx;
		MtxPtr taking = mHolder->getTakingMtx();
		MTXCopy(taking, mtx);
		mtx[1][3] += unk190;
		MtxPtr anm = getModel()->getAnmMtx(0);
		MTXCopy(mtx, anm);
		return;
	}

	// Assigned, not copy-constructed: retail's isZero keeps its three
	// fmuls apart (no fmadds), which only operator='s cast copy gives.
	JGeometry::TVec3<f32> vel;
	vel = getVelocity();
	if (!vel.isZero() || mGroundPlane->getActor() != nullptr)
		calcCurrentMtx();
}

BOOL TMapObjBall::receiveMessage(THitActor* sender, u32 message)
{
	if (TMapObjGeneral::receiveMessage(sender, message))
		return TRUE;

	if (message == HIT_MESSAGE_TAKE && (unkF8 & 0x100000)) {
		hold((TTakeActor*)sender);
		return TRUE;
	}

	// Mario walking into a ball kicks it, except for the watermelon and
	// except when he is trying to pick it up.
	if (sender->isActorType(0x80000001)) {
		if (!isActorType(0x400000D0) && message != HIT_MESSAGE_TAKE) {
			kicked();
			return TRUE;
		}
	}

	return FALSE;
}

void TMapObjBall::initMapObj()
{
	TMapObjGeneral::initMapObj();

	mInitialScaling.x = mScaling.x;
	mInitialScaling.y = mScaling.y;
	mInitialScaling.z = mScaling.z;

	// Per-kind physics. The layout is identical in every arm, so the switch
	// is really a table of tunables keyed on the object type.
	switch (mActorType) {
	case 0x400000D0: // watermelon
		unk14C = 4.0f;
		unk150 = 0.0f;
		unk154 = 0.0f;
		unk158 = 0.15f;
		unk15C = 0.0f;
		unk160 = 0.9f;
		unk164 = 0.06f;
		unk168 = 1.5f;
		unk16C = 0.5f;
		unk170 = 0.5f;
		unk174 = 0.2f;
		unk178 = 2.5f;
		unk17C = 0.001f;
		unk180 = 0.3f;
		unk184 = 1.5f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		mDepthAtFloating      = mBodyRadius / 3.0f;
		break;

	case 0x40000064:
		unk148 = 0.6f;
		unk14C = 2.0f;
		unk150 = 0.02f;
		unk154 = 0.0f;
		unk158 = 0.055f;
		unk15C = 0.02f;
		unk160 = 0.83f;
		unk170 = 0.9f;
		unk174 = 0.13f;
		unk178 = 20.0f;
		unk164 = 0.5f;
		unk168 = 0.02f;
		unk16C = 0.5f;
		unk17C = 1.2f;
		unk180 = 0.8f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		mDepthAtFloating      = mBodyRadius / 3.0f;
		break;

	case 0x40000393:
		unk148 = 0.6f;
		unk14C = 0.2f;
		unk150 = 1.3f;
		unk154 = 15.0f;
		unk158 = 0.5f;
		unk15C = 1.3f;
		unk160 = 1.0f;
		unk170 = 0.9f;
		unk174 = 0.13f;
		unk178 = 20.0f;
		unk164 = 2.0f;
		unk168 = 0.02f;
		unk16C = 0.3f;
		unk17C = 0.05f;
		unk180 = 0.5f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		mDepthAtFloating      = 50.0f;
		break;

	case 0x40000390:
	case 0x40000391:
	case 0x40000392:
		unk148 = 0.4f;
		unk14C = 0.2f;
		unk150 = 1.3f;
		unk154 = 0.0f;
		unk158 = 1.2f;
		unk15C = 0.8f;
		unk160 = 0.5f;
		unk170 = 0.9f;
		unk174 = 0.13f;
		unk178 = 20.0f;
		unk164 = 2.0f;
		unk168 = 0.02f;
		unk16C = 0.3f;
		unk17C = 0.05f;
		unk180 = 0.5f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		mDepthAtFloating      = 50.0f;
		break;

	case 0x40000394:
		unk148 = 0.2f;
		unk14C = 0.0f;
		unk150 = 0.0f;
		unk154 = 0.0f;
		unk158 = 0.0f;
		unk15C = 0.0f;
		unk160 = 0.0f;
		unk170 = 0.0f;
		unk174 = 0.0f;
		unk178 = 0.0f;
		unk164 = 0.0f;
		unk168 = 0.0f;
		unk16C = 0.0f;
		unk17C = 0.05f;
		unk180 = 0.5f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		mDepthAtFloating      = 50.0f;
		break;

	case 0x40000395:
		unk148 = 0.4f;
		unk14C = 0.2f;
		unk150 = 1.3f;
		unk154 = 0.0f;
		unk158 = 1.2f;
		unk15C = 0.8f;
		unk160 = 0.5f;
		unk170 = 0.9f;
		unk174 = 0.13f;
		unk178 = 20.0f;
		unk164 = 2.0f;
		unk168 = 0.02f;
		unk16C = 0.3f;
		unk17C = 0.05f;
		unk180 = 0.5f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		mDepthAtFloating      = 50.0f;
		break;

	}

	// unk190 is the lift applied while the ball is carried.
	if (isActorType(0x40000393)) {
		mBodyRadius = 45.0f * mScaling.y;
		unk190      = mBodyRadius;
	}
	if (isActorType(0x40000390)) {
		mBodyRadius = 40.0f * mScaling.y;
		unk190      = 20.0f;
	}
	if (isActorType(0x40000391)) {
		mBodyRadius = 40.0f * mScaling.y;
		unk190      = 20.0f;
	}
	if (isActorType(0x40000392))
		unk190 = 10.0f;
}

TMapObjBall::TMapObjBall(const char* name)
    : TMapObjGeneral(name)
{
	unk148 = 0.0f;
	unk14C = 0.0f;
	unk150 = 0.0f;
	unk154 = 0.0f;
	unk158 = 0.0f;
	unk15C = 0.0f;
	unk160 = 0.0f;
	unk164 = 0.0f;
	unk168 = 0.0f;
	unk16C = 0.0f;
	unk170 = 0.0f;
	unk174 = 0.0f;
	unk178 = 0.0f;
	unk17C = 0.0f;
	unk180 = 0.0f;
	unk184 = 0.0f;
	unk188 = 0.0f;
	mDepthAtFloating = 0.0f;
	unk190 = 0.0f;
	unk194 = 0;

	mInitialScaling.z = 0.0f;
	mInitialScaling.y = 0.0f;
	mInitialScaling.x = 0.0f;
}

u32 TResetFruit::mFruitLivingTime       = 14400;
f32 TResetFruit::mScaleUpSpeed          = 1.05f;
// UNUSED in the map; the value is not recoverable from the binary.
f32 TResetFruit::mRottingScaleSpeed     = 0.99f;
f32 TResetFruit::mBreakingScaleSpeed    = 0.96f;
u32 TResetFruit::mFruitWaitTimeToAppear = 360;
// UNUSED in the map; the value is not recoverable from the binary.
GXColorS10 TResetFruit::mRottenColor    = { 0, 0, 0, 0 };

void TResetFruit::checkGroundCollision(JGeometry::TVec3<f32>* param_1)
{
	u8 map = SMSGetMarDirector()->getCurrentMap();
	if (map != 7 && map != 4) {
		TMapObjGeneral::checkGroundCollision(param_1);
		return;
	}

	if (map == 4) {
		// Probe from well above so a fruit cannot fall through the deck.
		mGroundHeight = gpMap->checkGround(param_1->x, 200.0f + param_1->y,
		                                   param_1->z, &mGroundPlane);
		mGroundHeight += 1.0f;
		if (param_1->y <= getGroundHeight()) {
			touchGround(param_1);
			return;
		}
		onLiveFlag(LIVE_FLAG_AIRBORNE);
		return;
	}

	mGroundHeight = SMSGetMapBound()->checkGround(param_1->x, param_1->y + mHeadHeight,
	                                   param_1->z, &mGroundPlane);

	if (getGroundPlane()->isMapObjThrough()) {
		mGroundHeight = SMSGetMapBound()->checkGroundExactY(
		    param_1->x, mGroundHeight - 200.0f, param_1->z, &mGroundPlane);
	}

	mGroundHeight += 1.0f;
	if (param_1->y <= mGroundHeight) {
		touchGround(param_1);
		return;
	}
	onLiveFlag(LIVE_FLAG_AIRBORNE);
}

// By-value pointer fork over the model accessor, +4 of low region per site.
static inline J3DModel* MapObjBallModel(const TLiveActor* p) { return p->getModel(); }

// Binding level nested over the model fork, +8 of low region per site.
static inline MtxPtr MapObjBallAnmMtx0(const TLiveActor* p)
{
	MtxPtr mtx = MapObjBallModel(p)->getAnmMtx(0);
	return mtx;
}

// Binding level over the sound singleton, +8 of low region per site.
static inline MSound* ResetFruitAppearSound()
{
	MSound* sound = SMSGetMSound();
	return sound;
}

// Binding level over a raw member read, worth +16 of low region in
// TResetFruit::makeObjWaitingToAppear (batch 127).
static inline u8 MapObjBallUnk1A4(const TResetFruit* p)
{
	u8 v1A4 = p->unk1A4;
	return v1A4;
}

void TResetFruit::waitingToAppear()
{
	if (SMSGetMarDirector()->getCurrentMap() == 3 && MapObjBallUnk1A4(this))
		makeObjDead();

	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
		return;

	if (!isStateTimerEngaged() && mColCount == 0) {
		onMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
		makeObjAppeared();

		Mtx small;
		MTXScale(small, 0.2f, 0.2f, 0.2f);
		concatOnlyRotFromLeft(small, MapObjBallModel(this)->getAnmMtx(0),
		                      MapObjBallModel(this)->getAnmMtx(0));

		mScaling.y = 0.2f;
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mState = STATE_APPEARING;

		ResetFruitAppearSound()->startSoundActor(MSD_SE_IT_COMMON_APPEAR, &mPosition, 0,
		                                nullptr, 0, 4);
	}
}


void TResetFruit::makeObjWaitingToAppear()
{
	mState = STATE_LIVING;
	makeObjDefault();
	makeObjDead();
	calcRootMatrix();
	getModel()->calc();

	mStateTimer = mFruitWaitTimeToAppear;
	offMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
	mState = STATE_WAITING_TO_APPEAR;

	// On the map where these are a one-shot, do not queue a respawn.
	if (SMSGetMarDirectorBound()->mMap == 3 && MapObjBallUnk1A4(this))
		makeObjDead();
}

void TResetFruit::thrown()
{
	TMapObjGeneral::thrown();
	mState = STATE_LIVING;
}

// Inlined TMapObjBall::hold expansion for TResetFruit::hold. getVelocity()
// inside this callee is what sizes the second TVec3; the standalone
// TMapObjBall::hold uses MapObjBallHoldSpeed instead.
static inline void MapObjBallDoHold(TMapObjBall* p, TTakeActor* actor)
{
	if (JGeometry::TVec3<f32>(p->getVelocity()).length() > 10.0f)
		return;
	p->TMapObjGeneral::hold(actor);
	p->mVelocity.zero();
}

void TResetFruit::hold(TTakeActor* param_1)
{
	if (JGeometry::TVec3<f32>(mVelocity).length() > 10.0f)
		return;

	MapObjBallDoHold(this, param_1);
	mVelocity.zero();
	onLiveFlag(LIVE_FLAG_UNK10);

	if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000)) {
		if (!isStateTimerEngaged()) {
			onMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
			mStateTimer = getLivingTime();
		}
	}
}

// Binding level over the sound singleton, worth +0x10 of low region in
// TResetFruit::touchPollution.
static inline MSound* MapObjBallGetMSound()
{
	MSound* sound = SMSGetMSound();
	return sound;
}

void TResetFruit::touchPollution()
{
	gpMarioParticleManager->emitAndBindToPosPtr(0x8B, &mPosition, 0, nullptr);
	MapObjBallGetMSound()->startSoundActor(MSD_SE_OBJ_AWAY_INTO_GRAF,
	                                       &mPosition, 0, nullptr, 0, 4);
	makeObjDefault();
	makeObjWaitingToAppear();
}

void TResetFruit::touchWaterSurface()
{
	emitColumnWater();
	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_DRINA_TO_WATER, &mPosition);
	makeObjWaitingToAppear();
}

// TODO: loads drag before flow.x, as in TMapObjBall::touchWater. Also inert:
// `flow.x * drag + pushed.x`, `drag * flow.x`, raw unk17C, drag first.
u32 TResetFruit::touchWater(THitActor* param_1)
{
	if (!isState(STATE_HOLDING) && !isState(STATE_APPEARING)) {
		JGeometry::TVec3<f32> vel(mVelocity);
		JGeometry::TVec3<f32> pushed;
		pushed.set(vel);

		const JGeometry::TVec3<f32>& flow = getWaterSpeed(param_1);
		f32 drag = getUnk17C();
		pushed.x += flow.x * drag;
		pushed.y += flow.y * drag;
		pushed.z += flow.z * drag;
		mVelocity = pushed;

		offLiveFlag(LIVE_FLAG_UNK10);
	}

	makeObjLiving();
	return 1;
}

// Binding level worth +16 of low region, landing TResetFruit::touchActor's
// frame at 0x28 (batch 124).
static inline bool MapObjBallIsState(TResetFruit* p, u32 i)
{
	bool state = p->isState(i);
	return state;
}

void TResetFruit::touchActor(THitActor* param_1) { pick(param_1); }

void TResetFruit::touchGround(JGeometry::TVec3<f32>* param_1)
{
	if (mGroundPlane->isDeathPlane()) {
		makeObjWaitingToAppear();
		param_1->set(mPosition);
		return;
	}

	TMapObjBall::touchGround(param_1);
}

void TResetFruit::makeObjLiving()
{
	if (!isStateTimerEngaged()) {
		onMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
		mStateTimer = getLivingTime();
	}
	offLiveFlag(LIVE_FLAG_UNK10);
	mState = STATE_LIVING;
}

// UNUSED, 0x254 in the map.
// UNUSED, 0x254 in the map (ours 0x250): touchActor()'s body. Out of line it
// expands TMapObjBall::touchActor; reached through touchActor() or control()'s
// NORMAL loop it sits one level deeper, where retail calls that instead.
void TResetFruit::pick(THitActor* param_1)
{
	if (MapObjBallIsState(this, STATE_APPEARING))
		return;
	if (MapObjBallIsState(this, STATE_BREAKING))
		return;
	if (MapObjBallIsState(this, STATE_ROTTING))
		return;
	if (MapObjBallIsState(this, STATE_WAITING_TO_APPEAR))
		return;

	TMapObjBall::touchActor(param_1);

	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
		return;

	// Being knocked about starts the countdown, unless it is being carried.
	if (MapObjBallIsState(this, STATE_NORMAL)
	    && !checkLiveFlag(LIVE_FLAG_UNK10))
		makeObjLiving();
}

void TResetFruit::kicked()
{
	// Assigned in the last || term so the load sits after the two flag
	// tests and the value stays in f5 through the later fmsubs.
	f32 marioY;
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK2000000) || isState(STATE_HOLDING)
	    || (marioY = SMS_GetMarioSpeedY()) < 0.0f)
		return;

	if (JGeometry::TVec3<f32>(mVelocity).y <= 0.0f) {
		// Already in the air and heading away from Mario: leave it alone.
		JGeometry::TVec3<f32> diff;
		diff.x = SMS_GetMarioPos().x - getPosition().x;
		diff.y = 0.0f;
		diff.z = SMS_GetMarioPos().z - getPosition().z;
		f32 toward = JGeometry::TVec3<f32>(mVelocity).dot(diff);
		// checkLiveFlag2 is the signed BOOL that emits retail's
		// `li 1/0; cmpwi`. toward has to be computed first so that
		// materialisation lands after the dot product.
		BOOL airborne = checkLiveFlag2(LIVE_FLAG_AIRBORNE);
		if (airborne) {
			if (toward > 0.0f)
				return;
		}
		// TODO: 99.8%, every instruction matches. Frame is 0xc8 against
		// retail 0xe0 (ladder 330's TVec3-at-bottom-of-pool class): the
		// velocity copies sit low. getMapObjData() for minSpeed is +8
		// more, but TMapObjBall::kicked reads it raw (c-hs6). Each test copies mVelocity itself,
		// as in TMapObjBall::kicked; a named `vel` copied again for the dot
		// product reloads it instead of reusing the source registers.

		if (JGeometry::TVec3<f32>(mVelocity).y == 0.0f) {
			mVelocity.y = unk178;
		} else {
			mVelocity.y = unk174 * marioY
			    - unk160 * JGeometry::TVec3<f32>(mVelocity).y;
		}

		mVelocity.x += unk170 * SMS_GetMarioSpeedX();
		mVelocity.z += unk170 * SMS_GetMarioSpeedZ();

		f32 minSpeed = mMapObjData->getPhysicalData()->unkC;
		if (abs(mVelocity.x) < minSpeed && abs(mVelocity.z) < minSpeed) {
			mVelocity.x = 2.0f * MsRandF() - 1.0f;
			mVelocity.z = 2.0f * MsRandF() - 1.0f;
		}

		unk194 = 10;
		offLiveFlag(LIVE_FLAG_UNK10);
		SMS_GetMarioHitActor()->receiveMessage(this, HIT_MESSAGE_ATTACK);
		SMSGetMSound()->startSoundActor(MSD_SE_MA_KICK_DRIAN, &mPosition, 0,
		                                nullptr, 0, 4);
	}
}

// UNUSED, 0x188 in the map. control()'s LIVING arm without its doubled
// sand-pillar type test. TODO: 0x190 with one isActorType() (0x1b0 with two).
void TResetFruit::living()
{
	offHitFlag(HIT_FLAG_NO_COLLISION);
	if (gpMarDirector->mMap == 4 && checkLiveFlag(LIVE_FLAG_UNK10))
		offLiveFlag(LIVE_FLAG_UNK10);

	if (mGroundPlane->getActor()) {
		if (checkLiveFlag(LIVE_FLAG_UNK10))
			offLiveFlag(LIVE_FLAG_UNK10);

		const TLiveActor* owner = getGroundPlane()->getActor();
		if (mPosition.y < mGroundHeight + 200.0f) {
			if (owner->isActorType(0x400000CD)) {
				f32 wasRatio = unk198;
				unk198       = SMS_GetSandRiseUpRatio(owner);
				if (unk198 > 0.05f && unk198 > wasRatio)
					mVelocity.y += 20.0f;
			}
		}
	} else {
		unk198 = 0.0f;
	}

	TMapObjBall::control();
	rotting();
}

// UNUSED, 0xac in the map. The appear-effect half of waitingToAppear(): the
// original spells it out there, so this standalone copy is dead.
void TResetFruit::waitEffect()
{
	onMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
	makeObjAppeared();

	Mtx small;
	MTXScale(small, 0.2f, 0.2f, 0.2f);
	concatOnlyRotFromLeft(small, getModel()->getAnmMtx(0),
	                      getModel()->getAnmMtx(0));

	mScaling.y = 0.2f;
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mState = STATE_APPEARING;

	SMSGetMSound()->startSoundActor(MSD_SE_IT_COMMON_APPEAR, &mPosition, 0,
	                                nullptr, 0, 4);
}

// UNUSED, 0xac in the map. Inlined at the end of control()'s living and
// holding arms: once the countdown expires the fruit is dropped by whoever
// is carrying it, stopped dead, and starts to rot.
void TResetFruit::rotting()
{
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
		return;
	if (isStateTimerEngaged())
		return;

	if (mHolder) {
		mHolder->receiveMessage(this, HIT_MESSAGE_UNK8);
		mHolder->mHeldObject = nullptr;
		mHolder              = nullptr;
	}

	mVelocity.z = mVelocity.y = mVelocity.x = 0.0f;
	mState                                  = STATE_ROTTING;
}

void TResetFruit::breaking()
{
	Mtx squash;
	MTXScale(squash, 1.0f, mBreakingScaleSpeed, 1.0f);

	MtxPtr mtx = MapObjBallModel(this)->getAnmMtx(0);
	concatOnlyRotFromLeft(squash, mtx, mtx);

	mScaling.y *= mBreakingScaleSpeed;
	mtx[1][3] = mBodyRadius * mScaling.y + mPosition.y;

	if (mScaling.y < 0.2f) {
		mPosition.y += mBodyRadius / 2.0f;
		mScaling.x = mInitialScaling.x;
		mScaling.y = mInitialScaling.y;
		mScaling.z = mInitialScaling.z;

		emitAndScale(0xE5, 0, &mPosition);
		MapObjBallGetMSound()->startSoundActor(MSD_SE_SMOKE_EFFECT, &mPosition, 0,
		                                nullptr, 0, 4);
		mStateTimer = 240;
		sleep();
		mState = STATE_BROKEN;
	}
}

void TResetFruit::appearing()
{
	Mtx grow;
	MTXScale(grow, mScaleUpSpeed, mScaleUpSpeed, mScaleUpSpeed);

	MtxPtr mtx = MapObjBallModel(this)->getAnmMtx(0);
	concatOnlyRotFromLeft(grow, mtx, mtx);

	mScaling.y *= mScaleUpSpeed;
	mScaledBodyRadius = mBodyRadius * getScaling().y;
	mtx[1][3] = mBodyRadius * getScaling().y + mPosition.y;

	if (getScaling().y >= mInitialScaling.y) {
		mScaling.x = mInitialScaling.x;
		mScaling.y = mInitialScaling.y;
		mScaling.z = mInitialScaling.z;
		getModel()->calc();
		offHitFlag(HIT_FLAG_NO_COLLISION);
		makeObjAppeared();
		mState = STATE_NORMAL;
	}
}

// TODO: instruction-exact, frame 0xb8 vs 0xf8 (0x90 before the ground
// plane, map and initial scaling were read through their accessors, c-hs5).
// The APPEARING arm is TMapObjBall::control expanded; a 12-statement
// spelling of it (no `timer`, if/else instead of return; 99.9 out of line)
// inlines at all three arms and the frame jumps to 0x158. Retail must reach the LIVING and HOLDING arms one
// level deeper; living() itself is not it (it is then called, frame 0xe8).
void TResetFruit::control()
{
	switch (mState) {
	case STATE_NORMAL:
		offHitFlag(HIT_FLAG_NO_COLLISION);
		for (int i = 0; i < mColCount; ++i)
			pick(mCollisions[i]);
		if (getGroundPlane()->getActor())
			calcCurrentMtx();
		break;

	case STATE_LIVING:
		offHitFlag(HIT_FLAG_NO_COLLISION);
		if (gpMarDirector->getCurrentMap() == 4
		    && checkLiveFlag(LIVE_FLAG_UNK10))
			offLiveFlag(LIVE_FLAG_UNK10);

		if (getGroundPlane()->getActor()) {
			if (checkLiveFlag(LIVE_FLAG_UNK10))
				offLiveFlag(LIVE_FLAG_UNK10);

			// Sitting on a rising sand pillar lifts the fruit with it.
			const TLiveActor* owner = getGroundPlane()->getActor();
			if (mPosition.y < mGroundHeight + 200.0f) {
				// TODO: the original tests the same type twice here.
				if (owner->isActorType(0x400000CD)
				    || owner->isActorType(0x400000CD)) {
					f32 wasRatio = unk198;
					unk198       = SMS_GetSandRiseUpRatio(owner);
					if (unk198 > 0.05f && unk198 > wasRatio)
						mVelocity.y += 20.0f;
				}
			}
		} else {
			unk198 = 0.0f;
		}

		TMapObjBall::control();
		rotting();
		break;

	case STATE_HOLDING:
		TMapObjBall::control();
		rotting();
		break;

	case STATE_APPEARING:
	case STATE_BREAKING:
		TMapObjGeneral::control();
		if (unk194 != 0)
			unk194 -= 1;

		if (isState(STATE_HOLDING)) {
			Mtx held;
			MTXCopy(mHolder->getTakingMtx(), held);
			held[1][3] += unk190;
			MTXCopy(held, getModel()->getAnmMtx(0));
			break;
		}

		{
			JGeometry::TVec3<f32> vel;
			vel = mVelocity;
			if (!vel.isZero() || getGroundPlane()->getActor())
				calcCurrentMtx();
		}
		break;

	case STATE_ROTTING:
		// Sink into the ground, restore the original scale, puff smoke and
		// sleep until the respawn timer runs out.
		mPosition.y += mBodyRadius / 2.0f;
		mScaling.x = getInitialScaling().x;
		mScaling.y = getInitialScaling().y;
		mScaling.z = getInitialScaling().z;
		emitAndScale(0xE5, 0, &mPosition);
		SMSGetMSound()->startSoundActor(MSD_SE_SMOKE_EFFECT, &mPosition, 0,
		                                nullptr, 0, 4);
		mStateTimer = 240;
		sleep();
		mState = STATE_BROKEN;
		break;

	case STATE_BROKEN:
		if (isStateTimerEngaged())
			break;

		unk19C.r = 255;
		unk19C.g = 255;
		unk19C.b = 255;
		awake();
		mState = STATE_LIVING;
		makeObjDefault();
		makeObjDead();
		calcRootMatrix();
		getModel()->calc();

		mStateTimer = mFruitWaitTimeToAppear;
		offMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
		mState = STATE_WAITING_TO_APPEAR;
		if (gpMarDirector->getCurrentMap() == 3 && unk1A4)
			makeObjDead();
		break;
	}
}

// Binding level over the area-cube singleton, +8 of low region.
static inline TCubeManagerArea* MapObjBallGetCubeArea()
{
	TCubeManagerArea* area = gpCubeArea;
	return area;
}

void TResetFruit::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (SMSGetMarDirector()->mMap == 7) {
		if (MapObjBallIsState(this, STATE_HOLDING)
		    || !JGeometry::TVec3<f32>(getVelocity()).isZero()) {
			if (checkLiveFlag(LIVE_FLAG_UNK200))
				offLiveFlag(LIVE_FLAG_UNK200);
		} else if (!MapObjBallGetCubeArea()->isInAreaCube((const Vec&)mPosition)) {
			// Settled outside every area cube and away from where it
			// started: send it back to its spawn point.
			if (MapObjBallIsState(this, STATE_LIVING)
			    && (mPosition.x != mInitialPosition.x
			        || mPosition.z != mInitialPosition.z)) {
				makeObjWaitingToAppear();
				return;
			}
		}
	}

	TMapObjGeneral::perform(cue, graphics);
}

void TResetFruit::killByTimer(int param_1)
{
	mStateTimer = param_1;
	onMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
	mState = STATE_LIVING;
}

void TResetFruit::makeObjAppeared()
{
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
		makeObjDefault();

	TMapObjBase::makeObjAppeared();
	calcCurrentMtx();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	mtx[0][3] = getPosition().x;
	mtx[1][3] = mPosition.y + mBodyRadius;
	mtx[2][3] = getPosition().z;

	if (isActorType(0x40000394)) {
		if (mtx[1][1] > 0.0f)
			mtx[1][3] = -(50.0f * mtx[1][1] - mtx[1][3]);
	}

	if (isActorType(0x40000392))
		mtx[1][3] = -(10.0f * (1.0f - mtx[1][1]) - mtx[1][3]);

	unkE8 = 0;

	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
		mState = STATE_LIVING;
}

BOOL TResetFruit::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_UNKB) {
		if (MapObjBallIsState(this, STATE_NORMAL)
		    || MapObjBallIsState(this, STATE_HOLDING)
		    || MapObjBallIsState(this, STATE_LIVING)) {
			makeObjWaitingToAppear();
			return TRUE;
		}
		return FALSE;
	}

	if (message == HIT_MESSAGE_UNKD) {
		kill();
		return TRUE;
	}

	if (MapObjBallIsState(this, STATE_NORMAL)
	    || MapObjBallIsState(this, STATE_HOLDING)
	    || MapObjBallIsState(this, STATE_LIVING)) {
		// pick() expands here as in control(); the virtual touchActor()
		// could not be inlined.
		pick(sender);

		BOOL handled = TMapObjBall::receiveMessage(sender, message);
		// Putting the fruit down starts its countdown.
		if (message == HIT_MESSAGE_PUT) {
			if (MapObjBallIsState(this, STATE_NORMAL))
				mState = STATE_LIVING;
		}
		return handled;
	}

	return FALSE;
}

void TResetFruit::initMapObj()
{
	TMapObjBall::initMapObj();
	SMS_InitPacket_OneTevColor(getModel(), 0, GX_TEVREG0, &unk19C);
}

TResetFruit::TResetFruit(const char* name)
    : TMapObjBall(name)
{
	unk198 = 0.0f;
	unk1A4 = 0;

	unk19C.r = 255;
	unk19C.g = 255;
	unk19C.b = 255;
	unk19C.a = 255;
}

void TRandomFruit::initMapObj()
{
	switch ((int)(5.0f * MsRandF())) {
	case 0:
		snprintf(mModelName, sizeof(mModelName), "FruitCoconut");
		break;
	case 1:
		snprintf(mModelName, sizeof(mModelName), "FruitDurian");
		break;
	case 2:
		snprintf(mModelName, sizeof(mModelName), "FruitPapaya");
		break;
	case 3:
		snprintf(mModelName, sizeof(mModelName), "FruitPine");
		break;
	case 4:
	case 5:
	default:
		snprintf(mModelName, sizeof(mModelName), "FruitPine");
		break;
	}

	unkF4 = mModelName;
	TMapObjBall::initMapObj();
	SMS_InitPacket_OneTevColor(getModel(), 0, GX_TEVREG0, &unk19C);
}

TRandomFruit::TRandomFruit(const char* name)
    : TResetFruit(name)
{
	memset(mModelName, 0, sizeof(mModelName));
}

void TCoverFruit::calcRootMatrix()
{
	if (mHolder) {
		// While carried it simply rides the holder's matrix.
		MtxPtr held = mHolder->getTakingMtx();
		getModel()->setBaseTRMtx(held);
		mPosition.set(held[0][3], held[1][3], held[2][3]);
	} else {
		MsMtxSetXYZRPH(getModel()->getBaseTRMtx(), mPosition.x,
		               mPosition.y - mYOffset, mPosition.z, getRotation().x,
		               getRotation().y, mRotation.z);
	}

	getModel()->setBaseScale(*(Vec*)&mScaling);
}

BOOL TCoverFruit::receiveMessage(THitActor* sender, u32 message)
{
	// A Yoshi-class actor taking the cover fruit picks it up outright.
	if (sender->isActorType(0x08000083) && message == HIT_MESSAGE_TAKE) {
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mHolder = (TTakeActor*)sender;
		return TRUE;
	}

	if (message == HIT_MESSAGE_UNKB) {
		kill();
		TFlagManager::smInstance->setBool(true, 0x1038B);
		return TRUE;
	}

	return FALSE;
}

void TCoverFruit::loadAfter()
{
	TMapObjBase::loadAfter();
	if (TFlagManager::smInstance->getBool(0x1038B))
		makeObjDead();
}

void TBigWatermelon::touchWaterSurface()
{
	emitColumnWater();
	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_DRINA_TO_WATER, &mPosition);
	kill();
}

void TBigWatermelon::touchWall(JGeometry::TVec3<f32>* param_1,
                               TBGWallCheckRecord* param_2)
{
	TMapObjBall::touchWall(param_1, param_2);
}

void TBigWatermelon::rebound(JGeometry::TVec3<f32>* param_1)
{
	// A second bounce while already rotting bursts it.
	if (isState(STATE_ROTTING)) {
		kill();
		*param_1 = mPosition;
		return;
	}

	TMapObjBall::rebound(param_1);

	if (isState(STATE_LIVING))
		mState = STATE_ROTTING;
}

void TBigWatermelon::touchGround(JGeometry::TVec3<f32>* param_1)
{
	TMapObjBall::touchGround(param_1);
}

// TMapObjBall::touchActor expands here at depth 1 once its guards are one
// `||` chain; one level down (TResetFruit::pick) it stays a call.
void TBigWatermelon::touchActor(THitActor* param_1)
{
	if (isState(STATE_APPEARING))
		return;

	// Once it is falling, touching anything at all bursts it.
	if (!isState(STATE_NORMAL)) {
		// Assigned, not copy-constructed: retail's isZero keeps its three
	// fmuls apart (no fmadds), which only operator='s cast copy gives.
	JGeometry::TVec3<f32> vel;
	vel = getVelocity();
		if (vel.y < 0.0f) {
			kill();
			return;
		}
	}

	if (param_1->isActorType(0x80000001)) {
		// TODO: 97.3%. The ROM batches the fourth component load before the
		// first fsubs here; distance()'s doubled subtraction, a named
		// squared() and sqrt(squared(other)) all schedule it later. The
		// frame is also 16 bytes short after getVelocity() above. A named
		// `TVec3 diff; diff.sub(mPosition, param_1->mPosition);` tested by
		// diff.length() gives the exact frame and vel slots, but then fuses
		// the squares into fmadds (95.9%); retail keeps three fmuls.
		if (mPosition.distance(param_1->mPosition) < 0.6f * mBodyRadius) {
			kill();
			return;
		}
	}

	// A moving poihana bounces it back up instead.
	if (param_1->isActorType(0x10000015) && ((TPoiHana*)param_1)->isMoving()) {
		if (abs(mVelocity.y) < getMapObjData()->getPhysicalData()->unkC) {
			mVelocity.y += 30.0f;
			mState = STATE_LIVING;
		}
		return;
	}

	TMapObjBall::touchActor(param_1);
}

// Binding level over a raw member read, worth +16 of low region in
// TBigWatermelon::kill (batch 127).
static inline TWaterEmitInfo* MapObjBallUnk198(const TBigWatermelon* p)
{
	TWaterEmitInfo* v198 = p->unk198;
	return v198;
}

void TBigWatermelon::kill()
{
	emitAndScale(0x5D, 0, &mPosition);
	emitAndScale(0x5E, 0, &mPosition);
	emitAndScale(0x5F, 0, &mPosition);

	JGeometry::TVec3<f32> scale(1.0f, 1.0f, 1.0f);
	emitAndScale(0x6B, 0, &mPosition, scale);
	emitAndScale(0x6C, 0, &mPosition, scale);

	// Splash the juice through the water manager.
	MapObjBallUnk198(this)->mPos.value = mPosition;
	gpModelWaterManager->emitRequest(*unk198);

	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_WATERMELON_BLOCK, &mPosition, 0,
	                                nullptr, 0, 4);

	// Each burst drops one coin, up to ten over the object's lifetime.
	if (unk19C < 10) {
		TMapObjBase* coin = gpItemManager->makeObjAppear(
		    mPosition.x, mPosition.y, mPosition.z, 0x2000000E, true);
		if (coin) {
			coin->mVelocity.x = 0.0f;
			coin->mVelocity.y = 25.0f;
			coin->mVelocity.z = 0.0f;
			coin->offLiveFlag(LIVE_FLAG_UNK10);
			unk19C++;
		}
	}

	TMapObjGeneral::kill();
}

static inline J3DModel* BigWatermelonModel(const TBigWatermelon* p)
{
	J3DModel* model = p->getModel();
	return model;
}

static inline MtxPtr BigWatermelonAnmMtx(const TBigWatermelon* p)
{
	MtxPtr mtx = BigWatermelonModel(p)->getAnmMtx(0);
	return mtx;
}

void TBigWatermelon::appearing()
{
	TMapObjGeneral::appearing();

	MtxPtr mtx = BigWatermelonAnmMtx(this);
	calcRootMatrix();
	BigWatermelonModel(this)->calc();
	mtx[1][3] = mBodyRadius * (mScaling.y / mInitialScaling.y) + mPosition.y;

	mScaledBodyRadius = 50.0f * mScaling.x;
	mDamageRadius     = 50.0f * mScaling.x;
	calcEntryRadius();

	// Only once it has finished growing does it become the crushing type.
	if (isState(STATE_NORMAL)) {
		mActorType    = 0x400000D0;
		mAttackRadius = 50.0f * mScaling.x;
		calcEntryRadius();
		return;
	}

	mActorType    = 0x400000DB;
	mAttackRadius = 0.0f;
	calcEntryRadius();
}

void TBigWatermelon::control()
{
	JGeometry::TVec3<f32> scale;
	JGeometry::TVec3<f32> vel;
	Mtx held;

	TMapObjGeneral::control();
	if (unk194 != 0)
		unk194 -= 1;

	if (isState(STATE_HOLDING)) {
		MTXCopy(mHolder->getTakingMtx(), held);
		held[1][3] += unk190;
		MtxPtr anm = getModel()->getAnmMtx(0);
		MTXCopy(held, anm);
	} else {
		vel = getVelocity();
		if (!vel.isZero() || getGroundPlane()->getActor())
			calcCurrentMtx();
	}

	switch (mState) {
	case STATE_NORMAL:
		if (checkLiveFlag(LIVE_FLAG_UNK10))
			offLiveFlag(LIVE_FLAG_UNK10);

		{
			// Sitting on a rising sand pillar lifts the watermelon with it,
			// the same way TResetFruit::control does.
			const TLiveActor* owner = getGroundPlane()->getActor();
			if (mPosition.y < getGroundHeight() + 200.0f && owner) {
				// TODO: the original tests the same type twice here, as it
				// also does in TResetFruit::control.
				if (owner->isActorType(0x400000CD)
				    || owner->isActorType(0x400000CD)) {
					f32 wasRatio = unk1A0;
					unk1A0       = SMS_GetSandRiseUpRatio(owner);
					if (unk1A0 > 0.05f && unk1A0 > wasRatio)
						mVelocity.y += 20.0f;
				}
			}
		}
		break;

	case STATE_APPEARING:
	case STATE_WAITING_TO_APPEAR:
	case STATE_LIVING:
	case STATE_ROTTING:
		break;

	case STATE_BROKEN:
		if (!isStateTimerEngaged()) {
			scale.set(1.0f, 1.0f, 1.0f);
			emitAndScale(0x6B, 0, &mPosition, scale);
			emitAndScale(0x6C, 0, &mPosition, scale);
			mStateTimer = 30;
		}

		if (animIsFinished())
			makeObjDead();
		break;
	}
}

void TBigWatermelon::startEvent()
{
	// Only the one big watermelon on the Sirena roof runs the shine demo;
	// the others just burst into coins.
	if (strcmp(getName(), "スイカ（大）") == 0) {
		mPosition.x = -4660.0f;
		mPosition.y = 1300.0f;
		mPosition.z = 13600.0f;

		offMapObjFlag(MAP_OBJ_FLAG_UNK100);
		onLiveFlag(LIVE_FLAG_UNK10);
		mVelocity.x = mVelocity.y = mVelocity.z = 0.0f;
		onLiveFlag(LIVE_FLAG_UNK10);
		startAnim(7);

		SMSGetMarDirector()->fireStartDemoCamera("スイカゴールカメラ",
		                                         &mPosition, -1, 0.0f, true,
		                                         nullptr, 0, nullptr,
		                                         JDrama::TFlagT<u16>(0));
		gpItemManager->makeShineAppearWithDemoOffset(
		    "シャイン（お化けスイカ用）", "スイカシャインカメラ", 0.0f, 0.0f,
		    0.0f);

		mStateTimer = 380;
		mState      = STATE_BROKEN;
		return;
	}

	for (int i = 0; i < 10; ++i) {
		const JGeometry::TVec3<f32>& marioPos = SMS_GetMarioPos();
		TCoin* coin = (TCoin*)gpItemManager->makeObjAppear(
		    marioPos.x, marioPos.y, marioPos.z, 0x2000000E, true);
		if (coin) {
			coin->mVelocity.set(20.0f * (MsRandF() - 0.5f),
			                    20.0f * MsRandF() + 20.0f,
			                    20.0f * (MsRandF() - 0.5f));
			coin->offLiveFlag(LIVE_FLAG_UNK10);
			coin->unk14C = 0x3C0;
		}
	}

	makeObjDead();
}

void TBigWatermelon::checkWallCollision(JGeometry::TVec3<f32>* param_1)
{
	TMapObjGeneral::checkWallCollision(param_1);
}

BOOL TBigWatermelon::receiveMessage(THitActor* sender, u32 message)
{
	// Mario always bounces off the big watermelon, whatever the message.
	if (sender->isActorType(0x80000001)) {
		boundByActor(sender);
		return TRUE;
	}

	if (TMapObjGeneral::receiveMessage(sender, message))
		return TRUE;

	if (message == HIT_MESSAGE_TAKE && (unkF8 & 0x100000)) {
		hold((TTakeActor*)sender);
		return TRUE;
	}

	if (sender->isActorType(0x80000001)) {
		if (!isActorType(0x400000D0) && message != HIT_MESSAGE_TAKE) {
			kicked();
			return TRUE;
		}
	}

	return FALSE;
}

void TBigWatermelon::loadAfter()
{
	TMapObjGeneral::loadAfter();

	// Park the shine that belongs to this watermelon at its fixed spot.
	JDrama::TActor* shine
	    = JDrama::TNameRefGen::search<JDrama::TActor>("シャイン（お化けスイカ用）");
	shine->mPosition.x = -4659.0f;
	shine->mPosition.y = 460.0f;
	shine->mPosition.z = 13620.0f;
}

void TBigWatermelon::initMapObj()
{
	TMapObjBall::initMapObj();

	SMS_LoadParticle("/scene/mapObj/watermelon_bomb.jpa", 0x5D);
	SMS_LoadParticle("/scene/mapObj/watermelon_bomb_a.jpa", 0x5E);
	SMS_LoadParticle("/scene/mapObj/watermelon_bomb_b.jpa", 0x5F);
	SMS_LoadParticle("/scene/mapObj/watermelon_shrink_a.jpa", 0x6B);
	SMS_LoadParticle("/scene/mapObj/watermelon_shrink_b.jpa", 0x6C);

	unk198 = new TWaterEmitInfo("/watermelon.prm");
}

TBigWatermelon::TBigWatermelon(const char* name)
    : TMapObjBall(name)
{
	unk198 = 0;
	unk19C = 0;
	unk1A0 = 0.0f;
}
