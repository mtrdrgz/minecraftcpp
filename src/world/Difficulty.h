#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Port of net/minecraft/world/Difficulty.java (Minecraft Java Edition 26.1.2).
//
// A plain enum (implements StringRepresentable) describing the game difficulty.
// Each constant carries an int id and a String key. Declaration order
// (Difficulty.java:13-16):
//   PEACEFUL(0,"peaceful"), EASY(1,"easy"), NORMAL(2,"normal"), HARD(3,"hard")
// For this enum, ordinal() == id (the ctor passes 0..3 in declaration order).
//
// Ported here (verbatim from Difficulty.java):
//   getId()              Difficulty.java:29-31  (the `id` field)
//   byId(int)            Difficulty.java:41-44  via BY_ID = ByIdMap.continuous(
//                          getId, values(), OutOfBoundsStrategy.WRAP)
//                          (Difficulty.java:19). The continuous/WRAP map sorts
//                          values by id (here already 0..3) then indexes with
//                          Mth.positiveModulo(id, length) == Math.floorMod(id, 4)
//                          (ByIdMap.java:66-78, ByIdMap.java:75; Mth.java:165-167).
//   getSerializedName()  Difficulty.java:50-53  (the `key` field). This is also
//                          the only "key" surface — there is no public getKey();
//                          the private `key` field is exposed solely via
//                          getSerializedName(). getDisplayName/getInfo build a
//                          translatable Component from "options.difficulty." + key
//                          (Difficulty.java:33-39) but the key itself is `key`.
//
// NOT ported (hard-absent, no fabrication):
//   CODEC / STREAM_CODEC / BY_ID's IntFunction object (Difficulty.java:18-20),
//   byName(String) (Difficulty.java:46-48 — needs the StringRepresentable
//   EnumCodec), getDisplayName()/getInfo() (Difficulty.java:33-39 — return a
//   network/chat Component). Only the raw id, serialized-name string, and the
//   pure byId(int) lookup are provided.
// ---------------------------------------------------------------------------

namespace mc {

// Java: Difficulty enum constants, in declaration order (ordinals 0..3).
// (Difficulty.java:13-16). For this enum ordinal == id.
enum class Difficulty : int32_t {
    PEACEFUL = 0,
    EASY = 1,
    NORMAL = 2,
    HARD = 3,
};

inline constexpr int DIFFICULTY_COUNT = 4;

// All constants in declaration order, for iteration (Difficulty.values()).
inline constexpr Difficulty DIFFICULTY_VALUES[DIFFICULTY_COUNT] = {
    Difficulty::PEACEFUL,
    Difficulty::EASY,
    Difficulty::NORMAL,
    Difficulty::HARD,
};

// Per-constant id — the `id` ctor arg at Difficulty.java:13-16 (== ordinal here).
inline constexpr int DIFFICULTY_ID[DIFFICULTY_COUNT] = {
    0, // PEACEFUL
    1, // EASY
    2, // NORMAL
    3, // HARD
};

// Per-constant key — the `key` ctor arg at Difficulty.java:13-16.
inline constexpr const char* DIFFICULTY_KEY[DIFFICULTY_COUNT] = {
    "peaceful", // PEACEFUL
    "easy",     // EASY
    "normal",   // NORMAL
    "hard",     // HARD
};

// Java: Difficulty.getId() — Difficulty.java:29-31.
constexpr int difficultyGetId(Difficulty v) noexcept {
    return DIFFICULTY_ID[static_cast<int>(v)];
}

// Java: Difficulty.getSerializedName() — Difficulty.java:50-53 (the `key` field).
// This is also the value of the private `key`; there is no separate getKey().
constexpr const char* difficultySerializedName(Difficulty v) noexcept {
    return DIFFICULTY_KEY[static_cast<int>(v)];
}

// java.lang.Math.floorMod(int,int) — used by Mth.positiveModulo(int,int)
// (Mth.java:165-167), which ByIdMap.continuous/WRAP applies (ByIdMap.java:75).
constexpr int difficultyFloorMod(int a, int b) noexcept {
    int r = a % b;
    if (r != 0 && ((r ^ b) < 0)) r += b;
    return r;
}

// Java: Difficulty.byId(int) — Difficulty.java:41-44.
// BY_ID = ByIdMap.continuous(getId, values(), WRAP) (Difficulty.java:19).
// continuous() builds a sorted-by-id array (here 0..3, already in order) and the
// WRAP strategy returns sortedValues[Mth.positiveModulo(id, length)]
// (ByIdMap.java:66-78). length == 4. Since id == index in the sorted array,
// the result constant has id == floorMod(id, 4).
constexpr Difficulty difficultyById(int id) noexcept {
    return DIFFICULTY_VALUES[difficultyFloorMod(id, DIFFICULTY_COUNT)];
}

} // namespace mc
