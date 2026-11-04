// Outgoing ID_PLAYER_SYNC building, plus the small amount of quaternion maths
// the protocol needs.
//
// GTA world space is used everywhere: +X east, +Y north, +Z up. A heading is a
// counter-clockwise angle in degrees measured from +Y, which is exactly what
// GTA calls the Z rotation of a ped.
#pragma once

#include "SampProtocol.h"

namespace samp {

class Transport;

// Rotation around +Z as a unit quaternion, ordered w, x, y, z to match
// CQuaternion and therefore the sync layout the server expects.
void QuatFromHeading(float headingDeg, float out[4]);

// Inverse of the above for the incoming (normalised) quaternion.
float HeadingFromQuat(float w, float x, float y, float z);

// Sends one ID_PLAYER_SYNC packet (68 byte payload, unreliable sequenced on
// channel 1 - same as the Android client).
void SendOnFootSync(Transport& transport, const OnFootSyncData& data);

}  // namespace samp
