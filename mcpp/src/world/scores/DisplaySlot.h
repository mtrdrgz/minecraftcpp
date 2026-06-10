// 1:1 port of net.minecraft.world.scores.DisplaySlot (Minecraft 26.1.2).
//
// Source: 26.1.2/src/net/minecraft/world/scores/DisplaySlot.java
//
//   public enum DisplaySlot implements StringRepresentable {
//      LIST(0, "list"), SIDEBAR(1, "sidebar"), BELOW_NAME(2, "below_name"),
//      TEAM_BLACK(3, "sidebar.team.black"), ... TEAM_WHITE(18, "sidebar.team.white");
//
// Each constant carries an int id() (== ordinal here) and a serialized name.
// teamColorToSlot(ChatFormatting) maps the 16 vanilla colors to the TEAM_*
// slots and returns null (nullopt) for the 6 non-color format codes.
//
// BY_ID = ByIdMap.continuous(DisplaySlot::id, values(), OutOfBoundsStrategy.ZERO):
// since ids are 0..18 contiguous and equal to ordinals, byId(i) returns the
// constant with id==i for 0<=i<COUNT, else LIST(0) (the zero/sentinel value).
// Source: 26.1.2/src/net/minecraft/util/ByIdMap.java continuous()/ZERO branch.
//
// This header is the ported data table; it touches no registries/world/network
// and is fully comparable against the real enum (see DisplaySlotParity.java).

#ifndef MCPP_WORLD_SCORES_DISPLAYSLOT_H
#define MCPP_WORLD_SCORES_DISPLAYSLOT_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace mc::scores {

// Declaration order == ordinal() == id() for DisplaySlot.
enum class DisplaySlot : int32_t {
    LIST = 0,
    SIDEBAR = 1,
    BELOW_NAME = 2,
    TEAM_BLACK = 3,
    TEAM_DARK_BLUE = 4,
    TEAM_DARK_GREEN = 5,
    TEAM_DARK_AQUA = 6,
    TEAM_DARK_RED = 7,
    TEAM_DARK_PURPLE = 8,
    TEAM_GOLD = 9,
    TEAM_GRAY = 10,
    TEAM_DARK_GRAY = 11,
    TEAM_BLUE = 12,
    TEAM_GREEN = 13,
    TEAM_AQUA = 14,
    TEAM_RED = 15,
    TEAM_LIGHT_PURPLE = 16,
    TEAM_YELLOW = 17,
    TEAM_WHITE = 18,
};

inline constexpr int DISPLAY_SLOT_COUNT = 19;

struct DisplaySlotData {
    DisplaySlot slot;
    int32_t id;             // the explicit constructor id (== ordinal)
    const char* enumName;   // name() — Java constant identifier
    const char* serialized; // getSerializedName() == this.name
};

// Table in declaration order. Values verbatim from DisplaySlot.java lines 10-28.
inline constexpr DisplaySlotData DISPLAY_SLOTS[DISPLAY_SLOT_COUNT] = {
    {DisplaySlot::LIST, 0, "LIST", "list"},
    {DisplaySlot::SIDEBAR, 1, "SIDEBAR", "sidebar"},
    {DisplaySlot::BELOW_NAME, 2, "BELOW_NAME", "below_name"},
    {DisplaySlot::TEAM_BLACK, 3, "TEAM_BLACK", "sidebar.team.black"},
    {DisplaySlot::TEAM_DARK_BLUE, 4, "TEAM_DARK_BLUE", "sidebar.team.dark_blue"},
    {DisplaySlot::TEAM_DARK_GREEN, 5, "TEAM_DARK_GREEN", "sidebar.team.dark_green"},
    {DisplaySlot::TEAM_DARK_AQUA, 6, "TEAM_DARK_AQUA", "sidebar.team.dark_aqua"},
    {DisplaySlot::TEAM_DARK_RED, 7, "TEAM_DARK_RED", "sidebar.team.dark_red"},
    {DisplaySlot::TEAM_DARK_PURPLE, 8, "TEAM_DARK_PURPLE", "sidebar.team.dark_purple"},
    {DisplaySlot::TEAM_GOLD, 9, "TEAM_GOLD", "sidebar.team.gold"},
    {DisplaySlot::TEAM_GRAY, 10, "TEAM_GRAY", "sidebar.team.gray"},
    {DisplaySlot::TEAM_DARK_GRAY, 11, "TEAM_DARK_GRAY", "sidebar.team.dark_gray"},
    {DisplaySlot::TEAM_BLUE, 12, "TEAM_BLUE", "sidebar.team.blue"},
    {DisplaySlot::TEAM_GREEN, 13, "TEAM_GREEN", "sidebar.team.green"},
    {DisplaySlot::TEAM_AQUA, 14, "TEAM_AQUA", "sidebar.team.aqua"},
    {DisplaySlot::TEAM_RED, 15, "TEAM_RED", "sidebar.team.red"},
    {DisplaySlot::TEAM_LIGHT_PURPLE, 16, "TEAM_LIGHT_PURPLE", "sidebar.team.light_purple"},
    {DisplaySlot::TEAM_YELLOW, 17, "TEAM_YELLOW", "sidebar.team.yellow"},
    {DisplaySlot::TEAM_WHITE, 18, "TEAM_WHITE", "sidebar.team.white"},
};

// id() — DisplaySlot.java line 40.
inline constexpr int32_t id(DisplaySlot s) {
    return DISPLAY_SLOTS[static_cast<std::size_t>(s)].id;
}

// getSerializedName() — DisplaySlot.java line 45.
inline constexpr std::string_view getSerializedName(DisplaySlot s) {
    return DISPLAY_SLOTS[static_cast<std::size_t>(s)].serialized;
}

// name() — Java enum constant identifier.
inline constexpr std::string_view name(DisplaySlot s) {
    return DISPLAY_SLOTS[static_cast<std::size_t>(s)].enumName;
}

// ordinal() — declaration index (equals id for this enum).
inline constexpr int32_t ordinal(DisplaySlot s) {
    return static_cast<int32_t>(s);
}

// BY_ID = ByIdMap.continuous(id, values(), ZERO).
// ZERO branch: 0<=id<length ? sortedValues[id] : sortedValues[0].
// Source: ByIdMap.java continuous()/createSortedArray (ids are sorted by id;
// here id==ordinal so sortedValues[i] is the constant whose id==i).
inline constexpr DisplaySlot byId(int32_t id) {
    if (id >= 0 && id < DISPLAY_SLOT_COUNT) {
        return DISPLAY_SLOTS[static_cast<std::size_t>(id)].slot;
    }
    return DisplaySlot::LIST; // sortedValues[0]
}

// CODEC = StringRepresentable.fromEnum(values()): linear name lookup over the
// serialized names; returns nullopt when no constant matches.
// Source: StringRepresentable.fromEnum / EnumCodec resolution by serialized name.
inline std::optional<DisplaySlot> byName(std::string_view serialized) {
    for (const auto& d : DISPLAY_SLOTS) {
        if (serialized == d.serialized) return d.slot;
    }
    return std::nullopt;
}

// The 22 ChatFormatting constants in declaration order, used only to drive the
// teamColorToSlot switch below. Values verbatim from ChatFormatting.java 18-39.
enum class ChatFormatting : int32_t {
    BLACK = 0,
    DARK_BLUE = 1,
    DARK_GREEN = 2,
    DARK_AQUA = 3,
    DARK_RED = 4,
    DARK_PURPLE = 5,
    GOLD = 6,
    GRAY = 7,
    DARK_GRAY = 8,
    BLUE = 9,
    GREEN = 10,
    AQUA = 11,
    RED = 12,
    LIGHT_PURPLE = 13,
    YELLOW = 14,
    WHITE = 15,
    OBFUSCATED = 16,
    BOLD = 17,
    STRIKETHROUGH = 18,
    UNDERLINE = 19,
    ITALIC = 20,
    RESET = 21,
};

// teamColorToSlot(ChatFormatting) — DisplaySlot.java lines 49-69, VERBATIM.
// The 16 colors map to TEAM_*; the 6 format codes (BOLD/ITALIC/UNDERLINE/RESET/
// OBFUSCATED/STRIKETHROUGH) yield null (nullopt).
inline constexpr std::optional<DisplaySlot> teamColorToSlot(ChatFormatting color) {
    switch (color) {
        case ChatFormatting::BLACK:        return DisplaySlot::TEAM_BLACK;
        case ChatFormatting::DARK_BLUE:    return DisplaySlot::TEAM_DARK_BLUE;
        case ChatFormatting::DARK_GREEN:   return DisplaySlot::TEAM_DARK_GREEN;
        case ChatFormatting::DARK_AQUA:    return DisplaySlot::TEAM_DARK_AQUA;
        case ChatFormatting::DARK_RED:     return DisplaySlot::TEAM_DARK_RED;
        case ChatFormatting::DARK_PURPLE:  return DisplaySlot::TEAM_DARK_PURPLE;
        case ChatFormatting::GOLD:         return DisplaySlot::TEAM_GOLD;
        case ChatFormatting::GRAY:         return DisplaySlot::TEAM_GRAY;
        case ChatFormatting::DARK_GRAY:    return DisplaySlot::TEAM_DARK_GRAY;
        case ChatFormatting::BLUE:         return DisplaySlot::TEAM_BLUE;
        case ChatFormatting::GREEN:        return DisplaySlot::TEAM_GREEN;
        case ChatFormatting::AQUA:         return DisplaySlot::TEAM_AQUA;
        case ChatFormatting::RED:          return DisplaySlot::TEAM_RED;
        case ChatFormatting::LIGHT_PURPLE: return DisplaySlot::TEAM_LIGHT_PURPLE;
        case ChatFormatting::YELLOW:       return DisplaySlot::TEAM_YELLOW;
        case ChatFormatting::WHITE:        return DisplaySlot::TEAM_WHITE;
        case ChatFormatting::BOLD:
        case ChatFormatting::ITALIC:
        case ChatFormatting::UNDERLINE:
        case ChatFormatting::RESET:
        case ChatFormatting::OBFUSCATED:
        case ChatFormatting::STRIKETHROUGH:
            return std::nullopt;
    }
    return std::nullopt;
}

}  // namespace mc::scores

#endif  // MCPP_WORLD_SCORES_DISPLAYSLOT_H
