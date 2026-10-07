#include <JSystem/JAudio/JAInterface/JAIBasic.hpp>
#include <JSystem/JAudio/JAInterface/JAIGlobalParameter.hpp>
#include <JSystem/JAudio/JAInterface/JAISystemInterface.hpp>
#include <JSystem/JAudio/JAInterface/JAIParameters.hpp>
#include <JSystem/JAudio/JAInterface/JAIConst.hpp>
#include <math.h>

// TODO: 99.3%, frame 0x190 and every instruction exact; three GPR webs differ.
// Closure c-k4 (debugger): retail loads the category's mMaxPlaying straight
// into the register of the candidate counter (r26), so the two are one u8
// local (`num`, counted up while candidates are ranked, then reloaded with
// the category maximum). A separate `maxPlaying` is initialised once from a
// load, so the IR optimiser splits it (`maxPlaying = @250 = load`) and the
// hoisted `maxPlaying + 1` reads @250: that cost the extra `addi r29, r3, 0`
// and a whole-function callee-saved rotation (the four pool/base temps were
// pushed in the second simplify sweep, before the locals). With one `num`
// (two definitions, never split) the rotation and the copy are gone.
// Left: `it` r27 (retail r18), `pi` r18 (r20), the hoisted `(u8)camEnd` r20
// (r27). In the replay `it` reaches the second sweep with remaining degree 30
// (one over K after `cam` is pushed), so it is deferred and coloured at
// position 8; retail colours it between @245 and `pi` (positions 19-24), and
// then needs `cam` coloured before `pi`. Inert here: `u8 cam` declared at the
// top ahead of `pi` (69 markers), `it` declared at the top first (90) or
// after `pi` (68), and both together (85, 73).
// The replay closes with two changes together: C-style top declarations
// `..., bVar18; JAISound* it; u8 cam; ...* pi; u8 num;` (numbering pi < cam <
// it so `it` is pushed in the second sweep; alone 85 markers, because the
// four hoisted pool temps -- &candidates, 0x4330, &dummyZeroVec, 0x7fffffff --
// then drop below K in that sweep too) plus one more coalesced "ghost" web
// neighbouring those four pool temps but not `it`: `regalloc.py`-style replay
// with that ghost misses 0 webs. So retail has one extra coalesced copy (an
// IRO split temp like `snd`'s @242, or an argument copy) live across the i
// loop outside the `while (it)` loop; which statement carries it is unknown.
// c-k25 (replay with c-k4's top declarations): the ghost may neighbour every
// web except `it`, `l` and the F-loop `k` split (@254), so it is live outside
// `while (it)`, the `l` loop and the `else` branch's `k` loop. Inert on top of
// those declarations: all 107 reassignments of j/k/l across the eight loops
// (85 markers each), a `head` copy of mUsedHead, local `s` copies of
// candidates[k].sound or mSeTrack[i][j].mSound (72-85, whole rotation), a
// bVar18 hop for the second `num`, and `k == (u8)num`.
// Older readings: batch 145 (25 declaration orders, eight relocations into
// the `for (i...)` body) and c-jai (ternary `fVar3`/`fVar1`, a named `s16
// adjust`) were all measured against the split `maxPlaying` and are void now.
void JAIBasic::checkNextFrameSe()
{
	JAISound sound;

	struct Candidate {
		/* 0x0 */ u8 state;
		/* 0x4 */ u32 score;
		/* 0x8 */ JAISound* sound;
	};
	Candidate candidates[16];

	s16 prio;
	u8 camStart;
	u8 camEnd;
	JAISound* snd;
	u8 l;
	int i;
	u8 k;
	u8 j;
	u8 bVar7;
	u8 bVar18;
	JAISound::FabricatedPositionInfo* pi;

	f32 fVar6
	    = JAIGlobalParameter::distanceMax * JAIGlobalParameter::distanceMax;
	f32 fVar1 = JAIGlobalParameter::distanceMax / 1000.0f;
	if (fVar1 == 0.0f)
		fVar1 = 1.0f;

	for (i = 0; i < JAIGlobalParameter::getParamSeCategoryMax(); ++i) {
		for (j = 0; j < unk0->mCategoryInfoTable[mSoundScene][i].mMaxPlaying;
		     ++j) {
			candidates[j].score = 0x7fffffff;
			candidates[j].sound = nullptr;
			candidates[j].state = 0xff;
		}

		u8 num = 0;

		JAISound* it = unk0->mSeRegist[i].mUsedHead;
		while (it) {
			if (it->mState == SOUNDSTATE_Stored && (it->mSoundID & 0xC00)) {
				it->decWait();
			} else if (!(it->mSoundID & 0xC00)
			           && it->mState == SOUNDSTATE_Stopping) {
				sound.mNextSound = it->mNextSound;
				releaseSeRegist(it);
				it = &sound;
			}

			if (it->getWait() == 0) {
				sound.mNextSound = it->mNextSound;
				releaseSeRegist(it);
				it = &sound;
			} else if (it->mState != SOUNDSTATE_Inactive) {
				f32 fVar2 = 2147483647.0f;
				if (it->mCameraIdx == 4) {
					camStart = 0;
					camEnd   = JAIGlobalParameter::audioCameraMax;
				} else {
					camEnd   = it->mCameraIdx + 1;
					camStart = it->mCameraIdx;
				}

				for (u8 cam = camStart; cam < camEnd; ++cam) {
					pi = &it->unk1C[cam];

					pi->mPrevCamSpacePos = pi->mCamSpacePos;
					if (it->mActorTrans == nullptr) {
						pi->mCamSpacePos = JAIConst::dummyZeroVec;
					} else {
						MTXMultVec(mAudioCameras[cam].nViewMtx,
						           (Vec*)it->mActorTrans, &pi->mCamSpacePos);
					}

					pi->unk18 = pi->mCamSpacePos.x * pi->mCamSpacePos.x
					            + pi->mCamSpacePos.y * pi->mCamSpacePos.y
					            + pi->mCamSpacePos.z * pi->mCamSpacePos.z;
					prio = it->getInfoPriority();
					if (it->getAdjustPriority()) {
						prio += it->getAdjustPriority();
						if (prio < 0)
							prio = 0;
						else if (prio > 0xff)
							prio = 0xff;
					}

					it->mPriority
					    = (u32)((0xff - prio) * (0xff - prio) * 0x1690 / fVar1)
					      + (u32)(pi->unk18 / fVar1);
					if (pi->mCamSpacePos.z > 0.0f)
						it->mPriority
						    += (u32)(pi->mCamSpacePos.z * 6.0f / fVar1);

					if (cam == 0 || pi->unk18 < fVar2)
						fVar2 = pi->unk18;
				}

				if (it->mCameraIdx == 4)
					it->mPriority /= JAIGlobalParameter::audioCameraMax;

				f32 fVar3;
				if (it->getSwBit() & JAISeSwBit_DistanceLimit)
					fVar3 = fVar6;
				else
					fVar3 = 1e+10f;

				if (fVar2 > fVar3) {
					if (!(it->mSoundID & 0xC00)) {
						if (it->mState != SOUNDSTATE_Stored) {
							JAISystemInterface::writePortApp(
							    mSeSequence->getSeqParameter()->mSeqHandle,
							    (it->mTrack >> 4) + 0x20000000
							        + ((it->mTrack & 0xf) << 4),
							    0);
							mSeSequence->setTrackInterruptSwitch(it->mTrack, 1);
						}
						it->mState = SOUNDSTATE_Stored;
					} else {
						sound.mNextSound = it->mNextSound;
						stopSoundHandle(it, 0);
						it = &sound;
					}
				} else {
					bVar18 = unk0->mCategoryInfoTable
					             [mSoundScene][(u8)it->getSeCategoryNumber()]
					                 .mMaxPlaying;
					for (j = 0; j < bVar18; ++j) {
						if (it->mPriority < candidates[j].score
						    || (it->mPriority == candidates[j].score
						        && candidates[j].state >= it->mState)) {
							if (num < bVar18)
								++num;
							for (k = bVar18 - 1; k > j; --k) {
								candidates[k].score = candidates[k - 1].score;
								candidates[k].sound = candidates[k - 1].sound;
								candidates[j].state = candidates[k - 1].state;
							}
							candidates[j].score = it->mPriority;
							candidates[j].sound = it;
							candidates[j].state = it->mState;

							j = bVar18;
						}
					}
				}
			}
			if (it != nullptr)
				it = it->mNextSound;
		}

		for (k = 0; k < num; ++k) {
			snd = candidates[k].sound;
			if (snd->mState == SOUNDSTATE_Stored) {
				snd->mState = SOUNDSTATE_Prepared;
			} else if (snd->mState == SOUNDSTATE_Playing) {
				snd->mState = SOUNDSTATE_Started;
			}
		}

		num = unk0->mCategoryInfoTable[mSoundScene][i].mMaxPlaying;
		for (j = 0; j < num; ++j) {
			snd   = unk0->mSeTrack[i][j].mSound;
			bVar7 = 0;
			if (snd == nullptr) {
				bVar7 = 1;
			} else if (snd->mState == SOUNDSTATE_Playing) {
				if (snd->mSoundID & 0xC00) {
					releaseSeRegist(snd);
				} else {
					snd->mState     = SOUNDSTATE_Stored;
					snd->mWaitTimer = 0;
				}
				bVar7 = 1;
			} else if (snd->mState == SOUNDSTATE_Inactive) {
				unk0->mSeTrack[i][j].mSound = nullptr;
				bVar7                       = 1;
			} else {
				for (k = 0; k < num; ++k) {
					if (unk0->mSeTrack[i][j].mSound == candidates[k].sound) {
						candidates[k].sound = nullptr;
						k                   = num;
					}
				}
			}

			if (bVar7 == 1) {
				for (k = 0; k < num; ++k) {
					snd = candidates[k].sound;
					if (snd != nullptr && snd->mState != SOUNDSTATE_Started) {
						for (l = 0; l < num; ++l) {
							if (unk0->mSeTrack[i][l].mSound
							    && snd == unk0->mSeTrack[i][l].mSound) {
								bVar7 = 0;
								l     = num;
							}
						}

						if (bVar7 == 1) {
							unk0->mSeTrack[i][j].mSound = snd;
							candidates[k].sound         = nullptr;
							k                           = num + 1;
						}
					}
				}
				if (k == num) {
					unk0->mSeTrack[i][j].mSound = nullptr;
				}
			}
		}
	}
}

// TODO: every instruction matches and the frame is 0xa8 exact, but four stack
// slots sit elsewhere. The locals area is 0xc..0x30 (the f32-to-int conversion
// slot at 0x30 and the register saves from 0x3c match), and inside it retail
// has its temp pool end at 0x24 (the `get_ufloat_1` bit-cast at 0x1c, the
// `std::sqrtf` round trip at 0x20) with `readStatus1`/`readStatus0` at
// 0x26/0x28, where we have 8 bytes more temp pool (bit-cast 0x24, round trip
// 0x28) and the pair at 0x2c/0x2e. So two opposite corrections are needed: one
// inline expansion too many below the temps, and 6 bytes of named locals
// declared *before* `readStatus0` that we are missing (0x2a..0x30 is empty in
// retail, which is three more u16s or equivalent).
// TODO: 100.0% by instruction, frame 0xa8 exact; the eight remaining operands
// are all stack displacements. Retail puts the two `readPortApp` u16s at
// 0x26/0x28 and the two 4-byte float temps at 0x1c/0x20; ours are at 0x2c/0x2e
// and 0x24/0x28. Relative order and adjacency are right, so the low pool below
// the float temps is 8 bytes larger in ours (0xc..0x24 against 0xc..0x1c) and
// the named block starts 6 bytes higher, which is the -8 pool / +6 named split
// recorded in frame-gaps.md. Closure round 2026-09-18: writing the sqrt loop
// as `infos[k].unk18 = std::sqrtf(infos[k].unk18);` is byte-identical,
// dropping `infos` costs an instruction, and dropping the `portMask` binding
// takes the frame to 0xa0 (142 operands) -- so the pool is reachable from the
// `portMask` binding's end, but nothing found yet removes 8 bytes from it
// without moving code.
void JAIBasic::sendPlayingSeCommand()
{
	u16 readStatus0;
	u16 readStatus1;
	u8 j;
	JAISound* sound;
	u8 trackId = 0;

	for (u8 cat = 0; cat < JAIGlobalParameter::getParamSeCategoryMax(); ++cat) {
		for (j = 0; j < unk0
		                    ->mCategoryInfoTable[mSoundScene][(u8)cat]
		                    .mMaxPlaying;
		     ++trackId, ++j) {
			sound = unk0->mSeTrack[cat][j].mSound;
			if (sound == nullptr)
				continue;

			sound->incPlayGameFrameCounter();

			u32 portAddr
			    = ((trackId >> 4) & 0xF) + 0x20000000 + ((trackId & 0xF) << 4);

			u32 seqPort = mSeSequence->getSeqParameter()->mSeqHandle;

			JAISystemInterface::readPortApp(seqPort, portAddr + 0x20000,
			                                &readStatus0);
			JAISystemInterface::readPortApp(seqPort, portAddr, &readStatus1);

			JAISound::FabricatedPositionInfo* infos = sound->unk1C;
			for (u8 k = 0; k < JAIGlobalParameter::audioCameraMax; ++k) {
				f32* dPtr = &infos[k].unk18;
				*dPtr     = std::sqrtf(*dPtr);
			}

			u8 state = sound->getStatus();
			if (state == SOUNDSTATE_Prepared) {
				u32 swBit     = sound->getSwBit();
				sound->mTrack = trackId;
				if (swBit & JAISeSwBit_SeqMute)
					setSeqMuteFromSeStart(sound);

				if (swBit & JAISeSwBit_RandomPitchWidthMask) {
					s32 rnd = (s32)(255.0f * JAIConst::random.get_ufloat_1());
					switch (swBit & JAISeSwBit_RandomPitchWidthMask) {
					case JAISeSwBit_RandomPitchWidthSmall:
						sound->setRandom(rnd & 0xF);
						break;
					case JAISeSwBit_RandomPitchWidthMiddle:
						sound->setRandom(rnd & 0x1F);
						break;
					case JAISeSwBit_RandomPitchWidthLarge:
						sound->setRandom(rnd & 0x3F);
						break;
					default:
						sound->setRandom(0);
						break;
					}
				}

				u16* portMask = &sound->getSeParameter()->mPortUpdate;
				for (u8 i = 0; *portMask != 0; ++i) {
					u32 bit = 1 << i;
					if (*portMask & bit) {
						mSeSequence->setTrackPortData(
						    sound->mTrack, i,
						    sound->getSeParameter()->mPortData[i]);
						*portMask ^= bit;
					}
				}

				sound->setSeDistanceParameters();
				setSeExtParameter(sound);

				if (sound->mFadeCounter > 1) {
					sound->setSeInterVolume(6, 0.0f, 0, 0);
					sound->setSeInterVolume(6, 127.0f, sound->mFadeCounter, 0);
					sound->mFadeCounter = 0;
				}

				sendSeAllParameter(sound);

				u16 portValue = sound->mSoundID & JAISoundID_IndexMask;
				if (sound->checkSwBit(JAISeSwBit_GroundVariant)) {
					u32 tmp = sound->mActorGroundNumber;
					portValue += getMapInfoGround(tmp);
				}

				u16 distArg;
				if (JAIGlobalParameter::audioCameraMax == 1
				    && sound->checkSwBit(JAISeSwBit_DistanceWait)) {
					if (sound->unk1C[0].unk18
					    < JAIGlobalParameter::distanceMax) {
						distArg = JAIGlobalParameter::seDistanceWaitMax
						          * (u32)sound->unk1C[0].unk18
						          / (u32)JAIGlobalParameter::distanceMax;
					} else {
						distArg = JAIGlobalParameter::seDistanceWaitMax;
					}
				} else {
					distArg = 0;
				}

				JAISystemInterface::writePortApp(seqPort, portAddr + 0x30000,
				                                 distArg);
				JAISystemInterface::writePortApp(
				    seqPort, portAddr + 0x60000,
				    getMapInfoFxline(sound->mActorGroundNumber));
				JAISystemInterface::writePortApp(seqPort, portAddr + 0x40000,
				                                 portValue);
				JAISystemInterface::writePortApp(seqPort, portAddr, 1);

				if (sound->mSoundID & 0xC00) {
					sound->mState = SOUNDSTATE_Playing;
				} else {
					sound->mState = SOUNDSTATE_Stopping;
				}
			} else if (readStatus0 == 0 && readStatus1 != 1) {
				releaseSeRegist(sound);
			} else if (sound->mFadeCounter != 0) {
				if (sound->getSeParameter()->mVolume[6].mCurrentValue != 0.0f) {
					sound->setSeDistanceParameters();
					sendSeAllParameter(sound);
					if (sound->mSoundID & 0xC00) {
						sound->mState = SOUNDSTATE_Playing;
					} else {
						sound->mState = SOUNDSTATE_Stopping;
					}
				} else {
					releaseSeRegist(sound);
				}
			} else if (state == SOUNDSTATE_Started) {
				sound->setSeDistanceParameters();
				sendSeAllParameter(sound);
				if (sound->mSoundID & 0xC00) {
					sound->mState = SOUNDSTATE_Playing;
				} else {
					sound->mState = SOUNDSTATE_Stopping;
				}
			}
		}
	}
}

void JAIBasic::setSeqMuteFromSeStart(JAISound* param_1)
{
	for (u32 i = 0; i < JAIGlobalParameter::seqPlayTrackMax; ++i) {
		JAISound* sound = unk0->mSeqTrackInfo[i].mSound;
		if (i != mSeSequence->mTrack && sound
		    && !(sound->getSwBit() & JAISeqSwBit_NoSeqMute)) {
			sound->setSeqInterVolume(
			    9, JAIGlobalParameter::seqMuteVolumeSePlay / 127.0f,
			    JAIGlobalParameter::seqMuteMoveSpeedSePlay);
			unk30 |= 1 << param_1->mTrack;
		}
	}
}

void JAIBasic::clearSeqMuteFromSeStop(JAISound* sound)
{
	if (unk30 == 0 || !(sound->getSwBit() & JAISeSwBit_SeqMute))
		return;

	for (u32 i = 0; i < JAIGlobalParameter::seqPlayTrackMax; ++i) {
		JAISound* seq = unk0->mSeqTrackInfo[i].mSound;
		if (i != mSeSequence->mTrack && seq
		    && !(seq->getSwBit() & JAISeqSwBit_NoSeqMute)) {
			unk30 &= (1 << sound->mTrack) ^ 0xffffffff;
			if (unk30 == 0) {
				seq->setSeqInterVolume(
				    9, 1.0f, JAIGlobalParameter::seqMuteMoveSpeedSePlay);
			}
		}
	}
}

void JAIBasic::checkSeMovePara()
{
	if (!mSeSequence || mSeSequence->getSeqParameter()->mPauseMode == 2)
		return;

	for (u8 i = 0; i < JAIGlobalParameter::getParamSeCategoryMax(); ++i) {
		for (JAISound* it = unk0->mSeRegist[i].mUsedHead; it != nullptr;
		     it           = it->mNextSound) {
			unk0->setSeMovePara(it->getSeParameter()->mVolume);
			unk0->setSeMovePara(it->getSeParameter()->mPan);
			unk0->setSeMovePara(it->getSeParameter()->mFxmix);
			unk0->setSeMovePara(it->getSeParameter()->mFir);
			unk0->setSeMovePara(it->getSeParameter()->mDolby);
			unk0->setSeMovePara(it->getSeParameter()->mPitch);
		}
	}
}

void JAIBasic::sendSeAllParameter(JAISound* sound)
{
	JAIData::FabricatedSeTrackParameter* slot = &unk0->unk0[sound->mTrack];
	JAISeParameter* seParam                   = sound->getSeParameter();

	// Volume
	f32 vol;
	if (seParam->mVolume[7].mCurrentValue == -1.0f) {
		if (seParam->mVolumePointer != nullptr) {
			seParam->mVolume[0].mCurrentValue = *seParam->mVolumePointer;
		}
		vol = 1.0f;
		for (int i = 0; i < 7; ++i)
			vol *= seParam->mVolume[i].mCurrentValue;
	} else {
		vol = seParam->mVolume[7].mCurrentValue;
	}
	vol *= mSeCategoryVolume[(u8)sound->getSeCategoryNumber()];
	if (slot->mVolume != vol) {
		slot->mVolume = vol;
		if (sound->mState != SOUNDSTATE_Prepared) {
			unk0->mSeqTrackInfo[mSeSequence->mTrack].mTrackUpdate[sound->mTrack]
			    |= 0x1;
			JAISystemInterface::setSeqPortargsF32(
			    &unk0->mSeqTrackInfo[mSeSequence->mTrack], sound->mTrack, 2,
			    vol);
		}
	}

	// Pan
	f32 pan;
	if (seParam->mPan[7].mCurrentValue == -1.0f) {
		if (seParam->mPanPointer != nullptr) {
			seParam->mPan[0].mCurrentValue = *seParam->mPanPointer;
		}
		pan = 0.0f;
		for (int i = 0; i < 7; ++i)
			if (seParam->mPan[i].mCurrentValue != 0.5f)
				pan += seParam->mPan[i].mCurrentValue - 0.5f;
		pan += 0.5f;
		if (pan < 0.0f)
			pan = 0.0f;
		else if (pan > 1.0f)
			pan = 1.0f;
	} else {
		pan = seParam->mPan[7].mCurrentValue;
	}
	if (slot->mPan != pan) {
		slot->mPan = pan;
		if (sound->mState != SOUNDSTATE_Prepared) {
			unk0->mSeqTrackInfo[mSeSequence->mTrack].mTrackUpdate[sound->mTrack]
			    |= 0x4;
			JAISystemInterface::setSeqPortargsF32(
			    &unk0->mSeqTrackInfo[mSeSequence->mTrack], sound->mTrack, 4,
			    pan);
		}
	}

	// Pitch
	f32 pitch;
	if (seParam->mPitch[7].mCurrentValue == -1.0f) {
		if (seParam->mPitchPointer != nullptr) {
			seParam->mPitch[0].mCurrentValue = *seParam->mPitchPointer;
		}
		pitch = 1.0f;
		for (int i = 0; i < 7; ++i)
			pitch *= seParam->mPitch[i].mCurrentValue;
	} else {
		pitch = seParam->mPitch[7].mCurrentValue;
	}
	if (slot->mPitch != pitch) {
		slot->mPitch = pitch;
		if (sound->mState != SOUNDSTATE_Prepared) {
			unk0->mSeqTrackInfo[mSeSequence->mTrack].mTrackUpdate[sound->mTrack]
			    |= 0x2;
			JAISystemInterface::setSeqPortargsF32(
			    &unk0->mSeqTrackInfo[mSeSequence->mTrack], sound->mTrack, 3,
			    pitch);
		}
	}

	// FxMix
	f32 fxmix;
	if (seParam->mFxmix[7].mCurrentValue == -1.0f) {
		if (seParam->mFxmixPointer != nullptr) {
			seParam->mFxmix[0].mCurrentValue = *seParam->mFxmixPointer;
		}
		fxmix = 0.0f;
		for (int i = 0; i < 7; ++i)
			fxmix += seParam->mFxmix[i].mCurrentValue;
	} else {
		fxmix = seParam->mFxmix[7].mCurrentValue;
	}
	if (slot->mFxmix != fxmix) {
		slot->mFxmix = fxmix;
		if (sound->mState != SOUNDSTATE_Prepared) {
			unk0->mSeqTrackInfo[mSeSequence->mTrack].mTrackUpdate[sound->mTrack]
			    |= 0x8;
			JAISystemInterface::setSeqPortargsF32(
			    &unk0->mSeqTrackInfo[mSeSequence->mTrack], sound->mTrack, 5,
			    fxmix);
		}
	}

	// Dolby
	f32 dolby;
	if (seParam->mDolby[7].mCurrentValue == -1.0f) {
		if (seParam->mDolbyPointer != nullptr) {
			seParam->mDolby[0].mCurrentValue = *seParam->mDolbyPointer;
		}
		f32 center = JAIGlobalParameter::seDolbyCenterValue / 127.0f;
		dolby      = 0.0f;
		for (int i = 0; i < 7; ++i)
			dolby += seParam->mDolby[i].mCurrentValue - center;
		dolby += center;
		if (dolby < 0.0f)
			dolby = 0.0f;
		else if (dolby > 1.0f)
			dolby = 1.0f;
	} else {
		dolby = seParam->mDolby[7].mCurrentValue;
	}
	if (slot->mDolby != dolby) {
		slot->mDolby = dolby;
		if (sound->mState != SOUNDSTATE_Prepared) {
			unk0->mSeqTrackInfo[mSeSequence->mTrack].mTrackUpdate[sound->mTrack]
			    |= 0x10;
			JAISystemInterface::setSeqPortargsF32(
			    &unk0->mSeqTrackInfo[mSeSequence->mTrack], sound->mTrack, 6,
			    dolby);
		}
	}

	// Final block: U32 param
	if (unk0->mSeqTrackInfo[mSeSequence->mTrack].mTrackUpdate[sound->mTrack]
	    != 0) {
		JAISystemInterface::setSeqPortargsU32(
		    &unk0->mSeqTrackInfo[mSeSequence->mTrack], sound->mTrack, 1,
		    unk0->mSeqTrackInfo[mSeSequence->mTrack]
		        .mTrackUpdate[sound->mTrack]);
		unk0->mSeqTrackInfo[mSeSequence->mTrack]
		    .mPlayerParams[sound->mTrack]
		    .mCmd.mHead
		    = nullptr;
		unk0->mSeqTrackInfo[mSeSequence->mTrack]
		    .mPlayerParams[sound->mTrack]
		    .mCmd.addPortCmdOnce();
	}
}

void JAIBasic::releaseSeRegist(JAISound* sound)
{
	if (sound->mState != SOUNDSTATE_Stored) {
		JAISystemInterface::writePortApp(
		    mSeSequence->getSeqParameter()->mSeqHandle,
		    (sound->mTrack >> 4) + 0x20000000 + ((sound->mTrack & 0xf) << 4),
		    0);
		mSeSequence->setTrackInterruptSwitch(sound->mTrack, 1);
	}

	clearSeqMuteFromSeStop(sound);

	u8 cat;
	u8 maxCount = unk0->mCategoryInfoTable[mSoundScene]
	                                      [(u8)sound->getSeCategoryNumber()]
	                                          .mMaxPlaying;
	cat = sound->getSeCategoryNumber();
	for (u8 j = 0; j < maxCount; ++j) {
		JAISound** slot = &unk0->mSeTrack[cat][j].mSound;
		if (*slot == sound) {
			*slot = nullptr;
			j     = maxCount;
		}
	}

	sound->clearMainSoundPPointer();
	sound->mState = SOUNDSTATE_Inactive;
	releaseSeParameterPointer(sound->getSeParameter());
	releaseControllerHandle(&unk0->mSeRegist[cat], sound);
}
