#include <Enemy/BossHanachan.hpp>
#include <Enemy/BossHanachanChangeSaveParams.hpp>
#include <Camera/cameralib.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <M3DUtil/MActor.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/MapUtil.hpp>
#include <MarioUtil/ShadowUtil.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/Strategy.hpp>
#include <System/MarDirector.hpp>

// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

const char* cMapCollisionJointName    = "center";
const char* cBodyMapCollisionFileName = "body_col.col";
const char* cHeadMapCollisionFileName = "head_col.col";
const char* cLegJointName_L3          = "leg_L3";
const char* cLegJointName_R3          = "leg_R3";
const char* cNoseHallJointName_L      = "L_hall";
const char* cNoseHallJointName_R      = "R_hall";

// TODO: the four tables below carry a `$NNNN` suffix in the linker map, so in
// the original they were function-local statics of the two setAnm_ overrides
// rather than file-scope ones. They have no effect on codegen here.
static const int sBodyBckIndex[]
    = { 0x13, 0x0F, 0x0A, 0x0D, 0x00, 0x0C, 0x09, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07, 0x08, 0x10, 0x01, 0x11, 0x12 };
static const int sHeadBckIndex[]
    = { 0x24, 0x20, 0x1D, 0x1F, 0x14, 0x1E, 0x1C, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x21, 0x15, 0x22, 0x23 };
static const int sHeadBtpIndex[]
    = { 0x00, 0x00, 0x01, 0x02, 0x01, 0x02, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01, 0x01 };
static const int sHeadBtkIndex[]
    = { 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// TODO: reconstructed from the inlined bodies only; the exact original is
// unknown. UNUSED in the map, so nothing depends on the body but its size.
static void CalcMtxPtrFromJointName(JUTNameTab* joint_names,
                                    const char* joint_name, J3DModel* model,
                                    MtxPtr* out_mtx)
{
	// the index is truncated to 16 bits before it scales the matrix stride
	u16 index = (u16)joint_names->getIndex(joint_name);
	*out_mtx = model->getAnmMtx(index);
}

TBossHanachanPartsBase::TBossHanachanPartsBase(TBossHanachan* owner,
                                               u32 instance_index,
                                               int anm_data_index,
                                               const char* name)
    : TLiveActor(name)
{
	mCurAnm      = BH_ANM_KIND_UNK12;
	mOldAnm      = BH_ANM_KIND_UNK12;
	mOwner       = owner;
	mHitActor    = nullptr;
	mMapCollision          = nullptr;
	mMapCollisionJointMtx  = nullptr;
	unk10C                  = 0;
	mNonstopMotionBlend    = nullptr;

	mMActorKeeper  = owner->mMActorKeeper;
	mMActor        = mMActorKeeper->createMActorFromNthData(anm_data_index, 0);
	if (mMActor->mAnmBck != nullptr)
		mMActor->mAnmBck->initNormalMotionBlend();

	initHitActor(instance_index, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
	onHitFlag(HIT_FLAG_NO_COLLISION);

	switch (instance_index) {
	case 0x80000015:
		mScaledBodyRadius
		    = owner->mCommonSaveParams->mSLBodyShadowSize.get();
		break;
	case 0x80000014:
		mScaledBodyRadius
		    = owner->mCommonSaveParams->mSLHeadShadowSize.get();
		break;
	}

	onLiveFlag(LIVE_FLAG_UNK8);
	initAnmSound();
	mMActor->setLightType(1);
	mNonstopMotionBlend = new TBHNonstopMotionBlend(
	    CLBPalFrame<s16>(
	        owner->mCommonSaveParams->mSLMotionBlendFrames.get()));
}

TBossHanachanPartsBody::TBossHanachanPartsBody(TBossHanachan* owner,
                                               const char* name)
    : TBossHanachanPartsBase(owner, 0x80000015, 0, name)
{
	mBodyIndex = 0;
	unk120 = 0.0f;
	unk124.set(0.0f, 0.0f, 0.0f);
	unk130.set(0.0f, 0.0f, 0.0f);
	unk13C = 0.0f;
	unk140 = 0.0f;
	unk144 = 0.0f;
	unk148 = 0.0f;
	unk154.set(0.0f, 0.0f, 0.0f);

	J3DModel* model  = getModel();
	JUTNameTab* jnts = model->getModelData()->getJointName();
	CalcMtxPtrFromJointName(jnts, cLegJointName_L3, model, &mLegMtx[0]);
	CalcMtxPtrFromJointName(jnts, cLegJointName_R3, model, &mLegMtx[1]);
}

TBossHanachanPartsHead::TBossHanachanPartsHead(TBossHanachan* owner,
                                               const char* name)
    : TBossHanachanPartsBase(owner, 0x80000014, 1, name)
{
	J3DModel* model  = getModel();
	JUTNameTab* jnts = model->getModelData()->getJointName();
	CalcMtxPtrFromJointName(jnts, cNoseHallJointName_L, model, &mNoseHallMtxL);
	CalcMtxPtrFromJointName(jnts, cNoseHallJointName_R, model, &mNoseHallMtxR);
}

void TBossHanachanPartsBase::initMapCollisionAndHitActor_(TIdxGroupObj* group)
{
	const char* col_file;
	f32 attack_radius;
	f32 attack_height;
	f32 damage_radius;
	f32 damage_height;
	f32 hit_offset_y;

	if (getInstanceIndex() == 0x80000015) {
		col_file      = cBodyMapCollisionFileName;
		hit_offset_y  = mOwner->mCommonSaveParams->mSLBodyHitOffsetY.get();
		attack_radius = mOwner->mCommonSaveParams->mSLBodyAttackRadius.get();
		attack_height = mOwner->mCommonSaveParams->mSLBodyAttackHeight.get();
		damage_radius = mOwner->mCommonSaveParams->mSLBodyDamageRadius.get();
		damage_height = mOwner->mCommonSaveParams->mSLBodyDamageHeight.get();
	} else {
		col_file      = cHeadMapCollisionFileName;
		hit_offset_y  = mOwner->mCommonSaveParams->mSLHeadHitOffsetY.get();
		attack_radius = mOwner->mCommonSaveParams->mSLHeadAttackRadius.get();
		attack_height = mOwner->mCommonSaveParams->mSLHeadAttackHeight.get();
		damage_radius = mOwner->mCommonSaveParams->mSLHeadDamageRadius.get();
		damage_height = mOwner->mCommonSaveParams->mSLHeadDamageHeight.get();
	}

	CalcMtxPtrFromJointName(
	    mMActor->getModel()->getModelData()->getJointName(),
	    cMapCollisionJointName, mMActor->getModel(), &mMapCollisionJointMtx);

	mMapCollision = new TMapCollisionMove();
	mMapCollision->init(col_file, 0x8000, this);

	mHitActor = new TWaterHitActor("ボスハナチャンの足パーツ");
	mHitActor->initHitActor(getInstanceIndex(), 1, 0x8000, attack_radius,
	                         attack_height, damage_radius, damage_height);
	group->getChildren().push_back(mHitActor);

	offHitFlag(HIT_FLAG_NO_COLLISION);
	mHitActor->mPosition.x = mMapCollisionJointMtx[3][0];
	mHitActor->mPosition.y = mMapCollisionJointMtx[3][1] - hit_offset_y;
	mHitActor->mPosition.z = mMapCollisionJointMtx[3][2];
}

void TBossHanachanPartsBody::initFootHitActor_(TIdxGroupObj* group)
{
	static const char* sFootJointName[] = { "foot_L", "foot_R" };

	J3DFrameCtrl* unused_ctrl;
	const TBossHanachanCommonSaveParams* p = mOwner->mCommonSaveParams;
	(void)unused_ctrl;

	for (int i = 0; i < 2; i++) {
		u16 joint_index
		    = getModel()->getModelData()->getJointName()->getIndex(
		        sFootJointName[i]);
		mFootHitActor[i] = new TFootHitActor("ボスハナチャンの足");
		mFootHitActor[i]->initHitActor(
		    getInstanceIndex(), 1, 0x8000,
		    p->mSLFootAttackRadius.get(), p->mSLFootAttackHeight.get(),
		    p->mSLFootDamageRadius.get(), p->mSLFootDamageHeight.get());
		group->getChildren().push_back(mFootHitActor[i]);
		offHitFlag(HIT_FLAG_NO_COLLISION);

		MtxPtr mtx = getModel()->getAnmMtx(joint_index);
		mFootHitActor[i]->unk6C = mtx;
		mFootHitActor[i]->mPosition.set(mtx[3][0], mtx[3][1], mtx[3][2]);
	}
}

void TBossHanachanPartsBase::offNonstopMotionBlend_()
{
	mNonstopMotionBlend->unk24 = 0;
}

void TBossHanachanPartsBase::setNonstopMotionBlendRatio_(f32 ratio)
{
	mNonstopMotionBlend->unk28 = ratio;
}

// TODO: UNUSED (0x30 bytes in the map), body is a guess.
void TBossHanachanPartsBase::restartBck_()
{
	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
	ctrl->setFrame(0.0f);
	ctrl->setRate(1.0f);
}

void TBossHanachanPartsBase::changeTumbleAnmRate_()
{
	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
	switch (mCurAnm) {
	case BH_ANM_KIND_UNK10:
	case BH_ANM_KIND_UNK11:
		if (ctrl->getFrame() >= 40.0f) {
			f32 target = (f32)ctrl->getEnd() - ctrl->getFrame();
			f32 rate   = ctrl->getRate();
			CLBChaseConstantSpecifyFrame<f32>(&rate, SMSGetAnmFrameRate(),
			                                 target);
			ctrl->setRate(rate);
		}
		break;
	default:
		ctrl->setRate(SMSGetAnmFrameRate());
		break;
	}
}

void TBossHanachanPartsBase::moveMapCollision_()
{
	MtxPtr mtx = mMapCollisionJointMtx;
	JGeometry::TVec3<f32> trans(mtx[3][0], mtx[3][1], mtx[3][2]);
	mMapCollision->moveTrans(trans);
}

void TBossHanachanPartsBase::entryCircleShadow_()
{
	if (mOwner->mSpine->getCurrentNerve() == &TNerveBossHanachanDead::theNerve()
	    && mOwner->mSpine->getTime() > 200) {
		return;
	}

	TCircleShadowRequest request;
	request.mPosition.set(mMapCollisionJointMtx[3][0],
	                      mMapCollisionJointMtx[3][1],
	                      mMapCollisionJointMtx[3][2]);
	request.mRadiusZ = request.mRadiusX = mScaledBodyRadius;
	gpBindShadowManager->forceRequest(request, getInstanceIndex());
}

void TBossHanachanPartsBase::setDamageFog_(JDrama::TGraphics* graphics)
{
	bool fog = true;
	if (getInstanceIndex() - 0x80000000 != 0x14) {
		fog = false;
	}

	J3DModelData* data = mMActor->getModel()->getModelData();
	u16 num            = data->mMaterialNum;
	JGeometry::TVec3<f32> pos(mMapCollisionJointMtx[3][0],
	                          mMapCollisionJointMtx[3][1],
	                          mMapCollisionJointMtx[3][2]);

	if (mOwner->mSpine->getLatestNerve()
	    == &TNerveBossHanachanDamage::theNerve()) {
		SMS_AddDamageFogEffect(data, pos, graphics);
		if (fog) {
			for (u16 i = 0; i < num; i++) {
				data->mMaterials[i]->change();
			}
		}
		if (unk10C == 0) {
			mMActor->getModel()->unlock();
		}
	} else {
		SMS_ResetDamageFogEffect(data);
	}
}

bool TBossHanachanPartsBase::isCurBckAlreadyEnd_() const
{
	bool result = true;
	if (mMActor != nullptr) {
		J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
		if (ctrl != nullptr) {
			bool reached_end
			    = ctrl->checkState(J3DFrameCtrl::STATE_COMPLETED_ONCE)
			      || ctrl->checkState(J3DFrameCtrl::STATE_LOOPED_ONCE);
			if (!reached_end) {
				if (!(0.1f + ctrl->getFrame() >= (f32)ctrl->getEnd()))
					result = false;
			}
		}
	}
	return result;
}

void TBossHanachanPartsBase::copyFrameFromOldAnmToNewAnm_()
{
	J3DAnmTransformKey* old_anm = nullptr;
	if (mMActor->mAnmBck != nullptr)
		old_anm = mMActor->mAnmBck->unk24;

	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
	if (old_anm != nullptr && ctrl != nullptr) {
		f32 frame;
		if (mMActor->mAnmBck == nullptr) {
			frame = 0.0f;
		} else {
			frame = mMActor->mAnmBck->getOldMotionBlendFrame();
		}
		old_anm->setFrame(frame);
		ctrl->setFrame(frame);
	}
}

TLiveActor* TBossHanachanPartsBase::getSandActor_() const
{
	const TLiveActor* actor = SMS_GetGroundActor(mGroundPlane, 0x400000CD);
	if (actor == nullptr)
		actor = SMS_GetGroundActor(mGroundPlane, 0x400000CB);
	return const_cast<TLiveActor*>(actor);
}

// TODO: UNUSED (0x64 bytes in the map), body is a guess.
bool TBossHanachanPartsBase::isMarioOn_() const
{
	if (!SMS_IsMarioTouchGround4cm())
		return false;
	const TBGCheckData* ground = *gpMarioGroundPlane;
	if (ground == nullptr)
		return false;
	return ground->getActor() == this;
}

void TBossHanachanPartsBase::calcRotateZWhenGetUp_()
{
	if (unk10C == 0) {
		switch (mCurAnm) {
		case BH_ANM_KIND_UNKB:
		case BH_ANM_KIND_UNK8: {
			J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
			f32 target
			    = ((f32)ctrl->getEnd() - ctrl->getFrame()) * 2.0f;
			if (target < 0.001f) {
				mRotation.z = 0.0f;
			} else {
				CLBChaseConstantSpecifyFrame<f32>(&mRotation.z, 0.0f, target);
			}
			break;
		}
		}
	}
}

// TODO: UNUSED (0xC4 bytes in the map). Body is a guess: it never had a live
// call site, so only its presence is load-bearing.
bool TBossHanachanPartsBase::isReactToTrampleOrHipDrop_() const
{
	return getInstanceIndex() - 0x80000000 <= 0x15
	    && (getRotation().z == -179.0f || getRotation().z == 179.0f);
}

void TBossHanachanPartsBase::considerSetAnm_(EnumBossHanachanNerveAnm anm)
{
	bool on_ground = false;

	switch (anm) {
	case BH_NERVE_ANM_TUMBLE:
		switch (mCurAnm) {
		case BH_ANM_KIND_UNK5:
		case BH_ANM_KIND_UNK6:
		case BH_ANM_KIND_UNKD:
		case BH_ANM_KIND_UNK10:
		case BH_ANM_KIND_UNK11:
			if (isCurBckAlreadyEnd_()) {
				setAnm_(BH_ANM_KIND_UNK3, BH_STOP_MOTION_BLEND_OFF);
			}
			break;
		}
		break;

	case BH_NERVE_ANM_DOWN:
		if (SMS_IsMarioTouchGround4cm()) {
			const TBGCheckData* ground = *gpMarioGroundPlane;
			if (ground != nullptr && ground->getActor() == this) {
				on_ground = true;
			}
		}
		switch (mCurAnm) {
		case BH_ANM_KIND_UNK5:
		case BH_ANM_KIND_UNK6:
		case BH_ANM_KIND_UNKD:
		case BH_ANM_KIND_UNK10:
		case BH_ANM_KIND_UNK11:
			if (isCurBckAlreadyEnd_()) {
				if (getInstanceIndex() - 0x80000000 <= 0x15 && on_ground) {
					setAnm_(BH_ANM_KIND_UNK2, BH_STOP_MOTION_BLEND_ON);
				} else {
					setAnm_(BH_ANM_KIND_UNK3, BH_STOP_MOTION_BLEND_OFF);
				}
			}
			break;
		default:
			if (getInstanceIndex() - 0x80000000 <= 0x15) {
				bool blend = mNonstopMotionBlend->unk24 > 0
				             && mNonstopMotionBlend->unk28 > 0.0f;
				if (blend) {
					if (mCurAnm == BH_ANM_KIND_UNK2 && on_ground) {
						setAnm_(BH_ANM_KIND_UNK3,
						        BH_STOP_MOTION_BLEND_ON);
					} else if (on_ground) {
						setAnm_(BH_ANM_KIND_UNK2,
						        BH_STOP_MOTION_BLEND_ON);
					}
				}
			}
			break;
		}
		break;

	case BH_NERVE_ANM_GET_UP:
		if (unk10C > 0) {
			unk10C--;
		}
		if (unk10C == 0 && isCurBckAlreadyEnd_()) {
			switch (mCurAnm) {
			case BH_ANM_KIND_UNK7:
				setAnm_(BH_ANM_KIND_UNK8, BH_STOP_MOTION_BLEND_OFF);
				break;
			case BH_ANM_KIND_UNK8:
				setAnm_(BH_ANM_KIND_UNK9, BH_STOP_MOTION_BLEND_OFF);
				break;
			case BH_ANM_KIND_UNKA:
				setAnm_(BH_ANM_KIND_UNKB, BH_STOP_MOTION_BLEND_OFF);
				break;
			case BH_ANM_KIND_UNKB:
				setAnm_(BH_ANM_KIND_UNKC, BH_STOP_MOTION_BLEND_OFF);
				break;
			default:
				if (mRotation.z < 0.0f) {
					setAnm_(BH_ANM_KIND_UNK7, BH_STOP_MOTION_BLEND_ON);
				} else {
					setAnm_(BH_ANM_KIND_UNKA, BH_STOP_MOTION_BLEND_ON);
				}
				break;
			}
		}
		break;

	case BH_NERVE_ANM_DAMAGE:
		if (unk10C > 0) {
			unk10C--;
			if (unk10C == 0) {
				setAnm_(BH_ANM_KIND_UNK6, BH_STOP_MOTION_BLEND_OFF);
			}
		} else if (mCurAnm == BH_ANM_KIND_UNK6 && isCurBckAlreadyEnd_()) {
			setAnm_(BH_ANM_KIND_UNK4, BH_STOP_MOTION_BLEND_OFF);
		}
		break;

	case BH_NERVE_ANM_SNORT:
		if (unk10C > 0) {
			unk10C--;
		}
		if (unk10C == 0 && mCurAnm != BH_ANM_KIND_UNKE) {
			setAnm_(BH_ANM_KIND_UNKE, BH_STOP_MOTION_BLEND_ON);
		}
		break;

	case BH_NERVE_ANM_DEAD:
		if (unk10C > 0) {
			unk10C--;
		}
		if (unk10C == 0) {
			setAnm_(BH_ANM_KIND_UNKF, BH_STOP_MOTION_BLEND_OFF);
		}
		break;
	}
}

BOOL TBossHanachanPartsBody::setAnm_(
    EnumBossHanachanAnmKind anm, EnumBossHanachanStopMotionBlendOnOff blend)
{
	BOOL result = false;
	if (mCurAnm != anm) {
		mOldAnm = mCurAnm;
		mCurAnm = anm;
		if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) != sBodyBckIndex[anm]) {
			int index = sBodyBckIndex[anm];
			if (mBodyIndex == mOwner->mWeakBodyIndex) {
				switch (anm) {
				case BH_ANM_KIND_UNK2:
					index = 0xB;
					break;
				case BH_ANM_KIND_UNK3:
					index = 0xE;
					break;
				}
			}
			mMActor->setBckFromIndex(index);
			result = true;
			if (blend == BH_STOP_MOTION_BLEND_ON) {
				mNonstopMotionBlend->unk24 = mNonstopMotionBlend->unk4;
			} else {
				mNonstopMotionBlend->unk24 = 0;
			}
			setCurAnmSound();
			result = true;
		}
	}

	if (anm == BH_ANM_KIND_UNKF) {
		mMActor->setBrkFromIndex(0);
		mMActor->getFrameCtrl(5)->setAttribute(0);
		mMActor->getModel()->unlock();
	}
	return result;
}

BOOL TBossHanachanPartsHead::setAnm_(
    EnumBossHanachanAnmKind anm, EnumBossHanachanStopMotionBlendOnOff blend)
{
	BOOL result = false;
	if (mCurAnm != anm) {
		mOldAnm = mCurAnm;
		mCurAnm = anm;
		if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) != sHeadBckIndex[anm]) {
			mMActor->setBckFromIndex(sHeadBckIndex[anm]);
			if (blend == BH_STOP_MOTION_BLEND_ON) {
				mNonstopMotionBlend->unk24 = mNonstopMotionBlend->unk4;
			} else {
				mNonstopMotionBlend->unk24 = 0;
			}
			setCurAnmSound();
			result = true;
		}
	}
	if (mMActor->getCurAnmIdx(ANM_TYPE_BTP) != sHeadBtpIndex[anm]) {
		mMActor->setBtpFromIndex(sHeadBtpIndex[anm]);
	}
	if (mMActor->getCurAnmIdx(ANM_TYPE_BTK) != sHeadBtkIndex[anm]) {
		mMActor->setBtkFromIndex(sHeadBtkIndex[anm]);
	}
	if (anm == BH_ANM_KIND_UNKF) {
		mMActor->setBrkFromIndex(1);
		mMActor->getFrameCtrl(5)->setAttribute(0);
	}
	return result;
}

BOOL TBossHanachanPartsBody::receiveMessage(THitActor* sender, u32 message)
{
	// the target returns false in game states 1, 2 and 4, so the body only
	// runs in every other state. Assigning the `||` chain to a bool is what
	// makes MWCC synthesise the two intermediate flag registers (r3, r4) and
	// hoist the leading `li r4, 1`. Reading the director into a local is what
	// keeps one register live across the reload for the third comparison.
	TMarDirector* director = gpMarDirector;
	bool blocked            = director->unk124 == 1 || director->unk124 == 2
	               || director->unk124 == 4;
	if (blocked) {
		return false;
	}

	BOOL result      = false;
	bool can_receive = false;
	// the target calls getLatestNerve() once and keeps the nerve in a register
	// across both comparisons
	TSpineBase<TLiveActor>::Nerve latest = mOwner->mSpine->getLatestNerve();
	if (latest == &TNerveBossHanachanTumble::theNerve()
	    || latest == &TNerveBossHanachanDown::theNerve()) {
		if (getActorType() - 0x80000000 > 0x15) {
			can_receive = true;
		} else if (getRotation().z == -179.0f ? true : false
		           || getRotation().z == 179.0f ? true : false) {
			can_receive = true;
		}
	}
	if (!can_receive) {
		return false;
	}

	switch (message) {
	case 0:
		if (mCurAnm == BH_ANM_KIND_UNK5) {
			mMActor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
		} else {
			setAnm_(BH_ANM_KIND_UNK5, BH_STOP_MOTION_BLEND_OFF);
		}
		result = true;
		break;
	case 1: {
		bool weak = mBodyIndex == mOwner->mWeakBodyIndex;
		switch (mCurAnm) {
		case BH_ANM_KIND_UNK2:
		case BH_ANM_KIND_UNK3:
		case BH_ANM_KIND_UNK5:
		case BH_ANM_KIND_UNKD:
		case BH_ANM_KIND_UNK10:
		case BH_ANM_KIND_UNK11:
			if (!weak) {
				if (mCurAnm == BH_ANM_KIND_UNKD) {
					mMActor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
				} else {
					setAnm_(BH_ANM_KIND_UNKD,
					        BH_STOP_MOTION_BLEND_OFF);
				}
			} else {
				setAnm_(BH_ANM_KIND_UNK6, BH_STOP_MOTION_BLEND_OFF);
				mOwner->execDamage();
			}
			result = true;
			break;
		}
		break;
	}
	}
	return result;
}

BOOL TBossHanachanPartsHead::receiveMessage(THitActor* sender, u32 message)
{
	// the target returns false in game states 1, 2 and 4, so the body only
	// runs in every other state. Assigning the `||` chain to a bool is what
	// makes MWCC synthesise the two intermediate flag registers (r3, r4) and
	// hoist the leading `li r4, 1`. Reading the director into a local is what
	// keeps one register live across the reload for the third comparison.
	TMarDirector* director = gpMarDirector;
	bool blocked            = director->unk124 == 1 || director->unk124 == 2
	               || director->unk124 == 4;
	if (blocked) {
		return false;
	}

	BOOL result      = false;
	bool can_receive = false;
	// the target calls getLatestNerve() once and keeps the nerve in a register
	// across both comparisons
	TSpineBase<TLiveActor>::Nerve latest = mOwner->mSpine->getLatestNerve();
	if (latest == &TNerveBossHanachanTumble::theNerve()
	    || latest == &TNerveBossHanachanDown::theNerve()) {
		if (getActorType() - 0x80000000 > 0x15) {
			can_receive = true;
		} else if (getRotation().z == -179.0f ? true : false
		           || getRotation().z == 179.0f ? true : false) {
			can_receive = true;
		}
	}
	if (!can_receive) {
		return false;
	}

	switch (message) {
	case 0:
		if (mCurAnm == BH_ANM_KIND_UNK5) {
			mMActor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
		} else {
			setAnm_(BH_ANM_KIND_UNK5, BH_STOP_MOTION_BLEND_OFF);
		}
		result = true;
		break;
	case 1:
		setAnm_(BH_ANM_KIND_UNK6, BH_STOP_MOTION_BLEND_OFF);
		mHitActor->onWaterHitCounter();
		result = true;
		break;
	}
	return result;
}
