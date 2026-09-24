#include <GC2D/SelectShine2.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>

TSelectShineManager::~TSelectShineManager() { }

TSelectShineManager::TSelectShineManager(const char* name)
    : JDrama::TViewObj(name)
    , mDrawBuffer0(nullptr)
    , mDrawBuffer1(nullptr)
    , unk78(0.0f)
    , unk7c(0.0f)
    , unk88(0)
    , unk90(0.0f)
    , unk94(0.0f)
    , unk98(0)
    , unk9c(0)
    , unka0(0.0f)
    , unka4(0)
    , unka5(0)
    , unka6(0)
    , unka7(0)
{
}

TSelectShine::~TSelectShine() { }

void TSelectShineManager::startClose()
{
	TSelectShine* current
	    = reinterpret_cast<TSelectShine*>(mRumbleOption[mCurIndex]);
	current->unk24 = 0;

	for (int i = 0; i < 8; ++i) {
		TSelectShine* shine
		    = reinterpret_cast<TSelectShine*>(mRumbleOption[i]);
		if (shine != nullptr && i != mCurIndex && shine->unk49 == 0) {
			shine->unk49 = 1;
			shine->unk48 = 0;
		}
	}

	unka7 = 1;
}

void TSelectShineManager::startDecrease(int amount)
{
	TSelectShine* current
	    = reinterpret_cast<TSelectShine*>(mRumbleOption[mCurIndex]);
	if (current->unk4a != 2) {
		if (current->unk4a == 0) {
			current->mEmitter1->setStatus(1);
			current->mEmitter0->setStatus(1);
		}
		current->mEmitter2->setStatus(1);
	}

	unka5 = 1;
	unka0 = static_cast<f32>((amount * 0x28) / 10);
	mCurIndex -= amount;

	TSelectShine* next
	    = reinterpret_cast<TSelectShine*>(mRumbleOption[mCurIndex]);
	if (next->unk4a != 2) {
		if (next->unk4a == 0) {
			next->mEmitter1->clearStatus(1);
			next->mEmitter0->clearStatus(1);
		}
		next->mEmitter2->clearStatus(1);
	}
}

void TSelectShineManager::startIncrease(int amount)
{
	TSelectShine* current
	    = reinterpret_cast<TSelectShine*>(mRumbleOption[mCurIndex]);
	if (current->unk4a != 2) {
		if (current->unk4a == 0) {
			current->mEmitter1->setStatus(1);
			current->mEmitter0->setStatus(1);
		}
		current->mEmitter2->setStatus(1);
	}

	unka4 = 1;
	unka0 = static_cast<f32>((amount * -0x28) / 10);
	mCurIndex += amount;

	TSelectShine* next
	    = reinterpret_cast<TSelectShine*>(mRumbleOption[mCurIndex]);
	if (next->unk4a != 2) {
		if (next->unk4a == 0) {
			next->mEmitter1->clearStatus(1);
			next->mEmitter0->clearStatus(1);
		}
		next->mEmitter2->clearStatus(1);
	}
}
