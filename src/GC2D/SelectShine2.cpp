#include <GC2D/SelectShine2.hpp>

TSelectShineManager::~TSelectShineManager() { }

TSelectShineManager::TSelectShineManager(const char* name)
    : JDrama::TViewObj(name)
{
	mDrawBuffer0 = 0;
	mDrawBuffer1 = 0;
	unk78 = 0.0f;
	unk7c = 0.0f;
	unk88 = 0;
	unk90 = 0.0f;
	unk94 = 0.0f;
	unk98 = 0;
	unk9c = 0;
	unka0 = 0.0f;
	unka4 = 0;
	unka5 = 0;
	unka6 = 0;
	unka7 = 0;
}

TSelectShine::~TSelectShine() { }
