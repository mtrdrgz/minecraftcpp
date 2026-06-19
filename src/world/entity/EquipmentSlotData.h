#pragma once
// 1:1 port of net.minecraft.world.entity.EquipmentSlot (Minecraft 26.1.2).
//
// Source: 26.1.2/src/net/minecraft/world/entity/EquipmentSlot.java
//
// This is a self-contained, registry-free header carrying the per-constant
// data tables and pure accessor logic of the Java enum:
//   getType / getIndex() / getIndex(base) / getId / getFilterBit(offset) /
//   getName / getSerializedName / isArmor / canIncreaseExperience / byName /
//   BY_ID (ByIdMap.continuous with OutOfBoundsStrategy.ZERO).
//
// NOTE: this header intentionally lives in its own file (EquipmentSlotData.h)
// and does NOT touch the pre-existing `enum class EquipmentSlot` in
// world/entity/Entity.h. To avoid a name clash if both are included in one TU,
// everything here is in namespace mc::eqslot.
//
// NOT ported (require un-ported dependencies, see EquipmentSlotParity notes):
//   - limit(ItemStack)           : needs world.item.ItemStack.split
//   - VALUES / CODEC / STREAM_CODEC : registry/codec machinery (the *behavior*
//                                     of BY_ID and byName is reproduced here).

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mc::eqslot {

// EquipmentSlot.Type (EquipmentSlot.java:95-100).
enum class Type {
    HAND = 0,
    HUMANOID_ARMOR = 1,
    ANIMAL_ARMOR = 2,
    SADDLE = 3,
};

// EquipmentSlot.NO_COUNT_LIMIT (EquipmentSlot.java:22).
inline constexpr int NO_COUNT_LIMIT = 0;

// Enum ordinals in declaration order (EquipmentSlot.java:13-20).
enum class Slot {
    MAINHAND = 0,
    OFFHAND  = 1,
    FEET     = 2,
    LEGS     = 3,
    CHEST    = 4,
    HEAD     = 5,
    BODY     = 6,
    SADDLE   = 7,
};

inline constexpr int COUNT = 8;

// Per-constant immutable record (the 5 fields of the Java enum).
struct Data {
    Type        type;
    int         index;
    int         countLimit;
    int         id;
    const char* name;
};

// Declaration-order table, verbatim from EquipmentSlot.java:13-20.
//   The two-arg ctor (line 41) forwards countLimit = 0 (NO_COUNT_LIMIT).
//   The five-arg ctor (line 33) is (type, index, countLimit, id, name).
inline constexpr std::array<Data, COUNT> TABLE = {{
    // MAINHAND(Type.HAND,            index=0,            id=0, "mainhand")  [countLimit=0]
    { Type::HAND,           0, NO_COUNT_LIMIT, 0, "mainhand" },
    // OFFHAND (Type.HAND,            index=1,            id=5, "offhand")   [countLimit=0]
    { Type::HAND,           1, NO_COUNT_LIMIT, 5, "offhand"  },
    // FEET    (Type.HUMANOID_ARMOR,  index=0, countLimit=1, id=1, "feet")
    { Type::HUMANOID_ARMOR, 0, 1,              1, "feet"     },
    // LEGS    (Type.HUMANOID_ARMOR,  index=1, countLimit=1, id=2, "legs")
    { Type::HUMANOID_ARMOR, 1, 1,              2, "legs"     },
    // CHEST   (Type.HUMANOID_ARMOR,  index=2, countLimit=1, id=3, "chest")
    { Type::HUMANOID_ARMOR, 2, 1,              3, "chest"    },
    // HEAD    (Type.HUMANOID_ARMOR,  index=3, countLimit=1, id=4, "head")
    { Type::HUMANOID_ARMOR, 3, 1,              4, "head"     },
    // BODY    (Type.ANIMAL_ARMOR,    index=0, countLimit=1, id=6, "body")
    { Type::ANIMAL_ARMOR,   0, 1,              6, "body"     },
    // SADDLE  (Type.SADDLE,          index=0, countLimit=1, id=7, "saddle")
    { Type::SADDLE,         0, 1,              7, "saddle"   },
}};

inline constexpr const Data& data(Slot s) {
    return TABLE[static_cast<std::size_t>(s)];
}

// getType() — EquipmentSlot.java:45-47.
inline constexpr Type getType(Slot s) { return data(s).type; }

// getIndex() — EquipmentSlot.java:49-51.
inline constexpr int getIndex(Slot s) { return data(s).index; }

// getIndex(int base) — EquipmentSlot.java:53-55.  (Java int add, wraps 2's-comp.)
inline constexpr int getIndex(Slot s, int base) {
    return static_cast<int>(
        static_cast<unsigned>(base) + static_cast<unsigned>(data(s).index));
}

// getId() — EquipmentSlot.java:61-63.  (the field built from the "filterFlag"
// ctor argument; this is what the assignment calls "getFilterFlag".)
inline constexpr int getId(Slot s) { return data(s).id; }

// getFilterBit(int offset) — EquipmentSlot.java:65-67.  (Java int add.)
inline constexpr int getFilterBit(Slot s, int offset) {
    return static_cast<int>(
        static_cast<unsigned>(data(s).id) + static_cast<unsigned>(offset));
}

// getName() / getSerializedName() — EquipmentSlot.java:69-71, 77-80.
inline constexpr std::string_view getName(Slot s) {
    return std::string_view(data(s).name);
}

// isArmor() — EquipmentSlot.java:73-75.
inline constexpr bool isArmor(Slot s) {
    Type t = data(s).type;
    return t == Type::HUMANOID_ARMOR || t == Type::ANIMAL_ARMOR;
}

// canIncreaseExperience() — EquipmentSlot.java:82-84.
inline constexpr bool canIncreaseExperience(Slot s) {
    return data(s).type != Type::SADDLE;
}

// BY_ID — EquipmentSlot.java:24:
//   ByIdMap.continuous(s -> s.id, values(), OutOfBoundsStrategy.ZERO).
// ByIdMap.createSortedArray (ByIdMap.java:34-64) places each value at result[id]
// (ids are 0..7, continuous), then ZERO (ByIdMap.java:71-73) yields:
//   id in [0,8) -> sorted[id], else sorted[0].
// sorted[0] (the ZERO fallback) is the value whose id==0, i.e. MAINHAND.
inline constexpr Slot SORTED_BY_ID[COUNT] = {
    Slot::MAINHAND, // id 0
    Slot::FEET,     // id 1
    Slot::LEGS,     // id 2
    Slot::CHEST,    // id 3
    Slot::HEAD,     // id 4
    Slot::OFFHAND,  // id 5
    Slot::BODY,     // id 6
    Slot::SADDLE,   // id 7
};

inline constexpr Slot byId(int id) {
    if (id >= 0 && id < COUNT) return SORTED_BY_ID[id];
    return SORTED_BY_ID[0]; // ZERO strategy fallback
}

// byName(String) — EquipmentSlot.java:86-93.  CODEC.byName is a name->constant
// lookup over the serialized names; a miss throws IllegalArgumentException.
// Returns the matching Slot; throws std::invalid_argument on miss.
inline Slot byName(std::string_view name) {
    for (int i = 0; i < COUNT; ++i) {
        if (std::string_view(TABLE[static_cast<std::size_t>(i)].name) == name)
            return static_cast<Slot>(i);
    }
    throw std::invalid_argument("Invalid slot '" + std::string(name) + "'");
}

} // namespace mc::eqslot
