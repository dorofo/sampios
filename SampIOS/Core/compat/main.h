// Minimal stand-in for the Android project's samp/main.h.
//
// RakNet's SocketLayer.cpp / RakPeer.cpp / ReliabilityLayer.cpp include "main.h"
// for the SA-MP client configuration (RAKSAMP_CLIENT switches on the SA-MP
// datagram encryption in SocketLayer and the connection-cookie hack in
// RakPeer). The Android tree satisfies that include with the game-wide header;
// on iOS we only need the defines, so this file keeps the RakNet build
// independent from any engine/Android headers.
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define RAKSAMP_CLIENT 1
#define NETCODE_CONNCOOKIELULZ 0x6969
#define SAMP_VERSION "0.3.7"

// samp_auth.cpp picks its SHA1 word size with #ifdef. The Android arm64 build
// passes -DVER_x32 and therefore runs the 32-bit code path; keep it identical
// so the generated auth key matches what the server expects.
#define VER_x32 1
