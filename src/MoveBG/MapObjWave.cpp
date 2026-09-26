#include <MoveBG/MapObjWave.hpp>
#include <System/MarDirector.hpp>
#include <math.h>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// This unit is reverse_fn_order: with -inline deferred MWCC emits functions in
// the reverse of their source order, so the source runs backwards relative to
// the addresses in the map. Check with:
//   python tools/validate-symbol-order.py -u marioEU/MoveBG/MapObjWave --map <marioEU.MAP>
// The functions not written yet are the gaps between these.

// 0.15915507f is 1/2pi: it turns a world distance into a fraction of a
// wavelength before the phase is added.
f32 TMapObjWave::getWaveHeight(f32 x, f32 z) const
{
	if (!unk94)
		return 0.0f;

	f32 waveX = mAmplitude0 * sinf(mAngleSpeed0 * (0.15915507f * x) + mAngle0);
	f32 waveZ = mAmplitude1 * sinf(mAngleSpeed1 * (0.15915507f * z) + mAngle1);

	return waveX + waveZ;
}

f32 TMapObjWave::getHeight(f32 x, f32 y, f32 z) const
{
	const TBGCheckData* data;
	f32 result = gpMap->checkGroundExactY(x, 50.0f + y, z, &data);

	// One return rather than early ones: retail lays the "not water" case out
	// last, which is where the false block of an if/else goes. Still one
	// instruction short of a match -- retail has a second branch here that
	// reads like an explicit else on the inner if, but writing that else out
	// costs more than it gains.
	if (data->isWaterSurface()) {
		if (data->isSea())
			result = getWaveHeight(x, z);
	} else {
		result = y;
	}

	return result;
}

void TMapObjWave::noWave()
{
	unk34       = 0.0f;
	unk38       = 0.0f;
	unk2C       = 0.0f;
	unk30       = 0.0f;
	mAmplitude0 = 0.0f;
	mAmplitude1 = 0.0f;
}

#pragma dont_inline on
void TMapObjWave::updateTime()
{
	mAngle0 += mAngleSpeed0;
	if (mAngle0 > 6.2831802f)
		mAngle0 -= 6.2831802f;

	mAngle1 += mAngleSpeed1;
	if (mAngle1 > 6.2831802f)
		mAngle1 -= 6.2831802f;

	mTexPos0 += mTexSpeed;
	if (mTexPos0 > 1.0f)
		mTexPos0 -= 1.0f;

	mTexPos1 += mTexSpeed;
	if (mTexPos1 > 1.0f)
		mTexPos1 -= 1.0f;
}
#pragma dont_inline off

void TMapObjWave::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (!unk94)
		return;

	if (cue & CUE_MOVE) {
		updateTime();
		if (gpMarDirector->mMap == 4 || gpMarDirector->mMap == 6)
			updateHeightAndAlpha();
	}

	if (cue & CUE_DRAW) {
		initDraw();
		draw();
	}
}

void TMapObjWave::load(JSUMemoryInputStream& stream)
{
	JDrama::TNameRef::load(stream);

	mWaveSpan        = 5200.0f;
	mWaveHeight      = 200.0f;
	mHalfWaveSpan    = mWaveSpan * 0.5f;
	mInvHalfWaveSpan = 1.0f / mHalfWaveSpan;
	mWaveCount       = mWaveSpan / mWaveHeight;
	unk94            = (u32)JKRFileLoader::getGlbResource("/scene/map/map/wave.bti");
	mTexSpeed        = 0.0015f;
	mWaveTexScale    = 0.0012f;
	mWaveTexScale2   = 0.0015f;
	unk4C            = 400.0f;
	unk50            = 150.0f;
	mAngleSpeed0     = 0.02f;
	mAngleSpeed1     = 0.03f;

	switch (gpMarDirector->mMap) {
	case 3:
	case 30:
		unk2C = 25.0f;
		unk30 = 20.0f;
		unk34 = 0.0f;
		unk38 = 0.0f;
		break;
	case 4:
		unk2C = 40.0f;
		unk30 = 30.0f;
		unk34 = 5.0f;
		unk38 = 0.0f;
		break;
	case 13:
		unk2C = 30.0f;
		unk30 = 25.0f;
		unk34 = 5.0f;
		unk38 = 0.0f;
		break;
	case 9:
	case 52:
		unk2C = 10.0f;
		unk30 = 15.0f;
		unk34 = 0.0f;
		unk38 = 0.0f;
		break;
	default:
		unk2C = 30.0f;
		unk30 = 25.0f;
		unk34 = 0.0f;
		unk38 = 0.0f;
		break;
	}

	mAmplitude0 = unk2C;
	mAmplitude1 = unk30;
}

TMapObjWave::~TMapObjWave() { }
