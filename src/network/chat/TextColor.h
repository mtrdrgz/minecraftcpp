#pragma once

// 1:1 port of net.minecraft.network.chat.TextColor (TextColor.java) — the color behind every styled
// text run (chat, tooltips, GUI labels). Pure: parse/serialize + the named-color table derived from
// the certified ChatFormatting. Certified by text_color_parity.
//
// 1:1 details captured here:
//   * ctor masks value to 24 bits: value & 0xFFFFFF.
//   * serialize() = name (if a named color) else formatValue() = String.format("#%06X", value)
//     (UPPERCASE hex, zero-padded to 6).
//   * parseColor("#..."): Integer.parseInt(substring(1), 16) — signed parse; success only if the
//     parsed value is in [0, 0xFFFFFF], else error; a NumberFormatException (empty/non-hex/out-of-int-
//     range like "#80000000"/"#FFFFFFFF") -> error; a negative like "#-1" parses then fails the range.
//   * parseColor(name): NAMED_COLORS.get(name) — EXACT, CASE-SENSITIVE lookup (unlike ChatFormatting.
//     getByName's cleanName), keyed by ChatFormatting.getName() (lowercase, '_' kept). Only the 16
//     COLOR formats are present; "reset"/"bold"/unknown -> error.
//   * fromLegacyFormat(format) = the TextColor for a color format (value+name) else null.

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

#include "../../ChatFormatting.h"

namespace mc::chat {

struct TextColor {
    std::int32_t value = 0;
    bool hasName = false;
    std::string name;

    std::string serialize() const {
        if (hasName) return name;
        char buf[8];
        std::snprintf(buf, sizeof(buf), "#%06X", value);
        return buf;
    }
};

inline TextColor fromRgb(std::int32_t rgb) { return {rgb & 0xFFFFFF, false, ""}; }

// Mirror of Integer.parseInt(s, 16): signed, throws (here -> nullopt) on empty / invalid digit /
// value outside [Integer.MIN_VALUE, Integer.MAX_VALUE].
inline std::optional<std::int32_t> javaParseIntHex(const std::string& s) {
    if (s.empty()) return std::nullopt;
    bool negative = false;
    std::size_t i = 0;
    if (s[0] == '-') { negative = true; i = 1; }
    else if (s[0] == '+') { i = 1; }
    if (i >= s.size()) return std::nullopt;  // sign with no digits
    long long result = 0;
    for (; i < s.size(); ++i) {
        char c = s[i];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return std::nullopt;
        result = result * 16 + d;
        if (result > 5000000000LL) return std::nullopt;  // far past int range; guards long-input overflow
    }
    long long val = negative ? -result : result;
    if (val < -2147483648LL || val > 2147483647LL) return std::nullopt;
    return static_cast<std::int32_t>(val);
}

// NAMED_COLORS.get(name): exact match against each color format's getName().
inline std::optional<TextColor> namedColor(const std::string& name) {
    for (const auto& f : chat_formatting::VALUES) {
        if (chat_formatting::isColor(f) && chat_formatting::getName(f) == name)
            return TextColor{*chat_formatting::getColor(f) & 0xFFFFFF, true, chat_formatting::getName(f)};
    }
    return std::nullopt;
}

struct ParseResult {
    bool error = true;
    TextColor color;
};

inline ParseResult parseColor(const std::string& color) {
    if (!color.empty() && color[0] == '#') {
        auto v = javaParseIntHex(color.substr(1));
        if (v && *v >= 0 && *v <= 16777215) return {false, fromRgb(*v)};
        return {true, {}};
    }
    auto nc = namedColor(color);
    if (nc) return {false, *nc};
    return {true, {}};
}

// fromLegacyFormat: a TextColor for a color format (value + name) else null.
inline std::optional<TextColor> fromLegacyFormat(const ChatFormattingData& f) {
    if (chat_formatting::isColor(f))
        return TextColor{*chat_formatting::getColor(f) & 0xFFFFFF, true, chat_formatting::getName(f)};
    return std::nullopt;
}

}  // namespace mc::chat
