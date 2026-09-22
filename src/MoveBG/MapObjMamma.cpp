#include <MoveBG/MapObjMamma.hpp>

// NOTE: this TU uses -inline deferred, so definitions are emitted in
// reverse source order; keep them in reverse of marioEU.MAP address order
// (TSandEgg < TLeanMirror < TSandBomb in the map).

u32 TSandBomb::getSDLModelFlag() const
{
	return 0;
}

u32 TLeanMirror::getSDLModelFlag() const
{
	return 0;
}

u32 TSandEgg::getSDLModelFlag() const
{
	return 0;
}
