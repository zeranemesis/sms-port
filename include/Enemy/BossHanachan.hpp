#ifndef ENEMY_BOSS_HANACHAN_HPP
#define ENEMY_BOSS_HANACHAN_HPP

#include <Strategic/Nerve.hpp>
#include <Enemy/Enemy.hpp>

class TLiveActor;
class TLiveManager;

class TBossHanachan {
public:
	static void staticLoadParticle();
};

DECLARE_NERVE(TNerveSBH_Fall, TLiveActor);
DECLARE_NERVE(TNerveSBH_SleepContinue, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanDead, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanSnort, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanDamage, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanGetUp, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanDown, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanTumble, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanGraphWander, TLiveActor);

// Ordinal values verified against the li r4, N arguments passed to
// TBossHanachan::considerSetAnm() at each TNerveBossHanachan*::execute() call
// site (BossHanachanNerve.cpp). Enumerator names are descriptive labels tied
// to the nerve that uses each value, not recovered from the binary.
enum EnumBossHanachanNerveAnm {
	NERVE_ANM_TUMBLE = 0,
	NERVE_ANM_DOWN   = 1,
	NERVE_ANM_GETUP  = 2,
	NERVE_ANM_DAMAGE = 3,
	NERVE_ANM_SNORT  = 4,
	NERVE_ANM_DEAD   = 5,
};

// Ordinal values verified from the li r4, N arguments passed to
// TBossHanachan::isAllBckAlreadyEnd() / setHeadAndBodyAnm() in
// BossHanachanNerve.cpp (0x0, 0xE, 0xF observed so far). Enumerator names are
// placeholders pending disassembly of the functions that fully define this
// enum's meaning (owned by BossHanachanAnm.cpp / BossHanachanParts.cpp).
enum EnumBossHanachanAnmKind {
	ANM_KIND_0  = 0x0,
	ANM_KIND_E  = 0xE,
	ANM_KIND_F  = 0xF,
};

// Values verified as 0/1 from the li r4/r5 arguments in
// TBossHanachan::setTumbleAnm() / setHeadAndBodyAnm() call sites.
enum EnumBossHanachanStopMotionBlendOnOff {
	STOP_MOTION_BLEND_OFF = 0,
	STOP_MOTION_BLEND_ON  = 1,
};

class TBossHanachan : public TSpineEnemy {
public:
	TBossHanachan(const char*);

	void considerSetAnm(EnumBossHanachanNerveAnm);
	bool isAllBckAlreadyEnd(EnumBossHanachanAnmKind) const;
	void goToInitialRecoverGraphNode();
	void execSlip();
	void setAnmTimerWhenGetUp();
	bool isFinishedGetUp() const;
	void setRandomWeakBodyIndex();
	void setAnmTimerWhenSnort();
	void setTumbleAnm(EnumBossHanachanStopMotionBlendOnOff);
	bool isTumbleCompletelyAllBody() const;
	void setHeadAndBodyAnm(EnumBossHanachanAnmKind, EnumBossHanachanStopMotionBlendOnOff);
	void execWalk(bool);
	bool checkFallDecideAndSetup();
	void removeAllMapCollision();

public:
	// Object size (0x1C4) verified from `li r3, 0x1C4` before
	// `bl TBossHanachan::TBossHanachan(const char*)` in
	// TMarNameRefGen::getNameRef_BossEnemy() (src/System/MarNameRefGen_BossEnemy.cpp).
	// TSpineEnemy ends at 0x150 (Enemy.hpp); the region below is not yet
	// broken out into named fields except mUnk1C0, whose offset (and the
	// 0x1BC / 0x1A8 offsets read through it in TNerveBossHanachanDamage /
	// TNerveBossHanachanDown::execute) are read directly from disassembly.
	/* 0x150 */ u8 unk150[0x70];
	/* 0x1C0 */ u8* mUnk1C0;
};

#endif
