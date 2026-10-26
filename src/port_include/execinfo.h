#ifndef SMS_PORT_EXECINFO_H
#define SMS_PORT_EXECINFO_H

#ifdef _WIN32
#undef __declspec
#define __declspec(x) __attribute__((x))
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
static inline int backtrace(void** frames, int capacity)
{
	return (int)CaptureStackBackTrace(0, (DWORD)capacity, frames, NULL);
}
static inline void backtrace_symbols_fd(void* const* frames, int count, int)
{
	for (int i = 0; i < count; ++i)
		fprintf(stderr, "[port] frame %d: %p\n", i, frames[i]);
}
// Like glibc: one malloc'd block holding the pointer array and the strings,
// released with a single free(). Each entry is "module+0xoffset".
static inline char** backtrace_symbols(void* const* frames, int count)
{
	enum { kLen = 96 };
	if (count <= 0)
		return NULL;
	char** out = (char**)malloc(count * (sizeof(char*) + kLen));
	if (!out)
		return NULL;
	char* text = (char*)(out + count);
	for (int i = 0; i < count; ++i) {
		out[i] = text + i * kLen;
		HMODULE mod = NULL;
		char path[MAX_PATH] = "?";
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		                       (LPCSTR)frames[i], &mod))
			GetModuleFileNameA(mod, path, sizeof path);
		const char* base = path;
		for (const char* p = path; *p; ++p)
			if (*p == '\\' || *p == '/')
				base = p + 1;
		snprintf(out[i], kLen, "%s+0x%lx", base,
		         (unsigned long)((char*)frames[i] - (char*)mod));
	}
	return out;
}
#elif defined(__ANDROID__) && defined(__ANDROID_API__) && __ANDROID_API__ < 33
// Bionic exposes execinfo only from API 33. Keep older Android targets useful
// by unwinding natively, without depending on those newer libc symbols.
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unwind.h>

struct sms_port_backtrace_state {
	void** frames;
	int capacity;
	int count;
	int skip;
};

static _Unwind_Reason_Code sms_port_backtrace_capture(struct _Unwind_Context* context, void* argument)
{
	struct sms_port_backtrace_state* state = (struct sms_port_backtrace_state*)argument;
	uintptr_t ip = (uintptr_t)_Unwind_GetIP(context);
	if (!ip)
		return _URC_NO_REASON;
	if (state->skip) {
		--state->skip;
		return _URC_NO_REASON;
	}
	state->frames[state->count++] = (void*)ip;
	return state->count == state->capacity ? _URC_END_OF_STACK : _URC_NO_REASON;
}

// Do not inline: the first captured frame is this shim, which we skip.
static __attribute__((noinline, unused)) int backtrace(void** frames, int capacity)
{
	struct sms_port_backtrace_state state;
	if (!frames || capacity <= 0)
		return 0;
	state.frames = frames;
	state.capacity = capacity;
	state.count = 0;
	state.skip = 1;
	_Unwind_Backtrace(sms_port_backtrace_capture, &state);
	return state.count;
}

static inline int sms_port_backtrace_format(char* output, size_t capacity, const void* frame)
{
	Dl_info info;
	const char* module = "?";
	const char* symbol = "?";
	uintptr_t base = 0;
	memset(&info, 0, sizeof(info));
	if (dladdr(frame, &info)) {
		if (info.dli_fname)
			module = info.dli_fname;
		if (info.dli_sname)
			symbol = info.dli_sname;
		base = (uintptr_t)(info.dli_saddr ? info.dli_saddr : info.dli_fbase);
	}
	return snprintf(output, capacity, "%s(%s+0x%llx) [%p]", module, symbol,
	                (unsigned long long)((uintptr_t)frame - base), frame);
}

// Like libc: one allocation owns both the pointers and every string. Callers
// release the result with a single free(), not one free per frame.
static inline char** backtrace_symbols(void* const* frames, int count)
{
	size_t total, remaining;
	char** result;
	char* cursor;
	int i;
	if (!frames || count <= 0)
		return NULL;
	if ((size_t)count > (size_t)-1 / sizeof(char*)) {
		errno = EOVERFLOW;
		return NULL;
	}
	total = (size_t)count * sizeof(char*);
	for (i = 0; i < count; ++i) {
		int length = sms_port_backtrace_format(NULL, 0, frames[i]);
		if (length < 0)
			return NULL;
		if ((size_t)length + 1 > (size_t)-1 - total) {
			errno = EOVERFLOW;
			return NULL;
		}
		total += (size_t)length + 1;
	}
	result = (char**)malloc(total);
	if (!result)
		return NULL;
	cursor = (char*)(result + count);
	remaining = total - (size_t)count * sizeof(char*);
	for (i = 0; i < count; ++i) {
		int length = sms_port_backtrace_format(cursor, remaining, frames[i]);
		if (length < 0 || (size_t)length >= remaining) {
			free(result);
			errno = EOVERFLOW;
			return NULL;
		}
		result[i] = cursor;
		cursor += (size_t)length + 1;
		remaining -= (size_t)length + 1;
	}
	return result;
}

static inline int sms_port_backtrace_write(int fd, const char* data, size_t length)
{
	while (length) {
		ssize_t written = write(fd, data, length);
		if (written < 0) {
			if (errno == EINTR)
				continue;
			return 0;
		}
		if (!written) {
			errno = EIO;
			return 0;
		}
		data += (size_t)written;
		length -= (size_t)written;
	}
	return 1;
}

static inline void backtrace_symbols_fd(void* const* frames, int count, int fd)
{
	int i;
	if (!frames || count <= 0)
		return;
	for (i = 0; i < count; ++i) {
		char line[512];
		char* text = line;
		int length = sms_port_backtrace_format(line, sizeof(line), frames[i]);
		int ok, saved_errno;
		if (length < 0)
			return;
		if ((size_t)length >= sizeof(line)) {
			int actual;
			text = (char*)malloc((size_t)length + 1);
			if (!text)
				return;
			actual = sms_port_backtrace_format(text, (size_t)length + 1, frames[i]);
			if (actual < 0 || actual > length) {
				free(text);
				errno = EOVERFLOW;
				return;
			}
			length = actual;
		}
		ok = sms_port_backtrace_write(fd, text, (size_t)length) &&
		     sms_port_backtrace_write(fd, "\n", 1);
		saved_errno = errno;
		if (text != line)
			free(text);
		if (!ok) {
			errno = saved_errno;
			return;
		}
	}
}
#else
#include_next <execinfo.h>
#endif

#endif
