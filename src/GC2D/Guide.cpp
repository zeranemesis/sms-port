
#include <GC2D/Guide.hpp>

TGuide::TGuide(const char* name)
    : TViewObj(name)
    , unk10(8)
    , unkBC(0)
    , unkC0(nullptr)
    , unkC4(0)
    , unkC5(0)
    , unk160(0xFF)
    , unk164(1)
    , unk480(-1)
{
}

void TGuide::startMoveCursor()
{
	unk10  = 9;
	unk164 = 0;
}

TGuide::~TGuide() { }
