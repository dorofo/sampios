#include "PlayerSync.h"

#include "RakTransport.h"

#include "BitStream.h"
#include "PacketEnumerations.h"

#include <cmath>

namespace samp {
namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;
}  // namespace

void QuatFromHeading(float headingDeg, float out[4]) {
    // CQuaternion::SetFromMatrix on a pure Z rotation yields
    // (w, x, y, z) = (cos(a/2), 0, 0, sin(a/2)).
    const float half = headingDeg * kDegToRad * 0.5f;
    out[0] = std::cos(half);  // w
    out[1] = 0.0f;            // x
    out[2] = 0.0f;            // y
    out[3] = std::sin(half);  // z
}

float HeadingFromQuat(float w, float x, float y, float z) {
    (void)x;
    (void)y;
    float heading = 2.0f * std::atan2(z, w) * kRadToDeg;
    while (heading < 0.0f) heading += 360.0f;
    while (heading >= 360.0f) heading -= 360.0f;
    return heading;
}

void SendOnFootSync(Transport& transport, const OnFootSyncData& data) {
    RakNet::BitStream bs;
    bs.Write(static_cast<uint8_t>(ID_PLAYER_SYNC));
    bs.Write(reinterpret_cast<const char*>(&data), sizeof(data));
    transport.Send(bs, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 1);
}

}  // namespace samp
