#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Port of net/minecraft/world/level/block/state/properties/NoteBlockInstrument.java
// (Minecraft Java Edition 26.1.2).
//
// A StringRepresentable enum of note-block instruments. Each constant carries a
// serialized name, a SoundEvent holder, and a private Type {BASE_BLOCK, MOB_HEAD,
// CUSTOM}. Ordinals MATCH the Java declaration order exactly
// (NoteBlockInstrument.java:9-35).
//
// Ported here (verbatim from the Java):
//   getSerializedName()    NoteBlockInstrument.java:47-50 (the name field)
//   isTunable()            NoteBlockInstrument.java:56-58 (type == BASE_BLOCK)
//   hasCustomSound()       NoteBlockInstrument.java:60-62 (type == CUSTOM)
//   worksAboveNoteBlock()  NoteBlockInstrument.java:64-66 (type != BASE_BLOCK)
//
// NOT ported (registry/sound coupled — deliberately ABSENT, not faked):
//   getSoundEvent()  — returns Holder<SoundEvent>, requires the sound registry.
//   the StringRepresentable CODEC / keyable surface. Only the raw serialized-name
//   string and the Type-derived predicates are exposed.
//
// The private nested Type enum is mirrored here only so the predicates read 1:1;
// it is not part of any public API in Java.
// ---------------------------------------------------------------------------

namespace mc::block::state::properties {

// Java: NoteBlockInstrument.Type — private enum {BASE_BLOCK, MOB_HEAD, CUSTOM}
// (NoteBlockInstrument.java:68-72). Ordinals in declaration order.
enum class NoteBlockInstrumentType : int32_t {
    BASE_BLOCK = 0,
    MOB_HEAD = 1,
    CUSTOM = 2,
};

// Java: NoteBlockInstrument enum constants, in declaration order (ordinals 0..27).
// (NoteBlockInstrument.java:9-35)
enum class NoteBlockInstrument : int32_t {
    HARP = 0,
    BASEDRUM = 1,
    SNARE = 2,
    HAT = 3,
    BASS = 4,
    FLUTE = 5,
    BELL = 6,
    GUITAR = 7,
    CHIME = 8,
    XYLOPHONE = 9,
    IRON_XYLOPHONE = 10,
    COW_BELL = 11,
    DIDGERIDOO = 12,
    BIT = 13,
    BANJO = 14,
    PLING = 15,
    TRUMPET = 16,
    TRUMPET_EXPOSED = 17,
    TRUMPET_OXIDIZED = 18,
    TRUMPET_WEATHERED = 19,
    ZOMBIE = 20,
    SKELETON = 21,
    CREEPER = 22,
    DRAGON = 23,
    WITHER_SKELETON = 24,
    PIGLIN = 25,
    CUSTOM_HEAD = 26,
};

inline constexpr int NOTE_BLOCK_INSTRUMENT_COUNT = 27;

// Per-constant serialized name — the 1st ctor arg at NoteBlockInstrument.java:9-35.
// Indexed by ordinal.
inline constexpr const char* NOTE_BLOCK_INSTRUMENT_NAME[NOTE_BLOCK_INSTRUMENT_COUNT] = {
    "harp",              // HARP
    "basedrum",          // BASEDRUM
    "snare",             // SNARE
    "hat",               // HAT
    "bass",              // BASS
    "flute",             // FLUTE
    "bell",              // BELL
    "guitar",            // GUITAR
    "chime",             // CHIME
    "xylophone",         // XYLOPHONE
    "iron_xylophone",    // IRON_XYLOPHONE
    "cow_bell",          // COW_BELL
    "didgeridoo",        // DIDGERIDOO
    "bit",               // BIT
    "banjo",             // BANJO
    "pling",             // PLING
    "trumpet",           // TRUMPET
    "trumpet_exposed",   // TRUMPET_EXPOSED
    "trumpet_oxidized",  // TRUMPET_OXIDIZED
    "trumpet_weathered", // TRUMPET_WEATHERED
    "zombie",            // ZOMBIE
    "skeleton",          // SKELETON
    "creeper",           // CREEPER
    "dragon",            // DRAGON
    "wither_skeleton",   // WITHER_SKELETON
    "piglin",            // PIGLIN
    "custom_head",       // CUSTOM_HEAD
};

// Per-constant Type — the 3rd ctor arg at NoteBlockInstrument.java:9-35.
// Indexed by ordinal. HARP..TRUMPET_WEATHERED are BASE_BLOCK; ZOMBIE..PIGLIN are
// MOB_HEAD; CUSTOM_HEAD is CUSTOM.
inline constexpr NoteBlockInstrumentType NOTE_BLOCK_INSTRUMENT_TYPE[NOTE_BLOCK_INSTRUMENT_COUNT] = {
    NoteBlockInstrumentType::BASE_BLOCK, // HARP
    NoteBlockInstrumentType::BASE_BLOCK, // BASEDRUM
    NoteBlockInstrumentType::BASE_BLOCK, // SNARE
    NoteBlockInstrumentType::BASE_BLOCK, // HAT
    NoteBlockInstrumentType::BASE_BLOCK, // BASS
    NoteBlockInstrumentType::BASE_BLOCK, // FLUTE
    NoteBlockInstrumentType::BASE_BLOCK, // BELL
    NoteBlockInstrumentType::BASE_BLOCK, // GUITAR
    NoteBlockInstrumentType::BASE_BLOCK, // CHIME
    NoteBlockInstrumentType::BASE_BLOCK, // XYLOPHONE
    NoteBlockInstrumentType::BASE_BLOCK, // IRON_XYLOPHONE
    NoteBlockInstrumentType::BASE_BLOCK, // COW_BELL
    NoteBlockInstrumentType::BASE_BLOCK, // DIDGERIDOO
    NoteBlockInstrumentType::BASE_BLOCK, // BIT
    NoteBlockInstrumentType::BASE_BLOCK, // BANJO
    NoteBlockInstrumentType::BASE_BLOCK, // PLING
    NoteBlockInstrumentType::BASE_BLOCK, // TRUMPET
    NoteBlockInstrumentType::BASE_BLOCK, // TRUMPET_EXPOSED
    NoteBlockInstrumentType::BASE_BLOCK, // TRUMPET_OXIDIZED
    NoteBlockInstrumentType::BASE_BLOCK, // TRUMPET_WEATHERED
    NoteBlockInstrumentType::MOB_HEAD,   // ZOMBIE
    NoteBlockInstrumentType::MOB_HEAD,   // SKELETON
    NoteBlockInstrumentType::MOB_HEAD,   // CREEPER
    NoteBlockInstrumentType::MOB_HEAD,   // DRAGON
    NoteBlockInstrumentType::MOB_HEAD,   // WITHER_SKELETON
    NoteBlockInstrumentType::MOB_HEAD,   // PIGLIN
    NoteBlockInstrumentType::CUSTOM,     // CUSTOM_HEAD
};

// Java: NoteBlockInstrument.getSerializedName() — NoteBlockInstrument.java:47-50.
constexpr const char* noteBlockInstrumentSerializedName(NoteBlockInstrument v) noexcept {
    return NOTE_BLOCK_INSTRUMENT_NAME[static_cast<int>(v)];
}

// Java: NoteBlockInstrument.getType() helper (the private 'type' field).
constexpr NoteBlockInstrumentType noteBlockInstrumentType(NoteBlockInstrument v) noexcept {
    return NOTE_BLOCK_INSTRUMENT_TYPE[static_cast<int>(v)];
}

// Java: NoteBlockInstrument.isTunable() — type == BASE_BLOCK (NoteBlockInstrument.java:56-58).
constexpr bool noteBlockInstrumentIsTunable(NoteBlockInstrument v) noexcept {
    return noteBlockInstrumentType(v) == NoteBlockInstrumentType::BASE_BLOCK;
}

// Java: NoteBlockInstrument.hasCustomSound() — type == CUSTOM (NoteBlockInstrument.java:60-62).
constexpr bool noteBlockInstrumentHasCustomSound(NoteBlockInstrument v) noexcept {
    return noteBlockInstrumentType(v) == NoteBlockInstrumentType::CUSTOM;
}

// Java: NoteBlockInstrument.worksAboveNoteBlock() — type != BASE_BLOCK (NoteBlockInstrument.java:64-66).
constexpr bool noteBlockInstrumentWorksAboveNoteBlock(NoteBlockInstrument v) noexcept {
    return noteBlockInstrumentType(v) != NoteBlockInstrumentType::BASE_BLOCK;
}

} // namespace mc::block::state::properties
