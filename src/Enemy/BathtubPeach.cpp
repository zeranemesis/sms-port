#include <Enemy/BathtubPeach.hpp>
#include <MoveBG/MapObjCorona.hpp>
#include <Strategic/Spine.hpp>
#include <M3DUtil/MActor.hpp>
#include <Player/MarioAccess.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <System/Application.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* bathtubpeach_bastable[] = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/bathtubpeach/bas/peach_wait.bas",
	nullptr,
};

// fabricated
inline f32 wrapAngle(f32 angle)
{
	return -180.0f + std::fmodf(360.0f + (angle - -180.0f), 360.0f);
}

class TNervePeachStagger : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>* spine) const
	{
		TBathtubPeach* self = (TBathtubPeach*)spine->getBody();
		self->changeAnm(0, 0, 0.5f);
		if (self->getMActor()->curAnmEndsNext(ANM_TYPE_BCK, nullptr))
			return true;
		return false;
	}

	static const TNervePeachStagger& theNerve()
	{
		static TNervePeachStagger nerve;
		return nerve;
	}
};

// TODO: nonmatching. The target calls std::fmodf, TVec2::dot and
// TEnemyManager::getSaveParam out-of-line here (all three are weak inlines),
// and keeps the goTo() delta TVec2 on the stack because of the dot() call.
// The retail std::fmodf (emitted weak in wireTrap.cpp) is an fabs compare plus
// a long long truncation, unlike our MSL header's ::fmod wrapper, so the MSL
// header likely needs a (human-reviewed) fix before this can match.
class TNervePeachEscape : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>* spine) const
	{
		TBathtubPeach* self = (TBathtubPeach*)spine->getBody();
		TBathtub* bathtub
		    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");

		if (bathtub->unk29A)
			return false;

		if (!(spine->getTime() & 4)) {
			if (bathtub->getBathtubData().unk64)
				spine->pushNerve(&TNervePeachStagger::theNerve());
			return false;
		}

		self->changeAnm(1, 1, 0.5f);

		MtxPtr mtx = *bathtub->getRootJointMtx();
		f32 tubX   = mtx[0][3];
		f32 tubZ   = mtx[2][3];

		JGeometry::TVec3<f32> marioPos = *gpMarioPos;

		f32 marioAngle = 0.005493164f
		                 * matan(marioPos.z - tubZ, marioPos.x - tubX);
		f32 selfAngle = 0.005493164f
		                * matan(self->mPosition.z - tubZ,
		                        self->mPosition.x - tubX);

		f32 target;
		if (wrapAngle(selfAngle - marioAngle) < 0.0f)
			target = wrapAngle(marioAngle - self->getParam()->angle.get());
		else
			target = wrapAngle(marioAngle + self->getParam()->angle.get());

		f32 radius = self->getParam()->radius.get();
		s16 a      = 182.04445f * target;
		JGeometry::TVec3<f32> goal;
		goal.x = radius * JMASSin(a) + tubX;
		goal.z = radius * JMASCos(a) + tubZ;
		self->goTo(goal);
		self->faceTo(*gpMarioPos, self->getParam()->turnSpeed2.get());
		return false;
	}

	static const TNervePeachEscape& theNerve()
	{
		static TNervePeachEscape nerve;
		return nerve;
	}
};

TBathtubPeach::TBathtubPeach(const char* name)
    : TSpineEnemy(name)
{
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	offLiveFlag(LIVE_FLAG_UNK100);
	offLiveFlag(LIVE_FLAG_UNK10);
}

void TBathtubPeach::goTo(const JGeometry::TVec3<f32>& target)
{
	JGeometry::TVec2<f32> d(target.x - mPosition.x, target.z - mPosition.z);
	if (d.x * d.x + d.y * d.y
	    >= getParam()->speed.get() * getParam()->speed.get())
		d.setLength(((TBathtubPeachParams*)((TEnemyManager*)mManager)
		                 ->getSaveParam())
		                ->speed.get());
	mPosition.x += d.x;
	mPosition.z += d.y;
}

void TBathtubPeach::faceTo(const JGeometry::TVec3<f32>& target, f32 speed)
{
	f32 dz = target.z - mPosition.z;
	f32 dx = target.x - mPosition.x;
	if (dx * dx + dz * dz <= 0.0000038146973f)
		return;

	f32 angle = 0.005493164f * matan(dz, dx) - 90.0f;
	f32 diff  = wrapAngle(angle - mRotation.y);
	if (diff < -speed)
		mRotation.y = wrapAngle(mRotation.y - speed);
	else if (diff > speed)
		mRotation.y = wrapAngle(mRotation.y + speed);
	else
		mRotation.y = angle;
}

void TBathtubPeach::changeAnm(int bck, int btp, f32 rate)
{
	if (!getMActor()->checkCurBckFromIndex(bck)) {
		getMActor()->setBckFromIndex(bck);
		setAnmSound(getBas(bck));
	}
	if (getMActor()->getCurAnmIdx(ANM_TYPE_BTP) != btp)
		getMActor()->setBtpFromIndex(btp);
	J3DFrameCtrl* ctrl = getMActor()->getFrameCtrl(ANM_TYPE_BCK);
	ctrl->setRate(rate * (2.0f * SMSGetAnmFrameRate()));
}

const char** TBathtubPeach::getBasNameTable() const
{
	return bathtubpeach_bastable;
}

void TBathtubPeach::init(TLiveManager* manager)
{
	TSpineEnemy::init(manager);
	mSpine->initWith(&TNervePeachEscape::theNerve());
	initAnmSound();
	reset();
	mScaling.set(2.0f, 2.0f, 2.0f);
}

void TBathtubPeach::reset()
{
	mPosition.x *= 0.21f;
	mPosition.y *= 0.21f;
	mPosition.z *= 0.21f;
	mBathtubBinder.init(50.0f, 50.0f, 50.0f, 50.0f, 0.0f);
	unk130 = 0;
	mScaling.set(1.5f, 1.5f, 1.5f);
	mBinder = &mBathtubBinder;
	TSpineEnemy::reset();
	changeAnm(1, 1, 0.5f);
}

void TBathtubPeach::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSpineEnemy::perform(cue, graphics);
}

MtxPtr TBathtubPeach::getRootJointMtx() const
{
	return getModel()->getBaseTRMtx();
}

BOOL TBathtubPeach::receiveMessage(THitActor* sender, u32 message)
{
	return TSpineEnemy::receiveMessage(sender, message);
}

void TBathtubPeach::calcRootMatrix()
{
	TBathtub* bathtub = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
	if (bathtub && bathtub->unk29A) {
		MTXCopy(bathtub->getPeachMtxInDemo(), getModel()->getBaseTRMtx());
	} else {
		TLiveActor::calcRootMatrix();
	}
}

const TBathtubPeachParams* TBathtubPeach::getParam() const
{
	return (const TBathtubPeachParams*)((TEnemyManager*)mManager)->unk38;
}

TBathtubPeachManager::TBathtubPeachManager(const char* name)
    : TEnemyManager(name)
{
}

TSpineEnemy* TBathtubPeachManager::createEnemyInstance() { return nullptr; }

void TBathtubPeachManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "ahiru_peach.bmd", 0x14240000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TBathtubPeachManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
	unk38 = new TBathtubPeachParams("/enemy/bathtubpeach.prm");
}
