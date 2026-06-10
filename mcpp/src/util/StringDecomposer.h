#pragma once

// 1:1 port of the PURE, fixed-string methods of net.minecraft.util.StringDecomposer
// (Minecraft 26.1.2):
//
//   iterate(string, style, sink)
//   iterateBackwards(string, style, sink)
//   iterateFormatted(string, style, sink)                     -> offset 0, current==reset==style
//   iterateFormatted(string, offset, style, sink)             -> current==reset==style
//   iterateFormatted(string, offset, currentStyle, resetStyle, sink)
//   filterBrokenSurrogates(input)                             -> iterate over Style.EMPTY
//
// NOT ported (coupled to FormattedText components / Style codecs / chat machinery, which
// pull in registries + the component tree): the FormattedText overloads
//   iterateFormatted(FormattedText, rootStyle, sink) and getPlainText(FormattedText).
// They are listed as unported in the parity gate's notes.
//
// ---------------------------------------------------------------------------------------
// Java Strings are UTF-16. String.length()/charAt() operate on 16-bit code UNITS, and the
// surrogate handling in StringDecomposer is over code units, so we model strings as
// std::u16string and iterate char16_t to stay bit-exact. Code points emitted to the sink
// are ints (a combined surrogate pair, or U+FFFD == 65533 for a broken/lone surrogate).
//
// Character.* helpers used (verbatim contract, JDK):
//   isHighSurrogate(c) == c in [0xD800, 0xDBFF]
//   isLowSurrogate(c)  == c in [0xDC00, 0xDFFF]
//   isSurrogate(c)     == c in [0xD800, 0xDFFF]
//   toCodePoint(hi,lo) == ((hi - 0xD800) << 10) + (lo - 0xDC00) + 0x10000
//
// Section sign handling (iterateFormatted): char 167 ('§') consumes the next code unit as
// a legacy format code via ChatFormatting.getByCode(code); a recognized code mutates the
// running Style via Style.applyLegacyFormat (RESET restores resetStyle), and BOTH the '§'
// and the code unit are skipped (i++ past the code; the for-loop's i++ skips the '§').
//
// The only Style state observable through the decomposition (the running style is built up
// solely by applyLegacyFormat starting from EMPTY / the caller's style) is the legacy color
// + the five legacy flags. We model exactly that as DecomposerStyle. We deliberately do NOT
// model clickEvent/hoverEvent/insertion/font/shadowColor: applyLegacyFormat never touches
// them and they are irrelevant to the (pos, style, codePoint) stream this gate certifies.
//
// Java source (StringDecomposer.java, ChatFormatting.java, Style.applyLegacyFormat,
// TextColor.fromLegacyFormat):
//   feedChar(style, out, pos, ch): isSurrogate(ch) ? out.accept(pos, style, 65533)
//                                                   : out.accept(pos, style, ch)
//   iterateFormatted(string, offset, currentStyle, resetStyle, out):
//     style = currentStyle;
//     for (i = offset; i < size; i++) {
//       ch = string.charAt(i);
//       if (ch == 167) {
//         if (i + 1 >= size) break;
//         code = string.charAt(i + 1);
//         fmt = ChatFormatting.getByCode(code);
//         if (fmt != null) style = (fmt == RESET) ? resetStyle : style.applyLegacyFormat(fmt);
//         i++;
//       } else if (isHighSurrogate(ch)) { ...surrogate pairing, emit pair or 65533... }
//       else if (!feedChar(style, out, i, ch)) return false;
//     }
//     return true;
//
//   ChatFormatting.getByCode(code): toLowerCase(code), linear scan of the 22 enum codes.
//   Style.applyLegacyFormat(fmt): OBFUSCATED/BOLD/STRIKETHROUGH/UNDERLINE/ITALIC set that one
//     flag true (others kept); RESET -> EMPTY; default (a color) -> clear ALL five flags and
//     set color = TextColor.fromLegacyFormat(fmt).
//   TextColor.fromLegacyFormat(fmt): the per-color RGB (masked & 0xFFFFFF) for the 16 color
//     codes; null for the 5 format codes and RESET.
//
// Certified by string_decomposer_parity (ground truth: tools/StringDecomposerParity.java vs
// the real net.minecraft.util.StringDecomposer).

#include <cstdint>
#include <functional>
#include <string>

namespace mc::util::stringdecomposer {

// --- Character.* surrogate contract (JDK, verbatim) ------------------------------------
inline bool isHighSurrogate(char16_t c) { return c >= 0xD800 && c <= 0xDBFF; }
inline bool isLowSurrogate(char16_t c) { return c >= 0xDC00 && c <= 0xDFFF; }
inline bool isSurrogate(char16_t c) { return c >= 0xD800 && c <= 0xDFFF; }
inline int32_t toCodePoint(char16_t hi, char16_t lo) {
    // Character.toCodePoint: ((high << 10) + low) + (MIN_SUPPLEMENTARY_CODE_POINT
    //                         - (MIN_HIGH_SURROGATE << 10) - MIN_LOW_SURROGATE)
    return ((static_cast<int32_t>(hi) << 10) + static_cast<int32_t>(lo)) +
           (0x10000 - (0xD800 << 10) - 0xDC00);
}

constexpr int32_t REPLACEMENT_CHAR = 65533;  // U+FFFD '�'

// --- ChatFormatting (only the fields decomposition observes) ---------------------------
// The legacy format codes, in enum-declaration order (matters only for getByCode's linear
// scan, but the codes are unique so order is immaterial to the result).
enum class Formatting {
    BLACK, DARK_BLUE, DARK_GREEN, DARK_AQUA, DARK_RED, DARK_PURPLE, GOLD, GRAY,
    DARK_GRAY, BLUE, GREEN, AQUA, RED, LIGHT_PURPLE, YELLOW, WHITE,
    OBFUSCATED, BOLD, STRIKETHROUGH, UNDERLINE, ITALIC, RESET,
    NONE  // sentinel: getByCode found nothing (null)
};

struct FormattingEntry {
    char16_t code;     // the lowercase format code char
    int32_t color;     // RGB (already masked &0xFFFFFF in fromLegacyFormat path); -1 == null
    bool isColor;      // true for the 16 named colors (default branch of applyLegacyFormat)
};

// Table verbatim from ChatFormatting.java (name, code, id, color) for colors; (name, code,
// isFormat) for the 5 format flags; RESET. Only code/color/isColor are observable here.
// The color RGB values are the exact constructor literals.
inline const FormattingEntry& formattingEntry(Formatting f) {
    static const FormattingEntry table[] = {
        {u'0', 0, true},          // BLACK
        {u'1', 170, true},        // DARK_BLUE
        {u'2', 43520, true},      // DARK_GREEN
        {u'3', 43690, true},      // DARK_AQUA
        {u'4', 11141120, true},   // DARK_RED
        {u'5', 11141290, true},   // DARK_PURPLE
        {u'6', 16755200, true},   // GOLD
        {u'7', 11184810, true},   // GRAY
        {u'8', 5592405, true},    // DARK_GRAY
        {u'9', 5592575, true},    // BLUE
        {u'a', 5635925, true},    // GREEN
        {u'b', 5636095, true},    // AQUA
        {u'c', 16733525, true},   // RED
        {u'd', 16733695, true},   // LIGHT_PURPLE
        {u'e', 16777045, true},   // YELLOW
        {u'f', 16777215, true},   // WHITE
        {u'k', -1, false},        // OBFUSCATED
        {u'l', -1, false},        // BOLD
        {u'm', -1, false},        // STRIKETHROUGH
        {u'n', -1, false},        // UNDERLINE
        {u'o', -1, false},        // ITALIC
        {u'r', -1, false},        // RESET (color null; handled specially before color path)
    };
    return table[static_cast<int>(f)];
}

// Character.toLowerCase, but the format codes are all ASCII; this matches the JDK for the
// ASCII range, which is the only range that can ever match a format code.
inline char16_t toLowerAscii(char16_t c) {
    return (c >= u'A' && c <= u'Z') ? static_cast<char16_t>(c - u'A' + u'a') : c;
}

// ChatFormatting.getByCode(code): sanitized = Character.toLowerCase(code); linear scan.
// Returns Formatting::NONE for "null" (no match). Note Character.toLowerCase over the full
// Unicode range could lowercase a non-ASCII char to an ASCII code char in pathological
// cases; the JDK lowercase of any code unit that maps onto '0'..'9'/'a'..'z' is only the
// ASCII upper/lower forms, so ASCII-folding is exact for matching purposes.
inline Formatting getByCode(char16_t code) {
    char16_t sanitized = toLowerAscii(code);
    for (int idx = 0; idx <= static_cast<int>(Formatting::RESET); ++idx) {
        if (formattingEntry(static_cast<Formatting>(idx)).code == sanitized) {
            return static_cast<Formatting>(idx);
        }
    }
    return Formatting::NONE;
}

// --- The observable Style state (color + legacy flags), built only via applyLegacyFormat -
struct DecomposerStyle {
    bool hasColor = false;   // color != null
    int32_t color = 0;       // RGB value (masked &0xFFFFFF) when hasColor
    bool bold = false;
    bool italic = false;
    bool underlined = false;
    bool strikethrough = false;
    bool obfuscated = false;

    bool operator==(const DecomposerStyle& o) const {
        return hasColor == o.hasColor && (!hasColor || color == o.color) && bold == o.bold &&
               italic == o.italic && underlined == o.underlined &&
               strikethrough == o.strikethrough && obfuscated == o.obfuscated;
    }
};

inline DecomposerStyle EMPTY_STYLE() { return DecomposerStyle{}; }

// Style.applyLegacyFormat(format): exact branch structure from Style.java.
// Precondition: format != RESET (RESET is short-circuited to resetStyle by the caller, just
// as Style.applyLegacyFormat returns Style.EMPTY for RESET). We assert nothing; the parity
// driver never feeds RESET here.
inline DecomposerStyle applyLegacyFormat(const DecomposerStyle& in, Formatting format) {
    DecomposerStyle s = in;
    switch (format) {
        case Formatting::OBFUSCATED:
            s.obfuscated = true;
            break;
        case Formatting::BOLD:
            s.bold = true;
            break;
        case Formatting::STRIKETHROUGH:
            s.strikethrough = true;
            break;
        case Formatting::UNDERLINE:
            s.underlined = true;
            break;
        case Formatting::ITALIC:
            s.italic = true;
            break;
        case Formatting::RESET:
            return EMPTY_STYLE();  // Style.applyLegacyFormat(RESET) -> Style.EMPTY
        default: {
            // a color: clear all flags, set color = TextColor.fromLegacyFormat(format)
            s.obfuscated = false;
            s.bold = false;
            s.strikethrough = false;
            s.underlined = false;
            s.italic = false;
            const FormattingEntry& e = formattingEntry(format);
            s.hasColor = true;
            s.color = e.color & 0xFFFFFF;  // TextColor constructor: value & 16777215
            break;
        }
    }
    return s;
}

// --- The sink: void accept(pos, style, codePoint) -> bool (false aborts) ----------------
// We pass a const ref to DecomposerStyle, mirroring the Style passed to FormattedCharSink.
using Sink = std::function<bool(int32_t pos, const DecomposerStyle& style, int32_t codePoint)>;

// feedChar: surrogate -> 65533, else the char value.
inline bool feedChar(const DecomposerStyle& style, const Sink& out, int32_t pos, char16_t ch) {
    return isSurrogate(ch) ? out(pos, style, REPLACEMENT_CHAR)
                           : out(pos, style, static_cast<int32_t>(ch));
}

// iterate(string, style, output): forward, no format codes.
inline bool iterate(const std::u16string& string, const DecomposerStyle& style, const Sink& out) {
    int32_t size = static_cast<int32_t>(string.size());
    for (int32_t i = 0; i < size; i++) {
        char16_t ch = string[i];
        if (isHighSurrogate(ch)) {
            if (i + 1 >= size) {
                if (!out(i, style, REPLACEMENT_CHAR)) return false;
                break;
            }
            char16_t low = string[i + 1];
            if (isLowSurrogate(low)) {
                if (!out(i, style, toCodePoint(ch, low))) return false;
                i++;
            } else if (!out(i, style, REPLACEMENT_CHAR)) {
                return false;
            }
        } else if (!feedChar(style, out, i, ch)) {
            return false;
        }
    }
    return true;
}

// iterateBackwards(string, style, output): reverse, no format codes.
inline bool iterateBackwards(const std::u16string& string, const DecomposerStyle& style,
                             const Sink& out) {
    int32_t size = static_cast<int32_t>(string.size());
    for (int32_t i = size - 1; i >= 0; i--) {
        char16_t ch = string[i];
        if (isLowSurrogate(ch)) {
            if (i - 1 < 0) {
                if (!out(0, style, REPLACEMENT_CHAR)) return false;
                break;
            }
            char16_t high = string[i - 1];
            if (isHighSurrogate(high)) {
                if (!out(--i, style, toCodePoint(high, ch))) return false;
            } else if (!out(i, style, REPLACEMENT_CHAR)) {
                return false;
            }
        } else if (!feedChar(style, out, i, ch)) {
            return false;
        }
    }
    return true;
}

// iterateFormatted(string, offset, currentStyle, resetStyle, output): the full variant.
inline bool iterateFormatted(const std::u16string& string, int32_t offset,
                             const DecomposerStyle& currentStyle,
                             const DecomposerStyle& resetStyle, const Sink& out) {
    int32_t size = static_cast<int32_t>(string.size());
    DecomposerStyle style = currentStyle;
    for (int32_t i = offset; i < size; i++) {
        char16_t ch = string[i];
        if (ch == 167) {  // '§'
            if (i + 1 >= size) break;
            char16_t code = string[i + 1];
            Formatting formatting = getByCode(code);
            if (formatting != Formatting::NONE) {
                style = (formatting == Formatting::RESET) ? resetStyle
                                                          : applyLegacyFormat(style, formatting);
            }
            i++;
        } else if (isHighSurrogate(ch)) {
            if (i + 1 >= size) {
                if (!out(i, style, REPLACEMENT_CHAR)) return false;
                break;
            }
            char16_t low = string[i + 1];
            if (isLowSurrogate(low)) {
                if (!out(i, style, toCodePoint(ch, low))) return false;
                i++;
            } else if (!out(i, style, REPLACEMENT_CHAR)) {
                return false;
            }
        } else if (!feedChar(style, out, i, ch)) {
            return false;
        }
    }
    return true;
}

// iterateFormatted(string, offset, style, output): current == reset == style.
inline bool iterateFormatted(const std::u16string& string, int32_t offset,
                             const DecomposerStyle& style, const Sink& out) {
    return iterateFormatted(string, offset, style, style, out);
}

// iterateFormatted(string, style, output): offset 0.
inline bool iterateFormatted(const std::u16string& string, const DecomposerStyle& style,
                             const Sink& out) {
    return iterateFormatted(string, 0, style, out);
}

// filterBrokenSurrogates(input): iterate over Style.EMPTY, appending each code point.
// Returns the rebuilt UTF-16 string (broken surrogates replaced by U+FFFD).
inline std::u16string filterBrokenSurrogates(const std::u16string& input) {
    std::u16string out;
    DecomposerStyle empty = EMPTY_STYLE();
    iterate(input, empty, [&out](int32_t, const DecomposerStyle&, int32_t cp) -> bool {
        // StringBuilder.appendCodePoint: BMP -> one char unit; supplementary -> surrogate pair.
        if (cp <= 0xFFFF) {
            out.push_back(static_cast<char16_t>(cp));
        } else {
            int32_t v = cp - 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (v >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (v & 0x3FF)));
        }
        return true;
    });
    return out;
}

}  // namespace mc::util::stringdecomposer
