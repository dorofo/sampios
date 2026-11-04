#include "SampClient.h"

#include "PlayerSync.h"

#include "BitStream.h"
#include "NetworkTypes.h"
#include "PacketEnumerations.h"
#include "RakClient.h"
#include "StringCompressor.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

// Implemented in vendor/raknet/SAMP/samp_auth.cpp (the 0.3.7 auth response).
void gen_auth_key(char* buf, char* auth_in);

// Build with SAMP_DEBUG_PACKETS=1 to trace every datagram handed to the client.
#ifndef SAMP_DEBUG_PACKETS
#define SAMP_DEBUG_PACKETS 0
#endif

namespace samp {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

double NowMs() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return duration<double, std::milli>(steady_clock::now() - start).count();
}

float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Rotates `from` towards `to` by at most `maxStep` degrees.
float TurnTowards(float from, float to, float maxStep) {
    float delta = to - from;
    while (delta > 180.0f) delta -= 360.0f;
    while (delta < -180.0f) delta += 360.0f;
    if (delta > maxStep) delta = maxStep;
    if (delta < -maxStep) delta = -maxStep;
    float result = from + delta;
    while (result < 0.0f) result += 360.0f;
    while (result >= 360.0f) result -= 360.0f;
    return result;
}

int ByteLen(int bitLength) { return bitLength / 8 + 1; }

void RpcInitGame(RPCParameters* p) {
    Client::Get().OnRpc(RPC_InitGame, p->input, p->numberOfBitsOfData);
}
void RpcChat(RPCParameters* p) {
    Client::Get().OnRpc(RPC_Chat, p->input, p->numberOfBitsOfData);
}
void RpcClientMessage(RPCParameters* p) {
    Client::Get().OnRpc(RPC_ClientMessage, p->input, p->numberOfBitsOfData);
}
void RpcRequestClass(RPCParameters* p) {
    Client::Get().OnRpc(RPC_RequestClass, p->input, p->numberOfBitsOfData);
}
void RpcRequestSpawn(RPCParameters* p) {
    Client::Get().OnRpc(RPC_RequestSpawn, p->input, p->numberOfBitsOfData);
}
void RpcWorldPlayerAdd(RPCParameters* p) {
    Client::Get().OnRpc(RPC_WorldPlayerAdd, p->input, p->numberOfBitsOfData);
}
void RpcWorldPlayerRemove(RPCParameters* p) {
    Client::Get().OnRpc(RPC_WorldPlayerRemove, p->input, p->numberOfBitsOfData);
}
void RpcServerJoin(RPCParameters* p) {
    Client::Get().OnRpc(RPC_ServerJoin, p->input, p->numberOfBitsOfData);
}
void RpcServerQuit(RPCParameters* p) {
    Client::Get().OnRpc(RPC_ServerQuit, p->input, p->numberOfBitsOfData);
}
void RpcConnectionRejected(RPCParameters* p) {
    Client::Get().OnRpc(RPC_ConnectionRejected, p->input, p->numberOfBitsOfData);
}
void RpcSetPlayerPos(RPCParameters* p) {
    Client::Get().OnRpc(RPC_ScrSetPlayerPos, p->input, p->numberOfBitsOfData);
}
void RpcGameModeRestart(RPCParameters* p) {
    Client::Get().OnRpc(RPC_GameModeRestart, p->input, p->numberOfBitsOfData);
}
void RpcDialogBox(RPCParameters* p) {
    Client::Get().OnRpc(RPC_DialogBox, p->input, p->numberOfBitsOfData);
}

}  // namespace

Client& Client::Get() {
    static Client instance;
    return instance;
}

// ----------------------------------------------------------------- lifecycle
bool Client::Connect(const std::string& host, unsigned short port, const std::string& nick) {
    Disconnect();
    ResetSession();

    m_host = host;
    m_port = port;
    m_nick = nick.substr(0, kMaxNameLen);
    if (m_nick.empty()) m_nick = "iosplayer";

    m_connectAttempts = 0;
    if (!m_net.Start(m_host.c_str(), m_port)) {
        SetState(kStateError, "cannot start socket");
        return false;
    }
    RegisterRpcs();
    m_connectAtMs = NowMs();
    m_lastTickMs = m_connectAtMs;
    m_state = kStateConnecting;
    Log("connecting to " + m_host + ":" + std::to_string(m_port) + " as " + m_nick);
    return true;
}

void Client::Disconnect() {
    if (m_net.Active()) {
        m_net.Stop();
        Log("disconnected");
    }
    if (m_state != kStateIdle) SetState(kStateIdle);
}

void Client::RegisterRpcs() {
    RakClient* rc = m_net.Client();
    if (!rc) return;
    rc->RegisterAsRemoteProcedureCall(&RPC_InitGame, RpcInitGame);
    rc->RegisterAsRemoteProcedureCall(&RPC_Chat, RpcChat);
    rc->RegisterAsRemoteProcedureCall(&RPC_ClientMessage, RpcClientMessage);
    rc->RegisterAsRemoteProcedureCall(&RPC_RequestClass, RpcRequestClass);
    rc->RegisterAsRemoteProcedureCall(&RPC_RequestSpawn, RpcRequestSpawn);
    rc->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerAdd, RpcWorldPlayerAdd);
    rc->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerRemove, RpcWorldPlayerRemove);
    rc->RegisterAsRemoteProcedureCall(&RPC_ServerJoin, RpcServerJoin);
    rc->RegisterAsRemoteProcedureCall(&RPC_ServerQuit, RpcServerQuit);
    rc->RegisterAsRemoteProcedureCall(&RPC_ConnectionRejected, RpcConnectionRejected);
    rc->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos, RpcSetPlayerPos);
    rc->RegisterAsRemoteProcedureCall(&RPC_GameModeRestart, RpcGameModeRestart);
    rc->RegisterAsRemoteProcedureCall(&RPC_DialogBox, RpcDialogBox);
}

void Client::ResetSession() {
    m_localId = kInvalidPlayerId;
    m_health = 100;
    m_armour = 0;
    m_vz = 0.0f;
    m_speed = 0.0f;
    m_waitingSpawnReply = false;
    for (auto& player : m_players) player = RemotePlayer{};
    std::memset(&m_spawnInfo, 0, sizeof(m_spawnInfo));
}

void Client::SetState(int state, const char* reason) {
    m_state = state;
    if (reason) {
        m_error = reason;
        Log(std::string("state: ") + reason);
    } else {
        switch (state) {
            case kStateIdle: Log("state: idle"); break;
            case kStateConnecting: Log("state: connecting"); break;
            case kStateJoining: Log("state: joining"); break;
            case kStateSpawning: Log("state: spawning"); break;
            case kStateInGame: Log("state: in game"); break;
            default: break;
        }
    }
}

void Client::Log(const std::string& line) {
    m_logs.push_back(line);
    while (m_logs.size() > 200) m_logs.pop_front();
}

void Client::AddChat(int kind, const std::string& name, const std::string& text, uint32_t colour) {
    ChatMessage msg;
    msg.kind = kind;
    msg.name = name;
    msg.text = text;
    msg.colour = colour;
    m_chat.push_back(std::move(msg));
    while (m_chat.size() > 100) m_chat.pop_front();
}

bool Client::PopChat(ChatMessage& out) {
    if (m_chat.empty()) return false;
    out = m_chat.front();
    m_chat.pop_front();
    return true;
}

bool Client::PopLog(std::string& out) {
    if (m_logs.empty()) return false;
    out = m_logs.front();
    m_logs.pop_front();
    return true;
}

int Client::PlayerCount() const {
    int count = 0;
    for (uint16_t i = 0; i < kMaxPlayers; ++i) {
        if (m_players[i].active) ++count;
    }
    return count;
}

RemotePlayer* Client::Player(uint16_t id) {
    if (id >= kMaxPlayers || !m_players[id].active) return nullptr;
    return &m_players[id];
}

RemotePlayer* Client::EnsurePlayer(uint16_t id) {
    if (id >= kMaxPlayers) return nullptr;
    return &m_players[id];
}

const char* Client::NameOf(uint16_t id) const {
    static char fallback[16];
    if (id < kMaxPlayers && m_players[id].name[0]) return m_players[id].name;
    std::snprintf(fallback, sizeof(fallback), "Player %u", static_cast<unsigned>(id));
    return fallback;
}

// --------------------------------------------------------------------- input
void Client::SetInput(float lr, float ud, bool sprint, bool jump) {
    m_inLr = Clamp(lr, -1.0f, 1.0f);
    m_inUd = Clamp(ud, -1.0f, 1.0f);
    m_inSprint = sprint;
    m_inJump = jump;
}

void Client::SendChat(const std::string& text) {
    if (m_state != kStateInGame || text.empty()) return;
    std::string message = text.substr(0, 128);
    RakNet::BitStream bs;
    bs.Write(static_cast<uint8_t>(message.size()));
    bs.Write(message.c_str(), static_cast<int>(message.size()));
    m_net.Rpc(&RPC_Chat, bs, HIGH_PRIORITY, RELIABLE, 0);
}

// ---------------------------------------------------------------------- tick
void Client::Tick() {
    const double now = NowMs();
    float dt = static_cast<float>((now - m_lastTickMs) / 1000.0);
    if (m_lastTickMs == 0.0) dt = 1.0f / 60.0f;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.25f) dt = 0.25f;  // do not jump after a stall
    m_lastTickMs = now;

    m_net.Pump([this](Packet* packet) { OnPacket(packet); });

    if (!m_pendingAuthReply.empty()) {
        const uint8_t keyLen = static_cast<uint8_t>(m_pendingAuthReply.size());
        RakNet::BitStream out;
        out.Write(static_cast<uint8_t>(ID_AUTH_KEY));
        out.Write(keyLen);
        out.Write(m_pendingAuthReply.c_str(), keyLen);
        m_net.Send(out, SYSTEM_PRIORITY, RELIABLE, 0);
        m_pendingAuthReply.clear();
        Log("auth key answered");
    }

    if (m_state == kStateConnecting && m_connectAttempts < 8 &&
        now - m_connectAtMs > 1500.0) {
        ++m_connectAttempts;
        Log("no answer from server, retry " + std::to_string(m_connectAttempts));
        m_net.Start(m_host.c_str(), m_port);
        RegisterRpcs();
        m_connectAtMs = now;
    }

    if (m_state == kStateSpawning && m_waitingSpawnReply && now - m_spawnRequestAtMs > 4000.0) {
        m_spawnRequestAtMs = now;
        SendRequestSpawn();
    }

    UpdateLocalPlayer(dt);
    InterpolateRemotes(dt);
    SendSyncIfDue(now);
}

void Client::UpdateLocalPlayer(float dt) {
    if (m_state != kStateInGame) return;

    float lr = m_inLr;
    float ud = m_inUd;
    float mag = std::sqrt(lr * lr + ud * ud);
    if (mag > 1.0f) {
        lr /= mag;
        ud /= mag;
        mag = 1.0f;
    }

    const float camRad = m_cameraYaw * kDegToRad;
    // Camera basis in GTA space for a heading of `m_cameraYaw`.
    const float fx = -std::sin(camRad), fy = std::cos(camRad);
    const float rx = std::cos(camRad), ry = std::sin(camRad);

    float mx = fx * ud + rx * lr;
    float my = fy * ud + ry * lr;
    const float len = std::sqrt(mx * mx + my * my);

    float targetSpeed = 0.0f;
    if (len > 0.02f) {
        mx /= len;
        my /= len;
        targetSpeed = m_inSprint ? 7.0f : (mag < 0.7f ? 2.0f : 4.6f);
        m_heading = TurnTowards(m_heading, std::atan2(-mx, my) * kRadToDeg, 540.0f * dt);
        m_x += mx * targetSpeed * dt;
        m_y += my * targetSpeed * dt;
    }

    // Simple linear ramp so the remote ped animation does not snap.
    const float accel = 24.0f * dt;
    if (m_speed < targetSpeed) m_speed = std::fmin(targetSpeed, m_speed + accel);
    else m_speed = std::fmax(targetSpeed, m_speed - accel);
    if (len <= 0.02f) {
        mx = -std::sin(m_heading * kDegToRad);
        my = std::cos(m_heading * kDegToRad);
    }

    if (m_inJump && m_z <= m_groundZ + 0.001f && m_vz <= 0.0f) {
        m_vz = 5.4f;
    }
    if (m_z > m_groundZ || m_vz != 0.0f) {
        m_vz -= 14.0f * dt;
        m_z += m_vz * dt;
        if (m_z <= m_groundZ) {
            m_z = m_groundZ;
            m_vz = 0.0f;
        }
    }
}

void Client::InterpolateRemotes(float dt) {
    const float factor = 1.0f - std::exp(-dt * 12.0f);
    for (auto& player : m_players) {
        if (!player.active) continue;
        player.x += (player.targetX - player.x) * factor;
        player.y += (player.targetY - player.y) * factor;
        player.z += (player.targetZ - player.z) * factor;
        float delta = player.targetHeading - player.heading;
        while (delta > 180.0f) delta -= 360.0f;
        while (delta < -180.0f) delta += 360.0f;
        player.heading += delta * factor;
    }
}

void Client::SendSyncIfDue(double nowMs) {
    if (m_state != kStateInGame) return;
    const double interval = 1000.0 / kSyncHz;
    if (nowMs - m_lastSyncMs < interval) return;
    m_lastSyncMs = nowMs;

    OnFootSyncData sync{};
    sync.lrAnalog = static_cast<uint16_t>(static_cast<int16_t>(m_inLr * 32767.0f));
    sync.udAnalog = static_cast<uint16_t>(static_cast<int16_t>(-m_inUd * 32767.0f));
    sync.wKeys = 0;
    if (m_inSprint && m_speed > 3.0f) sync.wKeys |= KEY_SPRINT;
    if (m_z > m_groundZ + 0.01f) sync.wKeys |= KEY_JUMP;

    sync.pos[0] = m_x;
    sync.pos[1] = m_y;
    sync.pos[2] = m_z;
    QuatFromHeading(m_heading, sync.quat);

    sync.health = static_cast<uint8_t>(m_health);
    sync.armour = static_cast<uint8_t>(m_armour);
    sync.weapon = 0;
    sync.specialAction = 0;

    sync.moveSpeed[0] = -std::sin(m_heading * kDegToRad) * m_speed;
    sync.moveSpeed[1] = std::cos(m_heading * kDegToRad) * m_speed;
    sync.moveSpeed[2] = m_vz;
    sync.surfId = kInvalidVehicleId;
    sync.animation = 0x80000000u;

    SendOnFootSync(m_net, sync);
}

// ------------------------------------------------------------------- packets
void Client::OnPacket(Packet* packet) {
    if (!packet || !packet->data || packet->length == 0) return;

    const unsigned char* data = packet->data;
    int length = static_cast<int>(packet->length);
    unsigned char id = data[0];
    if (id == ID_TIMESTAMP && length > 1 + static_cast<int>(sizeof(RakNetTime))) {
        data += 1 + sizeof(RakNetTime);
        length -= 1 + static_cast<int>(sizeof(RakNetTime));
        id = data[0];
    }

    // Keep a short packet trail in the log so a device session can be diagnosed
    // from the in-app console.
    if (m_rxLogBudget > 0) {
        --m_rxLogBudget;
        char line[64];
        std::snprintf(line, sizeof(line), "rx id=%u len=%d connected=%d",
                      static_cast<unsigned>(id), length, m_net.Connected() ? 1 : 0);
        Log(line);
    }

#if SAMP_DEBUG_PACKETS
    {
        char hex[3 * 24 + 1] = {};
        const int shown = length < 24 ? length : 24;
        for (int i = 0; i < shown; ++i) {
            std::snprintf(hex + i * 3, 4, "%02X ", data[i]);
        }
        char line[160];
        std::snprintf(line, sizeof(line), "rx id=%u len=%d [%s]", static_cast<unsigned>(id),
                      length, hex);
        Log(line);
    }
#endif

    switch (id) {
        case ID_AUTH_KEY: {
            // SA-MP challenge/response: the server sends a random string, we
            // answer with the 0.3.7 auth key derived from it.
            RakNet::BitStream bs(const_cast<unsigned char*>(data), length, false);
            uint8_t len = 0;
            char challenge[264] = {};
            bs.IgnoreBits(8);
            bs.Read(len);
            if (len == 0 || len >= sizeof(challenge)) return;
            bs.Read(challenge, len);
            challenge[len] = '\0';

            char key[269] = {};
            gen_auth_key(key, challenge);
            m_pendingAuthReply.assign(key);
            Log("auth key computed");
            break;
        }
        case ID_CONNECTION_REQUEST_ACCEPTED: {
            RakNet::BitStream bs(packet->data, packet->length, false);
            uint16_t playerId = 0;
            uint32_t challenge = 0;
            bs.IgnoreBits(8);
            bs.IgnoreBits(32);
            bs.IgnoreBits(16);
            if (!bs.Read(playerId)) return;
            if (!bs.Read(challenge)) return;
            challenge ^= static_cast<uint32_t>(kClientVersion);
            m_localId = playerId;
            SetState(kStateJoining);
            Log("handshake ok, local id " + std::to_string(playerId));
            SendClientJoin(challenge);
            m_joinAtMs = NowMs();
            break;
        }
        case ID_DISCONNECTION_NOTIFICATION:
            SetState(kStateError, "server closed the connection");
            m_net.Stop();
            break;
        case ID_CONNECTION_LOST:
            SetState(kStateError, "connection lost");
            m_net.Stop();
            break;
        case ID_CONNECTION_ATTEMPT_FAILED:
            Log("connection attempt failed");
            m_connectAtMs = 0.0;  // retry immediately in Tick
            break;
        case ID_NO_FREE_INCOMING_CONNECTIONS:
            SetState(kStateError, "server is full");
            m_net.Stop();
            break;
        case ID_CONNECTION_BANNED:
            SetState(kStateError, "you are banned");
            m_net.Stop();
            break;
        case ID_INVALID_PASSWORD:
            SetState(kStateError, "invalid server password");
            m_net.Stop();
            break;
        case ID_PLAYER_SYNC:
            if (m_state == kStateInGame) {
                RakNet::BitStream bs(const_cast<unsigned char*>(data), length, false);
                uint8_t packetId = 0;
                uint16_t playerId = 0;
                bs.Read(packetId);
                if (!bs.Read(playerId)) return;
                RemotePlayer* player = Player(playerId);
                if (!player) return;

                bool hasLR = false, hasUD = false;
                uint16_t lr = 0, ud = 0, keys = 0;
                float pos[3] = {0, 0, 0};
                float w = 1, x = 0, y = 0, z = 0;
                uint8_t healthArmour = 0, weapon = 0, specialAction = 0;
                float speed[3] = {0, 0, 0};

                bs.Read(hasLR);
                if (hasLR) bs.Read(lr);
                bs.Read(hasUD);
                if (hasUD) bs.Read(ud);
                bs.Read(keys);
                bs.Read(reinterpret_cast<char*>(pos), sizeof(pos));
                bs.ReadNormQuat(w, x, y, z);
                bs.Read(healthArmour);
                bs.Read(weapon);
                bs.Read(specialAction);
                bs.ReadVector(speed[0], speed[1], speed[2]);

                player->targetX = pos[0];
                player->targetY = pos[1];
                player->targetZ = pos[2];
                player->targetHeading = HeadingFromQuat(w, x, y, z);
            }
            break;
        default:
            break;
    }
}

void Client::OnRpc(int rpcId, const unsigned char* data, int bitLength) {
    if (!data || bitLength <= 0) return;
    RakNet::BitStream bs(const_cast<unsigned char*>(data), ByteLen(bitLength), false);

    switch (rpcId) {
        case 139: {  // RPC_InitGame
            bool flag = false;
            float fvalue = 0.0f;
            int ivalue = 0;
            uint8_t bvalue = 0;
            uint16_t localId = 0;

            bs.ReadCompressed(flag);            // zone names
            bs.ReadCompressed(flag);            // CJ walk
            bs.ReadCompressed(flag);            // allow weapons
            bs.ReadCompressed(flag);            // limit global chat radius
            bs.Read(fvalue);                    // global chat radius
            bs.ReadCompressed(flag);            // stunt bonus
            bs.Read(fvalue);                    // nametag draw distance
            bs.ReadCompressed(flag);            // disable interior enter/exit
            bs.ReadCompressed(flag);            // nametag line of sight
            bs.ReadCompressed(flag);            // manual engine/lights
            bs.Read(ivalue);                    // spawns available
            bs.Read(localId);                   // our player id
            bs.ReadCompressed(flag);            // show nametags
            bs.Read(ivalue);                    // show player markers
            bs.Read(bvalue);                    // world hour
            bs.Read(bvalue);                    // weather
            bs.Read(fvalue);                    // gravity
            bs.ReadCompressed(flag);            // lan mode
            bs.Read(ivalue);                    // death drop money
            bs.ReadCompressed(flag);            // instagib
            bs.Read(ivalue);                    // onfoot send rate
            bs.Read(ivalue);                    // incar send rate
            bs.Read(ivalue);                    // firing send rate
            bs.Read(ivalue);                    // send multiplier
            bs.Read(ivalue);                    // lag compensation

            uint8_t nameLen = 0;
            bs.Read(nameLen);
            char hostName[257] = {};
            if (nameLen > 0 && nameLen < sizeof(hostName)) bs.Read(hostName, nameLen);
            hostName[nameLen] = '\0';

            char models[212] = {};
            bs.Read(models, sizeof(models));

            m_localId = localId;
            SetState(kStateSpawning);
            Log(std::string("joined server: ") + hostName);
            AddChat(kChatInfo, "", std::string("Подключено к ") + hostName, 0xFFB9C9BF);
            SendRequestClass();
            break;
        }
        case 128: {  // RPC_RequestClass
            uint8_t outcome = 0;
            bs.Read(outcome);
            if (outcome) {
                bs.Read(reinterpret_cast<char*>(&m_spawnInfo), sizeof(m_spawnInfo));
                Log("class selected, skin " + std::to_string(m_spawnInfo.skin));
                SendRequestSpawn();
            } else {
                Log("server refused class selection");
                SetState(kStateError, "server refused class selection");
            }
            break;
        }
        case 129: {  // RPC_RequestSpawn
            uint8_t outcome = 0;
            bs.Read(outcome);
            if (outcome == 2 || (outcome && m_waitingSpawnReply)) {
                SendSpawn();
            } else {
                m_waitingSpawnReply = false;
                Log("server refused spawn");
            }
            break;
        }
        case 101: {  // RPC_Chat
            uint16_t playerId = kInvalidPlayerId;
            uint8_t len = 0;
            char text[258] = {};
            bs.Read(playerId);
            bs.Read(len);
            if (len > 0 && len < sizeof(text)) bs.Read(text, len);
            text[len] = '\0';
            if (playerId == m_localId) {
                AddChat(kChatChatter, m_nick, text, 0xFFFFFFFF);
            } else {
                AddChat(kChatChatter, NameOf(playerId), text, 0xFFE0E0E0);
            }
            break;
        }
        case 93: {  // RPC_ClientMessage
            uint32_t colour = 0xFFFFFFFF;
            uint32_t len = 0;
            char text[258] = {};
            bs.Read(colour);
            bs.Read(len);
            if (len > 0 && len < sizeof(text)) bs.Read(text, len);
            text[len < sizeof(text) ? len : 0] = '\0';
            AddChat(kChatServer, "", text, colour);
            break;
        }
        case 32: {  // RPC_WorldPlayerAdd
            uint16_t playerId = kInvalidPlayerId;
            uint8_t team = 0;
            int32_t skin = 0;
            float pos[3] = {0, 0, 0};
            float rotation = 0.0f;
            uint32_t colour = 0xFFFFFFFF;
            uint8_t fightingStyle = 0;
            uint16_t unknown[11] = {};

            bs.Read(playerId);
            bs.Read(team);
            bs.Read(skin);
            bs.Read(reinterpret_cast<char*>(pos), sizeof(pos));
            bs.Read(rotation);
            bs.Read(colour);
            bs.Read(fightingStyle);
            bs.Read(reinterpret_cast<char*>(unknown), sizeof(unknown));

            RemotePlayer* player = EnsurePlayer(playerId);
            if (!player) return;
            player->active = true;
            player->id = playerId;
            player->skin = skin;
            player->colour = colour;
            player->targetX = player->x = pos[0];
            player->targetY = player->y = pos[1];
            player->targetZ = player->z = pos[2];
            player->targetHeading = player->heading = rotation;
            break;
        }
        case 163: {  // RPC_WorldPlayerRemove
            uint16_t playerId = kInvalidPlayerId;
            bs.Read(playerId);
            if (playerId < kMaxPlayers) m_players[playerId] = RemotePlayer{};
            break;
        }
        case 137: {  // RPC_ServerJoin
            uint16_t playerId = kInvalidPlayerId;
            uint32_t colour = 0xFFFFFFFF;
            uint8_t isNpc = 0;
            uint8_t len = 0;
            char name[64] = {};
            bs.Read(playerId);
            bs.Read(colour);
            bs.Read(isNpc);
            bs.Read(len);
            if (len > 0 && len < sizeof(name)) bs.Read(name, len);
            name[len < sizeof(name) ? len : 0] = '\0';
            RemotePlayer* player = EnsurePlayer(playerId);
            if (player) {
                player->id = playerId;
                player->colour = colour;
                std::strncpy(player->name, name, kMaxNameLen);
                player->name[kMaxNameLen] = '\0';
            }
            break;
        }
        case 138: {  // RPC_ServerQuit
            uint16_t playerId = kInvalidPlayerId;
            uint8_t reason = 0;
            bs.Read(playerId);
            bs.Read(reason);
            if (playerId < kMaxPlayers) m_players[playerId] = RemotePlayer{};
            break;
        }
        case 12: {  // RPC_ScrSetPlayerPos
            uint16_t playerId = kInvalidPlayerId;
            float pos[3] = {0, 0, 0};
            bs.Read(playerId);
            bs.Read(reinterpret_cast<char*>(pos), sizeof(pos));
            if (playerId == m_localId) {
                m_x = pos[0];
                m_y = pos[1];
                m_z = pos[2];
                m_groundZ = pos[2];
                m_vz = 0.0f;
                Log("teleported by server");
            } else if (RemotePlayer* player = Player(playerId)) {
                player->targetX = pos[0];
                player->targetY = pos[1];
                player->targetZ = pos[2];
            }
            break;
        }
        case 130: {  // RPC_ConnectionRejected
            uint8_t len = 0;
            char reason[256] = {};
            bs.Read(len);
            if (len > 0 && len < sizeof(reason)) bs.Read(reason, len);
            reason[len] = '\0';
            SetState(kStateError, reason[0] ? reason : "connection rejected");
            m_net.Stop();
            break;
        }
        case 61: {  // RPC_DialogBox
            DialogInfo dialog;
            uint8_t len = 0;
            bs.Read(dialog.id);
            bs.Read(dialog.style);

            bs.Read(len);
            if (len > 0 && len < sizeof(dialog.title)) bs.Read(dialog.title, len);
            bs.Read(len);
            if (len > 0 && len < sizeof(dialog.button1)) bs.Read(dialog.button1, len);
            bs.Read(len);
            if (len > 0 && len < sizeof(dialog.button2)) bs.Read(dialog.button2, len);

            char message[1024] = {};
            StringCompressor::Instance()->DecodeString(message, sizeof(message) - 1, &bs);
            std::strncpy(dialog.message, message, sizeof(dialog.message) - 1);

            dialog.active = true;
            m_dialog = dialog;
            Log("dialog " + std::to_string(dialog.id) + " style " + std::to_string(dialog.style));
            break;
        }
        case 40: {  // RPC_GameModeRestart
            Log("gamemode restart, reconnecting");
            AddChat(kChatInfo, "", "Сервер перезапускается, подключаемся заново", 0xFFFFC000);
            ResetSession();
            m_connectAttempts = 0;
            m_net.Start(m_host.c_str(), m_port);
            RegisterRpcs();
            m_connectAtMs = 0.0;
            m_state = kStateConnecting;
            break;
        }
        default:
            break;
    }
}

// ------------------------------------------------------------------- dialogs
void Client::RespondDialog(int button, const std::string& input, int listItem) {
    if (!m_dialog.active) return;

    RakNet::BitStream bs;
    bs.Write(m_dialog.id);
    bs.Write(static_cast<uint8_t>(button));
    bs.Write(static_cast<int16_t>(listItem));
    const uint8_t len = static_cast<uint8_t>(input.size() > 250 ? 250 : input.size());
    bs.Write(len);
    bs.Write(input.c_str(), len);
    m_net.Rpc(&RPC_DialogResponse, bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0);

    Log("dialog " + std::to_string(m_dialog.id) + " answered with button " +
        std::to_string(button));
    m_dialog.active = false;
}

// ----------------------------------------------------------------- handshake
void Client::SendClientJoin(uint32_t challenge) {
    char authKey[269] = {};
    gen_auth_key(authKey, const_cast<char*>(kAuthBs));

    const uint8_t nameLen = static_cast<uint8_t>(m_nick.size());
    const uint8_t authLen = static_cast<uint8_t>(std::strlen(authKey));
    const uint8_t modLen = static_cast<uint8_t>(std::strlen(kModName));

    RakNet::BitStream bs;
    bs.Write(static_cast<int>(kClientVersion));
    bs.Write(static_cast<uint8_t>(1));  // mod: 1 = SA-MP
    bs.Write(nameLen);
    bs.Write(m_nick.c_str(), nameLen);
    bs.Write(challenge);
    bs.Write(authLen);
    bs.Write(authKey, authLen);
    bs.Write(modLen);
    bs.Write(kModName, modLen);

    m_net.Rpc(&RPC_ClientJoin, bs, HIGH_PRIORITY, RELIABLE, 0);
    Log("ClientJoin sent");
}

void Client::SendRequestClass() {
    RakNet::BitStream bs;
    bs.Write(static_cast<int>(0));
    m_net.Rpc(&RPC_RequestClass, bs, HIGH_PRIORITY, RELIABLE, 0);
    Log("requested class 0");
}

void Client::SendRequestSpawn() {
    RakNet::BitStream bs;
    m_net.Rpc(&RPC_RequestSpawn, bs, HIGH_PRIORITY, RELIABLE, 0);
    m_waitingSpawnReply = true;
    m_spawnRequestAtMs = NowMs();
    Log("requested spawn");
}

void Client::SendSpawn() {
    m_waitingSpawnReply = false;

    m_x = m_spawnInfo.pos[0];
    m_y = m_spawnInfo.pos[1];
    m_z = m_spawnInfo.pos[2];
    m_groundZ = m_spawnInfo.pos[2];
    m_vz = 0.0f;
    m_heading = m_spawnInfo.rotation;
    m_cameraYaw = m_heading;
    m_health = 100;
    m_armour = 0;
    m_speed = 0.0f;
    m_lastSyncMs = 0.0;

    RakNet::BitStream bs;
    m_net.Rpc(&RPC_Spawn, bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0);

    SetState(kStateInGame);
    Log("spawned at " + std::to_string(m_x) + ", " + std::to_string(m_y) + ", " +
        std::to_string(m_z));
    AddChat(kChatInfo, "", "Спавн выполнен", 0xFFB9C9BF);
}

}  // namespace samp
