#ifndef ENEMY_BOSS_WANWAN_HPP
#define ENEMY_BOSS_WANWAN_HPP

#include <Strategic/Nerve.hpp>
#include <Enemy/Enemy.hpp>
#include <System/ParamInst.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/Binder.hpp>
#include <Strategic/TakeActor.hpp>
#include <M3DUtil/M3UJoint.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorAnm.hpp>
#include <M3DUtil/MActorData.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>

class TLiveActor;

class TBWParams : public TSpineEnemyParams {
public:
	TBWParams(const char*);

	/* 0x0A8 */ TParamRT<f32> mSLMarchSpeed;
	/* 0x0BC */ TParamRT<f32> mSLTurnSpeed;
	/* 0x0D0 */ TParamRT<f32> mSLLeashNodeLen;
	/* 0x0E4 */ TParamRT<f32> mSLPicketHeight;
	/* 0x0F8 */ TParamRT<f32> mSLPicketRadius;
	/* 0x10C */ TParamRT<f32> mSLChainHitHeight;
	/* 0x120 */ TParamRT<f32> mSLChainHitRadius;
	/* 0x134 */ TParamRT<f32> mSLChainGroundRadius;
	/* 0x148 */ TParamRT<f32> mSLPullLimit;
	/* 0x15C */ TParamRT<f32> mSLAttackSpeed;
	/* 0x170 */ TParamRT<s32> mSLStunTimer;
	/* 0x184 */ TParamRT<f32> mSLSearchLength;
	/* 0x198 */ TParamRT<f32> mSLSearchAngle;
	/* 0x1AC */ TParamRT<u8> mSLBWHitPointMax;
	/* 0x1C0 */ TParamRT<f32> mSLHeadGap;
	/* 0x1D4 */ TParamRT<f32> mSLShakeLengthMax;
	/* 0x1E8 */ TParamRT<f32> mSLShakeLengthMaxHP0;
};

class TBossWanwan;
class TBWLeash;
class TRope;

class TBWBinder : public TBinder {
public:
	virtual ~TBWBinder() { }
	virtual void bind(TLiveActor*);
};

class TBossWanwanMtxCalc : public M3UMtxCalcSIAnmBlendQuat {
public:
	// `basic` is always false and the blend ratio is always reset to 0 here;
	// the remaining store of 0 goes to the J3DMtxCalc virtual base's tail.
	TBossWanwanMtxCalc(TBossWanwan* owner)
	    : M3UMtxCalcSIAnmBlendQuat(false)
	    , mOwner(owner)
	{
		mMotionBlendRatio = 0.0f;
	}

	virtual ~TBossWanwanMtxCalc() { }
	virtual void calc(u16);

	void joinAnm(int index);

public:
	/* 0x64 */ TBossWanwan* mOwner;
};

class TBWHit : public THitActor {
public:
	TBWHit(TBossWanwan* owner, int jointIndex, const char* name)
	    : THitActor(name)
	    , mOwner(owner)
	    , mJointIndex(jointIndex)
	{
	}
	virtual ~TBWHit() { }
	virtual void perform(u32, JDrama::TGraphics*);
	virtual BOOL receiveMessage(THitActor*, u32);

public:
	/* 0x68 */ TBossWanwan* mOwner;
	/* 0x6C */ int mJointIndex;
};

class TBWPicket : public TTakeActor {
public:
	TBWPicket(TBossWanwan* owner, const char* name)
	    : TTakeActor(name)
	    , mOwner(owner)
	    , mTakingMtx()
	    , mPicketMActor(nullptr)
	{
		MTXIdentity(mTakingMtx.mMtx);
	}
	virtual ~TBWPicket() { }
	virtual void perform(u32, JDrama::TGraphics*);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual MtxPtr getTakingMtx();
	virtual BOOL moveRequest(const JGeometry::TVec3<f32>&);

public:
	/* 0x70 */ TBossWanwan* mOwner;
	/* 0x74 */ JGeometry::SMatrix34C<f32> mTakingMtx; ///< returned by
	                                             ///< getTakingMtx()
	/* 0xA4 */ MActor* mPicketMActor; ///< chain model of this stake
};

class TBWLeash;

class TBWLeashNode : public THitActor {
public:
	TBWLeashNode(TBWLeash* leash, int index, const char* name)
	    : THitActor(name)
	    , mLeash(leash)
	    , mIndex(index)
	{
	}
	virtual ~TBWLeashNode() { }
	virtual void perform(u32, JDrama::TGraphics*);

	void calcMatrix();
	void calcTemperature();

public:
	/* 0x68 */ TBWLeash* mLeash;
	/* 0x6C */ MActor* mMActor;
	/* 0x70 */ f32 mTemperature;
	/* 0x74 */ int mIndex;
};

class TBWLeash : public JDrama::TViewObj {
public:
	TBWLeash(TBossWanwan*, int, const char*);
	virtual ~TBWLeash() { }
	virtual void perform(u32, JDrama::TGraphics*);

public:
	/* 0x10 */ TBossWanwan* mOwner; // TODO: guessed
	/* 0x14 */ TRope* mRope;
	/* 0x18 */ TBWLeashNode** mNodes;
};

class TBossWanwanManager : public TEnemyManager {
public:
	TBossWanwanManager(const char* name);
	virtual ~TBossWanwanManager() { }
	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();

	void initJParticle();
};

class TBossWanwan : public TSpineEnemy {
public:
	TBossWanwan(const char* name);
	virtual ~TBossWanwan() { }
	virtual void perform(u32, JDrama::TGraphics*);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void control();
	virtual void kill() { }

	void emitEffects();
	void slideToCurPathNode(f32, f32);
	void shakeCamera(int);
	void changeBck(int);

	TBWParams* getBWParams() const { return (TBWParams*)getSaveParam(); }

public:
	/* 0x150 */ TBossWanwanMtxCalc* mMtxCalc;
	/* 0x154 */ TBWLeash* mLeash;
	/* 0x158 */ TBWPicket* unk158; // TODO: name is a guess
	/* 0x15C */ JGeometry::TVec3<f32> unk15C;
	/* 0x168 */ f32 unk168;
	/* 0x16C */ int unk16C;
	/// The two hit boxes. The ROM walks this pair as one 2-element array
	/// (`this + i*4 + 0x170`), so keep it as an array.
	/* 0x170 */ TBWHit* unk170[2];
	/* 0x178 */ f32 unk178; // 1 / frame count of the current bck
	/* 0x17C */ int unk17C;
	/* 0x180 */ int unk180;
	/* 0x184 */ int unk184;
	/* 0x188 */ int unk188;
	/* 0x18C */ s8 unk18C;
	/* 0x18D */ bool unk18D;
	/* 0x190 */ int unk190;
	/* 0x194 */ s8 unk194;
	/* 0x195 */ s8 unk195;
	/* 0x198 */ int unk198;
	/* 0x19C */ int unk19C;
	/* 0x1A0 */ s8 unk1A0;
	/* 0x1A4 */ JGeometry::TVec3<f32> unk1A4; ///< scratch position for
	                                        ///< the 0x168 particle emitter
	/* 0x1B0 */ int unk1B0;
	/* 0x1B4 */ s16 unk1B4;
};

DECLARE_NERVE(TNerveBWGraphWander, TLiveActor);
DECLARE_NERVE(TNerveBWRoll, TLiveActor);
DECLARE_NERVE(TNerveBWBark, TLiveActor);
DECLARE_NERVE(TNerveBWJump, TLiveActor);
DECLARE_NERVE(TNerveBWStun, TLiveActor);
DECLARE_NERVE(TNerveBWWakeup, TLiveActor);
DECLARE_NERVE(TNerveBWJumpToBath, TLiveActor);
DECLARE_NERVE(TNerveBWDie, TLiveActor);
DECLARE_NERVE(TNerveBWJumpAway, TLiveActor);
DECLARE_NERVE(TNerveBWShake, TLiveActor);
DECLARE_NERVE(TNerveBWFall, TLiveActor);

#endif
