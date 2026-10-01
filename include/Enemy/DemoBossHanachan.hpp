#ifndef ENEMY_DEMOBOSSHANACHAN_HPP
#define ENEMY_DEMOBOSSHANACHAN_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <JSystem/JDrama/JDRGraphics.hpp>
#include <System/BaseParam.hpp>
#include <System/ParamInst.hpp>
#include <System/Params.hpp>
#include <dolphin/types.h>

class THitActor;
class TLiveManager;
class TDemoBossHanachan;
class TDemoBossHanachanManager;
class TDemoBossHanachanSaveParams;

class TDemoBossHanachan : public TSpineEnemy {
public:
	// The ROM inlines this constructor at the name-factory call site: it calls
	// TSpineEnemy("?") and then stores the TDemoBossHanachan vtable, so the
	// original took the name and forwarded it to the base.
	TDemoBossHanachan(const char* name = "?")
	    : TSpineEnemy(name)
	{
	}
	virtual BOOL receiveMessage(THitActor*, u32);

	void initBase(TLiveManager*, u32);
};

class TDemoBossHanachanManager : public TEnemyManager {
public:
	// There is no __ct__24TDemoBossHanachanManagerF* in mario.MAP, so the
	// original constructor was inline; System/MarNameRefGen_BossEnemy.cpp
	// expands it: TEnemyManager(name), the vtable patch, then the save params.
	TDemoBossHanachanManager(const char* name = "?");
	virtual void clipEnemies(JDrama::TGraphics*);

public:
	/* 0x54 */ TDemoBossHanachanSaveParams* mSaveParams;
};

class TDemoBossHanachanSaveParams : public TParams {
public:
	TDemoBossHanachanSaveParams(const char*);

public:
	/* 0x8  */ TParamRT<f32> mSLViewClipFar;
	/* 0x1C */ TParamRT<f32> mSLViewClipRadius;
};

// Defined out of class so that TDemoBossHanachanSaveParams is already complete
// here -- the body newses one up.
inline TDemoBossHanachanManager::TDemoBossHanachanManager(const char* name)
    : TEnemyManager(name)
{
	mSaveParams = new TDemoBossHanachanSaveParams("/enemy/sleepBossHanachan.prm");
}

#endif // ENEMY_DEMOBOSSHANACHAN_HPP
