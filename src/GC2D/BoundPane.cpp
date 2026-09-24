#include <GC2D/BoundPane.hpp>
#include <JSystem/J2D/J2DScreen.hpp>

TBoundPane::TBoundPane(J2DScreen* param_1, u32 param_2)
{
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
	if (unk24) {
		if (unk28 > 1.0f) {
			unk28 = 1.0f;
			unk24 = false;
		}

		f32 fVar3 = unk28;
		f32 fVar2 = 1.0f - fVar3;
		f32 fVar5 = fVar3 * fVar3;
		f32 fVar6 = fVar2 * fVar2;
		f32 fVar7 = 2.0f * fVar2 * fVar3;
		f32 fVar4 = unk38.x * fVar6 + unk40.x * fVar7 + unk48.x * fVar5;
		f32 fVar1 = unk38.y * fVar6 + unk40.y * fVar7 + unk48.y * fVar5;

		fVar4 += fVar4 > 0.0f ? 0.5f : -0.5f;
		fVar1 += fVar1 > 0.0f ? 0.5f : -0.5f;

		unk14.x1 = fVar4;
		unk14.y1 = fVar1;
		unk0->move(unk4.x1 + unk14.x1, unk4.y1 + unk14.y1);

		unk28 += unk2C;
	}

	if (unk25) {
		if (unk30 > 1.0f) {
			unk30 = 1.0f;
			unk25 = false;
		}

		f32 fVar3 = unk30;
		f32 fVar2 = 1.0f - fVar3;
		f32 fVar5 = fVar3 * fVar3;
		f32 fVar6 = fVar2 * fVar2;
		f32 fVar7 = 2.0f * fVar2 * fVar3;
		f32 fVar4 = unk50.x * fVar6 + unk58.x * fVar7 + unk60.x * fVar5;
		f32 fVar1 = unk50.y * fVar6 + unk58.y * fVar7 + unk60.y * fVar5;

		fVar4 += fVar4 > 0.0f ? 0.5f : -0.5f;
		fVar1 += fVar1 > 0.0f ? 0.5f : -0.5f;

		unk14.x2 = fVar4;
		unk14.y2 = fVar1;
		unk0->resize(unk14.x2 + unk4.getWidth(), unk14.y2 + unk4.getHeight());

		unk30 += unk34;
	}

	bool result = false;
	if (!unk24 && !unk25)
		result = true;

	return result;
}

void TBoundPane::makeNewPosition(f32, JUTPoint&, JUTPoint&, JUTPoint&,
                                 JUTPoint&)
{
}
