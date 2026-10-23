#include "../core.hpp"
#include "../consumables.hpp"
#include "port_ap.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
static std::array<bool, 948> bits;
static int lives = 3;
static unsigned appliedOneUps = 0;
static int reconcileOneUps(int accepted, int *applied) {
  const int delta = smsap::one_up_delta(accepted, appliedOneUps);
  if (delta < 0)
    return -1;
  lives = std::min(99, lives + delta);
  appliedOneUps = accepted;
  *applied = appliedOneUps;
  return lives;
}
static int checks(unsigned f) {
  if (f == 0x20001)
    return lives;
  return f >= 0x10000 && f < 0x103b4 && bits[f - 0x10000];
}
int main(int argc, char **argv) {
  if (argc == 1) {
    unsigned blueCount = 0;
    for (unsigned bit = 0; bit < 948; ++bit)
      if (smsap::blue_coin_bit(bit)) ++blueCount;
    assert(blueCount == 240);
    for (unsigned trade = 0; trade < 24; ++trade) {
      unsigned bit = trade < 16 ? 70 + trade : 108 + trade - 16;
      assert(smsap::trade_index(bit) == int(trade));
      assert(!smsap::location_bit(bit, 1, trade));
      assert(smsap::location_bit(bit, 1, trade + 1));
    }
    assert(!smsap::location_bit(119, 1, 24));
    assert(!smsap::location_bit(870, 1, 24));
    assert(!smsap::location_bit(140, 1, 24));
    std::vector<smsap::Item> h = {{523004, 1, 2, 1},
                                  {523004, 2, 2, 1},
                                  {523002, -2, 1, 1},
                                  {523005, -2, 1, 1},
                                  {523140, 5, 2, 0},
                                  {523014, 6, 2, 0}};
    auto s = smsap::derive(h);
    assert(s.shines == 2 && s.nozzles == 5 && s.tickets == 1 &&
           s.oneUps == 1 && s.parked == std::vector<unsigned>{5});
    assert(smsap::one_up_delta(3, 1) == 2);
    assert(smsap::one_up_delta(3, 3) == 0);
    assert(smsap::one_up_delta(2, 3) == -1);
    assert(smsap::preferred_nozzle(s) == 1);
    assert(smsap::nozzle(s, 0) && smsap::nozzle(s, 1) && !smsap::nozzle(s, 4) &&
           !smsap::nozzle(s, 5));
    assert(smsap::stage(s, 2, 3) && !smsap::stage(s, 3, 3) &&
           !smsap::stage(s, 52, 3) && !smsap::stage(s, 10, 3) &&
           !smsap::stage(s, 34, 3));
    smsap::verify_prefix(h, h);
    auto next = h;
    next.push_back({523004, 8, 2, 1});
    smsap::verify_prefix(h, next);
    assert(smsap::derive(next).shines == 3);
    next.push_back({523140, 9, 2, 0});
    assert(smsap::derive(next).oneUps == 2);
    assert(smsap::stage(smsap::derive(next), 52, 3));
    assert(smsap::stage(s, 28, 3));
    assert(!smsap::stage(s, 30, 3));
    bool bad = false;
    next[0].location = 999;
    try {
      smsap::verify_prefix(h, next);
    } catch (...) {
      bad = true;
    }
    assert(bad);
    bad = false;
    next = h;
    next.pop_back();
    try {
      smsap::verify_prefix(h, next);
    } catch (...) {
      bad = true;
    }
    assert(bad);
    assert(smsap::hash({}) == "cbf29ce484222325");
    std::puts(
        "SMS AP reward, consumable cursor, replay conflict and ticket/nozzle "
        "gate tests passed");
    return 0;
  }
  unsigned char guid[16];
  std::memset(guid, 0x11, 16);
  if (std::strcmp(argv[1], "serve") == 0) {
    std::mutex mutex;
    std::queue<std::string> commands;
    std::atomic<bool> input_done(false);
    std::thread input([&] {
      std::string line;
      while (std::getline(std::cin, line)) {
        std::lock_guard<std::mutex> g(mutex);
        commands.push(line);
      }
      input_done = true;
    });
    bool playing = argc < 3 || std::stoi(argv[2]) != 0;
    bool dying = false;
    int stage = 1, episode = -1;
    if (argc > 3)
      appliedOneUps = std::stoul(argv[3]);
    if (argc > 4)
      lives = std::stoi(argv[4]);
    bool persisted = true, quit = false;
    while (!quit) {
      {
        std::lock_guard<std::mutex> g(mutex);
        while (!commands.empty()) {
          std::istringstream cmd(commands.front());
          commands.pop();
          std::string action;
          cmd >> action;
          if (action == "quit")
            quit = true;
          else if (action == "check") {
            unsigned bit;
            cmd >> bit;
            if (bit < 948)
              bits[bit] = true;
          } else if (action == "ready")
            cmd >> playing;
          else if (action == "death") { dying = true; playing = false; }
          else if (action == "alive") { dying = false; playing = true; }
          else if (action == "position") { cmd >> stage >> episode; }
          else if (action == "persisted")
            cmd >> persisted;
          else if (action == "lives")
            cmd >> lives;
          else if (action == "applied")
            cmd >> appliedOneUps;
          else if (action == "saveGUID") {
            std::string h;
            cmd >> h;
            if (h.size() == 32)
              for (int i = 0; i < 16; ++i)
                guid[i] =
                    (unsigned char)std::stoul(h.substr(i * 2, 2), nullptr, 16);
          }
        }
      }
      if (input_done)
        quit = true;
      port_ap_observe_goal(persisted && bits[119]);
      port_ap_observe_position(episode);
      port_ap_observe_death(dying, playing && !dying);
      port_ap_tick(guid, persisted, playing && !quit, 0, stage, checks,
                   reconcileOneUps);
      if (playing && port_ap_take_linked_death()) { dying = true; playing = false; }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    // The pipe input reader returns after the smoke controller closes stdin.
    if (input.joinable())
      input.join();
    return 0;
  }
  bits[86] = true;
  bits[119] = true;
  if (std::strcmp(argv[1], "fixture-all") == 0)
    bits.fill(true);
  const bool persisted = std::strcmp(argv[1], "unsaved") != 0;
  if (argc > 2)
    appliedOneUps = std::stoul(argv[2]);
  if (argc > 3)
    lives = std::stoi(argv[3]);
  if (std::strcmp(argv[1], "wrong-save") == 0)
    std::memset(guid, 0x22, 16);
  for (int i = 0; i < 2; ++i) {
    port_ap_observe_goal(persisted && bits[119]);
    port_ap_tick(guid, persisted, true, 0, 1, checks, reconcileOneUps);
    std::this_thread::sleep_for(std::chrono::milliseconds(270));
  }
  std::printf(
      "fixture readback shines=%d rocket=%d hover=%d stage2=%d stage3=%d "
      "lives=%d applied=%u blue_total=%d\n",
      port_ap_shines(), port_ap_nozzle_allowed(1), port_ap_nozzle_allowed(4),
      port_ap_stage_allowed(2), port_ap_stage_allowed(3), lives, appliedOneUps,
      port_ap_blue_coin_total(240));
  return 0;
}
