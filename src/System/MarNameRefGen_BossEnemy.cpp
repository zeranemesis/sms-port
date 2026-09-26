#include <Enemy/Emario.hpp>
#include <Enemy/BossHanachan.hpp>
#include <Enemy/DemoBossHanachan.hpp>
#include <Enemy/SleepBossHanachan.hpp>
#include <Enemy/BossEel.hpp>
#include <Enemy/TinKoopa.hpp>
#include <Enemy/BossGesso.hpp>
#include <Enemy/CoasterKiller.hpp>
#include <Enemy/KoopaJr.hpp>
#include <Enemy/BathtubKiller.hpp>
#include <Enemy/BathtubPeach.hpp>
#include <Enemy/BossPakkun.hpp>
#include <Enemy/BossManta.hpp>
#include <System/MarNameRefGen.hpp>

// rogue include: puts the dummy string pair and the MActor mtx-calc names in
// .rodata ahead of the real name table, which is what the original TU did
#include <M3DUtil/InfectiousStrings.hpp>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

JDrama::TNameRef* TMarNameRefGen::getNameRef_BossEnemy(const char* name) const
{
	// Branch list and per-branch constructor arguments recovered from the
	// original: each `bl strcmp` site was paired with the `operator new` size
	// and the name argument loaded into r4 immediately before the constructor
	// call. The order below is the order of the original comparisons.
	if (strcmp(name, "EMario") == 0)
		return new TEMario("マリオモードキ");

	if (strcmp(name, "EMarioManager") == 0)
		return new TEMarioManager("典型敵マネージャー");

	if (strcmp(name, "BossHanachan") == 0)
		return new TBossHanachan("?");

	if (strcmp(name, "BossHanachanManager") == 0)
		return new TBossHanachanManager("?");

	// PAL builds a TDemoBossHanachan here (0x160 bytes): the inlined
	// constructor calls TSpineEnemy("?") and stores the TDemoBossHanachan
	// vtable. The name "SleepBossHanachan" is the scene name, not the class.
	if (strcmp(name, "SleepBossHanachan") == 0)
		return new TDemoBossHanachan("?");

	// TODO: PAL constructs a TEnemyManager subclass (0x58 bytes) for
	// "SleepBossHanachanManager", passing the same "?" name; not declared yet.
	// if (strcmp(name, "SleepBossHanachanManager") == 0)
	// 	return new TSleepBossHanachanManager("?");

	if (strcmp(name, "BossEel") == 0)
		return new TBossEel("?");

	if (strcmp(name, "BossEelManager") == 0)
		return new TBossEelManager("?");

	if (strcmp(name, "BEelTearsManager") == 0)
		return new TBEelTearsManager("めおとウナギ涙マネージャー");

	// PAL has no "BEelTears" branch here; TBEelTears objects are only built
	// through "OilBall" further down.

	// TODO: PAL constructs a 0x1bc-byte class for "Koopa" and a 0x54-byte
	// manager for "KoopaManager"; neither class is declared yet.
	// if (strcmp(name, "Koopa") == 0)
	// 	return new TKoopa;
	// if (strcmp(name, "KoopaManager") == 0)
	// 	return new TKoopaManager;

	if (strcmp(name, "BossGesso") == 0)
		return new TBossGesso("ボスゲッサー");

	if (strcmp(name, "BossGessoManager") == 0)
		return new TBossGessoManager("ボスゲッサーマネージャー");

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

	// TODO: PAL constructs a 0x1b8-byte class for "BossWanwan" and a 0x54-byte
	// manager for "BossWanwanManager"; neither class is declared yet.
	// if (strcmp(name, "BossWanwan") == 0)
	// 	return new TBossWanwan("ボスワンワン");
	// if (strcmp(name, "BossWanwanManager") == 0)
	// 	return new TBossWanwanManager("ボスワンワンマネージャ");

	if (strcmp(name, "BossPakkun") == 0)
		return new TBossPakkun("ボスパックン改");

	if (strcmp(name, "KBossPakkun") == 0)
		return new TBossPakkun("ボスパックン軽");

	if (strcmp(name, "BossPakkunManager") == 0)
		return new TBossPakkunManager("ボスパックンマネージャー", 0);

	if (strcmp(name, "KBossPakkunManager") == 0)
		return new TBossPakkunManager("ボスパックン軽マネージャ", 1);

	// TODO: PAL constructs a 0x38c-byte class for "BossTelesa" and a 0x54-byte
	// manager for "BossTelesaManager"; neither class is declared yet.
	// if (strcmp(name, "BossTelesa") == 0)
	// 	return new TBossTelesa("ボステレサ");
	// if (strcmp(name, "BossTelesaManager") == 0)
	// 	return new TBossTelesaManager("ボステレサマネージャー");

	// TODO: PAL constructs a 0x60-byte class for "BubbleManager"; not declared.
	// if (strcmp(name, "BubbleManager") == 0)
	// 	return new TBubbleManager("バブルマネージャー");

	if (strcmp(name, "OilBall") == 0)
		return new TOilBall("油タマ");

	if (strcmp(name, "BossManta") == 0)
		return new TBossManta("ボスマンタ");

	if (strcmp(name, "BossMantaManager") == 0)
		return new TBossMantaManager("ボスマンタマネージャー");

	return nullptr;
}
