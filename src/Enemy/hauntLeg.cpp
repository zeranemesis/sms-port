#include <Enemy/HauntLeg.hpp>
#include <Enemy/Walker.hpp>
#include <Enemy/Spider.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/Conductor.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Strategy.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <Map/MapData.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* hauntleg_bastable[] = {
	nullptr,
	nullptr,
	nullptr,
};

static THauntLeg* gpCurHauntLeg;

static BOOL HauntLegCallback(J3DNode* node, int param_2)
{
	if (param_2 == 0) {
		if (gpCurHauntLeg == nullptr || !gpCurHauntLeg->isUseCallBack())
			return true;

		MtxPtr mA = gpCurHauntLeg->getMActor()->getModel()->getAnmMtx(
		    ((J3DJoint*)node)->getJntNo());

		Mtx local_48;
		MsMtxSetRotZ(local_48, gpCurHauntLeg->unk1AC);

		MTXConcat(mA, local_48, mA);
		MTXConcat(J3DSys::mCurrentMtx, local_48, J3DSys::mCurrentMtx);
	}

	return true;
}

THauntLegManager::THauntLegManager(const char* name)
    : TSmallEnemyManager(name)
{
	gpCurHauntLeg = nullptr;
}

void THauntLegManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TWalkerEnemyParams("/enemy/hauntLeg.prm");
}

TSmallEnemy* THauntLegManager::createEnemyInstance() { return new THauntLeg; }

void THauntLegManager::initSetEnemies()
{
	static const GXColorS10 tevColorData1[8] = {
		{ 0x0, 0x0, 0x78, 0xFF },   { 0x78, 0x0, 0x0, 0xFF },
		{ 0x0, 0x78, 0x0, 0xFF },   { 0x78, 0x78, 0x0, 0xFF },
		{ 0x78, 0x0, 0x78, 0xFF },  { 0x64, 0xC8, 0x0, 0xFF },
		{ 0x0, 0x64, 0xC8, 0xFF },  { 0xC8, 0x64, 0x96, 0xFF },
	};
	static const GXColorS10 tevColorData2[8] = {
		{ 0x0, 0x0, 0xFA, 0xFF },   { 0xFA, 0x0, 0x0, 0xFF },
		{ 0x0, 0xFA, 0x0, 0xFF },   { 0xFA, 0xFA, 0x0, 0xFF },
		{ 0xFA, 0x0, 0xFA, 0xFF },  { 0x96, 0xFA, 0x0, 0xFF },
		{ 0x0, 0x96, 0xFA, 0xFF },  { 0xFA, 0x96, 0xC8, 0xFF },
	};

	int colorIdx = 0;
	for (int i = 0; i < mObjNum; ++i) {
		TGraphWeb* graph = gpConductor->getGraphByName("main");
		THauntLeg* enemy = (THauntLeg*)unk18[i];

		int nodeIdx = TMsRange<s32>(0, graph->unk8).rand();
		JGeometry::TVec3<f32> pos;
		graph->getGraphNode(nodeIdx).getPoint(&pos);
		enemy->mPosition = pos;
		enemy->mPosition.y += 5.0f;
		enemy->onLiveFlag(LIVE_FLAG_AIRBORNE);
		enemy->reset();

		for (u16 j = 0;
		     j < enemy->getMActor()->getModel()->getModelData()->getMaterialNum();
		     ++j) {
			SMS_InitPacket_TwoTevColor(
			    ((THauntLeg*)unk18[i])->getMActor()->getModel(), j, GX_TEVREG0,
			    &tevColorData1[colorIdx], GX_TEVREG1, &tevColorData2[colorIdx]);
		}

		++colorIdx;
		if (colorIdx >= 8)
			colorIdx = 0;
	}
}

void THauntLegManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "hauntleg.bmd", 0x10220000, 0 },
	};
	createModelDataArray(entry);
}

BOOL THauntedObject::receiveMessage(THitActor* sender, u32 message)
{
	if (message <= 1) {
		unk68->kill();
		return true;
	}

	if (message == 0xf)
		return true;

	return false;
}

// TODO: guessed body, UNUSED in the map (0x18)
void THauntedObject::checkHit() { offHitFlag(HIT_FLAG_NO_COLLISION); }

void THauntedObject::kill() { onHitFlag(HIT_FLAG_NO_COLLISION); }

THauntLeg::THauntLeg(const char* name)
    : TWalkerEnemy(name)
    , unk194(nullptr)
    , unk198(0)
    , unk199(1)
    , unk19C(nullptr)
{
}

void THauntLeg::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000025;
	unk150     = 0x3A;
	onHitFlag(0x60000000);
	getWalker()->setMode(1);
	unk130 = 2;
	getMActor()->setJointCallback(1, &HauntLegCallback);

	unk194 = new THauntedObject("ハントオブジェクト");

	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(unk194);

	f32 radius = 30.0f * mBodyScale;
	unk194->initHitActor(0x10000025, 2, 0x80000000, radius, radius, radius,
	                     radius);
	unk194->unk68 = this;
}

void THauntLeg::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("hauntleg.bmd", 3);
}

void THauntLeg::reset()
{
	unk19C = nullptr;
	unk198 = 0;
	unk199 = 1;
	TWalkerEnemy::reset();
}

// TODO: same cross product codegen issues as TNameKuri::calcRootMatrix
void THauntLeg::calcRootMatrix()
{
	gpCurHauntLeg = this;

	if (isEaten())
		return;

	getModel()->setBaseScale(mScaling);

	MtxPtr anmMtx = getModel()->getBaseTRMtx();

	if (getWalker()->unk2C->unk10 > 0.0f && unk138 != nullptr) {
		JGeometry::TVec3<f32> local_30(0.0f, 1.0f, 0.0f);

		JGeometry::TVec3<f32> normal = unk138->getNormal();

		JGeometry::TVec3<f32> local_a0;
		local_a0.cross(normal, local_30);
		MsVECNormalize(&local_a0, &local_a0);

		local_30.cross(local_a0, normal);
		MsVECNormalize(&local_30, &local_30);

		anmMtx[0][0] = local_a0.x;
		anmMtx[1][0] = local_a0.y;
		anmMtx[2][0] = local_a0.z;

		anmMtx[0][1] = normal.x;
		anmMtx[1][1] = normal.y;
		anmMtx[2][1] = normal.z;

		anmMtx[0][2] = local_30.x;
		anmMtx[1][2] = local_30.y;
		anmMtx[2][2] = local_30.z;

		anmMtx[0][3] = 0;
		anmMtx[1][3] = 0;
		anmMtx[2][3] = 0;

		f32 angle = (1.0f - getWalker()->unk2C->unk10) * 90.0f;

		Mtx local_7c;
		MsMtxSetRotX(local_7c, angle);

		MTXConcat(anmMtx, local_7c, anmMtx);
	} else {
		JGeometry::TVec3<f32> local_88(MsSin(mRotation.y), 0.0f,
		                               MsCos(mRotation.y));

		JGeometry::TVec3<f32> normal = mGroundPlane->getNormal();

		JGeometry::TVec3<f32> local_a0;
		local_a0.cross(normal, local_88);
		MsVECNormalize(&local_a0, &local_a0);

		local_88.cross(local_a0, normal);
		MsVECNormalize(&local_88, &local_88);

		anmMtx[0][0] = local_a0.x;
		anmMtx[1][0] = local_a0.y;
		anmMtx[2][0] = local_a0.z;

		anmMtx[0][1] = normal.x;
		anmMtx[1][1] = normal.y;
		anmMtx[2][1] = normal.z;

		anmMtx[0][2] = local_88.x;
		anmMtx[1][2] = local_88.y;
		anmMtx[2][2] = local_88.z;
	}

	anmMtx[0][3] = mPosition.x;
	anmMtx[1][3] = mPosition.y;
	anmMtx[2][3] = mPosition.z;

	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		unk194->mPosition = mPosition;
	} else {
		MtxPtr mtx = getModel()->getAnmMtx(2);
		unk194->mPosition.x = mtx[0][3];
		unk194->mPosition.y = mtx[1][3];
		unk194->mPosition.z = mtx[2][3];
	}

	for (int i = 0; i < unk194->mColCount; ++i)
		;
}

void THauntLeg::setGenerateAnm() { setBckAnm(0); }

void THauntLeg::setWaitAnm() { setBckAnm(2); }

void THauntLeg::setWalkAnm() { setBckAnm(1); }

void THauntLeg::setRunAnm() { setBckAnm(1); }

void THauntLeg::setDeadAnm()
{
	if (unk19C != nullptr) {
		unk19C->receiveMessage(this, 6);
		mHolder     = nullptr;
		mHeldObject = nullptr;
	}
	unk194->kill();
}

void THauntLeg::attackToMario()
{
	updateSquareToMario();
	if (getDistToMarioSquared() < 10000.0f)
		sendAttackMsgToMario();
}

bool THauntLeg::isCollidMove(THitActor* other)
{
	if (mSpine->getCurrentNerve() != &TNerveHauntLegHaunt::theNerve()
	    && !unk198 && !checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		u32 type = other->mActorType & 0xFFFF0000;
		if (type == 0x20000000 || type == 0x40000000) {
			TTakeActor* takeActor = (TTakeActor*)other;
			if (takeActor->mHolder == nullptr || takeActor != unk19C) {
				unk19C = takeActor;
				mSpine->setNext(&TNerveHauntLegHaunt::theNerve());
			}
			return false;
		}
	}
	return false;
}

const char** THauntLeg::getBasNameTable() const { return hauntleg_bastable; }

MtxPtr THauntLeg::getTakingMtx()
{
	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		TPosition3f mtx;
		mtx.translation(mPosition.x, mPosition.y, mPosition.z);
		MTXCopy(mtx, getMActor()->getModel()->getBaseTRMtx());
		return getMActor()->getModel()->getBaseTRMtx();
	}

	return getMActor()->getModel()->getAnmMtx(2);
}

bool THauntLeg::isUseCallBack()
{
	return mSpine->getCurrentNerve() == &TNerveHauntLegHaunt::theNerve() ? true
	                                                                     : false;
}

// TODO: the jump speed constant is kept in f31 across getGravityY() in the
// target, and the squared length is contracted into an fmadds there.
DEFINE_NERVE(TNerveHauntLegHaunt, TLiveActor)
{
	THauntLeg* self = (THauntLeg*)spine->getBody();

	if (spine->getTime() == 0) {
		self->unk1A0 = self->calcVelocityToJumpToY(self->unk19C->mPosition,
		                                           10.0f, self->getGravityY());
		self->mVelocity = self->unk1A0;
		self->mPosition.y += 10.0f;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->unk199 = 1;
	} else if (!self->isAirborne()) {
		if (self->unk199) {
			self->mVelocity = self->unk1A0;
			self->mPosition.y += 10.0f;
			self->unk199 = 0;

			JGeometry::TVec3<f32> diff
			    = self->mPosition - self->unk19C->mPosition;
			if (JGeometry::TUtil<f32>::sqrt(diff.x * diff.x + diff.y * diff.y
			                                + diff.z * diff.z)
			        < 200.0f && self->unk19C->mHolder == nullptr
			    && self->unk19C->receiveMessage(self, 4)) {
				self->mHeldObject = self->unk19C;
				self->unk198      = 1;
			}
		} else {
			self->unk1AC = 0.0f;
			spine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
			return true;
		}
	}

	if (self->isAirborne()) {
		if (self->unk199)
			self->unk1AC = MsClamp(self->unk1AC + 2.0f, 0.0f, 180.0f);
		else
			self->unk1AC = MsClamp(self->unk1AC + 2.0f, 0.0f, 360.0f);
	}

	return false;
}
