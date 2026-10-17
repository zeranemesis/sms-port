// SMS_STAGE_TOUR: a test mode that plays a list of stages in one process,
// without the title screen or the file select.
//
//   SMS_STAGE_TOUR=2,0;3,0;9,0      stage,scenario pairs, in order
//   SMS_STAGE_TOUR_FRAMES=1800      game frames spent in each stage (default)
//   SMS_STAGE_TOUR_LOG=<file>       one line per stage start, with the
//                                   display-copy count (GXPC_FrameCount)
//
// After the logos, the application goes straight to the first stage. Once a
// stage has run its frames, the director is asked for the next one through
// setNextStage, as a pipe or a warp would; after the last, the game quits.
// A stage that is still there kLeaveGrace frames after that request (an event
// waiting for a button, such as the airstrip's arrival dialogue, defers the
// change) has its director end the scene itself.
// Nothing here runs unless SMS_STAGE_TOUR is set.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" unsigned int GXPC_FrameCount(void) __attribute__((weak));

namespace {

struct Stop {
    int stage, scenario;
};

// TApplication's states (System/Application.hpp).
enum { kBoot = 2 };
constexpr int kLeaveGrace = 300;

struct Tour {
    bool parsed = false;
    std::vector<Stop> stops;
    int frames = 1800;
    int index = -1;     // stop being played; -1 before the first
    int ticks = 0;      // direct() calls in the current stop
    bool leaving = false;
    int retries = 0;
    FILE* log = nullptr;
};

Tour g_tour;

Tour& tour()
{
    if (g_tour.parsed)
        return g_tour;
    g_tour.parsed = true;
    const char* list = std::getenv("SMS_STAGE_TOUR");
    if (!list || !*list)
        return g_tour;
    const char* p = list;
    while (*p) {
        int stage = -1, scenario = 0, used = 0;
        if (std::sscanf(p, " %d , %d%n", &stage, &scenario, &used) >= 1 && used > 0 && stage >= 0)
            g_tour.stops.push_back({stage, scenario});
        const char* next = std::strchr(p, ';');
        if (!next)
            break;
        p = next + 1;
    }
    if (const char* f = std::getenv("SMS_STAGE_TOUR_FRAMES"))
        g_tour.frames = std::atoi(f) > 0 ? std::atoi(f) : g_tour.frames;
    if (const char* f = std::getenv("SMS_STAGE_TOUR_LOG"))
        g_tour.log = std::fopen(f, "w");
    std::fprintf(stderr, "[tour] %zu stages, %d frames each\n", g_tour.stops.size(), g_tour.frames);
    return g_tour;
}

unsigned frame_count()
{
    return GXPC_FrameCount ? GXPC_FrameCount() : 0;
}

} // namespace

// Called by TApplication::proc as one state ends. Returns 0 to leave the
// transition alone, 1 to go to *stage,*scenario, or -1 to quit.
extern "C" int port_stage_tour_transition(int finishedState, int* stage, int* scenario)
{
    Tour& t = tour();
    // The logos stay: the end of the boot logo sets up the game's archives.
    // The title screen that would follow the Nintendo logo is skipped.
    if (t.stops.empty() || finishedState == kBoot)
        return 0;
    if (t.index < 0) {
        t.index = 0;
    } else if (t.leaving || t.retries >= 1) {
        // A state that ended without running its frames (a movie played in
        // the stage's place, or the stage failed to load) retries once.
        ++t.index;
        t.retries = 0;
    } else {
        ++t.retries;
    }
    t.ticks = 0;
    t.leaving = false;
    if (t.index >= (int)t.stops.size()) {
        std::fprintf(stderr, "[tour] done\n");
        if (t.log)
            std::fclose(t.log), t.log = nullptr;
        return -1;
    }
    *stage = t.stops[t.index].stage;
    *scenario = t.stops[t.index].scenario;
    std::fprintf(stderr, "[tour] stage %d,%d at frame %u\n", *stage, *scenario, frame_count());
    if (t.log) {
        std::fprintf(t.log, "start %d %d %u\n", *stage, *scenario, frame_count());
        std::fflush(t.log);
    }
    return 1;
}

// Called once per TMarDirector::direct. Returns non-zero, once per stop, when
// the stop has run its frames: the caller then passes it to setNextStage.
extern "C" int port_stage_tour_tick(void)
{
    Tour& t = tour();
    if (t.stops.empty() || t.index < 0 || t.leaving)
        return 0;
    if (++t.ticks < t.frames)
        return 0;
    t.ticks = 0;
    t.leaving = true;
    if (t.log) {
        std::fprintf(t.log, "leave %d %d %u\n", t.stops[t.index].stage, t.stops[t.index].scenario,
                     frame_count());
        std::fflush(t.log);
    }
    const int next = t.index + 1 < (int)t.stops.size() ? t.index + 1 : t.index;
    return ((t.stops[next].stage + 1) << 8) | (t.stops[next].scenario & 0xff);
}

// Called once per TMarDirector::direct, after port_stage_tour_tick. Returns 1
// once a stage has ignored its setNextStage for kLeaveGrace frames: direct
// then returns the gameplay state, which ends the scene like a finished fade.
extern "C" int port_stage_tour_force_exit(void)
{
    Tour& t = tour();
    if (t.stops.empty() || t.index < 0 || !t.leaving)
        return 0;
    if (++t.ticks < kLeaveGrace)
        return 0;
    if (t.ticks == kLeaveGrace)
        std::fprintf(stderr, "[tour] stage %d,%d ignored its exit for %d frames; ending it\n",
                     t.stops[t.index].stage, t.stops[t.index].scenario, kLeaveGrace);
    return 1;
}
