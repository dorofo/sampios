// SA-MP 0.3.7 wire protocol: RPC ids, packet ids and the payload layouts the
// iOS client needs. Field order is copied from the Android client
// (app/src/main/cpp/samp/net/*), which is the reference implementation for the
// server we run.
#pragma once

#include <stdint.h>

namespace samp {

// ---------------------------------------------------------------- constants
constexpr int kClientVersion = 4057;              // "0.3.7-R5" handshake id
constexpr char kAuthBs[] = "E02262CF28BC542486C558D4BE9EFB716592AFAF8B";
constexpr char kModName[] = "0.3.7";
constexpr int kMaxPlayers = 1000;
constexpr int kMaxNameLen = 24;
constexpr uint16_t kInvalidPlayerId = 0xFFFF;
constexpr uint16_t kInvalidVehicleId = 0xFFFF;

// Outgoing player sync rate. Matches the server's onfoot_rate of 30 Hz.
constexpr int kSyncHz = 30;

// ------------------------------------------------------------------- events
enum State {
    kStateIdle = 0,
    kStateConnecting = 1,
    kStateJoining = 2,
    kStateSpawning = 3,
    kStateInGame = 4,
    kStateError = 5,
};

enum ChatKind {
    kChatChatter = 0,   // RPC_Chat, a player said something
    kChatServer = 1,    // RPC_ClientMessage, server/script message
    kChatInfo = 2,      // client-side note
};

// --------------------------------------------------------------- rpc numbers
// Defined in SampProtocol.cpp; RakNet's RPC API wants a stable int* identity.
extern int RPC_ClientJoin;              // 25
extern int RPC_WorldPlayerAdd;          // 32
extern int RPC_Spawn;                   // 52
extern int RPC_ClientMessage;           // 93
extern int RPC_Chat;                    // 101
extern int RPC_RequestClass;            // 128
extern int RPC_RequestSpawn;            // 129
extern int RPC_ConnectionRejected;      // 130
extern int RPC_ServerJoin;              // 137
extern int RPC_ServerQuit;              // 138
extern int RPC_InitGame;                // 139
extern int RPC_WorldPlayerRemove;       // 163
extern int RPC_ScrSetPlayerPos;         // 12
extern int RPC_GameModeRestart;         // 40
extern int RPC_DialogBox;               // 61
extern int RPC_DialogResponse;          // 62

// A script dialog (RPC 61). Server game modes use these for login/registration.
struct DialogInfo {
    bool active = false;
    uint16_t id = 0;
    uint8_t style = 0;   // 0 msgbox, 1 input, 2 list, 3 password, 4/5 tablist
    char title[256] = {};
    char message[1024] = {};
    char button1[64] = {};
    char button2[64] = {};
};

// ------------------------------------------------------------- packet layouts
#pragma pack(push, 1)

// Written to the server verbatim, exactly like CLocalPlayer::SendOnFootFullSyncData.
// 68 bytes. Note the quaternion order: CQuaternion stores w, x, y, z.
struct OnFootSyncData {
    uint16_t lrAnalog;
    uint16_t udAnalog;
    uint16_t wKeys;
    float pos[3];
    float quat[4];          // w, x, y, z
    uint8_t health;
    uint8_t armour;
    uint8_t weapon;
    uint8_t specialAction;
    float moveSpeed[3];
    float surfOffsets[3];
    uint16_t surfId;
    uint32_t animation;
};

struct PlayerSpawnInfo {
    uint8_t team;
    int32_t skin;
    uint8_t unknown;
    float pos[3];
    float rotation;
    int32_t weapons[3];
    int32_t ammo[3];
};

#pragma pack(pop)

static_assert(sizeof(OnFootSyncData) == 68, "on-foot sync layout drifted");
static_assert(sizeof(PlayerSpawnInfo) == 46, "spawn info layout drifted");

// ------------------------------------------------------------ key bit masks
// GTA SA controller bits used in OnFootSyncData::wKeys.
enum PadKeys : uint16_t {
    KEY_ACTION = 1 << 0,
    KEY_CROUCH = 1 << 1,
    KEY_FIRE = 1 << 2,
    KEY_SPRINT = 1 << 3,       // run
    KEY_SECONDARY_ATTACK = 1 << 4,
    KEY_JUMP = 1 << 5,
};

}  // namespace samp
