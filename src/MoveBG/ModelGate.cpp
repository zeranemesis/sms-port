#include <MoveBG/ModelGate.hpp>
#include <System/EmitterViewObj.hpp>
#include <dolphin/mtx.h>
#include <stdlib.h>

static const char* gateMActorNames[5]
    = { "05_gate01", "05_gate02rico", "05_gate03manma", "05_gate04monte",
	    "05_gate05mare" };

void TModelGate::startOpen()
{
	unk70 |= 1;
	unkC4 = 0;
	unk78->setBpk(gateMActorNames[unk71]);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	unk70 |= 2;
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
						gpMarioParticleManager->emitWithRotate(
						    0x1DD, &sender->getPosition(), 0, unk74, 0, 2,
						    nullptr);
						gpMarioParticleManager->emitWithRotate(
						    0x1DE, &sender->getPosition(), 0, unk74, 0, 2,
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

MtxPtr TModelGate::getTakingMtx()
{
	return nullptr;
}
