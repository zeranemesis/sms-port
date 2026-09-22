#include <MoveBG/MapObjWave.hpp>
#include <System/MarDirector.hpp>
#include <math.h>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>

// Functions are defined in the order the binary lays them out; the ones not
// written yet are the gaps between them.

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

void TMapObjWave::noWave()
{
	unk34       = 0.0f;
	unk38       = 0.0f;
	unk2C       = 0.0f;
	unk30       = 0.0f;
	mAmplitude0 = 0.0f;
	mAmplitude1 = 0.0f;
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
