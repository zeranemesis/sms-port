#include <GC2D/MovieSubtitle.hpp>
#include <stdio.h>
#include <macros.h>
#include <JSystem/J2D/J2DScreen.hpp>
#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/J2D/J2DOrthoGraph.hpp>
#include <System/THPRender.hpp>
#include <System/Application.hpp>
#ifdef VERSION_GMSP01
#include <System/FlagManager.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <JSystem/JSupport/JSUMemoryOutputStream.hpp>
#endif

// TODO: removeme
static const char* dummyMactorStringValue1 = "\0\0\0\0\0\0\0\0\0\0\0";
static const char* SMS_NO_MEMORY_MESSAGE   = "メモリが足りません\n";

namespace {

const int cLongHeightMovieIdList[] = { 9, 20 };

bool is_longheight_movie(u32 param_1)
{
	const int* i = cLongHeightMovieIdList;
	const int* e = cLongHeightMovieIdList + ARRAY_COUNT(cLongHeightMovieIdList);
	while (i != e && *i != param_1)
		++i;
	return i != e;
}

} // namespace

TMovieSubTitle::TMovieSubTitle(const TTHPRender* param_1)
    : unk10(param_1)
    , unk14(nullptr)
    , unk18(nullptr)
    , unk1C(nullptr)
{
}

void TMovieSubTitle::setupResource(const char* param_1, JKRArchive* param_2)
{
	if (is_longheight_movie(SMSGetApplication()->getMovie()))
		unk14 = new J2DSetScreen("demo_1.blo", param_2);
	else
		unk14 = new J2DSetScreen("demo_2.blo", param_2);

	hide();

	unk18 = (J2DTextBox*)unk14->search('me_a');
	unk1C = (J2DTextBox*)unk14->search('me_b');

	char buffer[0x400];

	// inline?
	memset(buffer, ' ', ARRAY_COUNT(buffer));
	buffer[ARRAY_COUNT(buffer) - 1] = '\0';
	unk18->setString(buffer);
	unk1C->setString(buffer);

	unk20 = new TMessageLoader;
	makeBmgName(buffer, ARRAY_COUNT(buffer), param_1);
	unk20->loadMessageData(buffer);

	unk24 = 0;
}

void TMovieSubTitle::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_MOVE)
		movement();

	if (cue & CUE_DRAW)
		draw(graphics);
}

void TMovieSubTitle::movement()
{
	if (unk28)
		checkSubTitleOff();
	else
		checkSubTitleOn();
}

void TMovieSubTitle::checkSubTitleOff()
{
	int frame = unk10->getFrameNumber();
	if (getCurEntry()) {
		if (getCurEntry()->unk6 <= frame) {
			hide();
			++unk24;
		}
	}
}

void TMovieSubTitle::checkSubTitleOn()
{
	int frame = unk10->getFrameNumber();
	if (getCurEntry()) {
		if (getCurEntry()->unk4 <= frame)
			show();
	}
}

void TMovieSubTitle::show()
{
	unk28 = true;
	unk14->show();
	setCurMessage();
}

void TMovieSubTitle::hide()
{
	unk28 = false;
	unk14->hide();
}

const TMessageLoader::EntryInfo* TMovieSubTitle::getCurEntry() const
{
	if (unk20->getMessageNum() <= unk24)
		return nullptr;

	return unk20->getMessageEntry(unk24);
}

void TMovieSubTitle::setCurMessage()
{
#ifdef VERSION_GMSP01
	const TMessageLoader::EntryInfo* entry = nullptr;
	if (unk20->getMessageNum() > unk24
	    && TFlagManager::getInstance()->getFlag(0x90001))
		entry = unk20->getMessageEntry(unk24);

	const u8* message = unk20->getMessageData() + entry->unk0;
	u8 code;
	u8 length;
	u8 color[4];
	bool hasColor;
	JSUMemoryInputStream input(message, 0x400);
	JSUMemoryOutputStream output(unk18->getStringPtr(), 0x400);
	char markup[0x100];

	while (input.getPosition() != input.getLength()) {
		input.read(&code, 1);

		if (code == '\n') {
			output.write(&code, 1);
			continue;
		}
		if (code == '\0') {
			output.write(&code, 1);
			return;
		}
		if (code == 0x1A) {
			input.read(&length, 1);
			input.skip(length - 2);
			continue;
		}

		hasColor = true;
		switch (code) {
		case '@': color[0] = 100; color[1] = 255; color[2] = 100; break;
		case '#': color[0] = 255; color[1] = 160; color[2] = 100; break;
		case '%': color[0] = 255; color[1] = 255; color[2] = 0; break;
		case '*':
		case '+':
		case '<':
		case '>':
		case 0xA5:
			color[0] = 220;
			color[1] = 220;
			color[2] = 220;
			break;
		case 'n': color[0] = 110; color[1] = 230; color[2] = 255; break;
		default: hasColor = false; break;
		}

		if (hasColor) {
			color[3] = 255;
			snprintf(markup, 0xFF,
			         "\033GM[0]\033CC[%02x%02x%02x]\033SH[3]\033CD[4]",
			         color[0], color[1], color[2]);
			output.write(markup, 0x1D);
		}
		output.write(&code, 1);
		if (hasColor) {
			snprintf(markup, 0xFF,
			         "\033GM[0]\033CC\033FX\033FY\033SH\033CU[4]\033GM[1]");
			output.write(markup, 0x1E);
		}
	}

	snprintf(unk1C->getStringPtr(), 0x400, "%s", unk18->getStringPtr());
#else
	const char* msg
	    = (const char*)(unk20->getMessageData() + getCurEntry()->unk0);
	snprintf(unk18->getStringPtr(), 256, "%s", msg);
	snprintf(unk1C->getStringPtr(), 256, "%s", msg);
#endif
}

void TMovieSubTitle::makeBmgName(char* buffer, int, const char* param_3)
{
	sprintf(buffer, "/subtitle/%s", param_3);
	char* it = strrchr(buffer, '.');
	strcpy(it, ".bmg");
}

void TMovieSubTitle::draw(JDrama::TGraphics* param_1)
{
	J2DOrthoGraph graph(param_1->getViewport());
	graph.setup2D();
	unk14->draw(0, 0, &graph);
}
