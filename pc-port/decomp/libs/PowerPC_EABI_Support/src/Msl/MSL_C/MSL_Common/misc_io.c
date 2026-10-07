#include "ansi_files.h"
#include "char_io.h"
#include "errno.h"
#include "stdio.h"

char* strerror(int errnum);

void clearerr(FILE* stream)
{
	stream->file_state.eof   = 0;
	stream->file_state.error = 0;
}

int feof(FILE* stream) { return stream->file_state.eof; }

int ferror(FILE* stream) { return stream->file_state.error; }

// TODO: perror is 0x8c here against the map's 0x74. This is the standard's
// description (prefix, ": ", strerror(errno), newline on stderr); testing only
// the pointer gives 0x80 and two fprintf calls 0x70, so MSL's spelling is open.
void perror(const char* s)
{
	if (s && *s) {
		fputs(s, stderr);
		fputs(": ", stderr);
	}

	fputs(strerror(errno), stderr);
	fputs("\n", stderr);
}

void __stdio_atexit(void) { }
