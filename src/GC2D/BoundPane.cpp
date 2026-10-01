#include <GC2D/BoundPane.hpp>
#include <JSystem/J2D/J2DScreen.hpp>

TBoundPane::TBoundPane(J2DScreen* param_1, u32 param_2)
{
	// TODO: frame pad to match the original stack frame size.
	char framePad_8_ctor[8];

	unk14.x1 = 0;
	unk14.y1 = 0;
	unk14.x2 = 0;
	unk14.y2 = 0;

	unk0  = param_1->search(param_2);
	unk4  = unk0->mBounds;
	unk28 = 0.0f;
	unk2C = 0.0f;
	unk30 = 0.0f;
	unk34 = 0.0f;
	unk24 = false;
	unk25 = false;
}

TBoundPane::TBoundPane(JUTTexture*, GXCullMode) { }

void TBoundPane::setPanePosition(s32 param_1, const JUTPoint& param_2,
                                 const JUTPoint& param_3,
                                 const JUTPoint& param_4)
{
	unk28 = 0.0f;
	unk2C = 1.0f / param_1;
	unk38 = param_2;
	unk40 = param_3;
	unk48 = param_4;
	unk24 = true;
}

void TBoundPane::setPaneSize(s32 param_1, const JUTPoint& param_2,
                             const JUTPoint& param_3, const JUTPoint& param_4)
{
	unk30 = 0.0f;
	unk34 = 1.0f / param_1;
	unk50 = param_2;
	unk58 = param_3;
	unk60 = param_4;
	unk25 = true;
}

bool TBoundPane::update()
{
	// TODO: frame pad to match the original stack frame size.
	char framePad_16_update[16];

	if (unk24) {
		if (unk28 > 1.0f) {
			unk28 = 1.0f;
			unk24 = false;
		}

		f32 fVar3 = unk28;
		f32 fVar5 = fVar3 * fVar3;
		f32 fVar2 = 1.0f - fVar3;
		f32 fVar6 = fVar2 * fVar2;
		f32 fVar7 = 2.0f * fVar2 * fVar3;
		f32 fVar4 = unk38.x * fVar6 + unk40.x * fVar7 + unk48.x * fVar5;
		f32 fVar1 = unk38.y * fVar6 + unk40.y * fVar7 + unk48.y * fVar5;
		fVar4 += fVar4 > 0.0f ? 0.5f : -0.5f;
		unk14.x1 = (s16)fVar4;

		fVar1 += fVar1 > 0.0f ? 0.5f : -0.5f;
		unk14.y1 = (s16)fVar1;
		unk0->move(unk4.x1 + unk14.x1, unk4.y1 + unk14.y1);

		unk28 += unk2C;
	}

	if (unk25) {
		if (unk30 > 1.0f) {
			unk30 = 1.0f;
			unk25 = false;
		}

		f32 fVar3 = unk30;
		f32 fVar5 = fVar3 * fVar3;
		f32 fVar2 = 1.0f - fVar3;
		f32 fVar6 = fVar2 * fVar2;
		f32 fVar7 = 2.0f * fVar2 * fVar3;
		f32 fVar4 = unk50.x * fVar6 + unk58.x * fVar7 + unk60.x * fVar5;
		f32 fVar1 = unk50.y * fVar6 + unk58.y * fVar7 + unk60.y * fVar5;
		fVar4 += fVar4 > 0.0f ? 0.5f : -0.5f;
		unk14.x2 = (s16)fVar4;

		fVar1 += fVar1 > 0.0f ? 0.5f : -0.5f;
		unk14.y2 = (s16)fVar1;
		unk0->resize(unk14.x2 + unk4.getWidth(), unk14.y2 + unk4.getHeight());

		unk30 += unk34;
	}

	bool result = false;
	if (!unk24 && !unk25)
		result = true;

	return result;
}

// Quadratic Bezier evaluation. UNUSED in the ROM (map size 0x138): the ROM never
// emitted a standalone copy, so this body has to match the shape that update()
// duplicates twice by hand.
// TODO: reconstructing it and calling it from update() makes the frame explode
// (0x48 -> 0xa0), so the original inlined it into a JUTPoint out-param that our
// J2DPane/JUTPoint types do not scalar-replace the same way.
void TBoundPane::makeNewPosition(f32 param_1, JUTPoint& param_2,
                                 JUTPoint& param_3, JUTPoint& param_4,
                                 JUTPoint& param_5)
{
	f32 fVar3 = param_1;
	f32 fVar2 = 1.0f - fVar3;
	f32 fVar5 = fVar3 * fVar3;
	f32 fVar6 = fVar2 * fVar2;
	f32 fVar7 = 2.0f * fVar2 * fVar3;

	param_5.x = param_2.x * fVar6 + param_3.x * fVar7 + param_4.x * fVar5;
	param_5.y = param_2.y * fVar6 + param_3.y * fVar7 + param_4.y * fVar5;
}
