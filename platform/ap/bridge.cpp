#include "core.hpp"
#include "port_ap.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
namespace {
struct Bridge {
  std::string dir, seed, guid, boot, config, error, reason;
  int team = 0, slot = 0, required = 50, level = 1, starting = 0;
  bool enabled = false, configured = false, loaded = false, active = false;
  int blueMode = 0, blueMaximum = 0, trades = 0;
  int allEpisodes = 0, nozzleBoxes = 0, deathLink = 0;
  bool stageFluddless = false;
  int episode = -1;
  bool dying = false, remoteDeath = false, linkedDeathPending = false;
  unsigned long long deaths = 0, deathCommand = 0, deathApplied = 0;
  unsigned long long linkedDeathDeadline = 0;
  unsigned long long heartbeat = 0;
  std::vector<std::pair<long long, unsigned>> locations;
  std::vector<smsap::Item> history;
  smsap::State state;
  int oneUpsApplied = 0;
  bool physicalGoal = false;
  int currentNozzle = -1, secondaryNozzle = -1, hasFludd = 0;
  unsigned int padButtons = 0, padTrigger = 0;
  unsigned long long dpadEvents = 0, inputFrames = 0;
  int lastRequest = -1, lastBefore = -1, lastAfter = -1, lastAllowed = 0;

#ifdef _WIN32
  HANDLE lock = INVALID_HANDLE_VALUE;
#else
  int lock = -1;
#endif
  Bridge() {
    const char *p = std::getenv("SMS_AP_BRIDGE");
    if (p && *p) {
      dir = p;
      enabled = true;
    }
    unsigned char b[16];
    port_ap_new_guid(b);
    boot = hex(b);
  }
  static std::string hex(const unsigned char *b) {
    std::ostringstream o;
    for (int i = 0; i < 16; ++i)
      o << std::hex << std::setw(2) << std::setfill('0') << (int)b[i];
    return o.str();
  }
};
// Host globals in executable, outside libsms_game's save-state capture ranges.
Bridge &bridge() {
  static Bridge b;
  return b;
}
unsigned long pid() {
#ifdef _WIN32
  return GetCurrentProcessId();
#else
  return getpid();
#endif
}
std::string quote(const std::string &x) {
  std::string o = "\"";
  for (char c : x) {
    if (c == '\\' || c == '\"')
      o += '\\';
    if ((unsigned char)c >= 32)
      o += c;
  }
  return o + '"';
}
std::string read(const std::string &path, bool optional = false) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    if (optional)
      return "";
    throw std::runtime_error("cannot open " + path);
  }
  f.seekg(0, std::ios::end);
  auto size = f.tellg();
  f.seekg(0);
  if (size < 0 || size > 1024 * 1024)
    throw std::runtime_error("bridge file exceeds 1 MiB");
  std::ostringstream s;
  s << f.rdbuf();
  if (s.str().size() > 1024 * 1024)
    throw std::runtime_error("bridge file exceeds 1 MiB");
  return s.str();
}
void atomic(const std::string &path, const std::string &data) {
  std::string temp = path + ".tmp." + std::to_string(pid());
  FILE *f = std::fopen(temp.c_str(), "wb");
  if (!f)
    throw std::runtime_error("cannot create durable checkpoint");
  bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size() &&
            std::fflush(f) == 0;
#ifdef _WIN32
  if (ok)
    ok = _commit(_fileno(f)) == 0;
#else
  if (ok)
    ok = fsync(fileno(f)) == 0;
#endif
  if (std::fclose(f) != 0)
    ok = false;
  if (!ok) {
    std::remove(temp.c_str());
    throw std::runtime_error("checkpoint flush failed");
  }
#ifdef _WIN32
  // Readers without FILE_SHARE_DELETE (including Python's normal open) can
  // briefly prevent atomic replacement. Retry only sharing/access conflicts;
  // never acknowledge an unflushed or uncommitted journal.
  DWORD failure = ERROR_SUCCESS;
  bool replaced = false;
  for (int attempt = 0; attempt <= 10; ++attempt) {
    if (MoveFileExA(temp.c_str(), path.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      replaced = true;
      break;
    }
    failure = GetLastError();
    if (attempt == 10 ||
        (failure != ERROR_SHARING_VIOLATION &&
         failure != ERROR_LOCK_VIOLATION && failure != ERROR_ACCESS_DENIED))
      break;
    Sleep(2);
  }
  if (!replaced) {
    std::remove(temp.c_str());
    throw std::runtime_error("checkpoint replace failed, Win32=" +
                             std::to_string(failure));
  }
#else
  if (std::rename(temp.c_str(), path.c_str()) != 0)
    throw std::runtime_error("checkpoint replace failed");
#endif
}
bool hexstr(const std::string &s) {
  return !s.empty() && s.size() <= 512 && s.size() % 2 == 0 &&
         s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
void end(std::istream &in) {
  std::string extra;
  if (in >> extra)
    throw std::runtime_error("unexpected trailing wire data");
  if (!in.eof())
    throw std::runtime_error("truncated wire data");
}
void configure(Bridge &b) {
  std::string data = read(b.dir + "/session.txt");
  if (b.configured) {
    if (data != b.config)
      throw std::runtime_error(
          "session configuration changed while native game running");
    return;
  }
#ifdef _WIN32
  if (b.lock == INVALID_HANDLE_VALUE)
    b.lock = CreateFileA((b.dir + "/native.lock").c_str(),
                         GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
  if (b.lock == INVALID_HANDLE_VALUE)
    throw std::runtime_error("another native process owns bridge");
#else
  if (b.lock < 0)
    b.lock = open((b.dir + "/native.lock").c_str(), O_CREAT | O_RDWR, 0600);
  if (b.lock < 0 || flock(b.lock, LOCK_EX | LOCK_NB))
    throw std::runtime_error("another native process owns bridge");
#endif
  std::istringstream in(data);
  std::string magic;
  int starting = 0, level = 0;
  unsigned n;
  int blue_mode = 0, blue_maximum = 0, trades = 0;
  int episodes = 0, nozzles = 0, death_link = 0;
  if (!(in >> magic >> b.seed >> b.team >> b.slot >> b.required >> starting >>
        level) ||
      (magic != "SMSAP1" && magic != "SMSAP2" && magic != "SMSAP3") || !hexstr(b.seed) || b.team < 0 || b.slot <= 0 ||
      b.required < 0 || b.required > 345 || starting < 0 || starting > 2 || level < 0 || level > 1 ||
      (magic != "SMSAP3" && (starting != 0 || level != 1)) ||
      (magic == "SMSAP2" && (!(in >> blue_mode >> blue_maximum >> trades) ||
       (blue_mode != 1 && blue_mode != 2) || blue_maximum < 0 || blue_maximum > 240 ||
       trades < 0 || trades > 24 || (blue_mode == 1 && blue_maximum < trades * 10))) ||
      (magic == "SMSAP3" && (!(in >> blue_mode >> blue_maximum >> trades >> episodes >> nozzles >> death_link) ||
       blue_mode < 0 || blue_mode > 2 || blue_maximum < 0 || blue_maximum > 240 || trades < 0 || trades > 24 ||
       (blue_mode == 1 && blue_maximum < trades * 10) || episodes < 0 || episodes > 1 ||
       nozzles < 0 || nozzles > 1 || death_link < 0 || death_link > 1)) ||
      !(in >> n) || n > 400)
    throw std::runtime_error("unsupported or malformed SMSAP1/SMSAP2/SMSAP3 session");
  std::vector<long long> ids;
  std::vector<unsigned> bits;
  std::vector<std::pair<long long, unsigned>> locations;
  unsigned blue_count = 0;
  unsigned trade_count = 0;
  unsigned nozzle_count = 0;
  for (unsigned i = 0; i < n; ++i) {
    long long id;
    unsigned bit;
    if (!(in >> id >> bit) || id <= 0 || !smsap::location_bit(bit, blue_mode, trades, nozzles) ||
        (starting == 2 && bit == 86))
      throw std::runtime_error("invalid AP location flag");
    if (std::find(ids.begin(), ids.end(), id) != ids.end() ||
        std::find(bits.begin(), bits.end(), bit) != bits.end())
      throw std::runtime_error("duplicate AP location mapping");
    ids.push_back(id);
    bits.push_back(bit);
    locations.push_back({id, bit});
    if (smsap::blue_coin_bit(bit))
      ++blue_count;
    if (smsap::trade_shine_bit(bit))
      ++trade_count;
    if (smsap::nozzle_box_bit(bit)) ++nozzle_count;
  }
  end(in);
  if (blue_mode == 1 && blue_count != 240)
    throw std::runtime_error("full-shuffle session requires all 240 blue coin flags");
  if (blue_mode && trade_count != unsigned(trades))
    throw std::runtime_error("session requires exactly its configured trade flags");
  if (nozzle_count != (nozzles ? 10u : 0u))
    throw std::runtime_error("session requires exactly its configured nozzle box flags");
  b.locations = locations;
  b.blueMode = blue_mode;
  b.blueMaximum = blue_maximum;
  b.starting = starting;
  b.level = level;
  b.allEpisodes = episodes;
  b.nozzleBoxes = nozzles;
  b.deathLink = death_link;
  b.trades = trades;
  b.config = data;
  b.configured = true;
}
std::vector<smsap::Item> parse_history(Bridge &b, const std::string &data) {
  std::istringstream in(data);
  std::string magic, seed, guid;
  int team, slot;
  unsigned n;
  if (!(in >> magic >> seed >> team >> slot >> guid >> n) ||
      magic != "SMSAPH1" || seed != b.seed || team != b.team ||
      slot != b.slot || guid != b.guid || n > 10000)
    throw std::runtime_error("history belongs to another seed/team/slot/native "
                             "save or malformed history");
  std::vector<smsap::Item> h;
  for (unsigned i = 0; i < n; ++i) {
    unsigned index;
    smsap::Item x;
    if (!(in >> index >> x.id >> x.location >> x.player >> x.flags) ||
        index != i || x.id <= 0 || x.player < 0 || x.flags < 0 || x.flags > 7)
      throw std::runtime_error("invalid indexed AP history");
    h.push_back(x);
  }
  end(in);
  return h;
}
std::string serialize(Bridge &b, const std::vector<smsap::Item> &h) {
  std::ostringstream o;
  o << "SMSAPH1\n"
    << b.seed << ' ' << b.team << ' ' << b.slot << ' ' << b.guid << '\n'
    << h.size() << '\n';
  for (unsigned i = 0; i < h.size(); ++i)
    o << i << ' ' << h[i].id << ' ' << h[i].location << ' ' << h[i].player
      << ' ' << h[i].flags << '\n';
  return o.str();
}
void snapshot(Bridge &b, bool ready, int save, int stage,
              int (*flag)(unsigned)) {
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();
  std::ostringstream o;
  o << "{\"schema\":1,\"pid\":" << pid() << ",\"boot_id\":" << quote(b.boot)
    << ",\"session_hash\":" << quote(smsap::hash_text(b.config))
    << ",\"heartbeat\":" << ++b.heartbeat << ",\"timestamp_ms\":" << now
    << ",\"identity\":{\"game\":\"Super Mario Sunshine\",\"room_seed_hex\":"
    << quote(b.seed) << ",\"team\":" << b.team << ",\"slot\":" << b.slot
    << ",\"save_guid\":" << quote(b.guid) << ",\"save_slot\":" << save
    << "},\"ready\":" << (ready ? "true" : "false") << ",\"stage\":" << stage
    << ",\"episode\":" << b.episode << ",\"death_count\":" << b.deaths
    << ",\"death_applied\":" << b.deathApplied
    << ",\"death_acknowledged\":" << b.deathCommand
    << ",\"checks\":[";
  bool first = true;
  if (ready)
    for (auto p : b.locations)
      if (flag(0x10000 + p.second)) {
        if (!first)
          o << ',';
        o << p.first;
        first = false;
      }
  o << "],\"goal\":"
    << (b.active && b.physicalGoal ? "true" : "false")
    << ",\"received_shines\":" << b.state.shines
    << ",\"received_blue_coins\":" << b.state.blueCoins
    << ",\"one_up_count\":" << b.state.oneUps
    << ",\"one_up_applied_count\":" << b.oneUpsApplied
    << ",\"lives\":" << (b.active ? flag(0x20001) : -1)
    << ",\"nozzle_mask\":" << b.state.nozzles
    << ",\"native_current_nozzle\":" << b.currentNozzle
    << ",\"native_secondary_nozzle\":" << b.secondaryNozzle
    << ",\"native_has_fludd\":" << (b.hasFludd ? "true" : "false")
    << ",\"native_pad_buttons\":" << b.padButtons
    << ",\"native_pad_trigger\":" << b.padTrigger
    << ",\"native_dpad_events\":" << b.dpadEvents
    << ",\"native_input_frames\":" << b.inputFrames
    << ",\"native_last_nozzle_request\":" << b.lastRequest
    << ",\"native_last_nozzle_before\":" << b.lastBefore
    << ",\"native_last_nozzle_after\":" << b.lastAfter
    << ",\"native_last_request_allowed\":" << (b.lastAllowed ? "true" : "false")
    << ",\"yoshi\":" << (b.state.yoshi ? "true" : "false")
    << ",\"ticket_mask\":" << b.state.tickets
    << ",\"accepted_history_count\":" << b.history.size()
    << ",\"accepted_history_hash\":" << quote(smsap::hash(b.history))
    << ",\"accepted_items\":[";
  first = true;
  for (unsigned i = 0; i < b.history.size(); ++i) {
    if (!first)
      o << ',';
    const auto &x = b.history[i];
    o << '[' << i << ',' << x.id << ',' << x.location << ',' << x.player << ','
      << x.flags << ']';
    first = false;
  }
  o << "],\"parked_indices\":[";
  first = true;
  for (unsigned i : b.state.parked) {
    if (!first)
      o << ',';
    o << i;
    first = false;
  }
  o << "],\"reason\":" << quote(b.reason) << ",\"error\":" << quote(b.error)
    << "}\n";
  atomic(b.dir + "/snapshot.json", o.str());
}
} // namespace
extern "C" {
void port_ap_new_guid(unsigned char guid[16]) {
  std::random_device r;
  for (int i = 0; i < 16; ++i)
    guid[i] = (unsigned char)r();
  guid[6] = (guid[6] & 15) | 64;
  guid[8] = (guid[8] & 63) | 128;
}
int port_ap_enabled() { return bridge().enabled; }
void port_ap_observe_position(int episode) { bridge().episode = episode; }
void port_ap_observe_death(int dying, int alive) {
  Bridge &b = bridge();
  if (!b.enabled || !b.configured || !b.deathLink) return;
  if (dying && !b.dying) {
    b.linkedDeathPending = false;
    if (!b.remoteDeath) ++b.deaths;
    b.remoteDeath = false;
    b.dying = true;
  } else if (alive && !dying) b.dying = false;
}
int port_ap_take_linked_death() {
  Bridge &b = bridge();
  if (!b.active || !b.linkedDeathPending) return 0;
  auto current = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  if (b.dying || (unsigned long long)current > b.linkedDeathDeadline) {
    b.linkedDeathPending = false;
    return 0;
  }
  b.linkedDeathPending = false;
  b.remoteDeath = true;
  b.deathApplied = b.deathCommand;
  return 1;
}
int port_ap_ticket_mode() { return bridge().level; }
int port_ap_starting_nozzle() { return bridge().starting; }
int port_ap_episode_count(int count) {
  return bridge().enabled && bridge().configured && bridge().allEpisodes ? 8 : count;
}
void port_ap_observe_fludd_stage(int absent) { bridge().stageFluddless = absent != 0; }
int port_ap_fludd_allowed() { return port_ap_has_nozzle() && !bridge().stageFluddless; }
int port_ap_has_nozzle() {
  Bridge &b = bridge();
  return !b.enabled || (b.active ? b.state.nozzles != 0 : b.starting != 2);
}
int port_ap_shuffle_blue_coins() {
  const Bridge &b = bridge();
  return b.enabled && b.configured && b.blueMode == 1;
}
int port_ap_blue_coin_total(int native_total) {
  const Bridge &b = bridge();
  if (!b.enabled || !b.configured || !b.blueMode) return native_total;
  const int total = b.blueMode == 1 ? (b.active ? b.state.blueCoins : 0) : native_total;
  return std::min(total, b.trades * 10);
}
int port_ap_block_blue_reward(unsigned flag) {
  const Bridge &b = bridge();
  if (!b.enabled || !b.configured || !b.blueMode) return 0;
  if (flag == 0x40001) return b.blueMode == 1;
  int trade = flag >= 0x10000 ? smsap::trade_index(flag - 0x10000) : -1;
  return trade >= b.trades && trade >= 0;
}
void port_ap_reset_authority() {
  bridge().active = false;
  bridge().physicalGoal = false;
  bridge().linkedDeathPending = false;
}
void port_ap_observe_goal(int physical_goal) {
  bridge().physicalGoal = physical_goal != 0;
}
void port_ap_observe_game(int current, int secondary, int has_fludd) {
  Bridge &b = bridge();
  b.currentNozzle = current;
  b.secondaryNozzle = secondary;
  b.hasFludd = has_fludd;
}
void port_ap_observe_input(unsigned int buttons, unsigned int trigger,
                           int request, int before, int after, int secondary,
                           int allowed) {
  Bridge &b = bridge();
  ++b.inputFrames;
  b.padButtons = buttons;
  b.padTrigger = trigger;
  if (request >= 0) {
    ++b.dpadEvents;
    b.lastRequest = request;
    b.lastBefore = before;
    b.lastAfter = after;
    b.lastAllowed = allowed;
  }
  b.currentNozzle = after;
  b.secondaryNozzle = secondary;
}

int port_ap_yoshi() { return bridge().active && bridge().state.yoshi; }
int port_ap_preferred_nozzle() {
  return smsap::preferred_nozzle(bridge().active ? bridge().state
                                             : smsap::derive({}, false, bridge().starting));
}
int port_ap_shines() { return bridge().active ? bridge().state.shines : 0; }
int port_ap_nozzle_allowed(int n) {
  return !bridge().enabled ||
         smsap::nozzle(bridge().active ? bridge().state : smsap::derive({}, false, bridge().starting), n);
}
int port_ap_ticket_flag(unsigned f, int *value) {
  Bridge &b = bridge();
  if (!b.enabled)
    return 0;
  int i = smsap::ticket_index(f);
  if (i == 0 && b.level == 0 && b.starting == 2) {
    *value = b.configured;
    return 1;
  }
  if (i >= 0 && b.level == 1) {
    *value = (b.active && (b.state.tickets & (1u << i))) != 0;
    return 1;
  }
  if (f == 0x103ae) {
    *value = b.active && b.state.shines >= b.required;
    return 1;
  }
  return 0;
}
int port_ap_stage_allowed(int s) {
  Bridge &b = bridge();
  return !b.enabled ||
         smsap::stage(b.active ? b.state : smsap::State(), s, b.required, b.level == 1);
}
void port_ap_tick(const unsigned char guid[16], int persisted, int playing,
                  int save, int stage, int (*flag)(unsigned),
                  int (*reconcile_one_ups)(int, int *)) {
  Bridge &b = bridge();
  if (!b.enabled)
    return;
  std::string nativeGuid = Bridge::hex(guid);
  if (!persisted || nativeGuid != b.guid)
    b.active = false;
  // Poll and publish at most four times/second. Readiness changes still publish
  // immediately.
  static auto last = std::chrono::steady_clock::time_point();
  static bool was_ready = false;
  auto now = std::chrono::steady_clock::now();
  bool ready = playing && persisted && save == 0;
  if (ready == was_ready && now - last < std::chrono::milliseconds(250))
    return;
  last = now;
  was_ready = ready;
  try {
    configure(b);
    std::string current = Bridge::hex(guid);
    if (!b.guid.empty() && b.guid != current)
      throw std::runtime_error(
          "different native save loaded: restart bridge with its own pairing");
    if (persisted && save == 0) {
      b.guid = current;
      const std::string journal = b.dir + "/journal-" + b.guid + ".txt";
      if (!b.loaded) {
        std::ifstream exists(journal);
        if (exists)
          b.history = parse_history(b, read(journal));
        b.state = smsap::derive(b.history, b.blueMode == 1, b.starting);
        b.loaded = true;
      }
      b.active = true;
      if (ready) {
        if (b.deathLink) {
          std::string command = read(b.dir + "/death-link.txt", true);
          if (!command.empty()) {
            std::istringstream input(command);
            std::string magic, seed, guid, boot;
            int team, slot;
            unsigned long long sequence, timestamp;
            if (!(input >> magic >> seed >> team >> slot >> guid >> boot >> sequence >> timestamp))
              throw std::runtime_error("malformed DeathLink command");
            end(input);
            auto timestampNow = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            // Retire valid same-boot commands even when their transient payload
            // expired during a menu/transition. Otherwise the client waits for
            // this acknowledgment forever and cannot publish a fresh next link.
            // Wrong session/save/boot commands never advance the sequence.
            if (magic == "SMSAPD1" && seed == b.seed && team == b.team && slot == b.slot &&
                guid == b.guid && boot == b.boot && sequence > b.deathCommand) {
              b.deathCommand = sequence;
              if (timestamp <= (unsigned long long)timestampNow + 1000 &&
                  timestamp + 10000 >= (unsigned long long)timestampNow) {
                b.linkedDeathPending = true;
                b.linkedDeathDeadline = timestamp + 10000;
              }
            }
          }
        }
        std::string incoming = read(b.dir + "/history.txt", true);
        if (!incoming.empty()) {
          auto next = parse_history(b, incoming);
          smsap::verify_prefix(b.history, next);
          if (next.size() != b.history.size()) {
            atomic(journal, serialize(b, next));
            b.history = next;
            b.state = smsap::derive(next, b.blueMode == 1, b.starting);
          }
        }
        // Persist the accepted tuples before mutating the native save. The
        // native saved cursor reconciles a restart/restore without replenishing
        // lives spent after an already-applied receipt.
        if (reconcile_one_ups) {
          int applied = -1;
          int lives = reconcile_one_ups(b.state.oneUps, &applied);
          if (applied != b.state.oneUps || lives < 0 || lives > 99)
            throw std::runtime_error("native 1-UP reconciliation failed");
          b.oneUpsApplied = applied;
        } else if (b.state.oneUps != 0) {
          throw std::runtime_error("native 1-UP reconciliation unavailable");
        }
      }
    }
    b.reason.clear();
    if (!persisted) {
      b.error.clear();
      b.reason = "save normally once to persist this native save identity";
    } else if (save != 0)
      b.error = "native slice supports save slot A only";
    else {
      b.error.clear();
      if (!playing)
        b.reason = "waiting for safe gameplay frame";
    }
    snapshot(b, ready, save, stage, flag);
  } catch (const std::exception &e) {
    b.error = e.what();
    try {
      snapshot(b, false, save, stage, flag);
    } catch (...) {
    }
  }
}
}
