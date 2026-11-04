#include "FileLog.h"

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace samp {
namespace filelog {
namespace {

FILE* g_file = nullptr;
std::string g_path;

// Only async-signal-safe work happens here: write(2) through a raw descriptor.
int g_fd = -1;

void RawWrite(const char* text) {
    if (g_fd < 0 || !text) return;
#if defined(_WIN32)
    _write(g_fd, text, static_cast<unsigned>(std::strlen(text)));
#else
    (void)!write(g_fd, text, std::strlen(text));
#endif
}

void OnSignal(int sig) {
    const char* name = "signal";
    switch (sig) {
        case SIGSEGV: name = "SIGSEGV"; break;
        case SIGABRT: name = "SIGABRT"; break;
#if defined(SIGBUS)
        case SIGBUS: name = "SIGBUS"; break;
#endif
        case SIGILL: name = "SIGILL"; break;
        case SIGFPE: name = "SIGFPE"; break;
        default: break;
    }
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "!!! CRASH %s (signal %d)\n", name, sig);
    RawWrite(buffer);
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

std::string Timestamp() {
    char buffer[32] = {};
    const std::time_t now = std::time(nullptr);
    std::tm tmValue{};
#if defined(_WIN32)
    localtime_s(&tmValue, &now);
#else
    localtime_r(&now, &tmValue);
#endif
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &tmValue);
    return buffer;
}

}  // namespace

void Init(const std::string& path) {
    if (g_file) return;
    g_path = path;
    g_file = std::fopen(g_path.c_str(), "ab");
    if (!g_file) return;
    g_fd = fileno(g_file);
    Write("=== session start ===");
    InstallCrashHandler();
}

void Write(const std::string& line) {
    if (!g_file) return;
    const std::string stamped = Timestamp() + " " + line + "\n";
    std::fwrite(stamped.data(), 1, stamped.size(), g_file);
    std::fflush(g_file);
}

void Stage(const char* name) {
    Write(std::string("stage: ") + (name ? name : "?"));
}

void InstallCrashHandler() {
#if !defined(_WIN32)
    std::signal(SIGSEGV, OnSignal);
    std::signal(SIGABRT, OnSignal);
#if defined(SIGBUS)
    std::signal(SIGBUS, OnSignal);
#endif
    std::signal(SIGILL, OnSignal);
    std::signal(SIGFPE, OnSignal);
#endif
}

std::string PreviousSessionTail(int maxLines) {
    if (g_path.empty()) return {};
    FILE* file = std::fopen(g_path.c_str(), "rb");
    if (!file) return {};

    std::vector<std::string> lines;
    char buffer[512];
    std::string current;
    while (std::fgets(buffer, sizeof(buffer), file)) {
        current = buffer;
        if (!current.empty() && current.back() == '\n') current.pop_back();
        lines.push_back(current);
        if (lines.size() > static_cast<size_t>(maxLines * 4)) {
            lines.erase(lines.begin(), lines.begin() + maxLines);
        }
    }
    std::fclose(file);

    std::string out;
    const int start = lines.size() > static_cast<size_t>(maxLines)
                          ? static_cast<int>(lines.size()) - maxLines
                          : 0;
    for (int i = start; i < static_cast<int>(lines.size()); ++i) {
        out += lines[static_cast<size_t>(i)];
        out += '\n';
    }
    return out;
}

}  // namespace filelog
}  // namespace samp
