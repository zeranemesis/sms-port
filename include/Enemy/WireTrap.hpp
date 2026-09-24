#ifndef ENEMY_WIRE_TRAP_HPP
#define ENEMY_WIRE_TRAP_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Enemy/WireBinder.hpp>
#include <Strategic/Nerve.hpp>

class TWireTrap;

// TODO: nerve bodies are not yet reconstructed (execute() logic still unknown).
DECLARE_NERVE(TNerveWireTrapGoWait, TLiveActor)
DECLARE_NERVE(TNerveWireTrapWait, TLiveActor)
DECLARE_NERVE(TNerveWireTrapSearch, TLiveActor)
DECLARE_NERVE(TNerveWireTrapOnewayMoveStart, TLiveActor)
DECLARE_NERVE(TNerveWireTrapOnewayMove, TLiveActor)
DECLARE_NERVE(TNerveWireTrapOnewayMoveEnd, TLiveActor)
DECLARE_NERVE(TNerveWireTrapReturnMove, TLiveActor)

class TWireTrapManager : public TEnemyManager {
public:
	TWireTrapManager(const char* name = "ワイヤートラップマネージャ");

	virtual ~TWireTrapManager();
	virtual void load(JSUMemoryInputStream&);
	virtual TSpineEnemy* createEnemyInstance() { return nullptr; } // TODO

	void createModelData();
};

class TWireTrap : public TSpineEnemy {
public:
	TWireTrap(const char* name = "ワイヤートラップ");

	virtual ~TWireTrap();
	virtual void load(JSUMemoryInputStream&);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void moveObject();
	virtual void kill();
	virtual BOOL receiveMessage(THitActor*, u32);

	void checkHitActors();
	TNerveBase<TLiveActor>* getNerveFromMode(int mode);
	TWireBinder* getWireBinder() const;
	const JGeometry::TVec3<f32>& getWireDir() const;

private:
	// TODO: remaining TWireTrap fields (mode/timer at 0x160/0x164 relative to
	// TSpineEnemy base, plus scale/vec fields around 0x24-0x5c) are not yet
	// reconstructed; only functions not touching them are implemented so far.
public:
	/* 0x150 */ u8 unk150[0x24];
	/* 0x174 */ int mWaitTime; // compared against the spine timer in TNerveWireTrapWait
};

#endif
