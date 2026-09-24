#include <MoveBG/ModelGate.hpp>
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
#include <M3DUtil/SampleCtrlNode.hpp>
#include <Camera/Camera.hpp>
#include <MarioUtil/ScreenUtil.hpp>
#include <stdio.h>
#include <dolphin/mtx.h>
#include <stdlib.h>

static const char* gateMActorNames[5]
    = { "05_gate01", "05_gate02rico", "05_gate03manma", "05_gate04monte",
	    "05_gate05mare" };
static const char* gateDestinationNames[5]
    = { "Gate", "GateToRicco", "GateToMamma", "GateToMonte", "GateToMare" };

void TModelGate::loadAfter()
{
	initHitActor(0x080000C0, 5, -0x80000000, 300.0f, 400.0f, 300.0f, 400.0f);
	mHitFlags |= HIT_FLAG_NO_COLLISION;
	unk70 = 0;
	unk71 = 0;

	for (u8 i = 0; i < 5; ++i) {
		if (strcmp(getName(), gateDestinationNames[i]) == 0) {
			unk71 = i;
			break;
		}
	}

	char modelPath[0x100];
	snprintf(modelPath, sizeof(modelPath), "/scene/map/map/gate/%s.bmd",
	         gateMActorNames[unk71]);
	unk78 = SMS_MakeMActor("/scene/map/map/gate", modelPath, 0,
	                       0x11100000);
	unk72 = unk78->getModel()->getModelData()->getJointName()->getIndex(
	    "center");

	if (ActivePlayer.open) {
		THPVideoInfo videoInfo;
		THPPlayerGetVideoInfo(&videoInfo);
		ResTIMG* images = unk78->getModel()->getModelData()->getTexture()->getResTIMG(0);
		images[0].format = 1;
		images[0].width = videoInfo.xSize;
		images[0].height = videoInfo.ySize;
		for (u8 i = 1; i < 3; ++i) {
			images[i].format = 1;
			images[i].width = (videoInfo.xSize >> 1) & 0x7fff;
			images[i].height = (videoInfo.ySize >> 1) & 0x7fff;
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
	unkC4 = 1;
	unkC5[0] = 0x20;
	unkC5[1] = 0xFF;
	unkC8 = 360;
	unkCA = 0;
	unkCC = 0;
	unkCE = 60;
	unkD0 = 0.0f;
	unkD4 = 0.1f;
	unkD8 = 0.02f;
	unkDC = 0.025f;
	mScaling.set(1.0f, 1.0f, 1.0f);

	TMapObjBase::joinToGroup("マップグループ", this);

	Mtx actorMtx;
	SMS_GetActorMtx(*this, actorMtx);
	PSMTXCopy(actorMtx, unk78->getModel()->getBaseTRMtx());
	unk78->getModel()->update();

	Mtx localMtx;
	PSMTXTrans(localMtx, 0.0f, 0.0f, 250.0f);
	MtxPtr centerMtx = unk78->getModel()->getAnmMtx(unk72);
	PSMTXConcat(centerMtx, localMtx, localMtx);
	unkAC.set(0.0f, 0.0f, 0.0f);
	PSMTXMultVec(localMtx, (Vec*)&unkAC, (Vec*)&unkAC);
	PSMTXInverse(centerMtx, unk7C);
	unk74 = matan(centerMtx[2][2], centerMtx[0][2]);

	unkE0 = 0;
	unkE4 = 0.0f;
	unkE8 = 0.01f;
	unkEC = 0.025f;
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

	static const char* particlePaths[] = {
	    "/scene/map/map/gate/ms_mariowp_body.jpa",
	    "/scene/map/map/gate/ms_mariowp_head.jpa",
	    "/scene/map/map/gate/ms_mariowp_cap.jpa",
	    "/scene/map/map/gate/ms_mariowp_rhand.jpa",
	    "/scene/map/map/gate/ms_mariowp_lhand.jpa",
	    "/scene/map/map/gate/ms_mariowp_rleg.jpa",
	    "/scene/map/map/gate/ms_mariowp_rfoot.jpa",
	    "/scene/map/map/gate/ms_mariowp_lleg.jpa",
	    "/scene/map/map/gate/ms_mariowp_lfoot.jpa",
	    "/scene/map/map/gate/ms_mariowp_watgun.jpa",
	    "/scene/map/map/gate/ms_mariowp_dust.jpa",
	    "/scene/map/map/gate/ms_mariowp_senko.jpa",
	    "/scene/map/map/gate/ms_gatewind_a.jpa",
	    "/scene/map/map/gate/ms_gatewind_a2.jpa",
	    "/scene/map/map/gate/ms_gatewind_a3.jpa",
	    "/scene/map/map/gate/ms_gatewind_b.jpa",
	    "/scene/map/map/gate/ms_gatehit_a.jpa",
	    "/scene/map/map/gate/ms_gatehit_b.jpa",
	};
	static const u16 particleIds[] = {
	    0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22,
	    0x23, 0x3C, 0x51, 0x131, 0x132, 0x133, 0x134, 0x1DD, 0x1DE,
	};
	for (u32 i = 0; i < sizeof(particleIds) / sizeof(particleIds[0]); ++i)
		SMS_LoadParticle(particlePaths[i], particleIds[i]);

	bool opened = false;
	switch (unk71) {
	case 0: opened = TFlagManager::getInstance()->getBool(0x10385); break;
	case 1: opened = TFlagManager::getInstance()->getBool(0x10386); break;
	case 2:
	case 3:
	case 4: opened = TFlagManager::getInstance()->getBool(0x10387); break;
	}
	if (opened) {
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
	JGeometry::TVec3<f32> direction(gpMarioPos->x - mPosition.x, 0.0f,
	                                gpMarioPos->z - mPosition.z);
	PSVECNormalize((Vec*)&direction, (Vec*)&direction);

	Vec screenDirection;
	PSMTXMultVecSR(graphics->getViewMtx(), (Vec*)&direction,
	                &screenDirection);

	JGeometry::TVec3<f32> localMario;
	PSMTXMultVec(unk7C, (Vec*)gpMarioPos, (Vec*)&localMario);

	f32 distanceStrength = 0.0f;
	if (-250.0f <= localMario.x && localMario.x <= 250.0f
	    && -250.0f <= localMario.y && localMario.y <= 250.0f
	    && 0.0f <= localMario.z && localMario.z <= unkF4) {
		if (localMario.z < unkF0) {
			distanceStrength = 1.0f;
		} else if (localMario.z <= unkF4) {
			distanceStrength
			    = 1.0f - (localMario.z - unkF0) / (unkF4 - unkF0);
		}
	}

	const s16 gateCameraAngle = (s16)(s32)(
	    182.04445f * mRotation.y - (f32)gpCamera->unk258);
	f32 targetStrength = (f32)unkE0 * distanceStrength;
	if (gateCameraAngle < -10922 || gateCameraAngle > 10922)
		targetStrength = 0.0f;

	unkE4 += unkE8 * (targetStrength - unkE4);
	gpAfterEffect->unk15 = 2;
	gpAfterEffect->unk1C
	    = (u8)((s32)((1.0f - gpCamera->unk270) * unkE4));
	gpAfterEffect->unk50 = unkEC;
	gpAfterEffect->unk5C = screenDirection.x;
	gpAfterEffect->unk60 = screenDirection.y;
	gpAfterEffect->unk64 = screenDirection.z;
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
		Mtx localMtx;
		JGeometry::TVec3<f32> localPos;
		PSMTXMultVec(unk7C, (Vec*)&sender->getPosition(), (Vec*)&localPos);
		PSMTXCopy(unk78->getModel()->getAnmMtx(unk72), localMtx);

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

					if (0.000030517578f * (f32)rand() < unkF8) {
						const JGeometry::TVec3<f32>* position = &sender->getPosition();
						gpMarioParticleManager->emitWithRotate(
						    0x1DD, position, 0, unk74, 0, 2,
						    nullptr);
						gpMarioParticleManager->emitWithRotate(
						    0x1DE, position, 0, unk74, 0, 2,
						    nullptr);
					}

					return TRUE;
				}
			}
		}

		return FALSE;
	}

	return FALSE;
}

void TModelGate::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (!(unk70 & 1))
		return;

	if ((cue & 8) && ActivePlayer.open && ActivePlayer.dispTextureSet != nullptr) {
		ResTIMG* images
		    = unk78->getModel()->getModelData()->getTexture()->getResTIMG(0);
		images[0].imageDataOffset
		    = (u32)ActivePlayer.dispTextureSet->ytexture - (u32)&images[0];
		images[1].imageDataOffset
		    = (u32)ActivePlayer.dispTextureSet->utexture - (u32)&images[1];
		images[2].imageDataOffset
		    = (u32)ActivePlayer.dispTextureSet->vtexture - (u32)&images[2];
	}

	unk78->perform(cue, graphics);

	if (cue & 1) {
		if (!(unk70 & 2)) {
			JGeometry::TVec3<f32> fromGate = *gpMarioPos;
			fromGate -= mPosition;
			if (fromGate.length() < 1000.0f) {
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
				if (SMS_IsMarioStatusTypeJumping()) {
					THitActor* marioActor = SMS_GetMarioHitActor();
					if (marioActor->receiveMessage(this, 4) == 1)
						mHeldObject = (TTakeActor*)marioActor;
				}
			} else {
				f32 dx = gpMarioPos->x - mPosition.x;
				f32 dz = gpMarioPos->z - mPosition.z;
				f32 distance = JGeometry::TUtil<f32>::sqrt(dx * dx + dz * dz);
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
		if (unkC4 == 0) {
			if (unk78->getFrameCtrl(2)->getState() & 3)
				unkC4 = 1;
		} else if (unkC4 == 1) {
			if (unkCA > 0) {
				MtxPtr jointMtx = unk78->getModel()->getAnmMtx(unk72);
				for (u16 particle = 0x131; particle <= 0x134; ++particle)
					gpMarioParticleManager->emitAndBindToMtxPtr(particle, jointMtx,
					                                            1, this);

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
		} else {
			unkD0 -= unkDC;
		}

		if (unkD0 > 1.0f)
			unkD0 = 1.0f;
		if (unkD0 < 0.0f)
			unkD0 = 0.0f;

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
			J3DMaterial* material
			    = unk78->getModel()->getModelData()->getMaterialNodePointer(0);
			J3DTevStageInfo* tevInfo = sampledMaterial->unk3C;
			tevInfo[0].field_0x5 = 0;
			tevInfo[2].field_0x5 = 0;
			tevInfo[3].field_0x5 = 0;
			tevInfo[3].field_0x6 = 0;
			tevInfo[3].field_0x8 = 1;
			tevInfo[5].field_0x11 = 1;
			if (unkB9 == 0) {
				tevInfo[2].field_0x5 = 1;
				tevInfo[3].field_0x6 = 0;
			} else if (unkB9 == 1) {
				tevInfo[0].field_0x5 = 8;
			} else if (unkB9 == 2) {
				tevInfo[2].field_0x5 = 8;
			} else if (unkB9 == 3) {
				tevInfo[3].field_0x7 = 1;
			} else if (unkB9 == 4) {
				tevInfo[5].field_0x11 = 0;
			} else if (unkB9 == 5) {
				tevInfo[2].field_0x7 = 1;
			} else if (unkB9 == 6) {
				tevInfo[2].field_0x5 = 1;
			} else if (unkB9 == 7) {
				tevInfo[2].field_0x5 = 0;
				tevInfo[3].field_0x6 = 1;
				tevInfo[3].field_0x8 = 0;
			}

#pragma dont_inline on
			const u8 stageIndices[4] = { 0, 2, 3, 5 };
			for (u32 i = 0; i < 4; ++i) {
				const J3DTevStageInfo& info = tevInfo[stageIndices[i]];
				J3DTevStage* stage = material->getTevStage(stageIndices[i]);
				stage->setTevColorOp(info.field_0x5, info.field_0x6,
				                     info.field_0x7, info.field_0x8,
				                     info.field_0x9);
				stage->setTevColorAB(info.field_0x1, info.field_0x2);
				stage->setTevColorCD(info.field_0x3, info.field_0x4);
				stage->setAlphaABCD(info.field_0xa, info.field_0xb,
				                    info.field_0xc, info.field_0xd);
				stage->setTevAlphaOp(info.field_0xe, info.field_0xf,
				                     info.field_0x10, info.field_0x11,
				                     info.field_0x12);
			}
#pragma dont_inline off
		}

		J3DFrameCtrl* btkCtrl = unk78->getFrameCtrl(5);
		btkCtrl->setRate(unkD0 * btkCtrl->getEnd());
	}
	if (cue & 4)
		screenBlur(graphics);

	THitActor::perform(cue, graphics);
}

MtxPtr TModelGate::getTakingMtx()
{
	return nullptr;
}
