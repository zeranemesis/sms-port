#ifndef ENEMY_KOOPA_HPP
#define ENEMY_KOOPA_HPP

#include <Strategic/Nerve.hpp>
#include <Strategic/HitActor.hpp>
#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <M3DUtil/MActor.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <System/ParamInst.hpp>

class TKoopa;

extern f32 SMSGetAnmFrameRate(); // avoid including Application.hpp

class TKoopaParams : public TSpineEnemyParams {
public:
	TKoopaParams(const char* path)
	    : TSpineEnemyParams(path)
	    , PARAM_INIT(turnSpeed, 1.6f)
	    , PARAM_INIT(turnAnim, 3.7f)
	    , PARAM_INIT(waitStep, 600.0f)
	    , PARAM_INIT(downStep, 1000.0f)
	    , PARAM_INIT(attackRadius, 800.0f)
	    , PARAM_INIT(attackHeight, 1000.0f)
	    , PARAM_INIT(focusRange, 2.0f)
	    , PARAM_INIT(waitRange, 12.0f)
	    , PARAM_INIT(fireSpeed, 3.5f)
	    , PARAM_INIT(tumbleWeight, 8.3f)
	    , PARAM_INIT(tumbleSpeed, 2.0f)
	    , PARAM_INIT(tumbleStartFrame, 95.0f)
	    , PARAM_INIT(tumbleEndFrame, 160.0f)
	    , PARAM_INIT(waitSpeed, 2.0f)
	    , PARAM_INIT(staggerSpeed, 2.0f)
	    , PARAM_INIT(downSpeed, 2.0f)
	    , PARAM_INIT(flameVelocity, 35.0f)
	    , PARAM_INIT(flameScale, 1.0f)
	    , PARAM_INIT(flameCount, 500)
	    , PARAM_INIT(flameFocusStartStep, 25)
	    , PARAM_INIT(flameFocusEndStep, 500)
	    , PARAM_INIT(flameRadius, 300.0f)
	    , PARAM_INIT(flameHeight, 1000.0f)
	    , PARAM_INIT(headRadius, 400.0f)
	    , PARAM_INIT(waterhitSpeed, 2.0f)
	    , PARAM_INIT(flameOverStart, 1.0f)
	    , PARAM_INIT(flameNeckRange, 17.0f)
	    , PARAM_INIT(flameNeckDownRate, 0.3f)
	    , PARAM_INIT(flameJump, 80.0f)
	    , PARAM_INIT(fallSpeed, 2.0f)
	    , PARAM_INIT(marioEstimationFire, 20.0f)
	    , PARAM_INIT(marioEstimationWait, 10.0f)
	{
		TParams::load(mPrmPath);
	}

	/* 0xA8 */ TParamRT<f32> turnSpeed;
	/* 0xBC */ TParamRT<f32> turnAnim;
	/* 0xD0 */ TParamRT<f32> waitStep;
	/* 0xE4 */ TParamRT<f32> downStep;
	/* 0xF8 */ TParamRT<f32> attackRadius;
	/* 0x10C */ TParamRT<f32> attackHeight;
	/* 0x120 */ TParamRT<f32> focusRange;
	/* 0x134 */ TParamRT<f32> waitRange;
	/* 0x148 */ TParamRT<f32> fireSpeed;
	/* 0x15C */ TParamRT<f32> tumbleWeight;
	/* 0x170 */ TParamRT<f32> tumbleSpeed;
	/* 0x184 */ TParamRT<f32> tumbleStartFrame;
	/* 0x198 */ TParamRT<f32> tumbleEndFrame;
	/* 0x1AC */ TParamRT<f32> waitSpeed;
	/* 0x1C0 */ TParamRT<f32> staggerSpeed;
	/* 0x1D4 */ TParamRT<f32> downSpeed;
	/* 0x1E8 */ TParamRT<f32> flameVelocity;
	/* 0x1FC */ TParamRT<f32> flameScale;
	/* 0x210 */ TParamRT<s32> flameCount;
	/* 0x224 */ TParamRT<s32> flameFocusStartStep;
	/* 0x238 */ TParamRT<s32> flameFocusEndStep;
	/* 0x24C */ TParamRT<f32> flameRadius;
	/* 0x260 */ TParamRT<f32> flameHeight;
	/* 0x274 */ TParamRT<f32> headRadius;
	/* 0x288 */ TParamRT<f32> waterhitSpeed;
	/* 0x29C */ TParamRT<f32> flameOverStart;
	/* 0x2B0 */ TParamRT<f32> flameNeckRange;
	/* 0x2C4 */ TParamRT<f32> flameNeckDownRate;
	/* 0x2D8 */ TParamRT<f32> flameJump;
	/* 0x2EC */ TParamRT<f32> fallSpeed;
	/* 0x300 */ TParamRT<f32> marioEstimationFire;
	/* 0x314 */ TParamRT<f32> marioEstimationWait;
};

class TKoopaManager : public TEnemyManager {
public:
	TKoopaManager(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

class TKoopaParts : public THitActor {
public:
	TKoopaParts(const char* name, u32 actorType, TKoopa* owner, f32 radius);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void control() { }
	virtual void attack_(THitActor* sender) { }

public:
	/* 0x68 */ TKoopa* mOwner;
};

class TKoopaBody : public TKoopaParts {
public:
	TKoopaBody(const char* name, u32 actorType, TKoopa* owner, f32 radius)
	    : TKoopaParts(name, actorType, owner, radius)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void attack_(THitActor* sender);
};

class TKoopaHead : public TKoopaParts {
public:
	TKoopaHead(const char* name, u32 actorType, TKoopa* owner, f32 radius)
	    : TKoopaParts(name, actorType, owner, radius)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void attack_(THitActor* sender);
};

class TKoopaHand : public TKoopaParts {
public:
	TKoopaHand(const char* name, u32 actorType, TKoopa* owner, f32 radius)
	    : TKoopaParts(name, actorType, owner, radius)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void attack_(THitActor* sender);
};

class TKoopaFlame : public TKoopaParts {
public:
	TKoopaFlame(const char* name, u32 actorType, TKoopa* owner, f32 radius)
	    : TKoopaParts(name, actorType, owner, radius)
	{
		unk88 = 0.0f;
		unk8C = 1.0f;
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void attack_(THitActor* sender);
	virtual void control();

	// fabricated
	void launch(const JGeometry::TVec3<f32>& pos,
	            const JGeometry::TVec3<f32>& dir, f32 height, f32 radius,
	            f32 speed)
	{
		mPosition.x = pos.x;
		mPosition.y = pos.y;
		mPosition.z = pos.z;
		unk78.x     = dir.x;
		unk78.y     = dir.y;
		unk78.z     = dir.z;
		unk6C.x     = pos.x;
		unk6C.y     = pos.y;
		unk6C.z     = pos.z;
		unk84       = speed;
		unk88       = 4000.0f;
		unk8C       = 0.0f;
		unk90       = radius;
		unk94       = height;
	}

public:
	// TODO: names; offsets from control()
	/* 0x6C */ JGeometry::TVec3<f32> unk6C; // start position
	/* 0x78 */ JGeometry::TVec3<f32> unk78; // velocity per step
	/* 0x84 */ f32 unk84;                   // age increment
	/* 0x88 */ f32 unk88;                   // lifetime
	/* 0x8C */ f32 unk8C;                   // age
	/* 0x90 */ f32 unk90;                   // hit radius
	/* 0x94 */ f32 unk94;                   // hit height
};

class TKoopa : public TSpineEnemy {
public:
	TKoopa(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void updateAnmSound();
	virtual void reset();
	virtual const char** getBasNameTable() const;

	void changeAnm(int, int, f32);
	void fall();
	BOOL allowsLaunch() const;
	f32 getTargetDir(const JGeometry::TVec3<f32>&) const;
	BOOL effectsTumble() const;
	f32 getFlameDirRate() const;
	f32 getFlameDirDegree() const;
	BOOL isFlaming() const;
	bool getShowered();
	void stagger(bool);
	void getDown();
	f32 getNeckFocus() const;
	void setUpHitActors();

	// fabricated
	bool isFlameStart() const
	{
		if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) == 4)
			return true;
		if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) == 5
		    && mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() >= 85.0f)
			return true;
		return false;
	}

	// fabricated
	bool isFlameEnd() const
	{
		if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) == 6) {
			f32 frame = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
			if (68.0f <= frame && frame <= 164.0f)
				return true;
		}
		return false;
	}

	// fabricated
	void changeBck(int bck)
	{
		if (!mMActor->checkCurBckFromIndex(bck)) {
			mMActor->setBckFromIndex(bck);
			setAnmSound(getBas(bck));
		}
	}

	// fabricated
	void changeBtp(int btp)
	{
		if (btp != mMActor->getCurAnmIdx(ANM_TYPE_BTP))
			mMActor->setBtpFromIndex(btp);
	}

	// fabricated
	TKoopaParams* getParams() const
	{
		TEnemyManager* manager = (TEnemyManager*)mManager;
		return (TKoopaParams*)manager->getSaveParam();
	}

public:
	/* 0x150 */ f32 unk150;
	/* 0x154 */ u8 unk154;
	/* 0x155 */ u8 unk155;
	/* 0x158 */ JGeometry::TVec3<f32> unk158;
	/* 0x164 */ TKoopaFlame* unk164[10];
	/* 0x18C */ TKoopaHand* unk18C[2];
	/* 0x194 */ TKoopaHead* unk194;
	/* 0x198 */ TKoopaBody* unk198;
	/* 0x19C */ s32 unk19C;
	/* 0x1A0 */ s32 unk1A0; // head joint index
	/* 0x1A4 */ s32 unk1A4; // neck joint index
	/* 0x1A8 */ s32 unk1A8; // ago (jaw) joint index
	/* 0x1AC */ u8 unk1AC[0x1b8 - 0x1ac];
	/* 0x1B8 */ f32 unk1B8;
};

class TNerveKoopaTurn : public TNerveBase<TLiveActor> {
};

// In the retail binary theNerve() is inlined in every user (weak function-local
// statics), so the accessor is defined in the class.
#define KOOPA_NERVE(Name, Base)                                                	class Name : public Base {                                                 	public:                                                                    		virtual BOOL execute(TSpineBase<TLiveActor>*) const;                   		static const Name& theNerve()                                          		{                                                                      			static Name nerve;                                                 			return nerve;                                                      		}                                                                      	};

KOOPA_NERVE(TNerveKoopaTurnL, TNerveKoopaTurn);
KOOPA_NERVE(TNerveKoopaTurnR, TNerveKoopaTurn);
KOOPA_NERVE(TNerveKoopaTumble, TNerveKoopaTurn);
KOOPA_NERVE(TNerveKoopaWait, TNerveKoopaTurn);
KOOPA_NERVE(TNerveKoopaFlame, TNerveKoopaTurn);
KOOPA_NERVE(TNerveKoopaFall, TNerveBase<TLiveActor>);
KOOPA_NERVE(TNerveKoopaStagger, TNerveBase<TLiveActor>);
KOOPA_NERVE(TNerveKoopaGetShowered, TNerveBase<TLiveActor>);
KOOPA_NERVE(TNerveKoopaGetDown, TNerveBase<TLiveActor>);
KOOPA_NERVE(TNerveKoopaProvoke, TNerveBase<TLiveActor>);

#endif
