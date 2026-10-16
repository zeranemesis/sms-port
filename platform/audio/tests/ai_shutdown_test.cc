// Standalone lifetime regression test of the real AI implementation.
// Run each mode in a separate process: sdl, idempotent, timer, disabled,
// deterministic, wav PATH. SDL is replaced only at the device boundary;
// the FIFO callback, fallback timer and atexit shutdown are the real code.
// .cc keeps this test out of the production platform/*.cpp source glob.
#include "../ai.cpp"
// port_compat renames the game's entry point; this is a host test program.
#undef main
#include <cassert>
#include <chrono>
#include <thread>
#include <string>

namespace {
std::atomic<int> kicks{0};
std::atomic<int> callbacks{0};
std::atomic<bool> deviceClosed{false};
pthread_t deviceThread;
int closeCalls = 0;
bool deterministic = false;
std::string mode;
std::string wavPath;

void* mock_device(void*)
{
	uint8_t stream[512 * 4];
	while (!deviceClosed.load()) {
		sdl_callback(NULL, stream, sizeof stream);
		callbacks.fetch_add(1);
		std::this_thread::yield();
	}
	return NULL;
}

void mock_close(uint32_t dev)
{
	assert(dev == 17);
	// A caller holding mu would deadlock here, just as a real device close
	// would deadlock waiting for a callback blocked on that same mutex.
	{
		std::lock_guard<std::mutex> lock(g.mu);
	}
	++closeCalls;
	deviceClosed = true;
	assert(pthread_join(deviceThread, NULL) == 0);
}

void audit_after_shutdown()
{
	assert(g.stopping.load());
	assert(!g.running.load());
	assert(!g.sdl && g.sdlDevice == 0 && !g.clockStarted);
	assert(g.due.load() == 0 && g.wav == NULL);
	// This atexit hook runs after audio shutdown and before g's destructor.
	// The mutex and FIFO must still be alive while the producers are gone.
	std::lock_guard<std::mutex> lock(g.mu);
	assert(g.fifo.size() == 32768 * 2);
	if (mode == "sdl" || mode == "idempotent") {
		assert(closeCalls == 1 && callbacks.load() > 0);
		uint8_t stream[8];
		memset(stream, 0x7f, sizeof stream);
		int before = kicks.load();
		sdl_callback(NULL, stream, sizeof stream);
		for (uint8_t byte : stream)
			assert(byte == 0);
		assert(kicks.load() == before);
	} else {
		assert(closeCalls == 0);
	}
	if (mode == "wav") {
		FILE* file = fopen(wavPath.c_str(), "rb");
		assert(file);
		uint8_t header[44];
		assert(fread(header, 1, sizeof header, file) == sizeof header);
		assert(!memcmp(header, "RIFF", 4));
		assert(header[4] == 40 && header[5] == 0);
		assert(header[40] == 4 && header[41] == 0);
		fclose(file);
	}
	puts("AI shutdown lifetime checks passed");
}

template<class Predicate> void await(Predicate ready)
{
	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (!ready() && std::chrono::steady_clock::now() < deadline)
		std::this_thread::yield();
	assert(ready());
}
} // namespace

extern "C" {
int port_no_audio = 0;
int port_vi_deterministic(void) { return deterministic; }
void port_irq_add_source(port_irq_poll_fn) {}
void port_irq_kick(void) { kicks.fetch_add(1); }
void port_log(const char*, ...) {}
int port_audio_dsp_frame_pending(void) { return 0; }
int GXPC_GetSpeed(void) { return 1; }
}

int main(int argc, char** argv)
{
	assert(argc >= 2);
	mode = argv[1];
	assert(mode == "sdl" || mode == "idempotent" || mode == "timer" ||
	       mode == "disabled" || mode == "deterministic" || mode == "wav");
	port_setenv("SMS_AUDIO_OUT", "null", 1);
	port_setenv("SMS_AUDIO", mode == "wav" ? "1" : "0", 1);
	port_setenv("SMS_AUDIO_WAV", "", 1);
	deterministic = mode != "timer";
	port_no_audio = mode == "disabled";
	if (mode == "wav") {
		assert(argc == 3);
		wavPath = argv[2];
		port_setenv("SMS_AUDIO_WAV", wavPath.c_str(), 1);
	}
	assert(atexit(audit_after_shutdown) == 0);
	AIInit(NULL); // registers shutdown later, so it runs before our audit
	assert(AICheckInit());
	if (mode == "sdl" || mode == "idempotent") {
		g.sdlDevice = 17;
		g.closeDevice = mock_close;
		g.sdl = true;
		assert(pthread_create(&deviceThread, NULL, mock_device, NULL) == 0);
		await([] { return callbacks.load() > 0; });
		if (mode == "idempotent") {
			shutdown_output();
			shutdown_output();
			assert(closeCalls == 1);
		}
	} else if (mode == "timer") {
		assert(g.clockStarted && !g.sdl);
		AIStartDMA();
		await([] { return kicks.load() > 1; });
	} else if (mode == "wav") {
		assert(g.wav);
		const uint8_t frame[4] = {1, 2, 3, 4};
		assert(fwrite(frame, 1, sizeof frame, g.wav) == sizeof frame);
		g.wavBytes = sizeof frame;
	} else {
		assert(!g.clockStarted && !g.sdl);
	}
	// Exercise the same std::exit route used by the frame-limited game run.
	std::exit(0);
}
