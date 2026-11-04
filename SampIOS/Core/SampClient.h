// SA-MP client state machine: join handshake, spawn, player pool, chat and the
// on-foot movement simulation that feeds ID_PLAYER_SYNC.
//
// Everything is called from one thread (the main/UI thread). RakNet dispatches
// RPC callbacks from inside Transport::Pump, i.e. also on that thread.
#pragma once

#include "LogServer.h"
#include "RakTransport.h"
#include "SampProtocol.h"

#include <deque>
#include <mutex>
#include <string>

namespace samp {

struct RemotePlayer {
    bool active = false;
    uint16_t id = 0;
    char name[kMaxNameLen + 1] = {};
    int skin = 0;
    uint32_t colour = 0xFFFFFFFF;

    // Last state received from the server.
    float targetX = 0.0f, targetY = 0.0f, targetZ = 0.0f, targetHeading = 0.0f;
    // Smoothed values used for rendering.
    float x = 0.0f, y = 0.0f, z = 0.0f, heading = 0.0f;
};

struct ChatMessage {
    int kind = kChatInfo;
    uint32_t colour = 0xFFFFFFFF;
    std::string name;
    std::string text;
};

class Client {
public:
    static Client& Get();

    // Starts the file log and the HTTP log endpoint; safe to call repeatedly.
    void InitDiagnostics();

    bool Connect(const std::string& host, unsigned short port, const std::string& nick);
    void Disconnect();

    // Call at display rate. Pumps the socket, simulates movement, sends sync.
    void Tick();

    // Client build string sent in ClientJoin; servers validate it strictly, so
    // the UI value is tried first and then the usual 0.3.7 build names.
    void SetClientVersion(const std::string& version);
    std::string ClientVersion() const;

    void SetInput(float lr, float ud, bool sprint, bool jump);
    void SetCameraYaw(float degrees) { m_cameraYaw = degrees; }
    void SendChat(const std::string& text);

    int State() const { return m_state; }
    const std::string& LastError() const { return m_error; }
    uint16_t LocalId() const { return m_localId; }
    float X() const { return m_x; }
    float Y() const { return m_y; }
    float Z() const { return m_z; }
    float Heading() const { return m_heading; }
    int Health() const { return m_health; }
    int Armour() const { return m_armour; }

    const RemotePlayer* Players() const { return m_players; }
    int PlayerCount() const;

    // Returns false when the queue is empty.
    bool PopChat(ChatMessage& out);
    bool PopLog(std::string& out);

    // Full diagnostic log, served over HTTP by the embedded LogServer.
    std::string DumpLogText() const;
    unsigned short LogPort();

    // Script dialog currently shown by the server (login, registration, ...).
    bool HasDialog() const { return m_dialog.active; }
    const DialogInfo& Dialog() const { return m_dialog; }
    void RespondDialog(int button, const std::string& input, int listItem = -1);

    // Exposed for the RPC thunks in SampClient.cpp.
    void OnPacket(Packet* packet);
    void OnRpc(int rpcId, const unsigned char* data, int bitLength);

private:
    Client() = default;

    void SetState(int state, const char* reason = nullptr);
    void Log(const std::string& line);
    void AddChat(int kind, const std::string& name, const std::string& text, uint32_t colour);
    void RegisterRpcs();

    void SendClientJoin(uint32_t challenge);
    void SendRequestClass();
    void SendRequestSpawn();
    void SendSpawn();
    void UpdateLocalPlayer(float dt);
    void InterpolateRemotes(float dt);
    void SendSyncIfDue(double nowMs);

    RemotePlayer* Player(uint16_t id);
    RemotePlayer* EnsurePlayer(uint16_t id);
    const char* NameOf(uint16_t id) const;
    void ResetSession();

    Transport m_net;
    int m_state = kStateIdle;
    std::string m_error;
    std::string m_nick;
    std::string m_host;
    std::string m_userVersion = "0.3.7";
    int m_versionIndex = 0;
    bool m_needStop = false;
    bool m_needReconnect = false;
    unsigned short m_port = 7777;

    uint16_t m_localId = kInvalidPlayerId;
    PlayerSpawnInfo m_spawnInfo = {};

    float m_x = 0.0f, m_y = 0.0f, m_z = 0.0f;
    float m_groundZ = 0.0f;
    float m_vz = 0.0f;
    float m_heading = 0.0f;
    float m_speed = 0.0f;
    float m_cameraYaw = 0.0f;
    int m_health = 100;
    int m_armour = 0;

    float m_inLr = 0.0f;
    float m_inUd = 0.0f;
    bool m_inSprint = false;
    bool m_inJump = false;

    double m_lastTickMs = 0.0;
    double m_lastSyncMs = 0.0;
    double m_connectAtMs = 0.0;
    double m_joinAtMs = 0.0;
    double m_spawnRequestAtMs = 0.0;
    double m_spawnAtMs = 0.0;
    int m_authVariant = 0;
    int m_joinVariant = 0;
    int m_connectAttempts = 0;
    bool m_waitingSpawnReply = false;

    // Auth answers are queued here and flushed from Tick() so that no send ever
    // happens inside a packet callback.
    std::string m_pendingAuthReply;
    // The auth answer sent before the connection is CONNECTED never leaves the
    // box, so remember the challenge and answer again right before ClientJoin.
    std::string m_authChallenge;
    bool m_pendingJoin = false;
    uint32_t m_joinChallenge = 0;
    DialogInfo m_dialog;
    int m_rxLogBudget = 80;

    RemotePlayer m_players[kMaxPlayers];
    std::deque<ChatMessage> m_chat;
    // m_logs is read by the LogServer thread and written by the client thread.
    mutable std::mutex m_logMutex;
    std::deque<std::string> m_logs;
    LogServer m_logServer;
};

}  // namespace samp
