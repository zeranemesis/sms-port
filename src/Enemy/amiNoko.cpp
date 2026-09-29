#include <Enemy/AmiNoko.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair and the four MtxCalcType names from M3DUtil/InfectiousStrings.hpp;
// without it every string offset in this object is shifted.
#include <M3DUtil/InfectiousStrings.hpp>
#include <Strategic/ObjManager.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/ObjModel.hpp> // TMActorKeeper
#include <Strategic/Strategy.hpp> // TIdxGroupObj
#include <System/MarDirector.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <System/Particles.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <Enemy/Enemy.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// This TU is -inline deferred: the definition order below is the reverse of
// the .text layout in mario.MAP.

static const char* amiNoko_bastable[] = {
	0,
	"/scene/amiNoko/bas/aminoko_flying1_start.bas",
	"/scene/amiNoko/bas/aminoko_hit1.bas",
	0,
	"/scene/amiNoko/bas/aminoko_run1_loop.bas",
	0,
	0,
	"/scene/amiNoko/bas/aminoko_run2_loop.bas",
	0,
	0,
	"/scene/amiNoko/bas/aminoko_turn1_loop.bas",
	0,
	0,
	"/scene/amiNoko/bas/aminoko_turn2_loop.bas",
	0,
	0,
};

// TODO: weak in the original (inline in the class body); defined out of line
// until the vtable/dtor of TAmiNoko are emitted by this TU.
bool TAmiNoko::isCollidMove(THitActor*) { return false; }

TAmiNokoManager::TAmiNokoManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TAmiNokoManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TAmiNokoParams("/enemy/amiNoko.prm");
	TSmallEnemyManager::load(stream);
}

// TODO: 0x10220000 flags not fully decoded (J3DMLF_* bitfield)
void TAmiNokoManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "aminoko_model1.bmd", 0x10220000, 0 },
		{ 0, 0, 0 },
	};
	createModelDataArray(entry);
}

TSmallEnemy* TAmiNokoManager::createEnemyInstance() { return 0; }

BOOL TAmiHit::receiveMessage(THitActor* sender, u32 message)
{
	return mParent->receiveMessage(sender, message);
}

TAmiNoko::TAmiNoko(const char* name)
    : TWalkerEnemy(name)
{
	unk194 = 0;
	unk198 = 0;
	unk20C = 1;
	unk19C.set(0.0f, 1.0f, 0.0f);
	unk1A8.set(0.0f, 0.0f, 1.0f);
	unk1B4 = unk19C;
	unk1C0 = unk1A8;
}

void TAmiNoko::load(JSUMemoryInputStream& stream)
{
	TSpineEnemy::load(stream);
	stream.read(&mCoinId, 4);
}

void TAmiNoko::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000021;
	unk150     = 0x11;
	mSpine->initWith(&TNerveAmiNokoWalkOnFence::theNerve());
	unk208 = getSaveParam();
	reset();
	setMActorAndKeeper();
	onLiveFlag(LIVE_FLAG_UNK10);
	initialGraphNode();
	if (gpMarDirector->mMap == 8)
		unk20C = 0;
	unkE8    = 0;
	TAmiHit* amiHit = new TAmiHit("アミノコ当り判定");
	amiHit->mParent = this;
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(amiHit);
	amiHit->initHitActor(0x10000021, 1, 0x80000000, 1.0f, 1.0f, 1.0f, 1.0f);
	amiHit->offHitFlag(HIT_FLAG_NO_COLLISION);
	mAmiHit = amiHit;
}

void TAmiNoko::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor = mMActorKeeper->createMActor("aminoko_model1.bmd", 3);
}

void TAmiNoko::reset() { TWalkerEnemy::reset(); }

void TAmiNoko::behaveToWater(THitActor*)
{
	if (mSpine->getCurrentNerve() != &TNerveAmiNokoFreeze::theNerve()) {
		mSpine->setNext(&TNerveAmiNokoFreeze::theNerve());
		mSpine->pushNerve(&TNerveAmiNokoFreeze::theNerve());
	}
	mSprayedByWaterCooldown = 0;
}

void TAmiNoko::setWalkAnm()
{
	if (unk20C)
		setBckAnm(5);
	else
		setBckAnm(8);
}

void TAmiNoko::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & 0x2)
		calcDirection();
	TSmallEnemy::perform(cue, graphics);
	mAmiHit->perform(cue, graphics);
}

f32 TAmiNoko::getGravityY() const
{
	if (mSpine->getLatestNerve() == &TNerveAmiNokoDie::theNerve())
		return 0.0f;

	return mGravity;
}

const char** TAmiNoko::getBasNameTable() const { return amiNoko_bastable; }

DEFINE_NERVE(TNerveAmiNokoFreeze, TLiveActor)
{
	TAmiNoko* self = (TAmiNoko*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(2);
		J3DModel* model  = self->getModel();
		JPABaseEmitter* emitter
		    = gpMarioParticleManager->emitAndBindToMtxPtr(
		        0xCA, model->getAnmMtx(0), 0, nullptr);
		if (emitter)
			emitter->setGlobalScale(JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f));
	}

	if (self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(2)) {
			self->setBckAnm(0xF);
			return TRUE;
		}
		if (spine->getTime()
		    > ((TWalkerEnemyParams*)self->getSaveParam())
		           ->mSLFreezeWait.get())
			return TRUE;
	}
	return FALSE;
}
