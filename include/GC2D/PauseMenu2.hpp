#ifndef GC2D_PAUSE_MENU_2_HPP
#define GC2D_PAUSE_MENU_2_HPP

#include <JSystem/J2D/J2DScreen.hpp>
#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>

class J2DPicture;
class J2DSetScreen;
class J2DTextBox;
class J2DPane;
class TMarioGamePad;
class JPABaseEmitter;
class TCardSave;

class TPauseMenu2 : public JDrama::TViewObj {
public:
	TPauseMenu2(const char* = "<TPauseMenu>");
	void load(JSUMemoryInputStream&);
	void loadAfter();
	void appearWindow();
	void disappearWindow();
	void perform(u32 cue, JDrama::TGraphics* graphics);
	u8 getNextState();
	void setDrawStart();
	void setDrawEnd();
	void drawAppearPane(J2DPicture* picture, f32 anim, JUTRect& rect,
	                    f32 rotation);

	// fabricated; smells fake
	void setEmitterScale(f32 x, f32 y, f32 z)
	{
		mEmitter->setEmitterScale(JGeometry::TVec3<f32>(x, y, z));
	}

	// fabricated
	inline void draw(JDrama::TGraphics* gfx);

public:
	enum PauseMenuState {
		MENU_APPEARING    = 0,
		MENU_OPEN         = 1,
		UNK2              = 2,
		MENU_SAVING       = 3,
		MENU_DISAPPEARING = 4,
		MENU_CLOSED       = 5
	};

	// Menu state.
	/* 0x10 */ PauseMenuState mState;

	// Screen and menu/background panes.
	/* 0x14 */ J2DSetScreen* mScreen;
	/* 0x18 */ J2DPane* mBackground;
	/* 0x1C */ J2DPane* mMenuPane;

	// "Pause" letters.
	/* 0x20 */ J2DPicture* mPauseLetters[5];
	/* 0x34 */ u32 unk34;
	/* 0x38 */ JUTRect mOrigLetterBounds[5];
	/* 0x88 */ f32 mOrigLetterAngles[5];

	// Menu items.
	/* 0x9C */ J2DPicture* mMenuItems[3];
	/* 0xA8 */ JUTRect mOrigItemBounds[3];

	// Stage and scenario name.
	/* 0xD8 */ J2DTextBox* mStageName;
	/* 0xDC */ J2DPane* mStagePane;
	/* 0xE0 */ J2DTextBox* mScenarioName;

	// Menu option items.
	/* 0xE4 */ u8 mSelectedItem;
	/* 0xE8 */ u32 mItemColor;
	/* 0xEC */ f32 mBounceAnim;

	// Appear/disappear animation.
	/* 0xF0 */ f32 mFadeAnim;
	/* 0xF4 */ u8 mBackgroundAlpha;
	/* 0xF8 */ f32 mBackgroundFadeInSpeed;
	/* 0xFC */ s16 mFirstItemAngle;
	/* 0x100 */ u32 unk100; // unused?

	// "Spark" effect for the bouncing animation.
	/* 0x104 */ s16 mEffectKeyFrame;
	/* 0x106 */ u16 mEffectStretch;

	/* 0x108 */ s32 mNumItems;
	/* 0x10C */ bool mPressedB;
	/* 0x10D */ bool mSelectionConfirmed;
	/* 0x110 */ TMarioGamePad* mGamePad;
	/* 0x114 */ JPABaseEmitter* mEmitter;
	/* 0x118 */ u32 unk118; // unused?
	/* 0x11C */ TCardSave* mCardSave;
};

#endif
