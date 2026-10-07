#include <Enemy/KukkuNerve.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/PathNode.hpp>
#include <Strategic/LiveActor.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Strategy.hpp>
#include <Animal/AnimalBase.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/TexUtil.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/PollutionManager.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <Player/MarioAccess.hpp>
#include <System/Application.hpp>
#include <System/Particles.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>

// rogue includes needed for matching sinit & bss
#include <M3DUtil/InfectiousStrings.hpp>
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// The six .bck slots of the tori model in the alphabetical order the model
// data indexes them. Slot 2 has no .bas, and the only name that sorts between
// "tori_down" and "tori_fall_end" is "tori_fall".
// TODO: slot 2's name is deduced from the alphabetical rule, not read from the
// model data.
enum {
	KUKKU_ANM_BACK     = 0,
	KUKKU_ANM_DOWN     = 1,
	KUKKU_ANM_FALL     = 2,
	KUKKU_ANM_FALL_END = 3,
	KUKKU_ANM_HIT      = 4,
	KUKKU_ANM_WAIT     = 5,
};

static const char* tori_bastable[] = {
	"/scene/tori/bas/tori_back.bas",
	"/scene/tori/bas/tori_down.bas",
	nullptr,
	"/scene/tori/bas/tori_fall_end.bas",
	"/scene/tori/bas/tori_hit.bas",
	"/scene/tori/bas/tori_wait.bas",
};

namespace {
// Indexed 1..3, so entry 0 is never read.
const int cDropCoinNumTable[] = { 3, 3, 1, 2 };
}

// UNUSED, 0x70 in the map.
TKukkuBall::TKukkuBall(MActor* actor)
    : THitActor("\x83\x4e\x83\x62\x83\x4e\x8b\xca")
    , mMActor(actor)
    , mFlags(KUKKUBALL_FLAG_DEAD)
    , unk7C(0)
{
}

void TKukkuBall::init()
{
	initHitActor(0x1000002E, 1, -0x80000000, 30.0f, 30.0f, 0.0f, 0.0f);

	onHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);

	TIdxGroupObj* group = JDrama::TNameRefGen::search<TIdxGroupObj>(
	    "\x93\x47\x83\x4f\x83\x8b\x81\x5b\x83\x76");
	group->getChildren().push_back(this);

	ResTIMG* image = (ResTIMG*)JKRFileLoader::getGlbResource(
	    "/scene/map/pollution/H_ma_rak.bti");
	if (image) {
		J3DModelData* data = mMActor->getModel()->getModelData();
		SMS_ChangeTextureAll(data, "K_name_dummy", *image);
	}
}

// TODO: 97.2%. Frame is 0x98 against retail 0xb0 (bind()'s pos sits 0x20
// low, mtx 0x18 low), and retail loads mAttackHeight before pos.y for both
// map calls in bind(). `pos += mVelocity`, pos.add(), an aggregate sum and a
// separate `groundY += 1.0f` are inert or worse.
void TKukkuBall::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (mFlags & KUKKUBALL_FLAG_DEAD)
		return;

	if (cue & CUE_CALC_ANIM) {
		JGeometry::TPosition3<
		    JGeometry::TMatrix34<JGeometry::SMatrix34C<f32> > >
		    mtx;
		// Spelled out: behind translation() the identity33() expansion would be
		// one level deeper and retail keeps it inline.
		mtx.identity33();
		mtx.setTrans(mPosition);
		mMActor->getModel()->setBaseScale(getScaling());
		MTXCopy(mtx, mMActor->getModel()->getBaseTRMtx());
		mMActor->getModel()->calc();
	}

	if (cue & CUE_MOVE) {
		checkHitActors();
		moveObject();
		bind();
	}

	if (!(mFlags & KUKKUBALL_FLAG_NO_DRAW))
		mMActor->perform(cue, graphics);
}

// UNUSED, 0x3c in the map.
void TKukkuBall::moveObject()
{
	mVelocity.y -= 0.9f;
	mVelocity.scale(0.94f);
}

// UNUSED, 0x128 in the map: the mud ball's own one-step integrator, with a
// ground test that kills it and a wall test that slides it.
void TKukkuBall::bind()
{
	JGeometry::TVec3<f32> pos(mPosition);
	pos.x += mVelocity.x;
	pos.y += mVelocity.y;
	pos.z += mVelocity.z;

	const TBGCheckData* ground;
	f32 groundY
	    = gpMap->checkGround(pos.x, pos.y + mAttackHeight, pos.z, &ground)
	    + 1.0f;
	if (pos.y <= 0.05f + groundY)
		kill();

	gpMap->isTouchedOneWallAndMoveXZ(&pos.x, pos.y + mAttackHeight, &pos.z,
	                                 mAttackRadius);

	mPosition = pos;
}

// UNUSED, 0xc0 in the map.
void TKukkuBall::checkHitActors()
{
	THitActor** end = &mCollisions[mColCount];
	for (THitActor** col = mCollisions; col != end; col++) {
		switch ((*col)->mActorType) {
		case 0x80000001:
			SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
			kill();
			break;
		}
	}
}

// UNUSED, 0x4c in the map.
void TKukkuBall::startToMove(const JGeometry::TVec3<f32>& position,
                             const JGeometry::TVec3<f32>& velocity)
{
	offHitFlag(HIT_FLAG_NO_COLLISION);
	mFlags &= ~KUKKUBALL_FLAG_DEAD;
	mPosition = position;
	mVelocity = velocity;
}

// UNUSED, 0x50 in the map.
void TKukkuBall::kill()
{
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mFlags |= KUKKUBALL_FLAG_DEAD;
	gpPollution->pollute(mPosition.x, mPosition.y, mPosition.z, 500.0f);
}

// UNUSED, 0x28 in the map.
bool TKukkuBall::isDead() const
{
	return (mFlags & KUKKUBALL_FLAG_DEAD) && unk7C == 0;
}

// UNUSED, 0x44 in the map. TODO: fabricated; nothing in the retail code
// references this class, so only the three names and their sizes are evidence.
TEnemyCoinUnit::TEnemyCoinUnit(int num)
    : mItemNum(num)
{
	mItems = new TMapObjBase*[num];
}

// UNUSED, 0xa8 in the map. TODO: fabricated, see the constructor.
void TEnemyCoinUnit::init()
{
	for (int i = 0; i < mItemNum; i++)
		mItems[i] = TMapObjBaseManager::newAndRegisterObj("coin");
}

// UNUSED, 0x48 in the map. TODO: fabricated, see the constructor.
TMapObjBase* TEnemyCoinUnit::getUnusedItem()
{
	for (int i = 0; i < mItemNum; i++)
		if (mItems[i]->checkLiveFlag(LIVE_FLAG_DEAD))
			return mItems[i];
	return nullptr;
}

TKukku::TKukku(const char* name)
    : TSmallEnemy(name)
    , mOneUp(nullptr)
{
}

void TKukku::init(TLiveManager* live_manager)
{
	mManager = live_manager;
	mManager->manageActor(this);

	mMActorKeeper = new TMActorKeeper(mManager, 4);
	mMActor       = mMActorKeeper->createMActor("tori.bmd", 0);

	mSpine->initWith(&TNerveKukkuGraphWander::theNerve());

	mOneUp = TMapObjBaseManager::newAndRegisterObj("mushroom1upR");

	initBalls();
	initCollision();
	initAnmSound();
	initParticle();

	mCenterJointIndex
	    = getModel()->getModelData()->getJointName()->getIndex("center");

	reset();
}

// UNUSED, 0xc4 in the map.
void TKukku::initBalls()
{
	for (TKukkuBall** ball = mBalls; ball != &mBalls[3]; ball++) {
		*ball = new TKukkuBall(
		    mMActorKeeper->createMActor("torifun.bmd", 3));
		(*ball)->init();
	}
}

// UNUSED, 0x58 in the map.
void TKukku::initCollision()
{
	initHitActor(0x1000002E, 1, -0x80000000, 30.0f, 30.0f, 100.0f, 100.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);
}

// UNUSED, 0x8c in the map.
void TKukku::initParticle()
{
	SMS_LoadParticle("/scene/tori/jpa/ms_cooc_ase.jpa", 0x18c);
	SMS_LoadParticle("/scene/tori/jpa/ms_cooc_hane.jpa", 0x18d);
}

void TKukku::reset()
{
	mHitTimer         = 0;
	mShootTimer       = 0;
	mGravity          = 0.0f;
	mDroppedCoins     = 0;
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	mScaledBodyRadius = 75.0f;
}

BOOL TKukku::receiveMessage(THitActor* sender, u32 message)
{
	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return FALSE;

	switch (message) {
	case HIT_MESSAGE_TRAMPLE:
	case HIT_MESSAGE_HIP_DROP:
		behaveHitTrample();
		return TRUE;
	default:
		return TSmallEnemy::receiveMessage(sender, message);
	}
}

void TKukku::control()
{
	if (getHitTimer() > 0)
		mHitTimer--;

	TLiveActor::control();
}

// The product is the two-argument tilt.mul(tilt, yaw) and the copy out is
// getModel()->setBaseTRMtx(mtx) (the `mr r4, r3` and &mtx in r30).
// TODO: 98.8%. Left: the up-vector FPR colouring (retail x/y/z in
// f25/f27/f28, ours f29/f28/f27) and a low region 0x10 short, which the old
// four-local mul(a, b) body used to fill; the one-argument tilt.mul(yaw) fills
// the frame but schedules the product differently (96.0). Inert for up:
// `up = normal`, set(n.x, n.y, n.z), normalize(normal) and setLength(normal,
// 1) (worse), else-first, a (0,1,0) initialiser (worse).
void TKukku::calcRootMatrix()
{
	if (isDying()) {
		// Dead: lie flat against whatever it landed on instead of using the
		// spine enemy's upright matrix.
		JGeometry::TVec3<f32> up;
		if (mGroundPlane) {
			// Copying the normal into the local first is what lets the
			// squared length contract into fmadds, as retail does.
			up.set(mGroundPlane->getNormal());
			up.normalize();
		} else {
			up.set(0.0f, 1.0f, 0.0f);
		}

		JGeometry::TQuat4<f32> yaw;
		yaw.setEulerY(0.017453294f * mRotation.y);

		JGeometry::TQuat4<f32> tilt;
		tilt.setRotate(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f), up,
		               JGeometry::TUtil<f32>::one());
		tilt.mul(tilt, yaw);

		JGeometry::TPosition3<
		    JGeometry::TMatrix34<JGeometry::SMatrix34C<f32> > >
		    mtx;
		// setQT() is the one-line forwarder that keeps setQuat() a `bl`.
		mtx.setQT(tilt, mPosition);

		getModel()->setBaseTRMtx(mtx);
		getModel()->setBaseScale(getScaling());
	} else {
		TSpineEnemy::calcRootMatrix();
	}

	updateEffect();
}

void TKukku::bind() { TLiveActor::bind(); }

void TKukku::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);

	for (TKukkuBall** ball = mBalls; ball != &mBalls[3]; ball++)
		(*ball)->perform(cue, graphics);
}

// UNUSED, 0x130 in the map: the sweat trail while falling and the wing puff
// while flapping backwards.
void TKukku::updateEffect()
{
	if (isFalling())
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x18d, getModel()->getAnmMtx(mCenterJointIndex), 1, this);

	if (getMActor()->checkCurAnm("tori_back", ANM_TYPE_BCK))
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x18c, getModel()->getAnmMtx(mCenterJointIndex), 1, this);
}

void TKukku::behaveToWater(THitActor* water)
{
	if (isFalling())
		return;

	if (isRecoveringGraph())
		return;

	getSpine()->reset();
	getSpine()->setNext(&TNerveKukkuFall::theNerve());
}

// UNUSED, 0x58 in the map.
void TKukku::behaveHitTrample()
{
	mSpine->reset();
	mSpine->setNext(&TNerveSmallEnemyDie::theNerve());
}

// UNUSED, 0x2c8 in the map. TODO: fabricated. The GraphWander nerve holds
// this code inline, and nothing distinguishes a pasted body from a dead
// helper; calling this from the nerve puts shotBall one level deeper, where
// it stops expanding (c-k14 measured GraphWander 56.4%). This body is the
// nerve's goal and shooting blocks without the steering, which compiles to
// the map size; with the steering (updateRotation plus
// updateLinearVelocity, or doRecoverToCurPathNode) every spelling of the
// shoot timer measured is 0x20 or more over (c-k16). The post-decrement
// test is also a size choice: the nerve's own `if (timer >= 0) timer--; else
// shotBall();` here is 0x2cc (raw member) or 0x2d0 (getShootTimer()).
void TKukku::doFlyToCurPathNode()
{
	if (isReachedToGoal()) {
		goToRandomNextGraphNode();
		if (!getMActor()->checkCurAnm("tori_wait", ANM_TYPE_BCK))
			changeBck("tori_wait");
	}

	if (isFindOutMario()) {
		if (mShootTimer-- < 0)
			shotBall();
	}
}

// Instruction-exact (99.8%); the residual is a 0x18 frame excess against the
// target's 0x70, which is the same excess the twelve-statement version had.
//
// Retail *calls* this at depth 1 (`mr r3, r30; bl updateRotation`), so by the
// statement budget in docs/catalog/codegen-tells.md its body needed 15 counted
// statements. The three that were missing are the ones written out below, and
// all three are readable off the asm rather than guessed:
//   - `length()` split into `squared()` plus TUtil<f32>::sqrt: the ROM's three
//     `fmuls`/two `fadds`, the `fcmpo 0.0f` guard and the single Newton step
//     are exactly that pair, and the split is also worth 8 bytes of frame.
//   - the two banking factors named per component, because the ROM expands
//     isFalling() twice (two `getLatestNerve` reads and four theNerve()
//     calls) instead of reusing one value.
// Together they take TNerveKukkuRecoverGraph::execute 0.0 -> 67.7% and
// TNerveKukkuGraphWander::execute 42.0 -> 92.9%.
//
// Measured and rejected: naming `0.5f * dist` inside the branch instead of one
// of the above (14 statements, both nerves back to 0.0/42.0); splitting the
// two parameter fetches into declaration plus assignment (no effect at all);
// keeping `length()` with the two bank locals (nerves back to 0.0/42.0 *and*
// frame 0x90).
//
// The last 0x18 of frame: each `TParamRT::get()` reference temporary is 0x10
// of pool, so retail read one speed raw and the other through the plain
// cast (0x88 -> 0x78; both raw is 0x68), and the two bank factors are one
// reassigned local (the second named `f32` was the extra 4-byte slot).
void TKukku::updateRotation()
{
	JGeometry::TVec3<f32> toGoal(getUnkF4().getPoint());
	toGoal.sub(mPosition);

	f32 distSq = toGoal.squared();
	f32 dist   = JGeometry::TUtil<f32>::sqrt(distSq);
	if (dist < 100.0f)
		return;

	f32 marchSpeed = getSaveParams()->mMarchSpeed.value;
	f32 turnSpeed  = ((TKukkuParams*)getSaveParam())->getTurnSpeed();

	if (dist <= 2.0f * calcMinimumTurnRadius(marchSpeed, turnSpeed))
		turnSpeed = calcTurnSpeedToReach(marchSpeed, 0.5f * dist);

	TAnimalBase::getRotationFlyToDir(&mRotation, toGoal, marchSpeed,
	                                 turnSpeed);

	// A falling gull keeps the yaw it had but loses its banking; retail
	// evaluates isFalling() once per component.
	f32 bank = isFalling() ? 0.0f : 1.0f;
	mRotation.x *= bank;
	bank = isFalling() ? 0.0f : 1.0f;
	mRotation.z *= bank;
}

// 98.9%, frame exact at 0x78, all 71 opcodes in place; the residue is the
// order of the q.x/q.z products inside the expanded rotate (9 markers).
//
// c-k14: the three-statement body is retail's. The fifteen-statement body it
// replaces was only there to keep calcMomentum out of line in
// TNerveKukkuRecoverGraph; the statement-mode reading of the two nerves
// (docs/catalog/codegen-tells.md, c-r24) puts that refusal on a missing
// level instead (TKukku::updateLinearVelocity): calcMomentum is judged at
// level 3 in TNerveKukkuGraphWander, where it expands and leaves the TVec4
// copy, set<f> and rotate as `bl`s one level below, and at level 4 through
// doRecoverToCurPathNode, where it is called.
JGeometry::TVec3<f32> TKukku::calcMomentum(f32 speed)
{
	JGeometry::TQuat4<f32> quat = SMS_Eular2Quat(mRotation);
	JGeometry::TVec3<f32> velocity(0.0f, 0.0f, speed);
	quat.rotate(velocity, velocity);
	return velocity;
}

// UNUSED, 0xa0 in the map; TNerveKukkuRecoverGraph inlines it. Ours is
// 0xb8: the 0x18 over is the extra copy of calcMomentum's return value
// described at TNerveKukkuGraphWander. With the uncast TVec3 `operator=`
// (the cc23 header migration, docs/catalog/frame-gaps.md) it is 0xa0.
void TKukku::doRecoverToCurPathNode()
{
	updateRotation();
	updateLinearVelocity();
}

// UNUSED, 0xd8 in the map. TODO: fabricated. "Habataki" is flapping, and
// mHabatakiTimer is what the recovery nerves count against: the gull keeps
// its heading and flaps forward until the timer runs out. This compiles to
// the map size with our TVec3 header, whose return copy (see
// doRecoverToCurPathNode) is 0x18 of it; under the eliding header it is
// 0xc0, and `changeBck("tori_back")` before the store is then the body that
// measures 0xd8 (c-k16).
void TKukku::doHabataki()
{
	if (getSaveParams()->getHabatakiTimer() < mSpine->getTime())
		return;

	updateLinearVelocity();
}

// UNUSED, 0x7c in the map.
void TKukku::decideFlyingAnm()
{
	if (getMActor()->checkCurAnm("tori_back", ANM_TYPE_BCK))
		changeBck("tori_back");
	else
		changeBck("tori_wait");
}

// UNUSED, 0x20c in the map: spit one of the three mud balls straight up, with
// the joint lookup the original left in and never used.
void TKukku::shotBall()
{
	TKukkuBall* ball = getUnusedBall();
	if (!ball)
		return;

	JGeometry::TVec3<f32> velocity(0.0f, 1.0f, 0.0f);
	velocity.setLength(getSaveParams()->getShootSpeed());

	getModel()->getModelData()->getJointName()->getIndex("null_osen");

	JGeometry::TVec3<f32> position(mPosition);
	ball->startToMove(position, velocity);

	mShootTimer = getSaveParams()->getShootInterval();

	gpMSound->startSoundActor(MSD_SE_EN_TORI_SHIT, &mPosition, 0, nullptr, 0,
	                          4);
}

// `forward` is built one inline level down: that is what makes retail `bl`
// TVec3::set<f> (the unit's weak copy) while the sine/cosine lookups expand,
// and the argument order puts mRotation.y in f26 before getDropSpeed().
static inline JGeometry::TVec3<f32> KukkuVecFromRotY(f32 length, f32 rot_y)
{
	f32 x = length * JMASin(rot_y);
	f32 z = length * JMACos(rot_y);
	return JGeometry::TVec3<f32>(x, 0.0f, z);
}

// The rotates are rotateQ(), the one-level member-read body JGQuat4.hpp's
// TODO proposes for rotate(v, rDest): frame 0x1f0 -> 0x168 (retail 0x158),
// 86.4 -> 86.5. Spell them rotate() again once that header change lands.
// Both translations read mPosition raw: getPosition() kept &mPosition in
// r28 across the coin loop, and the copy-constructed velocity was 0x10 of
// frame.
void TKukku::dropCoins()
{
	if (mDroppedCoins > 10)
		return;

	// The eleventh drop is the 1UP the gull was carrying.
	if (mDroppedCoins == 10 && mOneUp) {
		mDroppedCoins++;
		mOneUp->appear();
		mOneUp->JSGSetTranslation(mPosition);
		// Retail reloads mOneUp before each of the three calls above (they
		// clobber it) but holds it across the three velocity stores and the
		// flag clear, which needs a pointer local declared exactly here: the
		// member spelling reloads between the stores, and .set(0,0,0) turns
		// the first store into an `stfsu` that costs the register.
		TMapObjBase* oneUp = mOneUp;
		oneUp->mVelocity.x = 0.0f;
		oneUp->mVelocity.y = 0.0f;
		oneUp->mVelocity.z = 0.0f;
		oneUp->offLiveFlag(LIVE_FLAG_UNK10);
		return;
	}

	int index = (int)(4.0f * MsRandF());
	if (index < 1)
		index = 1;
	else if (index > 3)
		index = 3;
	int coinNum = cDropCoinNumTable[index];

	JGeometry::TQuat4<f32> spin;
	spin.setEulerY(6.2831855f / (f32)coinNum);

	JGeometry::TQuat4<f32> pitch;
	pitch.setEulerX(3.1415927f * getSaveParams()->getDropAngleX());

	JGeometry::TVec3<f32> velocity
	    = KukkuVecFromRotY(getSaveParams()->getDropSpeed(), mRotation.y);
	pitch.rotateQ(velocity, velocity);
	spin.rotateQ(velocity, velocity);

	for (int i = 0; i < coinNum; i++) {
		TMapObjBase* coin = gpItemManager->makeObjAppear(0x2000000E);
		if (!coin)
			break;

		coin->appear();
		coin->JSGSetTranslation(mPosition);
		coin->mVelocity.set(velocity);
		coin->offLiveFlag(LIVE_FLAG_UNK10);

		if (++mDroppedCoins == 10)
			break;

		spin.rotateQ(velocity, velocity);
	}
}

// UNUSED, 0xac in the map.
bool TKukku::isFalling() const
{
	const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();
	return nerve == &TNerveKukkuFall::theNerve()
	    || nerve == &TNerveKukkuPostFall::theNerve();
}

// UNUSED, 0x98 in the map.
bool TKukku::isRecoveringGraph() const
{
	if (mSpine->getLatestNerve() != &TNerveKukkuRecoverGraph::theNerve())
		return false;
	return true;
}

// UNUSED, 0x4c in the map. TODO: dead and fabricated.
bool TKukku::isDying() const
{
	return mSpine->getLatestNerve() == &TNerveSmallEnemyDie::theNerve();
}

// UNUSED, 0xb4 in the map: Mario is within the search range, measured flat.
bool TKukku::isFindOutMario() const
{
	JGeometry::TVec3<f32> toMario(*gpMarioPos);
	toMario.sub(mPosition);
	toMario.y = 0.0f;

	f32 range = getSaveParams()->getSearchRange();
	return toMario.squared() < range * range;
}

// UNUSED, 0x38 in the map.
void TKukku::changeBck(const char* name)
{
	getMActor()->setBck(name);
	setCurAnmSound();
}

void TKukku::setDeadAnm() { changeBck("tori_down"); }

void TKukku::setAfterDeadEffect()
{
	TSmallEnemy::setAfterDeadEffect();
	gpPollution->stamp(((TSmallEnemyManager*)mManager)->getUnk58(),
	                   mPosition.x, mPosition.y, mPosition.z, 1000.0f);
}

// UNUSED, 0x8 in the map. TODO: two instructions, so it returns a constant,
// but nothing in the binary says which one.
f32 TKukku::getWaterDamageRate() const { return 1.0f; }

// UNUSED, 0x38 in the map.
f32 TKukku::getWaterPowerY() const
{
	return getSaveParams()->getWaterPowerY();
}

// UNUSED, 0x4c in the map.
TKukkuBall* TKukku::getUnusedBall()
{
	for (TKukkuBall** ball = mBalls; ball != &mBalls[3]; ball++)
		if ((*ball)->isDead())
			return *ball;
	return nullptr;
}

// UNUSED, 0x250 in the map.
TKukkuParams::TKukkuParams(const char* prm)
    : TSmallEnemyParams(prm)
    , PARAM_INIT(mMarchSpeed, 3.0f)
    , PARAM_INIT(mTurnSpeed, 0.2f)
    , PARAM_INIT(mWaterPowerY, 12.0f)
    , PARAM_INIT(mShootSpeed, 1.0f)
    , PARAM_INIT(mShootInterval, 60)
    , PARAM_INIT(mSearchRange, 800.0f)
    , PARAM_INIT(mHabatakiTimer, 45)
    , PARAM_INIT(mAirFric, 0.97f)
    , PARAM_INIT(mUpperVelocityY, 0.0f)
    , PARAM_INIT(mDropSpeed, 5.0f)
    , PARAM_INIT(mDropAngleX, -0.25f)
{
	TParams::load(mPrmPath);
}

TKukkuManager::TKukkuManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TKukkuManager::load(JSUMemoryInputStream& stream)
{
	TKukkuParams* params = new TKukkuParams("/enemy/kukku.prm");
	unk38                = params;

	// The gull's collision is the same for every map, so the .prm never gets
	// to override these four.
	params->mSLAttackRadius.set(30);
	params->mSLAttackHeight.set(30);
	params->mSLDamageRadius.set(100);
	params->mSLDamageHeight.set(100);
	params->mSLBodyRadius.set(5.0f);

	TSmallEnemyManager::load(stream);
}

// UNUSED, 0x4 in the map.
void TKukkuManager::initCoins() { }

void TKukkuManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "tori.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

const char** TKukku::getBasNameTable() const { return tori_bastable; }

// TODO: 97.0%, every call decision right (SMS_Eular2Quat, TVec4's copy
// constructor, set<f> and rotate are `bl`s inside the expanded calcMomentum,
// as in retail). What is left is one copy: retail stores calcMomentum's
// `velocity` (0x74) straight into mLinearVelocity, where ours copies it into
// the return temporary first (six instructions, frame 0x100 against 0x108).
// The same six instructions are what doRecoverToCurPathNode is over its map
// size. Tried (c-k14): the store as a named TVec3 local, through
// setLinearVelocity, with the speed named first (frame exact here, but
// RecoverGraph's frame 8 over), and as `TVec3<f32>(calcMomentum(...))`.
// c-k16: the copy is the cc23 header class (docs/catalog/frame-gaps.md), not
// a site lever. With JGVec3.hpp's `operator=(const TVec3&)` uncast
// (`*(Vec*)this = other`) and nothing else changed, this nerve is 99.5%
// (246 instructions, all in place, frame 0x100 against 0x108),
// TNerveKukkuFall 97.0 -> 99.8, calcMomentum stays 98.9 and
// doRecoverToCurPathNode is the map's 0xa0. Declaring calcMomentum as
// returning `Vec` (the map does not mangle the return type) gives the same
// inline results but costs calcMomentum its copy constructor's 8 bytes of
// frame (98.9 -> 98.4), so retail returned a TVec3. Also inert:
// `mLinearVelocity.set(calcMomentum(...))` (this nerve 96.1, RecoverGraph
// 96.0).
DEFINE_NERVE(TNerveKukkuGraphWander, TLiveActor)
{
	TKukku* kukku = (TKukku*)spine->getBody();

	if (spine->getTime() == 0) {
		kukku->setVelocity(JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f));
		kukku->getTracer()->reset();
		kukku->goToShortestNextGraphNode();
		kukku->decideFlyingAnm();
	}

	if (kukku->isReachedToGoal()) {
		kukku->goToRandomNextGraphNode();
		if (!kukku->getMActor()->checkCurAnm("tori_wait", ANM_TYPE_BCK))
			kukku->changeBck("tori_wait");
	}

	if (kukku->isFindOutMario()) {
		if (kukku->getShootTimer() >= 0)
			kukku->mShootTimer--;
		else
			kukku->shotBall();
	}

	kukku->updateRotation();
	kukku->updateLinearVelocity();
	return FALSE;
}

// TODO: dead in retail (0xfc, with its vtable and destructor UNUSED too), so
// this body is a guess from the name and the tori_hit animation.
DEFINE_NERVE(TNerveKukkuHit, TLiveActor)
{
	TKukku* kukku = (TKukku*)spine->getBody();

	if (spine->getTime() == 0)
		kukku->changeBck("tori_hit");

	if (kukku->checkCurAnmEnd(ANM_TYPE_BCK)) {
		spine->pushAfterCurrent(&TNerveKukkuFall::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveKukkuFall, TLiveActor)
{
	TKukku* kukku = (TKukku*)spine->getBody();

	if (spine->getTime() == 0) {
		kukku->changeBck("tori_wait");
		J3DFrameCtrl* ctrl = kukku->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setRate(2.0f * SMSGetAnmFrameRate());
		kukku->setVelocity(JGeometry::TVec3<f32>(
		    0.0f, -kukku->getSaveParams()->getWaterPowerY(), 0.0f));
		kukku->dropCoins();
	}

	// checkLiveFlag(), not isAirborne(): retail branches on the flag directly
	// instead of materialising isAirborne()'s BOOL.
	if (!kukku->checkLiveFlag(LIVE_FLAG_AIRBORNE)) {
		SMS_EasyEmitParticle((E_SMS_EFFECT_ONETIME_NORMAL)0xa1,
		                     &kukku->mPosition, nullptr,
		                     JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
		SMS_EasyEmitParticle((E_SMS_EFFECT_ONETIME_NORMAL)0xa2,
		                     &kukku->mPosition, nullptr,
		                     JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
		spine->pushAfterCurrent(&TNerveSmallEnemyDie::theNerve());
		return TRUE;
	}

	// TODO: retail keeps this copy low in the frame (0x48, below the
	// temporaries of the first block) and stores the setVelocity temporary's
	// zero x before loading the water power. Inert: a named block vector
	// (ctor or set()), a named power scalar.
	JGeometry::TVec3<f32> velocity(kukku->getVelocity());
	velocity.scale(kukku->getSaveParams()->getAirFric());

	velocity.y += kukku->getSaveParams()->getUpperVelocityY();
	bool landed = false;
	if (-0.1f < velocity.y) {
		landed     = true;
		velocity.y = 0.0f;
	}
	kukku->setVelocity(velocity);

	if (landed) {
		spine->pushAfterCurrent(&TNerveKukkuRecoverGraph::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveKukkuPostFall, TLiveActor)
{
	TKukku* kukku = (TKukku*)spine->getBody();

	if (spine->getTime() == 0) {
		kukku->changeBck("tori_back");
		kukku->setVelocity(JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f));
	}

	if (kukku->checkCurAnmEnd(ANM_TYPE_BCK)
	    && kukku->getSaveParams()->getHabatakiTimer() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveKukkuGraphWander::theNerve());
		return TRUE;
	}

	return FALSE;
}

// The tail is the UNUSED doRecoverToCurPathNode() inlined: its
// calcMomentum return temporary is then a depth-1 callee object, created
// after the velocity temporary and the habataki read, which is retail's slot.
DEFINE_NERVE(TNerveKukkuRecoverGraph, TLiveActor)
{
	TKukku* kukku = (TKukku*)spine->getBody();

	if (spine->getTime() == 0) {
		kukku->changeBck("tori_back");
		kukku->setVelocity(JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f));
	}

	if (kukku->getSaveParams()->getHabatakiTimer() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveKukkuGraphWander::theNerve());
		return TRUE;
	}

	kukku->doRecoverToCurPathNode();
	return FALSE;
}
