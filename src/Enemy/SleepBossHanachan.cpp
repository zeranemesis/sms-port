#include <Enemy/SleepBossHanachan.hpp>

// rogue include: the original TU opens .rodata with the dummy string pair
// from System/DummyStrings.hpp followed by the four MActor mtx-calc type
// names from M3DUtil/InfectiousStrings.hpp (which pulls DummyStrings in
// first, so that pair still precedes them); without it every string offset
// in this object is shifted. The MtxCalcTypeName[] array itself is not
// emitted -- only the literals it is built from.
#include <M3DUtil/InfectiousStrings.hpp>
#include <Enemy/BossHanachan.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MoveBG/Item.hpp>
#include <MoveBG/ItemManager.hpp>
#include <Strategic/MirrorActor.hpp>
#include <Strategic/Spine.hpp>
#include <System/FlagManager.hpp>

static const char* sleepBossHanachan_bastable[] = {
	"/scene/sleepBossHanachan/bas/demohanatyan_fall.bas",
	nullptr,
};

void TSleepBossHanachanManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "demohanatyan_model.bmd", 0x10010000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TSleepBossHanachan::init(TLiveManager* manager)
{
	initBase(manager, 3);
	mSpine->initWith(&TNerveSBH_SleepContinue::theNerve());
	initHitActor(0x08000016, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	initAnmSound();
	getMActor()->setBckFromIndex(1);
	setCurAnmSound();
	// TODO: the ROM applies the actor's own rotation to the model's base
	// TR matrix here rather than leaving it to calcRootMatrix().
	MsMtxSetXYZRPH(getModel()->getBaseTRMtx(), mPosition.x, mPosition.y,
	               mPosition.z, mRotation.x, mRotation.y, mRotation.z);
	unk15C = new TMirrorActor("寝てるボスハナチャンin鏡");
	unk15C->init(getModel(), 0xa);
}

void TSleepBossHanachan::calcRootMatrix() {}

const char** TSleepBossHanachan::getBasNameTable() const
{
	return sleepBossHanachan_bastable;
}

// TODO: 97.0% -- every instruction matches, but the original stack frame is
// 0x48 while MWCC only reserves 0x40 here. The 8 missing bytes are never
// written or read (same "reserved space" phenomenon as the dead zones in
// hamukuri.cpp), so the remaining `~` markers are all frame-offset renamings.
void TSleepBossHanachan::startFall(f32 x, f32 y, f32 z)
{
	TFlagManager::smInstance->setBool(true, 0x5000B);
	unk150.x = x;
	unk150.y = y;
	unk150.z = z;
	getMActor()->setBckFromIndex(0);
	setCurAnmSound();
	mSpine->setNext(&TNerveSBH_Fall::theNerve());
}

DEFINE_NERVE(TNerveSBH_SleepContinue, TLiveActor)
{
	return false;
}

// TODO: 99.8% -- instruction-for-instruction identical, but the original
// frame is 0x40 and MWCC reserves 0x30. The 16 extra bytes of the original
// (a 12-byte hole below `pos` plus 4 bytes above it) are never accessed;
// all remaining differences are frame-offset renamings.
DEFINE_NERVE(TNerveSBH_Fall, TLiveActor)
{
	TSleepBossHanachan* body = (TSleepBossHanachan*)spine->getBody();

	if (body->getMActor()->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
		// The shine is spawned at the position stashed by startFall().
		JGeometry::TVec3<f32> pos = body->unk150;
		TShine* shine = gpItemManager->makeShineAppearWithDemo(
		    "シャイン（ボス用）", "ボスシャインカメラ", pos.x, pos.y, pos.z);
		// TODO: unknown Shine flag (0x20000000); set right after it appears.
		shine->unkF8 |= 0x20000000;
		body->onLiveFlag(LIVE_FLAG_DEAD);
		// TODO: unknown TMirrorActor flag (0x1); hiding the reflection comes
		// with dying so that the body is not mirrored any more.
		TMirrorActor* mirror = body->unk15C;
		mirror->unk1A |= 1;
		SMS_HideAllShapePacket(mirror->unk14);
		spine->pushAfterCurrent(&TNerveSBH_SleepContinue::theNerve());
		return true;
	}

	return false;
}
