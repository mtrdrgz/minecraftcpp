#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Port of net/minecraft/world/entity/Pose.java (Minecraft Java Edition 26.1.2).
//
// A plain enum (implements StringRepresentable) describing an entity's pose.
// Each constant carries an explicit numeric id and a serialized name string.
//
// Ordinals MATCH the Java declaration order exactly (Pose.java:12-29). In this
// enum the explicit id ctor arg equals the ordinal for every constant (0..17,
// sequential), but both are exposed separately to mirror the Java surface:
//   STANDING(0)..INHALING(17) — Pose.java:12-29
//
// Ported here (verbatim from the Java ctor args at Pose.java:12-29):
//   id()                Pose.java:42-44  (the id field)
//   getSerializedName() Pose.java:46-49  (the name field)
//   ordinal() / name()  java.lang.Enum surface (declaration order / constant name)
//
// NOT ported (hard-absent, no fabrication):
//   BY_ID = ByIdMap.continuous(Pose::id, values(), OutOfBoundsStrategy.ZERO)
//           (Pose.java:31) — index helper; not exposed here.
//   CODEC = StringRepresentable.fromEnum(Pose::values)        (Pose.java:32)
//   STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, Pose::id)    (Pose.java:33)
//   The codec / stream-codec surface is serialization/network-coupled and not
//   exposed here. Only the raw id, ordinal, name and serialized-name are provided.
// ---------------------------------------------------------------------------

namespace mc {

// Java: Pose enum constants, in declaration order (ordinals 0..17).
// (Pose.java:12-29). The enumerator value here is the ordinal; the explicit
// Java id ctor arg happens to equal the ordinal for every constant.
enum class Pose : int32_t {
    STANDING = 0,
    FALL_FLYING = 1,
    SLEEPING = 2,
    SWIMMING = 3,
    SPIN_ATTACK = 4,
    CROUCHING = 5,
    LONG_JUMPING = 6,
    DYING = 7,
    CROAKING = 8,
    USING_TONGUE = 9,
    SITTING = 10,
    ROARING = 11,
    SNIFFING = 12,
    EMERGING = 13,
    DIGGING = 14,
    SLIDING = 15,
    SHOOTING = 16,
    INHALING = 17,
};

inline constexpr int POSE_COUNT = 18;

// Per-constant explicit id — the `id` ctor arg at Pose.java:12-29.
inline constexpr int POSE_ID[POSE_COUNT] = {
    0,  // STANDING
    1,  // FALL_FLYING
    2,  // SLEEPING
    3,  // SWIMMING
    4,  // SPIN_ATTACK
    5,  // CROUCHING
    6,  // LONG_JUMPING
    7,  // DYING
    8,  // CROAKING
    9,  // USING_TONGUE
    10, // SITTING
    11, // ROARING
    12, // SNIFFING
    13, // EMERGING
    14, // DIGGING
    15, // SLIDING
    16, // SHOOTING
    17, // INHALING
};

// Per-constant Java enum-constant name (java.lang.Enum.name(); declaration order).
// (Pose.java:12-29)
inline constexpr const char* POSE_NAME[POSE_COUNT] = {
    "STANDING",     // STANDING
    "FALL_FLYING",  // FALL_FLYING
    "SLEEPING",     // SLEEPING
    "SWIMMING",     // SWIMMING
    "SPIN_ATTACK",  // SPIN_ATTACK
    "CROUCHING",    // CROUCHING
    "LONG_JUMPING", // LONG_JUMPING
    "DYING",        // DYING
    "CROAKING",     // CROAKING
    "USING_TONGUE", // USING_TONGUE
    "SITTING",      // SITTING
    "ROARING",      // ROARING
    "SNIFFING",     // SNIFFING
    "EMERGING",     // EMERGING
    "DIGGING",      // DIGGING
    "SLIDING",      // SLIDING
    "SHOOTING",     // SHOOTING
    "INHALING",     // INHALING
};

// Per-constant serialized name — the `name` ctor arg at Pose.java:12-29.
inline constexpr const char* POSE_SERIALIZED_NAME[POSE_COUNT] = {
    "standing",     // STANDING
    "fall_flying",  // FALL_FLYING
    "sleeping",     // SLEEPING
    "swimming",     // SWIMMING
    "spin_attack",  // SPIN_ATTACK
    "crouching",    // CROUCHING
    "long_jumping", // LONG_JUMPING
    "dying",        // DYING
    "croaking",     // CROAKING
    "using_tongue", // USING_TONGUE
    "sitting",      // SITTING
    "roaring",      // ROARING
    "sniffing",     // SNIFFING
    "emerging",     // EMERGING
    "digging",      // DIGGING
    "sliding",      // SLIDING
    "shooting",     // SHOOTING
    "inhaling",     // INHALING
};

// Java: Pose.id() — Pose.java:42-44.
constexpr int poseId(Pose v) noexcept {
    return POSE_ID[static_cast<int>(v)];
}

// Java: java.lang.Enum.ordinal() — declaration order (== enumerator value).
constexpr int poseOrdinal(Pose v) noexcept {
    return static_cast<int>(v);
}

// Java: java.lang.Enum.name() — the enum-constant identifier.
constexpr const char* poseName(Pose v) noexcept {
    return POSE_NAME[static_cast<int>(v)];
}

// Java: Pose.getSerializedName() — Pose.java:46-49 (the name field).
constexpr const char* poseSerializedName(Pose v) noexcept {
    return POSE_SERIALIZED_NAME[static_cast<int>(v)];
}

} // namespace mc
