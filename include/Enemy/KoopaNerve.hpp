#ifndef ENEMY_KOOPANERVE_HPP
#define ENEMY_KOOPANERVE_HPP

#include <Enemy/Koopa.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Strategic/Nerve.hpp>

class TLiveActor;

// The nerve singletons in this unit are not the DEFINE_NERVE shape: the map
// mangles their statics as nerve$localstatic0$theNerve__..., which is what
// MWCC emits for a static inside an *inline* function, so theNerve() was
// written out in the header. Same as limitkoopa.cpp's ten nerves.
//
// TNerveKoopaTurn's vtable is {0, 0, dtor, 0} with no execute slot filled, so
// it is an abstract intermediate. Its children are the five nerves whose
// destructors are 0x6c (two vtable stores) rather than 0x5c: Wait, Tumble,
// TurnL, TurnR and Flame.
class TNerveKoopaTurn : public TNerveBase<TLiveActor> {
public:
	virtual ~TNerveKoopaTurn() { }
};

#define DECLARE_KOOPA_NERVE(Name, Base)                                        \
	class Name : public Base {                                                 \
	public:                                                                    \
		virtual BOOL execute(TSpineBase<TLiveActor>*) const;                   \
		static const Name& theNerve()                                          \
		{                                                                      \
			static Name nerve;                                                 \
			return nerve;                                                      \
		}                                                                      \
	};

// These two nerves' execute symbols are weak, so the bodies were written in
// the class. They need the complete TKoopa, which is why the whole nerve set
// lives in this header rather than in Koopa.hpp.
//
// One turn routine for both directions, called with a constant `left`: MWCC
// folds the other arm only after inlining, so its objects (bindings, callee
// locals, IRO temporaries) stay in each nerve's frame as dead words. That is
// 0xb0 of the 0x108 both nerves used to be short (docs/catalog/frame-model.md,
// "Dead code keeps its objects"); the arm order and spelling are frame-inert.
// TODO: both frames still 0x58 short (0x148/0x150 vs 0x1a0/0x1a8), no stack
// access to place the rest; a third `turnBody(diff)` arm gives +0x40.
static inline bool KoopaTurn(TKoopa* koopa, f32 diff, bool left)
{
	if (left) {
		if (diff < -koopa->getTurnSpeed())
			return koopa->turnBody(-koopa->getTurnStep());
		else if (diff < 0.0f)
			return koopa->turnBody(diff);
	} else {
		if (diff > koopa->getTurnSpeed())
			return koopa->turnBody(koopa->getTurnStep());
		else if (diff > 0.0f)
			return koopa->turnBody(diff);
	}
	return false;
}
class TNerveKoopaTurnR : public TNerveKoopaTurn {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>* spine) const
	{
		TKoopa* koopa = (TKoopa*)spine->getBody();
		if (KoopaTurn(koopa,
		               WrapDegreesF(koopa->mTargetDir - koopa->mRotation.y), false))
			return FALSE;
		return TRUE;
	}

	static const TNerveKoopaTurnR& theNerve()
	{
		static TNerveKoopaTurnR nerve;
		return nerve;
	}
};

class TNerveKoopaTurnL : public TNerveKoopaTurn {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>* spine) const
	{
		TKoopa* koopa = (TKoopa*)spine->getBody();
		if (KoopaTurn(koopa,
		               WrapDegreesF(koopa->mTargetDir - koopa->mRotation.y), true))
			return FALSE;
		return TRUE;
	}

	static const TNerveKoopaTurnL& theNerve()
	{
		static TNerveKoopaTurnL nerve;
		return nerve;
	}
};

// Declaration order is the retail .sbss/.bss order of the nerve statics
// (TurnR, TurnL, Tumble, Provoke, Wait, Flame, GetDown, Stagger, Fall,
// GetShowered): MWCC lays out an inline function's local static in the order
// the inline *definitions* appear in the TU, not by first use.
DECLARE_KOOPA_NERVE(TNerveKoopaTumble, TNerveKoopaTurn)
DECLARE_KOOPA_NERVE(TNerveKoopaProvoke, TNerveBase<TLiveActor>)
DECLARE_KOOPA_NERVE(TNerveKoopaWait, TNerveKoopaTurn)
DECLARE_KOOPA_NERVE(TNerveKoopaFlame, TNerveKoopaTurn)
DECLARE_KOOPA_NERVE(TNerveKoopaGetDown, TNerveBase<TLiveActor>)
DECLARE_KOOPA_NERVE(TNerveKoopaStagger, TNerveBase<TLiveActor>)
DECLARE_KOOPA_NERVE(TNerveKoopaFall, TNerveBase<TLiveActor>)
DECLARE_KOOPA_NERVE(TNerveKoopaGetShowered, TNerveBase<TLiveActor>)

#endif
