#ifndef MSHANDLE_HPP
#define MSHANDLE_HPP

#include <JSystem/JAudio/JAInterface/JAISound.hpp>
#include <dolphin/types.h>
#include <dolphin/mtx.h>

struct SeCategory {
	u8 mType;
	f32 unk4;
	f32 unk8;
	f32 unkC;
};

// Fabricated name: the smSeCategory index of a sound id (MSHandle,
// MAnmSoundMario::startAnimSound, MSoundSE's gate). The map has no symbol for
// it in any of those units, so every copy was expanded.
inline u32 MSGetSeCategory(u32 param_1)
{
	u32 uVar1 = param_1 >> 30;
	u32 uVar2 = param_1 >> 12 & 0xF;

	if (uVar1 == 0)
		return uVar2;

	if (uVar1 == 2)
		return 0x10;

	if (uVar1 == 3)
		return 0x11;

	return 0xffffffff;
}

class MSHandle : public JAISound {
public:
	static SeCategory smSeCategory[];
	static f32 smACosPrm[];
	static f32 cPan_MaxAmp;
	static f32 cPan_CAdjust;
	static f32 cPan_CShift;
	static f32 cPan_HiSence_Dist;
	static f32 cMS_DistanceMax_Sence;
	static f32 cDol_0Rad;
	static f32 cDol_HalfRad;
	static f32 cDol_FullRad;

	MSHandle() { }

	virtual void setSeDistanceParameters();
	virtual void setSeDistanceVolume(u8 moveTime);
	virtual void setSeDistancePan(u8 moveTime);
	virtual void setSeDistancePitch(u8 moveTime);
	virtual void setSeDistanceDolby(u8 moveTime);
	virtual f32 setDistanceVolumeCommon(f32 volume, u8 moveTime);

	static f32 calcVolume(f32 param1, f32 param2, f32 param3, u8 param4,
	                      u8 param5);
	static f32 calcPan(const Vec& vec, f32 param1, f32 param2);
	static f32 calcDolby(const Vec& vec, f32 param);
	static f32 MSACos(f32 param);
};

#endif // MSHANDLE_HPP
