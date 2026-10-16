#ifdef _WIN32
#include "port_host.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <io.h>
#include <direct.h>
#include <mutex>
#include <climits>
#include <windows.h>

static std::mutex s_file_position_mutex;

ssize_t port_pread(int fd, void* buffer, size_t count, unsigned long long offset)
{
	std::lock_guard<std::mutex> lock(s_file_position_mutex);
	if (_lseeki64(fd, offset, SEEK_SET) < 0)
		return -1;
	return _read(fd, buffer, (unsigned int)std::min(count, (size_t)INT_MAX));
}

int port_setenv(const char* name, const char* value, int overwrite)
{
	if (!overwrite && getenv(name))
		return 0;
	return _putenv_s(name, value);
}

int port_mkdir(const char* path, int)
{
	return _mkdir(path);
}

void port_sleep_until(const struct timespec* deadline)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	long long ns = (long long)(deadline->tv_sec - now.tv_sec) * 1000000000LL +
	               (deadline->tv_nsec - now.tv_nsec);
	if (ns > 0)
		Sleep((DWORD)((ns + 999999LL) / 1000000LL));
}

// VI only: a per-thread timer avoids changing audio timing or the system timer
// resolution. Unsupported hosts retain the original Sleep implementation.
void port_sleep_until_precise(const struct timespec* deadline)
{
	struct Timer {
		HANDLE handle = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
		                                      TIMER_MODIFY_STATE | SYNCHRONIZE);
		Timer() { std::fprintf(stderr,"[vi] precise timer: %s\n",handle ? "high-resolution waitable timer" : "Sleep fallback"); }
		~Timer() { if(handle) CloseHandle(handle); }
	};
	thread_local Timer timer;
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	const long long ns = (long long)(deadline->tv_sec-now.tv_sec)*1000000000LL + deadline->tv_nsec-now.tv_nsec;
	if(ns<=0) return;
	LARGE_INTEGER due;
	due.QuadPart = -((ns+99)/100); // Relative deadline, rounded to 100 ns.
	if(timer.handle && SetWaitableTimer(timer.handle,&due,0,nullptr,nullptr,FALSE) &&
	   WaitForSingleObject(timer.handle,INFINITE)==WAIT_OBJECT_0) return;
	port_sleep_until(deadline);
}
#endif
