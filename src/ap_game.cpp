#include "port_ap.h"
#include <GC2D/GCConsole2.hpp>
#include <GC2D/ShineTable.hpp>
#include <GC2D/SelectMenu.hpp>
#include <MoveBG/Item.hpp>
#include <JSystem/JKernel/JKRExpHeap.hpp>
#include <Player/Mario.hpp>
#include <Player/WaterGun.hpp>
#include <System/Application.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/MarioGamePad.hpp>
#include <System/StageUtil.hpp>
#include <dolphin/pad.h>
#include "../platform/ap/consumables.hpp"
#include "../platform/ap/blue_coins.hpp"
#include <time.h>
// Tutorial completion is a physical check, not the shuffled reward quantity.
bool SMSAPShouldReplayAirstrip(const TFlagManager *flags) {
  return port_ap_enabled() ? (port_ap_starting_nozzle() != 2 && !flags->getShineFlag(0x56))
                           : flags->getFlag(0x40000) < 1;
}
static int APReadFlag(unsigned int flag) {
  return TFlagManager::getInstance()->getFlag(flag);
}
// Ticket mode keeps every plaza entrance/object in the post-Pinna layout.
// The original client returns to the drained plaza when its Corona flag opens.
int SMSAPNextPlazaScenario() {
  TFlagManager *flags = TFlagManager::getInstance();
  if (flags->getBool(0x103AE)) return 2;
  if (port_ap_ticket_mode()) return 8;
  // Native vanilla unlock events keep their normal statue/entrance sequence.
  if (flags->getBool(0x10389)) return 8;
  if (flags->getBool(0x10386) && flags->getBool(0x10387))
    return flags->getFlag(0x40000) >= 10 ? 7 : 6;
  if (flags->getBool(0x10385)) return 5;
  if (flags->getBool(0x10384) || port_ap_starting_nozzle() == 2) return 1;
  return 0;
}
static int APReconcileOneUps(TFlagManager *flags, int accepted, int *applied) {
  if (accepted < 0 || !applied)
    return -1;
  int delta = smsap::one_up_delta((unsigned)accepted, flags->mAPOneUps);
  if (delta < 0)
    return -1;
  if (delta)
    flags->incMario(delta); // Native method includes the 99-life cap.
  flags->mAPOneUps = (unsigned)accepted;
  *applied = accepted;
  return flags->getFlag(0x20001);
}
static int SMSAPReconcileOneUps(int accepted, int *applied) {
  return APReconcileOneUps(TFlagManager::getInstance(), accepted, applied);
}
void ApplicationAPFrame(TApplication *app, bool restoring) {
  if (!port_ap_enabled())
    return;
  TFlagManager *flags = TFlagManager::getInstance();
  if (!flags)
    return;
  TMarDirector *dir =
      app->mAppState == TApplication::APP_STATE_GAMEPLAY ? gpMarDirector : 0;
  bool playing = !restoring && dir && gpMarioOriginal && !gpMarioOriginal->checkFlag(MARIO_FLAG_GAME_OVER) &&
                 dir->mState == TMarDirector::STATE_UNK4 &&
                 !dir->checkUnk4CFlag(
                     TMarDirector::DIRECTOR_FLAG_STAGE_TRANSITION_PENDING);
  port_ap_observe_position(dir ? (app->mCurrArea.getStage() == 1 ? -1 : app->mCurrArea.getScenario()) : -1);
  port_ap_observe_death(!restoring && dir && (dir->mState == TMarDirector::STATE_UNK7 ||
                       (gpMarioOriginal && gpMarioOriginal->checkFlag(MARIO_FLAG_GAME_OVER))), playing);
  TWaterGun *observedGun = playing ? gpMarioOriginal->mWaterGun : 0;
  port_ap_observe_game(observedGun ? observedGun->mCurrentNozzle : -1,
                       observedGun ? observedGun->mSecondNozzle : -1,
                       playing &&
                           gpMarioOriginal->checkFlag(MARIO_FLAG_HAS_FLUDD));
  // MovieDirector sets the physical Bowser flag during ending movie 14,
  // after leaving normal gameplay. Observation must survive that transition.
  port_ap_observe_goal(!restoring && flags->mAPPersisted &&
                      flags->getShineFlag(0x77));
  port_ap_tick(flags->mAPGuid, flags->mAPPersisted, playing, app->mSaveFile,
               app->mCurrArea.getStage(), APReadFlag, SMSAPReconcileOneUps);
  if (!playing)
    return;
  if (port_ap_take_linked_death()) {
    // The native director consumes this same signal from TMario::loserExec,
    // runs the normal game-over/life/save transition, then respawns Mario.
    gpMarioOriginal->mHealth = 0;
    gpMarioOriginal->onFlag(MARIO_FLAG_GAME_OVER);
    return;
  }
  if (!port_ap_stage_allowed(app->mCurrArea.getStage())) {
    dir->setNextStage(1, 0); // Plaza with scenario selected by ticket/Corona state.
    return;
  }
  flags->setFlag(0x40000, port_ap_shines());
  if (TGCConsole2 *console = dir->getConsole()) {
    console->unk24 = port_ap_shines();
    console->unk64 = port_ap_shines();
  }
  if (TWaterGun *gun = gpMarioOriginal->mWaterGun) {
    if (port_ap_fludd_allowed()) gpMarioOriginal->onFlag(MARIO_FLAG_HAS_FLUDD);
    else gpMarioOriginal->offFlag(MARIO_FLAG_HAS_FLUDD);
    if (!port_ap_nozzle_allowed(gun->mSecondNozzle) ||
        gun->mSecondNozzle == TWaterGun::Spray)
      gun->mSecondNozzle = port_ap_preferred_nozzle();
    if (!port_ap_nozzle_allowed(gun->mCurrentNozzle))
      gun->changeNozzle((TWaterGun::TNozzleType)port_ap_preferred_nozzle(), false);
  }
}
// Called after fresh pad read/updateMeaning, before director simulation ticks.
// Later ticks reset the one-frame trigger; the top safe point cannot consume
// it.
static void APHandleNozzleInput(TWaterGun *gun, TMarioGamePad *pad) {
  const u32 trigger = pad->getTrigger();
  const u32 shortcut =
      trigger & (PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN);
  int request = -1;
  if (shortcut == PAD_BUTTON_LEFT)
    request = TWaterGun::Hover;
  else if (shortcut == PAD_BUTTON_RIGHT)
    request = TWaterGun::Turbo;
  else if (shortcut == PAD_BUTTON_DOWN)
    request = TWaterGun::Rocket;
  const int before = gun->mCurrentNozzle;
  const bool allowed = request >= 0 && port_ap_nozzle_allowed(request);
  if (request >= 0)
    gun->changeNozzle((TWaterGun::TNozzleType)request, true);
  port_ap_observe_input(pad->getButton(), trigger, request, before,
                        gun->mCurrentNozzle, gun->mSecondNozzle, allowed);
}
void ApplicationAPInput(TApplication *app, bool restoring) {
  if (!port_ap_enabled() || restoring ||
      app->mAppState != TApplication::APP_STATE_GAMEPLAY)
    return;
  TFlagManager *flags = TFlagManager::getInstance();
  TMarDirector *dir = gpMarDirector;
  if (!flags || !flags->mAPPersisted || app->mSaveFile != 0 || !dir ||
      !gpMarioOriginal || dir->mState != TMarDirector::STATE_UNK4 ||
      dir->checkUnk4CFlag(TMarDirector::DIRECTOR_FLAG_STAGE_TRANSITION_PENDING))
    return;
  TWaterGun *gun = gpMarioOriginal->mWaterGun;
  TMarioGamePad *pad = gpMarioOriginal->getGamePad();
  if (gun && pad && gpMarioOriginal->checkFlag(MARIO_FLAG_HAS_FLUDD))
    APHandleNozzleInput(gun, pad);
}
// Mechanism fixture below the UI/card write boundary. Calls the shipped
// methods; it cannot certify menu selection, card completion or reachable game
// checks.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static TFlagManager *sAPSaveFixtureFlags;
static int APFixtureReadFlag(unsigned int flag) {
  return sAPSaveFixtureFlags->getFlag(flag);
}
int SMSAPNativeBlueSelftest() {
  const char *directory = getenv("SMS_AP_BRIDGE");
  const char *modeText = getenv("SMS_AP_SELFTEST_BLUE_MODE");
  const char *tradesText = getenv("SMS_AP_SELFTEST_TRADES");
  const int mode = port_ap_enabled() ? (modeText ? atoi(modeText) : 1) : 0;
  const int trades = tradesText ? atoi(tradesText) : 3;
  const int receipts = trades ? (trades * 10 > 40 ? trades * 10 : 40) : 0;
  const int currency = mode ? trades * 10 : 240;
  const bool coins = getenv("SMS_AP_SELFTEST_COIN_SHINES") != nullptr;
  if (mode < 0 || mode > 2 || (port_ap_enabled() && mode == 0)) return 2;
  if (trades < 0 || trades > 24) return 2;
  char path[2048];
  if (mode) {
    snprintf(path, sizeof(path), "%s/session.txt", directory);
    if (FILE *existing = fopen(path, "rb")) {
      fclose(existing);
      fprintf(stderr, "blue selftest requires a fresh fixture directory\n");
      return 2;
    }
    FILE *session = fopen(path, "wb");
    if (!session) return 3;
    fprintf(session, "SMSAP2\n616263 0 1 3 0 1 %d %d %d\n%d\n",
            mode, mode == 1 ? receipts : 0, trades, (mode == 1 ? 240 : 0) + trades + (coins ? 8 : 0));
    for (unsigned bit = 0; bit < 948; ++bit)
      if ((mode == 1 && smsap::blue_coin_bit(bit)) ||
          (smsap::trade_index(bit) >= 0 && smsap::trade_index(bit) < trades) ||
          (coins && bit >= 100 && bit <= 107))
        fprintf(session, "%u %u\n", bit + 1, bit);
    if (fclose(session)) return 4;
  }
  TFlagManager first;
  TFlagManager::smInstance = &first;
  sAPSaveFixtureFlags = &first;
  unsigned char initial[0x800] = {0};
  JSUMemoryOutputStream initialOut(initial, sizeof(initial));
  first.save(initialOut);
  first.saveSuccess();
  if (mode) port_ap_tick(first.mAPGuid, true, false, 0, 1, APFixtureReadFlag);
  if (port_ap_shuffle_blue_coins() != (mode == 1)) return 5;
  // Every real blue pickup uses the shipped stage->card-bool mapping.
  unsigned count = 0;
  for (unsigned bit = 120; bit < 570; ++bit) {
    if (!smsap::blue_coin_bit(bit)) continue;
    unsigned stage = (bit - 120) / 50 + 1;
    // Area 7 is Sirena's casino subarea (same Shine stage as area 6).
    unsigned area = stage >= 7 ? stage + 1 : stage;
    unsigned coin = (bit - 120) % 50;
    first.setBlueCoinFlag(area, coin);
    first.setBlueCoinFlag(area, coin); // Replayed pickup is idempotent.
    if (!first.getBlueCoinFlag(area, coin) || !first.getBool(0x10000 + bit))
      return 6;
    ++count;
  }
  if (count != 240 || first.getFlag(0x40001) != (mode == 1 ? 0 : currency))
    return 7;
  for (unsigned bit = 120; bit < 920; ++bit)
    if (!smsap::blue_coin_bit(bit) && first.getBool(0x10000 + bit)) return 8;
  if (mode == 1) {
    snprintf(path, sizeof(path), "%s/history.txt", directory);
    FILE *history = fopen(path, "wb");
    if (!history) return 9;
    fputs("SMSAPH1\n616263 0 1 ", history);
    for (int i = 0; i < 16; ++i) fprintf(history, "%02x", first.mAPGuid[i]);
    fprintf(history, "\n%d\n", receipts);
    for (int i = 0; i < receipts; ++i) fprintf(history, "%d 523014 %d 2 1\n", i, i + 1);
    if (fclose(history)) return 10;
    port_ap_tick(first.mAPGuid, true, true, 0, 1, APFixtureReadFlag);
    if (first.getFlag(0x40001) != currency || port_ap_shines()) return 11;
  }
  // Retail subtracts ten for each physical trade flag from the total counter.
  // The bridge must expose a total, not subtract the spent amount a second time.
  for (unsigned i = 0; i < unsigned(mode ? trades : 3); ++i) {
    first.setShineFlag(i < 16 ? 70 + i : 108 + i - 16);
    if (mode && trades) {
      int spent = 0;
      for (unsigned trade = 0; trade < 24; ++trade)
        if (first.getShineFlag(trade < 16 ? 70 + trade : 108 + trade - 16)) ++spent;
      if (spent != int(i + 1) || first.getFlag(0x40001) - 10 * spent != currency - 10 * int(i + 1))
        return 18; // Retail's remaining-currency calculation after each trade.
    }
  }
  if (mode && trades < 24) {
    unsigned denied = trades < 16 ? 70 + trades : 108 + trades - 16;
    first.setShineFlag(denied);
    first.setBool(true, 0x10000 + denied); // Generic scripts cannot bypass cap.
    if (first.getShineFlag(denied)) return 12;
  }
  if (first.getShineFlag(72) != (!mode || trades >= 3)) return 12;
  if (coins) {
    for (unsigned bit = 100; bit <= 107; ++bit) {
      first.setShineFlag(bit);
      first.setShineFlag(bit);
      if (!first.getShineFlag(bit)) return 19;
    }
    if (first.getFlag(0x40000) != 0 && mode) return 20;
  }
  first.correctFlag();
  if (first.getFlag(0x40001) != currency) return 13;
  unsigned char save[0x800] = {0};
  JSUMemoryOutputStream out(save, sizeof(save));
  first.save(out);
  first.saveSuccess();
  TFlagManager loaded;
  JSUMemoryInputStream in(save, sizeof(save));
  loaded.load(in);
  if (mode == 1) port_ap_tick(loaded.mAPGuid, true, false, 0, 1, APFixtureReadFlag);
  for (unsigned bit = 120; bit < 550; ++bit)
    if (smsap::blue_coin_bit(bit) && !loaded.getBool(0x10000 + bit)) return 14;
  if (loaded.getShineFlag(72) != (!mode || trades >= 3) || loaded.getFlag(0x40001) != currency) return 15;
  if (coins) for (unsigned bit = 100; bit <= 107; ++bit)
    if (!loaded.getShineFlag(bit)) return 21;
  loaded.setBool(false, 0x10078);
  loaded.restore();
  if (!loaded.getBlueCoinFlag(1, 0) || loaded.getShineFlag(72) != (!mode || trades >= 3)) return 16;
  if (mode == 1) port_ap_tick(loaded.mAPGuid, true, true, 0, 1, APFixtureReadFlag);
  if (loaded.getFlag(0x40001) != currency) return 17;
  fprintf(stderr, "SMS native blue methods passed: mode=%d trades=%d, all 240 pickup flags, "
          "duplicate pickup, currency authority, trade cap, card save/load/restore\n", mode, trades);
  return 0;
}
static int APFixtureOption(const char *name, int fallback) {
  const char *s = getenv(name);
  return s ? atoi(s) : fallback;
}
int SMSAPNativeOptionsSelftest() {
  const char *directory = getenv("SMS_AP_BRIDGE");
  if (!directory || !port_ap_enabled()) return 2;
  if (!JKRGetRootHeap()) JKRExpHeap::createRoot(1, false);
  if (!JKRGetRootHeap()) return 22;
  JKRGetRootHeap()->becomeCurrentHeap();
  int level = APFixtureOption("SMS_AP_SELFTEST_LEVEL", 1), start = APFixtureOption("SMS_AP_SELFTEST_START", 0);
  int blue = APFixtureOption("SMS_AP_SELFTEST_BLUE_MODE", 0), boxes = APFixtureOption("SMS_AP_SELFTEST_BOXES", 1);
  int episodes = APFixtureOption("SMS_AP_SELFTEST_EPISODES", 1);
  const int trades = blue ? 3 : 0;
  char path[2048];
  snprintf(path, sizeof(path), "%s/session.txt", directory);
  if (FILE *existing = fopen(path, "rb")) { fclose(existing); return 2; }
  FILE *session = fopen(path, "wb");
  if (!session) return 3;
  fprintf(session, "SMSAP3\n616263 0 1 3 %d %d %d 40 %d %d %d 1\n%d\n",
          start, level, blue, trades, episodes, boxes, 8 + (boxes ? 10 : 0) + (blue == 1 ? 240 : 0) + trades);
  for (unsigned bit = 0; bit < 948; ++bit)
    if ((bit >= 100 && bit <= 107) || (boxes && smsap::nozzle_box_bit(bit)) ||
        (blue == 1 && smsap::blue_coin_bit(bit)) ||
        (blue && smsap::trade_index(bit) >= 0 && smsap::trade_index(bit) < trades))
      fprintf(session, "%u %u\n", bit + 1, bit);
  if (fclose(session)) return 4;
  TFlagManager flags;
  TFlagManager::smInstance = &flags;
  sAPSaveFixtureFlags = &flags;
  unsigned char initial[0x800] = {};
  JSUMemoryOutputStream initialOut(initial, sizeof(initial));
  flags.save(initialOut);
  flags.saveSuccess();
  port_ap_tick(flags.mAPGuid, true, false, 0, 1, APFixtureReadFlag);
  if (port_ap_ticket_mode() != level || port_ap_starting_nozzle() != start ||
      port_ap_nozzle_allowed(TWaterGun::Spray) != (start == 0) ||
      port_ap_nozzle_allowed(TWaterGun::Hover) != (start == 1) ||
      port_ap_nozzle_allowed(TWaterGun::Rocket) ||
      SMSAPShouldReplayAirstrip(&flags) != (start != 2)) return 5;
  flags.setBool(true, 0x1038B);
  if (flags.getBool(0x1038B) != (level == 0)) return 6;
  if (start == 2 && !level && !flags.getBool(0x10385)) return 7;
  if (port_ap_episode_count(1) != (episodes ? 8 : 1) || SMS_isGetShine(2, 0, false)) return 8;
  TSelectMenu menu("AP fixture");
  menu.mNumUnlockedShines = port_ap_episode_count(1);
  for (unsigned i = 0; i < 8; ++i) menu.mShineUnlockStates[i] = i < menu.mNumUnlockedShines ? 2 : 0;
  menu.mSelectedShine = 0;
  if (menu.getNextIndex() != (episodes ? 1 : -1)) return 9;
  TMario mario;
  mario.mFlag = 0;
  mario.mHealth = 8;
  gpMarioOriginal = &mario;
  TWaterGun gun(&mario);
  gun.mNozzleList[TWaterGun::Spray] = &gun.mNozzleDeform;
  gun.mNozzleList[TWaterGun::Rocket] = &gun.mNozzleRocket;
  gun.mNozzleList[TWaterGun::Hover] = &gun.mNozzleHover;
  gun.mNozzleList[TWaterGun::Turbo] = &gun.mNozzleTurbo;
  gun.mCurrentNozzle = TWaterGun::Spray;
  gun.mSecondNozzle = TWaterGun::Spray;
  gun.mCurrentWater = gun.mNozzleDeform.mEmitParams.mAmountMax.get();
  mario.mWaterGun = &gun;
  // Fixture exits the process; normal director teardown requires stage audio.
  TMarDirector &director = *new TMarDirector();
  director.mConsole = nullptr;
  director.unk4C = 0;
  director.mState = TMarDirector::STATE_UNK4;
  gpApplication.mAppState = TApplication::APP_STATE_GAMEPLAY;
  gpApplication.mSaveFile = 0;
  gpApplication.mCurrArea.set(1, 0, 0);
  port_ap_observe_fludd_stage(false);
  ApplicationAPFrame(&gpApplication, false);
  if (mario.checkFlag(MARIO_FLAG_HAS_FLUDD) != (start != 2) ||
      (start == 1 && gun.mCurrentNozzle != TWaterGun::Hover)) return 10;
  // The production director pickup callback, not a substitute flag array.
  TItemNozzle item;
  const unsigned areas[] = {2, 2, 3, 3, 4, 4, 8, 9, 9, 10};
  const unsigned types[] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0};
  const unsigned bits[] = {872, 873, 874, 875, 876, 877, 882, 884, 885, 886};
  for (unsigned i = 0; i < 10; ++i) {
    gpApplication.mCurrArea.set(areas[i], 0, 0);
    item.mActorType = types[i] ? 0x2000002A : 0x20000022;
    director.fireGetNozzle(&item);
    director.fireGetNozzle(&item);
    if (!flags.getBool(0x10000 + bits[i])) return 11;
  }
  if (port_ap_nozzle_allowed(TWaterGun::Rocket) || port_ap_nozzle_allowed(TWaterGun::Turbo)) return 12;
  for (unsigned bit = 100; bit <= 107; ++bit) flags.setShineFlag(bit);
  if (port_ap_shines() || SMS_isGetShine(2, 0, false)) return 13;
  snprintf(path, sizeof(path), "%s/history.txt", directory);
  FILE *history = fopen(path, "wb");
  if (!history) return 14;
  fputs("SMSAPH1\n616263 0 1 ", history);
  for (int i = 0; i < 16; ++i) fprintf(history, "%02x", flags.mAPGuid[i]);
  const int total = 4 + 50 + (level ? 7 : 0) + (blue == 1 ? 40 : 0);
  fprintf(history, "\n%d\n", total);
  int index = 0;
  for (int i = 0; i < 4; ++i, ++index) fprintf(history, "%d %d 1 2 1\n", index, 523000 + i);
  for (int i = 0; i < 50; ++i, ++index) fprintf(history, "%d 523004 1 2 1\n", index);
  if (level) for (int i = 0; i < 7; ++i, ++index) fprintf(history, "%d %d 1 2 1\n", index, 523005 + i);
  if (blue == 1) for (int i = 0; i < 40; ++i, ++index) fprintf(history, "%d 523014 1 2 1\n", index);
  if (fclose(history)) return 15;
  port_ap_tick(flags.mAPGuid, true, false, 0, 1, APFixtureReadFlag);
  gpApplication.mCurrArea.set(1, 0, 0);
  ApplicationAPFrame(&gpApplication, false);
  if (!mario.checkFlag(MARIO_FLAG_HAS_FLUDD) || port_ap_shines() != 50 ||
      !port_ap_nozzle_allowed(TWaterGun::Rocket) || !port_ap_stage_allowed(10) ||
      !flags.getBool(0x103AE)) return 16;
  // Stage-script FLUDD removal remains authoritative even with all AP nozzles.
  port_ap_observe_fludd_stage(true);
  ApplicationAPFrame(&gpApplication, false);
  if (mario.checkFlag(MARIO_FLAG_HAS_FLUDD)) return 17;
  port_ap_observe_fludd_stage(false);
  unsigned char card[0x800] = {};
  JSUMemoryOutputStream out(card, sizeof(card));
  flags.save(out); flags.saveSuccess();
  TFlagManager loaded;
  JSUMemoryInputStream in(card, sizeof(card));
  loaded.load(in);
  for (unsigned bit = 100; bit <= 107; ++bit) if (!loaded.getShineFlag(bit)) return 18;
  for (unsigned i = 0; i < 10; ++i) if (!loaded.getBool(0x10000 + bits[i])) return 19;
  // A matching transient command must reach the actual game-thread actor effect.
  TFlagManager::smInstance = &loaded;
  sAPSaveFixtureFlags = &loaded;
  snprintf(path, sizeof(path), "%s/snapshot.json", directory);
  FILE *snapshot = fopen(path, "rb");
  if (!snapshot) return 23;
  char contents[4096] = {};
  fread(contents, 1, sizeof(contents) - 1, snapshot); fclose(snapshot);
  const char *bootStart = strstr(contents, "\"boot_id\":\"");
  if (!bootStart) return 24;
  char boot[33] = {};
  memcpy(boot, bootStart + strlen("\"boot_id\":\""), 32);
  snprintf(path, sizeof(path), "%s/death-link.txt", directory);
  FILE *linked = fopen(path, "wb");
  if (!linked) return 25;
  fputs("SMSAPD1\n616263 0 1 ", linked);
  for (int i = 0; i < 16; ++i) fprintf(linked, "%02x", loaded.mAPGuid[i]);
  fprintf(linked, " %s 1 %llu\n", boot, (unsigned long long)time(0) * 1000);
  fclose(linked);
  port_ap_tick(loaded.mAPGuid, true, false, 0, 1, APFixtureReadFlag);
  mario.mFlag = 0; mario.mHealth = 8;
  ApplicationAPFrame(&gpApplication, false);
  if (mario.mHealth != 0 || !mario.checkFlag(MARIO_FLAG_GAME_OVER)) return 26;
  ApplicationAPFrame(&gpApplication, false); // Observe the remote death without echo.
  mario.mFlag = 0; mario.mHealth = 8;
  ApplicationAPFrame(&gpApplication, false); // Fixture respawn; no command replay.
  if (mario.mHealth != 8 || mario.checkFlag(MARIO_FLAG_GAME_OVER)) return 27;
  mario.onFlag(MARIO_FLAG_GAME_OVER);
  ApplicationAPFrame(&gpApplication, false); // A distinct local death is reportable.
  fprintf(stderr, "SMS native options callbacks passed: level=%d start=%d blue=%d boxes=%d episodes=%d; actor FLUDD, real director nozzle callbacks, native select navigation, card save/load\n",
          level, start, blue, boxes, episodes);
  return 0;
}
int SMSAPNativeSaveSelftest() {
  if (!port_ap_enabled()) {
    fprintf(stderr, "native save selftest requires isolated SMS_AP_BRIDGE\n");
    return 2;
  }
  const char *fixtureDirectory = getenv("SMS_AP_BRIDGE");
  char fixturePath[2048];
  snprintf(fixturePath, sizeof(fixturePath), "%s/session.txt",
           fixtureDirectory);
  if (FILE *existing = fopen(fixturePath, "rb")) {
    fclose(existing);
    fprintf(stderr, "selftest refuses an existing session; use a fresh "
                    "isolated bridge directory\n");
    return 2;
  }
  TFlagManager first;
  unsigned char original[16];
  memcpy(original, first.mAPGuid, 16);
  if (first.mAPPersisted || !SMSAPShouldReplayAirstrip(&first))
    return 3;
  first.setShineFlag(86);
  if (!first.getShineFlag(86) || first.getFlag(0x40000) != 0 ||
      SMSAPShouldReplayAirstrip(&first))
    return 4;
  first.correctFlag();
  if (first.getFlag(0x40000) != 0)
    return 5;
  unsigned char save[0x800];
  memset(save, 0, sizeof(save));
  JSUMemoryOutputStream out(save, sizeof(save));
  first.save(out);
  if (first.mAPPersisted || !first.mAPSavePending)
    return 6;
  first.saveSuccess();
  if (!first.mAPPersisted)
    return 7;
  TFlagManager second;
  if (!memcmp(original, second.mAPGuid, 16) || second.mAPPersisted)
    return 8;
  JSUMemoryInputStream in(save, sizeof(save));
  second.load(in);
  if (memcmp(original, second.mAPGuid, 16) || !second.mAPPersisted ||
      !second.getShineFlag(86) || SMSAPShouldReplayAirstrip(&second))
    return 9;
  second.firstStart();
  if (!memcmp(original, second.mAPGuid, 16) || second.mAPPersisted ||
      second.getShineFlag(86))
    return 10;
  JSUMemoryInputStream restored(save, sizeof(save));
  second.load(restored);
  if (memcmp(original, second.mAPGuid, 16) || !second.mAPPersisted ||
      !second.getShineFlag(86))
    return 11;
  second.setShineFlag(117);
  second.restore();
  if (second.getShineFlag(117) || !second.getShineFlag(86) ||
      memcmp(original, second.mAPGuid, 16))
    return 12;
  // Exercise the shipped TWaterGun mutation with default parameter objects.
  // No model, Mario actor, nozzle box or movement path is constructed.
  TWaterGun gun(0);
  gun.mNozzleList[TWaterGun::Spray] = &gun.mNozzleDeform;
  gun.mNozzleList[TWaterGun::Rocket] = &gun.mNozzleRocket;
  gun.mNozzleList[TWaterGun::Hover] = &gun.mNozzleHover;
  gun.mNozzleList[TWaterGun::Turbo] = &gun.mNozzleTurbo;
  gun.mCurrentNozzle = TWaterGun::Spray;
  gun.mSecondNozzle = TWaterGun::Spray;
  gun.mCurrentWater = gun.mNozzleDeform.mEmitParams.mAmountMax.get();
  gun.changeNozzle(TWaterGun::Rocket, true);
  if (gun.mCurrentNozzle != TWaterGun::Spray)
    return 13;
  const char *directory = getenv("SMS_AP_BRIDGE");
  char path[2048];
  snprintf(path, sizeof(path), "%s/session.txt", directory);
  FILE *session = fopen(path, "wb");
  if (!session)
    return 14;
  fputs("SMSAP1\n616263 0 1 3 0 1\n1\n1 86\n", session);
  if (fclose(session))
    return 15;
  sAPSaveFixtureFlags = &second;
  port_ap_tick(second.mAPGuid, true, false, 0, 1, APFixtureReadFlag);
  snprintf(path, sizeof(path), "%s/history.txt", directory);
  FILE *history = fopen(path, "wb");
  if (!history)
    return 16;
  fputs("SMSAPH1\n616263 0 1 ", history);
  for (int i = 0; i < 16; ++i)
    fprintf(history, "%02x", second.mAPGuid[i]);
  fputs(
      "\n4\n0 523004 1 2 1\n1 523004 4 2 1\n2 523002 5 2 1\n3 523005 -2 1 1\n",
      history);
  if (fclose(history))
    return 17;
  port_ap_tick(second.mAPGuid, true, true, 0, 1, APFixtureReadFlag);
  if (second.getFlag(0x40000) != 2 || !second.getBool(0x10385) ||
      second.getShineFlag(0))
    return 18;
  second.correctFlag();
  if (second.getFlag(0x40000) != 2)
    return 19;
  gun.changeNozzle(TWaterGun::Rocket, true);
  if (gun.mCurrentNozzle != TWaterGun::Rocket ||
      gun.mSecondNozzle != TWaterGun::Rocket)
    return 20;
  gun.changeNozzle(TWaterGun::Hover, true);
  if (gun.mCurrentNozzle != TWaterGun::Rocket)
    return 21;
  // Reproduce the production ordering with actual native pad methods.
  // At 60/120 fps, later director simulation ticks call resetButtons().
  // The old top-of-next-frame input site therefore misses the press entirely.
  TMarioGamePad pad(JUTGamePad::EPort1);
  PADStatus released = {0};
  PADStatus down = {0};
  down.button = PAD_BUTTON_DOWN;
  for (int ticks = 1; ticks <= 4; ticks *= 2) {
    gun.changeNozzle(TWaterGun::Spray, true);
    pad.mButton.update(&released, 0);
    pad.mButton.update(&down, 0);
    pad.updateMeaning();
    for (int i = 1; i < ticks; ++i) {
      pad.resetButtons();
      pad.updateMeaning();
    }
    APHandleNozzleInput(&gun,
                        &pad); // Old hook site: after simulation finished.
    const bool oldEquipped = gun.mCurrentNozzle == TWaterGun::Rocket;
    if (oldEquipped != (ticks == 1))
      return 22;
    gun.changeNozzle(TWaterGun::Spray, true);
    pad.mButton.update(&released, 0);
    pad.mButton.update(&down, 0);
    pad.updateMeaning();
    APHandleNozzleInput(&gun, &pad); // New site: freshly sampled, before reset.
    for (int i = 1; i < ticks; ++i) {
      pad.resetButtons();
      pad.updateMeaning();
    }
    if (gun.mCurrentNozzle != TWaterGun::Rocket ||
        gun.mSecondNozzle != TWaterGun::Rocket)
      return 23;
  }
  PADStatus left = {0};
  left.button = PAD_BUTTON_LEFT;
  pad.mButton.update(&released, 0);
  pad.mButton.update(&left, 0);
  pad.updateMeaning();
  APHandleNozzleInput(&gun, &pad);
  if (gun.mCurrentNozzle != TWaterGun::Rocket)
    return 24;
  // Shared native FlagManager and the same physical predicate used by
  // SelectMenu: tickets/received quantity never fabricate episode completion.
  TFlagManager::smInstance = &second;
  if (SMSAPNextPlazaScenario() != 8 || port_ap_yoshi() ||
      second.getBool(0x103AB) || second.getBool(0x1038B) ||
      second.getBool(0x1038C) || SMS_isGetShine(2, 0, false))
    return 25;
  second.setBool(true, 0x1038B);
  second.setBool(true, 0x1038C);
  if (second.getBool(0x1038B) || second.getBool(0x1038C))
    return 26; // Vanilla removal events cannot mint tickets.
  second.setShineFlag(SMS_getShineID(2, 0, false));
  if (!SMS_isGetShine(2, 0, false) || SMS_isGetShine(2, 1, false) ||
      port_ap_shines() != 2 || SMSAPNextPlazaScenario() != 8)
    return 27;
  snprintf(path, sizeof(path), "%s/history.txt", directory);
  history = fopen(path, "wb");
  if (!history)
    return 28;
  fputs("SMSAPH1\n616263 0 1 ", history);
  for (int i = 0; i < 16; ++i)
    fprintf(history, "%02x", second.mAPGuid[i]);
  fputs("\n14\n0 523004 1 2 1\n1 523004 4 2 1\n2 523002 5 2 1\n"
        "3 523005 -2 1 1\n4 523006 6 2 1\n5 523007 7 2 1\n"
        "6 523008 8 2 1\n7 523009 9 2 1\n8 523010 10 2 1\n"
        "9 523011 11 2 1\n10 523013 12 2 1\n11 523001 13 2 1\n"
        "12 523003 14 2 1\n13 523004 15 2 1\n", history);
  if (fclose(history))
    return 29;
  // Toggle readiness to force the production bridge to poll new history.
  port_ap_tick(second.mAPGuid, true, false, 0, 1, APFixtureReadFlag);
  port_ap_tick(second.mAPGuid, true, true, 0, 1, APFixtureReadFlag);
  if (SMSAPNextPlazaScenario() != 2 || !port_ap_yoshi() ||
      !second.getBool(0x103AB) || !second.getBool(0x1038B) ||
      !second.getBool(0x1038C) || !port_ap_stage_allowed(52) ||
      second.getShineFlag(119) || second.getBool(0x1038F))
    return 30;
  gun.changeNozzle(TWaterGun::Hover, true);
  if (gun.mCurrentNozzle != TWaterGun::Hover)
    return 31;
  gun.changeNozzle(TWaterGun::Turbo, true);
  if (gun.mCurrentNozzle != TWaterGun::Turbo)
    return 32;
  TApplication ending;
  ending.mAppState = TApplication::APP_STATE_MOVIE;
  ending.mSaveFile = 0;
  ending.mCurrArea.set(15, 0, 0);
  second.setShineFlag(119); // MovieDirector's actual Bowser completion flag.
  ApplicationAPFrame(&ending, false);
  snprintf(path, sizeof(path), "%s/snapshot.json", directory);
  FILE *snapshot = fopen(path, "rb");
  if (!snapshot)
    return 33;
  char json[8192] = {0};
  fread(json, 1, sizeof(json) - 1, snapshot);
  fclose(snapshot);
  if (!strstr(json, "\"goal\":true") || !strstr(json, "\"ready\":false"))
    return 34;
  // Actual life mutation, spending, native card payload, saved cursor and
  // restore exercise the game-side transaction rather than a fake callback.
  TFlagManager lives;
  int applied = -1;
  if (lives.mAPOneUps || APReconcileOneUps(&lives, 2, &applied) != 5 ||
      applied != 2)
    return 35;
  lives.incMario(-1);
  if (APReconcileOneUps(&lives, 2, &applied) != 4)
    return 36;
  unsigned char lifeSave[0x800] = {0};
  JSUMemoryOutputStream lifeOut(lifeSave, sizeof(lifeSave));
  lives.save(lifeOut);
  lives.saveSuccess();
  if (lives.mAPOneUps != 2 || lives.mAPSavedOneUps != 2)
    return 37;
  TFlagManager loadedLives;
  JSUMemoryInputStream lifeIn(lifeSave, sizeof(lifeSave));
  loadedLives.load(lifeIn);
  if (loadedLives.mAPOneUps != 2 ||
      APReconcileOneUps(&loadedLives, 2, &applied) != 4 ||
      APReconcileOneUps(&loadedLives, 3, &applied) != 5)
    return 38;
  loadedLives.restore();
  if (loadedLives.mAPOneUps != 2 || loadedLives.getFlag(0x20001) != 4 ||
      APReconcileOneUps(&loadedLives, 3, &applied) != 5)
    return 39;
  loadedLives.setFlag(0x20001, 98);
  if (APReconcileOneUps(&loadedLives, 8, &applied) != 99 || applied != 8 ||
      APReconcileOneUps(&loadedLives, 8, &applied) != 99 ||
      APReconcileOneUps(&loadedLives, 7, &applied) != -1)
    return 40;
  loadedLives.firstStart();
  if (loadedLives.mAPOneUps || loadedLives.mAPSavedOneUps)
    return 41;
  fprintf(stderr, "SMS AP native progression passed: ticket plaza8 then "
                  "Corona plaza2; physical episode checks remain separate; "
                  "Noki/Sirena/Pianta flags and Yoshi readback; all nozzles; "
                  "Bowser goal publishes during ending outside gameplay\n");
  fprintf(stderr, "SMS AP native 1-UP methods passed: grant, spend, duplicate "
                  "replay, native save/load/restore cursor, 99 cap and "
                  "regression rejection\n");
  fprintf(stderr, "SMS AP native input regression passed: old hook loses D-pad "
                  "at 2/4 simulation ticks; fresh input equips Rocket at 1/2/4 "
                  "ticks, denies unowned Hover\n");
  fprintf(stderr, "SMS AP normal Continue predicate passed: new save replays "
                  "Airstrip; saved physical Airstrip flag with zero received "
                  "Shines continues beyond tutorial\n");
  fprintf(stderr,
          "SMS AP actual WaterGun changeNozzle readback passed: unowned "
          "denied, received Rocket equipped without box, unowned Hover denied; "
          "native Shine=2 and Bianco ticket readback passed\n");
  fprintf(stderr, "SMS AP native save methods passed: new-save isolation, "
                  "serialization/load/restore, save acknowledgement, physical "
                  "Shine separation\n");
  return 0;
}
