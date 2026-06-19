#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Port of net/minecraft/world/entity/MobCategory.java (Minecraft Java Edition 26.1.2).
//
// A plain enum (implements StringRepresentable) describing the spawn category of a
// mob type. Each constant carries: serialized name, max-instances-per-chunk, an
// isFriendly flag, an isPersistent flag, and a despawn distance. noDespawnDistance
// is a constant 32 for every category.
//
// Ordinals MATCH the Java declaration order exactly (MobCategory.java:7-14):
//   MONSTER=0, CREATURE=1, AMBIENT=2, AXOLOTLS=3,
//   UNDERGROUND_WATER_CREATURE=4, WATER_CREATURE=5, WATER_AMBIENT=6, MISC=7
//
// Ported here (verbatim from the Java ctor args at MobCategory.java:7-14):
//   getName()                  MobCategory.java:32-34  (the name field)
//   getSerializedName()        MobCategory.java:36-39  (same name field)
//   getMaxInstancesPerChunk()  MobCategory.java:41-43  (the max field)
//   isFriendly()               MobCategory.java:45-47  (the isFriendly field)
//   isPersistent()             MobCategory.java:49-51  (the isPersistent field)
//   getDespawnDistance()       MobCategory.java:53-55  (the despawnDistance field)
//   getNoDespawnDistance()     MobCategory.java:57-59  (constant 32, MobCategory.java:21)
//
// NOT ported (hard-absent, no fabrication):
//   CODEC = StringRepresentable.fromEnum(...) (MobCategory.java:16) — the codec/keyable
//   surface is registry/serialization-coupled and not exposed here. Only the raw
//   serialized-name string is provided.
// ---------------------------------------------------------------------------

namespace mc {

// Java: MobCategory enum constants, in declaration order (ordinals 0..7).
// (MobCategory.java:7-14)
enum class MobCategory : int32_t {
    MONSTER = 0,
    CREATURE = 1,
    AMBIENT = 2,
    AXOLOTLS = 3,
    UNDERGROUND_WATER_CREATURE = 4,
    WATER_CREATURE = 5,
    WATER_AMBIENT = 6,
    MISC = 7,
};

inline constexpr int MOB_CATEGORY_COUNT = 8;

// Per-constant serialized name — the name field at MobCategory.java:7-14.
inline constexpr const char* MOB_CATEGORY_NAME[MOB_CATEGORY_COUNT] = {
    "monster",                    // MONSTER
    "creature",                   // CREATURE
    "ambient",                    // AMBIENT
    "axolotls",                   // AXOLOTLS
    "underground_water_creature", // UNDERGROUND_WATER_CREATURE
    "water_creature",             // WATER_CREATURE
    "water_ambient",              // WATER_AMBIENT
    "misc",                       // MISC
};

// Per-constant max-instances-per-chunk — the `max` ctor arg at MobCategory.java:7-14.
inline constexpr int MOB_CATEGORY_MAX[MOB_CATEGORY_COUNT] = {
    70, // MONSTER
    10, // CREATURE
    15, // AMBIENT
    5,  // AXOLOTLS
    5,  // UNDERGROUND_WATER_CREATURE
    5,  // WATER_CREATURE
    20, // WATER_AMBIENT
    -1, // MISC
};

// Per-constant isFriendly flag — the `isFriendly` ctor arg at MobCategory.java:7-14.
inline constexpr bool MOB_CATEGORY_IS_FRIENDLY[MOB_CATEGORY_COUNT] = {
    false, // MONSTER
    true,  // CREATURE
    true,  // AMBIENT
    true,  // AXOLOTLS
    true,  // UNDERGROUND_WATER_CREATURE
    true,  // WATER_CREATURE
    true,  // WATER_AMBIENT
    true,  // MISC
};

// Per-constant isPersistent flag — the `isPersistent` ctor arg at MobCategory.java:7-14.
inline constexpr bool MOB_CATEGORY_IS_PERSISTENT[MOB_CATEGORY_COUNT] = {
    false, // MONSTER
    true,  // CREATURE
    false, // AMBIENT
    false, // AXOLOTLS
    false, // UNDERGROUND_WATER_CREATURE
    false, // WATER_CREATURE
    false, // WATER_AMBIENT
    true,  // MISC
};

// Per-constant despawn distance — the `despawnDistance` ctor arg at MobCategory.java:7-14.
inline constexpr int MOB_CATEGORY_DESPAWN_DISTANCE[MOB_CATEGORY_COUNT] = {
    128, // MONSTER
    128, // CREATURE
    128, // AMBIENT
    128, // AXOLOTLS
    128, // UNDERGROUND_WATER_CREATURE
    128, // WATER_CREATURE
    64,  // WATER_AMBIENT
    128, // MISC
};

// Java: MobCategory.noDespawnDistance = 32, constant for every category
// (MobCategory.java:21, returned by getNoDespawnDistance() at MobCategory.java:57-59).
inline constexpr int MOB_CATEGORY_NO_DESPAWN_DISTANCE = 32;

// Java: MobCategory.getName() — MobCategory.java:32-34.
constexpr const char* mobCategoryGetName(MobCategory v) noexcept {
    return MOB_CATEGORY_NAME[static_cast<int>(v)];
}

// Java: MobCategory.getSerializedName() — MobCategory.java:36-39 (same name field).
constexpr const char* mobCategorySerializedName(MobCategory v) noexcept {
    return MOB_CATEGORY_NAME[static_cast<int>(v)];
}

// Java: MobCategory.getMaxInstancesPerChunk() — MobCategory.java:41-43.
constexpr int mobCategoryGetMaxInstancesPerChunk(MobCategory v) noexcept {
    return MOB_CATEGORY_MAX[static_cast<int>(v)];
}

// Java: MobCategory.isFriendly() — MobCategory.java:45-47.
constexpr bool mobCategoryIsFriendly(MobCategory v) noexcept {
    return MOB_CATEGORY_IS_FRIENDLY[static_cast<int>(v)];
}

// Java: MobCategory.isPersistent() — MobCategory.java:49-51.
constexpr bool mobCategoryIsPersistent(MobCategory v) noexcept {
    return MOB_CATEGORY_IS_PERSISTENT[static_cast<int>(v)];
}

// Java: MobCategory.getDespawnDistance() — MobCategory.java:53-55.
constexpr int mobCategoryGetDespawnDistance(MobCategory v) noexcept {
    return MOB_CATEGORY_DESPAWN_DISTANCE[static_cast<int>(v)];
}

// Java: MobCategory.getNoDespawnDistance() — MobCategory.java:57-59 (returns 32).
constexpr int mobCategoryGetNoDespawnDistance(MobCategory /*v*/) noexcept {
    return MOB_CATEGORY_NO_DESPAWN_DISTANCE;
}

} // namespace mc
