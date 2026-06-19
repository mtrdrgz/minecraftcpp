// 1:1 port of net.minecraft.world.item.ItemDisplayContext (Minecraft 26.1.2)
//
// Pure data enum implementing StringRepresentable. Each constant carries an int
// id (stored as a byte) and a serialized name. Declaration order == ordinal order
// == id order (continuous 0..9). We also reproduce the BY_ID lookup, which is
// built with ByIdMap.continuous(ItemDisplayContext::getId, values(),
// OutOfBoundsStrategy.ZERO).
//
// Source: 26.1.2/src/net/minecraft/world/item/ItemDisplayContext.java
//         26.1.2/src/net/minecraft/util/ByIdMap.java   (continuous / ZERO)
//
//   NONE                    (0, "none")
//   THIRD_PERSON_LEFT_HAND  (1, "thirdperson_lefthand")
//   THIRD_PERSON_RIGHT_HAND (2, "thirdperson_righthand")
//   FIRST_PERSON_LEFT_HAND  (3, "firstperson_lefthand")
//   FIRST_PERSON_RIGHT_HAND (4, "firstperson_righthand")
//   HEAD                    (5, "head")
//   GUI                     (6, "gui")
//   GROUND                  (7, "ground")
//   FIXED                   (8, "fixed")
//   ON_SHELF                (9, "on_shelf")
//
// getId()             == this.id    (byte; here always == ordinal)
// getSerializedName() == this.name  (the second ctor arg)
// firstPerson()       == this == FIRST_PERSON_LEFT_HAND || this == FIRST_PERSON_RIGHT_HAND
// leftHand()          == this == FIRST_PERSON_LEFT_HAND || this == THIRD_PERSON_LEFT_HAND
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace mc::world::item {

// 0-based ordinal of each ItemDisplayContext constant, in declaration order.
enum class ItemDisplayContext : std::int32_t {
   NONE = 0,
   THIRD_PERSON_LEFT_HAND = 1,
   THIRD_PERSON_RIGHT_HAND = 2,
   FIRST_PERSON_LEFT_HAND = 3,
   FIRST_PERSON_RIGHT_HAND = 4,
   HEAD = 5,
   GUI = 6,
   GROUND = 7,
   FIXED = 8,
   ON_SHELF = 9,
};

struct ItemDisplayContextData {
   std::int32_t ordinal;        // 0-based declaration index (== name() identity)
   std::int8_t id;              // getId() — the byte id from the ctor
   std::string_view name;       // getSerializedName()
};

inline constexpr std::int32_t ITEM_DISPLAY_CONTEXT_COUNT = 10;

// Declaration order == ordinal order == id order (continuous 0..9).
inline constexpr std::array<ItemDisplayContextData, ITEM_DISPLAY_CONTEXT_COUNT> ITEM_DISPLAY_CONTEXTS{{
   ItemDisplayContextData{0, 0, "none"},
   ItemDisplayContextData{1, 1, "thirdperson_lefthand"},
   ItemDisplayContextData{2, 2, "thirdperson_righthand"},
   ItemDisplayContextData{3, 3, "firstperson_lefthand"},
   ItemDisplayContextData{4, 4, "firstperson_righthand"},
   ItemDisplayContextData{5, 5, "head"},
   ItemDisplayContextData{6, 6, "gui"},
   ItemDisplayContextData{7, 7, "ground"},
   ItemDisplayContextData{8, 8, "fixed"},
   ItemDisplayContextData{9, 9, "on_shelf"},
}};

inline constexpr const ItemDisplayContextData& data(ItemDisplayContext c) {
   return ITEM_DISPLAY_CONTEXTS[static_cast<std::size_t>(c)];
}

// Accessors mirroring ItemDisplayContext instance methods / the private fields.
inline constexpr std::int8_t getId(ItemDisplayContext c) { return data(c).id; }
inline constexpr std::int32_t ordinal(ItemDisplayContext c) { return data(c).ordinal; }
inline constexpr std::string_view getSerializedName(ItemDisplayContext c) { return data(c).name; }

inline constexpr bool firstPerson(ItemDisplayContext c) {
   return c == ItemDisplayContext::FIRST_PERSON_LEFT_HAND ||
          c == ItemDisplayContext::FIRST_PERSON_RIGHT_HAND;
}

inline constexpr bool leftHand(ItemDisplayContext c) {
   return c == ItemDisplayContext::FIRST_PERSON_LEFT_HAND ||
          c == ItemDisplayContext::THIRD_PERSON_LEFT_HAND;
}

// ItemDisplayContext.BY_ID == ByIdMap.continuous(getId, values(), ZERO):
// because the ids are continuous and equal to the ordinals, the sorted array is
// just values(), so:
//   id >= 0 && id < length ? values[id] : values[0]  (NONE on out-of-range).
inline constexpr ItemDisplayContext byId(std::int32_t id) {
   if (id >= 0 && id < ITEM_DISPLAY_CONTEXT_COUNT) {
      return static_cast<ItemDisplayContext>(id);
   }
   return ItemDisplayContext::NONE;  // values[0]
}

}  // namespace mc::world::item
