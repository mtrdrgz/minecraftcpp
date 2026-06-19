// 1:1 port of net.minecraft.world.item.ItemUseAnimation (Minecraft 26.1.2)
//
// Pure-data enum implementing StringRepresentable. Each constant carries an int
// id, a serialized name, and a boolean `customArmTransform` (default false; set
// true via the 3-arg ctor for EAT/DRINK/SPEAR). Declaration order == ordinal
// order == id order (continuous 0..11). We also reproduce the BY_ID lookup, which
// is built with ByIdMap.continuous(ItemUseAnimation::getId, values(),
// OutOfBoundsStrategy.ZERO).
//
// Source: 26.1.2/src/net/minecraft/world/item/ItemUseAnimation.java
//         26.1.2/src/net/minecraft/util/ByIdMap.java   (continuous / ZERO)
//
//   NONE     (0,  "none")
//   EAT      (1,  "eat",     true)
//   DRINK    (2,  "drink",   true)
//   BLOCK    (3,  "block")
//   BOW      (4,  "bow")
//   TRIDENT  (5,  "trident")
//   CROSSBOW (6,  "crossbow")
//   SPYGLASS (7,  "spyglass")
//   TOOT_HORN(8,  "toot_horn")
//   BRUSH    (9,  "brush")
//   BUNDLE   (10, "bundle")
//   SPEAR    (11, "spear",   true)
//
// getId()                == this.id    (int; here always == ordinal)
// getSerializedName()    == this.name  (the second ctor arg)
// hasCustomArmTransform()== this.customArmTransform (default false; true for the
//                           3-arg ctor constants EAT, DRINK, SPEAR)
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace mc::world::item {

// 0-based ordinal of each ItemUseAnimation constant, in declaration order.
enum class ItemUseAnimation : std::int32_t {
    NONE = 0,
    EAT = 1,
    DRINK = 2,
    BLOCK = 3,
    BOW = 4,
    TRIDENT = 5,
    CROSSBOW = 6,
    SPYGLASS = 7,
    TOOT_HORN = 8,
    BRUSH = 9,
    BUNDLE = 10,
    SPEAR = 11,
};

struct ItemUseAnimationData {
    std::int32_t ordinal;       // 0-based declaration index (== name() identity)
    std::int32_t id;            // getId() — the int id from the ctor
    std::string_view name;      // getSerializedName()
    bool customArmTransform;    // hasCustomArmTransform()
};

inline constexpr std::int32_t ITEM_USE_ANIMATION_COUNT = 12;

// Declaration order == ordinal order == id order (continuous 0..11).
inline constexpr std::array<ItemUseAnimationData, ITEM_USE_ANIMATION_COUNT> ITEM_USE_ANIMATIONS{{
    ItemUseAnimationData{0, 0, "none", false},
    ItemUseAnimationData{1, 1, "eat", true},
    ItemUseAnimationData{2, 2, "drink", true},
    ItemUseAnimationData{3, 3, "block", false},
    ItemUseAnimationData{4, 4, "bow", false},
    ItemUseAnimationData{5, 5, "trident", false},
    ItemUseAnimationData{6, 6, "crossbow", false},
    ItemUseAnimationData{7, 7, "spyglass", false},
    ItemUseAnimationData{8, 8, "toot_horn", false},
    ItemUseAnimationData{9, 9, "brush", false},
    ItemUseAnimationData{10, 10, "bundle", false},
    ItemUseAnimationData{11, 11, "spear", true},
}};

inline constexpr const ItemUseAnimationData& data(ItemUseAnimation c) {
    return ITEM_USE_ANIMATIONS[static_cast<std::size_t>(c)];
}

// Accessors mirroring ItemUseAnimation instance methods / the private fields.
inline constexpr std::int32_t getId(ItemUseAnimation c) { return data(c).id; }
inline constexpr std::int32_t ordinal(ItemUseAnimation c) { return data(c).ordinal; }
inline constexpr std::string_view getSerializedName(ItemUseAnimation c) { return data(c).name; }
inline constexpr bool hasCustomArmTransform(ItemUseAnimation c) { return data(c).customArmTransform; }

// ItemUseAnimation.BY_ID == ByIdMap.continuous(getId, values(), ZERO):
// because the ids are continuous and equal to the ordinals, the sorted array is
// just values(), so:
//   id >= 0 && id < length ? values[id] : values[0]  (NONE on out-of-range).
inline constexpr ItemUseAnimation byId(std::int32_t id) {
    if (id >= 0 && id < ITEM_USE_ANIMATION_COUNT) {
        return static_cast<ItemUseAnimation>(id);
    }
    return ItemUseAnimation::NONE;  // values[0]
}

}  // namespace mc::world::item
