// 1:1 port of net.minecraft.world.item.DyeColor (Minecraft 26.1.2)
//
// Pure data enum: 16 colors, each carrying id, name, textureDiffuseColor,
// mapColor (id + col), fireworkColor, textColor.
//
// Source: 26.1.2/src/net/minecraft/world/item/DyeColor.java
//         26.1.2/src/net/minecraft/world/level/material/MapColor.java
//         26.1.2/src/net/minecraft/util/ARGB.java   (opaque = color | 0xFF000000)
//         26.1.2/src/net/minecraft/util/ByIdMap.java (continuous / ZERO strategy)
//
// In the constructor Java does:
//   this.textColor          = ARGB.opaque(textColor);
//   this.textureDiffuseColor = ARGB.opaque(textureDiffuseColor);
//   this.fireworkColor       = fireworkColor;   // raw, NOT opaque-ified
// We bake the opaque() result into the stored constants here.
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace mc::world::item {

// ARGB.opaque(color) = color | 0xFF000000
constexpr std::int32_t argb_opaque(std::int32_t color) {
   return static_cast<std::int32_t>(static_cast<std::uint32_t>(color) | 0xFF000000u);
}

struct MapColorRef {
   std::int32_t id;
   std::int32_t col;
};

// MapColor constants referenced by DyeColor (from MapColor.java).
namespace mapcolor {
inline constexpr MapColorRef SNOW{8, 16777215};
inline constexpr MapColorRef COLOR_ORANGE{15, 14188339};
inline constexpr MapColorRef COLOR_MAGENTA{16, 11685080};
inline constexpr MapColorRef COLOR_LIGHT_BLUE{17, 6724056};
inline constexpr MapColorRef COLOR_YELLOW{18, 15066419};
inline constexpr MapColorRef COLOR_LIGHT_GREEN{19, 8375321};
inline constexpr MapColorRef COLOR_PINK{20, 15892389};
inline constexpr MapColorRef COLOR_GRAY{21, 5000268};
inline constexpr MapColorRef COLOR_LIGHT_GRAY{22, 10066329};
inline constexpr MapColorRef COLOR_CYAN{23, 5013401};
inline constexpr MapColorRef COLOR_PURPLE{24, 8339378};
inline constexpr MapColorRef COLOR_BLUE{25, 3361970};
inline constexpr MapColorRef COLOR_BROWN{26, 6704179};
inline constexpr MapColorRef COLOR_GREEN{27, 6717235};
inline constexpr MapColorRef COLOR_RED{28, 10040115};
inline constexpr MapColorRef COLOR_BLACK{29, 1644825};
}  // namespace mapcolor

struct DyeColorData {
   std::int32_t id;
   std::string_view name;
   std::int32_t textureDiffuseColor;  // already opaque()-ified
   MapColorRef mapColor;
   std::int32_t fireworkColor;        // raw, as declared
   std::int32_t textColor;            // already opaque()-ified
};

// Helper to build an entry the way the Java constructor does, so the literal
// table below reads exactly like the Java enum-constant argument lists.
constexpr DyeColorData make_dye(std::int32_t id, std::string_view name,
                                std::int32_t textureDiffuseColor, MapColorRef mapColor,
                                std::int32_t fireworkColor, std::int32_t textColor) {
   return DyeColorData{id, name, argb_opaque(textureDiffuseColor), mapColor,
                       fireworkColor, argb_opaque(textColor)};
}

// Declaration order == id order (continuous 0..15).
inline constexpr std::array<DyeColorData, 16> DYE_COLORS{{
   make_dye(0, "white", 16383998, mapcolor::SNOW, 15790320, 16777215),
   make_dye(1, "orange", 16351261, mapcolor::COLOR_ORANGE, 15435844, 16738335),
   make_dye(2, "magenta", 13061821, mapcolor::COLOR_MAGENTA, 12801229, 16711935),
   make_dye(3, "light_blue", 3847130, mapcolor::COLOR_LIGHT_BLUE, 6719955, 10141901),
   make_dye(4, "yellow", 16701501, mapcolor::COLOR_YELLOW, 14602026, 16776960),
   make_dye(5, "lime", 8439583, mapcolor::COLOR_LIGHT_GREEN, 4312372, 12582656),
   make_dye(6, "pink", 15961002, mapcolor::COLOR_PINK, 14188952, 16738740),
   make_dye(7, "gray", 4673362, mapcolor::COLOR_GRAY, 4408131, 8421504),
   make_dye(8, "light_gray", 10329495, mapcolor::COLOR_LIGHT_GRAY, 11250603, 13882323),
   make_dye(9, "cyan", 1481884, mapcolor::COLOR_CYAN, 2651799, 65535),
   make_dye(10, "purple", 8991416, mapcolor::COLOR_PURPLE, 8073150, 10494192),
   make_dye(11, "blue", 3949738, mapcolor::COLOR_BLUE, 2437522, 255),
   make_dye(12, "brown", 8606770, mapcolor::COLOR_BROWN, 5320730, 9127187),
   make_dye(13, "green", 6192150, mapcolor::COLOR_GREEN, 3887386, 65280),
   make_dye(14, "red", 11546150, mapcolor::COLOR_RED, 11743532, 16711680),
   make_dye(15, "black", 1908001, mapcolor::COLOR_BLACK, 1973019, 0),
}};

inline constexpr std::int32_t DYE_COLOR_COUNT = 16;

// Accessors mirroring DyeColor instance methods.
inline constexpr std::int32_t getId(const DyeColorData& d) { return d.id; }
inline constexpr std::string_view getName(const DyeColorData& d) { return d.name; }
inline constexpr std::int32_t getTextureDiffuseColor(const DyeColorData& d) {
   return d.textureDiffuseColor;
}
inline constexpr const MapColorRef& getMapColor(const DyeColorData& d) { return d.mapColor; }
inline constexpr std::int32_t getFireworkColor(const DyeColorData& d) { return d.fireworkColor; }
inline constexpr std::int32_t getTextColor(const DyeColorData& d) { return d.textColor; }

// DyeColor.byId: ByIdMap.continuous(..., ZERO) ->
//   id >= 0 && id < length ? values[id] : values[0]
inline constexpr const DyeColorData& byId(std::int32_t id) {
   if (id >= 0 && id < DYE_COLOR_COUNT) {
      return DYE_COLORS[static_cast<std::size_t>(id)];
   }
   return DYE_COLORS[0];  // WHITE
}

// DyeColor.byFireworkColor: BY_FIREWORK_COLOR.get(color) (null if no match).
// Returns index 0..15, or -1 if no color has that raw fireworkColor.
inline constexpr std::int32_t byFireworkColorIndex(std::int32_t color) {
   for (std::size_t i = 0; i < DYE_COLORS.size(); ++i) {
      if (DYE_COLORS[i].fireworkColor == color) {
         return static_cast<std::int32_t>(i);
      }
   }
   return -1;
}

}  // namespace mc::world::item
