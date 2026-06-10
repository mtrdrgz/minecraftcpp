#pragma once
// 1:1 port of net.minecraft.world.entity.EquipmentSlotGroup (Minecraft 26.1.2)
// plus the minimal subset of net.minecraft.world.entity.EquipmentSlot it needs.
//
// EquipmentSlotGroup is a pure enum: each constant carries an `id`, a serialized
// `key`, and a Predicate<EquipmentSlot> that decides which slots belong to the
// group. The `slots()` list is just EquipmentSlot.VALUES filtered by that
// predicate, in EquipmentSlot declaration order. This header reproduces ALL of
// that VERBATIM — the ids, names, predicate semantics, the membership matrix,
// the BY_ID lookup (ByIdMap.continuous with OutOfBoundsStrategy.ZERO), and
// bySlot(EquipmentSlot).
//
// EquipmentSlot constants (from EquipmentSlot.java), with their (type, id, name):
//   MAINHAND(HAND,            id=0, "mainhand")
//   OFFHAND (HAND,            id=5, "offhand")
//   FEET    (HUMANOID_ARMOR,  id=1, "feet")
//   LEGS    (HUMANOID_ARMOR,  id=2, "legs")
//   CHEST   (HUMANOID_ARMOR,  id=3, "chest")
//   HEAD    (HUMANOID_ARMOR,  id=4, "head")
//   BODY    (ANIMAL_ARMOR,    id=6, "body")
//   SADDLE  (SADDLE,          id=7, "saddle")
// isArmor() == (type == HUMANOID_ARMOR || type == ANIMAL_ARMOR).
//
// EquipmentSlotGroup constants (from EquipmentSlotGroup.java):
//   ANY(0,"any", slot->true)
//   MAINHAND(1,"mainhand", == MAINHAND)
//   OFFHAND(2,"offhand",  == OFFHAND)
//   HAND(3,"hand", slot.getType()==HAND)
//   FEET(4,"feet",   == FEET)
//   LEGS(5,"legs",   == LEGS)
//   CHEST(6,"chest", == CHEST)
//   HEAD(7,"head",   == HEAD)
//   ARMOR(8,"armor", EquipmentSlot::isArmor)
//   BODY(9,"body",   == BODY)
//   SADDLE(10,"saddle", == SADDLE)
//
// All ids are contiguous 0..N-1 in declaration order, so the C++ enum's `int`
// value (declaration order) equals the Java field `id` for both enums. BY_ID
// (ByIdMap.continuous, ZERO strategy) therefore returns values()[id] for id in
// [0,count) and values()[0] otherwise (the "zero value", which is the constant
// whose id==0 since the sorted array equals declaration order here).

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace mc::entity {

// ── net.minecraft.world.entity.EquipmentSlot.Type ──
enum class EquipmentSlotType : int {
    HAND = 0,
    HUMANOID_ARMOR = 1,
    ANIMAL_ARMOR = 2,
    SADDLE = 3,
};

// ── net.minecraft.world.entity.EquipmentSlot ──
// Enum order == ordinal. The Java `id` field is NOT the ordinal (offhand/body/
// saddle are reordered), so it is stored explicitly per constant.
enum class EquipmentSlot : int {
    MAINHAND = 0,
    OFFHAND = 1,
    FEET = 2,
    LEGS = 3,
    CHEST = 4,
    HEAD = 5,
    BODY = 6,
    SADDLE = 7,
};

struct EquipmentSlotInfo {
    EquipmentSlot slot;
    EquipmentSlotType type;
    int id;                  // the EquipmentSlot.id field (network id)
    std::string_view name;   // serialized name
};

// public static final List<EquipmentSlot> VALUES = List.of(values());
// Order == enum declaration order (ordinal order).
inline constexpr std::array<EquipmentSlotInfo, 8> EQUIPMENT_SLOTS = {{
    {EquipmentSlot::MAINHAND, EquipmentSlotType::HAND,           0, "mainhand"},
    {EquipmentSlot::OFFHAND,  EquipmentSlotType::HAND,           5, "offhand"},
    {EquipmentSlot::FEET,     EquipmentSlotType::HUMANOID_ARMOR, 1, "feet"},
    {EquipmentSlot::LEGS,     EquipmentSlotType::HUMANOID_ARMOR, 2, "legs"},
    {EquipmentSlot::CHEST,    EquipmentSlotType::HUMANOID_ARMOR, 3, "chest"},
    {EquipmentSlot::HEAD,     EquipmentSlotType::HUMANOID_ARMOR, 4, "head"},
    {EquipmentSlot::BODY,     EquipmentSlotType::ANIMAL_ARMOR,   6, "body"},
    {EquipmentSlot::SADDLE,   EquipmentSlotType::SADDLE,         7, "saddle"},
}};

inline constexpr const EquipmentSlotInfo& slotInfo(EquipmentSlot s) noexcept {
    return EQUIPMENT_SLOTS[static_cast<std::size_t>(static_cast<int>(s))];
}

// EquipmentSlot.getType()
inline constexpr EquipmentSlotType slotType(EquipmentSlot s) noexcept {
    return slotInfo(s).type;
}
// EquipmentSlot.getId()
inline constexpr int slotId(EquipmentSlot s) noexcept {
    return slotInfo(s).id;
}
// EquipmentSlot.getName() / getSerializedName()
inline constexpr std::string_view slotName(EquipmentSlot s) noexcept {
    return slotInfo(s).name;
}
// EquipmentSlot.isArmor()
inline constexpr bool slotIsArmor(EquipmentSlot s) noexcept {
    EquipmentSlotType t = slotType(s);
    return t == EquipmentSlotType::HUMANOID_ARMOR || t == EquipmentSlotType::ANIMAL_ARMOR;
}

// ── net.minecraft.world.entity.EquipmentSlotGroup ──
// Enum order == ordinal == the Java `id` field (all contiguous 0..10 here).
enum class EquipmentSlotGroup : int {
    ANY = 0,
    MAINHAND = 1,
    OFFHAND = 2,
    HAND = 3,
    FEET = 4,
    LEGS = 5,
    CHEST = 6,
    HEAD = 7,
    ARMOR = 8,
    BODY = 9,
    SADDLE = 10,
};

inline constexpr int GROUP_COUNT = 11;

struct EquipmentSlotGroupInfo {
    EquipmentSlotGroup group;
    int id;
    std::string_view key;
};

// values() in declaration order.
inline constexpr std::array<EquipmentSlotGroupInfo, GROUP_COUNT> EQUIPMENT_SLOT_GROUPS = {{
    {EquipmentSlotGroup::ANY,      0,  "any"},
    {EquipmentSlotGroup::MAINHAND, 1,  "mainhand"},
    {EquipmentSlotGroup::OFFHAND,  2,  "offhand"},
    {EquipmentSlotGroup::HAND,     3,  "hand"},
    {EquipmentSlotGroup::FEET,     4,  "feet"},
    {EquipmentSlotGroup::LEGS,     5,  "legs"},
    {EquipmentSlotGroup::CHEST,    6,  "chest"},
    {EquipmentSlotGroup::HEAD,     7,  "head"},
    {EquipmentSlotGroup::ARMOR,    8,  "armor"},
    {EquipmentSlotGroup::BODY,     9,  "body"},
    {EquipmentSlotGroup::SADDLE,   10, "saddle"},
}};

inline constexpr const EquipmentSlotGroupInfo& groupInfo(EquipmentSlotGroup g) noexcept {
    return EQUIPMENT_SLOT_GROUPS[static_cast<std::size_t>(static_cast<int>(g))];
}

// EquipmentSlotGroup.getSerializedName()
inline constexpr std::string_view groupKey(EquipmentSlotGroup g) noexcept {
    return groupInfo(g).key;
}
// the EquipmentSlotGroup.id field
inline constexpr int groupId(EquipmentSlotGroup g) noexcept {
    return groupInfo(g).id;
}

// EquipmentSlotGroup.test(EquipmentSlot) — the per-constant predicate, VERBATIM.
inline constexpr bool groupTest(EquipmentSlotGroup g, EquipmentSlot slot) noexcept {
    switch (g) {
        case EquipmentSlotGroup::ANY:      return true;
        case EquipmentSlotGroup::MAINHAND: return slot == EquipmentSlot::MAINHAND;
        case EquipmentSlotGroup::OFFHAND:  return slot == EquipmentSlot::OFFHAND;
        case EquipmentSlotGroup::HAND:     return slotType(slot) == EquipmentSlotType::HAND;
        case EquipmentSlotGroup::FEET:     return slot == EquipmentSlot::FEET;
        case EquipmentSlotGroup::LEGS:     return slot == EquipmentSlot::LEGS;
        case EquipmentSlotGroup::CHEST:    return slot == EquipmentSlot::CHEST;
        case EquipmentSlotGroup::HEAD:     return slot == EquipmentSlot::HEAD;
        case EquipmentSlotGroup::ARMOR:    return slotIsArmor(slot);
        case EquipmentSlotGroup::BODY:     return slot == EquipmentSlot::BODY;
        case EquipmentSlotGroup::SADDLE:   return slot == EquipmentSlot::SADDLE;
    }
    return false;
}

// EquipmentSlotGroup.BY_ID = ByIdMap.continuous(s -> s.id, values(), ZERO).
// Ids are contiguous 0..10 in declaration order, so sortedValues[id] == values()[id]
// and the "zero value" is the id==0 constant (ANY). For id in [0,count) return
// values()[id]; otherwise return the zero value.
inline constexpr EquipmentSlotGroup groupById(int id) noexcept {
    if (id >= 0 && id < GROUP_COUNT) {
        return EQUIPMENT_SLOT_GROUPS[static_cast<std::size_t>(id)].group;
    }
    return EQUIPMENT_SLOT_GROUPS[0].group;  // zeroValue = sortedValues[0]
}

// EquipmentSlotGroup.bySlot(EquipmentSlot) — the explicit switch, VERBATIM.
inline constexpr EquipmentSlotGroup groupBySlot(EquipmentSlot slot) noexcept {
    switch (slot) {
        case EquipmentSlot::MAINHAND: return EquipmentSlotGroup::MAINHAND;
        case EquipmentSlot::OFFHAND:  return EquipmentSlotGroup::OFFHAND;
        case EquipmentSlot::FEET:     return EquipmentSlotGroup::FEET;
        case EquipmentSlot::LEGS:     return EquipmentSlotGroup::LEGS;
        case EquipmentSlot::CHEST:    return EquipmentSlotGroup::CHEST;
        case EquipmentSlot::HEAD:     return EquipmentSlotGroup::HEAD;
        case EquipmentSlot::BODY:     return EquipmentSlotGroup::BODY;
        case EquipmentSlot::SADDLE:   return EquipmentSlotGroup::SADDLE;
    }
    // The Java switch is exhaustive over EquipmentSlot; unreachable.
    return EquipmentSlotGroup::ANY;
}

}  // namespace mc::entity
