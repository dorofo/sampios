// Desktop harness for the shared protocol core.
//
// It runs the exact same SampClient code that the iOS app uses, but without
// SceneKit: connect, spawn, walk in a circle, send a chat line and print what
// the server pushes back. Handy for checking the protocol against a local
// samp-server without a device.
//
//   sampcli [host] [port] [nick] [seconds]

#include "SampBridge.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

namespace {

double NowSeconds() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return duration<double>(steady_clock::now() - start).count();
}

const char* StateName(int state) {
    switch (state) {
        case 0: return "idle";
        case 1: return "connecting";
        case 2: return "joining";
        case 3: return "spawning";
        case 4: return "in-game";
        default: return "error";
    }
}

}  // namespace

int main(int argc, char** argv) {
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    const int port = argc > 2 ? std::atoi(argv[2]) : 7777;
    const char* nick = argc > 3 ? argv[3] : "iostest";
    const double duration = argc > 4 ? std::atof(argv[4]) : 20.0;

    std::setvbuf(stdout, nullptr, _IONBF, 0);
    samp_init();
    if (!samp_connect(host, port, nick)) {
        std::printf("connect() failed\n");
        return 1;
    }

    int lastState = -1;
    double chatSentAt = -1.0;
    double start = NowSeconds();
    bool moved = false;

    int traceTicks = 5;
    while (NowSeconds() - start < duration) {
        if (traceTicks-- > 0) std::printf(".. tick enter\n");
        samp_tick();
        if (traceTicks >= 0) std::printf(".. tick leave\n");

        char logLine[256];
        while (samp_poll_log(logLine, sizeof(logLine))) {
            std::printf("[log] %s\n", logLine);
        }

        char name[64], text[256];
        int kind = 0;
        uint32_t colour = 0;
        while (samp_poll_chat(name, sizeof(name), text, sizeof(text), &kind, &colour)) {
            std::printf("[chat:%d] %s%s%s (0x%08X)\n", kind, name, name[0] ? ": " : "",
                        text, colour);
        }

        SampSnapshot snap{};
        samp_snapshot(&snap);
        if (snap.state != lastState) {
            std::printf("== state %s (id %d) at %.2f,%.2f,%.2f\n", StateName(snap.state),
                        snap.localId, snap.x, snap.y, snap.z);
            lastState = snap.state;
        }

        if (snap.state == 4) {
            if (!moved) {
                std::printf("== spawned, walking\n");
                moved = true;
            }
            // Walk a slow circle so other clients can see the ped move.
            const double t = NowSeconds() - start;
            samp_set_camera_yaw(static_cast<float>(std::fmod(t * 20.0, 360.0)));
            samp_set_input(0.0f, 1.0f, 0, 0);

            if (chatSentAt < 0.0) chatSentAt = NowSeconds();
            if (NowSeconds() - chatSentAt > 2.0) {
                samp_chat_send("priver from ios core test");
                chatSentAt = NowSeconds();
            }
        }

        SampRemote remotes[32];
        const int count = samp_players(remotes, 32);
        static int lastCount = -1;
        if (count != lastCount) {
            std::printf("== players: %d\n", count);
            for (int i = 0; i < count; ++i) {
                std::printf("   #%d %s skin=%d at %.1f,%.1f,%.1f hdg=%.0f\n", remotes[i].id,
                            remotes[i].name, remotes[i].skin, remotes[i].x, remotes[i].y,
                            remotes[i].z, remotes[i].heading);
            }
            lastCount = count;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    SampSnapshot snap{};
    samp_snapshot(&snap);
    std::printf("== final state %s at %.2f, %.2f, %.2f heading %.1f\n", StateName(snap.state),
                snap.x, snap.y, snap.z, snap.heading);
    samp_disconnect();
    return snap.state == 4 ? 0 : 2;
}
