#include "RakTransport.h"

#include "RakClient.h"

namespace samp {

Transport::Transport() = default;

Transport::~Transport() {
    Stop();
}

bool Transport::Start(const char* host, unsigned short port) {
    Stop();
    if (!host || !host[0]) return false;

    m_client = new RakClient();
    // clientPort 0 -> let the OS pick, threadSleepTimer 2 ms like the Android
    // client, which keeps latency low without busy-waiting.
    return m_client->Connect(host, port, 0, 0, 2);
}

void Transport::Stop() {
    if (m_pumping) {
        m_stopPending = true;
        return;
    }
    Destroy();
}

void Transport::Destroy() {
    if (!m_client) return;
    RakClient* client = m_client;
    m_client = nullptr;
    if (client->IsConnected()) client->Disconnect(100);
    delete client;
}

bool Transport::Connected() const {
    return m_client && m_client->IsConnected();
}

bool Transport::Send(const RakNet::BitStream& stream, PacketPriority priority,
                     PacketReliability reliability, char channel) {
    if (!m_client) return false;
    return m_client->Send(const_cast<RakNet::BitStream*>(&stream), priority, reliability, channel);
}

void Transport::Rpc(int* rpcId, RakNet::BitStream& stream, PacketPriority priority,
                    PacketReliability reliability, char channel) {
    if (!m_client || !m_client->IsConnected()) return;
    m_client->RPC(rpcId, &stream, priority, reliability, channel, false,
                  UNASSIGNED_NETWORK_ID, nullptr);
}

void Transport::Pump(const PacketHandler& handler) {
    if (!m_client || m_pumping) return;

    m_pumping = true;
    RakClient* client = m_client;
    while (Packet* packet = client->Receive()) {
        if (handler) handler(packet);
        client->DeallocatePacket(packet);
        if (m_stopPending) break;
    }
    m_pumping = false;

    if (m_stopPending) {
        m_stopPending = false;
        Destroy();
    }
}

}  // namespace samp
