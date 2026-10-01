#include <Enemy/Cannon.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <System/Application.hpp>
#include <System/Particles.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MAnmSound.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <Player/MarioAccess.hpp>
#include <Enemy/bombhei.hpp>
#include <Enemy/popo.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <JSystem/JMath.hpp>
#include <Camera/cameralib.hpp>
#include <Enemy/killer.hpp>
#include <Enemy/Igaiga.hpp>
#include <Enemy/Graph.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <Strategic/Strategy.hpp>
#include <Enemy/PathNode.hpp>
#include <System/MarDirector.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/EffectObj.hpp>
#include <System/FlagManager.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

#define SQ(x) ((x) * (x))

// NOTE: this TU is -inline deferred, so the out-of-line functions below are
// defined in the *reverse* order of mario.MAP's .text layout.

static const char* cannon_bastable[] = {
	nullptr,
	nullptr,
	"/scene/cannon/bas/CannonDom_break.bas",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/cannon/bas/tyorobe_appear1.bas",
	"/scene/cannon/bas/tyorobe_damage1.bas",
	"/scene/cannon/bas/tyorobe_down1.bas",
	"/scene/cannon/bas/tyorobe_hyde1.bas",
	"/scene/cannon/bas/tyorobe_throw1.bas",
	nullptr,
	nullptr,
	nullptr,
};

// TODO: UNUSED statics also listed in the map: mTestAngY, mShootAngVal
// (.sdata) and mThrowOffsetY, mCannonOffset (.sbss); their values are
// unknown.
u8 TCannon::mChorobeiJntIdx     = 4;
u8 TCannon::mChorobeiHandJntIdx = 4;
f32 TCannon::mVelocityRate      = 0.62f;
f32 TCannon::mSearchRate        = 0.02f;

TCannonSaveLoadParams::TCannonSaveLoadParams(const char* path)
    : TSmallEnemyParams(path)
    , PARAM_INIT(mSLHideDist, 300.0f)
    , PARAM_INIT(mSLBombDist, 2000.0f)
    , PARAM_INIT(mSLKillerDist, 10000.0f)
    , PARAM_INIT(mSLBombInterval, 100)
    , PARAM_INIT(mSLKillerInterval, 50)
    , PARAM_INIT(mSLShootInterval, 100)
    , PARAM_INIT(mSLChorobeiAttackRadius, 100.0f)
    , PARAM_INIT(mSLChorobeiAttackHeight, 100.0f)
    , PARAM_INIT(mSLChorobeiDamageRadius, 100.0f)
    , PARAM_INIT(mSLChorobeiDamageHeight, 100.0f)
    , PARAM_INIT(mSLKillerTransYOffset, -50.0f)
    , PARAM_INIT(mSLBombHeiGenerateRate, 0.7f)
    , PARAM_INIT(mSLThrowXZSpeed, 12.0f)
{
	TParams::load(mPrmPath);
}

TCannonManager::TCannonManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TCannonManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TCannonSaveLoadParams("/enemy/cannon.prm");
}

TSmallEnemy* TCannonManager::createEnemyInstance()
{
	return new TCannon("砲台");
}

// ============= TChorobei =============

TChorobei::TChorobei(TCannon* cannon, int jointIndex, const char* name)
    : THitActor(name)
    , mCannon(cannon)
    , mParts(nullptr)
    , unk70(0.0f)
    , mAnmSound(nullptr)
    , unk78(0)
    , unk7C(300.0f)
{
	mParts = new TSharedParts(mCannon, jointIndex,
	                          "/scene/cannon/tyorobe_model1.bmd", 0x10020000, 3);
	if (mAnmSound)
		return;

	mAnmSound = new MAnmSound(gpMSound);
	mAnmSound->initAnmSound(nullptr, 1, 0.0f);
}

void TChorobei::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (!mCannon->checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN
	                            | LIVE_FLAG_CLIPPED_OUT)
	    && unk70 == 0.0f) {
		if (cue & 2) {
			if (mAnmSound != nullptr && unk78 != 0) {
				J3DFrameCtrl* ctrl
				    = mParts->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
				mAnmSound->animeLoop((Vec*)&mPosition, ctrl->getFrame(),
				                     ctrl->getRate(), 0, 4);
			}

			Mtx mtx;
			MTXCopy(mParts->getConnectedMtx(), mtx);
			mtx[1][3] += unk7C;
			MTXCopy(mtx, mParts->getMActor()->getModel()->getBaseTRMtx());
			mPosition.x = mtx[0][3];
			mPosition.y = mtx[1][3] - 150.0f;
			mPosition.z = mtx[2][3];
		}
		THitActor::perform(cue, graphics);
		mParts->getMActor()->perform(cue, graphics);
	}
}

void TChorobei::setBckAnm(int index)
{
	getMActor()->setBckFromIndex(index);
	unk78 = mCannon->getBas(index);
	if (unk78 != 0) {
		mAnmSound->initAnmSound(JKRFileLoader::getGlbResource(unk78), 1, 0.0f);
	} else {
		mAnmSound->initAnmSound(nullptr, 1, 0.0f);
	}
}

void TChorobei::checkHit()
{
	for (int i = 0; i < mColCount; ++i) {
		THitActor* other = mCollisions[i];
		if (other->isActorType(0x80000001))
			SMS_SendMessageToMario(this, 0xE);

		if (other->isActorType(0x1000001E)) {
			TCannon* cannon = mCannon;
			if (cannon->mSpine->getCurrentNerve()
			        != &TNerveCannonDamage::theNerve()
			    && ((TBombHei*)other)->isDamageToCannon()) {
				cannon->mSpine->pushNerve(&TNerveCannonDamage::theNerve());
				((TLiveActor*)other)->kill();
			}
		}

		if (other->isActorType(0x1000001F)
		    && ((TKiller*)other)->isRollFly()) {
			mCannon->mSpine->pushNerve(&TNerveCannonDamage::theNerve());
			((TLiveActor*)other)->kill();
		}
	}
}

BOOL TChorobei::receiveMessage(THitActor* sender, u32 message) { return FALSE; }

bool TChorobei::isUpEnd()
{
	if (getMActor()->curAnmEndsNext()
	    && getMActor()->checkCurBckFromIndex(0xC))
		return true;

	unk70 = 0.0f;
	return false;
}

bool TChorobei::isDownEnd()
{
	if (getMActor()->curAnmEndsNext()
	    && getMActor()->checkCurBckFromIndex(0xF)) {
		unk70 = 1.0f;
		return true;
	}

	return false;
}

// ============= TCannonDom =============

TCannonDom::TCannonDom(TLiveActor* owner, int jointIndex,
                       SDLModelData* modelData, u32 flags, const char* name)
    : TSharedParts(owner, jointIndex, modelData, flags, name)
    , mAnmSound(nullptr)
    , unk20(0)
    , unk24(0)
    , unk28(0.0f)
    , unk2C(0.0f)
    , unk30(0.0f)
{
	unk30 = TMsRange<f32>(0.0f, 360.0f).rand();
	if (mAnmSound)
		return;

	mAnmSound = new MAnmSound(gpMSound);
	mAnmSound->initAnmSound(nullptr, 1, 0.0f);
}

void TCannonDom::perform(u32 cue, JDrama::TGraphics* graphics)
{
	Mtx rot;
	if (!unk10->checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN
	                          | LIVE_FLAG_CLIPPED_OUT)) {
		if (cue == 2) {
			if (mAnmSound != nullptr && unk20 != 0) {
				J3DFrameCtrl* ctrl = getMActor()->getFrameCtrl(ANM_TYPE_BCK);
				mAnmSound->animeLoop((Vec*)&unk10->mPosition, ctrl->getFrame(),
				                     ctrl->getRate(), 0, 4);
			}

			MtxPtr mtx = (MtxPtr)getConnectedMtx();
			MsMtxSetRotRPH(rot, unk28, unk2C, 0.0f);
			MTXConcat(mtx, rot, mtx);
			MTXCopy(mtx, getMActor()->getModel()->getBaseTRMtx());
		}
		getMActor()->perform(cue, graphics);
	}
}

void TCannonDom::setBckAnm(int index)
{
	getMActor()->setBckFromIndex(index);
	unk20 = unk10->getBas(index);
	if (unk20 != nullptr) {
		void* res = JKRFileLoader::getGlbResource(unk20);
		mAnmSound->initAnmSound(res, 1, 0.0f);
	} else {
		mAnmSound->initAnmSound(nullptr, 1, 0.0f);
	}
}

// ============= TCannon =============

TCannon::TCannon(const char* name)
    : TSmallEnemy(name)
    , unk1A0(nullptr)
    , unk1A8(nullptr)
    , unk1B8(nullptr)
    , unk1E0(nullptr)
    , unk214(0)
    , unk218(0)
    , unk21C(0)
    , unk220(0.0f)
    , unk230(1)
    , unk238(0)
    , unk239(1)
    , unk254(nullptr)
    , unk258(nullptr)
    , unk290(0)
    , unk2AC(0.0f)
{
	unk224.set(0.0f, 0.0f, 0.0f);
}

void TCannon::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	reset();
	mHitPoints = getMaxHitPoints();
	unk230     = gpApplication.mCurrArea.unk0;
	unk23C     = mPosition;
}

void TCannon::loadAfter()
{
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_a.jpa", 0xE8);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_b.jpa", 0xE9);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_c.jpa", 0xEA);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_d.jpa", 0xEB);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_e.jpa", 0xEC);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_smoke.jpa", 0x166);
	if (unk230 == 5) {
		unk254 = (TSpineEnemy*)JDrama::TNameRefGen::search("efMareGate");
		unk254->kill();
	}
}

void TCannon::init(TLiveManager* manager)
{
	TSmallEnemy::init(manager);
	mActorType = 0x1000001C;
	unk150     = 0x11;
	unk28C     = (TCannonSaveLoadParams*)getSaveParam();
	setBckAnm(3);

	void* domRes = JKRFileLoader::getGlbResource("/scene/cannon/cannon_Dom.bmd");
	SDLModelData* domModel = new SDLModelData(
	    J3DModelLoaderDataBase::load(domRes, J3DMLF_MaterialPEFull
	                                             | (5 << J3DMLF_TevStageNumShift)));

	unk234 = mRotation.y;
	unk230 = gpApplication.mCurrArea.unk0;

	if (unk230 == 5 || unk230 == 9) {
		mSpine->initWith(&TNerveCannonSearch::theNerve());

		if (unk230 == 9) {
			unk239 = 0;
			setGoalPath(
			    TPathNode(JGeometry::TVec3<f32>(-565.0f, 8500.0f, 7675.0f)));
		} else {
			setGoalPath(TPathNode((THitActor*)gpMarioAddress));
		}

		unk1A8 = new TChorobei(this, 0, "チョロベー");
		for (u8 i = 0; i < unk1A8->getMActor()->getModel()->getModelData()->getJointNum(); ++i) {
		}
		TCannonSaveLoadParams* params = getSLParams();
		unk1A8->initHitActor(0x1000001D, 3, 0x90000000,
		                     params->mSLChorobeiAttackRadius.get(),
		                     params->mSLChorobeiAttackHeight.get(),
		                     params->mSLChorobeiDamageRadius.get(),
		                     params->mSLChorobeiDamageHeight.get());
		static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
		    ->getChildren()
		    .push_back(unk1A8);

		static const char* sCannonDomPartsJointTable[]
		    = { "nullC", "nullB", "nullA" };
		JUTNameTab* jntNames
		    = getMActor()->getModel()->getModelData()->getJointName();
		for (int i = 0; i < 3; ++i) {
			int jointIndex = jntNames->getIndex(sCannonDomPartsJointTable[i]);
			unk1AC[i]      = new TCannonDom(this, jointIndex, domModel, 3, "砲身");
			unk1C0[i]      = new TMapCollisionMove;
			unk1C0[i]->init("/cannon/CannonDom", 2, this);
			unk1C0[i]->setUpTrans(mPosition);
		}

		unk2B0 = new TMapCollisionMove;
		unk2B0->init("/cannon/CannonFuta", 2, this);
		unk2B0->setUpTrans(mPosition);
	} else {
		mSpine->initWith(&TNerveCannonObject::theNerve());
		int jointIndex = getMActor()->getModel()->getModelData()->getJointName()->getIndex("nullA");
		unk1B8 = new TCannonDom(this, jointIndex, domModel, 3, "砲身");
		unk1B8->unk24 = 1;
	}

	void* hodaiRes = JKRFileLoader::getGlbResource("/scene/cannon/hodai_mario.bmd");
	SDLModelData* hodaiModel = new SDLModelData(
	    J3DModelLoaderDataBase::load(hodaiRes, J3DMLF_MaterialPEFull
	                                               | (1 << J3DMLF_TevStageNumShift)));
	unk1BC = new TSharedParts(this, 0, hodaiModel, 3, "<TSharedParts>");
	for (u8 i = 0; i < getModel()->getModelData()->getJointNum(); ++i) {
	}

	unk258 = new TMapCollisionMove;
	unk258->init(2, 0, 0, nullptr);
}

void TCannon::reset()
{
	TSmallEnemy::reset();
	mHitPoints = getMaxHitPoints();
	onHitFlag(HIT_FLAG_NO_COLLISION);
	unk214      = 1;
	mHeadHeight = 40.0f;
	onLiveFlag(LIVE_FLAG_UNK10);
	unk2AC = mRotation.y;
	if (unk230 == 9) {
		unk239 = 0;
		setGoalPath(TPathNode(JGeometry::TVec3<f32>(-565.0f, 8500.0f, 7675.0f)));
	}
}

void TCannon::moveObject()
{
	TSmallEnemy::moveObject();

	if (unk230 == 5 || unk230 == 9) {
		if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
			getChorobei()->mPosition = mPosition;
		} else {
			MtxPtr mtx = getModel()->getAnmMtx(mChorobeiJntIdx);
			getChorobei()->mPosition.x = mtx[0][3];
			getChorobei()->mPosition.y = mtx[1][3] - 100.0f;
			getChorobei()->mPosition.z = mtx[2][3];
		}
		getChorobei()->checkHit();

		JGeometry::TVec3<f32> vel = mVelocity;
		mPosition.y += vel.y;
		mVelocity.y -= getGravityY();
		if (mPosition.y < unk23C.y) {
			mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
			mPosition.y     = unk23C.y;
		}

		if (unk1A0) {
			unk1A0->mPosition = unk194 + getChorobei()->mPosition;
			unk1A0->offLiveFlag(LIVE_FLAG_AIRBORNE);

			if (mSpine->getCurrentNerve() == &TNerveCannonClose::theNerve()) {
				unk1A0->kill();
				unk1A0 = nullptr;
			} else if (!unk1A0->doKeepDistance()) {
				unk1A0 = nullptr;
				damage();
			}
		}
	}
}

BOOL TCannon::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->getActorType() == 0x40000235 && message == HIT_MESSAGE_TAKE
	    && mHolder == nullptr) {
		mHolder = (TTakeActor*)sender;
		return TRUE;
	}

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER)
		return TRUE;

	return FALSE;
}

void TCannon::calcObjCollision()
{
	// TODO: UNUSED in the target (0x12C bytes), not yet reconstructed
}

void TCannon::entryObjCollision()
{
	// TODO: UNUSED in the target (0x58 bytes), not yet reconstructed
}

const char** TCannon::getBasNameTable() const { return cannon_bastable; }

void TCannon::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);

	if (unk238) {
		unk1BC->getMActor()->perform(cue, graphics);

		if (cue & 1) {
			if (unk1BC->getMActor()->curAnmEndsNext())
				unk238 = 0;
		}

		if (cue & 2) {
			if (unk230 == 1) {
				if (unk1BC->getMActor()
				        ->getFrameCtrl(ANM_TYPE_BCK)
				        ->checkPass(174.0f)) {
					MtxPtr mtx = unk1B8->getMActor()->getModel()->getAnmMtx(0);
					gpMarioParticleManager->emitAndBindToMtxPtr(0xE8, mtx, 0,
					                                            nullptr);
					gpMarioParticleManager->emitAndBindToMtxPtr(0xE9, mtx, 0,
					                                            nullptr);
					gpMarioParticleManager->emitAndBindToMtxPtr(0xEA, mtx, 0,
					                                            nullptr);
					gpMarioParticleManager->emitAndBindToMtxPtr(0xEB, mtx, 0,
					                                            nullptr);
					gpMarioParticleManager->emitAndBindToMtxPtr(0xEC, mtx, 0,
					                                            nullptr);
				}

				if (unk1BC->getMActor()
				        ->getFrameCtrl(ANM_TYPE_BCK)
				        ->getFrame()
				    > 175.0f) {
					gpMarioParticleManager->emitAndBindToMtxPtr(
					    0x166,
					    unk1BC->getMActor()->getModel()->getAnmMtx(0), 1, this);
				}
			}
		}
	}

	if (unk230 == 5 || unk230 == 9) {
		unk1A8->perform(cue, graphics);

		if (cue & 0x200) {
			if (mSpine->getCurrentNerve() == &TNerveCannonDamage::theNerve()) {
				unk1A8->getMActor()->offMakeDL();
				SMS_AddDamageFogEffect(
				    unk1A8->getMActor()->getModel()->getModelData(),
				    mPosition, graphics);
			}
		}

		for (int i = 0; i < 3; ++i) {
			if (unk1AC[i]->unk24 == 0) {
				if (cue == 1) {
					if (SQ(unk28C->mSLBombDist.get()) < mDistToMarioSquared
					    && unk230 == 5) {
						unk1AC[i]->unk2C = (&unk224.x)[i];
						if (i != unk214
						    && mSpine->getCurrentNerve()
						           != &TNerveCannonObject::theNerve()) {
							unk1AC[i]->unk30 += 1.0f;
							if (unk1AC[i]->unk30 > 360.0f)
								unk1AC[i]->unk30 -= 360.0f;

							if (i == 2)
								unk1AC[i]->unk28
								    = -10.0f
								      * JMASSin(182.04445f
								                * (0.5f * unk1AC[i]->unk30));
							else
								unk1AC[i]->unk28
								    = -20.0f
								      * JMASSin(182.04445f
								                * (0.5f * unk1AC[i]->unk30));
						}
					} else {
						unk1AC[i]->unk30 = 0.0f;
						unk1AC[i]->unk28 *= 0.99f;
						unk1C0[i]->moveMtx((MtxPtr)unk1AC[i]->getConnectedMtx());
					}
				}
				unk1AC[i]->perform(cue, graphics);
			}
		}
	} else {
		if (unk1B8->unk24 == 0)
			unk1B8->perform(cue, graphics);
	}
}

void TCannon::calcRootMatrix()
{
	if (mSpine->getCurrentNerve() != &TNerveCannonObject::theNerve()) {
		static const f32 xzTable[4][2] = {
			{ 1.0f, -1.0f },
			{ 1.0f, 1.0f },
			{ -1.0f, 1.0f },
			{ -1.0f, -1.0f },
		};

		mHeadHeight = 50.0f;

		JGeometry::TVec3<f32> base = mPosition;
		base.y = getMActor()->getModel()->getAnmMtx(4)[1][3];
		for (int i = 0; i < 4; ++i) {
			unk25C[i] = base;
			unk25C[i].x += 200.0f * xzTable[i][0];
			unk25C[i].z += 200.0f * xzTable[i][1];
		}
		unk258->setVertexData(0, unk25C[2], unk25C[1], unk25C[0]);
		unk258->setVertexData(1, unk25C[0], unk25C[3], unk25C[2]);
	}

	if (mHolder != nullptr) {
		MtxPtr mtx = mHolder->getTakingMtx();
		if (mSpine->getCurrentNerve() == &TNerveCannonObject::theNerve()) {
			MTXCopy(mtx, getModel()->getBaseTRMtx());
			mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);
		} else {
			if (gpMarDirector->isDemoModeNow())
				mRotation.y = -80.0f;

			mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);
			MsMtxSetXYZRPH(getMActor()->getModel()->getBaseTRMtx(),
			               mPosition.x, mPosition.y, mPosition.z, mRotation.x,
			               mRotation.y, mRotation.z);
		}
	} else {
		TSpineEnemy::calcRootMatrix();
	}

	if (unk1A8 && unk1A8->getMActor()->checkCurBckFromIndex(0xE)
	    && unk1A8->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(
	        2.0f)) {
		for (int i = 0; i < 3; ++i) {
			MtxPtr mtx = unk1AC[i]->getConnectedMtx();
			unk294.set(mtx[0][3], mtx[1][3], mtx[2][3]);
			unk1AC[i]->setBckAnm(2);
			gpMarioParticleManager->emit(0x14, &unk294, 0, nullptr);
			gpMarioParticleManager->emit(0x13, &unk294, 0, nullptr);
			gpMarioParticleManager->emit(0x12, &unk294, 0, nullptr);
		}
	}
}

MtxPtr TCannon::getTakingMtx()
{
	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		unk1E4.translation(unk1A8->mPosition.x, 800.0f + unk1A8->mPosition.y,
		                   unk1A8->mPosition.z);
		return unk1E4;
	} else {
		unk1E4.translation(
		    unk1A8->mPosition.x,
		    unk1A8->getMActor()
		        ->getModel()
		        ->getAnmMtx(mChorobeiHandJntIdx)[1][3],
		    unk1A8->mPosition.z);
		return unk1E4;
	}
}

void TCannon::bombSet()
{
	f32 chance = TMsRange<f32>(0.0f, 1.0f).rand();
	TCannonSaveLoadParams* params = unk28C;
	unk21C = 0;
	unk1A4 = nullptr;

	TSpineEnemy* enemy;
	if (chance < params->mSLBombHeiGenerateRate.get()) {
		enemy = gpConductor->makeOneEnemyAppear(mPosition,
		                                        "ボム兵マネージャー", 1);
	} else if (TMsRange<s32>(0, 100).rand() % 2 == 1) {
		enemy = gpConductor->makeOneEnemyAppear(mPosition, "ポポマネージャー", 1);
		if (enemy)
			((TPopo*)enemy)->thrownByChorobei();
	} else {
		enemy = gpConductor->makeOneEnemyAppear(mPosition,
		                                        "ハムクリマネージャー", 1);
	}

	if (unk1A4 == nullptr) {
		unk1A4 = enemy;
		if (enemy) {
			enemy->reset();
			enemy->getMActor()->setFrameRate(0.0f, 0);
		}
	}

	if (unk1A4) {
		unk1A4->mPosition = unk1A8->mPosition;
		unk220            = unk1A4->mScaling.x;
		unk1A4->mScaling.set(0.0f, 0.0f, 0.0f);
		unk1A4->mRotation = mRotation;
		if (unk1A4->receiveMessage(this, HIT_MESSAGE_TAKE))
			mHeldObject = unk1A4;
	}
}

void TCannon::bombShoot()
{
	if (unk1A4) {
		JGeometry::TVec3<f32> dir(gpMarioPos->x - mPosition.x, 0.0f,
		                          gpMarioPos->z - mPosition.z);
		if (dir.x == 0.0f && dir.z == 0.0f)
			dir.x = 1.0f;
		MsVECNormalize(&dir, &dir);

		TMsRange<f32> range(-30.0f, 30.0f);
		Mtx mtx;
		MsMtxSetRotRPH(mtx, 0.0f, mRotation.y + range.rand(), 0.0f);

		f32 speed = unk28C->mSLThrowXZSpeed.get();
		dir.y     = speed;
		dir.x *= speed;
		dir.z *= speed;

		if (unk21C) {
			unk1A4->mVelocity.set(dir.x, dir.y, dir.z);
			unk1A4->offLiveFlag(LIVE_FLAG_UNK10);
		} else {
			unk1A4->mVelocity = dir;
			unk1A4->onLiveFlag(LIVE_FLAG_AIRBORNE);
			unk1A4->getMActor()->setFrameRate(SMSGetAnmFrameRate(), 0);
		}

		unk1A4->mPosition.y += 2.0f;
		unk1A4->receiveMessage(this, HIT_MESSAGE_PUT);
	}
}

void TCannon::bombScaleUp()
{
	if (unk1A4) {
		f32 add            = 0.2f * unk220;
		unk1A4->mScaling.x = MsClamp(unk1A4->mScaling.x + add, 0.0f, unk220);
		unk1A4->mScaling.set(mScaling.x, mScaling.x, mScaling.x);
	}
}

void TCannon::hitHead(TBombHei*)
{
	// TODO: UNUSED in the target (0x154 bytes), not yet reconstructed
}

void TCannon::updateAttachPos()
{
	// TODO: UNUSED in the target (0x1C0 bytes), not yet reconstructed
}

void TCannon::killerShoot()
{
	if (unk239) {
		TKiller* killer = (TKiller*)gpConductor->makeOneEnemyAppear(
		    mPosition, "キラーマネージャー", 0);
		if (killer) {
			killer->reset();

			unk1E0 = unk1AC[unk214]->getMActor()->getModel()->getBaseTRMtx();
			unk1E0 = unk1AC[unk214]->getMActor()->getModel()->getAnmMtx(1);
			TPosition3f mtx;
			JGeometry::TVec3<f32> vel;
			JGeometry::TVec3<f32> target;
			mtx.translation(0.0f, -60.0f, 150.0f);
			PSMTXConcat(unk1E0, mtx, mtx);
			mtx.getTrans(killer->mPosition);

			f32 marioSpeedX = *gpMarioSpeedX;
			f32 marioSpeedZ = *gpMarioSpeedZ;
			target = *gpMarioPos;
			TMsRange<f32> range(-300.0f, 300.0f);
			target.x += range.rand();
			switch (unk214) {
			case 0:
				target.z -= 2.0f * fabsf(range.rand());
				break;
			case 2:
				target.z += 2.0f * fabsf(range.rand());
				break;
			}

			vel = killer->calcVelocityToJumpToY(target, 5.0f,
			                                   killer->getGravityY());
			JGeometry::TVec3<f32> delta;
			delta = target - mPosition;
			f32 flightTime = fabsf(MsVECMag2(delta)
			                       / (vel.y * mVelocityRate));

			killer->unk1A5 = 0;
			f32 speedRate = mVelocityRate;
			if (TMsRange<s32>(0, 100).rand() % 5 == 0) {
				killer->unk1A5 = 1;
			} else {
				if (*gpMarioSpeedX > 2.0f)
					speedRate = 0.55f;
				if (*gpMarioSpeedX < -2.0f)
					speedRate = 0.68f;
			}

			JGeometry::TVec3<f32> lead;
			lead = JGeometry::TVec3<f32>(
			    mSearchRate * (marioSpeedX * flightTime) + target.x, target.y,
			    mSearchRate * (marioSpeedZ * flightTime) + target.z);
			vel = killer->calcVelocityToJumpToY(lead, 5.0f,
			                                   killer->getGravityY());
			vel.scale(speedRate);

			f32 rotY = MsGetRotFromZaxisY(vel);
			killer->mRotation.set(0.0f, MsWrap(rotY, 0.0f, 360.0f), 0.0f);
			killer->mScaling.set(0.1f, 0.1f, 0.1f);

			if (gpMarDirector->mState == 1) {
				vel.x *= 0.2f;
				vel.y *= 0.4f;
				vel.z *= 0.2f;
				killer->mScaling.set(0.6f, 0.6f, 0.6f);
			}

			killer->setColorType();
			killer->unk1A8    = vel;
			killer->mVelocity = vel;
			killer->onLiveFlag(LIVE_FLAG_AIRBORNE);

			JGeometry::TVec3<f32> diff;
			diff = *gpMarioPos - mPosition;
			target.x += diff.x;
			target.z += diff.z;
			killer->setGoalPath(TPathNode(target));

			if (gpMSound->gateCheck(0x285D))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x285D, &killer->mPosition, 0, nullptr, 0, 4);
			if (gpMSound->gateCheck(0x20A9))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x20A9, &killer->mPosition, 0, nullptr, 0, 4);
		}
	} else {
		TIgaiga* iga = (TIgaiga*)gpConductor->makeOneEnemyAppear(
		    mPosition, "イガイガマネージャー", 1);
		if (iga) {
			iga->reset();

			unk1E0 = unk1AC[unk214]->getMActor()->getModel()->getAnmMtx(1);
			TPosition3f mtx;
			mtx.translation(0.0f, -60.0f, 150.0f);
			PSMTXConcat(unk1E0, mtx, mtx);
			mtx.getTrans(iga->mPosition);

			JPABaseEmitter* emitter = gpMarioParticleManager->emitWithRotate(
			    0xCB, &iga->mPosition, 0, (s16)(182.04445f * iga->mRotation.y),
			    0, 0, nullptr);
			if (emitter) {
				JGeometry::TVec3<f32> sc(mScaling.x * 1.5f, mScaling.y * 1.5f,
				                        mScaling.z * 1.5f);
				emitter->setGlobalScale(sc);
			}

			JGeometry::TVec3<f32> pt;
			iga->getTracer()->getGraph()->getGraphNode(0).getPoint(&pt);
			unk248 = pt;

			JGeometry::TVec3<f32> vel = iga->calcVelocityToJumpToY(
			    pt, 10.0f, iga->getGravityY());
			iga->mRotation = mRotation;
			iga->shoot(vel);
		}
	}
}

void TCannon::endKillerShoot()
{
	// TODO: UNUSED in the target (0x24 bytes), not yet reconstructed
}

void TCannon::damage()
{
	mSpine->pushNerve(&TNerveCannonDamage::theNerve());
}

void TCannon::setKillerGoalPoint()
{
	if (unk239) {
		s16 angle = TMsRange<f32>(0.0f, 360000.0f).rand();
		JGeometry::TVec3<f32> pos = *gpMarioPos;
		pos.x += 500.0f * JMASCos(angle);
		pos.z += 500.0f * JMASSin(angle);
		setGoalPath(TPathNode(pos));
	} else {
		setGoalPath(TPathNode(unk248));
	}

	unk1AC[unk214]->setBckAnm(1);
}

void TCannon::deadCannon()
{
	// TODO: UNUSED in the target (0x20 bytes), not yet reconstructed
}

void TCannon::startDemo() { }

void TCannon::startMarioDemo() { }

void TCannon::turnToGoal()
{
	// TODO: UNUSED in the target (0x2C bytes), not yet reconstructed
}

bool TCannon::isObject()
{
	if (isBckAnm(4) && checkCurAnmEnd(0))
		return true;

	return false;
}

void TCannon::killShootAct()
{
	// TODO: UNUSED in the target (0x3C bytes), not yet reconstructed
}

void TCannon::gateOpen()
{
	// TODO: UNUSED in the target (0xD0 bytes), not yet reconstructed
}

void TCannon::startChorobeiShout() { }

// ============= nerves =============

DEFINE_NERVE(TNerveCannonOpen, TLiveActor)
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() == 0) {
		self->getChorobei()->setBckAnm(0xC);
		self->setBckAnm(3);
	}

	if (self->getChorobei()->isUpEnd() && self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveCannonSearch::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveCannonSearch, TLiveActor)
{
	TCannon* self = (TCannon*)spine->getBody();

	self->updateSquareToMario();
	f32 bombSq = SQ(self->getSLParams()->mSLBombDist.get());
	f32 distSq = self->mDistToMarioSquared;

	if (spine->getTime() == 0) {
		if (distSq < bombSq)
			self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));

		self->getChorobei()->setBckAnm(0x13);
	}

	if (self->getChorobei()->getMActor()->curAnmEndsNext()
	    && self->getChorobei()->getMActor()->checkCurBckFromIndex(0x13))
		self->getChorobei()->setBckAnm(0x12);

	if (distSq < SQ(self->getSLParams()->mSLHideDist.get())) {
		spine->pushAfterCurrent(&TNerveCannonClose::theNerve());
		return TRUE;
	}

	if (distSq < SQ(self->getSLParams()->mSLBombDist.get())) {
		if (spine->getTime() > self->getSLParams()->mSLBombInterval.get()) {
			self->unk290 = 1;
			spine->pushAfterCurrent(&TNerveCannonShoot::theNerve());
			return TRUE;
		}
	} else if ((self->unk230 == 5 || self->unk230 == 9)
	           && distSq < SQ(self->getSLParams()->mSLKillerDist.get())
	           && spine->getTime() > self->getSLParams()->mSLShootInterval.get()) {
		self->unk290 = 0;
		spine->pushAfterCurrent(&TNerveCannonSearch::theNerve());
		spine->pushAfterCurrent(&TNerveCannonForceBombShoot::theNerve());
		spine->pushAfterCurrent(&TNerveCannonShoot::theNerve());
		spine->pushAfterCurrent(&TNerveCannonShoot::theNerve());
		spine->pushAfterCurrent(&TNerveCannonShoot::theNerve());
		return TRUE;
	}

	if (self->unk2AC != self->mRotation.y) {
		self->unk2AC = self->mRotation.y;
		if (gpMSound->gateCheck(0x20C8))
			MSoundSESystem::MSoundSE::startSoundActor(
			    0x20C8, &self->mPosition, 0, nullptr, 0, 4);
	}

	if (gpApplication.mCurrArea.unk0 == 5 && gpMarDirector->mState == 1) {
		JGeometry::TVec3<f32> diff = *gpMarioPos - self->mPosition;
		self->mRotation.y          = MsGetRotFromZaxis(diff).y;
	} else {
		self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);
	}
	return FALSE;
}

DEFINE_NERVE(TNerveCannonShoot, TLiveActor)
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() == 0) {
		if (self->getShootKind() == 0)
			self->setKillerGoalPoint();
		else
			self->getChorobei()->setBckAnm(0x11);
	}

	if (self->getShootKind() != 0) {
		if (self->getChorobei()->getMActor()->checkCurBckFromIndex(0x11)) {
			if (self->getChorobei()->getMActor()->curAnmEndsNext()) {
				self->getChorobei()->setBckAnm(0x10);
				self->bombSet();
			}
			self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);
		} else if (self->getChorobei()->getMActor()->checkCurBckFromIndex(
		               0x10)) {
			if (self->getChorobei()->getMActor()->curAnmEndsNext()) {
				spine->pushAfterCurrent(&TNerveCannonSearch::theNerve());
				return TRUE;
			}

			if (self->getChorobei()->getMActor()
			        ->getFrameCtrl(ANM_TYPE_BCK)
			        ->checkPass(38.0f))
				self->bombShoot();

			if (self->getChorobei()->getMActor()
			        ->getFrameCtrl(ANM_TYPE_BCK)
			        ->getFrame()
			    > 26.0f)
				self->bombScaleUp();
		}
	} else {
		if (spine->getTime() < 40)
			self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);

		if (spine->getTime() == 40)
			self->killerShoot();

		if (spine->getTime() > self->getSLParams()->mSLKillerInterval.get()) {
			self->unk214++;
			if (self->unk214 >= 3)
				self->unk214 = 0;
			return TRUE;
		}
	}
	return FALSE;
}

DEFINE_NERVE(TNerveCannonForceBombShoot, TLiveActor)
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() == 0) {
		if (self->mDistToMarioSquared
		    < 2.0f * SQ(self->getSLParams()->mSLBombDist.get()))
			self->getChorobei()->setBckAnm(0x11);
		else
			return TRUE;
	}

	if (self->getChorobei()->getMActor()->checkCurBckFromIndex(0x11)) {
		if (self->getChorobei()->getMActor()->curAnmEndsNext()) {
			self->getChorobei()->setBckAnm(0x10);
			self->bombSet();
		}
		self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);
	} else if (self->getChorobei()->getMActor()->checkCurBckFromIndex(0x10)) {
		if (self->getChorobei()->getMActor()->curAnmEndsNext())
			return TRUE;

		if (self->getChorobei()->getMActor()
		        ->getFrameCtrl(ANM_TYPE_BCK)
		        ->checkPass(38.0f))
			self->bombShoot();

		if (self->unk1A8->getMActor()
		        ->getFrameCtrl(ANM_TYPE_BCK)
		        ->getFrame()
		    > 26.0f) {
			self->bombScaleUp();
		}
	}
	return FALSE;
}

DEFINE_NERVE(TNerveCannonClose, TLiveActor)
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() < 2) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->getChorobei()->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->getChorobei()->setBckAnm(0xF);
		self->unk294 = self->mPosition;
		self->unk294.y += 300.0f;
		gpMarioParticleManager->emitAndBindToPosPtr(0xC9, &self->unk294, 0,
		                                            nullptr);
	}

	if (self->getChorobei()->isDownEnd() && !self->isBckAnm(0))
		self->setBckAnm(0);

	if (self->isBckAnm(0)) {
		if (self->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(10.0f)) {
			self->unk294 = self->mPosition;
			self->unk294.y += 290.0f;
			JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToPosPtr(
			    0x11, &self->unk294, 0, nullptr);
			if (emitter)
				emitter->setGlobalScale(JGeometry::TVec3<f32>(1.5f, 1.5f, 1.5f));
		}
	}

	self->updateSquareToMario();
	if (self->mDistToMarioSquared
	    > SQ(3.0f * self->getSLParams()->mSLHideDist.get())) {
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
		self->getChorobei()->offHitFlag(HIT_FLAG_NO_COLLISION);
		spine->pushAfterCurrent(&TNerveCannonOpen::theNerve());
		return TRUE;
	}

	self->unk2B0->moveMtx(self->getMActor()->getModel()->getAnmMtx(4));
	return FALSE;
}

DEFINE_NERVE(TNerveCannonDamage, TLiveActor)
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() == 0) {
		JPABaseEmitter* emitter;
		emitter = gpMarioParticleManager->emitAndBindToPosPtr(
		    0xC4, &self->mPosition, 0, nullptr);
		if (emitter)
			emitter->setGlobalScale(JGeometry::TVec3<f32>(2.0f, 2.0f, 2.0f));
		gpMarioParticleManager->emitAndBindToPosPtr(0xC5, &self->mPosition, 0,
		                                            nullptr);
		if (emitter)
			emitter->setGlobalScale(JGeometry::TVec3<f32>(2.0f, 2.0f, 2.0f));
		gpMarioParticleManager->emitAndBindToPosPtr(0xC6, &self->mPosition, 0,
		                                            nullptr);
		if (emitter)
			emitter->setGlobalScale(JGeometry::TVec3<f32>(2.0f, 2.0f, 2.0f));

		if (self->mHitPoints != 0)
			--self->mHitPoints;

		if (self->mHitPoints == 0) {
			if (self->unk1A4)
				self->unk1A4->kill();

			self->getChorobei()->setBckAnm(0xD);

			u8* area = &gpApplication.mCurrArea.unk0;
			if (*area == 5)
				SMSRumbleMgr->start(0x18, (f32*)nullptr);
			else
				SMSRumbleMgr->start(0x17, (f32*)nullptr);

			JPABaseEmitter* emitter2 = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0xC8,
			    self->getChorobei()->getMActor()->getModel()->getAnmMtx(12), 0,
			    nullptr);
			if (emitter2)
				emitter2->setGlobalScale(self->getChorobei()->mScaling);

			JPABaseEmitter* emitter3 = gpMarioParticleManager->emitAndBindToPosPtr(
			    0xC7, &self->unk294, 0, nullptr);
			if (emitter3)
				emitter3->setGlobalScale(self->getChorobei()->mScaling);

			if (*area == 5) {
				self->unk2A0   = self->mPosition;
				self->unk2A0.y = 0.0f;
				SMSGetMarDirector()->fireStartDemoCamera(
				    "tyorocam_pinna", &self->unk2A0, -1, self->mRotation.y,
				    true, nullptr, 0, nullptr, JDrama::TFlagT<u16>(0));
			} else {
				SMSGetMarDirector()->fireStartDemoCamera(
				    "tyorocam_mare", nullptr, -1, 0.0f, true, nullptr, 0,
				    nullptr, JDrama::TFlagT<u16>(0));
			}

			self->onHitFlag(HIT_FLAG_NO_COLLISION);
			self->unk1A8->onHitFlag(HIT_FLAG_NO_COLLISION);
		} else {
			self->setFreezeAnm();
			self->unk1A8->setBckAnm(0xD);

			MtxPtr mtx = self->unk1A8->getMActor()->getModel()->getAnmMtx(0);
			self->unk294.set(mtx[0][3], mtx[1][3], mtx[2][3]);
			JPABaseEmitter* emitter4 = gpMarioParticleManager->emitAndBindToPosPtr(
			    0xC7, &self->unk294, 0, nullptr);
			if (emitter4)
				emitter4->setGlobalScale(self->unk1A8->mScaling);
		}

		self->mVelocity = JGeometry::TVec3<f32>(0.0f, 4.0f, 0.0f);
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->mPosition.y += 10.0f;
	}

	if (self->unk1A8->getMActor()->curAnmEndsNext()) {
		SMS_ResetDamageFogEffect(
		    self->unk1A8->getMActor()->getModel()->getModelData());
		if (self->mHitPoints == 0) {
			spine->pushAfterCurrent(&TNerveCannonDamageDemo::theNerve());
			return TRUE;
		} else {
			spine->pushAfterCurrent(&TNerveCannonSearch::theNerve());
			return TRUE;
		}
	}
	return FALSE;
}

DEFINE_NERVE(TNerveCannonDamageDemo, TLiveActor)
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() == 0)
		self->getChorobei()->setBckAnm(0xE);

	if (spine->getTime() > 120 && self->getChorobei()->getMActor()->curAnmEndsNext()
	    && !self->isBckAnm(4)) {
		self->setBckAnm(4);
		if (gpMSound->gateCheck(0x38B3))
			MSoundSESystem::MSoundSE::startSoundActor(
			    0x38B3, &self->mPosition, 0, nullptr, 0, 4);

		TSpineEnemy* effectBase = gpConductor->makeOneEnemyAppear(
		    self->mPosition, "エフェクト爆発マネージャー", 1);
		if (effectBase != nullptr) {
			JGeometry::TVec3<f32> scale(2.5f, 2.5f, 2.5f);
			((TEffectExplosion*)effectBase)->generate(self->mPosition, scale);
		}

		TFlagManager::smInstance->setBool(true, 0x5000C);
		spine->pushAfterCurrent(&TNerveCannonObject::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveCannonObject, TLiveActor)
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() == 0) {
		if (!self->isBckAnm(4)) {
			self->setBckAnm(4);
			J3DFrameCtrl* ctrl = self->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
			ctrl->setFrame(ctrl->getEnd());
		}
		if (self->getChorobei())
			self->getChorobei()->unk70 = 1.0f;
	}

	if (self->isBckAnm(4)) {
		if (self->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(60.0f)) {
			if (gpMSound->gateCheck(0x38B4))
				MSoundSESystem::MSoundSE::startSoundActor(
				    0x38B4, &self->mPosition, 0, nullptr, 0, 4);
		}
	}

	if (spine->getTime() > 150 && self->getMareGate()) {
		if (self->getMareGate()->checkLiveFlag(LIVE_FLAG_DEAD)) {
			self->getMareGate()->reset();
			self->getMareGate()->mPosition = self->mPosition;
			self->getMareGate()->mScaling.set(0.27f, 0.02f, 0.27f);
			self->getMareGate()->getMActor()->setBtk("maregate");
		}
		self->getMareGate()->mScaling.y
		    = MsClamp(1.01f * self->unk254->mScaling.y, 0.0f, 0.22f);
	}
	return FALSE;
}
