// 1:1 port of net.minecraft.world.level.saveddata.maps.MapDecorationType (MC 26.1.2)
// and the static built-in registrations from MapDecorationTypes.
//
// Source: 26.1.2/src/net/minecraft/world/level/saveddata/maps/MapDecorationType.java
//         26.1.2/src/net/minecraft/world/level/saveddata/maps/MapDecorationTypes.java
//         26.1.2/src/net/minecraft/world/level/material/MapColor.java (COLOR_LIGHT_GRAY.col)
//
// MapDecorationType is a record:
//   record MapDecorationType(Identifier assetId, boolean showOnItemFrame, int mapColor,
//                            boolean explorationMapElement, boolean trackCount)
//   NO_MAP_COLOR = -1
//   hasMapColor() => mapColor != -1
//
// We port the pure data: the flattened constructor args produced by the two
// MapDecorationTypes.register(...) overloads, the registration name -> assetName
// mapping, and hasMapColor(). The Holder/Codec/StreamCodec/Registry machinery
// (CODEC, STREAM_CODEC, bootstrap, Registry.registerForHolder) is registry-coupled
// and intentionally NOT ported here.
//
// Identifiers in this class are all minecraft:<...> (Identifier.withDefaultNamespace),
// so assetId == "minecraft:" + assetName and the registration id == "minecraft:" + name.
#pragma once

#include <array>
#include <string_view>

namespace mc::saveddata::maps {

// MapDecorationType.NO_MAP_COLOR
inline constexpr int NO_MAP_COLOR = -1;

// MapColor.COLOR_LIGHT_GRAY.col (26.1.2 MapColor id=22) — used by village/temple/hut decorations.
inline constexpr int MAP_COLOR_LIGHT_GRAY = 10066329;

// The record net.minecraft.world.level.saveddata.maps.MapDecorationType.
// assetId / id namespaces are always "minecraft"; we store the path only and
// expose the default namespace as a constant.
struct MapDecorationType {
    std::string_view assetName;            // path of assetId Identifier (namespace = "minecraft")
    bool showOnItemFrame;
    int mapColor;
    bool explorationMapElement;
    bool trackCount;

    // MapDecorationType.hasMapColor(): return this.mapColor != -1;
    constexpr bool hasMapColor() const { return mapColor != NO_MAP_COLOR; }
};

// Default namespace ("minecraft") for both the registration name and the assetId.
inline constexpr std::string_view DEFAULT_NAMESPACE = "minecraft";

// One built-in registration entry: the registration name (registry key path) plus the
// flattened MapDecorationType it produces. Order matches the static field declaration
// order in MapDecorationTypes (which is the registration order).
struct MapDecorationEntry {
    std::string_view name;   // path of the ResourceKey Identifier (namespace = "minecraft")
    MapDecorationType type;
};

// register(name, assetName, showOnItemFrame, trackCount)
//   -> register(name, assetName, showOnItemFrame, -1, trackCount, false)
//      => MapDecorationType(assetName, showOnItemFrame, -1, /*explorationMapElement*/false, trackCount)
constexpr MapDecorationEntry reg4(std::string_view name, std::string_view assetName,
                                  bool showOnItemFrame, bool trackCount) {
    return MapDecorationEntry{name,
        MapDecorationType{assetName, showOnItemFrame, NO_MAP_COLOR, /*explorationMapElement*/false, trackCount}};
}

// register(name, assetName, showOnItemFrame, mapColor, trackCount, explorationMapElement)
//   => MapDecorationType(assetName, showOnItemFrame, mapColor, explorationMapElement, trackCount)
constexpr MapDecorationEntry reg6(std::string_view name, std::string_view assetName,
                                  bool showOnItemFrame, int mapColor,
                                  bool trackCount, bool explorationMapElement) {
    return MapDecorationEntry{name,
        MapDecorationType{assetName, showOnItemFrame, mapColor, explorationMapElement, trackCount}};
}

// All built-in MapDecorationTypes registrations, in declaration (= registration) order.
// Verbatim from MapDecorationTypes.java.
inline constexpr std::array<MapDecorationEntry, 35> BUILTIN_MAP_DECORATION_TYPES = {{
    reg4("player",            "player",            false, true),
    reg4("frame",             "frame",             true,  true),
    reg4("red_marker",        "red_marker",        false, true),
    reg4("blue_marker",       "blue_marker",       false, true),
    reg4("target_x",          "target_x",          true,  false),
    reg4("target_point",      "target_point",      true,  false),
    reg4("player_off_map",    "player_off_map",    false, true),
    reg4("player_off_limits", "player_off_limits", false, true),
    reg6("mansion",  "woodland_mansion", true, 5393476, false, true),
    reg6("monument", "ocean_monument",   true, 3830373, false, true),
    reg4("banner_white",      "white_banner",      true, true),
    reg4("banner_orange",     "orange_banner",     true, true),
    reg4("banner_magenta",    "magenta_banner",    true, true),
    reg4("banner_light_blue", "light_blue_banner", true, true),
    reg4("banner_yellow",     "yellow_banner",     true, true),
    reg4("banner_lime",       "lime_banner",       true, true),
    reg4("banner_pink",       "pink_banner",       true, true),
    reg4("banner_gray",       "gray_banner",       true, true),
    reg4("banner_light_gray", "light_gray_banner", true, true),
    reg4("banner_cyan",       "cyan_banner",       true, true),
    reg4("banner_purple",     "purple_banner",     true, true),
    reg4("banner_blue",       "blue_banner",       true, true),
    reg4("banner_brown",      "brown_banner",      true, true),
    reg4("banner_green",      "green_banner",      true, true),
    reg4("banner_red",        "red_banner",        true, true),
    reg4("banner_black",      "black_banner",      true, true),
    reg4("red_x",             "red_x",             true, false),
    reg6("village_desert",  "desert_village",  true, MAP_COLOR_LIGHT_GRAY, false, true),
    reg6("village_plains",  "plains_village",  true, MAP_COLOR_LIGHT_GRAY, false, true),
    reg6("village_savanna", "savanna_village", true, MAP_COLOR_LIGHT_GRAY, false, true),
    reg6("village_snowy",   "snowy_village",   true, MAP_COLOR_LIGHT_GRAY, false, true),
    reg6("village_taiga",   "taiga_village",   true, MAP_COLOR_LIGHT_GRAY, false, true),
    reg6("jungle_temple",   "jungle_temple",   true, MAP_COLOR_LIGHT_GRAY, false, true),
    reg6("swamp_hut",       "swamp_hut",       true, MAP_COLOR_LIGHT_GRAY, false, true),
    reg6("trial_chambers",  "trial_chambers",  true, 12741452,             false, true),
}};

} // namespace mc::saveddata::maps
