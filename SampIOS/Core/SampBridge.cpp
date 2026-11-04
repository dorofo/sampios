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
    Client::Get().InitDiagnostics();
}

int32_t samp_connect(const char* host, int32_t port, const char* nick) {
    if (!host || !nick) return 0;
    return Client::Get().Connect(host, static_cast<unsigned short>(port), nick) ? 1 : 0;
}

void samp_set_client_version(const char* version) {
    Client::Get().SetClientVersion(version ? version : "");
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

int32_t samp_log_port(void) {
    return static_cast<int32_t>(Client::Get().LogPort());
}

int32_t samp_dialog(SampDialog* out) {
    if (!out) return 0;
    const Client& client = Client::Get();
    const samp::DialogInfo& dialog = client.Dialog();
    out->active = dialog.active ? 1 : 0;
    out->id = dialog.id;
    out->style = dialog.style;
    std::memcpy(out->title, dialog.title, sizeof(out->title));
    out->title[sizeof(out->title) - 1] = '\0';
    std::memcpy(out->message, dialog.message, sizeof(out->message));
    out->message[sizeof(out->message) - 1] = '\0';
    std::memcpy(out->button1, dialog.button1, sizeof(out->button1));
    out->button1[sizeof(out->button1) - 1] = '\0';
    std::memcpy(out->button2, dialog.button2, sizeof(out->button2));
    out->button2[sizeof(out->button2) - 1] = '\0';
    return out->active;
}

void samp_dialog_response(int32_t button, const char* input, int32_t listItem) {
    Client::Get().RespondDialog(button, input ? input : "", listItem);
}

int32_t samp_poll_log(char* out, int32_t cap) {
    std::string line;
    if (!Client::Get().PopLog(line)) return 0;
    CopyString(out, cap, line);
    return 1;
}

}  // extern "C"
