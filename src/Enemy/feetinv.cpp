#include <Enemy/FeetInv.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

// TODO: the whole TU is dead in retail (every symbol that is not a weak
// JSystem inline is UNUSED), so there is no asm to verify any of this against.
// Only the sizes from the map can be used as a constraint.

// TODO: 0xA00 bytes in the map, some kind of two-bone IK that keeps the feet
// on the ground. Body unknown.
void FeetInvCalc(J3DModel* model, u16 param_2, u16 param_3, u16 param_4,
                 f32 param_5)
{
}

TMtxCalcFootInv::TMtxCalcFootInv(u16 param_1, u16 param_2, u16 param_3,
                                 u16 param_4, u16 param_5, u16 param_6,
                                 f32 param_7)
    : J3DMtxCalcSoftimageAnm(nullptr)
    , unk68(param_1)
    , unk6A(param_2)
    , unk6C(param_3)
    , unk6E(param_4)
    , unk70(param_5)
    , unk72(param_6)
    , unk74(param_7)
{
}

// TODO: incorrect size, 0x94 in the map vs 0x54 here
void TMtxCalcFootInv::calc(u16 idx)
{
	J3DMtxCalcSoftimageAnm::calc(idx);

	if (unk6C == idx)
		FeetInvCalc(j3dSys.getModel(), unk68, unk6A, unk6C, unk74);

	if (unk72 == idx)
		FeetInvCalc(j3dSys.getModel(), unk6E, unk70, unk72, unk74);
}
