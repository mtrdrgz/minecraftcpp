#pragma once

// 1:1 port of the PURE methods of net.minecraft.util.StringUtil (Minecraft 26.1.2):
//   isNullOrEmpty, truncateStringIfNecessary, lineCount, trimChatMessage,
//   isAllowedChatCharacter, isWhitespace, isBlank.
//
// Java Strings are UTF-16; String.length()/substring() and String.chars() operate on
// 16-bit code UNITS (not code points). To stay bit-exact we model strings as
// std::u16string and iterate char16_t. The methods that are NOT ported here
// (formatTickDuration, stripColor, endsWithNewLine, isValidPlayerName, filterText) are
// listed as unported in the parity gate's notes.
//
// Java source (StringUtil.java):
//   isNullOrEmpty(s)        -> StringUtils.isEmpty(s)      == (s==null || s.length()==0)
//   truncateStringIfNecessary(s, maxLength, addDotDotDotIfTruncated):
//       if (s.length() <= maxLength) return s;
//       else return addDots && maxLength > 3 ? s.substring(0, maxLength-3) + "..."
//                                            : s.substring(0, maxLength);
//   lineCount(s):
//       if (s.isEmpty()) return 0;
//       Matcher m = Pattern.compile("\\r\\n|\\v").matcher(s);  // \v == vertical whitespace
//       int count = 1; while (m.find()) count++; return count;
//   trimChatMessage(message) -> truncateStringIfNecessary(message, 256, false)
//   isAllowedChatCharacter(ch) -> ch != 167 && ch >= 32 && ch != 127
//   isWhitespace(cp) -> Character.isWhitespace(cp) || Character.isSpaceChar(cp)
//   isBlank(string)  -> (string != null && !string.isEmpty())
//                          ? string.chars().allMatch(StringUtil::isWhitespace) : true
//
// Certified by stringutil_parity (ground truth: tools/StringUtilParity.java vs the real
// net.minecraft.util.StringUtil).

#include <cstdint>
#include <optional>
#include <string>

namespace mc::util::stringutil {

// --- whitespace classification table (ground-truth dump of the JDK Unicode tables) ----
#include "StringUtilWhitespace.inc"

// java.lang.Character.isWhitespace(cp) || Character.isSpaceChar(cp).
//
// StringUtil.isBlank feeds this with String.chars() values, which are UTF-16 code UNITS
// (ints in 0..0xFFFF). The real isWhitespace takes an int code point; for the values
// isBlank can produce (0..0xFFFF, a single code unit) the classification is fully
// determined by the embedded BMP table. Values above 0xFFFF (supplementary code points)
// never reach this path from isBlank (chars() never yields them), and none of the
// supplementary code points are whitespace anyway, so a code-unit lookup is exact for
// every input isBlank can deliver.
inline bool isWhitespace(int32_t codepoint) {
    if (codepoint < 0 || codepoint > 0xFFFF) {
        // No supplementary (or negative) code point is whitespace under the JDK contract.
        return false;
    }
    char16_t c = static_cast<char16_t>(codepoint);
    for (char16_t w : STRINGUTIL_WHITESPACE_CODE_UNITS) {
        if (w == c) return true;
    }
    return false;
}

// public static boolean isNullOrEmpty(final String s) { return StringUtils.isEmpty(s); }
// StringUtils.isEmpty == (s == null || s.length() == 0). We model "null" as a missing
// optional and "" as an empty string.
inline bool isNullOrEmpty(const std::optional<std::u16string>& s) {
    return !s.has_value() || s->empty();
}
inline bool isNullOrEmpty(const std::u16string& s) { return s.empty(); }

// public static String truncateStringIfNecessary(final String s, final int maxLength,
//                                                final boolean addDotDotDotIfTruncated)
inline std::u16string truncateStringIfNecessary(const std::u16string& s, int32_t maxLength,
                                                bool addDotDotDotIfTruncated) {
    // if (s.length() <= maxLength) return s;
    if (static_cast<int64_t>(s.length()) <= static_cast<int64_t>(maxLength)) {
        return s;
    }
    // return addDots && maxLength > 3 ? s.substring(0, maxLength-3) + "..." : s.substring(0, maxLength);
    if (addDotDotDotIfTruncated && maxLength > 3) {
        return s.substr(0, static_cast<size_t>(maxLength - 3)) + u"...";
    }
    return s.substr(0, static_cast<size_t>(maxLength));
}

// Java regex \v == a single vertical-whitespace character:
//   [\n\x0B\f\r\x85  ]  == { U+000A, U+000B, U+000C, U+000D, U+0085, U+2028, U+2029 }
inline bool isVerticalWhitespace(char16_t c) {
    return c == 0x000A || c == 0x000B || c == 0x000C || c == 0x000D || c == 0x0085 ||
           c == 0x2028 || c == 0x2029;
}

// public static int lineCount(final String s)
// Mirrors Pattern.compile("\\r\\n|\\v").matcher(s) finding non-overlapping matches left
// to right: at each position try "\r\n" (consumes 2), else "\v" (consumes 1). count =
// 1 + number of matches; empty string => 0.
inline int32_t lineCount(const std::u16string& s) {
    if (s.empty()) {
        return 0;
    }
    int32_t count = 1;
    size_t i = 0;
    const size_t n = s.size();
    while (i < n) {
        // Alternation \r\n | \v, leftmost-first: \r\n is tried before \v.
        if (s[i] == 0x000D && i + 1 < n && s[i + 1] == 0x000A) {
            count++;
            i += 2;  // \r\n consumed as a single match
        } else if (isVerticalWhitespace(s[i])) {
            count++;
            i += 1;  // single vertical-whitespace char matched by \v
        } else {
            i += 1;  // no match at this position; advance
        }
    }
    return count;
}

// public static String trimChatMessage(final String message)
inline std::u16string trimChatMessage(const std::u16string& message) {
    return truncateStringIfNecessary(message, 256, false);
}

// public static boolean isAllowedChatCharacter(final int ch)
inline bool isAllowedChatCharacter(int32_t ch) {
    return ch != 167 && ch >= 32 && ch != 127;
}

// public static boolean isBlank(final String string)
// string != null && !string.isEmpty() ? string.chars().allMatch(isWhitespace) : true
inline bool isBlank(const std::optional<std::u16string>& string) {
    if (string.has_value() && !string->empty()) {
        for (char16_t c : *string) {
            // string.chars() yields code units as ints (zero-extended to 0..0xFFFF).
            if (!isWhitespace(static_cast<int32_t>(static_cast<uint16_t>(c)))) {
                return false;
            }
        }
        return true;  // allMatch over a non-empty sequence
    }
    return true;  // null or empty -> true
}
inline bool isBlank(const std::u16string& string) { return isBlank(std::optional<std::u16string>(string)); }

}  // namespace mc::util::stringutil
