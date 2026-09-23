#include <GC2D/Talk2D2.hpp>

#include <Camera/Camera.hpp>
#include <GC2D/GCConsole2.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <System/MarDirector.hpp>

TTalk2D2* gpTalk2D;
void* TTalk2D2::cColorTable;

// NOTE: -inline deferred TU; definitions below are ordered to match the
// emitted .text order recovered from build/GMSP01/asm/GC2D/Talk2D2.s and
// mario.MAP, which is the REVERSE of the declaration order in the header.

// TODO: not yet decompiled.
void TTalk2D2::openWindow(s8, f32) {}

// TODO: not yet decompiled.
void TTalk2D2::setTagParam(JSUMemoryInputStream&, J2DTextBox&, int*, int*) {}

// TODO: not yet decompiled.
void TTalk2D2::setupTextBox(const void*, JMSMesgEntry*) {}

// TODO: not yet decompiled.
void TTalk2D2::setupBoardTextBox(const void*, JMSMesgEntry*) {}

// UNUSED (inlined at all call sites, per mario.MAP: makeLine__8TTalk2D2FPfPffR8JUTPointR8JUTPointR8JUTPoint, size 0xe0).
// TODO: not yet reconstructed; likely inlined into makeBoxLine/openWindow.
void TTalk2D2::makeLine(f32*, f32*, f32, JUTPoint&, JUTPoint&, JUTPoint&) {}

// TODO: not yet decompiled.
void TTalk2D2::perform(u32 cue, JDrama::TGraphics* graphics) {}

// UNUSED (inlined at all call sites, per mario.MAP: appearBoardBoxWindow__8TTalk2D2Fv, size 0x30).
// TODO: not yet reconstructed; likely inlined into openBoardWindow.
void TTalk2D2::appearBoardBoxWindow() {}

// TODO: not yet decompiled.
void TTalk2D2::eraseBoardWindow() {}

// TODO: not yet decompiled.
void TTalk2D2::eraseNormalWindow() {}

// UNUSED (inlined at all call sites, per mario.MAP: closeBoardWindow__8TTalk2D2Fv, size 0x40).
// TODO: not yet reconstructed; body should be recovered from where this was
// inlined (likely eraseBoardWindow/checkBoardControler) and its compiled
// size checked with decomp-diff.py -s extra against 0x40.
void TTalk2D2::closeBoardWindow() {}

// TODO: not yet decompiled.
void TTalk2D2::closeNormalWindow() {}

// TODO: not yet decompiled.
void TTalk2D2::checkControler() {}

// TODO: not yet decompiled.
void TTalk2D2::moveTalkWindow() {}

// TODO: not yet decompiled.
void TTalk2D2::checkBoardControler() {}

// TODO: not yet decompiled.
void TTalk2D2::moveBoardWindow() {}

// TODO: not yet decompiled.
void TTalk2D2::openNormalWindow() {}

// TODO: not yet decompiled.
void TTalk2D2::openBoardWindow() {}

// TODO: not yet decompiled.
void TTalk2D2::makeBoxLine(s8, char*) {}

// TODO: not yet decompiled.
void TTalk2D2::openTalkWindow(TBaseNPC*) {}

// UNUSED (inlined at all call sites, per mario.MAP: closeTalkWindow__8TTalk2D2Fv, size 0xe4).
// TODO: not yet reconstructed; likely inlined into forceCloseTalk.
void TTalk2D2::closeTalkWindow() {}

void TTalk2D2::forceCloseTalk() {
	gpCamera->makeMtxForPrevTalk();
	if (unk28) {
		if (gpMSound->gateCheck(0x4851))
			MSoundSESystem::MSoundSE::startSoundSystemSE(0x4851, 0, 0, 0);
	} else {
		gpMSound->talkModeOut();
	}
	// TODO: stack frame is 0x28 vs target's 0x30 (one 4-byte slot short);
	// instructions and their order already match exactly (160B, 99.9%).
	TGCConsole2* console = SMSGetMarDirector()->getConsole();
	console->startAppearTelop(false);
	if (unk248 == 1)
		unk248 = 0;
	else
		unk248 = 6;
}

// TODO: not yet decompiled.
void TTalk2D2::setMessageID(u32, u32) {}

// TODO: not yet decompiled.
void TTalk2D2::loadAfter() {}

// TODO: not yet decompiled.
void TTalk2D2::load(JSUMemoryInputStream&) {}

// TODO: not yet decompiled; complex constructor with a large field layout
// (see build/GMSP01/asm/GC2D/Talk2D2.s __ct__8TTalk2D2FPCc). Needs the full
// set of member fields recovered from the store offsets before attempting a
// match; left as a minimal fabricated body so the TU compiles/links.
TTalk2D2::TTalk2D2(const char* name)
	: JDrama::TViewObj(name) {
	unk28 = false;
}
