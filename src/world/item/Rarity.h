// 1:1 port of net.minecraft.world.item.Rarity (Minecraft 26.1.2)
//
// Pure data enum: 4 rarities, each carrying an int id, a serialized name, and a
// ChatFormatting color. We also reproduce the BY_ID lookup, which is built with
// ByIdMap.continuous(..., OutOfBoundsStrategy.ZERO) keyed on `id`.
//
// Source: 26.1.2/src/net/minecraft/world/item/Rarity.java
//         26.1.2/src/net/minecraft/ChatFormatting.java   (the four color constants)
//         26.1.2/src/net/minecraft/util/ByIdMap.java      (continuous / ZERO)
//
//   COMMON  (0, "common",   ChatFormatting.WHITE)
//   UNCOMMON(1, "uncommon", ChatFormatting.YELLOW)
//   RARE    (2, "rare",     ChatFormatting.AQUA)
//   EPIC    (3, "epic",     ChatFormatting.LIGHT_PURPLE)
//
// getSerializedName() == this.name (the second ctor arg).
// color()             == this.color (the ChatFormatting constant).
//
// The four referenced ChatFormatting constants are reproduced verbatim from the
// ChatFormatting enum-constant declarations:
//   WHITE       ("WHITE",        'f', id 15, color 16777215)  ordinal 15
//   YELLOW      ("YELLOW",       'e', id 14, color 16777045)  ordinal 14
//   AQUA        ("AQUA",         'b', id 11, color 5636095)   ordinal 11
//   LIGHT_PURPLE("LIGHT_PURPLE", 'd', id 13, color 16733695)  ordinal 13
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace mc::world::item {

// The subset of ChatFormatting that Rarity references. Each field is verbatim
// from the matching ChatFormatting enum-constant declaration.
struct ChatFormattingRef {
   std::int32_t ordinal;        // position in the ChatFormatting enum (name() identity)
   std::string_view enumName;   // ChatFormatting.name() — the constant's Java name
   char code;                   // the '§'-prefix character (getChar())
   std::int32_t id;             // getId()
   std::int32_t color;          // getColor() (all four are colors, so never null here)
};

namespace chatformatting {
// (name, code, id, color) exactly as declared in ChatFormatting.java; ordinal is
// the 0-based declaration index.
inline constexpr ChatFormattingRef AQUA{11, "AQUA", 'b', 11, 5636095};
inline constexpr ChatFormattingRef LIGHT_PURPLE{13, "LIGHT_PURPLE", 'd', 13, 16733695};
inline constexpr ChatFormattingRef YELLOW{14, "YELLOW", 'e', 14, 16777045};
inline constexpr ChatFormattingRef WHITE{15, "WHITE", 'f', 15, 16777215};
}  // namespace chatformatting

struct RarityData {
   std::int32_t id;
   std::string_view name;        // getSerializedName()
   ChatFormattingRef color;      // color()
};

// Declaration order == ordinal order == id order (continuous 0..3).
inline constexpr std::array<RarityData, 4> RARITIES{{
   RarityData{0, "common", chatformatting::WHITE},
   RarityData{1, "uncommon", chatformatting::YELLOW},
   RarityData{2, "rare", chatformatting::AQUA},
   RarityData{3, "epic", chatformatting::LIGHT_PURPLE},
}};

inline constexpr std::int32_t RARITY_COUNT = 4;

// Accessors mirroring Rarity instance methods / the private `id` field.
inline constexpr std::int32_t getId(const RarityData& r) { return r.id; }
inline constexpr std::string_view getSerializedName(const RarityData& r) { return r.name; }
inline constexpr const ChatFormattingRef& color(const RarityData& r) { return r.color; }

// Rarity.BY_ID == ByIdMap.continuous(..., ZERO):
//   id >= 0 && id < length ? values[id] : values[0]
inline constexpr const RarityData& byId(std::int32_t id) {
   if (id >= 0 && id < RARITY_COUNT) {
      return RARITIES[static_cast<std::size_t>(id)];
   }
   return RARITIES[0];  // COMMON
}

}  // namespace mc::world::item
