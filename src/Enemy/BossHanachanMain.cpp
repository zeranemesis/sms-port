#include <Enemy/BossHanachan.hpp>
#include <Enemy/BossHanachanChangeSaveParams.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <Strategic/Binder.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/PathNode.hpp>
#include <Camera/CameraShake.hpp>
#include <Camera/cameralib.hpp>
#include <GC2D/GCConsole2.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <M3DUtil/MActor.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/MapUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/TexUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <NPC/NpcInbetween.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Strategic/ObjManager.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <System/MarDirector.hpp>
#include <System/TargetArrow.hpp>
#include <dolphin/mtx.h>
#include <math.h>

// rogue include: the original TU opens .rodata with the dummy string pair
// from System/DummyStrings.hpp (which this pulls in) followed by the four
// MActorMtxCalcType names; without both, every string offset in this object
// is shifted.
#include <M3DUtil/InfectiousStrings.hpp>
// rogue include: pulls in JALList.hpp's JSUList<T>::smList template statics,
// which is what marioEU.dol registers from __sinit_<TU>_cpp (see the same
// block in src/Enemy/BossHanachanNerve.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <MSound/MSModBgm.hpp>

// fabricated: the original .rodata prologue ends with twelve zero bytes and
// then three 1.0f values, whose statics have not been identified yet. They
// have to be here or every string offset in the object is shifted.
static const JGeometry::TVec3<f32> sUnknownZeroVec(0.0f, 0.0f, 0.0f);
static const JGeometry::TVec3<f32> sUnknownOneVec(1.0f, 1.0f, 1.0f);

// The five model/texture names live in .sdata in the original: they are read
// with a plain `lwz <sym>@sda21`, so they are pointers, not literals.
const char* cSandPillarModelName = "sunabashira.bmd";
const char* cHitPoint1_RailName   = "bosshanachan1";
const char* cHitPoint2_RailName   = "bosshanachan2";
const char* cSandTextureName      = "suna";
const char* cDummyTextureName     = "M_dummy";

// TODO: an 11 byte Japanese string ("...group") sits in the original .rodata
// between the two part-name strings and the common .prm name but nothing in
// this TU references it, so it is not reproduced here.

// NOTE: every function below is in REVERSE map order because this TU is
// built with -inline deferred, which emits the bodies in reverse source
// order. See tools/validate-symbol-order.py -u mario/Enemy/BossHanachanMain.

// ---------------------------------------------------------------------------
// TBossHanachanManager
// ---------------------------------------------------------------------------

TBossHanachanManager::TBossHanachanManager(const char* name)
    : TEnemyManager(name)
{
	static const char* sChangeSaveFileName[] = {
		"/enemy/bosshanachan0.prm",
		"/enemy/bosshanachan1.prm",
		"/enemy/bosshanachan2.prm",
	};
	static const char* sCommonSaveFileName = "/enemy/bosshanachanCommon.prm";

	mCommonSaveParams = new TBossHanachanCommonSaveParams(sCommonSaveFileName);
	for (int i = 0; i < 3; i++)
		mChangeSaveParams[i] = new TBossHanachanChangeSaveParams(
		    sChangeSaveFileName[i]);
}

void TBossHanachanManager::createModelData()
{
	// TODO: only the middle entry carries a model name in the original; the
	// other three .name pointers are left null.
	static TModelDataLoadEntry entry[] = {
		{ nullptr, 0x10300000, nullptr },
		{ cSandPillarModelName, 0x10100000, nullptr },
		{ nullptr, 0x10010000, nullptr },
		{ nullptr, 0, nullptr },
	};
	createModelDataArray(entry);
}

void TBossHanachanManager::loadAfter()
{
	// TODO: gpMapObjManager->unkC0 is a J3DMaterialTable*; the original walks
	// its +0x14 field (passed straight to JUTNameTab::getIndex) and its +0xC
	// field (dereferenced once more at +4) to reach the ResTIMG. None of
	// those fields is named in the SDK headers, so raw offsets are used.
	// TODO: the index is scaled by `<< 16 << 5` (MWCC fuses the pair into one
	// clrlslwi); that stride is not yet explained.
	u8* table = (u8*)gpMapObjManager->unkC0;
	s32 index = ((JUTNameTab*)(table + 0x14))->getIndex(cSandTextureName);
	ResTIMG* texture
	    = (ResTIMG*)(*(u32*)(*(u32*)(table + 0xC) + 4) + (index << 21));

	for (int i = 0; i < 2; i++) {
		SMS_ChangeTextureAll(getModelDataKeeper()->getNthData(i)->getModelData(),
		                     cDummyTextureName, *texture);
	}
}

void TBossHanachanManager::clipEnemies(JDrama::TGraphics* graphics)
{
	clipActorsAux(graphics, mCommonSaveParams->mSLViewClipFar.get(),
	              mCommonSaveParams->mSLViewClipRadius.get());
}

BOOL TBossHanachanManager::hasMapCollision() const { return true; }

// ---------------------------------------------------------------------------
// TBossHanachan
// ---------------------------------------------------------------------------

TBossHanachan::TBossHanachan(const char* name)
    : TSpineEnemy(name)
{
	mWeakBodyIndex    = 0;
	mSphereLink       = nullptr;
	unk17C.set(0.0f, 0.0f, 0.0f);
	unk188.set(0.0f, 0.0f, 0.0f);
	unk194            = 0.0f;
	unk198            = 0.0f;
	mSandPillar       = nullptr;
	unk1A0.set(0.0f, 0.0f, 0.0f);
	unk1AC.set(0.0f, 0.0f, 0.0f);
	unk1B8            = -1;
	mCommonSaveParams = nullptr;
	mChangeSaveParams = nullptr;
	setRandomWeakBodyIndex();
}

// TODO: J3DModel keeps its base matrix in the protected member unk20 and
// has no accessor for it; the original hands that address straight to
// CLBCalcRotateZXYTranslateMatrix. A real accessor belongs in the SDK header,
// which is off limits here.
static inline MtxPtr BHSModelMtx(J3DModel* model)
{
	return (MtxPtr)((u8*)model + 0x20);
}

static void CalcRevisionPosByRotateZ(const JGeometry::TVec3<f32>& rotation,
                                     f32 radius, Vec* out);

// TODO: this reproduces MWCC's signed-int -> f64 conversion idiom verbatim
// (the value is xored with 0x8000, i.e. the *high* halfword, combined with
// 0x43300000 in the high word and the 2^52 + 2^31 bias is then removed). The
// same trick appears in src/MoveBG/MapObjMare.cpp and src/Animal/BeeHive.cpp.
// TODO: it is spelled out inline rather than in a helper because the original
// emits no out-of-line copy of a converter here; a static helper would be
// inlined but still needs the right schedule.
void TBossHanachan::setRandomWeakBodyIndex()
{
	f64 biased;
	*((u32*)&biased + 1) = 0x43300000;
	*(u32*)&biased       = (u32)(rand() ^ 0x80000000);

	mWeakBodyIndex
	    = (int)(8.0f * (1.0f / 32768.0f * (f32)(biased - 4503601774854144.0)));
}

void TBossHanachan::init(TLiveManager* manager)
{
	mManager = manager;
	manager->manageActor(this);

	mMActorKeeper = new TMActorKeeper(manager, 10);
	mSandPillar   = mMActorKeeper->createMActor(cSandPillarModelName, 0);

	mCommonSaveParams = (TBossHanachanCommonSaveParams*)manager;
	mChangeSaveParams = (TBossHanachanChangeSaveParams*)manager;
	// TODO: the two params pointers are read out of TLiveManager at 0x54 and
	// 0x58; TLiveManager has not been given named accessors for them yet.

	mBodyScale         = 1.0f;
	mHeadHeight        = mCommonSaveParams->mSLViewClipRadius.get();
	mBodyRadius        = mHeadHeight;
	mScaledBodyRadius  = 500.0f;
	mMarchSpeed        = 0.0f;
	mGravity           = 2.0f;
	mHitPoints         = 0;
	mAngularVelocity.set(0.0f, 0.0f, 0.0f);
	mLiveFlag |= 0x1008;

	mSpine->initWith(&TNerveBossHanachanGraphWander::theNerve());

	getTracer()->mPrevIdx = -1;
	goToShortestNextGraphNode();

	initHitActor(0x80000014, 0x80000014, 0, 0.0f, 0.0f, 0.0f, 0.0f);
	mLiveFlag |= 1;

	for (int i = 0; i < 8; i++) {
		// The original hands the constructors a pointer into the middle of
		// these two literals; only the tail is used as the part name.
		mBody[i] = new TBossHanachanPartsBody(this, "ボスハナチャンの体" + 12);
		mBody[i]->mBodyIndex = i;
	}
	mHead = new TBossHanachanPartsHead(this, "ボスハナチャンの頭" + 12);

	// TODO: 0x74 is mMActor; the head's MActor is handed to the body.
	mMActor = mHead->mMActor;

	unk17C = mPosition;
	s16 angle = (s16)(mRotation.y * (65536.0f / 360.0f));
	f32 headLen = mCommonSaveParams->mSLHeadLength.get();
	JGeometry::TVec3<f32> headPos;
	headPos.x = unk17C.x - JMASSin(angle) * headLen;
	headPos.y = unk17C.y;
	headPos.z = unk17C.z - JMASCos(angle) * headLen;

	mSphereLink = new TSphereLink(8, headPos, 0.2f, -2.0f, -3.5f, headLen,
	                              mCommonSaveParams->mSLHeadPlusYByRotateZ
	                                  .get(),
	                              mRotation.y);

	mHead->mPosition        = mPosition;
	mHead->mRotation        = mRotation;
	mHead->mAngularVelocity = mAngularVelocity;
	mHead->mVelocity        = mVelocity;
	mHead->mGroundPlane     = mGroundPlane;

	// Each iteration swaps the body's two 12-byte vectors (three word copies
	// each way) and then overwrites the first one with the sphere point.
	for (int i = 0; i < 8; i += 2) {
		TBossHanachanPartsBody* b0 = mBody[i];
		TBossHanachanPartsBody* b1 = mBody[i + 1];
		b0->mPosition = mSphereLink->mPoints[i].unkC;
		b0->unk130    = b0->unk124;
		b0->unk124    = b0->mPosition;
		b0->mRotation = mRotation;
		b1->mPosition = mSphereLink->mPoints[i + 1].unkC;
		b1->unk130    = b1->unk124;
		b1->unk124    = b1->mPosition;
		b1->mRotation = mRotation;
	}

	setHeadAndBodyAnm(BH_ANM_KIND_UNK0, BH_STOP_MOTION_BLEND_OFF);

	JGeometry::TVec3<f32> headRevision;
	CalcRevisionPosByRotateZ(mRotation, headLen, &headRevision);
	CLBCalcRotateZXYTranslateMatrix(
	    BHSModelMtx(mHead->mMActor->getModel()), mRotation, headRevision);
	mHead->mMActor->calc();

	for (int i = 0; i < 8; i++) {
		TBossHanachanPartsBody* b = mBody[i];
		JGeometry::TVec3<f32> bodyRevision;
		CalcRevisionPosByRotateZ(b->mRotation,
		                         mCommonSaveParams->mSLBodyLength.get(),
		                         &bodyRevision);
		CLBCalcRotateZXYTranslateMatrix(
		    BHSModelMtx(b->mMActor->getModel()), b->mRotation, bodyRevision);
		PSMTXCopy(BHSModelMtx(b->mMActor->getModel()),
		          BHSModelMtx(b->mMActor->getModel()));
		b->mMActor->calc();
	}

	// TODO: the name handed to searchF() is the common .prm path in the
	// original binary; that looks wrong, so the real joint-group name is
	// still open.
	TIdxGroupObj* group = static_cast<TIdxGroupObj*>(
	    (JDrama::TViewObj*)JDrama::TNameRefGen::getInstance()
	        ->getRootNameRef()
	        ->searchF(JDrama::TNameRef::calcKeyCode(
	                      "/enemy/bosshanachanCommon.prm"),
	                  "/enemy/bosshanachanCommon.prm"));
	mHead->initMapCollisionAndHitActor_(group);
	for (int i = 0; i < 8; i++) {
		mBody[i]->initMapCollisionAndHitActor_(group);
		mBody[i]->initFootHitActor_(group);
	}
}

// Converts a raw s16 angle (the 0..65535 fixed-point form CLBRoundf returns)
// The original derives the launch angle from the boss's velocity direction
// (unk188/unk194/unk198, refreshed by moveObject()) and the throw strength
// from mMarchSpeed, then clamps it to the param range before handing both to
// SMS_ThrowMario. Every float literal below was read out of the ROM's
// .sdata2 at the addresses objdiff reports as @3153..@4699.
// The pragma is load-bearing: with the empty body below MWCC expands
// throwMario_() into perform() and deletes the three real call sites the
// original has there.
#pragma dont_inline on
void TBossHanachan::throwMario_(THitActor* hitActor)
{
	// Vector from the hit actor's owner to Mario.
	JGeometry::TVec3<f32> v = *gpMarioPos - hitActor->mPosition;

	f32 power;
	if (v.x * v.x + v.y * v.y + v.z * v.z <= 3.814697266e-06f) {
		// Mario is right on top of the hit actor: throw him straight up with
		// the full march-speed derived power.
		v.set(0.0f, 1.0f, 0.0f);
		power = mMarchSpeed * mChangeSaveParams->mSLThrowTotalPower.get();
	} else {
		// Otherwise aim along the boss's velocity direction. `ang` is the
		// boss's own heading and `ang2` the heading from the hit actor to
		// Mario; their difference decides how hard Mario is pushed sideways.
		f32 h;
		if (unk188.z == 0.0f) {
			h = (unk188.x >= 0.0f) ? 90.0f : -90.0f;
		} else if (unk188.z >= 0.0f) {
			h = (360.0f / 65536.0f) * (f32)matan(unk188.z, unk188.x);
		} else {
			h = 180.0f - (360.0f / 65536.0f) * (f32)matan(-unk188.z, unk188.x);
		}
		s16 ang = CLBRoundf<s16>(182.04445f * h);

		f32 h2;
		if (v.z == 0.0f) {
			h2 = (v.x >= 0.0f) ? 90.0f : -90.0f;
		} else if (v.z >= 0.0f) {
			h2 = (360.0f / 65536.0f) * (f32)matan(v.z, v.x);
		} else {
			h2 = 180.0f - (360.0f / 65536.0f) * (f32)matan(-v.z, v.x);
		}
		s16 ang2 = CLBRoundf<s16>(182.04445f * h2);

		s16 diff = (s16)(ang2 - ang);
		if (diff < 0)
			diff = -diff;

		// Same signed-int -> f64 bias trick as in setRandomWeakBodyIndex(), spelled
		// out inline because the original emits no out-of-line converter for
		// it in this TU.
		f64 biased;
		*((u32*)&biased + 1) = 0x43300000;
		*(u32*)&biased       = (u32)(diff ^ 0x8000);

		f32 deg = 3.051757812e-05f * (f32)(biased - 4503601774854144.0);
		f32 k   = 1.0f - deg;
		f32 s   = mChangeSaveParams->mSLThrowMoveDirPower.get();

		// The original stores unk188 into the scratch vector first and then
		// scales x/z by `k` and all three by `s`, in that order.
		JGeometry::TVec3<f32> d(unk188.x, unk188.y, unk188.z);
		d.x *= k;
		d.z *= k;
		d.x *= s;
		d.y *= s;
		d.z *= s;
		v += d;

		v.y = mChangeSaveParams->mSLThrowVecY.get();

		power = mMarchSpeed * mChangeSaveParams->mSLThrowTotalPower.get();
	}

	// Clamp the power into the param's throw-speed range.
	power = MsMin(MsMax(power, mChangeSaveParams->mSLThrowSpeedMin.get()),
	              mChangeSaveParams->mSLThrowSpeedMax.get());

	SMS_SendMessageToMario(mHead, 0xE);
	SMS_SendMessageToMario(mHead, 0x7);
	SMS_ThrowMario(v, power);
	mHead->mHitActor->onWaterHitCounter();
}
#pragma dont_inline off

// MsWrap<f32> is the header template in MarioUtil/MathUtil.hpp; the original
// emitted a local out-of-line copy of it in this TU (0x48 bytes) because
// perform() calls it out of line four times. Nothing calls it here yet, so the
// copy is not emitted -- it will appear as soon as perform() is reconstructed.

// Rotates the X/Z components of `param_3` by `param_2` (degrees) * ...
// The literal zero components come from the two vector temporaries: MWCC
// folds a hand written `0.0f * sn` away but keeps these.
// All four call sites (two in init, two in perform) are real `bl`s in the
// original, so this one is not inlined there either.
#pragma dont_inline on
static void CalcRevisionPosByRotateZ(const JGeometry::TVec3<f32>& param_1,
                                     f32 param_2, Vec* param_3)
{
	f32 z = param_1.z;

	param_3->y += param_2 * fabsf(z);
	if (fabsf(z) > 90.0f) {
		f32 rev = 7.0f * (fabsf(z) - 90.0f);
		if (param_1.z > 0.0f)
			rev = -rev;

		s32 trigAngle = CLBRoundf<s16>(182.04445f * param_1.y);
		u32 trigIndex = (u16)trigAngle >> jmaSinShift;

		// The literal zero components come from the two vector temporaries:
		// MWCC folds a hand written `0.0f * sn` away but keeps these.
		JGeometry::TVec3<f32> d(jmaCosTable[trigIndex], 0.0f,
		                         jmaSinTable[trigIndex]);
		JGeometry::TVec3<f32> e(rev, 0.0f, 0.0f);

		param_3->x += e.x * d.x + e.y * d.z;
		param_3->z += -e.x * d.z + e.y * d.x;
	}
}
#pragma dont_inline off

// UNUSED in the original (0x118 and 0x170 bytes); both bodies are called from
// perform() and have not been reconstructed yet. execHeadCalcAnim_ is defined
// first because -inline deferred emits the bodies in reverse source order.
void TBossHanachan::execHeadCalcAnim_() { }
void TBossHanachan::execBodyCalcAnim_() { }

void TBossHanachan::kill() { }

void TBossHanachan::bind()
{
	// TODO: bit 27 of mLiveFlag has no LIVE_FLAG_* name (the GMSP01 values in
	// include/Strategic/LiveActor.hpp are shifted one bit left relative to
	// this bit, which is why it cannot be spelled with an existing name). It
	// is the boss's own "dead / already bound" guard, tested before anything
	// else, so it is left as a raw mask rather than faking a flag constant.
	if (mLiveFlag & 0x04000000)
		return;

	if (mBinder != nullptr) {
		mBinder->bind(this);
		return;
	}

	JGeometry::TVec3<f32> v = mPosition;
	v.x += mLinearVelocity.x;
	v.y += mLinearVelocity.y;
	v.z += mLinearVelocity.z;
	v.x += mVelocity.x;
	v.y += mVelocity.y;
	v.z += mVelocity.z;

	mVelocity.y -= getGravityY();

	if (mVelocity.y < mVelocityMinY)
		mVelocity.y = mVelocityMinY;

	unk17C = v;
	f32 revX, revZ;
	BHSCalcRevisionDistXZByRotateZ(mRotation.y, mRotation.z,
	                               mSphereLink->unk14, &revX, &revZ);
	unk17C.x += revX;
	unk17C.z += revZ;

	mGroundHeight = gpMap->checkGroundIgnoreWaterSurface(
	    unk17C.x, unk17C.y + mHeadHeight, unk17C.z, &mGroundPlane);
	mGroundHeight += 1.0f;

	if (unk17C.y <= 0.05f + mGroundHeight) {
		if (mGroundPlane != nullptr && mGroundPlane->isIllegalData()) {
			// Landed: clear bits 23..25 of mLiveFlag, stop the fall and snap
			// the link position's Y onto the ground height.
			// TODO: 0x03800000 has no LIVE_FLAG_* name in GMSP01.
			mLiveFlag &= ~0x03800000;
			mVelocity.zero();
			unk17C.y = mGroundHeight;
		}
	} else {
		onLiveFlag(LIVE_FLAG_AIRBORNE);
	}

	gpMap->isTouchedOneWallAndMoveXZ(&unk17C.x, unk17C.y + mHeadHeight,
	                                 &unk17C.z, mBodyRadius);

	// The original makes three out-of-line TVec3 operator- calls here, of which
	// the first one's result is discarded:
	//   1. a local copy of unk17C minus unk17C itself (dead)
	//   2. v - mPosition
	//   3. v + (2), stored into mLinearVelocity
	JGeometry::TVec3<f32> scratch = unk17C;
	scratch                         = scratch - unk17C;
	mLinearVelocity                 = v + (v - mPosition);
}

void TBossHanachan::moveObject()
{
	updateSquareToMario();
	unk188.set(mLinearVelocity);
	TLiveActor::moveObject();

	const TNerveBase<TLiveActor>* cur = mSpine->getLatestNerve();
	if (cur != &TNerveBossHanachanGetUp::theNerve())
		CLBChaseDecrease(&mRotation.z, mBody[0]->mRotation.z, 0.04f, 0.0f);

	mHead->mPosition    = mPosition;
	mHead->mRotation    = mRotation;
	mHead->mGroundPlane = mGroundPlane;
}

// Returns the X/Z rotation, in degrees, of the direction vector `v` in the
// X/Z plane. Same shape as the MsGetRot* helpers in MarioUtil/MathUtil.hpp,
// but the second test really is re-issued in the original (the compiler does
// not fold it away), and the caller wraps the result with MsWrap<f32>.
static f32 BHSDegFromXZ(const JGeometry::TVec3<f32>& v)
{
	// TODO: the zero-multiplied terms of the 200-unit ground probe below come
	// from two vector temporaries, not from a hand written 0.0f * sn.
	if (0.0f == v.z) {
		return (v.x >= 0.0f) ? 90.0f : -90.0f;
	} else if (v.z >= 0.0f) {
		return (360.0f / 65536.0f) * matan(v.z, v.x);
	} else {
		return 180.0f - (360.0f / 65536.0f) * matan(-v.z, v.x);
	}
}

// perform() inlines this at three sites (head, body, foot), so it is written
// out by hand there rather than kept as a function.

void TBossHanachan::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_UNK200))
		return;

	// Demo / talk mode: the boss is not really there, it just drops a shine.
	if (checkLiveFlag(LIVE_FLAG_UNK40000)) {
		if (!(cue & 1))
			return;
		if (!(*(u16*)graphics & 2))
			return;
		if (gpMSound->gateCheck(0x6010)) {
			MSoundSESystem::MSoundSE::startSoundActor(0x6010, &mPosition, 0,
			                                          0, 0, 4);
		}
		if (gpMarDirector->isThing())
			return;
		if (!checkLiveFlag(LIVE_FLAG_UNK100000)
		    && gpMSound->unk98->modBgm(1, 1)) {
			return;
		}
		mLiveFlag |= LIVE_FLAG_DEAD | LIVE_FLAG_UNK40;
		gpItemManager->makeShineAppearWithDemo(
		    "シャイン（ボス用）", "ボスシャインカメラ",
		    unk17C.x,
		    unk17C.y
		        + mCommonSaveParams->mSLShineAppearOffsetY.get(),
		    unk17C.z);
		return;
	}

	if (cue & 1) {
		if (gpMarDirector->isThing()) {
			mLinearVelocity.zero();
			mAngularVelocity.zero();
			// TODO: the original leaves the whole update here; a goto is the
			// only spelling that reproduces the branch target.
			if (!(*(u16*)graphics & 2))
				goto moveMapCollision;
			if (mSpine->getLatestNerve() == &TNerveBossHanachanDead::theNerve()) {
				if (!checkLiveFlag(LIVE_FLAG_UNK100000)) {
					onLiveFlag(LIVE_FLAG_UNK100000);
					MSBgm::stopTrackBGM(1, 30);
				}
			}
			goto moveMapCollision;
		}
		// The hint balloon timer only runs while the boss is not tumbling.
		if (!checkLiveFlag(LIVE_FLAG_UNK80000) && mHitPoints == 3
		    && 0.0f != mMarchSpeed
		    && mSpine->getLatestNerve() != &TNerveBossHanachanTumble::theNerve()) {
			// TODO: 0x1C20 frames; gpMarDirector->mState has no accessor yet.
			if (unk1B8 == -1 && gpMarDirector->mState == 4) {
				unk1B8 = 7200;
			} else if (unk1B8 > 0) {
				unk1B8 -= 1;
				if (unk1B8 == 0) {
					unk1B8 = 7200;
					gpMarDirector->getConsole()->startAppearBalloon(6, true);
				}
			}
		}
		{
			moveObject();
			const TNerveBase<TLiveActor>* currentNerve = mSpine->getLatestNerve();

				for (int i = 0; i < 8; i++) {
					TBossHanachanPartsBody* body = mBody[i];
					const JGeometry::TVec3<f32>& previousPosition = body->unk124;
					body->unk130 = previousPosition;
					const JGeometry::TVec3<f32>& currentPosition = body->mPosition;
					body->unk124 = currentPosition;
					body->unk140 = body->unk13C;
					body->unk13C = body->mRotation.z;
					body->unk148 = body->unk144;
				}

				s16 bodyAngle
				    = CLBRoundf<s16>(182.04445f * mBody[0]->mRotation.y);
				CLBChaseAngleDecrease(
				    &bodyAngle, CLBRoundf<s16>(182.04445f * mRotation.y), 20);
				mBody[0]->mRotation.y
				    = (360.0f / 65536.0f) * (f32)bodyAngle;

				for (int i = 1; i < 8; i++) {
					TSpherePoint& point = mSphereLink->mPoints[i - 1];
					TBossHanachanPartsBody* body = mBody[i];
					JGeometry::TVec3<f32> v
					    = point.unkC - mSphereLink->mPoints[i].unkC;
					body->mRotation.y
					    = MsWrap<f32>(BHSDegFromXZ(v), 0.0f, 360.0f);
				}

				mSphereLink->unk18 = mBody[0]->mRotation.y;
				for (int i = 0; i < 8; i++) {
					mSphereLink->setDegreeZAndRevisionPosXZ(
					    i, mBody[i]->mRotation.z);
				}

				f32 revX, revZ;
				JGeometry::TVec3<f32> headPos = mPosition;
				headPos.x -= JMASSin((s16)(mRotation.y
				                          * (65536.0f / 360.0f)))
				             * mCommonSaveParams->mSLHeadLength.get();
				headPos.z -= JMASCos((s16)(mRotation.y
				                          * (65536.0f / 360.0f)))
				             * mCommonSaveParams->mSLHeadLength.get();
				BHSCalcRevisionDistXZByRotateZ(mRotation.y, mRotation.z,
				                               mSphereLink->unk14, &revX,
				                               &revZ);
				headPos.x += revX;
				headPos.z += revZ;
				mSphereLink->moveHead(headPos);

				for (int i = 0; i < 8; i++) {
					TBossHanachanPartsBody* body = mBody[i];
					BHSCalcRevisionDistXZByRotateZ(body->mRotation.y,
					                               body->mRotation.z,
					                               mSphereLink->unk14, &revX,
					                               &revZ);
					body->mPosition = mSphereLink->mPoints[i].unkC;
					body->mPosition.x -= revX;
					body->mPosition.z -= revZ;
				}

				bool isTumble = false;
				f32 centrifugal = 0.0f;
				if (mSpine->getLatestNerve()
				    == &TNerveBossHanachanTumble::theNerve()) {
					isTumble = true;
					centrifugal = getBodyMaxRotateZ();
				}
				for (int i = 0; i < 8; i++) {
					TBossHanachanPartsBody* body = mBody[i];
					if (!isTumble) {
						centrifugal = BHSCalcCentrifugalForce(
						    body->mPosition, body->unk124, body->unk130,
						    body->mRotation.y)
						    * mChangeSaveParams->mSLCentrifugalForce.get();
					}
					CLBChaseGeneralConstantSpecifySpeed<f32>(
					    &body->unk144, centrifugal,
					    mChangeSaveParams->mSLCentrifugalSpeed.get());
					body->unk144
					    = MsClamp(body->unk144, -179.0f, 179.0f);
				}

				if (currentNerve
				    != &TNerveBossHanachanDown::theNerve()) {
					// The original keeps these two in registers across the loop
					// (f15 / f14); MWCC materialises the literals instead, so
					// f14/f15 never appear in our prologue.
					f32 sandUp   = 70.0f;
					f32 sandDown = -70.0f;
					for (int i = 0; i < 8; i++) {
						TBossHanachanPartsBody* body = mBody[i];
						f32 groundY = gpMap->checkGroundIgnoreWaterSurface(
						    body->mPosition.x,
						    body->mPosition.y + 500.0f,
						    body->mPosition.z, &body->mGroundPlane);
						TLiveActor* sandActor = body->getSandActor_();
						if (sandActor != nullptr) {
							JGeometry::TVec3<f32> d = sandActor->mPosition
							                             - mPosition;
							JGeometry::TVec3<f32> e = d;
							if (d.x * d.x + d.z * d.z
							    < CLBSquared<f32>(50.0f)) {
								body->unk120 = 0.0f;
								continue;
							}
							f32 angle = MsWrap<f32>(BHSDegFromXZ(e), -180.0f,
							                        180.0f);
							f32 rel = MsWrap<f32>(
							    angle
							        - MsWrap<f32>(mRotation.y, -180.0f, 180.0f),
							    -180.0f, 180.0f);
							f32 absRel = CLBAbs<f32>(rel);
							if (absRel <= 15.0f || absRel >= 165.0f) {
								body->unk120 = 0.0f;
							} else {
								f32 riseRatio = SMS_GetSandRiseUpRatio(sandActor);
								if (rel > 15.0f)
									body->unk120 = sandUp * riseRatio;
								else
									body->unk120 = sandDown * riseRatio;
							}
						} else {
							s16 a = CLBRoundf<s16>(body->mRotation.y
							              * (65536.0f / 360.0f));
							f32 cosA = JMASCos(a);
							f32 sinA = JMASSin(a);
							JGeometry::TVec3<f32> probe(200.0f, 0.0f, 0.0f);
							f32 offsetX = probe.x * cosA + probe.z * sinA;
							f32 offsetZ = -probe.x * sinA + probe.z * cosA;
							JGeometry::TVec3<f32> oppositeProbe(-offsetX, -probe.y,
							                                      -offsetZ);
							f32 centerY = probe.y + body->mPosition.y;
							f32 oppositeY = oppositeProbe.y + body->mPosition.y;
							f32 oppositeX = oppositeProbe.x + body->mPosition.x;
							f32 oppositeZ = oppositeProbe.z + body->mPosition.z;
							const TBGCheckData* check = nullptr;
							f32 h1 = gpMap->checkGroundIgnoreWaterSurface(
							    body->mPosition.x + offsetX, 500.0f + centerY,
							    body->mPosition.z + offsetZ, &check);
							f32 h2 = gpMap->checkGroundIgnoreWaterSurface(
							    oppositeX, 500.0f + oppositeY, oppositeZ, &check);
							f32 t1 = h1 - groundY;
							f32 t2 = h2 - groundY;
							if (fabsf(t1) < 0.001f && fabsf(t2) < 0.001f) {
								body->unk120 = 0.0f;
							} else if (fabsf(t1) > fabsf(t2)) {
								body->unk120
								    = (360.0f / 65536.0f) * matan(200.0f, t1);
							} else {
								body->unk120
								    = -(360.0f / 65536.0f) * matan(200.0f, t2);
							}
						}
					}
				}

				if (currentNerve == &TNerveBossHanachanGetUp::theNerve()) {
					mHead->calcRotateZWhenGetUp_();
					mRotation.z = mHead->mRotation.z;
					for (int i = 0; i < 8; i++)
						mBody[i]->calcRotateZWhenGetUp_();
				} else {
					// wave / wander part
					f32 maxZ = getBodyMaxRotateZ();
					const TNerveBase<TLiveActor>* waveNerve = mSpine->getLatestNerve();
					const f32 k = 1.0f / 120.0f;
					f32 dec = mChangeSaveParams->mSLWaveDecrease.get() * k;
					f32 inv
					    = 1.0f
					      / (1.0f
					         + dec); // + dec
					f32 minus = 1.0f - dec;
					f32 invLen
					    = 1.0f
					      / (mCommonSaveParams->mSLBodyLength.get()
					         * mCommonSaveParams->mSLBodyLength.get());
					f32 waveV = mChangeSaveParams->mSLWaveVelocity.get();
					f32 coef = k * (k * (waveV * waveV));

					for (int i = 0; i < 8; i++) {
						TBossHanachanPartsBody* body = mBody[i];
						f32 rotZ = body->mRotation.z;
						if (((rotZ == -179.0f || rotZ == 179.0f) ? true : false)
						    && rotZ == maxZ)
							continue;
							f32 previous = (i == 0)
							    ? mBody[1]->unk13C
							    : mBody[i - 1]->unk13C;
							f32 next = (i == 7)
							    ? mBody[i - 1]->unk13C
							    : mBody[i + 1]->unk13C;
							f32 cur    = body->unk13C;
							f32 two    = 2.0f * cur;
							f32 sum    = next + previous - two;
							f32 acc    = body->unk148 + invLen * sum;
							acc        = coef * acc;
							acc        = inv * acc;
							f32 result = two * inv + acc - inv * (body->unk140
							                                * minus);
							f32 target = MsClamp(result, -179.0f, 179.0f);
						CLBChaseGeneralConstantSpecifySpeed<f32>(
						    &body->mRotation.z, target,
						    mChangeSaveParams->mSLRotateZLeanSpeed.get());

						bool onSand = false;
						if (waveNerve
						    == &TNerveBossHanachanGraphWander::theNerve()) {
							TLiveActor* sandActor = body->getSandActor_();
							if (sandActor != nullptr) {
								onSand = true;
								if (0.0f != body->unk120) {
									f32 max = body->unk120 * mMarchSpeed
									          * mChangeSaveParams
									                ->mSLSandSlopeForce
									                .get();
									CLBChaseGeneralConstantSpecifySpeed<f32>(
									    &body->mRotation.z,
									    body->unk120 >= 0.0f ? 179.0f : -179.0f,
									    max);
								}
							} else {
								CLBChaseGeneralConstantSpecifySpeed<f32>(
								    &body->mRotation.z, body->unk120,
								    mChangeSaveParams
								        ->mSLRotateZRestorationSpeed
								        .get());
							}
						} else if (waveNerve
						           == &TNerveBossHanachanTumble::theNerve()) {
							CLBChaseGeneralConstantSpecifySpeed<f32>(
							    &body->mRotation.z, unk194, unk198);
						}
						body->mRotation.z = MsClamp(body->mRotation.z, -179.0f,
						                            179.0f);

						if (waveNerve
						    == &TNerveBossHanachanGraphWander::theNerve()) {
							if (mSpine->getTime()
							        < mChangeSaveParams
							          ->mSLNotFallDownFrames
							          .get()
							    || !onSand) {
								f32 lim = mChangeSaveParams
								              ->mSLMaxRotateZNotSand
								              .get();
								f32 z = body->mRotation.z;
								if (z < -lim) {
									CLBChaseGeneralConstantSpecifySpeed<f32>(
									    &body->mRotation.z, -lim, 15.0f);
								} else if (z > lim) {
									CLBChaseGeneralConstantSpecifySpeed<f32>(
									    &body->mRotation.z, lim, 15.0f);
								}
							}
						}
					}

					f32 diffMax
					    = mChangeSaveParams->mSLDiffMaxRotateZ.get();
					for (int i = 1; i < 8; i++) {
						TBossHanachanPartsBody* a = mBody[i - 1];
						TBossHanachanPartsBody* b = mBody[i];
						f32 az = a->mRotation.z;
						f32 bz = b->mRotation.z;
						if (fabsf(bz - az) > diffMax) {
							if (bz < az)
								b->mRotation.z = az - diffMax;
							else
								b->mRotation.z = az + diffMax;
						}
					}
				}

				bool thrown = false;
				{
					TWaterHitActor* hit = mHead->mHitActor;
					if (hit->mWaterHitCounter >= 1)
						hit->mWaterHitCounter -= 1;
					for (int i = 0; i < hit->mColCount; i++) {
						if (hit->mCollisions[i]->mActorType == 0x80000001) {
							throwMario_(hit);
							thrown = true;
							break;
						}
					}
				}
				for (int i = 0; i < 8; i++) {
					{
						TWaterHitActor* hit = mBody[i]->mHitActor;
						if (hit->mWaterHitCounter >= 1)
							hit->mWaterHitCounter -= 1;
						if (!thrown) {
							for (int j = 0; j < hit->mColCount; j++) {
								if (hit->mCollisions[j]->mActorType
								    == 0x80000001) {
									throwMario_(hit);
									thrown = true;
									break;
								}
							}
						}
					}
					for (int j = 0; j < 2; j++) {
						TWaterHitActor* hit = mBody[i]->mFootHitActor[j];
						if (hit->mWaterHitCounter >= 1)
							hit->mWaterHitCounter -= 1;
						if (!thrown) {
							for (int k = 0; k < hit->mColCount; k++) {
								if (hit->mCollisions[k]->mActorType
								    == 0x80000001) {
									throwMario_(hit);
									thrown = true;
									break;
								}
							}
						}
					}
				}

				if (*(u16*)graphics & 2
				    && currentNerve
				           == &TNerveBossHanachanDead::theNerve()) {
					if (gpMSound->gateCheck(0x6010)) {
						MSoundSESystem::MSoundSE::startSoundActor(
						    0x6010, &mPosition, 0, 0, 0, 4);
					}
					if (!checkLiveFlag(LIVE_FLAG_UNK100000)) {
						gpMSound->unk98->modBgm(1, 1);
					}
				}
			}
	moveMapCollision:
		mHead->moveMapCollision_();
		for (int i = 0; i < 8; i++)
			mBody[i]->moveMapCollision_();
	}

	if (cue & 2) {
		gpTargetArrow->unk14 = 0;
		if (!(gpMarDirector->isThing())) {
			bool moving = false;
			if (mSpine->getLatestNerve()
			    == &TNerveBossHanachanGraphWander::theNerve()) {
				if (mMarchSpeed > 0.001f)
					moving = true;
			}
			MtxPtr mtx = mHead->mMapCollisionJointMtx;
			JGeometry::TVec3<f32> headJoint(mtx[0][3], mtx[1][3], mtx[2][3]);
			THitActor* headHit = mHead->mHitActor;
			headHit->mPosition.set(
			    headJoint.x,
			    headJoint.y - mCommonSaveParams->mSLHeadHitOffsetY.get(),
			    headJoint.z);
			// TODO: 0x7FFFFFFF and 0x80000000 are unnamed hit flags. MWCC
			// turns the test into `& ~1` no matter which spelling is used.
			if (moving) {
				if (!headHit->checkHitFlag(0x80000000u)) {
					headHit->onHitFlag(0x80000000);
					mHead->mMapCollision->remove();
				}
			} else {
				if (headHit->checkHitFlag(0x80000000u)) {
					headHit->offHitFlag(0x80000000u);
					mHead->mMapCollision->setUpTrans(headJoint);
				}
			}

			f32 footOffY = mCommonSaveParams->mSLFootHitOffsetY.get();
			f32 bodyOffY = mCommonSaveParams->mSLBodyHitOffsetY.get();
			for (int i = 0; i < 8; i++) {
				TBossHanachanPartsBody* body = mBody[i];
				MtxPtr bmtx        = body->mMapCollisionJointMtx;
				body->unk154.set(bmtx[0][3], bmtx[1][3], bmtx[2][3]);
				THitActor* hit     = body->mHitActor;
				hit->mPosition.set(
				    body->unk154.x, body->unk154.y - bodyOffY,
				    body->unk154.z);
				if (moving) {
					if (!hit->checkHitFlag(0x80000000u)) {
						hit->onHitFlag(0x80000000);
						body->mMapCollision->remove();
					}
				} else {
					if (hit->checkHitFlag(0x80000000u)) {
						hit->offHitFlag(0x80000000u);
						body->mMapCollision->setUpTrans(body->unk154);
					}
				}
				if (moving) {
					for (int j = 0; j < 2; j++) {
						TFootHitActor* foot = body->mFootHitActor[j];
						MtxPtr fmtx         = foot->unk6C;
						foot->mPosition.set(fmtx[0][3], fmtx[1][3] - footOffY,
						                   fmtx[2][3]);
						foot->onHitFlag(0x80000000);
					}
				} else {
					for (int j = 0; j < 2; j++) {
						TFootHitActor* foot = body->mFootHitActor[j];
						MtxPtr fmtx         = foot->unk6C;
						foot->mPosition.set(fmtx[0][3], fmtx[1][3] - footOffY,
						                   fmtx[2][3]);
						foot->offHitFlag(0x80000000u);
					}
				}
			}
			emitParticle_();
			emitCamShake_();
		}
	}

	if (checkLiveFlag(LIVE_FLAG_UNK10000)) {
		if (mSandPillar->curAnmEndsNext(0, nullptr)) {
			mLiveFlag &= ~0x00070000;
		}
	}

	if (!(gpMarDirector->isThing())) {
		changeAnmRateAndFrameUpdate_();
	}

	// TODO: mNonstopMotionBlend carries a fabricated placeholder type in
	// Enemy/BossHanachan.hpp (another agent owns that header); the real type
	// is TNpcInbetween, which is what execMotionBlend() lives on.
	((TNpcInbetween*)mHead->mNonstopMotionBlend)
	    ->execMotionBlend(mHead->mMActor);
	for (int i = 0; i < 8; i++) {
		((TNpcInbetween*)mBody[i]->mNonstopMotionBlend)
		    ->execMotionBlend(mBody[i]->mMActor);
	}

	if (cue & 2) {
		JGeometry::TVec3<f32> headRevision = mPosition;
		CalcRevisionPosByRotateZ(mRotation,
		                         mCommonSaveParams->mSLHeadPlusYByRotateZ
		                             .get(),
		                         &headRevision);
		CLBCalcRotateZXYTranslateMatrix(
		    BHSModelMtx(mHead->mMActor->getModel()), mRotation, headRevision);
		mHead->mMActor->calc();

		for (int i = 0; i < 8; i++) {
			TBossHanachanPartsBody* body = mBody[i];
			JGeometry::TVec3<f32> bodyRevision = body->mPosition;
			CalcRevisionPosByRotateZ(
			    body->mRotation,
			    mCommonSaveParams->mSLBodyPlusYByRotateZ.get(),
			    &bodyRevision);
			Mtx mtx;
			CLBCalcRotateZXYTranslateMatrix(mtx, body->mRotation,
			                                bodyRevision);
			PSMTXCopy(mtx, BHSModelMtx(body->mMActor->getModel()));
			body->mMActor->calc();
		}

		if (!(gpMarDirector->isThing())) {
			if (mSpine->getLatestNerve()
			        == &TNerveBossHanachanTumble::theNerve()
			    || mSpine->getLatestNerve()
			           == &TNerveBossHanachanDown::theNerve()) {
				MtxPtr mtx = mBody[mWeakBodyIndex]->mMapCollisionJointMtx;
				JGeometry::TVec3<f32> pos(mtx[0][3],
				                         mtx[1][3] + 400.0f, mtx[2][3]);
				gpTargetArrow->setPos(pos);
				gpTargetArrow->unk14 = 1;
			}
		}
	}

	if (cue & 0x200) {
		mHead->entryCircleShadow_();
		mHead->setDamageFog_(graphics);
		mHead->drawObject(graphics);
		for (int i = 0; i < 8; i++) {
			mBody[i]->entryCircleShadow_();
			mBody[i]->setDamageFog_(graphics);
			mBody[i]->drawObject(graphics);
		}
	}

	if (cue & 4) {
		mHead->mMActor->viewCalc();
		for (int i = 0; i < 8; i++)
			mBody[i]->mMActor->viewCalc();
	}

	if (checkLiveFlag(LIVE_FLAG_UNK10000))
		mSandPillar->perform(cue, graphics);
}

bool TBossHanachan::isTumbleCompletelyAllBody() const
{
	// The reference rotation is body 0's Z; every other body has to agree
	// with it, and it has to be one of the two fully-tumbled angles.
	f32 z    = mBody[0]->mRotation.z;
	bool ret = true;

	if (!((z == -179.0f || z == 179.0f) ? true : false)) {
		ret = false;
	} else {
		for (int i = 1; i < 8; i++) {
			if (mBody[i]->mRotation.z != z) {
				ret = false;
				break;
			}
		}
	}

	return ret;
}

bool TBossHanachan::checkFallDecideAndSetup()
{
	for (int i = 0; i < 8; i++) {
		TBossHanachanPartsBody* body = mBody[i];
		if (CLBAbs<f32>(body->mRotation.z)
		    > mChangeSaveParams->mSLFallDecideRotateZ.get()) {
			emitOneTimeSandPillar_(body);
			unk194 = (body->mRotation.z > 0.0f) ? 179.0f : -179.0f;
			unk198 = mChangeSaveParams->mSLWaveFallDownSpeed.get()
			    * CLBAbs<f32>(body->unk13C - body->mRotation.z);
			if (unk198 < mChangeSaveParams->mSLFallDecideMinSpeed.get())
				unk198 = mChangeSaveParams->mSLFallDecideMinSpeed.get();
			return true;
		}
	}
	return false;
}

	// perform() calls this with a real `bl` in the original, so keep it out of
// line there.
#pragma dont_inline on
f32 TBossHanachan::getBodyMaxRotateZ() const
{
	// TODO: the original materialises the address of each mBody[] element
	// (addi/lwz) instead of folding it into the member load, and evaluates
	// fabsf(ret) before the member read. A pointer-induction loop and an
	// index loop both compile to the folded form.
	f32 ret = 0.0f;
	for (int i = 0; i < 8; i++) {
		TBossHanachanPartsBody* body = mBody[i];
		if (fabsf(body->mRotation.z) > fabsf(ret))
			ret = body->mRotation.z;
	}
	return ret;
}
#pragma dont_inline off

// TODO: UNUSED in the original (0xC0 bytes) and only ever inlined; the body
// is a guess, the map size is the only constraint.
bool TBossHanachan::isCanWalk() const
{
	if (mGroundPlane == nullptr)
		return false;
	if (mGroundActorYaw < 0.0f)
		return false;
	return getBodyMaxRotateZ() < 90.0f;
}

void TBossHanachan::execWalk(bool param_1)
{
	if (param_1)
		CLBChaseGeneralConstantSpecifySpeed<f32>(
		    &mMarchSpeed,
		    mChangeSaveParams->mSLMaxMarchSpeed.get(),
		    mChangeSaveParams->mSLMarchAccel.get());
	else
		CLBChaseGeneralConstantSpecifySpeed<f32>(
		    &mMarchSpeed, 0.0f, mChangeSaveParams->mSLMarchDecrease.get());

	mTurnSpeed = mChangeSaveParams->mSLWalkTurnSpeed.get();

	bool reached = true;

	{
		JGeometry::TVec3<f32> p = unkF4.getPoint();
		JGeometry::TVec3<f32> d;
		d.set(p.x - mPosition.x, 0.0f, p.z - mPosition.z);
		f32 s = CLBSquared<f32>(d.x) + CLBSquared<f32>(d.y)
		    + CLBSquared<f32>(d.z);
		if (s < 10.0f)
			reached = false;
	}
	if (reached)
		walkToCurPathNode(mMarchSpeed, mTurnSpeed, 0.0f);

	{
		JGeometry::TVec3<f32> p = unkF4.getPoint();
		JGeometry::TVec3<f32> d;
		d.x = p.x - mPosition.x;
		d.y = 0.0f;
		d.z = p.z - mPosition.z;
		f32 s = CLBSquared<f32>(d.x) + CLBSquared<f32>(d.y)
		    + CLBSquared<f32>(d.z);
		if (s < 100.0f) {
			if (unk114.size() != 0)
				unkF4 = unk114.pop();
			else
				goToDirLimitedNextGraphNode(90.0f);
		}
	}
}

void TBossHanachan::execSlip()
{
	CLBChaseGeneralConstantSpecifySpeed<f32>(
	    &mMarchSpeed, 0.0f, mChangeSaveParams->mSLWalkBckRateMin.get());
	mTurnSpeed = 0.1f;

	JGeometry::TVec3<f32> add(0.0f, 0.0f, 0.0f);
	if (mMarchSpeed > 0.001f) {
		add = unk188;
		if (mMarchSpeed > 4.0f) {
			// getBodyMaxRotateZ() is spelled out here because perform() has
			// to call it out of line (see the pragma on its definition) and
			// this call site is expanded in the original.
			f32 maxZ = 0.0f;
			// TODO: the original materialises the address of each mBody[]
			// element (addi r3, r31, 0x150 / lwz r3, 0(r3)) instead of
			// folding it into the member load. Neither an index loop nor a
			// pointer-walk loop reproduces that under MWCC; see the same note
			// on getBodyMaxRotateZ().
			for (int j = 0; j < 8; j++) {
				TBossHanachanPartsBody* b = mBody[j];
				if (fabsf(b->mRotation.z) > fabsf(maxZ))
					maxZ = b->mRotation.z;
			}
			f32 zero = 0.0f;
			f32 sign = 1.0f;
			if (maxZ > 0.0f) {
				zero = -zero;
				sign = -sign;
			}
			s16 angle = CLBRoundf<s16>(mRotation.y * (65536.0f / 360.0f));
			f32 k     = 0.005f * mMarchSpeed;
			// The two literals are loaded from separate jma tables by the
			// original; here the compiler folds the pair into one table base.
			f32 sn = JMASSin(angle);
			f32 cs = JMASCos(angle);
			add.x += (zero * sn + sign * cs) * k;
			add.y += zero * k;
			add.z += (zero * cs - sign * sn) * k;
		}
	}

	if (add.x * add.x + add.y * add.y + add.z * add.z
	    >= JGeometry::TUtil<f32>::epsilon()) {
		MsVECNormalize(&add, &add);
		add *= 500.0f;
		JGeometry::TVec3<f32> next = mPosition + add;
		TPathNode node;
		node.unk4 = next;
		unkF4      = node;
		unk104     = node;
		unk114.clear();
		walkToCurPathNode(mMarchSpeed, mTurnSpeed, 0.0f);
	}

	// TODO: 0x9 is missing from EnumCamShakeMode in Camera/CameraShake.hpp.
	gpCameraShake->keepShake((EnumCamShakeMode)0x9, 1.0f);
	if (SMS_IsMarioTouchGround4cm() && mSpine->getTime() < 120)
		SMSRumbleMgr->start(0x16, 0, (Vec*)nullptr);
}

void TBossHanachan::goToInitialRecoverGraphNode()
{
	// No local for the tracer: the original re-reads unk124 at every use.
	getTracer()->mPrevIdx = -1;
	getTracer()->mCurrIdx = -1;

	int index = getTracer()->getGraph()->findNearestVisibleIndex(
	    mPosition, mRotation.y,
	    mCommonSaveParams->mSLRecoverSearchDist.get(),
	    mCommonSaveParams->mSLRecoverSearchDegree.get(), -1);
	if (index < 0) {
		goToShortestNextGraphNode();
		return;
	}

	getTracer()->setTo(index);
	setGoalPathFromGraph();
	unk128 = 0;
	unk12C = 0.0f;
}

void TBossHanachan::execDamage()
{
	mSpine->reset();

	if (mHitPoints != 0)
		mHitPoints -= 1;

	if (mHitPoints == 0) {
		// Dead: every hit actor on the head and all eight body segments is
		// switched off before the dead nerve is queued.
		mHead->mHitActor->onHitFlag(HIT_FLAG_NO_COLLISION);
		for (int i = 0; i < 8; i++) {
			mBody[i]->mHitActor->onHitFlag(HIT_FLAG_NO_COLLISION);
			mBody[i]->mFootHitActor[0]->onHitFlag(HIT_FLAG_NO_COLLISION);
			mBody[i]->mFootHitActor[1]->onHitFlag(HIT_FLAG_NO_COLLISION);
		}

		mSpine->setNext(&TNerveBossHanachanDead::theNerve());
		setAnmTimerWhenDead();

		unk1AC = *gpMarioPos;
		if (gpMSound->gateCheck(0x28E6)) {
			MSoundSESystem::MSoundSE::startSoundActor(0x28E6, &unk1AC, 0,
			                                          0, 0, 4);
		}
		return;
	}

	mSpine->setNext(&TNerveBossHanachanDamage::theNerve());
	setAnmTimerWhenDamage();

	mChangeSaveParams = static_cast<TBossHanachanManager*>(mManager)
	                        ->mChangeSaveParams[mHitPoints - 3];

	// The graph the tracer is reset onto depends on how many hit points are
	// left; the ROM loads cHitPoint1_RailName up front and only switches to
	// cHitPoint2_RailName for exactly two.
	const char* railName;
	switch (mHitPoints) {
	case 2:
		railName = cHitPoint2_RailName;
		break;
	default:
		railName = cHitPoint1_RailName;
		break;
	}
	// TODO: the web pointer at TGraphTracer+0 has no accessor name yet.
	unk124->unk0 = (TGraphWeb*)gpConductor->getGraphByName(railName);

	mLiveFlag |= 0x4000;

	if (gpMSound->gateCheck(0x280F)) {
		MSoundSESystem::MSoundSE::startSoundActor(
		    0x280F, &mBody[mWeakBodyIndex]->unk154, 0, 0, 0, 4);
	}
}

void TBossHanachan::removeAllMapCollision()
{
	mHead->mMapCollision->remove();
	for (int i = 0; i < 8; i++)
		mBody[i]->mMapCollision->remove();
}

BOOL TBossHanachan::hasMapCollision() const { return true; }
