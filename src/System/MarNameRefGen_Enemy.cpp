#include <System/MarNameRefGen.hpp>
#include <Enemy/EffectObj.hpp>
#include <Enemy/effectEnemy.hpp>
#include <Enemy/ElecNokonoko.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Enemy/Gesso.hpp>
#include <Enemy/HamuKuri.hpp>
#include <Enemy/HauntLeg.hpp>
#include <Enemy/Launcher.hpp>
#include <Enemy/MameGesso.hpp>
#include <Enemy/NameKuri.hpp>
#include <Enemy/Pakkun.hpp>
#include <Enemy/PoiHana.hpp>
#include <Enemy/popo.hpp>
#include <Enemy/RiccoHook.hpp>
#include <Enemy/Rocket.hpp>
#include <Enemy/SmallEnemy.hpp>
#include <Enemy/TamaNoko.hpp>
#include <Enemy/Telesa.hpp>
#include <Enemy/FruitsBoat.hpp>
#include <Enemy/DebuTelesa.hpp>
#include <Enemy/GateKeeper.hpp>
#include <Enemy/TypicalEnemy.hpp>
#include <Enemy/WalkerEnemy.hpp>
#include <Enemy/FireWanwan.hpp>
#include <Enemy/beam.hpp>
#include <Enemy/Amenbo.hpp>
#include <Enemy/Kumokun.hpp>
#include <Enemy/Kukku.hpp>
#include <Enemy/EggGen.hpp>
#include <Enemy/Igaiga.hpp>
#include <Enemy/Kazekun.hpp>
#include <Enemy/AmiNoko.hpp>
#include <Enemy/bombhei.hpp>
#include <Enemy/Cannon.hpp>
#include <Enemy/ChuuHana.hpp>
#include <Enemy/HanaSambo.hpp>
#include <Enemy/killer.hpp>
#include <Enemy/Seal.hpp>
#include <Enemy/TabePuku.hpp>
#include <Enemy/TobiPuku.hpp>
#include <Enemy/WireTrap.hpp>
#include <Enemy/Yumbo.hpp>
#include <Animal/AnimalBase.hpp>
#include <Animal/AnimalManager.hpp>
#include <Animal/BeeHive.hpp>
#include <Animal/Bird.hpp>
#include <Animal/Butterfly.hpp>
#include <Animal/fishoid.hpp>

// rogue include: puts the dummy string pair and the MActor mtx-calc names in
// .rodata ahead of the real name table, which is what the original TU did
#include <M3DUtil/InfectiousStrings.hpp>

// All 140 strcmp branches are present, in the ROM's order, with the ROM's
// strings (140/140) -- this function matches 100%. The only thing still off in
// this TU is a 4-byte ctor stub: the ROM calls
// JGeometry::TRotation3<TMatrix34<SMatrix34C<f32>>>::TRotation3() on
// TSimpleEffect's +0x48 member and emits that 4-byte `blr` stub here, while
// MWCC folds TRotation3's (empty) body into its base and calls
// TMatrix34<SMatrix34C<f32>>::TMatrix34() instead. Both stubs are 4 bytes of
// `blr`; picking the right one needs a change in
// libs/JSystem/include/JSystem/JGeometry/JGRotation3.hpp, which is off limits.
JDrama::TNameRef* TMarNameRefGen::getNameRef_Enemy(const char* name) const
{

	if (strcmp(name, "TypicalEnemy") == 0)
		return new TTypicalEnemy;

	if (strcmp(name, "TypicalManager") == 0)
		return new TTypicalManager;

	if (strcmp(name, "RiccoHook") == 0)
		return new TRiccoHook;

	if (strcmp(name, "RiccoHookManager") == 0)
		return new TRiccoHookManager;

	if (strcmp(name, "EnemyManager") == 0)
		return new TEnemyManager;

	if (strcmp(name, "FruitsBoat") == 0)
		return new TFruitsBoat("フルーツ運搬船");

	// PAL names the three colour variants B/C/D; the G/P/Y spelling seen in
	// other dumps of this function does not exist in the PAL string pool.
	if (strcmp(name, "FruitsBoatB") == 0)
		return new TFruitsBoat("フルーツ運搬船");

	if (strcmp(name, "FruitsBoatC") == 0)
		return new TFruitsBoat("フルーツ運搬船");

	if (strcmp(name, "FruitsBoatD") == 0)
		return new TFruitsBoat("フルーツ運搬船");

	if (strcmp(name, "FruitsBoatManager") == 0)
		return new TFruitsBoatManager(0, "フルーツ運搬船マネージャ");

	if (strcmp(name, "FruitsBoatManagerB") == 0)
		return new TFruitsBoatManager(1, "フルーツ運搬船マネージャ");

	if (strcmp(name, "FruitsBoatManagerC") == 0)
		return new TFruitsBoatManager(2, "フルーツ運搬船マネージャ");

	if (strcmp(name, "FruitsBoatManagerD") == 0)
		return new TFruitsBoatManager(3, "フルーツ運搬船マネージャ");

	// actorType: the ROM loads 0x00800001 (lis r4, 0x80 / addi r4, r4, 1).
	if (strcmp(name, "AnimalMew") == 0)
		return new TAnimalBase(0x800001);

	if (strcmp(name, "MewManager") == 0)
		return new TMewManager;

	if (strcmp(name, "AnimalBird") == 0)
		return new TAnimalBird("(幸せの青い)鳥");

	if (strcmp(name, "AnimalBirdManager") == 0)
		return new TAnimalBirdManager("(幸せの青い)鳥マネージャー");

	if (strcmp(name, "FishoidA") == 0)
		return new TFishoid(0, "回遊魚Ａ");

	if (strcmp(name, "FishoidB") == 0)
		return new TFishoid(1, "回遊魚Ｂ");

	if (strcmp(name, "FishoidC") == 0)
		return new TFishoid(2, "回遊魚Ｃ");

	if (strcmp(name, "FishoidD") == 0)
		return new TFishoid(3, "回遊魚Ｄ");

	if (strcmp(name, "FishoidManager") == 0)
		return new TFishoidManager;

	// NB: the header's default for TBeeHive is the short "ハチの巣"; the
	// original table spells the full name out here.
	if (strcmp(name, "BeeHive") == 0)
		return new TBeeHive("ハチの巣とハチ");

	if (strcmp(name, "BeeHiveManager") == 0)
		return new TBeeHiveManager;

	if (strcmp(name, "Butterfly") == 0)
		return new TButterfloid(0, "蝶Ａ");

	if (strcmp(name, "ButterflyB") == 0)
		return new TButterfloid(1, "蝶Ｂ");

	if (strcmp(name, "ButterflyC") == 0)
		return new TButterfloid(2, "蝶Ｃ");

	if (strcmp(name, "ButterflyManager") == 0)
		return new TButterfloidManager("");

	if (strcmp(name, "EggGenerator") == 0)
		return new TEggGenerator;

	if (strcmp(name, "WickedEggGenerator") == 0)
		return new TEggGenerator;

	if (strcmp(name, "EggGenManager") == 0)
		return new TEggGenManager;

	if (strcmp(name, "EffectColumWaterManager") == 0)
		return new TEffectColumWaterManager;

	if (strcmp(name, "EffectBombColumWaterManager") == 0)
		return new TEffectBombColumWaterManager;

	if (strcmp(name, "EffectColumSandManager") == 0)
		return new TEffectColumSandManager;

	if (strcmp(name, "EffectExplosionManager") == 0)
		return new TEffectExplosionManager;

	// TEffectEnemyManager's ctor is inline in effectEnemy.hpp (no
	// __ct__19TEffectEnemyManager in marioEU.MAP), so the ROM calls the base
	// ctor and only patches the vtable.
	if (strcmp(name, "EffectEnemyManager") == 0)
		return new TEffectEnemyManager;

	if (strcmp(name, "EffectEnemy") == 0)
		return new TEffectEnemy("エフェクト敵");

	if (strcmp(name, "HauntLegManager") == 0)
		return new THauntLegManager;

	if (strcmp(name, "HamuKuriManager") == 0)
		return new THamuKuriManager;

	if (strcmp(name, "HamuKuri") == 0)
		return new THamuKuri;

	// The header default here is still the base class' "ハムクリマネージャー";
	// the table uses the variant's own name.
	if (strcmp(name, "HaneHamuKuriManager") == 0)
		return new THaneHamuKuriManager("はねハムクリマネージャー");

	if (strcmp(name, "HaneHamuKuri") == 0)
		return new THaneHamuKuri;

	if (strcmp(name, "HaneHamuKuri2") == 0)
		return new THaneHamuKuri2;

	if (strcmp(name, "DoroHaneKuriManager") == 0)
		return new TDoroHaneKuriManager;

	if (strcmp(name, "DoroHaneKuri") == 0)
		return new TDoroHaneKuri;

	if (strcmp(name, "DangoHamuKuriManager") == 0)
		return new TDangoHamuKuriManager;

	if (strcmp(name, "BossDangoHamuKuriManager") == 0)
		return new TBossDangoHamuKuriManager;

	if (strcmp(name, "BossDangoHamuKuri") == 0)
		return new TBossDangoHamuKuri;

	if (strcmp(name, "FireHamuKuriManager") == 0)
		return new TFireHamuKuriManager;

	if (strcmp(name, "FireHamuKuri") == 0)
		return new TFireHamuKuri;

	if (strcmp(name, "DoroHamuKuriManager") == 0)
		return new TDoroHamuKuriManager;

	if (strcmp(name, "DoroHamuKuri") == 0)
		return new TDoroHamuKuri;

	if (strcmp(name, "NameKuriManager") == 0)
		return new TNameKuriManager;

	if (strcmp(name, "NameKuri") == 0)
		return new TNameKuri;

	if (strcmp(name, "DiffusionNameKuriManager") == 0)
		return new TDiffusionNameKuriManager;

	if (strcmp(name, "PakkunManager") == 0)
		return new TPakkunManager;

	if (strcmp(name, "Pakkun") == 0)
		return new TPakkun;

	// TStayPakkun's ctor is inline in Pakkun.hpp (no __ct__11TStayPakkun in
	// marioEU.MAP), so the ROM calls TPakkun's and patches the vtable.
	if (strcmp(name, "StayPakkun") == 0)
		return new TStayPakkun;

	if (strcmp(name, "HanaSamboManager") == 0)
		return new THanaSamboManager("ハナサンボマネージャー");

	if (strcmp(name, "HanaSambo") == 0)
		return new THanaSambo("ハナサンボ");

	if (strcmp(name, "SamboHeadManager") == 0)
		return new TSamboHeadManager("サンボヘッドマネージャー");

	if (strcmp(name, "SamboHead") == 0)
		return new TSamboHead("サンボヘッド");

	if (strcmp(name, "YumboManager") == 0)
		return new TYumboManager("ユンボマネージャー");

	// TSamboFlowerManager's ctor is inline in HanaSambo.hpp (no
	// __ct__19TSamboFlowerManager in marioEU.MAP), so the ROM calls
	// TEnemyManager's, patches the vtable and zeroes +0x54..+0x60.
	if (strcmp(name, "SamboFlowerManager") == 0)
		return new TSamboFlowerManager;

	if (strcmp(name, "SamboFlower") == 0)
		return new TSamboFlower("サンボフラワー");

	if (strcmp(name, "PopoManager") == 0)
		return new TPopoManager;

	if (strcmp(name, "Popo") == 0)
		return new TPopo;

	if (strcmp(name, "GorogoroManager") == 0)
		return new TGorogoroManager;

	if (strcmp(name, "GessoManager") == 0)
		return new TGessoManager;

	if (strcmp(name, "Gesso") == 0)
		return new TGesso;

	if (strcmp(name, "SurfGesso") == 0)
		return new TSurfGesso;

	if (strcmp(name, "LandGesso") == 0)
		return new TLandGesso;

	if (strcmp(name, "MameGessoManager") == 0)
		return new TMameGessoManager;

	if (strcmp(name, "MameGesso") == 0)
		return new TMameGesso;

	if (strcmp(name, "ElecNokonokoManager") == 0)
		return new TElecNokonokoManager;

	if (strcmp(name, "ElecNokonoko") == 0)
		return new TElecNokonoko;

	if (strcmp(name, "TelesaManager") == 0)
		return new TTelesaManager;

	if (strcmp(name, "Telesa") == 0)
		return new TTelesa;

	if (strcmp(name, "SeeTelesa") == 0)
		return new TSeeTelesa;

	if (strcmp(name, "LoopTelesa") == 0)
		return new TLoopTelesa;

	if (strcmp(name, "BoxTelesa") == 0)
		return new TBoxTelesa;

	if (strcmp(name, "MarioModokiTelesa") == 0)
		return new TMarioModokiTelesa;

	if (strcmp(name, "DebuTelesaManager") == 0)
		return new TDebuTelesaManager;

	if (strcmp(name, "DebuTelesa") == 0)
		return new TDebuTelesa;

	if (strcmp(name, "PoiHanaManager") == 0)
		return new TPoiHanaManager;

	if (strcmp(name, "PoiHana") == 0)
		return new TPoiHana;

	if (strcmp(name, "PoiHanaRed") == 0)
		return new TPoiHanaRed;

	if (strcmp(name, "SleepPoiHana") == 0)
		return new TSleepPoiHana;

	if (strcmp(name, "ChuuHanaManager") == 0)
		return new TChuuHanaManager;

	if (strcmp(name, "FireWanwanManager") == 0)
		return new TFireWanwanManager;

	if (strcmp(name, "FireWanwan") == 0)
		return new TFireWanwan;

	if (strcmp(name, "TobiPukuLaunchPadManager") == 0)
		return new TTobiPukuLaunchPadManager("とびプク発射台マネージャー");

	if (strcmp(name, "TobiPukuLaunchPad") == 0)
		return new TTobiPukuLaunchPad("とびプク発射台");

	if (strcmp(name, "MoePukuLaunchPadManager") == 0)
		return new TMoePukuLaunchPadManager;

	if (strcmp(name, "MoePukuLaunchPad") == 0)
		return new TMoePukuLaunchPad;

	if (strcmp(name, "TobiPukuManager") == 0)
		return new TTobiPukuManager("とびプクマネージャー");

	if (strcmp(name, "MoePukuManager") == 0)
		return new TMoePukuManager;

	if (strcmp(name, "PukuPuku") == 0)
		return new TPukuPuku("プクプク");

	if (strcmp(name, "MoePuku") == 0)
		return new TMoePuku;

	// TODO: the two header defaults here are still the placeholder
	// "たべプク"/"たべプクマネージャー"; the table uses the full names.
	if (strcmp(name, "TabePukuManager") == 0)
		return new TTabePukuManager("プクプク(レール巡回)マネージャー");

	if (strcmp(name, "TabePuku") == 0)
		return new TTabePuku("プクプク(レール巡回)");

	// TODO: same for TWireTrapManager / TWireTrap ("ワイヤートラップ...").
	if (strcmp(name, "WireTrapManager") == 0)
		return new TWireTrapManager("電線トラップマネージャー");

	if (strcmp(name, "WireTrap") == 0)
		return new TWireTrap("電線トラップ");

	if (strcmp(name, "RocketManager") == 0)
		return new TRocketManager("ロケットマネージャー");

	if (strcmp(name, "Rocket") == 0)
		return new TRocket;

	if (strcmp(name, "Yumbo") == 0)
		return new TYumbo("ユンボ");

	if (strcmp(name, "CannonManager") == 0)
		return new TCannonManager("砲台マネージャー");

	if (strcmp(name, "Cannon") == 0)
		return new TCannon("砲台");

	// TODO: the two header defaults here are still the placeholder
	// "ボムヘイ"/"ボムヘイマネージャー"; the table uses "ボム兵".
	if (strcmp(name, "BombHeiManager") == 0)
		return new TBombHeiManager("ボム兵マネージャー");

	if (strcmp(name, "BombHei") == 0)
		return new TBombHei("ボム兵");

	if (strcmp(name, "KillerManager") == 0)
		return new TKillerManager("キラーマネージャー");

	if (strcmp(name, "Killer") == 0)
		return new TKiller;

	if (strcmp(name, "AmiNokoManager") == 0)
		return new TAmiNokoManager;

	if (strcmp(name, "AmiNoko") == 0)
		return new TAmiNoko;

	if (strcmp(name, "KageMarioModokiManager") == 0)
		return new TKageMarioModokiManager;

	if (strcmp(name, "KageMarioModoki") == 0)
		return new TKageMarioModoki;

	if (strcmp(name, "CommonLauncher") == 0)
		return new TCommonLauncher;

	if (strcmp(name, "CommonLauncherManager") == 0)
		return new TCommonLauncherManager;

	if (strcmp(name, "GateKeeper") == 0)
		return new TBiancoGateKeeper;

	if (strcmp(name, "GateKeeperManager") == 0)
		return new TBiancoGateKeeperManager;

	if (strcmp(name, "OrangeSeal") == 0)
		return new TSeal("シール");

	if (strcmp(name, "SealManager") == 0)
		return new TSealManager("シールマネージャ");

	// TEMP-OUT
	// THamuKuriLauncherManager / TNameKuriLauncherManager are declared in
	// Launcher.hpp with inline constructors and implicit destructors; the ROM
	// emits their vtables (0x54) and destructors (0x74) in this very TU.
	if (strcmp(name, "HamukuriLauncher") == 0)
		return new THamuKuriLauncher;

	if (strcmp(name, "HamukuriLauncherManager") == 0)
		return new THamuKuriLauncherManager;

	if (strcmp(name, "NamekuriLauncher") == 0)
		return new TNameKuriLauncher;

	if (strcmp(name, "NamekuriLauncherManager") == 0)
		return new TNameKuriLauncherManager;

	if (strcmp(name, "IgaigaManager") == 0)
		return new TIgaigaManager;

	if (strcmp(name, "TamaNokoManager") == 0)
		return new TTamaNokoManager;

	if (strcmp(name, "TamaNoko") == 0)
		return new TTamaNoko;

	if (strcmp(name, "BeamManager") == 0) {
		gpBeamManager = new TBeamManager;
		return gpBeamManager;
	}

	// TODO: the two header defaults here are still "クッククマネージャー"
	// and the katakana "カゼクン"/"カゼクンマネージャー".
	if (strcmp(name, "KukkuManager") == 0)
		return new TKukkuManager("クックマネージャー");

	if (strcmp(name, "Kukku") == 0)
		return new TKukku;

	if (strcmp(name, "KumokunManager") == 0)
		return new TKumokunManager("くもくんマネージャー");

	if (strcmp(name, "Kumokun") == 0)
		return new TKumokun;

	if (strcmp(name, "AmenboManager") == 0)
		return new TAmenboManager;

	if (strcmp(name, "Amenbo") == 0)
		return new TAmenbo;

	if (strcmp(name, "KazekunManager") == 0)
		return new TKazekunManager("かぜくんマネージャ");

	if (strcmp(name, "Kazekun") == 0)
		return new TKazekun("かぜくん");

	// TSimpleEffect's ctor is inline in EffectObj.hpp (no __ct__13TSimpleEffect
	// in marioEU.MAP), so the whole thing is expanded here: the
	// JDrama::TActor ctor, the vtable patch, unk44 = 1, the +0x48 ctor call --
	// and because JDrama::TActor has a non-trivial destructor the fresh object
	// is spilled to the stack, which is what makes this function's frame 0x28
	// instead of 0x18.
	if (strcmp(name, "EffectPinnaFunsui") == 0)
		return new TEffectPinnaFunsui;

	if (strcmp(name, "EffectBiancoFunsui") == 0)
		return new TEffectBiancoFunsui;

	return nullptr;
}
