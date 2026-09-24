#ifndef ENEMY_WIRE_BINDER_HPP
#define ENEMY_WIRE_BINDER_HPP

#include <JSystem/JGeometry.hpp>
#include <Strategic/Binder.hpp>
#include <Strategic/LiveActor.hpp>

class TMapWire;

class TWireBinder : TBinder {
public:
	bool init(const JGeometry::TVec3<f32>&);
	bool reset(const JGeometry::TVec3<f32>&);

	void bind(TLiveActor*);
	const JGeometry::TVec3<f32>& getDir() const { return mDir; }
	JGeometry::TVec3<f32> getDirAtPos(const JGeometry::TVec3<f32>&, f32) const;
	void getPoint(JGeometry::TVec3<f32>*, f32) const;
	void getPoint(JGeometry::TVec3<f32>*, const JGeometry::TVec3<f32>&) const;

	static bool isOnWire(const JGeometry::TVec3<f32>&);
	f32 getRangePos(const JGeometry::TVec3<f32>&) const;
	TMapWire* getWire() const;
	bool isEndWire(const JGeometry::TVec3<f32>&, f32) const;

private:
	/* 0x04 */ s32 mWireNumber;
	/* 0x08 */ JGeometry::TVec3<f32> mDir;
};

#endif
