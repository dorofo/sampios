// Thin wrapper around RakNet's RakClient: connection lifetime, raw sends, RPCs
// and packet pumping. Everything here runs on the caller's thread; RakNet keeps
// its own socket thread and hands finished packets to Receive().
#pragma once

#include "BitStream.h"
#include "PacketPriority.h"

#include <functional>

class RakClient;
struct Packet;

namespace samp {

class Transport {
public:
    using PacketHandler = std::function<void(Packet*)>;

    Transport();
    ~Transport();

    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

    bool Start(const char* host, unsigned short port);
    void Stop();
    bool Active() const { return m_client != nullptr; }
    bool Connected() const;

    RakClient* Client() { return m_client; }

    bool Send(const RakNet::BitStream& stream, PacketPriority priority,
              PacketReliability reliability, char channel);
    void Rpc(int* rpcId, RakNet::BitStream& stream, PacketPriority priority,
             PacketReliability reliability, char channel = 0);

    // Drains the receive queue, handing every packet to `handler`. The handler
    // must not keep the pointer.
    void Pump(const PacketHandler& handler);

private:
    void Destroy();

    RakClient* m_client = nullptr;
    // Stop() may be called from inside a packet handler; deleting the client
    // there would make Pump() touch freed memory, so it is deferred.
    bool m_pumping = false;
    bool m_stopPending = false;
};

}  // namespace samp
