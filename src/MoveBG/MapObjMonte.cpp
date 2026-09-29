
#include <MoveBG/MapObjMonte.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Yoshi.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>

void TMapObjMonteRoot::initMapObj()
{
	TMapObjBase::initMapObj();
	f32 damageHeight = 1400.0f * mScaling.y;
	mDamageHeight = damageHeight;
	calcEntryRadius();
	f32 y = mInitialPosition.y + mYOffset;
	mPosition.y = y;
}

BOOL TJumpMushroom::receiveMessage(THitActor*, unsigned long)
{
	startAnim(1);
	return TRUE;
}

void TJumpMushroom::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	int value;
	stream.read(&value, 4);
	if (mMapCollisionManager) {
		mMapCollisionManager->getUnk8()->setAllData(value);
	}
}

void THangingBridgeBoard::calcDefaultMtx()
{
	Mtx rotX;
	Mtx rotY;
	makeRootMtxRotX(rotX);
	makeRootMtxRotY(rotY);
	PSMTXConcat(rotY, rotX, rotY);
	mDefaultMtx.set(rotY);
	mVelocity.y = 0.0f;
	mPosition.y = mInitialPosition.y;
}

void THangingBridgeBoard::setGroundCollision()
{
	if (SMS_GetYoshi()->isHatched()
	    && mPosition.x - mBodyRadius < SMS_GetYoshi()->getTranslation().x
	    && mPosition.x + mBodyRadius > SMS_GetYoshi()->getTranslation().x
	    && mPosition.z - mBodyRadius < SMS_GetYoshi()->getTranslation().z
	    && mPosition.z + mBodyRadius > SMS_GetYoshi()->getTranslation().z) {
		// TODO: 99.9% - frame 0x48 vs target 0x40. Naming `col` shrinks the frame
		// to 0x40 but allocates it to r0 (extra mr r3,r0); leaving it unnamed
		// puts it in r3 (exact instructions) but adds an 8-byte temp slot.
		J3DModel* model = getModel();
		MtxPtr anmMtx = model->getAnmMtx(0);
		if (mMapCollisionManager->getUnk8()) {
			mMapCollisionManager->getUnk8()->moveMtx(anmMtx);
		}
	} else {
		TMapObjBase::setGroundCollision();
	}
}

void THangingBridgeBoard::initMapObj()
{
	TLeanBlock::initMapObj();
	unk140 = 0.01f;
	unk144 = 0.02f;
	unk148 = 0.08f;
}

THangingBridgeBoard::THangingBridgeBoard(const char* name)
    : TLeanBlock(name)
{
	unk1BC = nullptr;
	unk194 = 0;
	unk198 = 0;
	unk19C = 0;
	unk1A0 = 0;
	unk1A4[0].zero();
	unk1A4[1].zero();
}

void THangingBridgeBoard::drawOneRope(const JGeometry::TVec3<f32>&) const {}

void THangingBridgeBoard::drawRopes() const {}

void THangingBridgeBoard::push(f32) {}

void THangingBridgeBoard::pushNeighbor(f32) {}

void THangingBridgeBoard::control() {}

f32 THangingBridge::mRopeWidthBetweenBoards = 0.0f;
f32 THangingBridge::mRopeWidthBetweenBoardsY = 0.0f;
int THangingBridge::mPointNumBetweenBoards = 0;
f32 THangingBridge::mBetweenBoardsTexPosRate = 0.0f;
f32 THangingBridge::mRopeHeight = 0.0f;

void THangingBridge::drawLowerMinus(const JGeometry::TVec3<f32>&,
                                    const JGeometry::TVec3<f32>&,
                                    const JGeometry::TVec2<f32>&, int) const
{
}

void THangingBridge::drawLowerPlus(const JGeometry::TVec3<f32>&,
                                   const JGeometry::TVec3<f32>&,
                                   const JGeometry::TVec2<f32>&, int) const
{
}

void THangingBridge::drawUpper(const JGeometry::TVec3<f32>&,
                               const JGeometry::TVec3<f32>&,
                               const JGeometry::TVec2<f32>&, int) const
{
}

void THangingBridge::setDrawPos(int, f32, JGeometry::TVec3<f32>*) const {}

void THangingBridge::drawRopeBetweenBoards(f32, int) const {}

void THangingBridge::initDraw() const {}

void THangingBridge::perform(u32 cue, JDrama::TGraphics*)
{
	if (cue & 0x1000) {
		initDraw();
	}
	for (int i = 0; i < unk10; ++i) {
		THangingBridgeBoard* board = unk14[i];
		JGeometry::TVec3<f32> vec1 = board->unk1A4[0];
		board->drawOneRope(vec1);
		JGeometry::TVec3<f32> vec2 = board->unk1A4[1];
		board->drawOneRope(vec2);
	}
	if (gpMarDirector->mState == 0xD) {
		drawRopeBetweenBoards(-60.0f, mPointNumBetweenBoards);
	} else {
		drawRopeBetweenBoards(0.0f, mPointNumBetweenBoards);
	}
	drawRopeBetweenBoards(mRopeHeight, 1);
}

void THangingBridge::loadAfter() {}

void THangingBridge::initMonte() {}

THangingBridge::THangingBridge(const char* name)
    : TViewObj(name)
{
	unk10 = 0;
	unk14 = 0;
	unk38 = 0;
	unk3C = 0.0f;
	unk40 = 0.0f;
	unk44 = 0.0f;
}

void TSwingBoard::drawOneRope(const JGeometry::TVec3<f32>&,
                               const JGeometry::TVec3<f32>&) const
{
}

void TSwingBoard::initDraw() const {}

void TSwingBoard::draw() const {}

void TSwingBoard::swing() {}

void TSwingBoard::control() {}

void TSwingBoard::load(JSUMemoryInputStream&) {}

TSwingBoard::TSwingBoard(const char* name)
    : TMapObjBase(name)
{
	unk138 = 5000.0f;
	unk13C = 0.0f;
	unk140 = 0.0f;
	unk144 = 0.0f;
	unk148 = 0.0f;
	unk188 = 0;
	unk178 = 0.0f;
	unk168 = 0.0f;
	unk158 = 0.0f;
	unk164 = 0.0f;
	unk154 = 0.0f;
	unk170 = 0.0f;
	unk150 = 0.0f;
	unk16C = 0.0f;
	unk15C = 0.0f;
	unk174 = 1.0f;
	unk160 = 1.0f;
	unk14C = 1.0f;
	unk184 = 0.0f;
	unk180 = 0.0f;
	unk17C = 0.0f;
}

void TGoalFlag::touchActor(THitActor* actor)
{
	if (actor->isActorType(0x80000001)) {
		if (!TFlagManager::getInstance()->getBool(0x00050005)) {
			TFlagManager::getInstance()->setBool(true, 0x00050005);
		}
		actor->receiveMessage(this, HIT_MESSAGE_ATTACK);
	} else if (actor->isActorType(0x08000002)) {
		actor->receiveMessage(this, HIT_MESSAGE_ATTACK);
	}
}

void TGoalFlag::initMapObj() { TMapObjBase::initMapObj(); }

u32 TFluff::touchWater(THitActor* actor)
{
	const JGeometry::TVec3<f32>& waterPos = getWaterPos(actor);
	JGeometry::TVec3<f32> normal;
	getNormalVecFromTarget(waterPos.x, waterPos.y, waterPos.z, &normal);
	mVelocity.x -= normal.x * unk160;
	mVelocity.y -= normal.y * unk160;
	mVelocity.z -= normal.z * unk160;
	return 1;
}

void TFluff::move() {}

void TFluff::kill()
{
	if (mHeldObject) {
		mHeldObject->receiveMessage(this, HIT_MESSAGE_UNK8);
		mHeldObject->mHolder = nullptr;
		mHeldObject = nullptr;
	}
	setState(3);
}

void TFluff::control() {}

void TFluff::appear() {}

void TFluff::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = 300.0f;
	unk13C = 0.5f;
}

TFluff::TFluff(const char* name)
    : TMapObjBase(name)
{
	unk138 = 0.0f;
	unk13C = 0.0f;
	unk140 = 0.0f;
	unk144 = 0.0f;
	unk148 = 0.0f;
	unk14C = 0.0f;
	unk150 = 0.0f;
	unk160 = 1.0f;
	unk164 = 0.95f;
	unk168 = nullptr;
	unk16C = 0;
	unk15C = 0.0f;
	unk158 = 0.0f;
	unk154 = 0.0f;
}

void TFluffManager::findNextFluff() {}

void TFluffManager::control() {}

void TFluffManager::registerNextFluff(TFluff*) {}

void TFluffManager::setUpNextFluff() {}

void TFluffManager::newFluff(const char*) {}

f32 TFluffManager::getRandomX() const { return 0.0f; }

f32 TFluffManager::getRandomZ() const { return 0.0f; }

void TFluffManager::loadAfter() {}

void TFluffManager::load(JSUMemoryInputStream&) {}

TFluffManager::TFluffManager(const char* name)
    : TMapObjBase(name)
{
	unk138 = 0.0f;
	unk13C = 0.0f;
	unk140 = 0.0f;
	unk144 = 0;
	unk154 = 0.0f;
	unk158 = 0;
	unk15C = 0;
	unk160 = 0;
	unk164 = 0;
	unk148 = 0.0f;
	unk14C = 0.0f;
	unk150 = 0.0f;
}
