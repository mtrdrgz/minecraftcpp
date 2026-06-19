#pragma once

// 1:1 port of the StringUtil helpers NOT covered by util/StringUtil.h
// (net.minecraft.util.StringUtil, Minecraft 26.1.2):
//   stripColor, filterText (both overloads), isValidPlayerName.
//
// This is a SEPARATE header so it does not touch the already-certified StringUtil.h.
// It reuses isAllowedChatCharacter from StringUtil.h for filterText (verbatim Java
// delegation). The remaining still-unported methods of StringUtil are:
//   formatTickDuration (needs Mth.floor + Java String.format / Locale.ROOT plumbing),
//   isNullOrEmpty/truncateStringIfNecessary/lineCount/trimChatMessage/isAllowedChatCharacter/
//   isWhitespace/isBlank  -> already in StringUtil.h (re-verified by this gate),
//   endsWithNewLine        -> LINE_END_PATTERN "(?:\r\n|\v)$" (not requested here).
//
// Java Strings are UTF-16; String.length()/substring()/chars()/toCharArray() operate on
// 16-bit code UNITS, so we model strings as std::u16string and iterate char16_t.
//
// Java source (StringUtil.java):
//   private static final Pattern STRIP_COLOR_PATTERN = Pattern.compile("(?i)\\u00A7[0-9A-FK-OR]");
//   public static String stripColor(String input) {
//       return STRIP_COLOR_PATTERN.matcher(input).replaceAll("");
//   }
//   public static boolean isValidPlayerName(String name) {
//       return name.length() > 16 ? false
//            : name.chars().filter(c -> c <= 32 || c >= 127).findAny().isEmpty();
//   }
//   public static String filterText(String input) { return filterText(input, false); }
//   public static String filterText(String input, boolean multiline) {
//       StringBuilder builder = new StringBuilder();
//       for (char c : input.toCharArray()) {
//           if (isAllowedChatCharacter(c))      builder.append(c);
//           else if (multiline && c == '\n')    builder.append(c);
//       }
//       return builder.toString();
//   }

#include <cstdint>
#include <string>

#include "StringUtil.h"  // for mc::util::stringutil::isAllowedChatCharacter

namespace mc::util::stringutil {

// --- stripColor ----------------------------------------------------------------------
// Pattern: (?i)§[0-9A-FK-OR]
//   - § is the section sign '§' (a single code unit).
//   - [0-9A-FK-OR] is, case-insensitively (?i):
//       digits  '0'..'9'
//       letters 'A'..'F' / 'a'..'f'   (range A-F)
//               'K'..'O' / 'k'..'o'   (range K-O)
//               'R'      / 'r'        (literal R)
// matcher.replaceAll("") removes every non-overlapping left-to-right match of this
// 2-code-unit pattern. After a match the regex engine resumes scanning AFTER the
// consumed 2 units (matches do not overlap).
inline bool isColorCodeChar(char16_t c) {
    // case-insensitive membership of [0-9A-FK-OR]
    if (c >= u'0' && c <= u'9') return true;
    char16_t u = c;
    // ASCII lower -> upper fold (only A-Z / a-z matter here).
    if (u >= u'a' && u <= u'z') u = static_cast<char16_t>(u - (u'a' - u'A'));
    if (u >= u'A' && u <= u'F') return true;  // A-F
    if (u >= u'K' && u <= u'O') return true;  // K-O
    if (u == u'R') return true;               // R
    return false;
}

inline std::u16string stripColor(const std::u16string& input) {
    std::u16string out;
    out.reserve(input.size());
    const size_t n = input.size();
    size_t i = 0;
    while (i < n) {
        if (input[i] == 0x00A7 && i + 1 < n && isColorCodeChar(input[i + 1])) {
            // Match "§<code>": drop both code units, resume after them (no overlap).
            i += 2;
        } else {
            out.push_back(input[i]);
            i += 1;
        }
    }
    return out;
}

// --- isValidPlayerName ---------------------------------------------------------------
// name.length() > 16 ? false : name.chars().filter(c -> c <= 32 || c >= 127).findAny().isEmpty()
// i.e. valid iff length <= 16 AND no UTF-16 code unit is <= 32 or >= 127.
inline bool isValidPlayerName(const std::u16string& name) {
    if (name.length() > 16) {
        return false;
    }
    for (char16_t c : name) {
        // String.chars() zero-extends each code unit into an int in 0..0xFFFF.
        int32_t v = static_cast<int32_t>(static_cast<uint16_t>(c));
        if (v <= 32 || v >= 127) {
            return false;  // a "bad" char exists -> filter().findAny() non-empty
        }
    }
    return true;  // findAny().isEmpty()
}

// --- filterText ----------------------------------------------------------------------
inline std::u16string filterText(const std::u16string& input, bool multiline) {
    std::u16string out;
    out.reserve(input.size());
    for (char16_t c : input) {
        // isAllowedChatCharacter widens the char to int (0..0xFFFF) just like Java.
        if (isAllowedChatCharacter(static_cast<int32_t>(static_cast<uint16_t>(c)))) {
            out.push_back(c);
        } else if (multiline && c == u'\n') {
            out.push_back(c);
        }
    }
    return out;
}

inline std::u16string filterText(const std::u16string& input) {
    return filterText(input, false);
}

}  // namespace mc::util::stringutil
