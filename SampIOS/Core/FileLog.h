// Crash-tolerant session log.
//
// Writes straight to a file in the app container with an fsync-ish flush per
// line, so the last line on disk after a crash identifies the exact stage that
// killed the process. A minimal signal handler appends the faulting signal and
// address on top of that.
#pragma once

#include <string>

namespace samp {
namespace filelog {

void Init(const std::string& path);
void Write(const std::string& line);

// Appends a marker that survives a crash right after it.
void Stage(const char* name);

// Returns the tail of the previous (possibly crashed) session, oldest first.
std::string PreviousSessionTail(int maxLines);

void InstallCrashHandler();

}  // namespace filelog
}  // namespace samp
