#ifndef ENEMY_KOOPANERVE_HPP
#define ENEMY_KOOPANERVE_HPP

#include <Strategic/Nerve.hpp>
#include <Strategic/LiveActor.hpp>

// The ten TKoopa nerve classes are declared here rather than in Enemy/Koopa.hpp
// because only Enemy/Koopa.cpp can see them in the retail binary. Every one of
// the KOOPA_NERVE macros below puts a `static Name nerve;` (plus MWCC's `init$`
// guard and a 12-byte destructor-registration object in .bss) behind
// Name::theNerve(), so a translation unit that merely *includes* the nerve
// classes emits all ten statics even when it never calls the accessor.
//
// Neither build/GMSP01/obj/System/MarNameRefGen_BossEnemy.o nor
// build/GMSP01/obj/Enemy/koopajr.o contains any of them, although both of those
// translation units include Koopa.hpp in this tree and both instantiate TKoopa
// (koopajr.cpp via KoopaJr.hpp). marioEU.MAP likewise lists all twenty
// `nerve$localstatic0$theNerve__..` / `init$localstatic1$theNerve__..` objects
// as "found in Enemy.a Koopa.cpp" and nowhere else. Keeping the block in a
// header that only Koopa.cpp includes is what reproduces that.

class TNerveKoopaTurn : public TNerveBase<TLiveActor> {
};

// In the retail binary theNerve() is inlined in every user (weak function-local
// statics), so the accessor is defined in the class.
//
// TODO: marioEU.MAP lists TNerveKoopaTurnL::execute and TNerveKoopaTurnR::execute
// as *weak*, i.e. they were defined in a header, while every other execute in
// the TU is a global defined in Koopa.cpp. Defining the two inline here would
// fix validate-symbol-order.py's linkage check for Koopa.cpp; it is left alone
// because moving their bodies (and the -180.0f / 360.0f literals they use) out
// of Koopa.cpp is a reconstruction of those two functions, not a header move.
#define KOOPA_NERVE(Name, Base)                                                	class Name : public Base {                                                 	public:                                                                    		virtual BOOL execute(TSpineBase<TLiveActor>*) const;                   		static const Name& theNerve()                                          		{                                                                      			static Name nerve;                                                 			return nerve;                                                      		}                                                                      	};

// The order of these ten declarations is load-bearing: each KOOPA_NERVE emits a
// `nerve` static (plus MWCC's init$ guard and a 12-byte destructor-registration
// object), and MWCC creates those in declaration order. build/GMSP01/obj's
// .sbss/.bss layout therefore pins the order exactly, and the `addi r5, r31, 0xNN`
// immediate every theNerve() passes to __register_global_object is the resulting
// offset:
//   0x00 TurnR  0x0c TurnL  0x18 Tumble 0x24 Provoke 0x30 Wait
//   0x3c Flame  0x48 GetDown 0x54 Stagger 0x60 Fall  0x6c GetShowered
// (`build/binutils/powerpc-eabi-nm.exe -n build/GMSP01/obj/Enemy/Koopa.o`).
// Note that the TNerveBase-derived nerves Provoke / GetDown are interleaved with
// the TNerveKoopaTurn-derived ones, so this is the original source order and not a
// grouping by base class.
KOOPA_NERVE(TNerveKoopaTurnR, TNerveKoopaTurn);
KOOPA_NERVE(TNerveKoopaTurnL, TNerveKoopaTurn);
KOOPA_NERVE(TNerveKoopaTumble, TNerveKoopaTurn);
KOOPA_NERVE(TNerveKoopaProvoke, TNerveBase<TLiveActor>);
KOOPA_NERVE(TNerveKoopaWait, TNerveKoopaTurn);
KOOPA_NERVE(TNerveKoopaFlame, TNerveKoopaTurn);
KOOPA_NERVE(TNerveKoopaGetDown, TNerveBase<TLiveActor>);
KOOPA_NERVE(TNerveKoopaStagger, TNerveBase<TLiveActor>);
KOOPA_NERVE(TNerveKoopaFall, TNerveBase<TLiveActor>);
KOOPA_NERVE(TNerveKoopaGetShowered, TNerveBase<TLiveActor>);

#endif