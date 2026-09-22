#include <Enemy/BathtubBinder.hpp>
#include <Strategic/LiveActor.hpp>

TBathtubBinder::TBathtubBinder()
{
	unk4 = nullptr;
	unk8 = 0;
}

TBathtubBinder::~TBathtubBinder() {}

void TBathtubBinder::bind(TLiveActor* actor)
{
	if (unk4 == nullptr || *(u8*)((u8*)unk4 + 0x29a) == 0) {
		float_(actor);
	}
}

// TODO: TBathtubBinder::init(f32, f32, f32, f32, f32) and
// TBathtubBinder::float_(TLiveActor*) are left unwritten. init() needs two
// string literals (used as JDrama::TNameRef keys at offsets 0xe0/0xec of a
// rodata object) recovered byte-for-byte before it can be safely written,
// and float_() is a 1148-byte function that was not attempted in the time
// budget available for this pass.
