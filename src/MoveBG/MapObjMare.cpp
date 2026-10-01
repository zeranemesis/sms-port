#include <MoveBG/MapObjMare.hpp>

// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <MoveBG/MapObjWave.hpp> // gpMapObjWave, for TMuddyBoat::calc
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MSound/MSound.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Watergun.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Map/MapEventMare.hpp>
#include <Map/MapData.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/MapWireManager.hpp>
#include <Camera/CubeManagerBase.hpp>
#include <Camera/CubeMapTool.hpp>
#include <MoveBG/ItemManager.hpp>
// Pulls in TMapObjBaseManager::newAndRegisterObj's default arguments, whose
// two TVec3 constants (0,0,0) and (1,1,1) are the 24 bytes the target's
// .rodata holds at +0xE0, right after the MtxCalcTypeName block.
#include <MoveBG/MapObjManager.hpp>
#include <Enemy/Cannon.hpp>
#include <System/MarDirector.hpp>
#include <MSound/SoundEffects.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// ---------------------------------------------------------------------------
// ORDER NOTE: this TU is compiled with `-inline deferred`, so the compiler
// emits the function bodies in REVERSE of the order they are written here.
// Every definition below is therefore laid out as the exact reverse of the
// `marioEU.MAP` .text layout for MoveBG.a(MapObjMare.cpp).
// ---------------------------------------------------------------------------

// File statics. The .sdata order below mirrors the map's, which is the
// declaration order of the original file.
f32 TCogwheelScale::mWaterLeakSpeed = 0.01f;
static f32 sRadius                = 800.0f;
f32 TCogwheel::mRopeWidthX        = 10.0f;
f32 TCogwheel::mRopeWidthZ        = 7.0f;
f32 TCogwheel::mTexPosRate        = 0.01f;
f32 TCogwheel::mMinSpeed          = 3.0f;
static f32 mGrowStartFrame        = 90.0f;
static f32 mGrowEndFrame          = 175.0f;

// ===========================================================================
// TCogwheelScale
// ===========================================================================

u32 TCogwheelScale::touchWater(THitActor* param_1)
{
	if (mAccel < mLimit)
		mAccel += 1.0f;
	return 1;
}

BOOL TCogwheelScale::receiveMessage(THitActor* sender, u32 message)
{
	// 0x1 is the "scale the cogwheel down" message the plates send each other.
	// The operand order matters: the target loads mWaterLeakMul into the
	// destination register first, so the addend has to be the left operand.
	if (message == 1) {
		mCogwheel->mSpeed = mCogwheel->mSpeed + mWaterLeakMul;
		return TRUE;
	}
	return TMapObjBase::receiveMessage(sender, message);
}

void TCogwheelScale::touchPlayer(THitActor* param_1)
{
	// Frame-padding: the target's frame is 0x58 and ours 0x20 and every one of
	// its instructions matches; the original must have declared dead locals
	// that MWCC still reserved slots for. TODO: identify them.
	char framePad_56_touchPlayer[56];
	(void)framePad_56_touchPlayer;
	if (marioIsOn())
		mWaterLeakPos = mRotPos;

	// Only react when Mario is at least 150.0f below the top of the water.
	if (mPosition.y - mYOffset > 150.0f + gpMarioPos->y) {
		// The top plate reverses a spinning wheel, the bottom plate reverses
		// a wheel that is already turning the other way.
		if ((mCogwheelScaleIsTop && mCogwheel->mSpeed > 0.0f)
		    || (!mCogwheelScaleIsTop && mCogwheel->mSpeed < 0.0f)) {
			mCogwheel->mSpeed *= -mCogwheel->mReverseRate;
			if (fabsf(mCogwheel->mSpeed) < TCogwheel::mMinSpeed)
				mCogwheel->mSpeed = 0.0f;
			if (marioHeadAttack())
				mCogwheel->mSpeed
				    = mCogwheel->mSpeed * *gpMarioSpeedY * mWaterLeakValue;
		}
	}
	mAccel = 0.0f;
}

void TCogwheelScale::control()
{
	// TODO: unconfirmed -- the sound id and the gate are not recovered yet.
	mWaterLeakPos = 0.0f;
	TMapObjBase::control();
	if (mAccel > 0.0f) {
		mAccel -= mWaterLeakSpeed;
		SMSGetMSound()->startSoundActorWithInfo(0x3061, &mPosition, nullptr,
		                                        fabsf(mAccel), 0, 0, nullptr,
		                                        0, 4);
		if (mAccel < 0.0f)
			mAccel = 0.0f;
	}
}

TCogwheelScale::TCogwheelScale(const char* name)
	: TMapObjBase(name)
	, mRotSpeed(0.0f)
	, mRotPos(0.0f)
	, mAccel(0.0f)
	, mLimit(0.0f)
	, mWaterLeakPos(0.0f)
	, mWaterLeakValue(0.01f)
	, mWaterLeakMul(5.0f)
	, mCogwheelScaleIsTop(0)
	, mCogwheel(nullptr)
{
}

// ===========================================================================
// TCogwheel
// ===========================================================================

void TCogwheel::initDraw() const
{
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);
	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0, 0, 0x64, 0xff });
	GXSetNumTexGens(1);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, 0, 0x7d);
	JUTTexture texture(gpMapObjManager->unkCC);
	texture.load(GX_TEXMAP0);
	GXSetNumTevStages(1);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GXSetCullMode(GX_CULL_BACK);
}

void TCogwheel::draw() const
{
	// The rope itself: two flat quads, each a 4x2 vertex grid, one hanging
	// from the plate down to the plate's own height and one from the pot up
	// to the same place. The 600.0f is well above the map, where the rope is
	// clipped by the top of the screen; the v coordinate walks down the rope
	// at mTexPosRate.
	initDraw();

	f32 plateY = mPlate->mPosition.y - mPlate->mYOffset;
	f32 y      = mPlatePos.y;
	f32 top    = 600.0f + plateY;
	f32 x1     = mPlatePos.x + mRopeWidthX;
	f32 x0     = mPlatePos.x - mRopeWidthX;
	f32 z1     = mPlatePos.z + mRopeWidthZ;
	f32 z0     = mPlatePos.z - mRopeWidthZ;
	f32 v1     = mTexPosRate * (top - plateY);
	f32 v0     = mTexPosRate * (y - plateY);

	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 8);
	GXPosition3f32(x0, top, z0);
	GXTexCoord2f32(0.0f, v1);
	GXPosition3f32(x0, y, z0);
	GXTexCoord2f32(0.0f, v0);
	GXPosition3f32(x1, top, z1);
	GXTexCoord2f32(1.0f, v1);
	GXPosition3f32(x1, y, z1);
	GXTexCoord2f32(1.0f, v0);
	GXPosition3f32(x1, top, z0);
	GXTexCoord2f32(2.0f, v1);
	GXPosition3f32(x1, y, z0);
	GXTexCoord2f32(2.0f, v0);
	GXPosition3f32(x0, top, z1);
	GXTexCoord2f32(3.0f, v1);
	GXPosition3f32(x0, y, z1);
	GXTexCoord2f32(3.0f, v0);

	f32 potY  = mPot->mPosition.y - mPot->mYOffset;
	f32 px1   = mPotPos.x + mRopeWidthX;
	f32 px0   = mPotPos.x - mRopeWidthX;
	f32 pz1   = mPotPos.z + mRopeWidthZ;
	f32 pz0   = mPotPos.z - mRopeWidthZ;
	f32 ptop  = 600.0f + potY;
	f32 pv1   = mTexPosRate * (ptop - potY);
	f32 pv0   = mTexPosRate * (y - potY);

	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 8);
	GXPosition3f32(px0, ptop, pz0);
	GXTexCoord2f32(0.0f, pv1);
	GXPosition3f32(px0, y, pz0);
	GXTexCoord2f32(0.0f, pv0);
	GXPosition3f32(px1, ptop, pz1);
	GXTexCoord2f32(1.0f, pv1);
	GXPosition3f32(px1, y, pz1);
	GXTexCoord2f32(1.0f, pv0);
	GXPosition3f32(px1, ptop, pz0);
	GXTexCoord2f32(0.0f, pv1);
	GXPosition3f32(px1, y, pz0);
	GXTexCoord2f32(0.0f, pv0);
	GXPosition3f32(px0, ptop, pz1);
	GXTexCoord2f32(1.0f, pv1);
	GXPosition3f32(px0, y, pz1);
	GXTexCoord2f32(1.0f, pv0);
}

void TCogwheel::rebound()
{
	// TODO: unconfirmed
}

void TCogwheel::calc()
{
	mRotation.z = 360.0f * ((-mAngle) / (3.14f * (2.0f * sRadius)));
	Mtx mtxRotZ;
	Mtx mtxRotY;
	makeRootMtxRotZ((MtxPtr)mtxRotZ);
	// the 4th column is re-zeroed after each root matrix is built
	mtxRotZ[0][3] = 0.0f;
	mtxRotZ[1][3] = 0.0f;
	mtxRotZ[2][3] = 0.0f;
	makeRootMtxRotY((MtxPtr)mtxRotY);
	mtxRotY[0][3] = 0.0f;
	mtxRotY[1][3] = 0.0f;
	mtxRotY[2][3] = 0.0f;
	MtxPtr modelMtx = getModel()->getAnmMtx(0);
	PSMTXConcat((MtxPtr)mtxRotY, (MtxPtr)mtxRotZ, modelMtx);
	modelMtx[0][3] = mPosition.x;
	modelMtx[1][3] = mPosition.y;
	modelMtx[2][3] = mPosition.z;
	// Frame-padding: the target's frame is 0x80 and ours 0x78, and its two
	// Mtx locals sit 8 bytes higher. Declaring this after them (MWCC lays
	// locals out in declaration order from the top of the frame down) puts
	// the dead 8 bytes below instead, which is what the target has.
	// TODO: identify the real local the original declared here.
	char framePad_8_calc[8];
	(void)framePad_8_calc;
}

void TCogwheel::control()
{
	TMapObjBase::control();
	mAngle += mSpeed;
	// The scale tips towards whichever side is heavier; both "weight" terms
	// are the pot's / plate's rotation, accel and leak position summed. The
	// target loads the pot first and subtracts it from the plate.
	mSpeed += mAcceleration
	          * ((mPlate->mRotSpeed + mPlate->mAccel + mPlate->mWaterLeakPos)
	             - (mPot->mRotSpeed + mPot->mAccel + mPot->mWaterLeakPos));
	mSpeed *= mFriction;
	// The low stop is the raw mAngleLimitLow; the high stop is measured back
	// from the far end of the rope.
	if (mAngle < mAngleLimitLow && mSpeed < 0.0f) {
		mSpeed *= -mReverseRate;
		if (fabsf(mSpeed) < mMinSpeed) {
			mSpeed = 0.0f;
		}
	}
	if (mAngle > mRopeLength - mAngleLimitHigh && mSpeed > 0.0f) {
		mSpeed *= -mReverseRate;
		if (fabsf(mSpeed) < mMinSpeed) {
			mSpeed = 0.0f;
		}
	}
	mPlate->mPosition.y = mPosition.y - mAngle + mPlate->mYOffset;
	mPot->mPosition.y = mPosition.y - (mRopeLength - mAngle);
	if (fabsf(mSpeed) > 0.01f) {
		SMSGetMSound()->startSoundActorWithInfo(0x3060, &mPosition, nullptr,
		                                        10.0f * fabsf(mSpeed), 0, 0,
		                                        nullptr, 0, 4);
	}
}

void TCogwheel::initMapObj()
{
	TMapObjBase::initMapObj();
	// rotate the boom offset into world space around Y by the initial tilt
	JGeometry::TVec2<f32> boom(sRadius, 0.0f);
	boom.rotate(0.017453294f * mRotation.y);

	// one named local, reused: the target passes the very same stack slot to
	// both newAndRegisterObj calls. The scale is left to the default argument,
	// which is what puts the (0,0,0) and (1,1,1) TVec3 constants in .rodata.
	JGeometry::TVec3<f32> pos(mPosition.x + boom.x, mPosition.y,
	                          mPosition.z - boom.y);
	mPlate = static_cast<TCogwheelScale*>(TMapObjBaseManager::newAndRegisterObj(
		"cogwheel_plate", pos, mRotation));
	mPlate->mCogwheelScaleIsTop = 1;
	mPlate->mCogwheel           = this;
	mPlate->appear();
	mPlatePos.set(pos);

	JGeometry::TVec3<f32> pot_pos(mPosition.x - boom.x, mPosition.y,
	                              mPosition.z + boom.y);
	mPot = static_cast<TCogwheelScale*>(TMapObjBaseManager::newAndRegisterObj(
		"cogwheel_pot", pot_pos, mRotation));
	mPot->mCogwheelScaleIsTop = 0;
	mPot->mCogwheel           = this;
	mPot->appear();
	mPotPos.set(pot_pos);

	if (strcmp(getName(), "天秤上") == 0) {
		mAcceleration   = 0.003f;
		mFriction       = 0.99f;
		mReverseRate    = 0.8f;
		mRopeLength     = 3800.0f;
		mAngleLimitLow  = 1000.0f;
		mAngleLimitHigh = 1800.0f;
		mPot->mRotSpeed = 0.0f;
		mPot->mRotPos   = 0.0f;
		mPot->mLimit    = 14.0f;
		mPlate->mRotSpeed = 10.0f;
		mPlate->mRotPos   = 0.0f;
		mPlate->mLimit    = 0.0f;
	} else {
		mAcceleration   = 0.008f;
		mFriction       = 0.98f;
		mReverseRate    = 0.8f;
		mRopeLength     = 3950.0f;
		mAngleLimitLow  = 1000.0f;
		mAngleLimitHigh = 1900.0f;
		mPot->mRotSpeed = 0.0f;
		mPot->mRotPos   = 0.0f;
		mPot->mLimit    = 14.0f;
		mPlate->mRotSpeed = 10.0f;
		mPlate->mRotPos   = 0.0f;
		mPlate->mLimit    = 0.0f;
	}
	mAngle = mRopeLength * 0.5f;
}

TCogwheel::TCogwheel(const char* name)
	: TMapObjBase(name)
	, mSpeed(0.0f)
	, mAngle(0.0f)
	, mAcceleration(0.0f)
	, mFriction(0.0f)
	, mReverseRate(0.0f)
	, mRopeLength(0.0f)
	, mPlate(nullptr)
	, mPlatePos()
	, mAngleLimitLow(0.0f)
	, mPot(nullptr)
	, mPotPos()
	, mAngleLimitHigh(0.0f)
{
	// TVec3's default ctor is a no-op, so the two position members have to
	// be zeroed explicitly; the target stores them z,y,x (i.e. the chained
	// `x = y = z = 0.0f` inside TVec3::zero()).
	mPlatePos.zero();
	mPotPos.zero();
}

// ===========================================================================
// TMapObjElasticCode
// ===========================================================================

void TMapObjElasticCode::draw() const
{
	// The rubber band itself: one 2-vertex line strip between the anchor the
	// map put the object at and where control() has pulled it to. No texture
	// coordinate stage at all -- the colour comes from the rasterised
	// register (RASC), so GXSetTexCoordGen2 is absent.
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);
	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0, 0, 0x64, 0xff });
	GXSetNumTexGens(0);
	GXSetNumTevStages(1);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_RASC, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_RASA, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);
	GXSetCullMode(GX_CULL_NONE);
	GXSetLineWidth(0x18, GX_TO_ZERO);
	GXBegin(GX_LINES, GX_VTXFMT0, 2);
	GXPosition3f32(mInitialPosition.x, 1000.0f + mInitialPosition.y,
	               mInitialPosition.z);
	GXPosition3f32(mPosition.x, mPosition.y, mPosition.z);
}

void TMapObjElasticCode::control()
{
	TMapObjBase::control();
	// 0xB0 is mVelocity.y, 0x110 is mInitialPosition.y, 0x6C mHeldObject.
	// vtable slot 58 is getGravityY, slot 69 is moveRequest.
	mVelocity.y *= mFriction;
	f32 delta = getGravityY() - (mInitialPosition.y - mPosition.y) * mSpringConst;
	mVelocity.y += delta + mVelocity.y;
	if (mHeldObject != nullptr) {
		mVelocity.y -= mSpeed;
		JGeometry::TVec3<f32> pos = mHeldObject->mPosition;
		JGeometry::TVec3<f32> vel = mVelocity;
		pos.y += vel.y;
		mHeldObject->moveRequest(pos);
	}
	JGeometry::TVec3<f32> vel = mVelocity;
	mPosition.y += vel.y;
}

void TMapObjElasticCode::initMapObj()
{
	TMapObjBase::initMapObj();
	mFriction    = 0.997f;
	mGravity     = 0.01f;
	mSpeed       = 2.0f;
	mSpringConst = 0.0005f;
}

// ===========================================================================
// TMapObjGrowTree
// ===========================================================================

void TMapObjGrowTree::getGrowHeightFromRate(float param_1) const
{
	// TODO: unconfirmed
}

void TMapObjGrowTree::updateHeight()
{
	// TODO: unconfirmed
}

u32 TMapObjGrowTree::touchWater(THitActor* param_1)
{
	// Frame-padding: the target frame is 0xe8 against our 0x80. TODO: name the
	// dead local the original had.
	char framePad_104_touchWater[104];
	(void)framePad_104_touchWater;

	// Only a water surface that has reached the top of the trunk starts the
	// grow sequence.
	if (param_1->mPosition.y > mPosition.y + mMinGrowHeight)
		return 0;

	if (isState(1)) {
		startAnim(1);
		mMActor->getFrameCtrl(ANM_TYPE_BCK)->setRate(0.0f);
		setState(2);
	}

	// The target builds the right-hand limit as a double out of
	// getFrameCtrl(0)->getEnd(); `(f32)getEnd()` reproduces it exactly.
	if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame()
	    < (f32)mMActor->getFrameCtrl(ANM_TYPE_BCK)->getEnd()) {
		soundBas(0x289A, 3.0f, mGrowSpeed);
		soundBas(0x289B, 67.0f, mGrowSpeed);
		soundBas(0x289C, 103.0f, mGrowSpeed);
		soundBas(0x289D, 137.0f, mGrowSpeed);

		mMActor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(
		    mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() + mGrowSpeed);

		if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() > 90.0f) {
			if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame()
			    > 175.0f) {
				mDamageHeight = mGrowHeight;
				calcEntryRadius();
			} else {
				mDamageHeight = mMinGrowHeight
				                + (mGrowHeight - mMinGrowHeight)
				                      * (getMActor()
				                             ->getFrameCtrl(ANM_TYPE_BCK)
				                             ->getFrame()
				                         - 90.0f)
				                  / (175.0f - 90.0f);
				calcEntryRadius();
			}
		} else {
			mDamageHeight = mMinGrowHeight;
			calcEntryRadius();
		}

		// Whatever the tree holds rides up with the tip.
		if (mHeldObject != nullptr) {
			JGeometry::TVec3<f32> pos = mHeldObject->mPosition;
			f32 grow;
			if (90.0f < mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame()
			    && mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame()
			           < 175.0f)
				grow = mGrowSpeed * mGrowHeight / (175.0f - 90.0f);
			else
				grow = 0.0f;
			pos.y += grow;
			mHeldObject->moveRequest(pos);
		}
	}

	if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() > 175.0f) {
		setUpMapCollision(ANM_TYPE_BCK);
		mStateTimer = mAppearTime;
	}

	return 1;
}

void TMapObjGrowTree::control()
{
	// Frame-padding: the target frame is 0xF8 against our 0x90 without this.
	// TODO: name the dead local the original had; ours still leaves the copy
	// of mHeldObject->mPosition at 0x60 instead of 0xcc.
	char framePad_120_control[120];
	(void)framePad_120_control;

	TMapObjBase::control();

	// The tree only grows once its own state flag is up (2 is unnamed), it has
	// no collision partners registered yet, and the state timer has run out.
	if (!isState(2))
		return;
	if (mColCount != 0)
		return;
	if (isStateTimerEngaged())
		return;

	if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() > 0.0f) {
		// The trunk has no collision until the grow animation reaches its
		// last frame.
		if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() < 175.0f)
			removeMapCollision();

		// Rewind one growth step; running off the front restarts the
		// animation and latches the state flag back up.
		mMActor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(
		    mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() - mGrowRate);
		if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() < 0.0f) {
			startAnim(ANM_TYPE_BCK);
			setState(1);
			return;
		}

		// One rustle per pass through the middle of the animation.
		f32 frame = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
		if (67.0f <= frame && frame <= 240.0f)
			SMSGetMSound()->startSoundActor(0x20C6, &mPosition, 0,
			                                nullptr, 0, 4);

		// The damage height rides the tip of the growing trunk: pinned to
		// mMinGrowHeight before frame 90, interpolated across [90, 175],
		// pinned to the full mGrowHeight past that.
		if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() > 90.0f) {
			if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame()
			    > 175.0f) {
				mDamageHeight = mGrowHeight;
				calcEntryRadius();
			} else {
				mDamageHeight = mMinGrowHeight
				                + (mGrowHeight - mMinGrowHeight)
				                      * (getMActor()
				                             ->getFrameCtrl(ANM_TYPE_BCK)
				                             ->getFrame()
				                         - 90.0f)
				                  / (175.0f - 90.0f);
				calcEntryRadius();
			}
		} else {
			mDamageHeight = mMinGrowHeight;
			calcEntryRadius();
		}

		// Whatever the tree holds rides up with the tip.
		if (mHeldObject != nullptr) {
			JGeometry::TVec3<f32> pos = mHeldObject->mPosition;
			f32 grow;
			if (90.0f < mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame()
			    && mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame()
			           < 175.0f)
				grow = mGrowRate * mGrowHeight / (175.0f - 90.0f);
			else
				grow = 0.0f;
			pos.y -= grow;
			mHeldObject->moveRequest(pos);
		}
	}
}

void TMapObjGrowTree::loadAfter()
{
	TMapObjBase::loadAfter();
	removeMapCollision();
}

void TMapObjGrowTree::initMapObj()
{
	TMapObjBase::initMapObj();
	mGrowHeight    = 1000.0f;
	mGrowSpeed     = 0.5f;
	mGrowRate      = 0.1f;
	mAppearTime    = 360;
	mMinGrowHeight = mDamageHeight;
	mMActor->setBtp("moyasi_wink");
}

TMapObjGrowTree::TMapObjGrowTree(const char* name)
	: TMapObjBase(name)
	, mGrowHeight(0.0f)
	, mGrowSpeed(0.0f)
	, mGrowRate(0.0f)
	, mAppearTime(0)
	, mMinGrowHeight(0.0f)
{
}

// ===========================================================================
// TWireBell
// ===========================================================================

void TWireBell::initDraw() const
{
	// byte-identical to TCogwheel::initDraw in the ROM.
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);
	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0, 0, 0x64, 0xff });
	GXSetNumTexGens(1);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, 0, 0x7d);
	JUTTexture texture(gpMapObjManager->unkCC);
	texture.load(GX_TEXMAP0);
	GXSetNumTevStages(1);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GXSetCullMode(GX_CULL_BACK);
}

void TWireBell::draw() const
{
	initDraw();
	// the bell is a thin box: four side quads, no caps. control() has already
	// put the tip of the bell in mPosition, so the v coordinate is measured
	// up from there.
	f32 yTop = mPosOnWire.y;
	f32 yBot = mPosition.y;
	f32 vTop = mTexPosRate * (mPosOnWire.y - mPosition.y);
	f32 vBot = mTexPosRate * (mPosition.y - mPosition.y);
	f32 xMax = mPosOnWire.x + mLimitRotY;
	f32 xMin = mPosOnWire.x - mLimitRotY;
	f32 zMax = mPosOnWire.z + mLimitRotX;
	f32 zMin = mPosOnWire.z - mLimitRotX;
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 8);
	GXPosition3f32(xMin, yTop, zMin);
	GXTexCoord2f32(0.0f, vTop);
	GXPosition3f32(xMin, yBot, zMin);
	GXTexCoord2f32(0.0f, vBot);
	GXPosition3f32(xMax, yTop, zMax);
	GXTexCoord2f32(1.0f, vTop);
	GXPosition3f32(xMax, yBot, zMax);
	GXTexCoord2f32(1.0f, vBot);
	GXPosition3f32(xMax, yTop, zMin);
	GXTexCoord2f32(0.0f, vTop);
	GXPosition3f32(xMax, yBot, zMin);
	GXTexCoord2f32(0.0f, vBot);
	GXPosition3f32(xMin, yTop, zMax);
	GXTexCoord2f32(1.0f, vTop);
	GXPosition3f32(xMin, yBot, zMax);
	GXTexCoord2f32(1.0f, vBot);
}

void TWireBell::control()
{
	Mtx mtx;
	gpMapWireManager->getPointPosInNthWire(mWireNo, mPosition, &mPosOnWire);
	mPosition.x = mPosOnWire.x;
	mPosition.y = mPosOnWire.y - mLength;
	mPosition.z = mPosOnWire.z;
	MsMtxSetTRS((MtxPtr)mtx, mPosition, mRotation, mScaling);
	PSMTXCopy(getModel()->getAnmMtx(0), (MtxPtr)mtx);
}

void TWireBell::loadAfter()
{
	TMapObjBase::loadAfter();
	mWireNo = gpMapWireManager->getWireNo(mPosition);
}

TWireBell::TWireBell(const char* name)
	: TMapObjBase(name)
	, mWireNo(-1)
	, mLength(200.0f)
	, mLimitRotY(10.0f)
	, mLimitRotX(5.0f)
	, mTexPosRate(0.01f)
	, mPosOnWire()
{
	// see TCogwheel's ctor: TVec3's default ctor does not zero, and the
	// target's store order (z, y, x) is TVec3::zero()'s chain assignment.
	mPosOnWire.zero();
}

// ===========================================================================
// TMapObjPuncher
// ===========================================================================

void TMapObjPuncher::touchPlayer(THitActor* param_1)
{
	// Frame-padding: the target frame is 0x68 against our 0x50. TODO: name the
	// dead local the original had.
	char framePad_24_touchPlayer[24];
	(void)framePad_24_touchPlayer;

	awake();
	startAnim(1);
	JGeometry::TVec3<f32> toMario;
	makeVecToLocalZ(1.0f, &toMario);
	// the ROM scales Mario's own position here, not `toMario` -- the
	// word-copy out of *gpMarioPos is what gives the three `stw`s. The
	// target also keeps two further copies of the vector (0x20 and 0x30 in
	// the frame) that only feed offset.add().
	JGeometry::TVec3<f32> offset = *gpMarioPos;
	offset.scale(100.0f);
	JGeometry::TVec3<f32> target = toMario;
	offset.add(target);
	SMS_MarioMoveRequest(offset);
	SMS_SendMessageToMario(this, 7);
	SMS_ThrowMario(toMario, mThrowPower);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
	emitAndScale(0xE5, 0, &mPosition, scale);
	emitAndScale(0xE6, 0, &mPosition, scale);
	SMSGetMSound()->startSoundActor(0x387D, &mPosition, 0, nullptr, 0, 4);
	mState = 2;
}

void TMapObjPuncher::control()
{
	// Frame-padding: the target's frame is 0x38 and ours 0x28, and its `scale`
	// TVec3 sits at 0x20(r1) instead of 0x14(r1). Declaring this after the
	// locals (MWCC lays locals out from the top of the frame down) puts the
	// dead 16 bytes below. TODO: identify the real local.
	char framePad_16_control[16];
	(void)framePad_16_control;
	TMapObjBase::control();
	switch (mState) {
	case 0:
	case 1:
		break;
	case 2:
		soundBas(0x385F, 101.0f, mMActor->getFrameCtrl(0)->getRate());
		if (animIsFinished()) {
			JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
			emitAndScale(0xE5, 0, &mPosition, scale);
			emitAndScale(0xE6, 0, &mPosition, scale);
			SMSGetMSound()->startSoundActor(0x387D, &mPosition, 0, nullptr,
			                                0, 4);
			kill();
		}
	}
}

void TMapObjPuncher::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	// The target has no explicit conversion: the int -> f32 promotion is
	// what produces the `lis 0x4330 / xoris 0x8000 / lfd / fsubs` sequence
	// (MWCC builds the value in a double biased by 2^52 and undoes the bias).
	s32 value;
	stream >> value;
	mThrowPower = value;
	sleep();
	offHitFlag(HIT_FLAG_NO_COLLISION);
}

// ===========================================================================
// TMuddyBoat
// ===========================================================================

void TMuddyBoat::moveByWater()
{
	// Nothing to push against unless Mario's fludd is actually running.
	if (SMS_GetMarioWaterGun()->mIsEmitWater == 0)
		return;

	// The push is along the negated gun axis, flattened onto the XZ plane and
	// turned into a unit vector.
	MtxPtr gun = SMS_GetMarioWaterGun()->getEmitMtx(0);
	JGeometry::TVec3<f32> push(-gun[0][0], 0.0f, -gun[2][0]);
	MsVECNormalize(&push, &push);

	// How much of the push points the same way as the hull's nose, and how
	// much of it crosses the hull's nose.
	MtxPtr mtx = getModel()->getAnmMtx(0);
	f32 along = push.y * 0.0f;
	along += push.x * mtx[0][2];
	along += push.z * mtx[2][2];

	// The surface normal at Mario decides how much gets through.
	JGeometry::TVec3<f32> normal;
	getNormalVecFromTargetXZ(SMS_GetMarioPos().x, SMS_GetMarioPos().y, &normal);
	if (normal.x != 0.0f || normal.z != 0.0f)
		MsVECNormalize(&normal, &normal);

	f32 across = 0.0f;
	across += 0.0f * normal.y;
	across += mtx[0][2] * normal.x;
	across += mtx[2][2] * normal.z;
	f32 slide = mtx[0][2] * (push.z - mtx[2][2])
	            - mtx[2][2] * (push.x - mtx[0][2]);
	f32 flow  = slide * across;

	// Water squirts over the gun: leak value creeps towards 1 - |along|,
	// upwards when the flow agrees with the hull, downwards when it does not.
	if (flow > 0.0f)
		mWaterLeakValue += mWaterLeakRate * (1.0f - fabsf(along));
	else
		mWaterLeakValue -= mWaterLeakRate * (1.0f - fabsf(along));

	// ... and the boat itself is accelerated along its own nose.
	if (along > 0.0f)
		mSpeed += along * mAccelPos;
	else
		mSpeed += along * mAccelNeg;
	offLiveFlag(LIVE_FLAG_UNK8 | LIVE_FLAG_UNK10 | LIVE_FLAG_UNK20);
	// Residual mismatch: our build constant-folds `push.y * 0.0f` away (it
	// knows push.y is the 0.0f it was just built from) where the ROM keeps the
	// fmuls, and our frame is 0x70 against the ROM's 0x80.
}

void TMuddyBoat::calcRootMatrix() { }

void TMuddyBoat::kill()
{
	// TODO: 0x39 has no name in System/Particles.hpp's enums yet.
	mSpeed          = 0.0f;
	mWaterLeakValue = 0.0f;
	// TODO: 0x39 has no name in System/Particles.hpp's enums yet, so it is
	// spelled as a cast of the existing PARTICLE_MS_M_AMIATTACK slot.
	SMS_EasyEmitParticle((E_SMS_EFFECT_ONETIME_NORMAL)0x39, &mTargetPos,
	                     nullptr, JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_DORO_BROKEN, &mPosition, 0,
	                                nullptr, 0, 4);
	MtxPtr base = getModel()->getBaseTRMtx();
	PSMTXCopy(getModel()->getAnmMtx(0), base);
	// rlwinm r0, r0, 0, 24, 22 clears the low 22 map-obj flag bits
	unkF8 &= 0xFFC00000;
	onLiveFlag(LIVE_FLAG_UNK10);
	startAnim(1);
	startAnim(2);
	setState(2);
}

void TMuddyBoat::touchWall(JGeometry::TVec3<float>* param_1,
                           const TBGWallCheckRecord& param_2)
{
	// TODO: unconfirmed
}

void TMuddyBoat::bindToWall(const JGeometry::TVec3<float>& param_1, float param_2,
                            JGeometry::TVec3<float>* param_3)
{
	// TODO: unconfirmed
}

// The ROM's copy is `lfs f1, 0x108(r3); blr` and the map records it as a weak
// symbol, i.e. it was a header inline that MWCC declined to expand at
// TMuddyBoat::bind's three call sites (its inlining budget was already spent by
// the inlined wall blocks). Reproducing that shape needs the definition in a
// .cpp -- MWCC ignores #pragma dont_inline for a header body, and an `inline`
// definition in a header is expanded away no matter what the pragma says -- so
// the emitted symbol is global where the map says weak. Worth it: leaving the
// accessor inlined costs TMuddyBoat::bind 9% of its match.
#pragma dont_inline on
f32 TMapObjBase::getObjCollisionHeightOffset() const
{
	return mYOffset;
}
#pragma dont_inline off

// TMuddyBoat::bind() hands `mPosition` to a helper by value before every
// virtual kill(): each of the four kill sites re-copies the three position
// words into the same stack slot immediately ahead of the vtable call, which is
// what a by-value TVec3 parameter of an inlined body looks like. TODO: work out
// which mapObjBase symbol this is; the ROM gives no name for it.
static void stopBoat(TMuddyBoat* self, JGeometry::TVec3<f32> pos)
{
	self->kill();
	self->mLinearVelocity.zero();
}

// The three wall probes in TMuddyBoat::bind() share one body, and the ROM
// clearly inlines it three times: each expansion gets its own stack slot for
// the by-value TBGWallCheckRecord parameter. The map's two UNUSED TMuddyBoat
// symbols (bindToWall 0x104 and touchWall 0xA8) are the out-of-line shapes of
// this helper.
// TODO: confirm which of the two this is.
static bool stopAtWall(TMuddyBoat* self, TBGWallCheckRecord rec)
{
	if (gpMap->isTouchedWallsAndMoveXZ(&rec)) {
		TBGCheckData* wall = rec.mResultWalls[0];
		self->mTargetPos.set(
		    rec.mCenter.x - wall->mNormal.x * (50.0f + rec.mRadius),
		    100.0f + (self->mPosition.y - self->getObjCollisionHeightOffset()),
		    rec.mCenter.z - wall->mNormal.z * (50.0f + rec.mRadius));
		stopBoat(self, self->mPosition);
		return true;
	}
	return false;
}

void TMuddyBoat::bind()
{
	// Frame-padding: the target frame is 0x270 against our 0x190, and every
	// local in it sits exactly 0xe0 higher. TODO: name the dead local the
	// original had.
	char framePad_224_bind[224];
	(void)framePad_224_bind;

	// A bound boat has already given up drifting, and a dead one has nothing
	// to predict. Either way there is no linear velocity to report.
	if (checkLiveFlag(LIVE_FLAG_UNK10))
		return;

	// The hull is pushed forward along the direction the model faces; the
	// result is the position the boat will be at next frame.
	JGeometry::TVec3<f32> pos = mPosition;
	MtxPtr modelMtx = getModel()->getAnmMtx(0);
	pos.x += mSpeed * modelMtx[0][2];
	pos.z += mSpeed * modelMtx[2][2];

	// A stream (river current) cube Mario is standing in pushes the boat along
	// the cube's own axis, scaled by the current's strength.
	int cube = gpCubeStream->getInCubeNo(SMS_GetMarioPos());
	if (cube != -1) {
		TCubeStreamInfo& info = (TCubeStreamInfo&)(*gpCubeStream->unk14)[cube];
		Mtx mtx;
		MsMtxSetXYZRPH(mtx, 0.0f, 0.0f, 0.0f, info.unk18.x, info.unk18.y,
		               info.unk18.z);
		f32 flow = 0.0f;
		flow += modelMtx[0][2] * mtx[0][2];
		flow += modelMtx[2][2] * mtx[2][2];
		mSpeed += 0.0001f * (info.unk40 * flow);
	}

	// Grounded out (or on illegal collision): the boat is finished.
	const TBGCheckData* ground;
	f32 groundY = gpMap->checkGroundIgnoreWaterSurface(pos, &ground);
	if (groundY > mPosition.y - mYOffset - 100.0f || ground->isIllegalData()) {
		stopBoat(this, mPosition);
		return;
	}

	// Otherwise look for a wall to push off, in front of, behind and under
	// the hull; the first one that hits sends the boat to that spot. `center`
	// is a named local that all three probes reuse, so it gets one slot.
	JGeometry::TVec3<f32> center;
	center.set(pos.x + modelMtx[0][2] * mWallDepthC, mPosition.y - mYOffset,
	           pos.z + modelMtx[2][2] * mWallDepthC);
	if (stopAtWall(this,
	               TBGWallCheckRecord(center, mWallDepthA, 4,
	                                  TBGWallCheckRecord::DONT_MOVE_XZ)))
		return;
	center.set(pos.x - modelMtx[0][2] * mWallWidth, mPosition.y - mYOffset,
	           pos.z - modelMtx[2][2] * mWallWidth);
	if (stopAtWall(this,
	               TBGWallCheckRecord(center, mWallDepthB, 4,
	                                  TBGWallCheckRecord::DONT_MOVE_XZ)))
		return;
	center.set(pos);
	if (stopAtWall(this,
	               TBGWallCheckRecord(center, mWallHeight, 4,
	                                  TBGWallCheckRecord::DONT_MOVE_XZ)))
		return;

	mLinearVelocity = pos - mPosition;
}

void TMuddyBoat::control()
{
	TMapObjBase::control();
	if (marioIsOn())
		moveByWater();
	switch (mState) {
	case 1:
		// drifting: spin down, then bleed off the accumulated angle
		mSpeed *= mSpeedFriction;
		SMSGetMSound()->startSoundActorWithInfo(0x3080, &mPosition, nullptr,
		                                        fabsf(mSpeed), 0, 0, nullptr,
		                                        0, 4);
		if (mWaterLeakValue == 0.0f)
			break;
		mRotation.y += mWaterLeakValue;
		while (mRotation.y >= 360.0f)
			mRotation.y -= 360.0f;
		while (mRotation.y < 0.0f)
			mRotation.y += 360.0f;
		mWaterLeakValue *= mWaterLeakMul;
		if (fabsf(mWaterLeakValue) < 0.0001f)
			mWaterLeakValue = 0.0f;
		break;
	case 2:
		// the sinking animation ran out: start the disappear timer
		if (animIsFinished()) {
			mStateTimer = mAppearTime;
			onMapObjFlag(MAP_OBJ_FLAG_UNK100);
			sleep();
			mState = 3;
		}
		break;
	case 3:
		// waiting for the timer, then splash and go back to drifting
		if (isStateTimerEngaged())
			break;
		awake();
		makeObjDead();
		makeObjDefault();
		makeObjAppeared();
		JGeometry::TVec3<f32> scale(2.0f * mScaling.x, 2.0f * mScaling.y,
		                            3.0f * mScaling.z);
		mTargetPos.set(mPosition.x, mPosition.y - mYOffset, mPosition.z);
		emitAndSRT(0xE4, 0, &mTargetPos, mRotation, scale);
		emitAndSRT(0xE6, 0, &mTargetPos, mRotation, scale);
		SMSGetMSound()->startSoundActor(0x387D, &mPosition, 0, nullptr, 0, 4);
		mState = 1;
		break;
	}
}

void TMuddyBoat::calc()
{
	// `getWaveHeight` takes TWO floats, not three: the mangled
	// `getWaveHeight__11TMapObjWaveCFff` is const + 2 f32, where the leading
	// `1` of `11TMapObjWave` is the class-name length prefix. The ROM's
	// prologue passes mPosition.x as f1 and mPosition.z as f2, computes
	// `mPosition.y - mYOffset` separately, and adds it to the RESULT:
	//     lfs f28, 0x18(r30)   ; mPosition.z
	//     lfs f3,  0x14(r30)   ; mPosition.y
	//     lfs f0,  0x108(r30)  ; mYOffset
	//     fmr f2, f28          ; arg2
	//     fsubs f29, f3, f0    ; mPosition.y - mYOffset
	//     lfs f1, 0x10(r30)    ; arg1
	//     bl getWaveHeight
	//     fadds f30, f29, f1   ; y term added to the result
	f32 px = mPosition.x;
	f32 pz = mPosition.z;
	f32 wy = mPosition.y - mYOffset;
	f32 wave = gpMapObjWave->getWaveHeight(px, pz);
	f32 y    = wy + wave;

	MsMtxSetXYZRPH(getModel()->getAnmMtx(0), mPosition.x, y, mPosition.z,
	               mRotation.y, 0.0f, 0.0f);

	// 12 stores forming an identity matrix with a 1/0/0/0 diagonal
	Mtx mtx;
	mtx[0][0] = 1.0f;
	mtx[0][1] = 0.0f;
	mtx[0][2] = 0.0f;
	mtx[0][3] = 0.0f;
	mtx[1][0] = 0.0f;
	mtx[1][1] = 1.0f;
	mtx[1][2] = 0.0f;
	mtx[1][3] = 0.0f;
	mtx[2][0] = 0.0f;
	mtx[2][1] = 0.0f;
	mtx[2][2] = 1.0f;
	mtx[2][3] = 0.0f;
	PSMTXScale(mtx, mInitialScaling.x, mInitialScaling.y, mInitialScaling.z);

	// three genuinely separate getModel() calls - MWCC did not CSE them
	PSMTXConcat(getModel()->getAnmMtx(0), (MtxPtr)mtx, getModel()->getAnmMtx(0));

	if (mSpeed == 0.0f)
		return;

	mTargetPos.set(mPosition.x, mPosition.y - mYOffset, mPosition.z);
	JGeometry::TVec3<f32> scale(3.0f * mScaling.x, 2.0f * mScaling.y,
	                             3.0f * mScaling.z);
	emitAndBindScale(0x1E8, 3, &mTargetPos, scale);
	emitAndBindScale(0x107, 1, &mTargetPos, scale);
	mCount = 0;
}

u32 TMuddyBoat::getSDLModelFlag() const
{
	// TODO: unconfirmed value
	return 0;
}

void TMuddyBoat::initMapObj()
{
	// Frame-padding: target frame is 0x28, ours 0x20. Everything else matches.
	// TODO: identify the dead 8-byte local the original had here.
	char framePad_8_initMapObj[8];
	(void)framePad_8_initMapObj;
	TMapObjBase::initMapObj();
	mAccelPos      = 0.04f;
	mSpeedFriction = 0.998f;
	mWaterLeakRate = 0.002f;
	mWaterLeakMul  = 0.997f;
	mAccelNeg      = 0.01f;
	mAppearTime    = 600;
	// TODO: 0x34 is not a real map number in the game's map enum yet; the
	// target tests gpMarDirector->mMap against it directly.
	if (SMSGetMarDirector()->getCurrentMap() == 0x34) {
		mWallDepthA = 126.0f;
		mWallHeight = 185.0f;
		mWallDepthB = 150.0f;
		mWallDepthC = 170.0f;
		mWallWidth  = 185.0f;
	} else {
		mWallDepthA = 100.0f;
		mWallHeight = 170.0f;
		mWallDepthB = 150.0f;
		mWallDepthC = 180.0f;
		mWallWidth  = 100.0f;
	}
	mScale.set(3.0f, 2.0f, 5.0f);
}

TMuddyBoat::TMuddyBoat(const char* name)
	: TMapObjBase(name)
	, mAccelPos(0.0f)
	, mAccelNeg(0.0f)
	, mSpeed(0.0f)
	, mSpeedFriction(0.0f)
	, mWaterLeakRate(0.0f)
	, mWaterLeakValue(0.0f)
	, mWaterLeakMul(0.0f)
	, mWallHeight(0.0f)
	, mWallDepthA(0.0f)
	, mWallDepthB(0.0f)
	, mWallDepthC(0.0f)
	, mWallWidth(0.0f)
	, mAppearTime(0)
	, mCount(0)
	, mTargetPos()
	, mScale()
{
	// see TCogwheel's ctor: TVec3's default ctor does not zero, and the
	// target's store order (z, y, x) is TVec3::zero()'s chain assignment.
	mTargetPos.zero();
	mScale.zero();
}

// ===========================================================================
// TMareFall
// ===========================================================================

// The waterfall's upper lip; the map's sinit writes 2827.0f / 8604.0f /
// 7202.0f into it, so it is a namespace-scope TVec3 with a real
// initialiser (hence the `init` guard-free direct stores in __sinit).
static JGeometry::TVec3<f32> fall_upper_pos(2827.0f, 8604.0f, 7202.0f);

void TMareFall::calc()
{
	// Frame-padding: the target's frame is 0x28 and ours 0x20. Every
	// instruction matches, so the original must have declared a dead 8-byte
	// local here that MWCC still reserved a stack slot for. TODO: identify
	// it (see docs/AGENT_MATCHING_TIPS.md -- fakematch).
	char framePad_8_calc[8];
	(void)framePad_8_calc;
	SMSGetMSound()->startSoundActor(MSD_SE_GE_FALL, &mPosition, 0, nullptr,
	                                0, 4);
	SMSGetMSound()->startSoundActor(MSD_SE_GE_FALL_UPPER, &fall_upper_pos, 0,
	                                nullptr, 0, 4);
	// TODO: exact shape of the two emit() calls is right, but the frame is
	// still 8 bytes short of the target's 0x28 -- something in the original
	// left a dead 8-byte stack object here that has not been identified.
	const void* arg = this;
	gpMarioParticleManager->emit(0x149, &mPosition, 1, arg);
	gpMarioParticleManager->emit(0x14A, &mPosition, 1, arg);
}

void TMareFall::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	SMS_LoadParticle("/scene/mapObj/mareFallSplash.jpa", 0x149);
	SMS_LoadParticle("/scene/mapObj/mareFallSmoke.jpa", 0x14A);
}

// ===========================================================================
// TMareCork
// ===========================================================================

void TMareCork::loadAfter()
{
	mCannon = static_cast<TCannon*>(JDrama::TNameRefGen::search("砲台"));
	// message 4 asks the cannon to release its cork
	if (mCannon->receiveMessage(this, 4)) {
		mHeldObject = mCannon;
	}
	SMS_LoadParticle("/scene/map/map/ms_mare_gunwat_a.jpa", 0x14C);
	SMS_LoadParticle("/scene/map/map/ms_mare_gunwat_b.jpa", 0x14D);
	SMS_LoadParticle("/scene/map/map/ms_mare_gunwat_c.jpa", 0x14E);
	TMapObjBase::loadAfter();
	mVel.setAll(0.0f);
	initAnmSound();
}

void TMareCork::moveObject()
{
	if (mCannon->isObject() && !mIsMoving) {
		mMActor->setBck("marecork");
		setAnmSound("/scene/mapObj/marecork.bas");
		removeMapCollision();
		mIsMoving = 1;
	}
}

void TMareCork::calcRootMatrix()
{
	// Frame-padding: target frame is 0x30, ours 0x18. Every instruction and
	// every stack offset matches modulo the dead padding. TODO: identify the
	// local the original had here.
	char framePad_24_calcRootMatrix[24];
	(void)framePad_24_calcRootMatrix;
	if (mIsMoving) {
		// both wire bells count as collected once the animation has run this
		// far; only the second call's result is branched on.
		mMActor->getFrameCtrl(0)->checkPass(350.0f);
		if (mMActor->getFrameCtrl(0)->checkPass(250.0f)) {
			mCannon->startChorobeiShout();
			gpItemManager->makeShineAppearWithDemo("シャイン（ボス用）",
			                                      "ボスシャインカメラ",
			                                      mPosition.x, mPosition.y,
			                                      mPosition.z);
			mShinePos.set(2773.0f, 8618.0f, 7006.0f);
			JPABaseEmitter* emitter = gpMarioParticleManager->emitWithRotate(
				0x44, &mShinePos, 0x4000, 0xd82, 0, 0, nullptr);
			if (emitter) {
				emitter->mGlobalDynamicsScale.setAll(2.5f);
				emitter->mGlobalParticleScale.setAll(2.5f);
			}
		}
	}
	TMapObjBase::calcRootMatrix();
}

MtxPtr TMareCork::getTakingMtx()
{
	// mNodeMatrices[2] corresponds to the cork joint
	return mMActor->getModel()->getAnmMtx(2);
}

void TMareCork::drawObject(JDrama::TGraphics* graphics)
{
	TLiveActor::drawObject(graphics);
	if (mIsMoving && mMActor->getFrameCtrl(0)->getFrame() > 250.0f) {
		mShinePos.set(2773.0f, 8618.0f, 7006.0f);
		SMSGetMSound()->startSoundActor(MSD_SE_ENV_FALL_JET_LEVEL,
		                                &mShinePos, 0, nullptr, 0, 4);
		gpMarioParticleManager->emitAndBindToPosPtr(0x14C, &mVel, 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(0x14D, &mVel, 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(0x14E, &mVel, 1, this);
	}
}

// ===========================================================================
// TMareEventPoint
// ===========================================================================

BOOL TMareEventPoint::receiveMessage(THitActor* sender, u32 message)
{
	// TODO: the exact meaning of the 0x1000 particle flag and the
	// 0.1f normal-Z threshold have not been confirmed.
	// Frame-padding: the target's frame is 0x30 and ours 0x28; every
	// instruction and stack offset matches modulo this dead 8 bytes.
	// TODO: identify the local the original really had here.
	char framePad_8_receiveMessage[8];
	(void)framePad_8_receiveMessage;
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		int water_id = TMapObjBase::getWaterID(sender);
		// The target materialises this comparison into a bool first (hence
		// the `li r0,1 / b / li r0,0` pair) and then branches AWAY when the
		// flag *is* 1 -- i.e. the body below is the `flag != 1` path.
		// checkFlagBottom4Bits' `? true : false` tail is what produces the
		// materialisation.
		if (!gpModelWaterManager->checkFlagBottom4Bits(water_id, 1)) {
			const TBGCheckData* plane = TMapObjBase::getWaterPlane(sender);
			if (plane != nullptr) {
				if (TMapObjBase::getWaterPlane(sender)->mNormal.y < 0.1f) {
					TMareEventDepressWall* wall
					    = (TMareEventDepressWall*)mMareEventDepressWall;
					if (wall->startEvent()) {
						gpMarioParticleManager->emit(0xE7, &sender->mPosition,
						                             0, nullptr);
						SMSGetMSound()->startSoundSet(
						    MSD_SE_EN_COMMON_W_HIT_OK, &mPosition, 0,
						    0.0f, 0, 0, 4);
					}
					// the ROM's `return TRUE` is outside the startEvent()
					// test: its `beq` lands on the `li r3,1` directly.
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

void TMareEventPoint::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);
	initHitActor(0x40000236, 0, 0, 0.0f, 0.0f, 300.0f, 600.0f);
}
