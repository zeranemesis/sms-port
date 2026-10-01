#include <MoveBG/ModelGate.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <M3DUtil/InfectiousStrings.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <M3DUtil/MActorUtil.hpp>
#include <M3DUtil/SampleCtrlModel.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/MtxUtil.hpp>
#include <Player/MarioAccess.hpp>
#include <System/FlagManager.hpp>
#include <System/Particles.hpp>
#include <System/EmitterViewObj.hpp>
#include <MSound/MSound.hpp>
#include <THPPlayer/THPPlayer.h>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTexture.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphBase/Components/J3DTevStage.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JDrama/JDRViewObjPtrList.hpp>
#include <M3DUtil/SampleCtrlNode.hpp>
#include <Camera/Camera.hpp>
#include <MarioUtil/ScreenUtil.hpp>
#include <stdio.h>
#include <dolphin/mtx.h>
#include <stdlib.h>
#include <math.h>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

static const char* gateMActorNames[5]
    = { "05_gate01", "05_gate02rico", "05_gate03manma", "05_gate04monte",
	    "05_gate05mare" };

void TModelGate::loadAfter()
{
	static const char* gateNames[] = { "Gate", "GateToRicco", "GateToMamma",
	                                 "GateToMonte", "GateToMare", nullptr };
	initHitActor(0x080000C0, 5, -0x80000000, 300.0f, 400.0f, 300.0f, 400.0f);
	mHitFlags |= HIT_FLAG_NO_COLLISION;
	unk70 = 0;
	unk71 = 0;

	for (u8 i = 0; i < 5; ++i) {
		if (strcmp(gateNames[i], getName()) == 0) {
			unk71 = i;
			break;
		}
	}

	u32 loaderFlags = 0x11100000;
	char modelPath[0x100];
	snprintf(modelPath, sizeof(modelPath), "/scene/map/map/gate/%s.bmd",
	         gateMActorNames[unk71]);
	unk78 = SMS_MakeMActor("/scene/map/map/gate", modelPath, 0,
	                       loaderFlags);
	unk72 = unk78->getModel()->getModelData()->getJointName()->getIndex(
	    "center");

	if (ActivePlayer.open) {
		THPVideoInfo videoInfo;
		THPPlayerGetVideoInfo(&videoInfo);
		u32 width = videoInfo.xSize;
		u32 height = videoInfo.ySize;
		J3DTexture* texture = unk78->getModel()->getModelData()->getTexture();
		ResTIMG* image = texture->getResTIMG(0);
		image->format = 1;
		image->width = width;
		image->height = height;
		for (u8 i = 1; i < 3; ++i) {
			image = texture->getResTIMG(i);
			image->format = 1;
			image->width = (width >> 1) & 0x7fff;
			image->height = (height >> 1) & 0x7fff;
		}
	}

	unkB8 = 0;
	unkB9 = 0;
	unkBA = 0;
	unkBE = 0xF0;
	unkBC = unkBE;
	unk78->setBtk(gateMActorNames[unk71]);
	unk78->setBrk(gateMActorNames[unk71]);
	unk78->getFrameCtrl(5)->setRate(0.0f);
	unkC0 = new SampleCtrlModelData(unk78->getModel()->getModelData());
	unkC5[0] = 0x20;
	unkC5[1] = 0xFF;
	unkC4 = 1;
	unkC8 = 360;
	unkCA = 0;
	unkCC = 0;
	unkCE = 60;
	unkD0 = 0.0f;
	unkD4 = 0.1f;
	unkD8 = 0.02f;
	unkDC = 0.025f;
	mScaling.set(1.0f, 1.0f, 1.0f);

	static_cast<JDrama::TViewObjPtrListT<THitActor>*>(
	    JDrama::TNameRefGen::search("マップグループ"))->push_back(this);

	Mtx actorMtx;
	SMS_GetActorMtx(*this, actorMtx);
	PSMTXCopy(actorMtx, unk78->getModel()->getBaseTRMtx());
	unk78->getModel()->calc();

	PSMTXTrans(actorMtx, 0.0f, 0.0f, 250.0f);
	PSMTXConcat(unk78->getModel()->getAnmMtx(unk72), actorMtx, actorMtx);
	unkAC.set(0.0f, 0.0f, 0.0f);
	PSMTXMultVec(actorMtx, (Vec*)&unkAC, (Vec*)&unkAC);
	PSMTXInverse(unk78->getModel()->getAnmMtx(unk72), unk7C);
	MtxPtr centerMtx = unk78->getModel()->getAnmMtx(unk72);
	unk74 = matan(centerMtx[2][2], centerMtx[0][2]);

	unkE0 = 0;
	unkE4 = 0.0f;
	unkE8 = 0.01f;
	unkEC = 0.02f;
	unkF0 = 500.0f;
	unkF4 = 1000.0f;
	unkF8 = 0.7f;
	unkFC = 0.0f;
	unk100 = 200.0f;
	unk104 = 150.0f;
	unk108 = -1000.0f;
	unk10C = 150.0f;
	unk110 = 0.0f;
	unk114 = 300.0f;

	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_body.jpa", 0x1A);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_head.jpa", 0x1B);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_cap.jpa", 0x1C);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_rhand.jpa", 0x1D);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_lhand.jpa", 0x1E);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_rleg.jpa", 0x1F);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_rfoot.jpa", 0x20);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_lleg.jpa", 0x21);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_lfoot.jpa", 0x22);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_watgun.jpa", 0x23);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_dust.jpa", 0x3C);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_senko.jpa", 0x51);
	SMS_LoadParticle("/scene/map/map/gate/ms_gatewind_a.jpa", 0x131);
	SMS_LoadParticle("/scene/map/map/gate/ms_gatewind_a2.jpa", 0x132);
	SMS_LoadParticle("/scene/map/map/gate/ms_gatewind_a3.jpa", 0x133);
	SMS_LoadParticle("/scene/map/map/gate/ms_gatewind_b.jpa", 0x134);
	SMS_LoadParticle("/scene/map/map/gate/ms_gatehit_a.jpa", 0x1DD);
	SMS_LoadParticle("/scene/map/map/gate/ms_gatehit_b.jpa", 0x1DE);

	bool opened = false;
	switch (unk71) {
	case 0: opened = TFlagManager::getInstance()->getBool(0x10385); break;
	case 1: opened = TFlagManager::getInstance()->getBool(0x10386); break;
	case 2: opened = TFlagManager::getInstance()->getBool(0x10387); break;
	case 3: opened = TFlagManager::getInstance()->getBool(0x10387); break;
	case 4: opened = TFlagManager::getInstance()->getBool(0x10387); break;
	}
	if (opened == true) {
		unk70 |= 1;
		mHitFlags &= ~HIT_FLAG_NO_COLLISION;
	} else {
		unk70 &= ~1;
		unk70 |= 2;
		mHitFlags |= HIT_FLAG_NO_COLLISION;
	}
}

void TModelGate::startOpen()
{
	unk70 |= 1;
	unkC4 = 0;
	unk78->setBpk(gateMActorNames[unk71]);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	unk70 |= 2;
}

void TModelGate::screenBlur(JDrama::TGraphics* graphics)
{
	Vec screenDirection;
	JGeometry::TVec3<f32> direction;
	direction.x = gpMarioPos->x - mPosition.x;
	direction.y = 0.0f;
	direction.z = gpMarioPos->z - mPosition.z;
	PSVECNormalize((Vec*)&direction, (Vec*)&direction);

	PSMTXMultVecSR(graphics->getViewMtx(), (Vec*)&direction,
	                &screenDirection);

	JGeometry::TVec3<f32> localMario;
	JGeometry::TVec3<f32> marioPosition = *gpMarioPos;
	PSMTXMultVec(unk7C, (Vec*)&marioPosition, (Vec*)&localMario);

	f32 distanceStrength = 0.0f;
	if (-250.0f <= localMario.x && localMario.x <= 250.0f
	    && -250.0f <= localMario.y && localMario.y <= 250.0f
	    && 0.0f <= localMario.z && localMario.z <= unkF4) {
		if (localMario.z < unkF0) {
			distanceStrength = 1.0f;
		}
		if (unkF0 <= localMario.z && localMario.z <= unkF4) {
			distanceStrength
			    = 1.0f - (localMario.z - unkF0) / (unkF4 - unkF0);
		}
		if (unkF4 < localMario.z)
			distanceStrength = 0.0f;
	}

	f32 targetStrength = (f32)unkE0 * distanceStrength;
	const s16 gateCameraAngle = (s16)(s32)(
	    182.04445f * mRotation.y - (f32)gpCamera->unk258);
	if (gateCameraAngle < -10922 || gateCameraAngle > 10922)
		targetStrength = 0.0f;

	unkE4 += unkE8 * (targetStrength - unkE4);
	f32 blurAlpha = unkE4;
	s32 alpha = (s32)(blurAlpha * (1.0f - gpCamera->unk270));
	f32 blurScale = unkEC;
	gpAfterEffect->setGateBlur(2, alpha, blurScale, JGeometry::TVec3<f32>(screenDirection.x, screenDirection.y, screenDirection.z));
}

BOOL TModelGate::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->getActorType() == 0x80000001) {
		if (message == HIT_MESSAGE_ATTACK) {
			unkC8 = 0;
			unkC4 = 2;
			return TRUE;
		}
	}

	if (sender->getActorType() == 0x1000001) {
		JGeometry::TVec3<f32> localPos;
		Mtx localMtx;
		PSMTXMultVec(unk7C, (Vec*)&sender->mPosition, (Vec*)&localPos);
		MtxPtr anmMtx = unk78->getModel()->getAnmMtx(unk72);
		PSMTXCopy(anmMtx, localMtx);

		if (localPos.x * localPos.x + localPos.y * localPos.y < 40000.0f) {
			if (-100.0f < localPos.z) {
				if (localPos.z < unkFC) {
					if (unk70 & 2) {
						unkD0 += unkD4;
						if (unkD0 > 1.0f) {
							unkCA = unkC8;
							unkD0 = 1.0f;
							unk70 &= ~2;
						}
					}

					f32 rnd = 0.000030517578f * (f32)rand();
					if (rnd < unkF8) {
						gpMarioParticleManager->emitWithRotate(
						    0x1DD, &sender->mPosition, 0, unk74, 0, 2,
						    nullptr);
						gpMarioParticleManager->emitWithRotate(
						    0x1DE, &sender->mPosition, 0, unk74, 0, 2,
						    nullptr);
					}

					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

void TModelGate::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (!(unk70 & 1))
		return;

	if ((cue & 8) && ActivePlayer.open && ActivePlayer.dispTextureSet != nullptr) {
		THPTextureSet* textureSet = ActivePlayer.dispTextureSet;
		J3DTexture* texture = unk78->getModel()->getModelData()->getTexture();
		ResTIMG* image = texture->getResTIMG(0);
		image->imageDataOffset = (u32)textureSet->ytexture - (u32)image;
		image = texture->getResTIMG(1);
		image->imageDataOffset = (u32)textureSet->utexture - (u32)image;
		image = texture->getResTIMG(2);
		image->imageDataOffset = (u32)textureSet->vtexture - (u32)image;
	}

	unk78->perform(cue, graphics);

	if (cue & 1) {
		if (!(unk70 & 2)) {
			if (SMS_DistanceFromMario(mPosition) < 1000.0f) {
				unkD0 += 0.01f;
				if (unkD0 > 1.0f) {
					unkD0 = 1.0f;
					unkCA = unkC8;
				}
			} else {
				--unkCA;
				if (unkCA <= 0)
					unkCA = 0;
				unkD0 = (f32)unkCA / 1000.0f;
			}
		}

		JGeometry::TVec3<f32> localMario;
		PSMTXMultVec(unk7C, (Vec*)gpMarioPos, (Vec*)&localMario);
		if (-unk104 < localMario.x && localMario.x < unk104
		    && unk108 < localMario.y && localMario.y < unk10C
		    && unk110 < localMario.z && localMario.z < unk114) {
			if (unkCA > 0) {
				bool jumping = false;
				if (SMS_IsMarioStatusTypeJumping())
					jumping = true;
				if (jumping == true) {
					if (SMS_GetMarioHitActor()->receiveMessage(this, 4) == 1)
						mHeldObject = (TTakeActor*)SMS_GetMarioHitActor();
				}
			} else {
				f32 dx = gpMarioPos->x - mPosition.x;
				f32 dz = gpMarioPos->z - mPosition.z;
				f32 distance = std::sqrtf(dx * dx + dz * dz);
				if (distance < unk100) {
					JGeometry::TVec3<f32> moveRequest = *gpMarioPos;
					moveRequest.x += 10.0f * (dx / distance);
					moveRequest.z += 10.0f * (dz / distance);
					SMS_MarioMoveRequest(moveRequest);
				}
			}
		}
	}

	if (cue & 2) {
		switch (unkC4) {
		case 0:
			if (unk78->getFrameCtrl(2)->checkState(3))
				unkC4 = 1;
			break;
		case 1:
			if (unkCA > 0) {
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0x131, unk78->getModel()->getAnmMtx(unk72), 1, this);
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0x132, unk78->getModel()->getAnmMtx(unk72), 1, this);
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0x133, unk78->getModel()->getAnmMtx(unk72), 1, this);
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0x134, unk78->getModel()->getAnmMtx(unk72), 1, this);

				--unkCA;
				++unkCC;
				gpMSound->startSoundActor(0x3076, &mPosition, 0, nullptr, 0, 4);
				gpMSound->startSoundActor(0x3077, &mPosition, 0, nullptr, 0, 4);
			} else {
				unkCA = 0;
				unkCC = 0;
				unkD0 -= unkD8;
				if (unkD0 < 0.0f)
					unkD0 = 0.0f;
			}
			break;
		default:
			unkD0 -= unkDC;
		}

		if (unkD0 > 1.0f)
			unkD0 = 1.0f;
		if (unkD0 < 0.0f)
			unkD0 = 0.0f;

		J3DTevBlock* tevBlock = unk78->getModel()->getModelData()
		                           ->getMaterialNodePointer(0)->getTevBlock();
		if (unkB8 == 1) {
			unkBA = unkB9;
			--unkBC;
			if (unkBC == 0) {
				++unkB9;
				unkBC = unkBE;
				if (unkB9 >= 8)
					unkB9 = 0;
			}
			SampleCtrlMaterial* sampledMaterial = unkC0->mMaterials[0];
			u8& color0 = sampledMaterial->unk3C[0].field_0x5;
			J3DTevStageInfo& stage2 = sampledMaterial->unk3C[2];
			u8& color2 = stage2.field_0x5;
			J3DTevStageInfo& stage3 = sampledMaterial->unk3C[3];
			u8& color3 = stage3.field_0x5;
			u8& bias3 = stage3.field_0x6;
			J3DTevStageInfo& stage5 = sampledMaterial->unk3C[5];
			u8& scale3 = stage3.field_0x7;
			u8& clamp3 = stage3.field_0x8;
			u8& clampAlpha5 = stage5.field_0x11;
			color0 = 0;
			color2 = 0;
			color3 = 0;
			bias3 = 0;
			scale3 = 0;
			clamp3 = 1;
			clampAlpha5 = 1;
			switch (unkB9) {
			case 1:
				color3 = 1;
				clamp3 = 0;
				break;
			case 2:
				color0 = 8;
				break;
			case 3:
				color3 = 8;
				break;
			case 4:
				scale3 = 1;
				break;
			case 5:
				clampAlpha5 = 0;
				break;
			case 6:
				color2 = 1;
				break;
			case 7:
				color3 = 0;
				bias3 = 1;
				clamp3 = 0;
				break;
			}

			J3DTevStageInfo& stage0 = sampledMaterial->unk3C[0];
			tevBlock->getTevStage(0)->setTevStageInfo(stage0);
			tevBlock->getTevStage(2)->setTevStageInfo(stage2);
			tevBlock->getTevStage(3)->setTevStageInfo(stage3);
			tevBlock->getTevStage(5)->setTevStageInfo(stage5);
		}

		f32 frame = unkD0 * unk78->getFrameCtrl(5)->getEnd();
		unk78->getFrameCtrl(5)->setFrame(frame);
	}
	if (cue & 4)
		screenBlur(graphics);

	THitActor::perform(cue, graphics);
}
