#include <NPC/NpcParts.hpp>
#include <JSystem/J3D/J3DGraphBase/Components/J3DGXColorS10.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTexture.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DShape.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/SharedParts.hpp>
#include <MarioUtil/TexUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/MtxUtil.hpp>
#include <MarioUtil/LightUtil.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <M3DUtil/MActor.hpp>
#include <NPC/NpcColor.hpp>
#include <NPC/NpcInitData.hpp>
#include <NPC/NpcBase.hpp>
#include <NPC/NpcManager.hpp>

// rogue
#include <M3DUtil/InfectiousStrings.hpp>

// TODO: figure out the odr violations with this symbol
// Defined in both NpcAnm and NpcParts, as retail keeps the string in both; the
// linker keeps the first. A PC build must give it internal or weak linkage.
const char* cNpcPartsNameRootJoint = "__ROOT_JOINT__";
const char* cPeachPartsTextureName = "H_peach_main_dummy";
const char* cPeachHostTextureName  = "H_peach_main_s3tc";

void SetMActorAnmFrame(MActor* actor, f32 frame, bool set_bck, bool set_btp)
{
	if (actor == nullptr)
		return;

	J3DFrameCtrl* ctrl;

	if (set_bck) {
		ctrl = actor->getFrameCtrl(ANM_TYPE_BCK);
		if (ctrl)
			ctrl->setFrame(frame);
	}

	if (set_btp) {
		ctrl = actor->getFrameCtrl(ANM_TYPE_BTP);
		if (ctrl)
			ctrl->setFrame(frame);
	}
}

// Starts a simple motion blend on one part; -1 takes the NPCs' shared
// blend length. `frame` is `s32` like the TParamRT it defaults to: MWCC does
// not fold the resulting `frame == -1` test, which retail keeps at both
// defaulted sites and schedules the `20` ahead of the parts load.
static inline void NpcPartsInitMotionBlend(TNpcParts* parts, int i, int j,
                                           s32 frame = -1)
{
	if (frame == -1)
		frame = TBaseNPC::mPtrSaveNormal->mMotionBlendFrame.get();
	parts->unk0[j][i]->getMActor()->initSimpleMotionBlend(frame);
}

// TODO: 99.6%, frame 0x198 vs retail 0x1f8 (0x60 short) and an r26/r27 swap
// (the hoisted `&initInfo->unk4[i]` against the parts/model temporaries).
// An `int` frame, or a `TSharedParts*`/`MActor*` parameter in place of the
// indices, lets MWCC fold or reorder the blend sites (93.7-96.8%).
// Re-reading `unk8[j]` for getPartsSDLModelData instead of passing `puVar3`
// fixes the swap but drops retail's `mr r25` copy (98.6%); lever-search's
// accessor levers move the frame in 8s only (best 0x1b0) with the swap intact.
TNpcParts::TNpcParts(u32 param_1, const J3DGXColorS10* param_2,
                     TBaseNPC* param_3)
    : unk60(param_3)
{
	const TNpcInitInfo* initInfo
	    = SMSGetNpcInitData(unk60->getActorType() - 0x4000001);

	// The clearing loop is a flat 24-trip pointer walk with the increment in
	// the `for`'s third clause: that is the only spelling MWCC unrolls by
	// eight into a `bdnz` with ctr = 3 and no remainder path, as retail does.
	// `*slot++ = nullptr` as the body, `slot[i] = nullptr`, and a nested
	// 2x12 walk are all different (82.0%, 82.0%, 84.8%); a pointer-compare
	// `while` is 90.5%.
	TSharedParts** slot = unk0[0];
	for (int i = 0; i < 24; ++i, ++slot)
		*slot = nullptr;

	for (int i = 0; i < 12; ++i) {
		if (initInfo->unk4[i] == nullptr || !(param_1 & (1 << i)))
			continue;

		u32 param3 = (&param_2->color.r)[initInfo->unk4[i]->unk28];

		const GXColor* param4 = nullptr;
		if (initInfo->unk4[i]->unk2A)
			param4 = unk60->getPtrInitPollutionColor();

		for (int j = 0; j < 2; ++j) {
			if (j >= unk60->getManager()->unk28)
				break;

			const char* puVar3 = initInfo->unk4[i]->unk8[j];
			if (puVar3 == nullptr)
				continue;

			int iVar6;
			if (strcmp(initInfo->unk4[i]->unk0[j], cNpcPartsNameRootJoint)
			    == 0) {
				iVar6 = -1;
			} else {
				iVar6 = unk60->mMActorKeeper->getMActor(j)
				            ->getModel()
				            ->getModelData()
				            ->getJointName()
				            ->getIndex(initInfo->unk4[i]->unk0[j]);
			}

			TNPCManager* manager    = (TNPCManager*)unk60->getManager();
			SDLModelData* modelData = manager->getPartsSDLModelData(puVar3);
			unk0[j][i] = new TSharedParts(unk60, iVar6, modelData, 3);
			if (initInfo->unk4[i]->unk2B)
				SMS_UnifyMaterial(unk0[j][i]->getMActor()->getModel());

			switch (unk60->getActorType()) {
			case 0x4000018:
				if (j != 0 || (i != 3 && i != 4)) {
					TSharedParts* parts = unk0[j][i];

					J3DModelData* pJVar17
					    = parts->getMActor()->getModel()->getModelData();
					J3DModelData* pJVar15 = unk60->getModel()->getModelData();

					int uVar9 = pJVar15->getTextureName()->getIndex(
					    cPeachHostTextureName);
					SMS_ChangeTextureAll(
					    pJVar17, cPeachPartsTextureName,
					    *pJVar15->getTexture()->getResTIMG(uVar9));
					parts->getMActor()->initDL();
				}

				if (j == 0) {
					switch (i) {
					case 0:
					case 3:
					case 4:
						NpcPartsInitMotionBlend(this, i, j);
						break;
					}
				}
				break;

			case 0x4000010:
				if (j == 0 && i == 9)
					NpcPartsInitMotionBlend(this, i, j, 20);
				break;

			case 0x4000015:
				if (j == 0 && i == 10) {
					NpcPartsInitMotionBlend(this, i, j);
				}
				break;
			}

			for (int k = 0; k < 3; ++k) {
				const TColorChangeInfo* ccInfo
				    = initInfo->unk4[i]->unk10[k][j];
				if (ccInfo != nullptr)
					SMS_InitChangeNpcColor(unk0[j][i]->getMActor(), ccInfo,
					                       param3, param4);
			}

			if (param4 != nullptr) {
				J3DModel* pJVar18     = unk0[j][i]->getMActor()->getModel();
				J3DModelData* pJVar15 = pJVar18->getModelData();
				u16 matNum            = pJVar15->getMaterialNum();
				for (u16 k = 0; k < matNum; ++k) {
					int shapeIdx = pJVar15->getMaterialNodePointer(k)
					                   ->getShape()
					                   ->getIndex();
					if (!pJVar18->getShapePacket(shapeIdx)->getUserArea()) {
						SMS_InitPacket_OneTevKColor(pJVar18, k, GX_KCOLOR0,
						                            param4);
					}
				}
			}

			unk0[j][i]->getMActor()->setLightType(LIGHT_TYPE_OBJECT);
		}
	}
}

void TNpcParts::addJellyFishParts(f32 param_1)
{
	TSharedParts** slot = &unk0[0][11];

	int iVar2 = gpMareJellyFishManager->getModelDataKeeper()->getModelDataNum();
	f32 fVar1 = MsRandF() * iVar2;
	int iVar3 = fVar1;

	SDLModelData* data
	    = gpMareJellyFishManager->getModelDataKeeper()->getNthData(iVar3);

	SDLModel* model = new SDLModel(data, 0, 1);
	MActor* actor   = new MActor(gpMareJellyFishManager->getMActorAnmData());
	actor->setModel(model, 0);

	*slot = new TSharedParts(unk60, -1, actor);

	actor->setBckFromIndex(0);
	actor->setBrkFromIndex(iVar3);
	actor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(param_1);
	actor->getFrameCtrl(ANM_TYPE_BRK)->setFrame(param_1);
	actor->setLightType(LIGHT_TYPE_INDIRECT);
}

void TNpcParts::setPartsAnmFrame(f32 param_1)
{
	switch (unk60->getActorType()) {
	case 0x4000010:
		SetMActorAnmFrame(getPartsMActor(9, 0), param_1, true, false);
		break;

	case 0x4000015:
		SetMActorAnmFrame(getPartsMActor(10, 0), param_1, true, true);
		break;

	case 0x4000018:
		SetMActorAnmFrame(getPartsMActor(0, 0), param_1, true, false);
		SetMActorAnmFrame(getPartsMActor(3, 0), param_1, true, false);
		SetMActorAnmFrame(getPartsMActor(4, 0), param_1, true, false);
		break;
	}
}

MActor* TNpcParts::getPartsMActor(int param_1, int param_2)
{
	MActor* result = nullptr;
	if (unk0[param_2][param_1])
		result = unk0[param_2][param_1]->getMActor();
	return result;
}

void TNpcParts::partsFrameUpdate()
{
	int i = 0;

	TLodAnm* lodAnm   = unk60->getLodAnm();
	int lod           = lodAnm->unk8;
	TSharedParts** it = unk0[lod];

	for (; i < 12; i++, ++it)
		if (*it) {
			MActor* mactor = (*it)->getMActor();
			mactor->frameUpdate();
		}
}

// Which of Peach's parts show in her current pose. As a predicate level its
// `result` is a callee object, so the flag load and the `li 1` take retail's
// r5/r4 (spelled in the loop they swap); the frame goes 0xd0 -> 0xc8.
static inline bool NpcPartsIsPeachPartShown(const TBaseNPC* npc, int part)
{
	bool result = true;
	if (npc->checkUnk1D8(TBaseNPC::UNK1D8_FLAG_UNK4)) {
		switch (part) {
		case 1:
		case 2:
		case 4:
			result = false;
			break;
		}
	} else if (npc->checkUnk1D8(TBaseNPC::UNK1D8_FLAG_UNK1)) {
		switch (part) {
		case 1:
		case 2:
			result = false;
			break;
		}
	} else {
		switch (part) {
		case 4:
		case 5:
		case 6:
			result = false;
			break;
		}
	}

	return result;
}

void TNpcParts::partsPerform(u32 param_1, JDrama::TGraphics* param_2)
{
	int i = 0;

	TSharedParts** it = unk0[unk60->getLodAnm()->unk8];

	for (; i < 12; ++i, ++it) {
		if (*it == nullptr)
			continue;

		if (unk60->getActorType() == 0x4000018
		    && !NpcPartsIsPeachPartShown(unk60, i))
			continue;

		if (param_1 & 2) {
			if (unk60->isJellyFishMare() && i == 11) {
				MActor* mactor = (*it)->getMActor();
				// TODO: still 40 bytes of frame short of the ROM after
				// the 4x4 fix (0xd0 vs 0xf8): 24 bytes below `mtx` and
				// 16 above it. The register permutation on top of that
				// is retail ranking these inner-block locals *below*
				// `this` (r23 starglowMatIdx, r22 j, r21 matNum, under
				// r24 this / r25 param_1 / r26 param_2) where we lift
				// matNum and j above the parameters. Declaration order
				// is inert on it -- `u16 j` at four positions and `mtx`
				// ahead of `mactor` all give 31 markers (batch 145,
				// docs/catalog/frame-gaps.md).
				// cc27: moving this block into a TU-local static inline
				// taking `*it` lands `mtx` and j/matNum/starglowMatIdx at
				// retail's ranks but hoists the "_starglow1" address into
				// r23 (retail r31), shifting every outer register by one
				// (97.9%, frame 0xe8); passing the string, the MActor or
				// the model data as the parameter, a named J3DTexMtx or
				// J3DModel, `u16 matNum` and `mtx` first are no better.
				// c-k19: with the Peach predicate level the frame is
				// 0xc8 (0x30 short); the starglow block as a helper on
				// top of it is 98.3 (0xe0), taking the parts or MActor.
				Mtx44 mtx;
				SMS_GetLightPerspectiveForEffectMtx(mtx);
				J3DModelData* data = mactor->getModel()->getModelData();
				int starglowMatIdx
				    = data->getMaterialName()->getIndex("_starglow1");
				int matNum = data->getMaterialNum();
				for (u16 j = 0; j < matNum; ++j) {
					if (j != starglowMatIdx)
						data->getMaterialNodePointer(j)
						    ->getTexGenBlock()
						    ->getTexMtx(0)
						    ->setEffectMtx(mtx);
				}
			}
		}

		(*it)->perform(param_1, param_2);
	}
}
