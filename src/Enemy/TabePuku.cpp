#include <Enemy/TabePuku.hpp>

// TODO: this entire translation unit is freshly scaffolded from mario.MAP
// and m2c drafts. Only trivial destructors are matched so far; the rest
// are placeholder stubs. TTabePuku's instance member layout has not been
// reconstructed beyond the TPHitActor pointer inferred from the ctor.

static const char* tabepuku_bastable[] = {
	// TODO: recover actual .bas paths from .rodata
	0,
};

DEFINE_NERVE(TNerveTabePukuDrag, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveTabePukuDive, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveTabePukuBite, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveTabePukuAttack, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveTabePukuRecoverGraph, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveTabePukuFound, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

DEFINE_NERVE(TNerveTabePukuGraphWander, TLiveActor)
{
	// TODO: not yet decompiled
	return FALSE;
}

void TTabePukuManager::createModelData()
{
	// TODO: not yet decompiled
}

void TTabePukuManager::load(JSUMemoryInputStream& stream)
{
	// TODO: not yet decompiled
}

TTabePukuManager::TTabePukuManager(const char* name)
    : TSmallEnemyManager(name)
{
	// TODO: not yet decompiled
}

void TTabePuku::swimTo(const JGeometry::TVec3<f32>& target)
{
	// TODO: not yet decompiled
}

bool TTabePuku::doKeepDistance()
{
	// TODO: not yet decompiled
	return false;
}

bool TTabePuku::isFindMario(float param_1)
{
	// TODO: not yet decompiled
	return false;
}

void TTabePuku::forceKill()
{
	// TODO: not yet decompiled
}

void TTabePuku::behaveToWater(THitActor* hitActor)
{
	// TODO: not yet decompiled
}

void TTabePuku::attackToMario()
{
	// TODO: not yet decompiled
}

const char** TTabePuku::getBasNameTable() const
{
	return (const char**)tabepuku_bastable;
}

MtxPtr TTabePuku::getTakingMtx()
{
	// TODO: not yet decompiled
	return 0;
}

BOOL TTabePuku::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: not yet decompiled
	return FALSE;
}

void TTabePuku::calcRootMatrix()
{
	// TODO: not yet decompiled
}

void TTabePuku::bind()
{
	// TODO: not yet decompiled
}

void TTabePuku::control()
{
	// TODO: not yet decompiled
}

void TTabePuku::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// TODO: not yet decompiled
}

void TTabePuku::reset()
{
	// TODO: not yet decompiled
}

void TTabePuku::init(TLiveManager* liveManager)
{
	// TODO: not yet decompiled
}

TTabePuku::TTabePuku(const char* name)
    : TSmallEnemy(name)
{
	// TODO: not yet decompiled
}

void TTPHitActor::bind()
{
	// TODO: not yet decompiled
}

void TTPHitActor::updateTerrainCollsion()
{
	// TODO: not yet decompiled
}

BOOL TTPHitActor::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: not yet decompiled
	return FALSE;
}

void TTPHitActor::init()
{
	// TODO: not yet decompiled
}

TTabePukuManager::~TTabePukuManager()
{
	// TODO: not yet decompiled
}

TTabePuku::~TTabePuku()
{
	// TODO: not yet decompiled
}

TTPHitActor::~TTPHitActor()
{
	// TODO: not yet decompiled
}
