#include <MSound/MSSetSound.hpp>
#include <JSystem/JAudio/JAInterface/JAIConst.hpp>
#include <JSystem/JAudio/JALibrary/JALSystem.hpp>
#include <MSound/MSoundStruct.hpp>
#include <MSound/MSound.hpp>

// rogue
#include <MSound/MSoundBGM.hpp>

MSSetSound* MSSetSound::smSetSound[9];

void MSSetSound::init()
{
	smSetSound[0] = new MSSetSound(
	    MSD_SE_WT_GND, "放水着地音", 2, 7, 6, 4, 184.0f, 1, 22.12f, 150.0f,
	    0.85f, 0.937f, 295.2f, 0, 937.3f, 0xaf, 0.95f, 0.97f, 51.63f, false);

	smSetSound[1]
	    = new MSSetSound(MSD_SE_ERASE_SCRAWL, "落書き消し音", 2, 4, 7, 5,
	                     100.0f, 1, 11.0f, 17000.0f, 0.52f, 0.9f, 221.78f, 0x16,
	                     8000.0f, 0xfa, 0.81f, 1.35f, 153.73f, true);

	smSetSound[2]
	    = new MSSetSound(MSD_SE_HINOKURI_LIQUID_GND, "ヒノクリ汚染着地音", 2, 9,
	                     15, 18, 100.0f, 1, 44.0f, 3.0f, 1.0f, 1.0f, 0.0f, 0xf,
	                     200.0f, 0xb4, 1.0f, 1.0f, 0.0f, false);

	smSetSound[3] = new MSSetSound(MSD_SE_EF_FIRE, "火柱", 2, 9, 15, 18, 100.0f,
	                               1, 44.0f, 3.0f, 1.0f, 1.0f, 0.0f, 0xf,
	                               200.0f, 0xb4, 1.0f, 1.0f, 0.0f, false);

	smSetSound[4] = new MSSetSound(MSD_SE_EF_ELEC, "電気柱", 2, 9, 15, 18,
	                               100.0f, 1, 44.0f, 3.0f, 1.0f, 1.0f, 0.0f,
	                               0xf, 200.0f, 0xb4, 1.0f, 1.0f, 0.0f, false);

	smSetSound[5] = new MSSetSound(MSD_SE_PO_WT_DRY_UP, "水乾燥音", 2, 4, 1, 10,
	                               320.0f, 1, 44.0f, 3.0f, 1.0f, 1.0f, 0.0f,
	                               0xf, 200.0f, 0xb4, 1.0f, 1.0f, 0.0f, false);

	smSetSound[6]
	    = new MSSetSound(MSD_SE_EN_COMMON_W_HIT_OK, "水ヒットマーク", 2, 3, 10,
	                     2, 300.0f, 1, 22.12f, 150.0f, 0.85f, 0.937f, 295.2f, 0,
	                     937.3f, 0xaf, 0.95f, 0.97f, 51.63f, true);

	smSetSound[7]
	    = new MSSetSound(MSD_SE_WT_INTO_WATER, "放水着地音", 2, 7, 6, 4, 184.0f,
	                     1, 22.12f, 150.0f, 0.85f, 0.937f, 295.2f, 0, 937.3f,
	                     0xaf, 0.95f, 0.97f, 51.63f, false);

	smSetSound[8]
	    = new MSSetSound(MSD_SE_BS_MANTA_ATTACK, "マンタ襲撃声", 3, 4, 63, 4,
	                     184.0f, 1, 22.12f, 150.0f, 0.94f, 0.815f, 280.2f, 0,
	                     937.3f, 0xaf, 0.95f, 0.97f, 51.63f, false);
}

bool MSSetSound::startSoundSet(u32 param1, const Vec* param2, u32 param3,
                               f32 param4, u32 param5, u32 param6, u8 param7)
{
	if (!MSGMSound->gateCheck(param1))
		return false;

	MSSetSoundTL<MSSetSound>* which = nullptr;
	switch (param1) {
		// clang-format off
	case MSD_SE_WT_GND: which = smSetSound[0]; break;
    case MSD_SE_ERASE_SCRAWL: which = smSetSound[1]; break;
	case MSD_SE_HINOKURI_LIQUID_GND: which = smSetSound[2]; break;
	case MSD_SE_EF_FIRE: which = smSetSound[3]; break;
	case MSD_SE_EF_ELEC: which = smSetSound[4]; break;
	case MSD_SE_PO_WT_DRY_UP:  which = smSetSound[5]; break;
	case MSD_SE_EN_COMMON_W_HIT_OK: which = smSetSound[6]; break;
	case MSD_SE_WT_INTO_WATER: which = smSetSound[7]; break;
	case MSD_SE_BS_MANTA_ATTACK: which = smSetSound[8]; break;
		// clang-format on
	}

	if (which)
		return which->startSoundSetDyna(param1, param2, param3, param4, param5,
		                                param6, param7, nullptr);

	return false;
}

bool MSSetSoundGrp::startSoundSetGrp(u32 param1, const Vec* param2, u32 param3,
                                     f32 param4, u32 param5, u32 param6,
                                     u8 param7)
{
	if (!MSGMSound->gateCheck(param1))
		return false;

	MSSetSoundGrp* grp = MSSetSoundGrp::searchGroup(param1);

	if (grp)
		return grp->startSoundSetDyna(param1, param2, param3, param4, param5,
		                              param6, param7, grp);

	return false;
}

// Their original include structure was complete nonsense,
// so this shall live here for now to avoid circular includes.
//
// Also open in this TU, but not fixable from the .cpp: the MSSetSoundTL
// constructor's 104 bytes of dead low region, now measured and written up
// beside the constructor in MSound/MSSetSound.hpp. The 13 x 8 reading (one
// dead local per JADPrm ctor expansion) is ruled out: the JADPrm constructors
// are called out of line and are 8 bytes each in the map.
//
// TODO: 98.1%, both instantiations. `bVar1 += uVar5;` before the
// `getPlayGameFrameCounter()` read (closure batch 123) is a real fix -- it
// reproduces retail's in-place `add rX, rX, r3` and dropped the diff from 63
// to 49 `~|<>` markers -- but retail still gives the longer-lived `uVar7`
// (compared three times) the lower-numbered register and the shorter-lived
// `bVar1 + uVar5` the higher one; ours does the opposite. Reordering the two
// declarations (`uVar7` first) regresses badly (136 markers): the allocator
// is not simply following declaration order here. Also open: a 4-byte
// low-region gap under the first branch's `JAIActor local_94` temp, and a
// float-register permutation in the `unk28`/`unk2C`/`unk44..unk4C`
// linearTransform block. All register-permutation-class residue, not
// pursued further this batch.
//
// Closure batch 128 added negative results. The r23/r24 assignment is not a
// declaration-order property at all: splitting the declarations from the
// assignments (`u32 uVar7; u32 bVar1; ... uVar7 = ...;`, in either order, with
// the reads left exactly where they are) is byte-identical to the current
// form, so neither the batch-123 "reorder the declarations" trial nor the
// declare-before-the-call rule applies here. The other structural item is the
// duplicated `li r0, 1` at the end of the `bVar2` chain: retail reaches one
// shared `li r0, 1` both from `param_8 == nullptr` and from the
// `candidate->unk18` compare, and our ternary emits a second copy plus a
// branch. Every restructuring is worse, and all of them *add* instructions
// where the ternary keeps retail's 470: `param_8 == nullptr || (...)` as one
// boolean expression 96.0% (476 instructions), an `if`/`else if` chain 94.3%
// (480), swapping the outer ternary arms 98.1% (unchanged), spelling the
// innermost arm `!(uVar7 < candidate->unk18)` 97.9%. The ternary is right and
// the merge is an allocator/branch-folding difference downstream of the same
// register permutation. JADPrm<T>::get() already returns T by value, so the
// 4-byte gap under `JAIActor local_94` is not a reference-return inline temp
// (which is worth 4-8 bytes per expansion and is what landed camerashake's
// startShake/keepShake).
//
// 98.4%: the "Huh" else belongs to the outer `unk5C[unk59] != nullptr` test
// (retail's null-sound arm jumps straight to the end).
// 99.4%: the linearTransform argument loads are fixed by reading the
// last-loaded parameter raw (`unk30.unk0`, `unk34.unk0`, `unk48.unk0`,
// `unk4C.unk0`: an inline get() is a call and is generated first), and the
// 1.0f pairs by `f30 = f29 = 1.0f` (retail materialises f29, copies to f30).
// The raw unk1E read is what puts it first in the fmuls: a simple right
// operand moves left of the getRandom_0_1() call at parse, the accessor
// (a call there too) does not.
// 99.5% (c-k7): the restart test is the inline predicate below, which fixes
// r23/r24 and the first JAIActor's slot (21 -> 9 markers). Still open:
// - the shared `li r0, 1`: retail's `param_8 == nullptr` branch lands on the
//   innermost `true`; ours has its own `li r0, 1` plus a branch. The same
//   test written as `if` statements with a fall-through `return true` gives
//   exactly retail's blocks, but a statement context expands searchD. Worse:
//   `grp == nullptr || ...` with `!(<)`, `>=` or a ternary inside, `!(grp &&
//   (... || <))`, swapped outer arms, `!(<)` innermost (mfcr).
// - local_AC 4 high (0x88, retail 0x84): the named `bVar2` in the caller
//   is one of retail's two words above it (without it 8 high); a named
//   `result` in the predicate as well is frame +8.
// - the volume/pitch `snd` takes r23 (lowest in use) where retail has r24:
//   the second frame counter's IRO temp @803 holds r23 and is not its
//   neighbour; reusing `sound` for it is inert.

// Whether a sound already playing in this slot may be restarted: not before
// its minimum interval (plus the random shift) has run, not while the thinning
// window holds it and it is close enough, and not when the group has a member
// for it that asks for a longer wait. Parked here, not in MSSetSound.hpp: the
// map has no symbol for it. Written this way (c-k7) it gives retail's r23/r24
// for the frame counter and the interval and the first JAIActor's slot; the
// member lookup sits in a ternary arm, where MWCC does not expand a callee that
// has a loop, which is what keeps searchD out of line as in retail (the same
// test as `if` statements expands it: +10 instructions, frame +0x38).
template <typename T>
static inline bool MSSetSoundCanRestart(MSSetSoundTL<T>* tl, f32 dist,
                                        MSSetSoundGrp* grp)
{
	u32 bVar1 = tl->unk1D.get();
	u32 uVar5 = JALCalc::getRandom_0_1() * tl->unk1E.unk0;
	bVar1 += uVar5;
	u32 uVar7 = tl->unk5C[tl->unk5A]->getPlayGameFrameCounter();
	if (uVar7 < bVar1)
		return false;
	if (tl->unk24.get() == 1 && uVar7 < tl->unk1F.get()
	    && dist < tl->unk20.get())
		return false;

	MSSetSoundMember* candidate;
	return grp != nullptr
	          ? ((candidate = grp->searchD(tl->unk5C[tl->unk5A]->getID()))
	                     == nullptr
	                 ? false
	                 : (uVar7 < candidate->unk18 ? false : true))
	          : true;
}

template <typename T>
bool MSSetSoundTL<T>::startSoundSetDyna(u32 param_1, const Vec* param_2,
                                        u32 param_3, f32 param_4, u32 param_5,
                                        u32 fade, u8 camera_idx,
                                        MSSetSoundGrp* param_8)
{
	f32 dVar9 = param_4;

	if (unkB8 == 1)
		return true;

	f32 f31 = JALCalc::getDist(&unkAC, (Vec*)param_2);

	if (unk5C[unk5A] == nullptr) {
		f32 f29 = param_4;
		u32 r31 = param_1;

		switch (param_1) {
		case MSD_SE_ERASE_SCRAWL:
			if (unk5C[unk5A] != nullptr) {
				if ((f32)unk5C[unk5A]->getPlayGameFrameCounter()
				        < (f32)unk3C.get()
				    && f31 < unk40.get())
					r31 = MSD_SE_ERASE_SCRAWL_CONT;
			}
			break;
		case MSD_SE_WT_GND:
			f29 = MSGMSound->getDistFromCamera((Vec*)param_2);
			break;
		}

		const Vec* ptr;
		if (unkB9 == 1) {
			unk70[unk59] = *param_2;
			ptr          = &unk70[unk59];
		} else {
			ptr = nullptr;
		}

		if (ptr == nullptr)
			ptr = param_2;

		JAIActor local_94(ptr, ptr, ptr, param_5);
		MSoundSESystem::MSoundSE::startSoundActorInner(
		    r31, &unk5C[unk59], &local_94, fade, camera_idx);
		JALSystem::processModFunc(unk5C[unk59], f29, 0, 3);

		unkAC = *param_2;
		unkB8 = 1;
	} else {
		bool bVar2 = MSSetSoundCanRestart(this, f31, param_8);
		if (!bVar2)
			return true;

		f32 f29 = param_4;
		u32 r26 = param_1;
		switch (param_1) {
		case MSD_SE_ERASE_SCRAWL:
			if (unk5C[unk5A] != nullptr) {
				if ((f32)unk5C[unk5A]->getPlayGameFrameCounter()
				        < (f32)unk3C.get()
				    && f31 < unk40.get()) {
					r26 = MSD_SE_ERASE_SCRAWL_CONT;
				}
			}
			break;
		case MSD_SE_WT_GND:
			f29 = MSGMSound->getDistFromCamera((Vec*)param_2);
			break;
		}

		const Vec* ptr;
		if (unkB9 == 1) {
			unk70[unk59] = *param_2;
			ptr          = &unk70[unk59];
		} else {
			ptr = nullptr;
		}

		if (ptr == nullptr)
			ptr = param_2;

		JAIActor local_AC(ptr, ptr, ptr, param_5);
		MSoundSESystem::MSoundSE::startSoundActorInner(
		    r26, &unk5C[unk59], &local_AC, fade, camera_idx);
		JALSystem::processModFunc(unk5C[unk59], f29, 0, 3);

		unkAC = *param_2;
		unkB8 = 1;

		if (unk5C[unk59] != nullptr) {
			JAISound* sound = unk5C[unk5A];
			if (sound != nullptr) {
				u32 uVar7 = sound->getPlayGameFrameCounter();
				f32 f30;
				f32 f29;
				f30 = f29 = 1.0f;

				f32 f1 = unk2C.get();
				if ((f32)uVar7 < (f32)unk28.get() && f31 < f1) {
					JALCalc::linearTransform(uVar7, unk1D.get(), unk28.get(),
					                         unk38.get(), 0.0f, false);
					f30 = JALCalc::linearTransform(uVar7, unk1D.get(),
					                               unk28.get(), unk30.unk0,
					                               1.0f, false);
					f29 = JALCalc::linearTransform(uVar7, unk1D.get(),
					                               unk28.get(), 1.0f,
					                               unk34.unk0, false);
				}

				f32 f28;
				f32 f27;
				f28 = f27 = 1.0f;
				if ((f32)uVar7 < (f32)unk3C.get() && f31 < unk40.get()) {
					unk58 = 1;
					JALCalc::linearTransform((f32)unk54, 0.0f, unk44.get(),
					                         0.0f, unk50.get(), false);
					f27 = JALCalc::linearTransform((f32)unk54, 0.0f,
					                               unk44.get(), 1.0f,
					                               unk48.unk0, false);
					f28 = JALCalc::linearTransform((f32)unk54, 0.0f,
					                               unk44.get(), 1.0f,
					                               unk4C.unk0, false);
				} else {
					unk58 = 0;
					unk54 = 0;
				}
				JAISound* snd = unk5C[unk59];
				snd->setVolume(f30 * f27, 3, 0);
				snd->setPitch(f29 * f28, 3, 0);
			}
		} else {
			// Huh.
			if (unk5C[unk59] != nullptr)
				unk5C[unk59]->setPortData(13, 1);
			unk58 = 0;
			unk54 = 0;
		}
	}

	unk59 = unk59 == unk1C.get() - 1 ? 0 : unk59 + 1;
	unk5A = unk5A == unk1C.get() - 1 ? 0 : unk5A + 1;
	return true;
}
