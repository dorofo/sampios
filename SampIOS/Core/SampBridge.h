// C interface consumed by the Swift layer. Only plain C types cross this
// boundary; everything is meant to be called from the main thread.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SampSnapshot {
    int32_t state;      // 0 idle, 1 connecting, 2 joining, 3 spawning, 4 in game, 5 error
    int32_t localId;
    float x;
    float y;
    float z;
    float heading;
    int32_t health;
    int32_t armour;
    int32_t players;
} SampSnapshot;

typedef struct SampRemote {
    int32_t id;
    char name[25];
    float x;
    float y;
    float z;
    float heading;
    int32_t skin;
} SampRemote;

void samp_init(void);
int32_t samp_connect(const char* host, int32_t port, const char* nick);
void samp_disconnect(void);
void samp_tick(void);
void samp_set_input(float lr, float ud, int32_t sprint, int32_t jump);
void samp_set_camera_yaw(float degrees);
void samp_chat_send(const char* text);
void samp_snapshot(SampSnapshot* out);
int32_t samp_players(SampRemote* out, int32_t maxCount);

// Pops one queued chat line. Returns 0 when the queue is empty.
int32_t samp_poll_chat(char* name, int32_t nameCap, char* text, int32_t textCap,
                       int32_t* kind, uint32_t* colour);
// Pops one queued diagnostic line. Returns 0 when the queue is empty.
int32_t samp_poll_log(char* out, int32_t cap);

#ifdef __cplusplus
}
#endif
