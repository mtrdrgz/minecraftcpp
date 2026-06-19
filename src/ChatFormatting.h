// 1:1 port of net.minecraft.ChatFormatting (Minecraft 26.1.2).
//
// Source: 26.1.2/src/net/minecraft/ChatFormatting.java
//
// Pure-data enum of 22 constants. Each constant carries:
//   name     — the first ctor arg (a String, e.g. "DARK_BLUE"); equals the enum
//              constant's Java name() for every constant.
//   code     — the single '§'-prefix char (getChar()).
//   isFormat — true for the 5 format codes (OBFUSCATED..ITALIC), false otherwise.
//   id       — getId(); 0..15 for the 16 colors, -1 for formats and RESET.
//   color    — getColor(); a @Nullable Integer RGB. Present (non-null) ONLY for
//              the 16 colors; null for the 5 formats and RESET.
//
// The two private ctors map onto the full ctor verbatim:
//   ChatFormatting(name, code, id, color)      -> isFormat=false           (colors + RESET)
//   ChatFormatting(name, code, isFormat)       -> id=-1, color=null        (formats)
//
// Methods ported (all VERBATIM from the Java):
//   getChar() / getId() / isFormat() / getColor()
//   isColor()          = !isFormat && this != RESET
//   getName()          = name().toLowerCase(Locale.ROOT)        (keeps '_')
//   getSerializedName()= getName()
//   getByCode(char)    = lowercase(code), linear scan over values() by code
//   getByName(String)  = FORMATTING_BY_NAME.get(cleanName(name))
//   getById(int)       = id<0 ? RESET : linear scan over values() by id else null
//   cleanName(String)  = toLowerCase(ROOT).replaceAll("[^a-z]", "")
//
// FORMATTING_BY_NAME is keyed on cleanName(format.name). cleanName strips every
// non-[a-z] char, so "DARK_BLUE" -> "darkblue". cleanName is also applied to the
// getByName() argument. Note: because two distinct constants never collapse to
// the same cleaned key, the map is a clean 1:1 (no toMap merge collision).
//
// NOT ported (Java-runtime-only; no C++ counterpart needed here, listed as such):
//   CODEC / COLOR_CODEC (Mojang serialization Codec), stripFormatting / the
//   STRIP_FORMATTING_PATTERN regex, getNames(boolean,boolean) (uses Guava Lists),
//   toString() (= "§"+code; trivially "§"+getChar() if ever needed).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mc {

// One ChatFormatting enum constant, fields verbatim from the declaration.
struct ChatFormattingData {
    std::int32_t ordinal;       // 0-based declaration index (== enum ordinal())
    std::string_view name;      // first ctor arg / Java name() (e.g. "DARK_BLUE")
    char code;                  // getChar()
    bool isFormat;              // isFormat()
    std::int32_t id;            // getId() (-1 for formats and RESET)
    bool hasColor;              // getColor() != null
    std::int32_t color;         // getColor() value (valid iff hasColor)
};

namespace chat_formatting {

// '§' (section sign) — PREFIX_CODE in the Java. (U+00A7.)
inline constexpr char32_t PREFIX_CODE = U'§';

// Declaration order == ordinal order, exactly as in ChatFormatting.java.
//   - The 16 colors use ctor (name, code, id, color): isFormat=false, color present.
//   - The 5 formats use ctor (name, code, isFormat=true): id=-1, color=null.
//   - RESET uses ctor (name, code, id=-1, color=null): isFormat=false, color=null.
inline constexpr std::array<ChatFormattingData, 22> VALUES{{
    // ordinal, name,             code, isFormat, id, hasColor, color
    {0,  "BLACK",        '0', false,  0, true,        0},
    {1,  "DARK_BLUE",    '1', false,  1, true,      170},
    {2,  "DARK_GREEN",   '2', false,  2, true,    43520},
    {3,  "DARK_AQUA",    '3', false,  3, true,    43690},
    {4,  "DARK_RED",     '4', false,  4, true, 11141120},
    {5,  "DARK_PURPLE",  '5', false,  5, true, 11141290},
    {6,  "GOLD",         '6', false,  6, true, 16755200},
    {7,  "GRAY",         '7', false,  7, true, 11184810},
    {8,  "DARK_GRAY",    '8', false,  8, true,  5592405},
    {9,  "BLUE",         '9', false,  9, true,  5592575},
    {10, "GREEN",        'a', false, 10, true,  5635925},
    {11, "AQUA",         'b', false, 11, true,  5636095},
    {12, "RED",          'c', false, 12, true, 16733525},
    {13, "LIGHT_PURPLE", 'd', false, 13, true, 16733695},
    {14, "YELLOW",       'e', false, 14, true, 16777045},
    {15, "WHITE",        'f', false, 15, true, 16777215},
    {16, "OBFUSCATED",   'k', true,  -1, false,       0},
    {17, "BOLD",         'l', true,  -1, false,       0},
    {18, "STRIKETHROUGH",'m', true,  -1, false,       0},
    {19, "UNDERLINE",    'n', true,  -1, false,       0},
    {20, "ITALIC",       'o', true,  -1, false,       0},
    {21, "RESET",        'r', false, -1, false,       0},
}};

inline constexpr std::int32_t COUNT = 22;

// ----- instance accessors (operate on a ChatFormattingData) -----

inline constexpr char getChar(const ChatFormattingData& f) { return f.code; }
inline constexpr std::int32_t getId(const ChatFormattingData& f) { return f.id; }
inline constexpr bool isFormat(const ChatFormattingData& f) { return f.isFormat; }

// isColor() = !this.isFormat && this != RESET
//   RESET is the only non-format that is not a color, so test it by ordinal.
inline constexpr bool isColor(const ChatFormattingData& f) {
    return !f.isFormat && f.ordinal != 21 /* RESET */;
}

// getColor(): @Nullable Integer. std::nullopt models the Java null.
inline constexpr std::optional<std::int32_t> getColor(const ChatFormattingData& f) {
    if (f.hasColor) return f.color;
    return std::nullopt;
}

// Character.toLowerCase, restricted to the only inputs that matter for codes:
// the format chars are ASCII letters/digits, and getByCode lowercases the input.
// We mirror Java's full Character.toLowerCase only over the ASCII range, which is
// all this enum's codes ever use; outside it the value is returned unchanged
// (matching Java for the non-letter inputs we feed and never matching a code).
inline constexpr char asciiToLower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// getName() = name().toLowerCase(Locale.ROOT). The enum name() is `name` here.
// Only ASCII upper-case letters and '_' appear, so ROOT-lowercasing is ASCII.
inline std::string getName(const ChatFormattingData& f) {
    std::string out(f.name);
    for (char& c : out) c = asciiToLower(c);
    return out;
}

// getSerializedName() = getName()
inline std::string getSerializedName(const ChatFormattingData& f) { return getName(f); }

// toString() = "§" + code  (kept for completeness; UTF-8 encoding of U+00A7).
inline std::string toString(const ChatFormattingData& f) {
    std::string out;
    out += static_cast<char>(0xC2);
    out += static_cast<char>(0xA7);
    out += f.code;
    return out;
}

// ----- static lookups -----

// getByCode(char): lowercase, linear scan over values() by code; null -> nullptr.
inline const ChatFormattingData* getByCode(char code) {
    char sanitized = asciiToLower(code);
    for (const auto& f : VALUES) {
        if (f.code == sanitized) return &f;
    }
    return nullptr;
}

// getById(int): id<0 -> RESET; else linear scan over values() by id; null -> nullptr.
inline const ChatFormattingData* getById(std::int32_t id) {
    if (id < 0) return &VALUES[21];  // RESET
    for (const auto& f : VALUES) {
        if (f.id == id) return &f;
    }
    return nullptr;
}

// cleanName(name) = name.toLowerCase(Locale.ROOT).replaceAll("[^a-z]", "").
// Lowercase first (ASCII inputs only here), then drop every char outside [a-z].
inline std::string cleanName(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    for (char ch : name) {
        char lc = asciiToLower(ch);
        if (lc >= 'a' && lc <= 'z') out += lc;
    }
    return out;
}

// getByName(name): FORMATTING_BY_NAME.get(cleanName(name)). The map is keyed by
// cleanName(format.name); we reproduce it as a linear scan over the same key.
// Null input -> nullptr (caller models null as not-found here). No-match -> nullptr.
inline const ChatFormattingData* getByName(std::string_view name) {
    std::string key = cleanName(name);
    for (const auto& f : VALUES) {
        if (cleanName(f.name) == key) return &f;
    }
    return nullptr;
}

}  // namespace chat_formatting
}  // namespace mc
