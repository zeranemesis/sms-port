#include <GC2D/ConsoleStr.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <GC2D/BoundPane.hpp>
#include <GC2D/ExPane.hpp>
#include <GC2D/MessageUtil.hpp>
#include <System/Application.hpp>
#include <System/MarDirector.hpp>
#include <System/StageUtil.hpp>
#include <System/FlagManager.hpp>
#include <JSystem/J2D/J2DPicture.hpp>
#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/J2D/J2DScreen.hpp>
#include <JSystem/J2D/J2DOrthoGraph.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/JParticle/JPAEmitterManager.hpp>
#include <JSystem/JUtility/JUTResFont.hpp>
#include <dolphin/gx/GXCull.h>
#include <stdio.h>

JUTPoint TConsoleStr::cShineGetRight1(150, -50);
JUTPoint TConsoleStr::cShineGetLeft1(-21, 7);
JUTPoint TConsoleStr::cShineGetRight2(0, 0);
JUTPoint TConsoleStr::cShineGetLeft2(-50, 7);
JUTPoint TConsoleStr::cShineGetRight3(0, 0);
JUTPoint TConsoleStr::cShineGetLeft3(-200, 65);

TConsoleStr::TConsoleStr(const char* name)
    : JDrama::TViewObj(name)
    , unk4C(nullptr)
    , unk50(nullptr)
    , unk20(0xB4)
	, unk24(0)
	, unk2A8(0)
	, unk2A9(0)
	, unk2B8(0)
    , unk2BC(7)
{
}

f32 TConsoleStr::getWipeCloseTime() { return 30.0f / SMSGetVSyncTimesPerSec(); }

void TConsoleStr::load(JSUMemoryInputStream& stream)
{
	// The ROM seeds the three counts with the out-of-range default and then
	// only overwrites what differs per case, so there is no `default:` label
	// and cases 2/3 touch one and two fields respectively.
	mGoPaneCount     = 11;
	mShinePaneCount  = 11;
	mMissPaneCount   = 12;

	switch (TFlagManager::getInstance()->getFlag(0xA00001)) {
	case 0:
		mGoPaneCount = 3;
		mShinePaneCount = 6;
		mMissPaneCount = 7;
		break;
	case 1:
		mGoPaneCount = 4;
		mShinePaneCount = 11;
		mMissPaneCount = 12;
		break;
	case 2:
		mGoPaneCount = 11;
		mShinePaneCount = 11;
		mMissPaneCount = 8;
		break;
	case 3:
		mGoPaneCount = 10;
		mMissPaneCount = 8;
		break;
	case 4:
		mGoPaneCount = 4;
		mShinePaneCount = 10;
		mMissPaneCount = 7;
		break;
	}

	JKRArchive* arch = SMSSwitch2DArchive("guide", gArBkConsole);
	JDrama::TViewObj::load(stream);
	unk4C = new J2DSetScreen("big_tx_1.blo", arch);
	unk4C->setCullBack(GX_CULL_BACK);
	unk50 = new J2DSetScreen("scenario_demo_1.blo", arch);
	unk50->setCullBack(GX_CULL_BACK);

	// The pane banks run past ten entries, so the two trailing digits of the
	// four-character texture name are built as a decimal pair.
	for (s32 i = 0; i < mGoPaneCount; ++i) {
		unk28[i] = new TBoundPane(unk4C, 'go00' + (i / 10) * 0x100 + i % 10);
		unk28[i]->unk0->hide();
	}

	for (s32 i = 0; i < mShinePaneCount; ++i) {
		unk244[i] = new TBoundPane(unk4C, 'sg00' + (i / 10) * 0x100 + i % 10);
	}

	for (s32 i = 0; i < mMissPaneCount; ++i) {
		unk268[i] = new TBoundPane(unk4C, 'ms00' + (i / 10) * 0x100 + i % 10);
		mMissBaseRotation[i] = (s32)unk268[i]->getPane()->getRotation();
	}

	for (int i = 0; i < 5; ++i) {
		unk27C[i] = new TExPane(unk4C, 're00' + i);
	}

	unk2A0[0] = (J2DTextBox*)unk50->search('\0map');
	SMSMakeTextBuffer(unk2A0[0], 0x80);
	unk2A0[1] = (J2DTextBox*)unk50->search('stry');
	SMSMakeTextBuffer(unk2A0[1], 0x80);

	for (int i = 0; i < 2; ++i) {
		unk290[i] = new TExPane(unk50, 'msk1' + i);
		unk2A0[i]->setFont(gpSystemFont);
	}

	unk298[0] = new TExPane(unk50, 'wp_l');
	unk298[1] = new TExPane(unk50, 'wp_r');

	u32 uVar1     = SMS_getShineStage(gpMarDirector->mMap);
	u32 uVar9     = TFlagManager::getInstance()->getFlag(0x40003);
	void* pvVar10 = JKRGetResource("/cmn2d/stagename.bmg");
	unk2A0[0]->setString(SMSGetMessageData(pvVar10, uVar1));

	if (gpMarDirector->mMap != 15) {
		void* pvVar10 = JKRGetResource("/cmn2d/scenarioname.bmg");

		s16 uVar2 = SMS_getShineID(uVar1, (u8)uVar9, false);

		const void* puVar15;
		if (pvVar10 == nullptr || uVar2 == -1)
			puVar15 = "";
		else
			puVar15 = SMSGetMessageData(pvVar10, SMS_getNormalStage(uVar2));

		snprintf(unk2A0[1]->getStringPtr(), 0x80, "%s", puVar15);
	}
}

void TConsoleStr::loadAfter()
{
	JDrama::TViewObj::loadAfter();
	for (s32 i = 0; i < mGoPaneCount; ++i)
		unk2AC[i] = nullptr;
}

void TConsoleStr::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// TODO: 89.3%. Everything matches except the GO-banner drawing loop, where
	// the ROM re-evaluates the `u32 -> f32` conversion of the alpha inside the
	// `if` (two double temporaries at 0x258/0x260) while we hoist it out, and
	// our frame is 0x240 against the ROM's 0x2C0.
	if (cue & CUE_MOVE) {
		if (gpMarDirector->mState != 5) {
			bool bVar6 = false;

			switch (unk2B8) {
			case 1:
				bVar6 = processGo(unk18);
				break;

			case 2:
				if (unk18 == unk20) {
					unk244[0]->setPanePosition(0x28, cShineGetRight1,
					                           cShineGetLeft1, cShineGetLeft1);
					unk244[0]->getPane()->show();
				} else if (unk18 > unk20) {
					bVar6 = processShineGet((unk18 - unk20) * 2.0f);
				}
				break;

			case 3:
				bVar6 = processMiss(unk18 * 2.0f);
				break;

			case 4:
				bVar6 = processScenario(unk18);
				break;

			case 5:
				bVar6 = processReady(unk18 * 2.0f);
				if (unk18 >= 60.0f) {
					if (unk18 == 60.0f) {
						for (s32 i = 0; i < mGoPaneCount; ++i) {
							unk28[i]->getPane()->hide();
							unk28[i]->getPane()->setAlpha(0xff);
						}
					}
					bVar6 &= processGo(unk18 - 60.0f);
				}
				break;
			}

			if (unk2BC == 2) {
				bool uVar13 = true;

				for (int i = 0; i < 2; ++i) {
					uVar13 &= unk290[i]->update();
					unk2A0[i]->setAlpha(0xff
					                    - unk290[i]->getPane()->getAlpha());
				}

				if (uVar13) {
					unk2BC = 3;
					unk18  = 0.0f;
				}
			} else if (unk2BC == 8) {
				bool uVar13 = true;

				for (int i = 0; i < 2; ++i)
					uVar13 &= unk290[i]->update();

				if (uVar13) {
					unk2BC = 3;
					unk18  = 0.0f;
				}
			} else if (unk2BC == 5) {
				bool uVar13 = true;
				for (int i = 0; i < 2; ++i)
					uVar13 &= unk298[i]->update();
				if (uVar13) {
					unk2BC = 6;
					bVar6  = true;
					unk298[0]->getPane()->hide();
					unk298[1]->getPane()->hide();
				}
			} else if (unk2BC == 3) {
				unk18 += 1.0f;
				if (unk18 > 60.0f) {
					unk18  = 0.0f;
					unk2BC = 4;
				}
			} else if (unk2BC == 1) {
				if (unk2A8 != 0)
					startCloseWipe(false);
			}

			if (bVar6)
				unk2B8 = 0;

			unk18 += 0.5f;
		}

		if (!unk2A9 && gpMarDirector->mState != 4) {
			// The emitter list is stopped as soon as the state machine leaves
			// its update phase; the ROM reads/ORs the emitter's mStatus word.
			for (s32 i = 0; i < mGoPaneCount; ++i) {
				if (unk2AC[i])
					((JPABaseEmitter*)unk2AC[i])->setStatus(1);
			}

			unk2A9 = true;
		}

		if (unk2A9 && gpMarDirector->mState != 4) {
			for (s32 i = 0; i < mGoPaneCount; ++i) {
				if (unk2AC[i])
					((JPABaseEmitter*)unk2AC[i])->clearStatus(1);
			}

			unk2A9 = false;
		}
	}

	if ((cue & CUE_DRAW) && unk2B8 != 0) {
		const JDrama::TRect& rect = graphics->getScissor();

		J2DOrthoGraph local_1a0(graphics->getViewport());
		local_1a0.setup2D();

		if (unk2B8 == 1 && unk18 > 60.0f) {
			for (int i = 0; i < mGoPaneCount; ++i) {
				TBoundPane* pane = unk28[i];

				int local_b0[3] = { 4, 10, 20 };

				u32 uVar13       = pane->getPane()->getAlpha();
				JUTRect local_a0 = pane->getPane()->getBounds();

				for (int j = 0; j < 3; ++j) {
					int iVar9 = local_b0[j];

					if (unk34[iVar9 + 22 * i].x != 0) {
						pane->getPane()->setAlpha(uVar13 * 0.7f);

						pane->getPane()->resize(iVar9 * 3 - local_a0.getWidth(),
						                        iVar9 * 3 - local_a0.getHeight());

						JUTRect local_90 = pane->getPane()->getBounds();
						((J2DPicture*)pane->getPane())
						    ->draw(unk34[iVar9 + 22 * i].x,
						           unk34[iVar9 + 22 * i].y, local_90.getWidth(),
						           local_90.getHeight(), false, false, false);
					}
				}

				pane->getPane()->setAlpha(uVar13);
				pane->getPane()->resize(local_a0.getWidth(),
				                        local_a0.getHeight());
			}
		}

		local_1a0.setup2D();
		if (unk2B8 == 4) {
			unk50->draw(0, 0, &local_1a0);
		} else {
			unk4C->draw(0, 0, &local_1a0);
		}

		local_1a0.setup2D();
		graphics->setScissor(rect);
	}
}

void TConsoleStr::startAppearReady()
{
	if (unk2B8 == 1)
		return;

	unk2B8 = 1;
	unk18  = 0.0f;
	unk2A9 = 0;
	for (s32 i = 0; i < mGoPaneCount; ++i) {
		unk28[i]->getPane()->hide();
		unk28[i]->getPane()->setAlpha(0xff);
	}
}

void TConsoleStr::startAppearGo()
{
	if (unk2B8 == 1)
		return;

	unk2B8 = 1;
	unk18  = 0.0f;
	unk2A9 = 0;
	for (s32 i = 0; i < mGoPaneCount; ++i) {
		unk28[i]->getPane()->hide();
		unk28[i]->getPane()->setAlpha(0xff);
	}
}

void TConsoleStr::startAppearShineGet()
{
	if (unk2B8 != 0)
		return;

	unk2B8 = 2;
	unk18  = 0.0f;
	unk24  = 0;
}

void TConsoleStr::startAppearMiss()
{
	if (unk2B8 == 3)
		return;

	if (unk2B8 == 1) {
		for (s32 i = 0; i < mGoPaneCount; ++i)
			unk28[i]->getPane()->hide();
	}

	unk2B8 = 3;
	unk18  = 0.0f;

	for (s32 i = 0; i < mMissPaneCount; ++i) {
		unk268[i]->getPane()->hide();
		unk268[i]->getPane()->setAlpha(0);
	}

	unk268[0]->setPanePosition(0x28, JUTPoint(0, -270), JUTPoint(0, -270),
	                           JUTPoint(0, 30));

	unk268[0]->getPane()->show();
}

void TConsoleStr::startAppearScenario()
{
	if (unk2B8 != 0)
		return;

	unk2B8 = 4;
	unk2BC = 0;
	unk18  = 0.0f;
	unk1C  = -200;

	for (int i = 0; i < 2; ++i) {
		unk2A0[i]->setAlpha(0);
		unk290[i]->getPane()->show();
	}

	unk290[0]->setPaneOffset(0x1E, 0.0f, 0.0f, 0.0f,
	                         -(unk290[0]->getInitialBounds().y2 + 1));
	unk290[1]->setPaneOffset(0x1E, 0.0f, 0.0f, 0.0f,
	                         465 - unk290[1]->getInitialBounds().y1);
}

#pragma dont_inline on
bool TConsoleStr::processReady(int param_1)
{
	bool result = false;

	for (int i = 0; i < 5; ++i) {
		if (param_1 == i * 10) {
			JUTRect local_d8 = unk27C[i]->getPane()->getBounds();
			unk27C[i]->setCenteredSize(
			    0x1E, local_d8.getWidth(), local_d8.getHeight(),
			    local_d8.getWidth() + 80, local_d8.getHeight() + 80);
			unk27C[i]->getPane()->show();
			unk27C[i]->getPane()->setAlpha(0);
		} else if (param_1 < i * 10 + 30) {
			unk27C[i]->update();
			u16 alpha = unk27C[i]->getPane()->getAlpha();
			alpha += 9;
			if (alpha > 0xff)
				alpha = 0xff;
			unk27C[i]->getPane()->setAlpha(alpha);
		} else if (param_1 >= i * 10 + 130) {
			if (param_1 == i * 10 + 130) {
				JUTRect local_e8 = unk27C[i]->getPane()->getBounds();

				unk27C[i]->setCenteredSize(
				    0x1E, local_e8.getWidth() - 20, local_e8.getHeight() - 20,
				    local_e8.getWidth(), local_e8.getHeight());
			} else if (param_1 < i * 10 + 160) {
				unk27C[i]->update();
				s16 alpha = unk27C[i]->getPane()->getAlpha();
				alpha -= 9;
				if (alpha < 0)
					alpha = 0;
				unk27C[i]->getPane()->setAlpha(alpha);
			} else if (i == 4) {
				result = true;
			}
		}
	}

	return result;
}

extern JPAEmitterManager* gpEmitterManager4D2;

bool TConsoleStr::processGo(float param_1)
{
	bool result = false;

	if (param_1 < 90.0f) {
		for (s32 i = 0; i < mGoPaneCount; ++i) {
			if (param_1 == i * 5) {
				unk28[i]->setPanePosition(
				    0x28, JUTPoint(0, 60), JUTPoint(0, -40),
				    JUTPoint(0, -40));
				unk28[i]->getPane()->show();
			}
		}

		for (s32 i = 0; i < mGoPaneCount; ++i) {
			JUTRect bounds = unk28[i]->getPane()->getBounds();
			(void)bounds;

			if (unk28[i]->update()) {
				bool atOrigin = false;
				if (unk28[i]->unk14.x1 == 0 && unk28[i]->unk14.y1 == 0)
					atOrigin = true;

				if (!atOrigin) {
					JUTPoint startPosition(0, 0);
					JUTPoint middlePosition(0, -40);
					JUTPoint endPosition(0, -40);
					unk28[i]->setPanePosition(0x1E, startPosition,
					                         middlePosition, endPosition);
				}
			}
		}
	} else if (param_1 >= 95.0f) {
		if (param_1 == 95.0f) {
			for (int i = 0; i < mGoPaneCount; ++i) {
				if (i % 3 == 0) {
					unk28[i]->setPanePosition(
					    0x50, JUTPoint(0, 0), JUTPoint(-170, -180),
					    JUTPoint(-340, -360));
				} else if (i % 3 == 1) {
					unk28[i]->setPanePosition(
					    0x50, JUTPoint(0, 0), JUTPoint(0, -220),
					    JUTPoint(0, -440));
				} else {
					unk28[i]->setPanePosition(
					    0x50, JUTPoint(0, 0), JUTPoint(160, -180),
					    JUTPoint(320, -360));
				}
			}

			for (int i = 0; i < mGoPaneCount; ++i) {
				for (int j = 0; j < 16; ++j) {
					JUTRect globalBounds =
					    unk28[i]->getPane()->getGlobalBounds();
					unk34[i * 22 + j].set(globalBounds.x1, globalBounds.y1);
				}

				JUTRect emitterBounds = unk28[i]->getPane()->getBounds();
				JGeometry::TVec3<f32> emitterPosition(
				    emitterBounds.x1 + emitterBounds.getWidth() * 0.5f,
				    emitterBounds.y1 + emitterBounds.getHeight() * 0.5f, 0.0f);
				gpEmitterManager4D2->createEmitter(emitterPosition, 0x1FD, nullptr,
				                               nullptr);
				unk2AC[i] = gpEmitterManager4D2->unkC8[0][0];
			}
		} else if (param_1 < 175.0f) {
			for (s32 i = 0; i < mGoPaneCount; ++i) {
				// The pane pointer is re-read at every use: the ROM reloads it
				// after the out-of-line JUTRect::copy call.
				s32 alpha = unk28[i]->getPane()->getAlpha() - 4;
				if (alpha < 0)
					alpha = 0;

				JUTRect globalBounds = unk28[i]->getPane()->getGlobalBounds();
				unk28[i]->getPane()->setAlpha(alpha);
				unk28[i]->getPane()->resize(globalBounds.getWidth() + 2,
				                            globalBounds.getHeight() + 2);

				if (unk2AC[i]) {
					JGeometry::TVec3<f32> emitterPosition(
					    globalBounds.x1 + globalBounds.getWidth() * 0.5f,
					    globalBounds.y1 + globalBounds.getHeight() * 0.5f, 0.0f);
					((JPABaseEmitter*)unk2AC[i])->setGlobalTranslation(
					    emitterPosition);
				}

				if (unk28[i]->update() && unk2AC[i]) {
					((JPABaseEmitter*)unk2AC[i])->setStatus(1);
					unk2AC[i] = nullptr;
				}

				if ((s32)param_1 % 2 == 0) {
					for (int j = 15; j > 0; --j)
						unk34[i * 22 + j] = unk34[i * 22 + j - 1];
					unk34[i * 22].set(globalBounds.x1, globalBounds.y1);
				}
			}
		} else if (param_1 == 175.0f) {
			for (s32 i = 0; i < mGoPaneCount; ++i) {
				JUTRect bounds = unk28[i]->getPane()->getBounds();
				unk28[i]->getPane()->resize(bounds.getWidth() - 0x50,
				                            bounds.getHeight() - 0x50);
			}
		} else {
			for (s32 i = 0; i < mGoPaneCount; ++i) {
				J2DPane* pane = unk28[i]->getPane();
				pane->hide();

				if (unk2AC[i]) {
					((JPABaseEmitter*)unk2AC[i])->setStatus(1);
					unk2AC[i] = nullptr;
				}
			}
			result = true;
		}
	}

	return result;
}

bool TConsoleStr::processShineGet(int param_1)
{
	bool result = true;

	for (s32 i = 0; i < mShinePaneCount; ++i) {
		if (param_1 == 6 * i) {
			unk244[i]->getPane()->show();
			unk244[i]->setPanePosition(0x28, cShineGetRight1, cShineGetLeft1,
			                           cShineGetLeft1);
			JUTRect local_74 = unk244[i]->getPane()->getBounds();
			JGeometry::TVec3<f32> local_80(
			    local_74.x1 + local_74.getWidth() * 0.5f,
			    local_74.y1 + local_74.getHeight() * 0.5f, 0.0f);
			gpEmitterManager4D2->createEmitter(local_80, 0x1FE, nullptr,
			                                   nullptr);
		}

		if (param_1 == i * 6 + 40) {
			unk244[i]->setPanePosition(0x14, cShineGetLeft2, cShineGetLeft2,
			                           cShineGetRight2);
		}

		if (param_1 == i * 6 + 60) {
			JUTRect local_94 = unk244[i]->getPane()->getBounds();
			JGeometry::TVec3<f32> local_80(
			    local_94.x1 + local_94.getWidth() * 0.5f,
			    local_94.y1 + local_94.getHeight() * 0.5f, 0.0f);
			gpEmitterManager4D2->createEmitter(local_80, 0x1FF, nullptr,
			                                   nullptr);
		}

		if (param_1 == i * 6 + 200) {
			unk244[i]->setPanePosition(0x28, cShineGetRight3, cShineGetRight3,
			                           cShineGetLeft3);
		}

		if (param_1 < i * 6 + 40) {
			u16 alpha = unk244[i]->getPane()->getAlpha() + 7;
			if (alpha > 0xff)
				alpha = 0xff;
			unk244[i]->getPane()->setAlpha(alpha);
		}

		if (param_1 > i * 6 + 200) {
			s16 alpha = unk244[i]->getPane()->getAlpha() - 7;
			if (alpha < 0)
				alpha = 0;
			unk244[i]->getPane()->setAlpha(alpha);
		}

		if (unk244[i]->update()) {
			if (param_1 > i * 6 + 280) {
				unk244[i]->getPane()->hide();
				result &= true;
			} else {
				result = 0;
			}
		} else {
			result = 0;
		}
	}

	return result;
}

bool TConsoleStr::processMiss(int param_1)
{
	// TODO: 88.5%. The instruction stream matches, but our frame is 0x198
	// against the ROM's 0x1B0 and the ROM keeps one more JUTPoint argument in
	// a stack temporary in the `i*10+60` branch (r7 = lwz 0x11c(r1)).
	bool result = true;

	for (s32 i = 0; i < mMissPaneCount; ++i) {
		if (param_1 == i * 10) {
			unk268[i]->getPane()->show();
			unk268[i]->setPanePosition(0x3C, JUTPoint(0, -270),
			                           JUTPoint(0, -270), JUTPoint(0, 30));
		}

		if (param_1 == i * 10 + 1) {
			JUTRect local_9c = unk268[i]->getPane()->getBounds();
			JGeometry::TVec3<f32> local_a8(
			    local_9c.x1 + local_9c.getWidth() * 0.5f,
			    local_9c.y1 + local_9c.getHeight() * 0.5f, 0.0f);
			gpEmitterManager4D2->createEmitter(local_a8, 0x1F9, nullptr,
			                                   nullptr);
		}

		if (param_1 == i * 10 + 60) {
			unk268[i]->getPane()->mRotation = (f32)mMissBaseRotation[i];
			unk268[i]->setPanePosition(0x28, JUTPoint(0, 30), JUTPoint(0, -80),
			                           JUTPoint(0, -80));
		}

		if (param_1 == i * 10 + 100) {
			unk268[i]->setPanePosition(0x28, JUTPoint(0, -80), JUTPoint(0, -80),
			                           JUTPoint(0, 0));
		}

		if (param_1 == i * 10 + 300) {
			unk268[i]->setPanePosition(0x1E, JUTPoint(0, 0), JUTPoint(0, 0),
			                           JUTPoint(0, 150));
		}

		if (param_1 < i * 10) {
			u16 alpha = unk268[i]->getPane()->getAlpha();
			alpha += 12;
			if (alpha > 0xff)
				alpha = 0xff;
			unk268[i]->getPane()->setAlpha(alpha);
		}

		if (unk268[i]->update()) {
			if (param_1 > i * 10 + 360) {
				unk268[i]->getPane()->hide();
				result &= true;
			} else {
				result = false;
			}
		} else {
			if (param_1 < i * 10) {
				unk268[i]->getPane()->mRotation =
				    mMissBaseRotation[i] + (i * 10 - param_1) * 6;
			}

			result = false;
		}
	}

	return result;
}
#pragma dont_inline off

bool TConsoleStr::processScenario(int)
{
	if (unk2BC > 0 && unk2BC != 7)
		return 0;

	bool uVar2 = true;
	uVar2 &= unk290[0]->update();
	uVar2 &= unk290[1]->update();

	unk1C += 2;

	if (unk1C > 355) {
		unk1C = 355;
		uVar2 &= true;
	} else {
		uVar2 = false;
	}

	int alpha = unk1C > 100 ? unk1C - 100 : 0;

	unk2A0[0]->setAlpha(alpha);
	unk2A0[1]->setAlpha(alpha);
	if (uVar2)
		unk2BC = 1;

	return false;
}

void TConsoleStr::startCloseWipe(bool param_1)
{
	// The two branches each own one JUTRect local: the first is filled by
	// JUTRect's copy constructor (`bl copy__7JUTRectFRC7JUTRect`) and the
	// second by the implicit word-wise operator=, which is why the ROM
	// stores the four fields inline the second time.
	//
	// TODO: 85.5%. The instruction stream is otherwise identical; our frame is
	// 0x170 against the ROM's 0x1E0. NOTE: a `char framePad_N[...]` here does
	// NOT help - measured at 112 and 212 bytes, both leave the score at 85.5%.
	// MWCC grows the frame *below* the named locals, whereas the ROM's deficit
	// is the 100-byte gap *between* the frame bottom and the locals, so a pad
	// cannot place it.

	if (param_1) {
		unk290[0]->getPane()->show();
		unk290[1]->getPane()->show();
		unk2A0[0]->hide();
		unk2A0[1]->hide();

		JUTRect rect = unk290[0]->getPane()->getBounds();
		unk290[0]->setPaneSize(0x2D, rect.x1 - rect.x2, -224,
		                       rect.x1 - rect.x2, 0);
		unk290[0]->setPaneAlpha(0x2D, 0xFF, 0);

		rect = unk290[1]->getPane()->getBounds();
		// TODO: the ROM keeps `465 - getInitialBounds().y1` un-reassociated and
		// re-evaluates it for the height below (`subfic 0x1d1` + `addi 0xe0`)
		// rather than folding to `241 - y1`.
		unk290[1]->setPaneOffset(0x2D, 0, 224 - rect.y1, 0,
		                         465 - unk290[1]->getInitialBounds().y1);
		unk290[1]->setPaneSize(0x2D, rect.x1 - rect.x2,
		                       465 - unk290[1]->getInitialBounds().y1 - 224,
		                       rect.x1 - rect.x2, 0);
		unk290[1]->setPaneAlpha(0x2D, 0xFF, 0);

		unk2BC = 8;
		unk2B8 = 4;
		unk2A8 = 1;
	} else if (unk2BC != 1) {
		// The ROM lays this branch out as `beq <body>` with this two-instruction
		// tail falling through, i.e. the guard is spelled inverted.
		unk2A8 = 1;
	} else {
		unk2BC    = 2;
		JUTRect rect = unk290[0]->getPane()->getBounds();
		unk290[0]->setPaneSize(0x2D, rect.x1 - rect.x2, -224,
		                       rect.x1 - rect.x2, rect.y1 - rect.y2);
		unk290[0]->setPaneAlpha(0x2D, 0xFF, unk290[0]->getPane()->getAlpha());

		rect = unk290[1]->getPane()->getBounds();
		unk290[1]->setPaneOffset(0x2D, 0, 224 - rect.y1, 0, 0);
		// TODO: the ROM computes this height as a *runtime* `224 - 464`
		// (subfic against a register holding 224) instead of folding it to the
		// constant -240.0f, so the original source must have reached the two
		// operands through something that stops MWCC folding them.
		unk290[1]->setPaneSize(0x2D, rect.x1 - rect.x2, 224 - 464,
		                       rect.x1 - rect.x2, rect.y1 - rect.y2);
		unk290[1]->setPaneAlpha(0x2D, 0xFF, unk290[1]->getPane()->getAlpha());
	}
}

void TConsoleStr::startOpenWipe()
{
	unk2A8 = 0;
	unk2BC = 5;
	unk290[0]->getPane()->hide();
	unk298[0]->getPane()->show();
	unk290[1]->getPane()->hide();
	unk298[1]->getPane()->show();

	// TODO: TExPane::setPaneAlpha is wrong

	JUTRect local_3c = unk298[0]->getPane()->getBounds();
	unk298[0]->setPaneOffset(0x1E, -local_3c.getWidth(), 0, 0, 0);
	unk298[0]->setPaneAlpha(30, 100, 255);

	local_3c = unk298[1]->getPane()->getBounds();
	unk298[1]->setPaneOffset(0x1E, local_3c.getWidth(), 0, 0, 0);
	unk298[1]->setPaneAlpha(30, 100, 255);
}
