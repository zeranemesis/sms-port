#ifndef ENEMY_WIRE_BINDER_HPP
#define ENEMY_WIRE_BINDER_HPP

#include <JSystem/JGeometry.hpp>
#include <Map/MapWireManager.hpp>
#include <Strategic/Binder.hpp>
#include <Strategic/LiveActor.hpp>

class TWireBinder : TBinder {
public:
	bool init(const JGeometry::TVec3<f32>&);
	inline bool reset(const JGeometry::TVec3<f32>&);

	void bind(TLiveActor*);
	// A reference, not a value: the map's weak copy is 0x8 bytes, a bare
	// `addi r3, r3, 8; blr`, with no hidden return buffer.
	const JGeometry::TVec3<f32>& getDir() const { return mDir; }
	JGeometry::TVec3<f32> getDirAtPos(const JGeometry::TVec3<f32>&, f32) const;
	void getPoint(JGeometry::TVec3<f32>*, f32) const;
	void getPoint(JGeometry::TVec3<f32>*, const JGeometry::TVec3<f32>&) const;

	static bool isOnWire(const JGeometry::TVec3<f32>&);
	inline f32 getRangePos(const JGeometry::TVec3<f32>&) const;
	inline TMapWire* getWire() const;
	inline bool isStartWire(const JGeometry::TVec3<f32>&, f32) const;
	bool isEndWire(const JGeometry::TVec3<f32>&, f32) const;
	static inline f32 getStartRangePos(f32);
	static inline f32 getEndRangePos(f32);

private:
	/* 0x04 */ s32 mWireNumber;
	/* 0x08 */ JGeometry::TVec3<f32> mDir;
};

inline bool TWireBinder::reset(const JGeometry::TVec3<f32>& param_1)
{
	JGeometry::TVec3<f32> local24;
	JGeometry::TVec3<f32> local30;

	mWireNumber = gpMapWireManager->getWireNo(param_1);
	if (mWireNumber == -1)
		return false;

	TMapWire* wire = gpMapWireManager->getWire(mWireNumber);

	local24 = wire->mStartPoint;
	local30 = wire->mEndPoint;
	local30 -= local24;

	mDir.normalize(local30);
	return true;
}

inline f32 TWireBinder::getRangePos(const JGeometry::TVec3<f32>& param_1) const
{
	return getWire()->getPosInWire(param_1);
}

inline TMapWire* TWireBinder::getWire() const
{
	return gpMapWireManager->getWire(mWireNumber);
}

inline f32 TWireBinder::getStartRangePos(f32 param_1)
{
	return 0.0f < param_1 ? 0.0f : 1.0f;
}

inline f32 TWireBinder::getEndRangePos(f32 param_1)
{
	return 0.0f < param_1 ? 1.0f : 0.0f;
}

inline bool TWireBinder::isStartWire(const JGeometry::TVec3<f32>& param_1, f32 param_2) const
{
	f32 posInWire = getRangePos(param_1);
	f32 targetPos = getStartRangePos(param_2);
	f32 diff      = posInWire - targetPos;

	return fabsf(diff) < 0.015f;
}

#endif
