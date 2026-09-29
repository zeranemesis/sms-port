#include <GC2D/Talk2D2.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <Camera/Camera.hpp>
#include <GC2D/GCConsole2.hpp>
#include <GC2D/MessageLoader.hpp>
#include <GC2D/MessageUtil.hpp>
#include <GC2D/BoundPane.hpp>
#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/J2D/J2DPane.hpp>
#include <JSystem/J2D/J2DScreen.hpp>
#include <JSystem/J2D/J2DOrthoGraph.hpp>
#include <JSystem/J2D/J2DPicture.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <JSystem/JUtility/JUTRect.hpp>
#include <JSystem/JUtility/JUTPoint.hpp>
#include <JSystem/JUtility/JUTResFont.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <JSystem/JKernel/JKRArchive.hpp>
#include <JSystem/JGeometry/JGUtil.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <JSystem/JSupport/JSUMemoryOutputStream.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <System/MarDirector.hpp>
#include <System/Application.hpp>
#include <System/MarioGamePad.hpp>
#include <System/FlagManager.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <MoveBG/MapObjHide.hpp>
#include <NPC/NpcBase.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MarioUtil/ReinitGX.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <stdio.h>
#include <dolphin/gx.h>
#include <dolphin/mtx.h>

// rogue include: pulls in JALList.hpp's JSUList<T>::smList template
// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp
// (see the same block in src/Enemy/effectObj.cpp).
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

TTalk2D2* gpTalk2D;
u32 TTalk2D2::cColorTable[6] = {
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFB48CFF, 0x6EE6FFFF, 0xFFFF00FF, 0xAAFF50FF,
};

// Exact 0x21C-byte sound lookup recovered from the DOL's scTalkSoundList.
static const u32 scTalkSoundList[0x87] = {
	0x8850, 0x8851, 0x8852, 0x8853, 0x8854, 0x8855, 0x8856, 0x8857, 0x8858,
	0x8859, 0x885A, 0x885B, 0x885C, 0x885D, 0x885E, 0x885F, 0x8860, 0x8861,
	0x8862, 0x8863, 0x8866, 0x8867, 0x8868, 0x8869, 0x886A, 0x886B, 0x886C,
	0x886D, 0x886E, 0x886F, 0x8870, 0x8871, 0x8872, 0x8873, 0x8874, 0x8875,
	0x8876, 0x8877, 0x8878, 0x8879, 0x887A, 0x887B, 0x887C, 0x887D, 0x887E,
	0x887F, 0x8880, 0x8881, 0x8882, 0x8883, 0x8884, 0x8885, 0x8886, 0x8887,
	0x8888, 0x8889, 0x888A, 0x888B, 0x888C, 0x888D, 0x888E, 0x888F, 0x8890,
	0x8891, 0x8892, 0x8893, 0x8894, 0x8895, 0x8896, 0xFFFFFFFF, 0x8899,
	0x889A, 0x889B, 0x889C, 0x889D, 0x889E, 0x889F, 0x88A0, 0x88A1, 0x88A2,
	0x88A3, 0x88C0, 0x88C1, 0x88C2, 0x88C3, 0x88C4, 0x88C5, 0x88C6, 0x88C7,
	0x88C8, 0x88C9, 0x88CA, 0x88CB, 0x88CC, 0x88CD, 0x88CE, 0x88CF, 0x88E5,
	0x88E6, 0x88E7, 0x88E8, 0x88E9, 0x88EA, 0x88EB, 0x88D0, 0x88EC, 0x88ED,
	0x88D1, 0x88EE, 0x88D2, 0x88EF, 0x88D3, 0x88D4, 0x88D5, 0x88D6, 0x88D7,
	0x88D8, 0x88D9, 0x88DA, 0x88DB, 0x88DC, 0x88DD, 0x88DE, 0x88DF, 0x88E0,
	0x88E1, 0x88E2, 0x88E3, 0x88E4, 0x4849, 0x80010025, 0x483D, 0x88A6, 0x8864,
	0x8865,
};

// NOTE: -inline deferred TU; definitions below are ordered to match the
// emitted .text order recovered from build/GMSP01/asm/GC2D/Talk2D2.s and
// mario.MAP, which is the REVERSE of the declaration order in the header.

TTalk2D2::~TTalk2D2() { }

void TTalk2D2::openWindow(s8 line, f32 progress)
{
	J2DPane* pane = unk90;
	const f32 centerX = pane->mGlobalBounds.x1 + 5.0f;
	const f32 centerY = pane->mGlobalBounds.y1 + 5.0f;
	Mtx rotation;
	Mtx transform;
	Mtx translation;

	PSMTXTrans(translation, -centerX, -centerY, 0.0f);
	PSMTXRotRad(rotation, 'z', (-pane->mRotation - 1.0f) * 0.017453292f);
	PSMTXConcat(rotation, translation, transform);
	PSMTXTrans(rotation, centerX, centerY, 0.0f);
	PSMTXConcat(rotation, transform, transform);
	GXLoadPosMtxImm(transform, GX_PNMTX0);

	GXSetCullMode(GX_CULL_BACK);
	GXSetNumTexGens(2);
	GXSetNumTevStages(2);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S8, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_TRUE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL,
	              GX_DF_NONE, GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL,
	              GX_DF_NONE, GX_AF_NONE);
	GXColor color;
	color.r = 0xFF;
	color.g = 0xFF;
	color.b = 0xFF;
	color.a = 0xFF;
	GXSetChanAmbColor(GX_COLOR0A0, color);

	J2DPicture* picture = static_cast<J2DPicture*>(unk3C[line]);
	JUTTexture* pictureTexture = picture->getTexture(0);
	pictureTexture->load(GX_TEXMAP1);
	unk244->load(GX_TEXMAP0);

	color.r = 0;
	color.g = 0;
	color.b = 0;
	color.a = 0xFF;
	GXSetTevColor(GX_TEVREG0, color);
	color.a = 0xA0;
	GXSetTevColor(GX_TEVREG1, color);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C0, GX_CC_C1, GX_CC_TEXC,
	                GX_CC_ZERO);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_A0, GX_CA_A1, GX_CA_TEXA,
	                GX_CA_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);

	PSMTXTrans(transform, progress, 0.0f, 0.0f);
	GXLoadTexMtxImm(transform, GX_TEXMTX0, GX_MTX2x4);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0,
	                  GX_FALSE, GX_PTIDENTITY);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0,
	              GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_CPREV, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_APREV, GX_CA_TEXA,
	                GX_CA_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);

	PSMTXIdentity(transform);
	GXLoadTexMtxImm(transform, GX_TEXMTX1, GX_MTX2x4);
	GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX1,
	                  GX_FALSE, GX_PTIDENTITY);
	GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1,
	              GX_COLOR_NULL);

	const JUTRect& bounds = picture->mGlobalBounds;
	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition2f32(bounds.x1, bounds.y1);
	GXTexCoord2u8(0, 0);
	GXPosition2f32(bounds.x2, bounds.y1);
	GXTexCoord2u8(1, 0);
	GXPosition2f32(bounds.x2, bounds.y2);
	GXTexCoord2u8(1, 1);
	GXPosition2f32(bounds.x1, bounds.y2);
	GXTexCoord2u8(0, 1);
}

// Decode the message's reveal, choice, counter and color controls.
void TTalk2D2::setTagParam(JSUMemoryInputStream& stream, J2DTextBox&,
                         int* glyph, int* line)
{
	u8 tagLength = stream.readU8();
	u8 tagType = stream.readU8();
	u16 tagCode = stream.readU16();

	// The two scalar type-0 tags are fully established by the DOL: code 0
	// consumes one byte into +0x280, while code 1 sets the +0x26D flag.
	switch (tagType) {
	case 0:
		switch (tagCode) {
		case 0:
			unk280 = stream.readU8();
			return;
		case 1:
			unk26D = 1;
			return;
		default:
			stream.skip(tagLength - 5);
			return;
		}
	case 1:
		switch (tagCode) {
		case 0: {
			if (unk214 == -1) {
				unk214 = 0;
				unk20C[1]->setAlpha(0xFE);
				unk20C[1]->hide();
				unk20C[0]->show();
			}

			const int bufferSize = tagLength - 4 < 0x11 ? tagLength - 4 : 0x11;
			snprintf(mChoiceText[0], bufferSize, "%s",
			         static_cast<const char*>(stream.getCurrent()));
			stream.skip(tagLength - 5);
			return;
		}
		case 1: {
			if (unk214 == -1) {
				unk214 = 1;
				unk20C[0]->setAlpha(0xFE);
				unk20C[0]->hide();
				unk20C[1]->show();
			}

			const int bufferSize = tagLength - 4 < 0x11 ? tagLength - 4 : 0x11;
			snprintf(mChoiceText[1], bufferSize, "%s",
			         static_cast<const char*>(stream.getCurrent()));
			stream.skip(tagLength - 5);
			return;
		}
		default:
			stream.skip(tagLength - 5);
			return;
		}
	case 2: {
		switch (tagCode) {
		case 0:
		case 1:
		case 6: {
			int time;
			if (tagCode == 0)
				time = TFlagManager::getInstance()->getFlag(0x20003);
			else if (tagCode == 1)
				time = TFlagManager::getInstance()->getFlag(0x20002);
			else
				time = TFlagManager::getInstance()->getFlag(0x20014);
			if (time > 599999)
				time = 599999;
			if (time < 0)
				time = 0;
			const u16 minutes = (time - time % 100) / 6000;
			const int remainder = time - minutes * 6000;
			const u16 seconds = remainder * 0.01;
			const u16 hundredths = remainder - seconds * 100;
			snprintf(unk9C[*line * 30 + *glyph]->getStringPtr(), 2, "%d",
			         minutes / 10);
			snprintf(unk9C[*line * 30 + *glyph + 1]->getStringPtr(), 2, "%d",
			         minutes % 10);
			snprintf(unk9C[*line * 30 + *glyph + 2]->getStringPtr(), 2, ":");
			snprintf(unk9C[*line * 30 + *glyph + 3]->getStringPtr(), 2, "%d",
			         seconds / 10);
			snprintf(unk9C[*line * 30 + *glyph + 4]->getStringPtr(), 2, "%d",
			         seconds % 10);
			snprintf(unk9C[*line * 30 + *glyph + 5]->getStringPtr(), 2, ":");
			snprintf(unk9C[*line * 30 + *glyph + 6]->getStringPtr(), 2, "%d",
			         hundredths / 10);
			snprintf(unk9C[*line * 30 + *glyph + 7]->getStringPtr(), 2, "%d",
			         hundredths % 10);
			for (int i = 0; i < 8; ++i) {
				unk9C[*line * 30 + *glyph + i]->mCharColor
				    = JUtility::TColor(unk27C);
				unk9C[*line * 30 + *glyph + i]->mGradColor
				    = JUtility::TColor(unk27C);
				unk9C[*line * 30 + *glyph + i]->setBlackWhite(
				    JUtility::TColor(unk27C & 0xFFFFFF00), JUtility::TColor(unk27C));
				unk281[*line * 30 + *glyph + i] = unk280;
			}
			*glyph += 8;
			break;
		}
		case 2: {
			const int number = static_cast<int>(
			    (TFlagManager::getInstance()->getFlag(0x20004) + 99) * 0.01f);
			if (number < 10) {
				snprintf(unk9C[*line * 30 + *glyph]->getStringPtr(), 2,
				         "%d", number);
				++*glyph;
			} else {
				snprintf(unk9C[*line * 30 + *glyph]->getStringPtr(), 2,
				         "%d", number / 10);
				snprintf(unk9C[*line * 30 + *glyph + 1]->getStringPtr(), 2,
				         "%d", number % 10);
				*glyph += 2;
			}
			break;
		}
		case 3: {
			int number = TFlagManager::getInstance()->getFlag(0x40001);
			int purchases = 0;
			for (int i = 0x46; i < 0x56; ++i)
				if (TFlagManager::getInstance()->getFlag(0x10000 + i) != 0)
					++purchases;
			for (int i = 0x6C; i <= 0x73; ++i)
				if (TFlagManager::getInstance()->getFlag(0x10000 + i) != 0)
					++purchases;
			number -= purchases * 10;
			if (number < 100) {
				snprintf(unk9C[*line * 30 + *glyph]->getStringPtr(), 2,
				         "%d", number / 10);
				++*glyph;
			} else {
				const int hundreds = number / 100;
				snprintf(unk9C[*line * 30 + *glyph]->getStringPtr(), 2,
				         "%d", hundreds);
				number -= hundreds * 100;
				snprintf(unk9C[*line * 30 + *glyph + 1]->getStringPtr(), 2,
				         "%d", number / 10);
				*glyph += 2;
			}
			break;
		}
		case 4: {
			u8 basketIndex;
			stream.read(&basketIndex, 1);
			TFruitBasketEvent* basket = nullptr;
			int fruitType = 0;
			switch (basketIndex) {
			case 0:
				basket = static_cast<TFruitBasketEvent*>(JDrama::TNameRefGen::search(
				    "\x83\x74\x83\x8B\x81\x5B\x83\x63\x82\xA9\x82\xB2\x82\x60"));
				fruitType = 0;
				break;
			case 1:
				basket = static_cast<TFruitBasketEvent*>(JDrama::TNameRefGen::search(
				    "\x83\x74\x83\x8B\x81\x5B\x83\x63\x82\xA9\x82\xB2\x82\x61"));
				fruitType = 4;
				break;
			case 2:
				basket = static_cast<TFruitBasketEvent*>(JDrama::TNameRefGen::search(
				    "\x83\x74\x83\x8B\x81\x5B\x83\x63\x82\xA9\x82\xB2\x82\x62"));
				fruitType = 3;
				break;
			case 3:
				basket = static_cast<TFruitBasketEvent*>(JDrama::TNameRefGen::search(
				    "\x83\x74\x83\x8B\x81\x5B\x83\x63\x82\xA9\x82\xB2\x82\x63"));
				fruitType = 1;
				break;
			}
			if (basket != nullptr) {
				int remaining = 3 - basket->getFruitNum(fruitType);
				if (remaining < 0 || remaining > 9)
					remaining = 0;
				snprintf(unk9C[*line * 30 + *glyph]->getStringPtr(), 2,
				         "%d", remaining);
				snprintf(unk9C[*line * 30 + *glyph + 1]->getStringPtr(), 2, " ");
				*glyph += 2;
			}
			break;
		}
		}
		return;
	}
	case 0xFF:
		if (tagCode == 0) {
			u8 colorIndex;
			stream.read(&colorIndex, 1);
			unk27C = cColorTable[colorIndex];
			return;
		}
		return;
	default:

		// The DOL's default case always calls skip(length - 5), including a
		// zero or negative displacement for malformed/short headers.
		stream.skip(tagLength - 5);
	}
}

#pragma dont_inline on
void TTalk2D2::setupTextBox(const void* buffer, JMSMesgEntry* entry)
{
	if (unk28) {
		setupBoardTextBox(buffer, entry);
		return;
	}

	// The DOL's normal path starts a 0x400-byte stream at the message's
	// relative offset plus the loader's current position, then resets its
	// parser counters before reading the three text lines.
	const u32 entryOffset = *reinterpret_cast<const u32*>(entry);
	JSUMemoryInputStream stream(
	    static_cast<const u8*>(buffer) + (entryOffset + unk278), 0x400);
	unk254 = entry;
	int glyph = 0;
	int line = 0;
	unk274 = 0;
	unk2DE = 0;

	while (line < 3) {
		char* text = unk9C[line * 30 + glyph]->getStringPtr();
		text[0] = '\0';
		s8 code;
		stream.read(&code, 1);
		switch (code) {
		case '\n':
			unk228[line] = glyph == 0 ? 0 : glyph - 1;
			glyph = 0;
			makeBoxLine(line, nullptr);
			++line;
			break;
		case '\0':
			if (glyph != 0) {
				unk228[line] = glyph - 1;
				glyph = 0;
				makeBoxLine(line, nullptr);
			}
			line = 3;
			unk26A = 1;
			break;
		case 0x1A:
			setTagParam(stream, *unk9C[line * 30 + glyph], &glyph, &line);
			break;
		default: {
			if (unk274 != line)
				unk274 = line;
			stream.skip(-1);
			u8 byte;
			stream.read(&byte, 1);
			text[0] = byte;
			JUtility::TColor color(0xFFFFFFFF);
			bool special = true;
			const int index = line * 30 + glyph;
			switch (static_cast<u8>(text[0])) {
			case '@':
				color.set(100, 255, 100, 255);
				break;
			case '#':
				color.set(255, 160, 100, 255);
				break;
			case '%':
				color.set(255, 255, 0, 255);
				break;
			case '<':
			case '>':
			case '+':
			case 0xA5:
				color.set(220, 220, 220, 255);
				break;
			case '$':
				unk9C[index]->setFontSize(30, 30);
				color.set(200, 180, 255, 255);
				break;
			case '*':
				color.set(220, 220, 220, 255);
				break;
			default:
				special = false;
				break;
			}
			text[1] = '\0';
			if (special) {
				unk9C[index]->mCharColor = color;
				unk9C[index]->mGradColor = color;
				unk9C[index]->setBlackWhite(
				    JUtility::TColor(static_cast<u32>(color) & 0xFFFFFF00), color);
			} else {
				unk9C[index]->mCharColor = JUtility::TColor(unk27C);
				unk9C[index]->mGradColor = JUtility::TColor(unk27C);
				unk9C[index]->setBlackWhite(
				    JUtility::TColor(unk27C & 0xFFFFFF00), JUtility::TColor(unk27C));
				unk9C[index]->setFontSize(20, 24);
			}
			unk281[static_cast<u16>(unk2DE)] = unk280;
			unk2DE = index;
			++glyph;
			break;
		}
		}
	}
	if (!unk26A) {
		s8 next;
		stream.peek(&next, 1);
		if (next == 0)
			unk26A = 1;
	}
	if (unk214 != -1) {
		if (unk214 == 0) {
			snprintf(unk208->getStringPtr(), 0x5E,
			         "%s\n\033CC[7f7f7f]\033GC[7f7f7f]%s", mChoiceText[0], mChoiceText[1]);
		} else {
			snprintf(unk208->getStringPtr(), 0x5E,
			         "\033CC[7f7f7f]\033GC[7f7f7f]%s"
			         "\033CC[ffffff]\033GC[ffffff]\n%s", mChoiceText[0], mChoiceText[1]);
		}
	}
	unk278 += stream.getPosition();
}
#pragma dont_inline off

void TTalk2D2::setupBoardTextBox(const void* buffer, JMSMesgEntry* messageEntry)
{
	const TMessageLoader::EntryInfo* entry
	    = reinterpret_cast<const TMessageLoader::EntryInfo*>(messageEntry);
	const u8* messageData = static_cast<const u8*>(buffer) + entry->unk0 + unk278;
	JSUMemoryInputStream input(messageData, 0x400);
	JSUMemoryOutputStream output(unk18->getStringPtr(), 0x200);
	unk254 = messageEntry;

	for (s32 lineBreaks = 0; lineBreaks < 6;) {
		u8 character;
		input.read(&character, 1);
		if (character == '\n') {
			output.write(&character, 1);
			++lineBreaks;
		} else if (character == '\0') {
			unk26A = 1;
			lineBreaks = 6;
		} else if (character == 0x1A) {
			lineBreaks = 6;
		} else {
			// Rewind the probe byte, then copy one Shift-JIS character.
			input.skip(-1);
			input.read(&character, 1);
			output.write(&character, 1);
			if (character >= 0x80) {
				input.read(&character, 1);
				output.write(&character, 1);
			}
		}
	}

	if (!unk26A) {
		u8 nextCharacter;
		input.peek(&nextCharacter, 1);
		if (static_cast<s8>(nextCharacter) == 0) {
			unk26A = 1;
			input.skip(1);
			const u8 terminator = 0;
			output.write(&terminator, 1);
		}
	} else {
		const u8 terminator = 0;
		output.write(&terminator, 1);
	}

	unk278 += input.getPosition();
}

// Inlined at its call sites in the original game (mario.MAP records 0xe0 bytes).
inline void TTalk2D2::makeLine(f32* x, f32* y, f32 t, JUTPoint& start,
                       JUTPoint& control, JUTPoint& end)
{
	const f32 inverse = 1.0f - t;
	const f32 weight0 = inverse * inverse;
	const f32 weight1 = (2.0f * t) * inverse;
	const f32 weight2 = t * t;
	*x = start.x * weight0 + control.x * weight1 + end.x * weight2;
	*y = start.y * weight0 + control.y * weight1 + end.y * weight2;
}

void TTalk2D2::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if ((cue & 1) && gpMarDirector->unk124 == 2 && unk248 <= 8) {
		switch (unk248) {
		case 2:
			if (gpCamera->isTalkCameraSpecifyMode(gpCamera->mMode)
			    && !gpCamera->isNowInbetween()) {
				unk251 = 0x14;
				unk248 = 3;
			}
			break;
		case 4: {
			const bool opened = unk28 ? openBoardWindow() : openNormalWindow();
			if (opened)
				unk248 = 5;
			break;
		}
		case 5: {
			bool finished;
			if (unk28) {
				finished = unk14->update();
			} else {
				finished = closeNormalWindow();
			}
			if (finished)
				unk248 = (unk270 & 1) ? 0 : 1;
			break;
		}
		case 6: {
			const bool erased = unk28 ? eraseBoardWindow() : eraseNormalWindow();
			if (erased)
				unk248 = unk28 ? 8 : 4;
			break;
		}
		case 8: {
			s32 alpha = unk18->getAlpha() + 4;
			if (alpha > 0xFF) {
				alpha = 0xFF;
				unk248 = 5;
			}
			unk18->setAlpha(alpha);
			break;
		}
		default:
			break;
		}
	}

	if ((cue & 2) && gpMarDirector->unk124 == 2) {
		if (unk248 == 3) {
			--unk251;
			if (unk251 < 0) {
				unk2DE = 0;
				unk248 = 4;
			}
		} else if (unk248 == 7) {
			s16 alpha = unk90->getAlpha() - 0x10;
			if (alpha < 0) {
				alpha = 0;
				unk234[0] = 1.0f;
				unk3C[0]->mVisible = false;
				unk224[0] = 0;
				unk6C[0]->mVisible = false;
				unk234[1] = 2.0f;
				unk3C[1]->mVisible = false;
				unk224[1] = 0;
				unk6C[1]->mVisible = false;
				unk234[2] = 3.0f;
				unk3C[2]->mVisible = false;
				unk224[2] = 0;
				unk6C[2]->mVisible = false;
				for (s32 i = 0; i < 90; ++i) {
					if (unk9C[i] != nullptr)
						unk9C[i]->mVisible = false;
				}
				TMessageLoader* loader = unk260;
				TMessageLoader::EntryInfo* entry
				    = loader->getMessageEntry(unk264 & 0xFFFF);
				setupTextBox(loader->unk4,
				             reinterpret_cast<JMSMesgEntry*>(entry));
				unk26C = 0;
				unk340 = 0x40;
				unk27C = -1;
				unk2DE = 0;
				unk2DC = 0;
				unk248 = 4;
			}
			unk90->setAlpha(alpha);
		}
	}

	if (!(cue & 8) || gpMarDirector->unk124 != 2)
		return;

	ReInitializeGX();
	SMS_DrawInit();
	J2DOrthoGraph graph(graphics->getViewport());
	graph.setup2D();

	if (unk250) {
		for (s32 i = 0; i < 3; ++i)
			unk3C[i]->mVisible = true;
		J2DPane* root = unk2C->search(0x524F4F54);
		root->setAlpha(0);
		unk2C->draw(0, 0, &graph);
		root->setAlpha(0xFF);
		graph.setup2D();
		unk250 = 0;
		for (s32 i = 0; i < 3; ++i)
			unk3C[i]->mVisible = false;
	}

	if (unk248 == 4) {
		for (s8 line = 0; line <= unk274; ++line)
			openWindow(line, unk234[line]);
	}

	graph.setup2D();
	if (unk28) {
		unk10->draw(0, 0, &graph);
	} else {
		unk90->move(unk330, unk332);
		unk90->mRotation = unk334;
		unk2C->draw(0, 0, &graph);
	}
}

// UNUSED (inlined at all call sites, per mario.MAP: appearBoardBoxWindow__8TTalk2D2Fv, size 0x30).
// TODO: not yet reconstructed; likely inlined into openBoardWindow.
void TTalk2D2::appearBoardBoxWindow() {}

bool TTalk2D2::eraseBoardWindow()
{
	s16 alpha = unk18->getAlpha();
	alpha -= 4;
	bool finished = false;
	if (alpha < 0) {
		TMessageLoader* loader = unk260;
		alpha = 0;
		const void* messageData = loader->unk4;
		TMessageLoader::EntryInfo* entry
		    = loader->getMessageEntry(unk264 & 0xFFFF);
		setupTextBox(messageData, reinterpret_cast<JMSMesgEntry*>(entry));
		unk27C = -1;
		unk2DE = 0;
		unk2DC = 0;
		finished = true;
	}
	unk18->setAlpha(alpha);
	return finished;
}

bool TTalk2D2::eraseNormalWindow()
{
	s16 alpha = unk90->getAlpha() - 0x10;
	bool finished = false;
	if (alpha < 0) {
		alpha = 0;
		finished = true;
		unk234[0] = 1.0f;
		unk234[1] = 2.0f;
		unk234[2] = 3.0f;

		for (s32 i = 0; i < 3; ++i) {
			unk3C[i]->mVisible = false;
			unk6C[i]->mVisible = false;
		}
		unk224[0] = 0;
		unk224[1] = 0;
		unk224[2] = 0;

		for (s32 i = 0; i < 90; ++i) {
			if (unk9C[i] != nullptr)
				unk9C[i]->mVisible = false;
		}

		TMessageLoader* loader = unk260;
		TMessageLoader::EntryInfo* entry
		    = loader->getMessageEntry(unk264 & 0xFFFF);
		setupTextBox(loader->unk4, reinterpret_cast<JMSMesgEntry*>(entry));
		unk26C = 0;
		unk340 = 0x40;
		unk27C = -1;
		unk2DE = 0;
		unk2DC = 0;
	}

	unk90->setAlpha(alpha);
	return finished;
}

// UNUSED (inlined at all call sites, per mario.MAP: closeBoardWindow__8TTalk2D2Fv, size 0x40).
// TODO: not yet reconstructed; body should be recovered from where this was
// inlined (likely eraseBoardWindow/checkBoardControler) and its compiled
// size checked with decomp-diff.py -s extra against 0x40.
void TTalk2D2::closeBoardWindow() {}

bool TTalk2D2::closeNormalWindow()
{
	s16 alpha = unk90->getAlpha() - 0x10;
	bool finished = false;
	if (alpha < 0) {
		alpha = 0;
		finished = true;
		unk234[0] = 1.0f;
		unk234[1] = 2.0f;
		unk234[2] = 3.0f;

		for (s32 i = 0; i < 3; ++i) {
			unk3C[i]->mVisible = false;
			unk6C[i]->mVisible = false;
		}
		unk224[0] = 0;
		unk224[1] = 0;
		unk224[2] = 0;

		for (s32 i = 0; i < 90; ++i) {
			if (unk9C[i] != nullptr)
				unk9C[i]->mVisible = false;
		}

		if (unk204->mVisible)
			unk204->mVisible = false;
	}

	unk90->setAlpha(alpha);
	if (unk204->mVisible)
		unk204->setAlpha(alpha);
	return finished;
}

void TTalk2D2::checkControler()
{
	const u32 meaning = unk24C->mEnabledFrameMeaning;
	const s32 line = unk274;
	if (unk6C[line]->mVisible) {
		if (unk26A) {
			if (!unk26D
			    && !(meaning & (TMarioGamePad::MEANING_SELECT_A
			                    | TMarioGamePad::MEANING_SELECT_B)))
				return;
		} else if (!(meaning & (TMarioGamePad::MEANING_SELECT_A
		                       | TMarioGamePad::MEANING_SELECT_B))) {
			return;
		}

		if (gpMSound->gateCheck(0x481C))
			MSoundSESystem::MSoundSE::startSoundSystemSE(0x481C, 0, 0, 0);

		if (unk26A && (unk270 & 1)) {
			if (unk264 == 0x1E) {
				if (gpMSound->gateCheck(0x4851))
					MSoundSESystem::MSoundSE::startSoundSystemSE(0x4851, 0, 0,
					                                             0);
			} else {
				if (unk28 && gpMSound->gateCheck(0x481A))
					MSoundSESystem::MSoundSE::startSoundSystemSE(0x481A, 0, 0,
					                                             0);
				gpMSound->talkModeOut();
			}

			gpCamera->makeMtxForPrevTalk();
			SMSGetMarDirector()->getConsole()->startAppearTelop(false);
			SMSRumbleMgr->finishPause();
			unk252 = 0;
		}
		unk248 = unk26A ? 6 : 7;
		return;
	}

	if (!unk204->mVisible)
		return;

	if ((meaning & TMarioGamePad::MEANING_SELECT_UP) && unk214 == 1) {
		if (gpMSound->gateCheck(0x481E))
			MSoundSESystem::MSoundSE::startSoundSystemSE(0x481E, 0, 0, 0);
		unk214 = 0;
		unk20C[1]->setAlpha(0xFE);
		unk20C[1]->mVisible = false;
		unk20C[0]->mVisible = true;
		return;
	}

	if ((meaning & TMarioGamePad::MEANING_SELECT_DOWN) && unk214 == 0) {
		if (gpMSound->gateCheck(0x481E))
			MSoundSESystem::MSoundSE::startSoundSystemSE(0x481E, 0, 0, 0);
		unk214 = 1;
		unk20C[0]->setAlpha(0xFE);
		unk20C[0]->mVisible = false;
		unk20C[1]->mVisible = true;
		return;
	}

	if (unk26A) {
		if (!(meaning & (TMarioGamePad::MEANING_SELECT_A
		                 | TMarioGamePad::MEANING_SELECT_B)))
			return;
		if (unk270 & 1)
			gpCamera->makeMtxForPrevTalk();
		if (gpMSound->gateCheck(0x481C))
			MSoundSESystem::MSoundSE::startSoundSystemSE(0x481C, 0, 0, 0);
		unk248 = 6;
	} else if (meaning & (TMarioGamePad::MEANING_SELECT_A
	                     | TMarioGamePad::MEANING_SELECT_B)) {
		if (gpMSound->gateCheck(0x481C))
			MSoundSESystem::MSoundSE::startSoundSystemSE(0x481C, 0, 0, 0);
		unk248 = 7;
	}
}

void TTalk2D2::moveTalkWindow()
{
	// Reveal the next character in each of the three lines. The DOL indexes
	// each marker as line * 30 + progress and uses the per-marker delay table
	// at +0x281 when it first becomes visible.
	for (s32 line = 0; line < 3; ++line) {
		const u8 progress = unk224[line];
		if (progress == 0 || progress > unk228[line])
			continue;

		const s32 markerIndex = line * 30 + progress;
		J2DTextBox* marker = unk9C[markerIndex];
		if (marker->mVisible) {
			const s32 alpha = marker->getAlpha() + unk340;
			marker->setAlpha(alpha > 0xFF ? 0xFF : alpha);
			if (alpha >= 0xFF)
				++unk224[line];
		} else if (unk2DC <= 0) {
			unk2DC = unk281[markerIndex];
			marker->mVisible = true;
			marker->setAlpha(0);
		} else {
			--unk2DC;
		}
	}

	const s32 line = unk274;
	if (unk224[line] < 30 && unk224[line] <= unk228[line])
		return;

	J2DPane* animatedPane;
	J2DPane* selectedPane;
	if (unk214 == -1) {
		if (unk26A) {
			unk78[line]->mVisible = false;
			unk84[line]->mVisible = true;
			animatedPane = unk84[line];
		} else {
			unk78[line]->mVisible = true;
			unk84[line]->mVisible = false;
			animatedPane = unk78[line];
		}
		selectedPane = unk6C[line];
	} else {
		selectedPane = unk204;
		animatedPane = unk20C[unk214];
	}

	if (!selectedPane->mVisible) {
		selectedPane->mVisible = true;
		selectedPane->setAlpha(0);
		animatedPane->setAlpha(0xFF);
		return;
	} else {
		s32 alpha = selectedPane->getAlpha() + 0x10;
		if (alpha > 0xFF)
			alpha = 0xFF;
		selectedPane->setAlpha(alpha);
	}

	s32 alpha = animatedPane->getAlpha();
	if (unk26B) {
		alpha += 2;
		if (alpha > 0xFF) {
			unk26B = 0;
			alpha = 0xFF;
		}
	} else {
		alpha -= 4;
		if (alpha < 0x3C) {
			unk26B = 1;
			alpha = 0x3C;
		}
	}
	animatedPane->setAlpha(alpha);

	if (unk214 == 1) {
		snprintf(unk208->getStringPtr(), 0x5E,
		         "\033CC[ffffff60]\033GC[ffffff60]%s"
		         "\033CC[ffff%02x]\033GC[ffff%02x]\n%s",
		         mChoiceText[0], alpha, alpha, mChoiceText[1]);
	} else if (unk214 == 0) {
		snprintf(unk208->getStringPtr(), 0x5E,
		         "\033CC[ffff%02x]\033GC[ffff%02x]%s"
		         "\n\033CC[ffffff60]\033GC[ffffff60]%s",
		         alpha, alpha, mChoiceText[0], mChoiceText[1]);
	}
}

void TTalk2D2::checkBoardControler()
{
	const bool selectButtonPressed = unk24C->checkFrameMeaning(
	    TMarioGamePad::MEANING_SELECT_A | TMarioGamePad::MEANING_SELECT_B);

	if (!unk26A) {
		if (selectButtonPressed) {
			if (gpMSound->gateCheck(0x481C))
				MSoundSESystem::MSoundSE::startSoundSystemSE(0x481C, 0, 0, 0);
			unk248 = 7;
		}
		return;
	}

	// A pending text-control tag bypasses the button gate in the DOL.
	if (!unk26D && !selectButtonPressed)
		return;

	if (gpMSound->gateCheck(0x481C))
		MSoundSESystem::MSoundSE::startSoundSystemSE(0x481C, 0, 0, 0);

	const JUTPoint start(0, -0x258);
	const JUTPoint control(0, 0);
	const JUTPoint end(0, 0);
	unk14->setPanePosition(0x3C, start, control, end);

	if (unk270 & 1) {
		if (unk264 == 0x1E) {
			if (gpMSound->gateCheck(0x4851))
				MSoundSESystem::MSoundSE::startSoundSystemSE(0x4851, 0, 0, 0);
		} else if (unk28) {
			if (gpMSound->gateCheck(0x481A))
				MSoundSESystem::MSoundSE::startSoundSystemSE(0x481A, 0, 0, 0);
			gpMSound->talkModeOut();
		} else {
			gpMSound->talkModeOut();
		}

		gpCamera->makeMtxForPrevTalk();
		SMSGetMarDirector()->getConsole()->startAppearTelop(false);
		SMSRumbleMgr->finishPause();
		unk252 = 0;
	}

	unk248 = 6;
}

void TTalk2D2::moveBoardWindow()
{
	s32 alpha = unk1C->getAlpha();
	if (alpha < 0xFF) {
		alpha += 4;
		if (alpha > 0xFF) {
			unk24->setAlpha(0);
			unk20->setAlpha(0);
			unk26B = 1;
			alpha = 0xFF;
		}
		unk1C->setAlpha(alpha);
		return;
	}

	J2DPane* pane = unk26A ? unk24 : unk20;
	alpha = pane->getAlpha();
	if (unk26B) {
		alpha += 4;
		if (alpha > 0xFF) {
			unk26B = 0;
			alpha = 0xFF;
		}
	} else {
		alpha -= 4;
		if (alpha < 0) {
			unk26B = 1;
			alpha = 0;
		}
	}
	pane->setAlpha(alpha);
}

bool TTalk2D2::openNormalWindow()
{
	bool finished = false;
	if (static_cast<u16>(unk2DE) > 2
	    && unk24C->checkMeaning(TMarioGamePad::MEANING_SELECT_A)) {
		unk26C = 1;
		unk340 = 0x80;
	}

	bool allLinesOpened = true;
	for (s32 line = 0; line <= static_cast<s32>(unk274); ++line) {
		f32& openProgress = unk234[line];
		if (openProgress > -0.109f) {
			allLinesOpened = false;
			openProgress -= unk338;
		}

		if (openProgress < 1.0f && line != 0) {
			const s32 precedingMarker
			    = (line - 1) * 30 + unk228[line - 1];
			if (!unk9C[precedingMarker]->mVisible)
				openProgress = 1.0f;
		}

		if (!unk224[line] && openProgress < unk33C) {
			if (gpMSound->gateCheck(0x4827))
				MSoundSESystem::MSoundSE::startSoundSystemSE(0x4827, 0, 0,
				                                             0);
			unk224[line] = 1;
			unk9C[line * 30]->mVisible = true;
		}
	}

	if (allLinesOpened) {
		for (s32 line = 0; line <= static_cast<s32>(unk274); ++line)
			unk3C[line]->mVisible = true;
		finished = true;
	}

	for (s32 line = 0; line < 3; ++line) {
		if (!unk224[line])
			continue;

		s32 markerIndex = line * 30 + unk224[line];
		while (unk224[line] <= unk228[line]) {
			J2DTextBox* marker = unk9C[markerIndex];
			if (marker->mVisible) {
				s32 alpha = marker->getAlpha() + unk340;
				if (alpha > 0xFF)
					alpha = 0xFF;
				marker->setAlpha(alpha);
				unk2DE = markerIndex;
				if (marker->getAlpha() < 0xFF)
					break;

				++unk224[line];
				++markerIndex;
				continue;
			}

			if (unk2DC <= 0) {
				unk2DC = unk26C ? 0 : unk281[markerIndex];
				marker->mVisible = true;
				marker->setAlpha(0);
			} else {
				--unk2DC;
			}
			break;
		}
	}
	return finished;
}

bool TTalk2D2::openBoardWindow()
{
	bool finished = false;
	switch (unk29) {
	case 0:
		if (unk14->update()) {
			unk14->setPanePosition(0x19, JUTPoint(0, 0x50),
			                      JUTPoint(0, 0x50), JUTPoint(0, 0));
			++unk29;
		}
		break;
	case 1:
		if (unk14->update()) {
			unk1C->setAlpha(0);
			unk1C->show();
			finished = true;
			++unk29;
		}
		break;
	}
	return finished;
}

void TTalk2D2::makeBoxLine(s8 line, char* text)
{
	const s32 lineIndex = line;
	JUTPoint control(0, unk54[lineIndex]->mBounds.y1 - unk220);
	JUTPoint start(unk48[lineIndex]->getBounds().x1,
	               unk48[lineIndex]->getBounds().y1);
	JUTPoint end(unk60[lineIndex]->getBounds().x2 - 10,
	             unk60[lineIndex]->getBounds().y1);

	f32 t = 0.0f;
	f32 previousX;
	f32 previousY;
	makeLine(&previousX, &previousY, t, start, control, end);
	s32 textOffset = 0;

	for (s32 markerIndex = 0; markerIndex < 30; ++markerIndex) {
		const s32 paneIndex = lineIndex * 30 + markerIndex;
		J2DTextBox*& marker = unk9C[paneIndex];

		if (text != 0) {
			char* markerText = marker->getStringPtr();
			const u8 firstByte = static_cast<u8>(text[textOffset]);
			markerText[0] = firstByte;
			if (firstByte >= 0x80) {
				markerText[1] = text[textOffset + 1];
				textOffset += 2;
			} else {
				markerText[1] = '\0';
				++textOffset;
			}
		}

		const u16 character = static_cast<s8>(marker->getStringPtr()[0]);
		if (character == 0)
			break;

		JUTFont::TWidth glyphWidth;
		gpSystemFont->getWidthEntry(static_cast<u8>(character), &glyphWidth);

		s32 verticalOffset = 0;
		bool keepFontSize = false;
		switch (static_cast<u8>(marker->getStringPtr()[0])) {
		case 0x23:
		case 0x25:
			verticalOffset = 2;
			break;
		case 0x24:
			t += 0.04f;
			keepFontSize = true;
			verticalOffset = 1;
			break;
		case 0x2A:
			t += 0.01f;
			break;
		case 0x2B:
			verticalOffset = 2;
			break;
		case 0x3C:
		case 0x3E:
			verticalOffset = 1;
			break;
		case 0x40:
			verticalOffset = 2;
			break;
		case 0xA5:
			t += 0.03f;
			verticalOffset = 2;
			break;
		}

		t += unk94 * (0.7f * glyphWidth.field_0x1 + 4.0f);
		f32 currentX;
		f32 currentY;
		makeLine(&currentX, &currentY, t, start, control, end);

		if (markerIndex < 29
		    && unk9C[paneIndex + 1]->getStringPtr()[0] == 'm')
			t += 0.01f;

		f32 angle = fabsf(atan2f(currentY - previousY, currentX - previousX));
		if (currentX > 0.0f)
			angle *= -1.0f;

		const f32 sumX = previousX + currentX;
		const f32 sumY = previousY + currentY;
		const f32 translateX = -0.5f * sumX;
		const f32 translateY = -0.5f * sumY;
		const f32 cosine = cosf(angle);
		const f32 sine = sinf(angle);
		f32 positionX = previousX * cosine + previousY * -sine;
		f32 positionY = previousX * sine + previousY * cosine;
		positionX += translateX;
		positionY += translateY;
		positionX += 0.5f * sumX;
		positionY += 0.5f * sumY;

		const s32 roundedX = static_cast<s32>(positionX + (positionX > 0.0f ? 0.5f : -0.5f));
		const s32 roundedY = static_cast<s32>(positionY + (positionY > 0.0f ? 0.5f : -0.5f));
		if (!keepFontSize) {
			marker->mFontSizeX = 20;
			marker->mFontSizeY = 22;
		}

		marker->move(static_cast<s16>(roundedX),
		            -38 - static_cast<s16>(roundedY) + verticalOffset);
		marker->setBasePosition(J2DBasePosition_4);
		marker->mRotation = (180.0f * angle) / 3.1415927f;
		unk30[lineIndex]->mPaneTree.appendChild(&marker->mPaneTree);

		if (t > 1.05f) {
			if (unk228[lineIndex] > markerIndex)
				unk228[lineIndex] = markerIndex;
			break;
		}

		makeLine(&previousX, &previousY, t, start, control, end);
	}
}

void TTalk2D2::openTalkWindow(TBaseNPC* npc)
{
	if (npc != nullptr)
		gpCamera->makeMtxForTalk(npc);

	if (unk28) {
		unk14->setPanePosition(0x3C, JUTPoint(0, -0x320),
		                      JUTPoint(0, 0x50), JUTPoint(0, 0x50));
		unk14->update();
		unk1C->setAlpha(0);
		unk248 = 4;
	} else {
		unk248 = 2;
	}
	unk90->setAlpha(0xFF);

	s32 boxX;
	s32 boxY;
	s32 boxRotation;
	switch (gpCamera->mMode) {
	case 0x2D:
		unk330 = 0xA0;
		unk332 = 0x87;
		unk334 = 0x14;
		boxX = 0xBE;
		boxY = 0xD2;
		boxRotation = 0x16;
		break;
	case 0xC:
	default:
		unk330 = 0x184;
		unk332 = 0x73;
		unk334 = -0x12;
		boxX = 0x16E;
		boxY = 0xBF;
		boxRotation = 0x156;
		break;
	}
	unk90->move(unk330, unk332);
	unk90->mRotation = unk334;
	unk204->move(boxX, boxY);
	unk204->mRotation = boxRotation;

	unk250 = 1;
	gpMarDirector->getConsole()->startDisappearTelop();
	gpMarDirector->getConsole()->startDisappearBalloon(
	    gpMarDirector->getConsole()->unk3E0, true);
	gpMarDirector->getConsole()->startDisappearMario();

	if (unk264 == 0x1E) {
		if (gpMSound->gateCheck(0x4848))
			MSoundSESystem::MSoundSE::startSoundSystemSE(0x4848, 0, 0, 0);
	} else if (unk28) {
		if (gpMSound->gateCheck(0x4819))
			MSoundSESystem::MSoundSE::startSoundSystemSE(0x4819, 0, 0, 0);
		gpMSound->talkModeIn(false);
	} else {
		gpMSound->talkModeIn(true);
	}

	SMSRumbleMgr->startPause();
}

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

// Reconstruct the NPC-specific message selection, message-loader setup, and
// the sound/talk-mode handling that follows it.
void TTalk2D2::setMessageID(u32 messageID, u32 secondaryParam)
{
	TBaseNPC* npc = SMSGetMarDirector()->getTalkingNPC();
	if (!npc->checkLiveFlag(LIVE_FLAG_UNK200)) {
		unk264 = messageID;
	} else {
		bool normalMonteM  = npc->isNormalMonteM();
		bool normalMonteW  = npc->isNormalMonteW();
		bool specialMonteM = npc->isSpecialMonteM();
		bool specialMonteW = npc->isSpecialMonteW();
		bool monteM        = normalMonteM || specialMonteM;
		bool monteW        = normalMonteW || specialMonteW;

		if (monteM || monteW) {
			if (monteW)
				unk264 = npc->isChild() ? 0x31 : 0x2C;
			else
				unk264 = npc->isChild() ? 0x2E : 0x28;
		} else {
			bool normalMareM  = npc->isNormalMareM();
			bool normalMareW  = npc->isNormalMareW();
			bool specialMareM = npc->isSpecialMareM();
			bool specialMareW = npc->isSpecialMareW();
			bool mareM        = normalMareM || specialMareM;
			bool mareW        = normalMareW || specialMareW;

			if (mareM || mareW) {
				if (mareW)
					unk264 = npc->isChild() ? 0x32 : 0x2D;
				else
					unk264 = npc->isChild() ? 0x2F : 0x29;
			} else if (npc->mActorType == 0x04000016) {
				unk264 = 0x2A;
			} else if (npc->mActorType == 0x04000010) {
				unk264 = 0x30;
			}
		}

		for (s32 i = 0; i < 10; ++i) {
			if (npc == unk2E0[i].npc) {
				unk264 = unk2E0[i].messageID;
				break;
			}
		}
	}
	// The DOL updates this flag for every NPC, independently of the live-flag
	// message selection above (actor type 0x0400001D).
	unk28 = npc->mActorType == 0x0400001D;

	unk270 = secondaryParam;
	unk278 = 0;
	unk214 = -1;
	unk26A = 0;
	unk27C = -1;
	unk280 = 0;
	unk26C = 0;
	unk26D = 0;
	unk254 = 0;
	unk29 = false;
	unk340 = 0x40;

	// The high half selects the language/message bank; the low half is the
	// entry number. A missing entry in the selected bank falls back to ID 4
	// in the default bank, exactly as the DOL's two getMessageEntry paths do.
	TMessageLoader* loader;
	if ((unk264 & 0xFFFF0000) == 0)
		loader = unk25C;
	else
		loader = unk258;

	TMessageLoader::EntryInfo* entry = 0;
	if (loader->unk4 != 0) {
		entry = loader->getMessageEntry(unk264 & 0xFFFF);
		if (entry == 0) {
			unk264 = 4;
			loader = unk25C;
			entry = loader->getMessageEntry(unk264);
		}
		setupTextBox(loader->unk4, reinterpret_cast<JMSMesgEntry*>(entry));
	} else {
		unk264 = 3;
		loader = unk25C;
		entry = loader->getMessageEntry(unk264);
		setupTextBox(loader->unk4, reinterpret_cast<JMSMesgEntry*>(entry));
	}

	unk260 = loader;
	unk2DC = 0;

	if (unk254 != 0) {
		const u8 soundIndex = reinterpret_cast<const u8*>(unk254)[0xA];
		const u32 soundID = scTalkSoundList[soundIndex];
		if (soundID != 0xFFFFFFFF && gpMSound->gateCheck(soundID)) {
			if (soundID & 1)
				MSBgm::startBGM(soundID);
			else
				MSoundSESystem::MSoundSE::startSoundSystemSE(soundID, 0, 0, 0);
		}
	}

	if (unk248 == 1) {
		unk248 = 3;
		// The target writes 0xFF to byte +0xCC of the object at +0x90.
		unk90->setAlpha(0xFF);
	}
	unk252 = 1;
}

void TTalk2D2::loadAfter()
{
	JDrama::TNameRef::loadAfter();
	JUTPoint control(0, unk54[1]->mBounds.y1 - unk220);
	JUTPoint start(unk48[1]->getBounds().x1, unk48[1]->getBounds().y1);
	JUTPoint end(unk60[1]->getBounds().x2 - 10, unk60[1]->getBounds().y1);
	f32 length = 0.0f;
	f32 previousX;
	f32 previousY;
	makeLine(&previousX, &previousY, 0.0f, start, control, end);
	for (f32 t = 0.01f; t <= 1.0f; t += 0.01f) {
		f32 x;
		f32 y;
		makeLine(&x, &y, t, start, control, end);
		const f32 dx = x - previousX;
		const f32 dy = y - previousY;
		length += JGeometry::TUtil<f32>::sqrt(dx * dx + dy * dy);
		previousX = x;
		previousY = y;
	}
	unk94 = 1.0f / length;

	// The three f_* groups form the 3x3 control-pane grid. The original hides
	// each pane's visible bit before creating its 90 small text-box markers.
	for (int i = 0; i < 3; ++i) {
		unk48[i]->mVisible = false;
		unk54[i]->mVisible = false;
		unk60[i]->mVisible = false;
	}

	// The 90 glyph panes start hidden; makeBoxLine later places them on the curve.
	for (int i = 0; i < 90; ++i) {
		unk9C[i] = new J2DTextBox(0, JUTRect(0, 0, 20, 20), gpSystemFont->getResFont(), "\x82\xA0", HBIND_LEFT,
		                     VBIND_CENTER);
		unk9C[i]->setFontSize(20, 24);
		JUtility::TColor white(0xFFFFFFFF);
		unk9C[i]->setBlackWhite(0xFFFFFF00, white);
		unk9C[i]->hide();
	}

	unk234[0] = 1.0f;
	unk6C[0]->hide();
	unk234[1] = 2.0f;
	unk6C[1]->hide();
	unk234[2] = 3.0f;
	unk6C[2]->hide();
	unk244->mWrapT = GX_REPEAT;
	unk244->mWrapS = GX_CLAMP;
	unk90->move(0x189, 0x73);
	unk90->mRotation = -18.0f;
	unk330 = unk90->mBounds.x1;
	unk332 = unk90->mBounds.y1;
	unk334 = unk90->mRotation;
	unk204->hide();
	char* spaces = new char[94];
	for (int i = 0; i < 93; ++i)
		spaces[i] = ' ';
	spaces[93] = '\0';
	unk208->setString(spaces);
	const void* messageData = unk25C->unk4;
	JMSMesgEntry* entry = reinterpret_cast<JMSMesgEntry*>(unk25C->getMessageEntry(3));
	setupTextBox(messageData, entry);
	unk260 = unk25C;
	const char* names[10] = {
		"\x8B\xF3\x8D\x60\x92\xBE\x82\xDD\x83\x82\x83\x93\x83\x65",
		"\x83\x82\x83\x93\x83\x65" "1", "", "", "", "", "", "", "", "",
	};
	u32 messageIDs[10] = { 0x2B, 0x28, 0, 0, 0, 0, 0, 0, 0, 0 };
	for (int i = 0; i < 10; ++i) {
		unk2E0[i].npc = static_cast<TBaseNPC*>(JDrama::TNameRefGen::search(names[i]));
		unk2E0[i].messageID = messageIDs[i];
	}
}

void TTalk2D2::load(JSUMemoryInputStream& stream)
{
	JDrama::TNameRef::load(stream);
	JKRArchive* archive = static_cast<JKRArchive*>(JKRFileLoader::getVolume("game_6"));
	unk2C = new J2DSetScreen("message_2.blo", archive);
	for (int i = 0; i < 3; ++i) {
		unk30[i] = unk2C->search('me_1' + i);
		unk3C[i] = unk2C->search('bac1' + i);
		unk48[i] = unk2C->search('\0f_1' + i * 3);
		unk54[i] = unk2C->search('\0f_2' + i * 3);
		unk60[i] = unk2C->search('\0f_3' + i * 3);
		unk6C[i] = unk2C->search('cu_1' + i);
		unk78[i] = unk2C->search('cc_1' + i);
		unk84[i] = unk2C->search('cs_1' + i);
	}
	unk244 = new JUTTexture(static_cast<const ResTIMG*>(
	    JKRGetResource("/game_6/timg/message_back_1.bti")));
	unk90 = unk2C->search('me_0');
	unk258 = new TMessageLoader;
	switch (TFlagManager::getInstance()->getFlag(0xA0001)) {
	case 0:
		unk258->loadMessageData("/scene/map/message_en.bmg");
		break;
	case 1:
		unk258->loadMessageData("/scene/map/message_ge.bmg");
		break;
	case 2:
		unk258->loadMessageData("/scene/map/message_fr.bmg");
		break;
	case 3:
		unk258->loadMessageData("/scene/map/message_sp.bmg");
		break;
	case 4:
		unk258->loadMessageData("/scene/map/message_it.bmg");
		break;
	default:
		unk258->loadMessageData("/scene/map/message.bmg");
		break;
	}
	unk25C = new TMessageLoader;
	unk25C->loadMessageData("/cmn2d/sys_message.bmg");
	unk204 = unk2C->search('me_4');
	unk208 = static_cast<J2DTextBox*>(unk2C->search('slct'));
	unk208->setFont(gpSystemFont);
	for (int i = 0; i < 2; ++i) {
		unk20C[i] = unk2C->search('sc_1' + i);
		mChoiceText[i] = new char[0x11];
	}
	unk10 = new J2DSetScreen("message_board_1.blo", archive);
	unk14 = new TBoundPane(unk10, 'mb_0');
	unk18 = static_cast<J2DTextBox*>(unk10->search('text'));
	SMSMakeTextBuffer(unk18, 0x200);
	unk18->setFont(gpSystemFont);
	unk1C = unk10->search('cu_1');
	unk20 = unk10->search('cc_1');
	unk24 = unk10->search('cs_1');
}

TTalk2D2::TTalk2D2(const char* name)
    : JDrama::TViewObj(name)
    , unk28(false)
    , unk2C(nullptr)
    , unk90(nullptr)
    , unk94(0.0f)
    , unk214(-1)
    , unk220(0x1A)
    , unk222(0)
    , unk248(0)
    , unk24C(nullptr)
    , unk250(1)
    , unk251(0)
    , unk252(0)
    , unk254(nullptr)
    , unk264(3)
    , unk26A(0)
    , unk26B(0)
    , unk26C(0)
    , unk26D(0)
    , unk270(1)
    , unk274(0)
    , unk278(0)
    , unk27C(-1)
    , unk280(0)
    , unk2DC(0)
    , unk330(0)
    , unk332(0)
    , unk334(0)
    , unk338(0.04f)
    , unk33C(1.0f)
    , unk340(100)
{
	gpTalk2D = this;
	for (int i = 0; i < 90; ++i)
		unk9C[i] = nullptr;
	for (int i = 0; i < 3; ++i) {
		unk224[i] = 0;
		unk6C[i] = nullptr;
		unk78[i] = nullptr;
		unk228[i] = 0;
	}
}
