#ifndef ENEMY_BATHTUB_BINDER_HPP
#define ENEMY_BATHTUB_BINDER_HPP

#include <Strategic/Binder.hpp>
#include <dolphin/types.h>

class TLiveActor;

// NOTE: no reference to TBathtubBinder exists anywhere else in the codebase
// (no allocation site was found), so the object's total size could not be
// confirmed from a `li r3, sizeof` call site. Field layout below is derived
// purely from offsets accessed in TBathtubBinder's own member functions.
class TBathtubBinder : public TBinder {
public:
	TBathtubBinder();
	virtual ~TBathtubBinder();

	bool init(f32, f32, f32, f32, f32);
	virtual void bind(TLiveActor*);

	// TODO: not yet decompiled (see BathtubBinder.cpp) - needs two joint/name
	// string literals recovered byte-for-byte before it can be written.
	void float_(TLiveActor*);

public:
	// set to nullptr in the ctor; populated in init() via a
	// TNameRef::calcKeyCode() lookup + virtual call (returns a pointer, not
	// an int -- bind() dereferences unk4 and reads a byte at offset 0x29a,
	// consistent with a TLiveActor* whose exact pointee type/offset meaning
	// was not confirmed). unk8 is cleared back to 0 in init() when unk4 is
	// null.
	/* 0x04 */ void* unk4;
	/* 0x08 */ s32 unk8;
	/* 0x0c */ f32 unkC;
	/* 0x10 */ f32 unk10;
	/* 0x14 */ f32 unk14;
	/* 0x18 */ f32 unk18;
	/* 0x1c */ f32 unk1C; // = unk18 / (unk18 + unk14)
	/* 0x20 */ f32 unk20;
};

#endif // ENEMY_BATHTUB_BINDER_HPP
