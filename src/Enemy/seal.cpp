// rogue include: the original TU opens .rodata with this dummy string
// pair, ahead of every other string constant in the object.
#include <M3DUtil/InfectiousStrings.hpp>

#include <Enemy/Seal.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <System/Particles.hpp>
#include <System/EmitterViewObj.hpp>

#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

TSeal::TSeal(const char* name)
    : TSpineEnemy(name)
{
	unk150 = 0;
	onLiveFlag(LIVE_FLAG_UNK10);
}

void TSeal::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 2);
	mMActor       = mMActorKeeper->createMActor("gene_orange_model1.bmd", 0);
	mMActor->offMakeDL();

	f32 radius = 100.0f * mScaling.x;
	initHitActor(0x10000024, 1, 0x81000000, radius, radius, radius, radius);
	offHitFlag(HIT_FLAG_NO_COLLISION);

	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(this);

	mRotation.x = MsWrap(270.0f + mRotation.x, 0.0f, 360.0f);

	mMapCollisionManager = new TMapCollisionManager(1, "/scene/seal", this);
	mMapCollisionManager->init("gene_orange_col1.col", 2, nullptr);
	mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation, mScaling);

	mHitPoints = getMaxHitPoints();
	mSpine->initWith(&TNerveSealSleep::theNerve());
}

BOOL TSeal::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->getActorType() == 0x1000001
	    && message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		gpMarioParticleManager->emit(PARTICLE_MS_ENM_WATHIT, &sender->mPosition,
		                             0, nullptr);
		SMSGetMSound()->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK,
		                              &sender->mPosition, 0, 0.0f, 0, 0, 4);
		if (gpModelWaterManager->unk5D5F) {
			SMSGetMSound()->startSoundSet(MSD_SE_ERASE_SCRAWL,
			                              &sender->mPosition, 0, 0.0f, 0, 0, 4);
			if (!mSpine->isNerve(&TNerveSealDie::theNerve())) {
				if (mMapCollisionManager->getUnk8())
					mMapCollisionManager->getUnk8()->remove();
				mSpine->pushNerve(&TNerveSealDie::theNerve());
			}
			unk150++;
			return true;
		}
		return true;
	}

	return false;
}

void TSeal::calcRootMatrix()
{
	J3DModel* model = getModel();
	MsMtxSetXYZRPH(model->getBaseTRMtx(), mPosition.x, mPosition.y,
	               mPosition.z, mRotation.x, mRotation.y, mRotation.z);
	model->setBaseScale(mScaling);
}

void TSeal::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (!checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN)
	    && (cue & CUE_MOVE)) {
		for (int i = 0; i < getColNum(); ++i) {
			THitActor* actor = getCollision(i);
			if (actor->isActorType(0x80000001))
				actor->receiveMessage(this, HIT_MESSAGE_ATTACK);
		}
	}

	TSpineEnemy::perform(cue, graphics);

	if (cue & CUE_MOVE) {
		updateSquareToMario();
		unk150 = 0;
	}

	if ((cue & CUE_CALC_ANIM) && !checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN)
	    && mDistToMarioSquared < 2250000.0f)
		SMSGetMSound()->startSoundActor(MSD_SE_EN_ORANGESEAL_WAIT, &mPosition,
		                                0, nullptr, 0, 4);
}

TSealManager::TSealManager(const char* name)
    : TEnemyManager(name)
{
}

void TSealManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "gene_orange_model1.bmd", 0x11210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TSealManager::initJParticle() { }

void TSealManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
}

DEFINE_NERVE(TNerveSealSleep, TLiveActor)
{
	TSeal* self = (TSeal*)spine->getBody();

	if (spine->getTime() == 0 && !self->getMActor()->checkCurBckFromIndex(1))
		self->getMActor()->setBckFromIndex(-1);

	if (self->getDistToMarioSquared() < 1000000.0f) {
		spine->pushAfterCurrent(&TNerveSealWait::theNerve());
		return true;
	}

	if (self->getMActor()->curAnmEndsNext()
	    && self->getMActor()->checkCurBckFromIndex(1))
		self->getMActor()->setBckFromIndex(-1);

	return false;
}

DEFINE_NERVE(TNerveSealWait, TLiveActor)
{
	TSeal* self = (TSeal*)spine->getBody();

	if (spine->getTime() == 0)
		self->getMActor()->setBckFromIndex(3);

	if (self->getMActor()->curAnmEndsNext()
	    && self->getMActor()->checkCurBckFromIndex(3))
		self->getMActor()->setBckFromIndex(2);

	if (self->getDistToMarioSquared() > 2250000.0f
	    && self->getMActor()->curAnmEndsNext()) {
		self->getMActor()->setBckFromIndex(1);
		spine->pushAfterCurrent(&TNerveSealSleep::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveSealDie, TLiveActor)
{
	TSeal* self = (TSeal*)spine->getBody();

	if (spine->getTime() == 0) {
		self->getMActor()->setBckFromIndex(0);

		MtxPtr mtx = self->getMActor()->getModel()->getBaseTRMtx();

		// TODO: name these particle IDs
		JPABaseEmitter* emitter
		    = gpMarioParticleManager->emitAndBindToMtxPtr(0xD1, mtx, 0, self);
		if (emitter)
			emitter->setGlobalScale(self->mScaling);

		emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    0xD2, mtx, 0, (u8*)self + 1);
		if (emitter)
			emitter->setGlobalScale(self->mScaling);
	}

	SMSGetMSound()->startSoundActor(MSD_SE_WT_BOSS_FADEAWAY, &self->mPosition,
	                                0, nullptr, 0, 4);

	if (self->getMActor()->curAnmEndsNext()) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->kill();
		spine->pushAfterCurrent(&TNerveSealSleep::theNerve());
		return true;
	}

	return false;
}
