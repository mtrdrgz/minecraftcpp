#pragma once

// 1:1 port of net.minecraft.world.entity.PositionMoveRotation (26.1.2) — the
// record carrying an absolute (position, deltaMovement, yRot, xRot) used by the
// teleport / position-sync packets. This header ports the PURE, dependency-free
// surface of the record:
//
//   • the canonical record (position, deltaMovement, yRot, xRot)
//   • withRotation(yRot, xRot)                      (PositionMoveRotation.java:32-34)
//   • calculateAbsolute(source, change, relatives)  (PositionMoveRotation.java:40-63)
//   • calculateDelta(...)                           (PositionMoveRotation.java:65-67)
//   • the Relative enum + mask packing/unpacking    (Relative.java)
//
// SKIPPED (coupled to live Entity / TeleportTransition / network codec — NOT a
// no-op, simply absent here and listed as unported):
//   • of(Entity)               — needs Entity interpolation / getKnownMovement
//   • of(TeleportTransition)   — needs TeleportTransition
//   • STREAM_CODEC             — needs FriendlyByteBuf / ByteBufCodecs
//
// Everything below is pure arithmetic over the certified Vec3 (xRot/yRot use the
// Mth sin/cos TABLE) plus Mth.clamp(float). (float)Math.toRadians(deg) is the
// exact JDK formula deg / 180.0 * PI evaluated in double then narrowed to float.
//
// Certified by position_move_rotation_parity (tools/PositionMoveRotationParity.java).

#include "../phys/Vec3.h"
#include "../level/levelgen/Mth.h"

#include <set>

namespace mc {

namespace mth = mc::levelgen::mth;

// net.minecraft.world.entity.Relative — bit index per ordinal (Relative.java:11-19).
enum class Relative : int {
    X            = 0,
    Y            = 1,
    Z            = 2,
    Y_ROT        = 3,
    X_ROT        = 4,
    DELTA_X      = 5,
    DELTA_Y      = 6,
    DELTA_Z      = 7,
    ROTATE_DELTA = 8,
};

// 1 << bit (Relative.getMask(), Relative.java:89-91).
inline int relativeMask(Relative r) { return 1 << static_cast<int>(r); }

// Relative.pack(Set<Relative>) — Relative.java:109-117.
inline int relativePack(const std::set<Relative>& set) {
    int result = 0;
    for (Relative r : set) result |= relativeMask(r);
    return result;
}

// Relative.unpack(int) — Relative.java:97-107. Iterates values() in ordinal order.
inline std::set<Relative> relativeUnpack(int value) {
    std::set<Relative> result;
    for (int i = 0; i <= static_cast<int>(Relative::ROTATE_DELTA); ++i) {
        Relative r = static_cast<Relative>(i);
        int mask = relativeMask(r);
        if ((value & mask) == mask) result.insert(r);
    }
    return result;
}

struct PositionMoveRotation {
    Vec3  position{};
    Vec3  deltaMovement{};
    float yRot = 0.0F;
    float xRot = 0.0F;

    PositionMoveRotation() = default;
    PositionMoveRotation(const Vec3& position_, const Vec3& deltaMovement_, float yRot_, float xRot_)
        : position(position_), deltaMovement(deltaMovement_), yRot(yRot_), xRot(xRot_) {}

    // PositionMoveRotation.java:32-34
    PositionMoveRotation withRotation(float newYRot, float newXRot) const {
        return PositionMoveRotation(position, deltaMovement, newYRot, newXRot);
    }

    // java.lang.Math.toRadians(angdeg) = angdeg / 180.0 * PI (JDK), exact double.
    static double toRadians(double angdeg) { return angdeg / 180.0 * 3.141592653589793; }

    // PositionMoveRotation.java:65-67
    static double calculateDelta(double currentDelta, double deltaChange,
                                 const std::set<Relative>& relatives, Relative relative) {
        return relatives.count(relative) ? currentDelta + deltaChange : deltaChange;
    }

    // PositionMoveRotation.java:40-63
    static PositionMoveRotation calculateAbsolute(const PositionMoveRotation& source,
                                                  const PositionMoveRotation& change,
                                                  const std::set<Relative>& relatives) {
        double offsetX = relatives.count(Relative::X) ? source.position.x : 0.0;
        double offsetY = relatives.count(Relative::Y) ? source.position.y : 0.0;
        double offsetZ = relatives.count(Relative::Z) ? source.position.z : 0.0;
        float offsetYRot = relatives.count(Relative::Y_ROT) ? source.yRot : 0.0F;
        float offsetXRot = relatives.count(Relative::X_ROT) ? source.xRot : 0.0F;
        Vec3 absolutePosition(offsetX + change.position.x, offsetY + change.position.y, offsetZ + change.position.z);
        float absoluteYRot = offsetYRot + change.yRot;
        float absoluteXRot = mth::clamp(offsetXRot + change.xRot, -90.0F, 90.0F);
        Vec3 rotatedCurrentMovement = source.deltaMovement;
        if (relatives.count(Relative::ROTATE_DELTA)) {
            float diffYRot = source.yRot - absoluteYRot;
            float diffXRot = source.xRot - absoluteXRot;
            rotatedCurrentMovement = rotatedCurrentMovement.xRot(static_cast<float>(toRadians(diffXRot)));
            rotatedCurrentMovement = rotatedCurrentMovement.yRot(static_cast<float>(toRadians(diffYRot)));
        }

        Vec3 absoluteDeltaMovement(
            calculateDelta(rotatedCurrentMovement.x, change.deltaMovement.x, relatives, Relative::DELTA_X),
            calculateDelta(rotatedCurrentMovement.y, change.deltaMovement.y, relatives, Relative::DELTA_Y),
            calculateDelta(rotatedCurrentMovement.z, change.deltaMovement.z, relatives, Relative::DELTA_Z));
        return PositionMoveRotation(absolutePosition, absoluteDeltaMovement, absoluteYRot, absoluteXRot);
    }
};

} // namespace mc
