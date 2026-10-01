#include <GC2D/SelectShine2.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <System/Application.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DDrawBuffer.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DMaterialAnm.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DAnmLoader.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/JGeometry/JGVec2.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <stdlib.h>
#include <math.h>
#include <new>
#include <dolphin/mtx.h>

// The screen-space anchor the shine icons are laid out around.  The
// original gives it a real (dynamic) initialiser, which is what makes
// __sinit_SelectShine2_cpp exist at all.
JGeometry::TVec3<f32> TSelectShineManager::cCenter(300.0f, 160.0f, -9000.0f);

// The J3DModelLoaderDataBase::load() flag word for both shine models. The
// ROM materialises it with a single `lis r4, 0x5104`, i.e. the 32-bit value
// 0x51040000 (a value above 0x7fff cannot come out of an `li`), which
// decomposes into the J3DMLF_* bits below.
static const u32 kShineModelFlags
    = J3DMLF_MaterialColorLightOn | J3DMLF_MaterialPEFull
      | J3DMLF_MaterialTexGenFull | J3DMLF_MaterialUseIndirect
      | (4 << J3DMLF_TevStageNumShift);

// NOTE on definition order: this TU is compiled with -inline deferred, so
// the functions are emitted in the REVERSE of the order below.  The order
// itself is fixed by mario.MAP (.text section layout for SelectShine2.cpp):
//   __dt__12TSelectShine / makeNewPosition / move / __ct__12TSelectShine /
//   perform / startDecrease / startIncrease / startClose / getPosition /
//   getAngle / initData / __ct__19TSelectShineManager /
//   __dt__19TSelectShineManager
// The two destructors are weak symbols in the map, so they are defined
// inline in the header and are not repeated here.

TSelectShineManager::TSelectShineManager(const char* name)
    : JDrama::TViewObj(name)
    , mDrawBuffer0(nullptr)
    , mDrawBuffer1(nullptr)
    , unk78(0.0f)
    , unk7c(0.0f)
    , unk88(0)
    , unk90(0.0f)
    , unk94(0.0f)
    , unk98(0)
    , unk9c(0)
    , unka0(0.0f)
    , unka4(0)
    , unka5(0)
    , unka6(0)
    , unka7(0)
{
}

void TSelectShineManager::initData(u8* shineTypes, u8 shineCount,
                                   u8 selectedIndex,
                                   JPAEmitterManager* emitterManager)
{
	// The shine selector owns both draw buffers and installs them in j3dSys.
	mDrawBuffer0 = new J3DDrawBuffer(0x400);
	j3dSys.setDrawBuffer(mDrawBuffer0, 0);
	mDrawBuffer1 = new J3DDrawBuffer(0x400);
	j3dSys.setDrawBuffer(mDrawBuffer1, 1);

	// The ROM looks both resources up before it loads either of them, so
	// the two getGlbResource() calls are hoisted into locals here.
	const void* normalModelRes
	    = JKRFileLoader::getGlbResource("/select/shine_menu.bmd");
	const void* normalAnmRes
	    = JKRFileLoader::getGlbResource("/select/shine_menu.bpk");
	J3DModelData* normalModel
	    = J3DModelLoaderDataBase::load(normalModelRes, kShineModelFlags);
	J3DAnmColor* normalAnimation = static_cast<J3DAnmColor*>(
	    J3DAnmLoaderDataBase::load(normalAnmRes));
	normalAnimation->searchUpdateMaterialID(normalModel);
	for (u16 i = 0; i < normalModel->mMaterialNum; ++i) {
		J3DMaterialAnm* materialAnimation = new J3DMaterialAnm;
		J3DMaterial* material = normalModel->mMaterials[i];
		material->change();
		// The ROM reloads the material pointer after change(): a call
		// invalidates the load, and the original plainly re-reads
		// mMaterials[i] instead of reusing the value in a register.
		normalModel->mMaterials[i]->setMaterialAnm(materialAnimation);
	}
	normalModel->entryMatColorAnimator(normalAnimation);

	const void* emptyModelRes
	    = JKRFileLoader::getGlbResource("/select/shine_menu_empty.bmd");
	const void* emptyAnmRes
	    = JKRFileLoader::getGlbResource("/select/shine_menu_empty.bpk");
	J3DModelData* emptyModel
	    = J3DModelLoaderDataBase::load(emptyModelRes, kShineModelFlags);
	J3DAnmColor* emptyAnimation = static_cast<J3DAnmColor*>(
	    J3DAnmLoaderDataBase::load(emptyAnmRes));
	emptyAnimation->searchUpdateMaterialID(emptyModel);
	// NOTE: the second loop tests normalModel->mMaterialNum, not
	// emptyModel's - the ROM really does read the first model's count.
	for (u16 i = 0; i < normalModel->mMaterialNum; ++i) {
		J3DMaterialAnm* materialAnimation = new J3DMaterialAnm;
		J3DMaterial* material = emptyModel->mMaterials[i];
		material->change();
		emptyModel->mMaterials[i]->setMaterialAnm(materialAnimation);
	}
	emptyModel->entryMatColorAnimator(emptyAnimation);

	// TODO: the two u8 arguments are named from their use only: the second
	// one becomes unk88 (the shine count that perform() loops over) and the
	// third one mCurIndex (the shine the cursor starts on). The ROM does
	// unk9c = mCurIndex * -40, not -shineCount * 40, which is what settles
	// the order.
	unk88 = shineCount;
	mCurIndex = selectedIndex;
	unk9c = mCurIndex * -0x28;

	for (int i = 0; i < 8; ++i) {
		const s16 angle = static_cast<s16>(unk9c + i * 0x28);
		const s32 trigAngle = static_cast<s32>(57.295776f * angle);
		const u32 trigIndex = static_cast<u16>(trigAngle) >> jmaSinShift;

		JGeometry::TVec3<f32> pos(cCenter.x
		                             + 1500.0f * jmaSinTable[trigIndex],
		                          cCenter.y,
		                          cCenter.z
		                              + 9000.0f * jmaCosTable[trigIndex]);

		JGeometry::TVec2<f32> diff(300.0f - pos.x, 1300.0f - pos.z);
		// NOTE: reconstructed zero-angle rotate(); see the identical site
		// in perform() for the full story, including the measurement that
		// spelling the multiplies out by hand regresses.
		diff.rotate(0.0f);
		s32 facing = static_cast<s32>(fabsf(atan2f(diff.x, diff.y))
		                              * 57.295776f);
		// NOTE: see the angle flip in perform() - the ROM uses mulli here.
		if (pos.x > cCenter.x)
			facing *= -1;

		const u8 type = shineTypes[i];
		if (type == 3) {
			TSelectShine* shine = static_cast<TSelectShine*>(
			    ::operator new(sizeof(TSelectShine)));
			// NOTE: the placement-new result is deliberately not assigned
			// back to shine. Doing so makes MWCC emit a second null test in
			// front of the constructor call, which the ROM does not have -
			// the ROM has exactly one, right after operator new.
			if (shine != nullptr) {
				const s32 randomValue = rand();
				const f32 randomUnit
				    = static_cast<f32>(randomValue) * 0.000030517578f;
				const s32 randomScaled
				    = static_cast<s32>(randomUnit * 4000.0f);
				const f32 phase = static_cast<f32>(randomScaled) / 1000.0f;
				// NOTE: an explicit constructor call rather than
				// placement new: the latter makes MWCC emit a null test in
				// front of the constructor call that the ROM does not have.
				shine->TSelectShine(normalModel, normalAnimation,
				                    emitterManager, pos,
				                    static_cast<s16>(facing), 0, phase,
				                    0.01f, 10.0f);
			}
			mSelectShines[i] = shine;
		} else if (type == 1 || type == 2) {
			TSelectShine* shine = static_cast<TSelectShine*>(
			    ::operator new(sizeof(TSelectShine)));
			if (shine != nullptr) {
				const s32 randomValue = rand();
				const f32 randomUnit
				    = static_cast<f32>(randomValue) * 0.000030517578f;
				const s32 randomScaled
				    = static_cast<s32>(randomUnit * 4000.0f);
				const f32 phase = static_cast<f32>(randomScaled) / 1000.0f;
				// NOTE: see the type == 3 branch above.
				shine->TSelectShine(emptyModel, emptyAnimation,
				                    emitterManager, pos,
				                    static_cast<s16>(facing), 1, phase,
				                    0.01f, 10.0f);
			}
			mSelectShines[i] = shine;
		} else {
			mSelectShines[i] = nullptr;
		}
	}
	// NOTE: the loop above iterates over all 8 slots even though only unk88
	// of them are ever used; the ROM does the same.

	TSelectShine* selected = mSelectShines[mCurIndex];
	if (selected->unk4a != 2) {
		if (selected->unk4a == 0) {
			selected->mEmitter1->clearStatus(1);
			selected->mEmitter0->clearStatus(1);
		}
		selected->mEmitter2->clearStatus(1);
	}
}

// NOTE: getAngle() and getPosition() are declared in SelectShine2.hpp but
// deliberately have no definition here. mario.MAP lists both as UNUSED
// (getAngle 0x88 bytes, getPosition 0x8c), i.e. the retail linker garbage
// collected them, and nothing in src/ calls either - so defining them only
// added 300 B of dead code that objdiff reports as `extra`. The shapes are
// preserved here as comments because they were the source of the inline
// reconstructions in perform() and initData().
//
//   getAngle(const TVec3<f32>& pos)            // was 0x88 bytes
//     TVec2<f32> diff(300.0f, 1300.0f);
//     diff.sub(TVec2<f32>(pos.x, pos.z));
//     s32 angle = (s32)(fabsf(atan2f(diff.x, diff.y)) * 57.295776f);
//     if (pos.x > cCenter.x) angle = -angle;
//     unk94 = (f32)angle;
//
//   getPosition(s16 angle)                     // was 0x8c bytes
//     s32 trigAngle = (s32)(57.295776f * angle);
//     u32 trigIndex = (u16)trigAngle >> jmaSinShift;
//     unka8[0].set(cCenter.x + 1500.0f * jmaSinTable[trigIndex],
//                  cCenter.y,
//                  cCenter.z + 9000.0f * jmaCosTable[trigIndex]);

void TSelectShineManager::startClose()
{
	TSelectShine* current
	    = mSelectShines[mCurIndex];
	current->unk24 = 0;

	for (int i = 0; i < 8; ++i) {
		TSelectShine* shine
		    = mSelectShines[i];
		if (shine != nullptr && i != mCurIndex && shine->unk49 == 0) {
			shine->unk49 = 1;
			shine->unk48 = 0;
		}
	}

	unka7 = 1;
}

void TSelectShineManager::startIncrease(int amount)
{
	TSelectShine* current
	    = mSelectShines[mCurIndex];
	if (current->unk4a != 2) {
		if (current->unk4a == 0) {
			current->mEmitter1->setStatus(1);
			current->mEmitter0->setStatus(1);
		}
		current->mEmitter2->setStatus(1);
	}

	unka4 = 1;
	unka0 = static_cast<f32>((amount * -0x28) / 10);
	mCurIndex += amount;

	TSelectShine* next
	    = mSelectShines[mCurIndex];
	if (next->unk4a != 2) {
		if (next->unk4a == 0) {
			next->mEmitter1->clearStatus(1);
			next->mEmitter0->clearStatus(1);
		}
		next->mEmitter2->clearStatus(1);
	}
}

void TSelectShineManager::startDecrease(int amount)
{
	TSelectShine* current
	    = mSelectShines[mCurIndex];
	if (current->unk4a != 2) {
		if (current->unk4a == 0) {
			current->mEmitter1->setStatus(1);
			current->mEmitter0->setStatus(1);
		}
		current->mEmitter2->setStatus(1);
	}

	unka5 = 1;
	unka0 = static_cast<f32>((amount * 0x28) / 10);
	mCurIndex -= amount;

	TSelectShine* next
	    = mSelectShines[mCurIndex];
	if (next->unk4a != 2) {
		if (next->unk4a == 0) {
			next->mEmitter1->clearStatus(1);
			next->mEmitter0->clearStatus(1);
		}
		next->mEmitter2->clearStatus(1);
	}
}

// cue bit 0 advances the animation of every shine, bit 1 pushes the
// animation frame and refreshes the models and bit 3 draws them.
// NOTE: bit 3, not bit 2. The ROM's third gate is
// `rlwinm. r0, r26, 0, 28, 28` (0x8016E524), and this compiler maps
// `cue & 2` to mb=30 / `cue & 4` to mb=29, so mb=28 is `cue & 8`.
void TSelectShineManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & 1) {
		for (int i = 0; i < static_cast<int>(unk88); ++i)
			mSelectShines[i]->move();
	}

	if (unka4 != 0 || unka5 != 0)
		unk9c = static_cast<s32>(static_cast<f32>(unk9c) + unka0);

	if (unka4 != 0) {
		const s32 limit = mCurIndex * -0x28;
		if (unk9c < limit) {
			unk9c = limit;
			unka4 = 0;
		}
	}

	if (unka5 != 0) {
		const s32 limit = mCurIndex * -0x28;
		if (unk9c > limit) {
			unk9c = limit;
			unka5 = 0;
		}
	}

	for (int i = 0; i < static_cast<int>(unk88); ++i) {
		TSelectShine* shine = mSelectShines[i];

		const s32 deg = static_cast<s32>(static_cast<f32>(static_cast<s16>(
		                                      unk9c + i * 0x28))
		                          * 57.295776f);
		JGeometry::TVec3<f32> pos;
		pos.set(cCenter.x + 1500.0f * JMASSin(deg), cCenter.y,
		        cCenter.z + 9000.0f * JMASCos(deg));
		shine->mPos = pos;

		// the angle is measured from the world position, i.e. after the
		// per-shine offset has been added in
		JGeometry::TVec3<f32> worldPos(shine->mPos);
		worldPos.add(shine->unk18);

		JGeometry::TVec2<f32> diff(300.0f, 1300.0f);
		diff.sub(JGeometry::TVec2<f32>(worldPos.x, worldPos.z));
		// NOTE: reconstructed. The ROM evaluates atan2f() on
		//   (x*1.0f - y*0.0f, x*0.0f + y*1.0f)
		// i.e. a rotate() by a zero angle, with the 1.0f/0.0f hoisted out of
		// the loop into f30/f31. That is the documented MWCC signature of a
		// multiplication by zero/one that arrives through an inlined
		// function, which is why it is spelled this way here. TODO: the
		// original probably rotated by a real angle that this call site
		// never needed; the 0.0f here is fabricated.
		// MEASURED: spelling the four multiplies out by hand instead (with
		// cosTheta/sinTheta as locals) regresses the unit 82.53 % -> 81.77 %
		// and both perform() and initData(), because MWCC folds the literal
		// 1.0f/0.0f pair away. The cosf()/sinf() call pair it emits is the
		// price of the header version; see JGVec2.hpp::rotate.
		diff.rotate(0.0f);
		s32 angle = static_cast<s32>(fabsf(atan2f(diff.x, diff.y))
		                             * 57.295776f);
		// NOTE: spelled as a *= -1 rather than a = -a because the ROM
		// materialises the flip with `mulli rX, rX, -1`, not with `neg`.
		if (pos.x > cCenter.x)
			angle *= -1;

		Mtx rotation;
		PSMTXRotRad(rotation, 'y', static_cast<f32>(static_cast<s16>(angle)
		                                             - shine->unk3a)
		                               * 0.017453292f);
		PSMTXConcat(shine->mModel->getBaseTRMtx(), rotation,
		            shine->mModel->getBaseTRMtx());
		shine->unk3a = static_cast<s16>(angle);
	}

	if (cue & 2) {
		for (int i = 0; i < static_cast<int>(unk88); ++i) {
			TSelectShine* shine = mSelectShines[i];
			shine->mAnmColor->setFrame(static_cast<f32>(shine->unk3c));
			shine->mModel->update();
			shine->mModel->viewCalc();
		}
	}

	if (cue & 8) {
		PSMTXCopy(graphics->mViewMtx, j3dSys.mViewMtx);
		j3dSys.drawInit();
		j3dSys.unk4C = 3;
		mDrawBuffer0->draw();
		j3dSys.unk50 = 4;
		mDrawBuffer1->draw();
		mDrawBuffer0->frameInit();
		mDrawBuffer1->frameInit();
	}
}

TSelectShine::TSelectShine(J3DModelData* modelData, J3DAnmColor* anmColor,
                           JPAEmitterManager* emitterManager,
                           JGeometry::TVec3<f32>& pos, s16 param5, u8 param6,
                           f32 param7, f32 param8, f32 param9)
{
	unk24 = 0;
	unk28 = 0.0f;
	unk2c = 0.0f;
	unk30 = 0.0f;
	unk34 = 0;
	unk38 = 0;
	unk3a = param5;
	unk3c = 0;
	unk3e = 0;
	unk40 = 0.0f;
	unk44 = 0.0f;
	unk48 = 0;
	unk49 = 0;
	unk4a = 0;
	mEmitterManager = nullptr;
	mEmitter0 = nullptr;
	mEmitter1 = nullptr;
	mEmitter2 = nullptr;

	mModel = new J3DModel(modelData, 0, 1);
	mAnmColor = anmColor;
	mPos.x = pos.x;
	mPos.y = pos.y;
	mPos.z = pos.z;
	unk18.set(0.0f, 0.0f, 0.0f);
	mEmitterManager = emitterManager;
	unk30 = param8;
	unk2c = param9;
	unk28 = param7;
	unk38 = 3;
	MtxPtr baseMtx = mModel->getBaseTRMtx();
	baseMtx[0][3] = mPos.x;
	baseMtx[1][3] = mPos.y;
	baseMtx[2][3] = mPos.z;

	Mtx rotation;
	PSMTXRotRad(rotation, 'y', static_cast<f32>(unk3a) * 0.017453292f);
	PSMTXConcat(baseMtx, rotation, baseMtx);

	unk4a = param6;
	// NOTE: the ROM tests unk4a against 2 twice - once here, and once more
	// after the emitters have been created. The second test is dead (the
	// first one already returned) but it is really in the binary. Both are
	// spelled `!= 2 { ... }` rather than `== 2 return;` because that is what
	// the ROM branches on: `cmplwi rX, 2` followed by a `beq` to the single
	// shared exit, which also collapses the two `mr r31, r3`-style returns
	// the early-return spelling needs into one.
	if (unk4a != 2) {
		JGeometry::TVec3<f32> emitterPos(mPos);
		emitterPos.add(unk18);
		if (unk4a == 0) {
			mEmitterManager->createEmitter(emitterPos, 0, nullptr, nullptr);
			mEmitter0 = mEmitterManager->unkC8[0][0];
			mEmitterManager->createEmitter(emitterPos, 1, nullptr,
			                               nullptr);
			mEmitter1 = mEmitterManager->unkC8[0][0];
			mEmitterManager->createEmitter(emitterPos, 2, nullptr,
			                               nullptr);
			mEmitter2 = mEmitterManager->unkC8[0][0];
		} else {
			mEmitterManager->createEmitter(emitterPos, 3, nullptr,
			                               nullptr);
		}
		mEmitter2 = mEmitterManager->unkC8[0][0];
		if (unk4a != 2) {
			if (unk4a == 0) {
				mEmitter1->setStatus(JPABaseEmitter::STATUS_STOP_EMIT);
				mEmitter0->setStatus(JPABaseEmitter::STATUS_STOP_EMIT);
			}
			mEmitter2->setStatus(JPABaseEmitter::STATUS_STOP_EMIT);
		}
	}
}

void TSelectShine::move()
{
	// TODO: the four easing branches below are reconstructed from the ROM.
	// Each one really contains a "0.0f *" product in the original binary.
	// That constant is real: SelectShine2.cpp's .sdata2 holds a 0.0f at
	// 0x80409968+0x8 (read out of the retail build/GMSP01/mario.dol), and
	// move() is what loads it. But MWCC folds the product away, costing
	// THREE instructions per branch - the `lfs` of the 0.0f, the
	// `fmuls` that forms remainSquared/tSquared for it, and the `fmadds`
	// that adds it - so ~48 B of this function is unreachable from source.
	// MEASURED: routing the literal through a named `static const f32
	// kEaseZero = 0.0f` is inert; MWCC propagates it just the same.
	const f32 phase = unk28;
	f32 eased = unk18.y;
	if (phase < 1.0f) {
		const f32 remain = 1.0f - phase;
		f32 cross = 2.0f * remain;
		const f32 phaseSquared = phase * phase;
		f32 deltaScaled = unk2c * 0.9f;
		const f32 remainSquared = remain * remain;
		cross *= phase;
		cross *= deltaScaled;
		cross += 0.0f * remainSquared;
		cross += unk2c * phaseSquared;
		eased = cross;
	} else if (phase < 2.0f) {
		const f32 t = phase - 1.0f;
		const f32 remain = 1.0f - t;
		f32 cross = 2.0f * remain;
		const f32 tSquared = t * t;
		f32 deltaScaled = unk2c * 0.9f;
		const f32 remainSquared = remain * remain;
		cross *= t;
		cross *= deltaScaled;
		cross += unk2c * remainSquared;
		cross += 0.0f * tSquared;
		eased = cross;
	} else if (phase < 3.0f) {
		const f32 t = phase - 2.0f;
		const f32 remain = unk2c - t;
		const f32 negativeVelocity = -unk2c;
		f32 cross = 2.0f * remain;
		const f32 tSquared = t * t;
		f32 deltaScaled = negativeVelocity * 0.9f;
		const f32 remainSquared = remain * remain;
		cross *= t;
		cross *= deltaScaled;
		cross += 0.0f * remainSquared;
		cross += negativeVelocity * tSquared;
		eased = cross;
	} else if (phase < 4.0f) {
		const f32 t = phase - 3.0f;
		const f32 remain = unk2c - t;
		const f32 negativeVelocity = -unk2c;
		f32 cross = 2.0f * remain;
		const f32 tSquared = t * t;
		f32 deltaScaled = negativeVelocity * 0.9f;
		const f32 remainSquared = remain * remain;
		cross *= t;
		cross *= deltaScaled;
		cross += negativeVelocity * remainSquared;
		cross += 0.0f * tSquared;
		eased = cross;
	}
	unk18.y = eased;

	MtxPtr baseMtx = mModel->getBaseTRMtx();
	JGeometry::TVec3<f32> translated(mPos);
	translated.add(unk18);
	baseMtx[0][3] = translated.x;
	baseMtx[1][3] = translated.y;
	baseMtx[2][3] = translated.z;

	s32 target = static_cast<s16>(translated.z * -0.05f);
	if (static_cast<s16>(translated.z * -0.05f) < 30)
		target = 30;

	if (unk48 != 0) {
		unk3c -= 8;
		if (unk3c < target)
			unk3c = target;
	} else if (unk49 != 0) {
		unk3c += 8;
		const s16 lastFrame = mAnmColor->getFrameMax() - 1;
		if (unk3c > lastFrame)
			unk3c = lastFrame;
	} else {
		unk3c = target;
		if (unk3c < 0)
			unk3c = 0;
		const s16 lastFrame = mAnmColor->getFrameMax();
		if (unk3c > lastFrame)
			unk3c = lastFrame;
	}

	const s16 maxFrame = mAnmColor->getFrameMax();
	const s32 alpha
	    = static_cast<s32>((1.0f - static_cast<f32>(unk3c / maxFrame))
	                       * 255.0f);
	if (unk4a != 2) {
		if (unk4a == 0) {
			mEmitter0->setGlobalRTMatrix(baseMtx);
			mEmitter1->setGlobalRTMatrix(baseMtx);
			mEmitter0->setGlobalAlpha(static_cast<u8>(alpha));
			mEmitter1->setGlobalAlpha(static_cast<u8>(alpha));
		}
		mEmitter2->setGlobalRTMatrix(baseMtx);
		mEmitter2->setGlobalAlpha(static_cast<u8>(alpha));
	}

	if (unk24 != 0) {
		const f32 frameRate = SMSGetAnmFrameRate();
		const f32 angleStep = static_cast<f32>(static_cast<s8>(unk38)) * frameRate;
		Mtx rotation;
		PSMTXRotRad(rotation, 'y', angleStep * 0.017453292f);

		const f32 accumulated
		    = static_cast<f32>(unk34)
		      + static_cast<f32>(static_cast<s8>(unk38)) * SMSGetAnmFrameRate();
		unk34 = static_cast<s32>(accumulated);
		if (unk34 > 360)
			unk34 -= 360;
		if (unk34 < 0)
			unk34 += 360;
		PSMTXConcat(baseMtx, rotation, baseMtx);
	} else if (unk34 != 0) {
		const s32 delta = static_cast<s16>(
		    static_cast<s32>(static_cast<f32>(static_cast<s8>(unk38)) * SMSGetAnmFrameRate()));
		const s32 accumulated = unk34 + delta;
		unk34 = accumulated;
		s32 angle = delta;
		if (unk34 > 360) {
			angle = delta - unk34 + 360;
			unk34 = 0;
		}

		Mtx rotation;
		PSMTXRotRad(rotation, 'y', static_cast<f32>(static_cast<s16>(angle))
		                               * 0.017453292f);
		PSMTXConcat(baseMtx, rotation, baseMtx);
	}

	unk28 += unk30;
	if (unk28 > 4.0f)
		unk28 = 0.0f;
}

// NOTE: makeNewPosition() is declared in SelectShine2.hpp but deliberately has
// no definition here, for the same reason as getAngle()/getPosition() above:
// mario.MAP lists it UNUSED at 0x2c bytes, nothing in src/ calls it, and
// defining it only added 20 B of dead `extra` code. Its recovered shape, whose
// inline site is the easing expression in move(), was:
//
//   unk18.y = param_1 * param_2 * param_3 * param_4;
