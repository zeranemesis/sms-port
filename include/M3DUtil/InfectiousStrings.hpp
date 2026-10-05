#ifndef M3DUTIL_INFECTIOUS_STRINGS_HPP
#define M3DUTIL_INFECTIOUS_STRINGS_HPP

// Every retail TU carrying these mtx calc type names also carries the zero
// object and the message of System/DummyStrings.hpp, and always ahead of them,
// so those headers come first here. The zero object leads in every such TU but
// BathWaterManager, which includes System/DummyStrings.hpp before this header.
#include <System/DummyMactorString.hpp>
#include <System/DummyStrings.hpp>

static const char* MtxCalcTypeName[] = {
	"MActorMtxCalcType_Basic クラシックスケールＯＮ",
	"MActorMtxCalcType_Softimage クラシックスケールＯＦＦ",
	"MActorMtxCalcType_MotionBlend モーションブレンド",
	"MActorMtxCalcType_User ユーザー定義",
};

#endif
