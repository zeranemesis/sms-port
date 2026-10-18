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
//
// A code mod can take the game into its own first scene without the
// application's transitions (Super Mario Eclipse's boot goes straight to its
// Tutorial). Until a transition has been seen, the tour then runs from the
// director alone: the current area turning into the stop asked for (or any
// other) is that stop starting, and the game quits once the last stop has run
// its frames.
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
    bool sawTransition = false; // the application's transitions drive the tour
    int area = -1;              // without them: the area seen last
    int pending = -1;           // and the stop asked for next
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
    t.sawTransition = true;
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

namespace {

int encode(const Stop& s)
{
    return ((s.stage + 1) << 8) | (s.scenario & 0xff);
}

// The tour without the application's transitions (see the top of the file).
int director_tick(Tour& t, int area)
{
    const int previous = t.area;
    t.area = area;
    if (area != previous) {
        if (t.pending >= 0) {
            t.index = t.pending;
            t.pending = -1;
            t.ticks = 0;
            t.leaving = false;
            std::fprintf(stderr, "[tour] stage %d,%d at frame %u\n", t.stops[t.index].stage,
                         t.stops[t.index].scenario, frame_count());
            if (t.log) {
                std::fprintf(t.log, "start %d %d %u\n", t.stops[t.index].stage, t.stops[t.index].scenario,
                             frame_count());
                std::fflush(t.log);
            }
        }
    }
    if (t.index < 0) {
        // The mod's own first scene: leave it for the first stop.
        if (t.pending >= 0 || ++t.ticks < 60)
            return 0;
        t.ticks = 0;
        t.pending = 0;
        return encode(t.stops[0]);
    }
    if (t.leaving || ++t.ticks < t.frames)
        return 0;
    t.ticks = 0;
    t.leaving = true;
    if (t.log) {
        std::fprintf(t.log, "leave %d %d %u\n", t.stops[t.index].stage, t.stops[t.index].scenario,
                     frame_count());
        std::fflush(t.log);
    }
    if (t.index + 1 >= (int)t.stops.size()) {
        std::fprintf(stderr, "[tour] done\n");
        if (t.log)
            std::fclose(t.log), t.log = nullptr;
        std::fflush(nullptr);
        std::_Exit(0);
    }
    t.pending = t.index + 1;
    return encode(t.stops[t.pending]);
}

} // namespace

// Called once per TMarDirector::direct. Returns non-zero, once per stop, when
// the stop has run its frames: the caller then passes it to setNextStage.
extern "C" int port_stage_tour_tick(int area)
{
    Tour& t = tour();
    if (!t.stops.empty() && !t.sawTransition)
        return director_tick(t, area);
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
    return encode(t.stops[next]);
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
