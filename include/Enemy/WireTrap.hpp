#ifndef ENEMY_WIRE_TRAP_HPP
#define ENEMY_WIRE_TRAP_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Enemy/WireBinder.hpp>
#include <Strategic/Nerve.hpp>

class TWireTrap;

// TWireTrapParams: TSpineEnemyParams plus the three params the wire trap
// nerve chain reads.  Offsets recovered from TWireTrapManager::load, which
// news the object (0xE4 bytes) and then constructs the three TParamRTs at
// 0xA8/0xBC/0xD0 with the keycodes/names/defaults below.  The constructor is
// out of line in the .cpp: mario.MAP lists it as UNUSED, and UNUSED symbols
// are never weak.
class TWireTrapParams : public TSpineEnemyParams {
public:
	TWireTrapParams(const char* path);

	/* 0xA8 */ TParamRT<f32> mInWaterPowerRate;
	/* 0xBC */ TParamRT<s32> mScaleTimerMax;
	/* 0xD0 */ TParamRT<s32> mGoTimerMax;
};

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

	virtual ~TWireTrapManager() { }
	virtual void load(JSUMemoryInputStream&);

	void createModelData();
};

class TWireTrap : public TSpineEnemy {
public:
	TWireTrap(const char* name = "ワイヤートラップ");

	virtual ~TWireTrap() { }
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

	// TODO: bodies reconstructed from the call sites in the nerve chain and
	// from mario.MAP, which lists all of these as UNUSED (fully inlined).
	// doScaleUp/doScaleDown are the bodies of the matching nerve execute().
	void setMoveMode(int mode);
	bool isReflect() const;
	bool isStartWire() const;
	bool isEndWire() const;
	f32 getRangePosInWire() const;
	f32 getWaterPow() const;
	void emitEffects();
	void updateCollision();
	void doResetToEdge();
	bool doScaleDown();
	// NOTE: the target tests the inlined doScaleUp() result with
	// `clrlwi. r0, r3, 24` (a byte-wide bool test) rather than the `cmpwi`
	// an int return would give, so this returns bool even though doScaleDown
	// was originally written as BOOL.
	bool doScaleUp();
	// UNUSED in the map (it only exists so the nerve bodies read well); the
	// target inlines it into the nerves, so it must stay inlinable here.
	void doSearchMove();
	void doOnewayMove();
	void doReturnMove();
	void calcMomentum();
	JGeometry::TVec3<f32> getDirAtWirePos() const;
	void initWire();
	void initParticle();
	void initCollision();
	void initThisColor(const GXColorS10*);
	// TODO: TWireTrap* parameter and the by-const-ref Vec arguments come
	// straight from the mangled name; the bodies are not reconstructed yet.
	void behaveHitWireTrap(TWireTrap*, const JGeometry::TVec3<f32>&,
	                       const JGeometry::TVec3<f32>&);
	void behaveHitWater(THitActor*);

	// TODO: mario.MAP also lists a non-const `getWireBinder()` (8 bytes,
	// UNUSED).  Declaring that overload here would make the non-const call
	// sites in checkHitActors()/calcRootMatrix() bind to it, but the target
	// calls the const overload everywhere, so it is deliberately left out.

	// fabricated accessor
	TWireTrapParams* getWireTrapParams() const
	{
		return (TWireTrapParams*)getSaveParam();
	}
	// TODO: reconstructed from codegen.  TWireTrap::moveObject re-reads
	// mScaleTimer for the decrement after testing it, which is the signature
	// of a by-value getter (the return value is force-loaded into a
	// compiler temporary rather than shared with the caller's read).
	// NOTE: none of the getter/operator spellings actually stops MWCC from
	// folding the two loads together, so both re-reads are still missing
	// (one instruction in moveObject and in each of the three move nerves).
	int getScaleTimer() const { return mScaleTimer; }
	void setScaleTimer(int timer) { mScaleTimer = timer; }
	// Same reconstruction for mSearchTimer: the three nerves that tick it
	// re-read the field between the test and the decrement.
	int getSearchTimer() const { return mSearchTimer; }
	void setSearchTimer(int timer) { mSearchTimer = timer; }

public:
	/* 0x150 */ f32 unk150;
	/* 0x154 */ f32 unk154;
	/* 0x158 */ f32 unk158;
	/* 0x15C */ f32 unk15C;
	/* 0x160 */ int mMoveMode;
	/* 0x164 */ int mScaleTimer;
	/* 0x168 */ f32 mScale;
	/* 0x16C */ int mSearchTimer;
	// 0x170 is which way along the wire the trap faces: load() sets it to
	// +/-1.0 from the actor's Y rotation, doSearchMove() overwrites it with
	// (f32)(dot(mario - pos, wireDir) > 0 ? 1 : < 0 ? -1 : 0), it is negated
	// by TNerveWireTrapReturnMove and fed to TWireBinder::getPoint() by
	// TNerveWireTrapOnewayMoveEnd.  "mWireLength" is a guess.
	/* 0x170 */ f32 mWireLength;
	/* 0x174 */ int mWaitTime; // compared against the spine timer in TNerveWireTrapWait
	/* 0x178 */ f32 mMomentum;
	/* 0x17C */ f32 mScaleRate;
	/* 0x180 */ s16 mWireNumber;
};

#endif
