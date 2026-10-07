#ifndef SYSTEM_GAME_SEQUENCE_HPP
#define SYSTEM_GAME_SEQUENCE_HPP

#include <JSystem/JDrama/JDRFlag.hpp>
#include <dolphin/types.h>

class TGameSequence {
public:
	// The defaulted arguments are the level that puts the flag member's
	// converting constructor out of line where decideNextStage is expanded
	// into TMarDirector::changeState/updateGameMode. Upstream's defaulted
	// flag parameter (instead of a TFlagT<u16>(0) argument built here) gives
	// TApplication::TApplication its 0x40 frame (99.6 -> 100) and
	// decideNextStage 99.65 -> 99.88, with nothing else moving.
	TGameSequence(u8 stage = 0, u8 scenario = 0, JDrama::TFlagT<u16> flag = 0)
	{
		set(stage, scenario, flag);
	}

	void set(u8 param_1, u8 param_2)
	{
		set(param_1, param_2, JDrama::TFlagT<u16>(0));
	}

	TGameSequence& operator=(const TGameSequence& other)
	{
		set(other.unk0, other.unk1, other.unk2);
		return *this;
	}

	void set(u8 param_1, u8 param_2, JDrama::TFlagT<u16> param_3)
	{
		unk0 = param_1;
		unk1 = param_2;
		unk2 = param_3;
	}

	u8 getStage() const { return unk0; }
	u8 getScenario() const { return unk1; }

public:
	/* 0x0 */ u8 unk0;
	/* 0x1 */ u8 unk1;
	/* 0x2 */ JDrama::TFlagT<u16> unk2;
};

#endif
