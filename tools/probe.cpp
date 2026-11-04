// Raw RakNet probe: no SampClient, no RPC map. Dumps every datagram the server
// sends during the 0.3.7 handshake so the state machine can be fixed against
// facts instead of guesses.

#include "RakClient.h"
#include "BitStream.h"
#include "PacketEnumerations.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

namespace {

double NowMs() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return duration<double, std::milli>(steady_clock::now() - start).count();
}

}  // namespace

int main(int argc, char** argv) {
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    const int port = argc > 2 ? std::atoi(argv[2]) : 7777;
    const double duration = argc > 3 ? std::atof(argv[3]) : 8.0;

    setvbuf(stdout, nullptr, _IONBF, 0);

    RakClient client;
    if (!client.Connect(host, static_cast<unsigned short>(port), 0, 0, 2)) {
        std::printf("connect() returned false\n");
        return 1;
    }

    const double start = NowMs();
    double lastConnectedLog = -1000;
    int packets = 0;

    while (NowMs() - start < duration) {
        while (Packet* p = client.Receive()) {
            ++packets;
            const unsigned char* d = p->data;
            std::printf("t=%6.0fms id=%3u len=%3u connected=%d  [", NowMs() - start, d[0],
                        p->length, client.IsConnected() ? 1 : 0);
            for (unsigned i = 0; i < p->length && i < 32; ++i) std::printf("%02X ", d[i]);
            std::printf("]\n");
            client.DeallocatePacket(p);
        }
        const bool connected = client.IsConnected();
        if (connected && NowMs() - lastConnectedLog > 500) {
            lastConnectedLog = NowMs();
            std::printf("t=%6.0fms IsConnected=1\n", NowMs() - start);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::printf("packets=%d connected=%d\n", packets, client.IsConnected() ? 1 : 0);
    client.Disconnect(100);
    return 0;
}
