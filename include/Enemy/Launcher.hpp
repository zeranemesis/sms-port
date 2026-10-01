#ifndef ENEMY_LAUNCHER_HPP
#define ENEMY_LAUNCHER_HPP

#include <Enemy/EnemyManager.hpp>
#include <Enemy/Enemy.hpp>

class TLauncherParams : public TSpineEnemyParams {
public:
	TLauncherParams(const char* path);

	s32 getLaunchPeriod() const { return mSLLaunchPeriod.get(); }

	/* 0xA8 */ TParamRT<s32> mSLLaunchPeriod;
};

class TLauncher : public TSpineEnemy {
public:
	enum {
		STATE_INITIAL    = 0,
		STATE_HITBYWATER = 1,
		STATE_NORMAL     = 2,
		STATE_LAUNCH     = 3,
		STATE_DIE        = 4,
	};

	TLauncher(const char* name);

	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init(TLiveManager*);
	virtual void control();
	virtual void bind();
	virtual void changeState(int);
	virtual void stateInitial();
	virtual void stateHitByWater();
	virtual void stateNormal();
	virtual void stateLaunch();
	virtual void stateDie();

	void resetLaunchTimer();
	TSpineEnemy* getProperEnemy(const char*);

	// fabricated
	TLauncherParams* getSaveParam2() const
	{
		return (TLauncherParams*)getSaveParam();
	}

public:
	/* 0x150 */ int mState;
	/* 0x154 */ int mNextState;
	/* 0x158 */ int mTicksSpentInCurState;
	/* 0x15C */ int mLaunchCooldown;
	/* 0x160 */ int mRegenTimer;
};

class TLauncherManager : public TEnemyManager {
public:
	TLauncherManager(const char* name);
	virtual void load(JSUMemoryInputStream&);
};

// The two launcher flavours. Neither declares a destructor and neither adds a
// virtual of its own: both vtables are 0x54 bytes, exactly TLauncherManager's,
// and marioEU.MAP lists no other member. The only symbols either class has
// anywhere in the binary are its vtable (weak) and its implicit destructor
// (weak, 0x74 bytes), and MarNameRefGen_Enemy is the one TU that emits both --
// because the classes have no out-of-line virtuals, so there is no TU that
// owns their vtable. The implicit ~TLauncherManager is what gets inlined into
// them (it sets __vt__16TLauncherManager and then calls
// __dt__13TEnemyManagerFv directly, never __dt__16TLauncherManagerFv).
class THamuKuriLauncherManager : public TLauncherManager {
public:
	THamuKuriLauncherManager(const char* name = "ハムクリランチャーマネージャー")
	    : TLauncherManager(name)
	{
	}
};

class TNameKuriLauncherManager : public TLauncherManager {
public:
	TNameKuriLauncherManager(const char* name = "ナメクリランチャーマネージャー")
	    : TLauncherManager(name)
	{
	}
};

class TCommonLauncher : public TLauncher {
public:
	TCommonLauncher(const char* name = "コモンランチャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void init(TLiveManager*);
	virtual const char** getBasNameTable() const;
	virtual void stateInitial();
	virtual void stateHitByWater();
	virtual void stateNormal();
	virtual void stateLaunch();
	virtual void stateDie();

	void changeBck(int);

public:
	/* 0x164 */ const char* unk164;
	/* 0x168 */ s32 mLaunchPeriod;
};

class TCommonLauncherManager : public TLauncherManager {
public:
	TCommonLauncherManager(const char* name = "コモンランチャーマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();

	void initJParticle();
};

template <class T> class TNerveWaitForever : public TNerveBase<T> {
public:
	virtual BOOL execute(TSpineBase<T>*) const { return false; }

	static const TNerveWaitForever& theNerve()
	{
		static TNerveWaitForever instance;
		return instance;
	}
};

#endif
