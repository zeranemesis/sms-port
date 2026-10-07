#include <Enemy/EnemyManager.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/Enemy.hpp>
#include <System/TimeRec.hpp>
#include <MSound/MAnmSound.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Strategy.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <JSystem/JDrama/JDRViewObjPtrList.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTransform.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

bool TEnemyManager::mIsCopyAnmMtx = true;

TSpineEnemyParams::TSpineEnemyParams(const char* path)
    : TParams(path)
    , PARAM_INIT(mSLHeadHeight, 120.0f)
    , PARAM_INIT(mSLBodyRadius, 30.0f)
    , PARAM_INIT(mSLWallRadius, 50.0f)
    , PARAM_INIT(mSLClipRadius, 300.0f)
    , PARAM_INIT(mSLFarClip, 10000.0f)
    , PARAM_INIT(mSLHitPointMax, 1)
    , PARAM_INIT(mSLInstanceNum, 100)
    , PARAM_INIT(mSLActiveEnemyNum, 10)
{
	TParams::load(mPrmPath);
}

void TSharedMActorSet::init(MActorAnmData* param_1, J3DModelData* param_2,
                            const char* param_3, int param_4)
{
	unk4 = param_4;
	unk0 = new MActor*[unk4];

	f32 coeff = 1.0f / unk4;
	for (int i = 0; i < unk4; ++i) {
		J3DModel* model = new J3DModel(param_2, 0, 1);
		unk0[i]         = new MActor(param_1);
		unk0[i]->setModel(model, 0);
		unk0[i]->setBck(param_3);
		J3DFrameCtrl* ctrl = unk0[i]->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setFrame(coeff * ctrl->getEnd() * i);
	}

	unk8 = unk0[0]->getCurAnmIdx(ANM_TYPE_BCK);
}

void TSharedMActorSet::calcAnm()
{
	for (int i = 0; i < unk4; ++i)
		unk0[i]->calcAnm();
}

// UNUSED (0x48).
void TSharedMActorSet::setScale(const JGeometry::TVec3<f32>& scale)
{
	for (int i = 0; i < unk4; ++i)
		unk0[i]->getModel()->setBaseScale(scale);
}

TEnemyManager::TEnemyManager(const char* name)
    : TLiveManager(name)
    , unk38(nullptr)
    , unk3C(1.0f)
    , unk40(nullptr)
    , unk44(0)
    , unk48(0)
    , unk4C(0xffffffff)
    , unk50(0)
{
	gpConductor->registerEnemyManager(this);
}

TEnemyManager::~TEnemyManager() { }

void TEnemyManager::createSharedMActorSet(const char** param_1)
{
	if (unk44 <= 0)
		return;

	u32 num = 0;
	for (int i = 0; param_1[i] != nullptr; ++i)
		++num;

	// TODO: ewwwwwwwwwwww
	u32 prev = (volatile int&)unk44;
	unk44    = num;
	unk40    = new TSharedMActorSet[unk44];

	for (int i = 0; i < unk44; ++i) {
		unk40[i].init(getMActorAnmData(),
		              getModelDataKeeper()->getNthData(0)->getModelData(),
		              param_1[i], prev);
	}
}

TSharedMActorSet* TEnemyManager::getSharedMActorSet(int idx)
{
	if (!unk40)
		return nullptr;

	for (int i = 0; i < unk44; ++i)
		if (idx == unk40[i].getIdx())
			return &unk40[i];

	return nullptr;
}

void TEnemyManager::load(JSUMemoryInputStream& stream)
{
	TLiveManager::load(stream);
	createModelData();
	stream >> unk44;
}

TSpineEnemy* TEnemyManager::createEnemyInstance() { return nullptr; }

// UNUSED (0xf8), inlined into createEnemies. The bool result is what gives
// the map size; getSaveParam() at both clamp sites lands the caller's frame.
bool TEnemyManager::createEnemy()
{
	TSpineEnemy* enemy = createEnemyInstance();
	if (enemy == nullptr)
		return false;
	TIdxGroupObj* group = JDrama::TNameRefGen::search<TIdxGroupObj>("敵グループ");
	group->getChildren().push_back(enemy);
	enemy->init(this);
	return true;
}

void TEnemyManager::createEnemies(int count)
{
	if (count + getObjNum() > getCapacity())
		count = getCapacity() - getObjNum();

	if (getSaveParam() != nullptr) {
		u8 limit = getSaveParam()->mSLInstanceNum.get();
		if (count + getObjNum() > limit)
			count = limit - getObjNum();
	}

	if (count >= 0)
		for (int i = 0; i < count; ++i)
			createEnemy();
}

void TEnemyManager::clipEnemies(JDrama::TGraphics* graphics)
{
	f32 fVar1;
	f32 fVar2;
	if (unk38 == nullptr) {
		fVar1 = 300.0f;
		fVar2 = gpConductor->unk84.mEnemyFarClip.get();
	} else {
		fVar2 = unk38->mSLFarClip.get();
		fVar1 = unk38->mSLClipRadius.get();
	}

	TLiveManager::clipActorsAux(graphics, fVar2, fVar1);
}

void TEnemyManager::setSharedFlags()
{
	if (!unk40)
		return;

	for (int i = 0; i < getObjNum(); ++i) {
		TSpineEnemy* enemy = getObj(i);
		enemy->offLiveFlag(LIVE_FLAG_UNK4000);
		if (!enemy->checkLiveFlag(LIVE_FLAG_HIDDEN | LIVE_FLAG_CLIPPED_OUT)) {
			int idx = enemy->getMActor()->getCurAnmIdx(ANM_TYPE_BCK);
			for (int i = 0; i < unk44; ++i) {
				if (idx < 0 || idx == unk40[i].getIdx()) {
					enemy->onLiveFlag(LIVE_FLAG_UNK4000);
					break;
				}
			}
		}
	}
}

void TEnemyManager::updateAnmSoundShared()
{
	if (!unk40)
		return;

	if (!getObj(0)->getAnmSound())
		return;

	for (int i = 0; i < getObjNum(); ++i) {
		TSpineEnemy* enemy = getObj(i);
		if (!enemy->checkLiveFlag(LIVE_FLAG_HIDDEN | LIVE_FLAG_CLIPPED_OUT)) {
			int idx = enemy->getMActor()->getCurAnmIdx(ANM_TYPE_BCK);
			for (int i = 0; i < unk44; ++i) {
				if (idx < 0 || idx == unk40[i].getIdx()) {
					// unused (mistake)
					J3DFrameCtrl* ctrl2
					    = unk40[i]
					          .getMActor(enemy->getInstanceIndex())
					          ->getFrameCtrl(ANM_TYPE_BCK);
					if (enemy->mAnmSoundPath) {
						J3DFrameCtrl* ctrl
						    = unk40[i]
						          .getMActor(enemy->getInstanceIndex())
						          ->getFrameCtrl(ANM_TYPE_BCK);

						enemy->getAnmSound()->animeLoop(
						    (Vec*)&enemy->getPosition(), ctrl->getFrame(),
						    ctrl->getRate(), 0, 4);
					}
					break;
				}
			}
		}
	}
}

// Binding level worth +8 of low region, landing
// TEnemyManager::copyFromShared's frame at 0x100 (batch 121).
static inline J3DModelData* EnemymanagerGetModelData(J3DModel* p)
{
	J3DModelData* modelData = p->getModelData();
	return modelData;
}

void TEnemyManager::copyFromShared()
{
	// The concat scratch is declared before the saved view matrix on purpose:
	// retail puts the saved view at 0x78 and the concat result at 0xa8, and
	// function-scope named locals are allocated downward from the frame top in
	// declaration order, so the concat matrix has to come first.
	Mtx concatMtx;
	Mtx viewMtx;
	MTXCopy(j3dSys.getViewMtx(), viewMtx);

	s32 r29 = getActiveObjNum();

	for (int i = 0; i < r29; ++i) {
		TSpineEnemy* enemy = getObj(i);
		if (!enemy->checkLiveFlag(LIVE_FLAG_UNK4000)
		    || enemy->checkLiveFlag(LIVE_FLAG_HIDDEN | LIVE_FLAG_CLIPPED_OUT))
			continue;

		int iVar6 = enemy->getMActor()->getCurAnmIdx(ANM_TYPE_BCK);
		for (int j = 0; j < unk44; ++j) {
			if (iVar6 >= 0 && iVar6 != unk40[j].unk8)
				continue;

			J3DModel* model
			    = unk40[j].getMActor(enemy->getInstanceIndex())->getModel();
			MtxPtr src = enemy->getModel()->getBaseTRMtx();
			MTXScaleApply(src, src, enemy->mScaling.x, enemy->mScaling.y,
			              enemy->mScaling.z);
			MTXConcat(viewMtx, src, concatMtx);
			j3dSys.setViewMtx(concatMtx);

			model->viewCalc();

			enemy->getModel()->swapAllMtx();
			enemy->getModel()->calcNrmMtx();
			enemy->getModel()->prepareShapePackets();

			J3DPSMtxArrayCopy(*model->getDrawMtxPtr(),
			                  *enemy->getModel()->getDrawMtxPtr(),
			                  EnemymanagerGetModelData(model)->getDrawMtxNum());

			DCStoreRange(enemy->getModel()->getDrawMtxPtr(),
			             model->getModelData()->getDrawMtxNum() * sizeof(Mtx));

			break;
		}
	}

	j3dSys.setViewMtx(viewMtx);
}

// The alive count is the inlined countLivingEnemy() (c-k5): as inliner
// objects its counter and the loop's byte offset share one zero (retail's
// `li r5, 0; addi r3, r5, 0`), which the old spelled-out loop with named
// locals could not give; countLivingEnemy reads the objects one level
// shallower than getObj() so that it still expands here at depth 2.
// The frame (0xf0) is the getObjNum() bound with raw unk18[i] in the
// no-collision loop (hsearch c-k12).
void TEnemyManager::performShared(u32 param_1, JDrama::TGraphics* param_2)
{
	if (unk30 & 1)
		TTimeRec::startTimer();

	if (countLivingEnemy() <= 0) {
		if ((unk30 & 1))
			TTimeRec::endTimer();
		return;
	}

	if (param_1 & CUE_CALC_ANIM) {
		clipEnemies(param_2);
		for (int i = 0; i < unk44; ++i)
			unk40[i].calcAnm();
		setSharedFlags();
		updateAnmSoundShared();
	}

	if (param_1 & CUE_CALC_VIEW)
		copyFromShared();

	if (unk30 & 1) {
		TTimeRec::endTimer();
		TTimeRec::startTimer(JUtility::TColor(0xff, 0x00, 0x00, 0xff));
	}

	int num = getActiveObjNum();
	if (param_1 & CUE_MOVE) {
		for (int i = num; i < getObjNum(); ++i)
			unk18[i]->onHitFlag(HIT_FLAG_NO_COLLISION);
	}

	for (int i = 0; i < num; ++i) {
		TSpineEnemy* enemy = getObj(i);
		if (enemy->checkLiveFlag(LIVE_FLAG_DEAD))
			continue;

		if (param_1 & CUE_MOVE)
			enemy->moveObject();

		if (param_1 & CUE_CALC_ANIM) {
			enemy->updateSquareToMario();
			enemy->calcRootMatrix();
			if (!enemy->checkLiveFlag(LIVE_FLAG_UNK4000)) {
				enemy->getMActor()->frameUpdate();
				if (!enemy->checkLiveFlag(LIVE_FLAG_HIDDEN
				                          | LIVE_FLAG_CLIPPED_OUT)) {
					enemy->getMActor()->calc();
					enemy->updateAnmSound();
				}
			} else {
				enemy->getMActor()->matAnmFrameUpdate();
			}
		}

		if (param_1 & CUE_CALC_VIEW)
			enemy->requestShadow();

		if (!enemy->checkLiveFlag(LIVE_FLAG_HIDDEN
		                          | LIVE_FLAG_CLIPPED_OUT)) {
			if ((param_1 & CUE_CALC_VIEW)
			    && !enemy->checkLiveFlag(LIVE_FLAG_UNK4000))
				enemy->getMActor()->viewCalc();
			if (param_1 & CUE_ENTRY) {
				enemy->getMActor()->setLightData(enemy->getGroundPlane(),
				                                 enemy->mPosition);
				enemy->getMActor()->entry();
			}
		}
	}

	if (unk30 & 1)
		TTimeRec::endTimer();
}

void TEnemyManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	changeDrawBuffer(cue);
	if (unk40) {
		performShared(cue, graphics);
		restoreDrawBuffer(cue);
		return;
	}

	if (unk30 & 1)
		TTimeRec::startTimer();

	if (cue & CUE_CALC_ANIM) {
		clipEnemies(graphics);
		setFlagOutOfCube();
	}

	if (unk30 & 1) {
		TTimeRec::endTimer();
		TTimeRec::startTimer(JUtility::TColor(0xff, 0x0, 0x0, 0xff));
	}

	int num = getActiveObjNum();
	// The cue test is inside the loop: MWCC unswitches it and leaves the
	// dead getObj() load out of the other copy, which is why retail has two
	// loops over the same range and only one of them touches the object.
	for (int i = num; i < mObjNum; ++i) {
		// One accessor level shallower than getObj(i): the three-level
		// TEnemyManager::getObj costs 8 bytes of frame here (0xa8 against
		// retail's 0xa0), and the THitActor* spelling without the cast
		// loses the register assignment.
		TSpineEnemy* enemy = (TSpineEnemy*)TObjManager::getObj(i);
		if (cue & CUE_MOVE)
			enemy->onHitFlag(HIT_FLAG_NO_COLLISION);
	}

	for (int i = 0; i < num; ++i)
		getObj(i)->testPerform(cue, graphics);

	restoreDrawBuffer(cue);
	if (unk30 & 1)
		TTimeRec::endTimer();
}

TSpineEnemy* TEnemyManager::getNearestEnemy(const JGeometry::TVec3<f32>& p)
{
	f32 dist          = 0.0f;
	TSpineEnemy* best = nullptr;

	for (int i = 0; i < getActiveObjNum(); ++i) {
		TSpineEnemy* candidate = getObj(i);

		if (candidate->checkLiveFlag(LIVE_FLAG_DEAD))
			continue;

		f32 d = VECDistance((Vec*)&p, (Vec*)&candidate->mPosition);
		if (!best || dist > d) {
			best = candidate;
			dist = d;
		}
	}

	return best;
}

TSpineEnemy* TEnemyManager::getDeadEnemy()
{
	s32 num = getActiveObjNum();
	for (int i = 0; i < num; ++i)
		if (getObj(i)->checkLiveFlag(LIVE_FLAG_DEAD))
			return getObj(i);

	return nullptr;
}

TSpineEnemy* TEnemyManager::getFarOutEnemy()
{
	f32 dist          = -1.0f;
	TSpineEnemy* best = nullptr;

	for (int i = 0; i < getActiveObjNum(); ++i) {
		TSpineEnemy* enemy = getObj(i);
		if (enemy->checkLiveFlag(LIVE_FLAG_DEAD)) {
			best = enemy;
			break;
		}

		if (!enemy->checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)
		    || !enemy->checkLiveFlag(LIVE_FLAG_UNK800))
			continue;

		if (!best || dist > enemy->getDistToMarioSquared()) {
			best = enemy;
			dist = enemy->getDistToMarioSquared();
		}
	}

	return best;
}

void TEnemyManager::killChildren()
{
	for (int i = 0; i < mObjNum; ++i)
		getObj(i)->onLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_UNK40);
}

void TEnemyManager::killChildrenWithin(const JGeometry::TVec3<f32>& p, f32 r)
{
	if (!mObjNum)
		return;

	if (!getObj(0)->checkActorType(ACTOR_TYPE_ENEMY)
	    && !getObj(0)->checkActorType(ACTOR_TYPE_BOSS))
		return;

	for (int i = 0; i < mObjNum; ++i) {
		TSpineEnemy* enemy = getObj(i);
		if (VECSquareDistance((Vec*)&p, (Vec*)&enemy->mPosition) <= r * r)
			enemy->onLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_UNK40);
	}
}

// UNUSED (0xa4). Kills the enemies beyond the active count; it requests no
// literal, as retail's .sdata2 order requires of everything between
// createCopyAnmMtx and getFarOutEnemy.
// TODO: 0xa8, one instruction over the map; the conductor-list spelling
// (killing every other manager's children) is 0xbc and plain onLiveFlag 0x70.
void TEnemyManager::killOtherEnemies()
{
	for (int i = getActiveObjNum(); i < mObjNum; ++i) {
		TSpineEnemy* enemy = getObj(i);
		if (!enemy->checkLiveFlag(LIVE_FLAG_DEAD))
			enemy->kill();
	}
}

int TEnemyManager::countLivingEnemy() const
{
	s32 num = getActiveObjNum();

	int result = 0;
	for (int i = 0; i < num; ++i)
		if (!((const TSpineEnemy*)TObjManager::getObj(i))
		         ->checkLiveFlag(LIVE_FLAG_DEAD))
			++result;

	return result;
}

// UNUSED (0x15c). Bakes bck `idx` of the first enemy into per-frame joint
// matrices, which copyAnmMtx concatenates with each enemy's scaled base
// matrix. `new TPosition3f[]` is what emits the dead-stripped weak
// TPosition3 ctor the map lists right after this function, and the (f32)
// frame index requests the int-to-float double that retail's .sdata2 puts
// first. The body is reconstructed from the map size and copyAnmMtx's use of
// unk48/unk4C/unk50, not from retail code.
// TODO: 0x160, one instruction over the map.
void TEnemyManager::createCopyAnmMtx(int idx)
{
	unk4C = idx;

	MActor* actor = getObj(0)->getMActor();
	int prev = actor->getCurAnmIdx(ANM_TYPE_BCK);
	actor->setBckFromIndex(idx);
	J3DModel* model = actor->getModel();
	unk50           = model->getModelData()->getJointNum();

	J3DFrameCtrl* ctrl = actor->getFrameCtrl(ANM_TYPE_BCK);
	int frames         = ctrl->getEnd();
	unk48              = new TPosition3f*[frames];
	for (int f = 0; f < frames; ++f) {
		unk48[f] = new TPosition3f[unk50];
		ctrl->setFrame(f);
		MTXIdentity(model->getBaseTRMtx());
		actor->calc();
		for (int i = 0; i < unk50; ++i)
			MTXCopy(model->getAnmMtx(i), unk48[f][i]);
	}
	actor->setBckFromIndex(prev);
}

// Binding level that lands copyAnmMtx's frame at retail's 0xc0 (0xb0
// without it). Applied at one site only -- the level saturates per receiver.
static inline MActor* EnemymanagerGetMActor(const TSpineEnemy* p)
{
	MActor* actor = p->getMActor();
	return actor;
}

// Closed (c-k5). The scratch matrix is a TPosition3f, as `unk48` is (the
// dead-stripped weak TPosition3 ctor after createCopyAnmMtx): its conversion
// operator is what keeps the matrix address in r28 across the loop, which a
// fabricated `MtxPtr wtf = afStack;` used to imitate. The scale reads are
// raw `mScaling` (a named `getScaling()` reference left a dead word under the
// matrix, 0x68 for retail's 0x64), and the frame index is read straight from
// `getCurAnmFrameNo`, so the raw value and its `slwi` share r27.
// TODO: EnemymanagerGetMActor is an identity binder kept only for its +0x10
// of frame (0xb0 without it); every raw/named/accessor spelling of the first
// test and every getModel() subset of the other three sites is worse.
bool TEnemyManager::copyAnmMtx(TSpineEnemy* enemy)
{
	if (unk4C != EnemymanagerGetMActor(enemy)->getCurAnmIdx(ANM_TYPE_BCK))
		return false;

	int f = enemy->getCurAnmFrameNo(ANM_TYPE_BCK);
	enemy->calcRootMatrix();
	enemy->updateAnmSound();
	enemy->getMActor()->frameUpdate();

	TPosition3f concat;
	MtxPtr mtx = enemy->getMActor()->getModel()->getBaseTRMtx();

	mtx[0][0] *= enemy->mScaling.x;
	mtx[0][1] *= enemy->mScaling.y;
	mtx[0][2] *= enemy->mScaling.z;
	mtx[1][0] *= enemy->mScaling.x;
	mtx[1][1] *= enemy->mScaling.y;
	mtx[1][2] *= enemy->mScaling.z;
	mtx[2][0] *= enemy->mScaling.x;
	mtx[2][1] *= enemy->mScaling.y;
	mtx[2][2] *= enemy->mScaling.z;

	for (int i = 0; i < unk50; ++i) {
		MTXConcat(mtx, unk48[f][i].mMtx, concat);
		enemy->getMActor()->getModel()->setAnmMtx(i, concat);
	}

	if (enemy->getMActor()->getModel()->getModelData()->getWEvlpMtxNum())
		enemy->getModel()->calcWeightEnvelopeMtx();

	return true;
}
