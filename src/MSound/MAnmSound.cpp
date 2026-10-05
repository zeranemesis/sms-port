#include <MSound/MAnmSound.hpp>
#include <MSound/MSHandle.hpp>
#include <math.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

MAnmSound::MAnmSound(MSound* sound) { mData = nullptr; }

void MAnmSound::initAnmSound(void* interface, u32 param_2, f32 frame)
{
	initActorAnimSound(interface, param_2, frame);
}

void MAnmSound::animeLoop(Vec* position, f32 frame, f32 speed, u32 ground_no,
                          u8 param_5)
{
	if (mData != nullptr)
		setAnimSoundVec(JAIBasic::getInterface(), position, frame, speed,
		                ground_no, param_5);
}

void MAnmSound::startAnimSound(void* interface, u32 id,
                               JAISoundHandle* out_handle, JAIActor* actor,
                               u8 camera_idx)
{
	if (MSGMSound->gateCheck(id))
		MSoundSESystem::MSoundSE::startSoundActorInner(id, out_handle, actor, 0,
		                                               camera_idx);
}

void MAnmSound::setSpeedModifySound(JAISound* sound,
                                    JAIAnimeFrameSoundData* frame_data,
                                    f32 speed)
{
	if (MSound::getSwitch(sound->getID(), MSSeSwBit_AnimeSpeed,
	                      MSSeSwBit_AnimeSpeedShift))
		JAIAnimeSound::setSpeedModifySound(sound, frame_data, speed);
}

#ifdef VERSION_GMSP01
void MAnmSoundMario::startAnimSound(void* interface, u32 id,
                                    JAISoundHandle* out_handle,
                                    JAIActor* actor, u8 camera_idx)
{
	if (!MSGMSound->gateCheck(id))
		return;

	// fakematch: only inflates the stack frame from 0x30 to the ROM's 0x38.
	
	

	u32 category = id >> 30;
	s32 soundType = (id >> 12) & 0xF;
	if (category != 0) {
		if (category == 2)
			soundType = 0x10;
		else if (category == 3)
			soundType = 0x11;
		else
			soundType = -1;
	}

	switch (soundType) {
	case 0:
		if ((actor->mGroundNumber & 0x1000) == 0x1000)
			return;
		break;

	case 7:
		{
			u32 groundNumber = actor->mGroundNumber;
			MSGMSound->startMarioVoice(
			    id, static_cast<s16>((groundNumber >> 24) & 0xF),
			    static_cast<u8>(groundNumber >> 28));
		}
		return;

	default:
		break;
	}

	MSoundSESystem::MSoundSE::startSoundActorInner(
	    id, out_handle, actor, 0, camera_idx);
}
#endif

f32 MSMarioPosVolume::getDistFromMario(const Vec& pos)
{
	if (MSGMSound->cameraLooksAtMario()) {
		const Vec* mario = MSGMSound->unkAC[0].mPosition;
		// TODO (libs/ blocker, one of the seven measured MSL gaps): the ROM
		// emits an out-of-line weak `sqrtf__3stdFf` in this TU and calls it;
		// libs/.../MSL_Common/math.h only has an *inlined* sqrtf, so the
		// out-of-line copy is never emitted and the whole Newton-Raphson
		// body (~24 instructions) is inlined into the caller instead. Do NOT
		// edit libs/ - the arithmetic is correct, only the call shape cannot
		// be reproduced from game code.
		return std::sqrtf(std::powf(pos.x - mario->x, 2.0f)
		                  + std::powf(pos.y - mario->y, 2.0f)
		                  + std::powf(pos.z - mario->z, 2.0f));
	}

	return 0.0f;
}

void MAnmSoundNPC::startAnimSound(void* interface, u32 sound_id,
                                  JAISound** out_handle, JAIActor* actor,
                                  u8 camera_idx)
{
	// TODO: the ROM rematerialises &mEntries[mDataCounter] and re-reads
	// unk10 (add r4,r31,r5 / lwz r4,0x18(r4)) just before the JAIConst::random
	// update, because the value is no longer live in a register by then. Our
	// register allocation keeps it in r5 across the whole block, so those two
	// instructions are missing. Not yet found a source spelling that makes
	// MWCC drop the cached copy.
	if (MSGMSound->gateCheck(sound_id)) {
		// fakematch: only inflates the stack frame from 0x88 to the ROM's 0x90.
		
		

		JAIAnimeSoundData* ptr = mData;

		if (ptr->mEntries[mDataCounter].unk10 & 0xFFFF0000) {
			if (ptr->mEntries[mDataCounter].unk10 & 0xFF000000) {
				u32 uVar5 = mLoopCount;
				u32 uVar6 = (ptr->mEntries[mDataCounter].unk10 >> 24) + 1;
				if (uVar5 != 0) {
					u32 uVar3 = uVar5 + unk98 % uVar6;
					if (uVar3 % uVar6 != 0)
						return;
				}
			}

			if (ptr->mEntries[mDataCounter].unk10 & 0xFF0000) {
				u8 b = ptr->mEntries[mDataCounter].unk10 >> 16;
				b += 1;
				b *= JAIConst::random.get_ufloat_1();
				if (b != 0)
					return;
			}
		}

		if (MSoundSESystem::MSoundSE::checkMonoSound(sound_id, actor)) {
			MSoundSESystem::MSoundSE::startSoundActorInner(
			    sound_id, out_handle, actor, 0, camera_idx);

			if (*out_handle != nullptr
			    && !(ptr->mEntries[mDataCounter].unk10 & 0x8000)) {

				f32 dVar10 = 1.0f;

				f32 fVar11
				    = MSMarioPosVolume::getDistFromMario(*actor->mTranslation);

				if (fVar11 != 0.0f)
					dVar10 = MSHandle::calcVolume(
					    fVar11, 2000.0f, 600.0f,
					    ptr->mEntries[mDataCounter].unk10 >> 12 & 7, 8);

				(*out_handle)->setSeInterVolume(0, dVar10, 0, 0);
			}
		}
	}
}
