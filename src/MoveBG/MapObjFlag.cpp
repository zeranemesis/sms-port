#include <MoveBG/MapObjFlag.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <dolphin/gx.h>
#include <System/MarDirector.hpp>
#include <stdlib.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

TMapObjFlagManager* gpMapObjFlagManager;

TMapObjFlagManager::~TMapObjFlagManager() { }

f32 TMapObjFlag::mFlutterSpeed = 4.0f;

TMapObjFlag::TMapObjFlag(const char* name)
    : THitActor(name)
{
	unk68 = 0.0f;
	unk6C = 0.0f;
	unk70 = 0;
	unk74 = 0;
	unk78 = 0;
	unk7C = 125.0f;
	unk80 = 130.0f;
	unk84 = 20.0f;
	unk88 = 4.0f * ((f32)rand() * 0.000030517578f);
	unkBC = 1;
	unkB8 = 0.0f;
	unkA8 = 0.0f;
	unk98 = 0.0f;
	unkA4 = 0.0f;
	unk94 = 0.0f;
	unkB0 = 0.0f;
	unk90 = 0.0f;
	unkAC = 0.0f;
	unk9C = 0.0f;
	unkB4 = 1.0f;
	unkA0 = 1.0f;
	unk8C = 1.0f;
}

void TMapObjFlag::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);
	char name[0x40];
	stream.readString(name, sizeof(name));
	init(name);
}

TMapObjFlagManager::TMapObjFlagManager(const char* name)
    : JDrama::TViewObj(name)
{
	gpMapObjFlagManager = this;
}

void TMapObjFlagManager::load(JSUMemoryInputStream& stream)
{
	JDrama::TNameRef::load(stream);
	char buffer[8];
	stream.readString(buffer, sizeof(buffer));

	switch (gpMarDirector->mMap) {
	case 0:
		TMapObjFlag::mFlutterSpeed = 16.0f;
		break;
	case 2:
		TMapObjFlag::mFlutterSpeed = 16.0f;
		break;
	case 4:
		TMapObjFlag::mFlutterSpeed = 12.0f;
		break;
	default:
		TMapObjFlag::mFlutterSpeed = 8.0f;
		break;
	}
}
