#include <Strategic/spcinterp.hpp>
#include <Strategic/LiveActor.hpp>
#include <Enemy/Enemy.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/Conductor.hpp>
#include <Player/MarioAccess.hpp>

// Only weak symbols of this TU made it into the final binary: every ein*
// builtin and TEinBinary itself are UNUSED in the map. Their bodies below
// are guesses based on names and sizes and still need verification.

class TEinBinary : public TSpcTypedBinary<TLiveActor> {
public:
	TEinBinary(void* data)
	    : TSpcTypedBinary<TLiveActor>(data)
	{
	}

	virtual ~TEinBinary();
	virtual void initUserBuiltin();
};

// TODO: guessed body, UNUSED in the map (0xdc)
void einGetLengthToMario(TSpcTypedInterp<TLiveActor>* interp, u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	TSpineEnemy* enemy = (TSpineEnemy*)interp->getOwner();
	interp->push(enemy->mPosition.distance(SMS_GetMarioPos()));
}

// TODO: guessed body, UNUSED in the map (0xf0)
void einGetLengthToCurPathNode(TSpcTypedInterp<TLiveActor>* interp,
                               u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	TSpineEnemy* enemy = (TSpineEnemy*)interp->getOwner();
	interp->push(enemy->getUnkF4().getPoint().distance(enemy->mPosition));
}

// TODO: guessed body, UNUSED in the map (0xa8)
void einGoToRandomNextGraphNode(TSpcTypedInterp<TLiveActor>* interp,
                                u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	TSpineEnemy* enemy = (TSpineEnemy*)interp->getOwner();
	enemy->goToRandomNextGraphNode();
	interp->push();
}

// TODO: guessed body, UNUSED in the map (0x15c)
void einWalkToCurPathNode(TSpcTypedInterp<TLiveActor>* interp, u32 arg_num)
{
	interp->verifyArgNum(3, &arg_num);
	f32 arg2 = interp->pop();
	f32 arg1 = interp->pop();
	f32 arg0 = interp->pop();
	TSpineEnemy* enemy = (TSpineEnemy*)interp->getOwner();
	enemy->walkToCurPathNode(arg0, arg1, arg2);
	interp->push();
}

// TODO: guessed body, UNUSED in the map (0x198)
void einSetGraph(TSpcTypedInterp<TLiveActor>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	TSpcSlice arg = interp->pop();
	TSpineEnemy* enemy = (TSpineEnemy*)interp->getOwner();
	TGraphWeb* graph = gpConductor->getGraphByName(arg.getDataString());
	enemy->getTracer()->setGraph(graph);
	enemy->getTracer()->setToNearest(enemy->mPosition);
	enemy->setGoalPathFromGraph();
	interp->push();
}

// TODO: guessed body, UNUSED in the map (0xa0)
void TEinBinary::initUserBuiltin()
{
	TSpcTypedBinary<TLiveActor>::initUserBuiltin();
	bindSystemDataToSymbol("setGraph", (u32)&einSetGraph);
	bindSystemDataToSymbol("walkToCurPathNode", (u32)&einWalkToCurPathNode);
	bindSystemDataToSymbol("goToRandomNextGraphNode",
	                       (u32)&einGoToRandomNextGraphNode);
	bindSystemDataToSymbol("getLengthToCurPathNode",
	                       (u32)&einGetLengthToCurPathNode);
	bindSystemDataToSymbol("getLengthToMario", (u32)&einGetLengthToMario);
}

TEinBinary::~TEinBinary() { }
