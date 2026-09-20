// Reports where a hard host crash happened.
//
// A fault in this port used to leave no trace whatsoever - no FATAL line, no
// exit code, nothing in stderr - and no command-line debugger is installed on
// the development machine, so the only way to learn a crash location is to
// catch it in-process. This reports and then lets the fault take its normal
// course; it deliberately does NOT recover, because continuing after an access
// violation turns a located fault into an unlocated one later.
//
// Windows uses a vectored exception handler with DbgHelp symbolisation;
// everything else uses POSIX signals with backtrace(). Symbol names need a
// build with debug info - without one the report still gives addresses, which
// name the culprit once mapped.
#pragma once

namespace sms::recomp::crash {

// Call once, as early as possible in main().
void install();

} // namespace sms::recomp::crash
