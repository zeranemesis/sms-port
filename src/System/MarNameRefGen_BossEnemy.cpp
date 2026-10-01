#include <Enemy/Emario.hpp>
#include <Enemy/BossHanachan.hpp>
#include <Enemy/DemoBossHanachan.hpp>
#include <Enemy/SleepBossHanachan.hpp>
#include <Enemy/BossEel.hpp>
#include <Enemy/TinKoopa.hpp>
#include <Enemy/BossGesso.hpp>
#include <Enemy/CoasterKiller.hpp>
#include <Enemy/Koopa.hpp>
#include <Enemy/KoopaJr.hpp>
#include <Enemy/BathtubKiller.hpp>
#include <Enemy/BathtubPeach.hpp>
#include <Enemy/BossWanwan.hpp>
#include <Enemy/BossPakkun.hpp>
#include <Enemy/bosstelesa.hpp>
#include <Enemy/BossManta.hpp>
#include <System/MarNameRefGen.hpp>

// rogue include: puts the dummy string pair in .rodata ahead of everything the
// name table uses, which is what the original TU did. InfectiousStrings.hpp is
// split in two on purpose: the target's .rodata order is
//   @1490 (12 zero bytes), @1726 (no-memory message), cDirtyFileName,
//   cDirtyTexName, then the four MActorMtxCalcType names,
// so the cDirty pair sat *between* the dummy pair and the mtx-calc names.
#include <System/DummyStrings.hpp>

const char cDirtyFileName[] = "/scene/map/pollution/H_ma_rak.bti";
const char cDirtyTexName[]  = "H_ma_rak_dummy";

#include <M3DUtil/InfectiousStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

// NOTE: Koopa.hpp has to stay up with the other Enemy headers even though it
// costs something. Its KOOPA_NERVE macro puts a function-local `static Name
// nerve;` (plus its init$ guard) behind each of the ten TNerveKoopa*
// ::theNerve(), and MWCC emits those into .bss even though nothing here calls
// them -- 10 x 12 bytes ahead of the JSUList statics, which pushes all fifteen
// of __sinit_'s `addi r5, r31, N` offsets to +0x78. Moving the include does not
// help (the .bss order is not include-order driven); the only fix is splitting
// the nerve declarations out of Koopa.hpp, which also needs koopajr.cpp and
// popo.cpp updated.

// All 37 strcmp branches are present, in the ROM's order, with the ROM's
// strings. Each was recovered by pairing every `bl strcmp` site in the target
// with the `li r3, <size>` passed to operator new (i.e. sizeof the class), the
// `bl __ct__` it calls, the name argument loaded into r4 right before that
// call and the vtable(s) stored right after it.
JDrama::TNameRef* TMarNameRefGen::getNameRef_BossEnemy(const char* name) const
{
	if (strcmp(name, "EMario") == 0)
		return new TEMario("マリオモドキ");

	if (strcmp(name, "EMarioManager") == 0)
		return new TEMarioManager("典型敵マネージャ");

	if (strcmp(name, "BossHanachan") == 0)
		return new TBossHanachan("?");

	if (strcmp(name, "BossHanachanManager") == 0)
		return new TBossHanachanManager("?");

	// PAL builds a TSleepBossHanachan (0x160 bytes) here. Its constructor is
	// inline (no __ct__18TSleepBossHanachanF* in marioEU.MAP) and is expanded
	// whole: TSpineEnemy("?"), the TDemoBossHanachan vtable, then this class's,
	// then the +0x150 TVec3 set. That set is the TU's only out-of-line copy of
	// Two spellings are load-bearing here and neither alone is enough. The call
	// passes no argument, so the header default supplies the same "?" literal,
	// and +0x150 is set from a member *initialiser* rather than a body
	// statement. Together they are what leave JGeometry::TVec3<f32>::set<f32>
	// as this TU's only out-of-line copy of it.
	// "SleepBossHanachan" is the scene name, not a class name.
	if (strcmp(name, "SleepBossHanachan") == 0)
		return new TSleepBossHanachan;

	if (strcmp(name, "SleepBossHanachanManager") == 0)
		return new TSleepBossHanachanManager("?");

	if (strcmp(name, "BossEel") == 0)
		return new TBossEel("?");

	if (strcmp(name, "BossEelManager") == 0)
		return new TBossEelManager("?");

	if (strcmp(name, "BEelTearsManager") == 0)
		return new TBEelTearsManager("めおとウナギ涙マネージャー");

	if (strcmp(name, "Koopa") == 0)
		return new TKoopa("クッパ");

	if (strcmp(name, "KoopaManager") == 0)
		return new TKoopaManager("クッパマネージャー");

	if (strcmp(name, "BossGesso") == 0)
		return new TBossGesso("ボスゲッソー");

	if (strcmp(name, "BossGessoManager") == 0)
		return new TBossGessoManager("ボスゲッソーマネージャ");

	if (strcmp(name, "TinKoopa") == 0)
		return new TTinKoopa("メカクッパ");

	if (strcmp(name, "TinKoopaManager") == 0)
		return new TTinKoopaManager("メカクッパマネージャ");

	if (strcmp(name, "CoasterKillerManager") == 0)
		return new TCoasterKillerManager("コースターキラーマネージャー");

	if (strcmp(name, "CoasterKiller") == 0)
		return new TCoasterKiller("コースターキラー");

	if (strcmp(name, "KoopaJrManager") == 0)
		return new TKoopaJrManager("クッパジュニアマネージャー");

	if (strcmp(name, "KoopaJr") == 0)
		return new TKoopaJr("クッパジュニア");

	if (strcmp(name, "KoopaJrSubmarineManager") == 0)
		return new TKoopaJrSubmarineManager("クッパジュニアサブマリンマネージャー");

	if (strcmp(name, "KoopaJrSubmarine") == 0)
		return new TKoopaJrSubmarine("クッパジュニアサブマリン");

	if (strcmp(name, "BathtubKillerManager") == 0)
		return new TBathtubKillerManager("バスタブキラーマネージャー");

	if (strcmp(name, "BathtubKiller") == 0)
		return new TBathtubKiller("バスタブキラー");

	if (strcmp(name, "BathtubPeachManager") == 0)
		return new TBathtubPeachManager("バスタブピーチマネージャー");

	if (strcmp(name, "BathtubPeach") == 0)
		return new TBathtubPeach("バスタブピーチ");

	if (strcmp(name, "BossWanwan") == 0)
		return new TBossWanwan("ボスワンワン");

	if (strcmp(name, "BossWanwanManager") == 0)
		return new TBossWanwanManager("ボスワンワンマネージャ");

	if (strcmp(name, "BossPakkun") == 0)
		return new TBossPakkun("ボスパックン改");

	if (strcmp(name, "KBossPakkun") == 0)
		return new TBossPakkun("ボスパックン軽");

	if (strcmp(name, "BossPakkunManager") == 0)
		return new TBossPakkunManager("ボスパックンマネージャー", 0);

	if (strcmp(name, "KBossPakkunManager") == 0)
		return new TBossPakkunManager("ボスパックン軽マネージャ", 1);

	if (strcmp(name, "BossTelesa") == 0)
		return new TBossTelesa("ボステレサ");

	if (strcmp(name, "BossTelesaManager") == 0)
		return new TBossTelesaManager("ボステレサマネージャー");

	if (strcmp(name, "BubbleManager") == 0)
		return new TBubbleManager("バブルマネージャー");

	// "OilBall" builds a TOilBall (0x170 bytes): its inline constructor calls
	// TBEelTears out of line and only patches the TOilBall vtable.
	if (strcmp(name, "OilBall") == 0)
		return new TOilBall("油ダマ");

	if (strcmp(name, "BossManta") == 0)
		return new TBossManta("ボスマンタ");

	if (strcmp(name, "BossMantaManager") == 0)
		return new TBossMantaManager("ボスマンタマネージャ");

	return nullptr;
}
