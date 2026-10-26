#ifndef SMS_AP_CORE_HPP
#define SMS_AP_CORE_HPP
#include "blue_coins.hpp"
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace smsap {
struct Item {
  std::int64_t id, location;
  int player, flags;
  bool operator==(const Item &o) const {
    return id == o.id && location == o.location && player == o.player &&
           flags == o.flags;
  }
};
struct State {
  int shines = 0, oneUps = 0, blueCoins = 0;
  unsigned nozzles = 1, tickets = 0;
  bool yoshi = false;
  std::vector<unsigned> parked;
};
inline State derive(const std::vector<Item> &h, bool shuffle_blue = false, int starting = 0) {
  State s;
  s.nozzles = starting == 0 ? 1 : starting == 1 ? 2 : 0;
  for (unsigned i = 0; i < h.size(); ++i) {
    auto id = h[i].id;
    if (id == 523004)
      ++s.shines;
    else if (id >= 523000 && id <= 523003)
      s.nozzles |= 1u << (id - 523000);
    else if (id >= 523005 && id <= 523011)
      s.tickets |= 1u << (id - 523005);
    else if (id == 523013)
      s.yoshi = true;
    else if (id == 523140)
      ++s.oneUps;
    else if (id == 523014 && shuffle_blue)
      ++s.blueCoins;
    else
      s.parked.push_back(i);
  }
  return s;
}
inline void verify_prefix(const std::vector<Item> &old,
                          const std::vector<Item> &next) {
  if (next.size() < old.size())
    throw std::runtime_error("received history shrank");
  for (unsigned i = 0; i < old.size(); ++i)
    if (!(old[i] == next[i]))
      throw std::runtime_error(
          "received history conflicts with durable prefix");
}
inline std::string hash_text(const std::string &text) {
  std::uint64_t h = 14695981039346656037ull;
  for (unsigned char c : text) {
    h ^= c;
    h *= 1099511628211ull;
  }
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << h;
  return out.str();
}
inline std::string hash(const std::vector<Item> &items) {
  // FNV-1a64 over canonical ASCII 'index item location player flags\n'.
  std::uint64_t h = 14695981039346656037ull;
  for (unsigned i = 0; i < items.size(); ++i) {
    std::ostringstream line;
    line << i << ' ' << items[i].id << ' ' << items[i].location << ' '
         << items[i].player << ' ' << items[i].flags << '\n';
    for (unsigned char c : line.str()) {
      h ^= c;
      h *= 1099511628211ull;
    }
  }
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << h;
  return out.str();
}
inline bool nozzle(const State &s, int n) {
  if (n == 0)
    return s.nozzles & 1;
  if (n == 4)
    return s.nozzles & 2;
  if (n == 1)
    return s.nozzles & 4;
  if (n == 5)
    return s.nozzles & 8;
  return n == 2 ||
         n == 3; // payload permits underwater/Yoshi; eggs themselves are gated
}
inline int preferred_nozzle(const State &s) {
  if (s.nozzles & 2)
    return 4;
  if (s.nozzles & 8)
    return 5;
  if (s.nozzles & 4)
    return 1;
  return 0;
}
inline int ticket_index(unsigned flag) {
  const unsigned flags[] = {0x10385, 0x10386, 0x10387, 0x10389,
                            0x103ab, 0x1038b, 0x1038c};
  for (int i = 0; i < 7; ++i)
    if (flag == flags[i])
      return i;
  return -1;
}
inline bool stage(const State &s, int area, int required, bool tickets = true) {
  // Pinned native StageUtil.cpp shineStageTable, including secrets/subareas.
  const unsigned char worlds[] = {
      0, 1, 2, 3, 4, 5, 6, 6, 7, 8, 9, 1, 1, 5, 6, 1, 8, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 2, 1, 3, 8, 4, 4, 5, 5, 5, 5, 5, 5, 6, 5,
      7, 7, 8, 8, 2, 2, 3, 3, 5, 6, 9, 1, 1, 2, 6, 8, 5, 3, 9};
  if (area < 0 || area >= int(sizeof(worlds)))
    return false;
  const int world = worlds[area];
  if (world == 9)
    return s.shines >= required;
  if (!tickets) return true; // Native vanilla entrance events enforce the source thresholds.
  const int ticket_for_world[] = {-1, -1, 0, 1, 2, 3, 5, 6, 4};
  const int i = ticket_for_world[world];
  return i < 0 || (s.tickets & (1u << i)) != 0;
}
} // namespace smsap
#endif
