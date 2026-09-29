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
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <stdlib.h>
#include <math.h>
#include <new>
#include <dolphin/mtx.h>

JGeometry::TVec3<f32> TSelectShineManager::cCenter;

static inline f32 calcBezier(f32 t, f32 p0, f32 p1, f32 p2)
{
	f32 omt = 1.0f - t;
	return p0 * (omt * omt) + p1 * (2.0f * omt * t) + p2 * (t * t);
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
	if (unk4a != 2) {
		JGeometry::TVec3<f32> emitterPos(mPos);
		emitterPos.add(unk18);
		if (unk4a == 0) {
			mEmitterManager->createEmitter(emitterPos, 0, nullptr, nullptr);
			mEmitter0 = mEmitterManager->unkC8[0][0];
			mEmitterManager->createEmitter(emitterPos, 1, nullptr, nullptr);
			mEmitter1 = mEmitterManager->unkC8[0][0];
			mEmitterManager->createEmitter(emitterPos, 2, nullptr, nullptr);
		} else {
			mEmitterManager->createEmitter(emitterPos, 3, nullptr, nullptr);
		}
		mEmitter2 = mEmitterManager->unkC8[0][0];
		if (unk4a == 0) {
			mEmitter1->setStatus(JPABaseEmitter::STATUS_STOP_EMIT);
			mEmitter0->setStatus(JPABaseEmitter::STATUS_STOP_EMIT);
		}
		mEmitter2->setStatus(JPABaseEmitter::STATUS_STOP_EMIT);
	}
}

TSelectShineManager::~TSelectShineManager() { }

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

TSelectShine::~TSelectShine() { }

void TSelectShine::move()
{
	f32 t = unk28;
	f32 splineY;
	if (t < 1.0f) {
		f32 amp  = unk2c;
		f32 amp9 = amp * 0.9f;
		splineY  = calcBezier(t, 0.0f, amp9, amp);
	} else if (t < 2.0f) {
		f32 u    = t - 1.0f;
		f32 amp  = unk2c;
		f32 amp9 = amp * 0.9f;
		splineY  = calcBezier(u, amp, amp9, 0.0f);
	} else if (t < 3.0f) {
		f32 u    = t - 2.0f;
		f32 amp  = -unk2c;
		f32 amp9 = amp * 0.9f;
		splineY  = calcBezier(u, 0.0f, amp9, amp);
	} else if (t < 4.0f) {
		f32 u    = t - 3.0f;
		f32 amp  = -unk2c;
		f32 amp9 = amp * 0.9f;
		splineY  = calcBezier(u, amp, amp9, 0.0f);
	} else {
		splineY = unk18.y;
	}
	unk18.y = splineY;

	MtxPtr modelMtx = mModel->getBaseTRMtx();
	JGeometry::TVec3<f32> world;
	world = mPos + unk18;
	modelMtx[0][3] = world.x;
	modelMtx[1][3] = world.y;
	modelMtx[2][3] = world.z;

	s32 capped = static_cast<s32>(-0.05f * world.z);
	s16 cap16  = static_cast<s16>(capped);
	if (cap16 < 30)
		capped = 30;

	if (unk48 != 0) {
		unk3c = static_cast<s16>(unk3c - 8);
		if (unk3c < static_cast<s16>(capped))
			unk3c = static_cast<s16>(capped);
	} else if (unk49 != 0) {
		unk3c = static_cast<s16>(unk3c + 8);
		s32 maxFrame = mAnmColor->getFrameMax() - 1;
		if (unk3c > maxFrame)
			unk3c = maxFrame;
	} else {
		unk3c = static_cast<s16>(capped);
		if (static_cast<s16>(capped) < 0)
			unk3c = 0;
		s16 maxFrame = mAnmColor->getFrameMax();
		if (static_cast<s16>(capped) > maxFrame)
			unk3c = maxFrame;
	}

	s32 alpha = static_cast<s32>(
	    255.0f * (1.0f - static_cast<f32>(unk3c / mAnmColor->getFrameMax())));

	if (unk4a != 2) {
		if (unk4a == 0) {
			mEmitter0->setGlobalRTMatrix(modelMtx);
			mEmitter1->setGlobalRTMatrix(modelMtx);
			mEmitter0->setGlobalAlpha(static_cast<u8>(alpha));
			mEmitter1->setGlobalAlpha(static_cast<u8>(alpha));
		}
		mEmitter2->setGlobalRTMatrix(modelMtx);
		mEmitter2->setGlobalAlpha(static_cast<u8>(alpha));
	}

	if (unk24 != 0) {
		f32 rate = SMSGetAnmFrameRate();
		Mtx rotMtx;
		PSMTXRotRad(rotMtx, 'y', 0.017453292f * static_cast<f32>(unk38) * rate);

		f32 rate2 = SMSGetAnmFrameRate();
		unk34 = static_cast<s32>(
		    static_cast<f32>(unk34) + static_cast<f32>(unk38) * rate2);
		if (unk34 > 360)
			unk34 -= 360;
		if (unk34 < 0)
			unk34 += 360;
		PSMTXConcat(modelMtx, rotMtx, modelMtx);
	} else if (unk34 != 0) {
		f32 rate = SMSGetAnmFrameRate();
		s16 advance = static_cast<s16>(static_cast<f32>(unk38) * rate);
		unk34 += advance;
		if (unk34 > 360) {
			advance = static_cast<s16>(advance - unk34 + 360);
			unk34 = 0;
		}
		Mtx rotMtx;
		PSMTXRotRad(rotMtx, 'y', 0.017453292f * static_cast<f32>(advance));
		PSMTXConcat(modelMtx, rotMtx, modelMtx);
	}

	unk28 = unk28 + unk30;
	if (unk28 > 4.0f)
		unk28 = 0.0f;
}

void TSelectShineManager::initData(u8* shineTypes, u8 selectedIndex,
                                   u8 shineCount, JPAEmitterManager* emitterManager)
{
	// The shine selector owns both draw buffers and installs them in j3dSys.
	mDrawBuffer0 = new J3DDrawBuffer(0x400);
	j3dSys.setDrawBuffer(mDrawBuffer0, 0);
	mDrawBuffer1 = new J3DDrawBuffer(0x400);
	j3dSys.setDrawBuffer(mDrawBuffer1, 1);

	J3DModelData* normalModel = J3DModelLoaderDataBase::load(
	    JKRFileLoader::getGlbResource("/select/shine_menu.bmd"), 0x5104);
	J3DAnmColor* normalAnimation = static_cast<J3DAnmColor*>(
	    J3DAnmLoaderDataBase::load(
	        JKRFileLoader::getGlbResource("/select/shine_menu.bpk")));
	normalAnimation->searchUpdateMaterialID(normalModel);
	for (u16 i = 0; i < normalModel->mMaterialNum; ++i) {
		J3DMaterialAnm* materialAnimation = new J3DMaterialAnm;
		J3DMaterial* material = normalModel->mMaterials[i];
		material->change();
		material->setMaterialAnm(materialAnimation);
	}
	normalModel->entryMatColorAnimator(normalAnimation);

	J3DModelData* emptyModel = J3DModelLoaderDataBase::load(
	    JKRFileLoader::getGlbResource("/select/shine_menu_empty.bmd"), 0x5104);
	J3DAnmColor* emptyAnimation = static_cast<J3DAnmColor*>(
	    J3DAnmLoaderDataBase::load(
	        JKRFileLoader::getGlbResource("/select/shine_menu_empty.bpk")));
	emptyAnimation->searchUpdateMaterialID(emptyModel);
	for (u16 i = 0; i < normalModel->mMaterialNum; ++i) {
		J3DMaterialAnm* materialAnimation = new J3DMaterialAnm;
		J3DMaterial* material = emptyModel->mMaterials[i];
		material->change();
		material->setMaterialAnm(materialAnimation);
	}
	emptyModel->entryMatColorAnimator(emptyAnimation);

	unk88 = selectedIndex;
	mCurIndex = shineCount;
	unk9c = -static_cast<s32>(shineCount) * 0x28;

	for (int i = 0; i < 8; ++i) {
		const s16 angle = static_cast<s16>(unk9c + i * 0x28);
		const s32 trigAngle = static_cast<s32>(57.295776f * angle);
		const u32 trigIndex = static_cast<u16>(trigAngle) >> jmaSinShift;

		JGeometry::TVec3<f32> pos;
		pos.set(cCenter.x + 1500.0f * jmaSinTable[trigIndex], cCenter.y,
		        cCenter.z + 9000.0f * jmaCosTable[trigIndex]);

		const f32 deltaX = 300.0f - pos.x;
		const f32 deltaZ = 1300.0f - pos.z;
		f32 facing = fabsf(atan2f(-deltaX, deltaZ) * 57.295776f);
		if (pos.x <= cCenter.x)
			facing = -facing;

		const u8 type = shineTypes[i];
		if (type == 3) {
			void* storage = ::operator new(sizeof(TSelectShine));
			TSelectShine* shine = static_cast<TSelectShine*>(storage);
			if (shine != nullptr) {
				const s32 randomValue = rand();
				const f32 randomUnit
				    = static_cast<f32>(randomValue) * 0.000030517578f;
				const s32 randomScaled
				    = static_cast<s32>(randomUnit * 4000.0f);
				const f32 phase = static_cast<f32>(randomScaled) / 1000.0f;
				shine = new (storage) TSelectShine(
				    normalModel, normalAnimation, emitterManager, pos,
				    static_cast<s16>(facing), 0, phase, 0.01f, 10.0f);
			}
			mSelectShines[i] = shine;
		} else if (type == 1 || type == 2) {
			void* storage = ::operator new(sizeof(TSelectShine));
			TSelectShine* shine = static_cast<TSelectShine*>(storage);
			if (shine != nullptr) {
				const s32 randomValue = rand();
				const f32 randomUnit
				    = static_cast<f32>(randomValue) * 0.000030517578f;
				const s32 randomScaled
				    = static_cast<s32>(randomUnit * 4000.0f);
				const f32 phase = static_cast<f32>(randomScaled) / 1000.0f;
				shine = new (storage) TSelectShine(
				    emptyModel, emptyAnimation, emitterManager, pos,
				    static_cast<s16>(facing), 1, phase, 0.01f, 10.0f);
			}
			mSelectShines[i] = shine;
		} else {
			mSelectShines[i] = nullptr;
		}
	}

	TSelectShine* selected = mSelectShines[mCurIndex];
	if (selected->unk4a != 2) {
		if (selected->unk4a == 0) {
			selected->mEmitter1->clearStatus(1);
			selected->mEmitter0->clearStatus(1);
		}
		selected->mEmitter2->clearStatus(1);
	}
}

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
