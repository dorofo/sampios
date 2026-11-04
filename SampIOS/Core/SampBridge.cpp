#include "SampBridge.h"

#include "SampClient.h"

#include <cstring>
#include <string>

using samp::Client;

namespace {

void CopyString(char* dst, int cap, const std::string& src) {
    if (!dst || cap <= 0) return;
    const int n = static_cast<int>(src.size());
    const int copy = (n < cap - 1) ? n : cap - 1;
    if (copy > 0) std::memcpy(dst, src.data(), static_cast<size_t>(copy));
    dst[copy] = '\0';
}

}  // namespace

extern "C" {

void samp_init(void) {
    Client::Get();
}

int32_t samp_connect(const char* host, int32_t port, const char* nick) {
    if (!host || !nick) return 0;
    return Client::Get().Connect(host, static_cast<unsigned short>(port), nick) ? 1 : 0;
}

void samp_disconnect(void) {
    Client::Get().Disconnect();
}

void samp_tick(void) {
    Client::Get().Tick();
}

void samp_set_input(float lr, float ud, int32_t sprint, int32_t jump) {
    Client::Get().SetInput(lr, ud, sprint != 0, jump != 0);
}

void samp_set_camera_yaw(float degrees) {
    Client::Get().SetCameraYaw(degrees);
}

void samp_chat_send(const char* text) {
    if (text) Client::Get().SendChat(text);
}

void samp_snapshot(SampSnapshot* out) {
    if (!out) return;
    const Client& client = Client::Get();
    out->state = client.State();
    out->localId = client.LocalId();
    out->x = client.X();
    out->y = client.Y();
    out->z = client.Z();
    out->heading = client.Heading();
    out->health = client.Health();
    out->armour = client.Armour();
    out->players = client.PlayerCount();
}

int32_t samp_players(SampRemote* out, int32_t maxCount) {
    if (!out || maxCount <= 0) return 0;
    const samp::RemotePlayer* players = Client::Get().Players();
    int32_t count = 0;
    for (int i = 0; i < samp::kMaxPlayers && count < maxCount; ++i) {
        if (!players[i].active) continue;
        SampRemote& dst = out[count++];
        dst.id = players[i].id;
        std::memcpy(dst.name, players[i].name, sizeof(dst.name));
        dst.name[sizeof(dst.name) - 1] = '\0';
        dst.x = players[i].x;
        dst.y = players[i].y;
        dst.z = players[i].z;
        dst.heading = players[i].heading;
        dst.skin = players[i].skin;
    }
    return count;
}

int32_t samp_poll_chat(char* name, int32_t nameCap, char* text, int32_t textCap,
                       int32_t* kind, uint32_t* colour) {
    samp::ChatMessage message;
    if (!Client::Get().PopChat(message)) return 0;
    CopyString(name, nameCap, message.name);
    CopyString(text, textCap, message.text);
    if (kind) *kind = message.kind;
    if (colour) *colour = message.colour;
    return 1;
}

int32_t samp_poll_log(char* out, int32_t cap) {
    std::string line;
    if (!Client::Get().PopLog(line)) return 0;
    CopyString(out, cap, line);
    return 1;
}

}  // extern "C"
