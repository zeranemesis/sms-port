#ifndef GC2D_SELECT_SHINE_2_HPP
#define GC2D_SELECT_SHINE_2_HPP

#include <GC2D/Option.hpp>
#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JGeometry/JGVec3.hpp>
#include <JSystem/JParticle/JPAEmitterManager.hpp>

class J3DModelData;
class J3DModel;
class J3DAnmColor;
class J3DDrawBuffer;

/**
 * @brief A single spinning shine icon on the shine-select screen, complete
 * with its 3D model and sparkle particle effects.
 */
class TSelectShine {
public:
	TSelectShine(J3DModelData* modelData, J3DAnmColor* anmColor,
	             JPAEmitterManager* emitterManager,
	             JGeometry::TVec3<f32>& pos, s16 param5, u8 param6,
	             f32 param7, f32 param8, f32 param9);
	virtual ~TSelectShine();
	virtual void move();

public:
	/* 0x00 */ // vtable
	/* 0x04 */ J3DModel* mModel;
	/* 0x08 */ J3DAnmColor* mAnmColor;
	/* 0x0C */ JGeometry::TVec3<f32> mPos;
	/* 0x18 */ JGeometry::TVec3<f32> unk18;
	/* 0x24 */ u8 unk24;
	/* 0x25 */ u8 unk25[3];
	/* 0x28 */ f32 unk28;
	/* 0x2C */ f32 unk2c;
	/* 0x30 */ f32 unk30;
	/* 0x34 */ u32 unk34;
	/* 0x38 */ u8 unk38;
	/* 0x39 */ u8 unk39;
	/* 0x3A */ s16 unk3a;
	/* 0x3C */ s16 unk3c;
	/* 0x3E */ u8 unk3e;
	/* 0x3F */ u8 unk3f;
	/* 0x40 */ f32 unk40;
	/* 0x44 */ f32 unk44;
	/* 0x48 */ u8 unk48;
	/* 0x49 */ u8 unk49;
	/* 0x4A */ u8 unk4a;
	/* 0x4B */ u8 unk4b;
	/* 0x4C */ JPAEmitterManager* mEmitterManager;
	/* 0x50 */ JPABaseEmitter* mEmitter0;
	/* 0x54 */ JPABaseEmitter* mEmitter1;
	/* 0x58 */ JPABaseEmitter* mEmitter2;
};

class TSelectShineManager : public JDrama::TViewObj {
public:
	TSelectShineManager(const char*);
	virtual ~TSelectShineManager();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void initData(u8*, u8, u8, JPAEmitterManager*);
	void startClose();
	void startIncrease(int);
	void startDecrease(int);

public:
	/* 0x10 */ TOptionRumbleUnit* mRumbleOption[8];
	/* 0x30 */ u8 unk30[0x20];
	/* 0x50 */ J3DDrawBuffer* mDrawBuffer0;
	/* 0x54 */ J3DDrawBuffer* mDrawBuffer1;
	/* 0x58 */ u8 unk58[0x20];
	/* 0x78 */ f32 unk78;
	/* 0x7C */ f32 unk7c;
	/* 0x80 */ u8 unk80[0x8];
	/* 0x88 */ u32 unk88;
	/* 0x8C */ s32 mCurIndex;
	/* 0x90 */ f32 unk90;
	/* 0x94 */ f32 unk94;
	/* 0x98 */ u32 unk98;
	/* 0x9C */ u32 unk9c;
	/* 0xA0 */ f32 unka0;
	/* 0xA4 */ u8 unka4;
	/* 0xA5 */ u8 unka5;
	/* 0xA6 */ u8 unka6;
	/* 0xA7 */ u8 unka7;
	/* 0xA8 */ JGeometry::TVec3<f32> unka8[8];
	/* 0x108 */ u8 unk108[0x18];
};

#endif // GC2D_SELECT_SHINE_2_HPP
