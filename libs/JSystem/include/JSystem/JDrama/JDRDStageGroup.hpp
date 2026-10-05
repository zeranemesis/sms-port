#ifndef JDR_DSTAGE_GROUP_HPP
#define JDR_DSTAGE_GROUP_HPP

#include <JSystem/JDrama/JDRViewObjPtrList.hpp>
#include <JSystem/JDrama/JDRFrmGXSet.hpp>

namespace JDrama {

// The `TFlagT<u16>` default argument follows the JDrama creatable-object
// convention (TDStageDisp, TEfbCtrlDisp, TEfbCtrlTex all take
// `(const char* = "<...>", TFlagT<u16> = 0)`); nothing in the inlined body
// uses it. Its temporary is the dead top word of every `new TDStageGroup`
// site (MenuDir/MovieDirector setup 0x3c, GCLogoDir setup's parse-time block,
// SelectDir rsetup) and its per-field IRO copy is one of the bottom words
// (research c-r28). The other words the setups were short of came from their
// pad statement (`getGamePad()->setFlag(1)`), not from this chain.
class TDStageGroup : public TViewObjPtrListT<TViewObj> {
public:
	TDStageGroup(TDisplay* display, const char* name = "<TDStageGroup>",
	             TFlagT<u16> flag = 0)
	    : TViewObjPtrListT<TViewObj>(name)
	    , unk20(display)
	{
	}

	virtual void perform(u32 cue, TGraphics* graphics);

public:
	/* 0x20 */ TFrmGXSet unk20;
};

} // namespace JDrama

#endif
