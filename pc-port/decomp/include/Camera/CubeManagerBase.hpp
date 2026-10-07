#ifndef CAMERA_CUBE_MANAGER_BASE_HPP
#define CAMERA_CUBE_MANAGER_BASE_HPP

#include <Camera/CubeMapTool.hpp>
#include <Strategic/NameRefPtrAry.hpp>
#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JGadget/std-vector.hpp>
#include <dolphin/mtx.h>

class TCubeManagerBase;

extern TCubeManagerBase* gpCubeCamera;
extern TCubeManagerBase* gpCubeMirror;
extern TCubeManagerBase* gpCubeWire;
extern TCubeManagerBase* gpCubeStream;
extern TCubeManagerBase* gpCubeShadow;
extern TCubeManagerBase* gpCubeSoundChange;
extern TCubeManagerBase* gpCubeSoundEffect;

class TCubeManagerBase : public JDrama::TViewObj {
public:
	TCubeManagerBase(const char*, const char*);
	TCubeManagerBase(const char*, u8);

	virtual ~TCubeManagerBase() { }
	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	// fabricated
	TCubeGeneralInfo* getCubeInfo(s32 i) const { return (*unk14)[i]; }

	// Fabricated: the same lookup through JGadget::TVector::begin(), which
	// puts begin() one level deeper than getCubeInfo. bosseel's mouth cube
	// and MSStageCubeFade's sound cubes read it this way: that depth is what
	// makes the ROM `bl` begin() inside TBossEel::perform and
	// MSStageCubeFade::calcParamRatioInCube, while getCubeInfo's callers in
	// CubeManagerBase and MSStageCubeFade::proc drop to 97.7-99.8 under it.
	TCubeGeneralInfo* getCube(s32 i) const
	{
		return unk14->getChildren().begin()[i];
	}

	s32 getDataNo(s32) const;
	int getInCubeNo(const Vec&) const;
	bool isInCube(const Vec&, s32) const;
	bool isInCube(const Vec&, const char*) const;
	void calcPointInCubeRatio(const Vec&, s32, float*, float*, float*) const;


public:
	/* 0x10 */ u8 unk10;
	/* 0x14 */ TNameRefPtrAryT<TCubeGeneralInfo>* unk14;
	/* 0x18 */ const char* unk18;
};

class TCubeManagerMarioIn : public TCubeManagerBase {
public:
	TCubeManagerMarioIn(const char* param_1, const char* param_2)
	    : TCubeManagerBase(param_1, param_2)
	    , unk1C(-1)
	{
	}

	// fabricated
	int getInCubeNoSave() const { return unk1C; }

	/* 0x1C */ int unk1C;
};

class TCubeManagerArea;

extern TCubeManagerArea* gpCubeArea;

class TCubeManagerArea : public TCubeManagerMarioIn {
public:
	TCubeManagerArea(const char* param_1, const char* param_2)
	    : TCubeManagerMarioIn(param_1, param_2)
	{
	}

	bool isInAreaCube(const Vec&) const;
};

class TCubeManagerFast;

extern TCubeManagerFast* gpCubeFastA;
extern TCubeManagerFast* gpCubeFastB;
extern TCubeManagerFast* gpCubeFastC;

class TCubeManagerFast : public TCubeManagerMarioIn {
public:
	TCubeManagerFast(const char* param_1, const char* param_2)
	    : TCubeManagerMarioIn(param_1, param_2)
	{
	}

	bool isInOtherCube(const Vec&) const;
};

bool SMS_IsInOtherFastCube(const Vec&);
bool SMS_IsInSameCameraCube(const Vec&);

#endif
