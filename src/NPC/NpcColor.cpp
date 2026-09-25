#include <NPC/NpcColor.hpp>

#include <MarioUtil/PacketUtil.hpp>
#include <M3DUtil/MActor.hpp>

// TODO: UNUSED in the original (fully inlined). These bodies are inferred
// from the PacketUtil calls they wrap; their emitted sizes match marioEU.MAP,
// but the dead-stripped target instructions are unavailable for comparison.
void InitChangeOneColor_Base(J3DModel* model, u16 mat_idx, GXTevRegID tev_reg_id,
                             const GXColorS10* tev_color,
                             const GXColor* tev_k_color)
{
	if (tev_k_color != nullptr) {
		SMS_InitPacket_OneTevColorAndOneTevKColor(
		    model, mat_idx, tev_reg_id, tev_color, tev_k_color);
	} else {
		SMS_InitPacket_OneTevColor(model, mat_idx, tev_reg_id, tev_color);
	}
}

// TODO: UNUSED (dead code in the original): see InitChangeOneColor_Base.
void InitChangeTwoColor_Base(J3DModel* model, u16 mat_idx,
                             const GXColorS10* tev_color0,
                             const GXColorS10* tev_color1,
                             const GXColor* tev_k_color)
{
	if (tev_k_color != nullptr) {
		SMS_InitPacket_TwoTevColorAndOneTevKColor(
		    model, mat_idx, GX_TEVREG1, tev_color0, GX_TEVREG2, tev_color1,
		    tev_k_color);
	} else {
		SMS_InitPacket_TwoTevColor(model, mat_idx, GX_TEVREG1, tev_color0,
		                           GX_TEVREG2, tev_color1);
	}
}

// TODO: All instructions match, but MWCC reserves a 0x40-byte frame instead
// of the target's 0x38. The 5+ argument PacketUtil calls introduce two hidden
// 4-byte temporaries; no non-artificial source spelling removes them yet.
void SMS_InitChangeNpcColor(const MActor* param1,
                            const TColorChangeInfo* param2, s16 param3,
                            const GXColor* param4)
{
	J3DModel* model = param1->getModel();
	s32 matIdx
	    = model->getModelData()->getMaterialName()->getIndex(param2->unk4);
	switch (param2->unk0) {
	case 0:
		if (param2->unk8 != nullptr) {
			GXColor* matColor = new GXColor();
			matColor->r       = param2->unk8[param3].r;
			matColor->g       = param2->unk8[param3].g;
			matColor->b       = param2->unk8[param3].b;
			matColor->a       = 0xff;
			SMS_InitPacket_MatColor(model, matIdx, GX_COLOR0, matColor);
		}
		break;
	case 1:
		if (param2->unk8 != nullptr) {
			InitChangeOneColor_Base(model, matIdx, GX_TEVREG0,
			                        &param2->unk8[param3], param4);
		}
		break;
	case 2:
		if (param2->unk8 != nullptr && param2->unkC != nullptr) {
			InitChangeTwoColor_Base(model, matIdx, &param2->unk8[param3],
			                        &param2->unkC[param3], param4);
		} else if (param2->unk8 != nullptr && param2->unkC == nullptr) {
			InitChangeOneColor_Base(model, matIdx, GX_TEVREG1,
			                        &param2->unk8[param3], param4);
		} else if (param2->unk8 == nullptr && param2->unkC != nullptr) {
			InitChangeOneColor_Base(model, matIdx, GX_TEVREG2,
			                        &param2->unkC[param3], param4);
		}
		break;
	}
}
