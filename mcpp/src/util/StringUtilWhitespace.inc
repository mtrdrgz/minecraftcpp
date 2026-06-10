// Generated/verified by tools/StringUtilParity.java (it emits a WSCP row for every
// BMP code unit cp in 0..0xFFFF where the REAL net.minecraft.util.StringUtil.isWhitespace(cp)
// returns true, i.e. java.lang.Character.isWhitespace(cp) || Character.isSpaceChar(cp)).
// DO NOT EDIT BY HAND — it is the ground-truth dump of the JDK Unicode tables (Unicode
// 16.0 in JDK 25). The stringutil_parity gate re-checks this table against every such
// codepoint, so any drift fails the gate.
//
// Contents are exactly: the 9 whitespace control chars classified by
// Character.isWhitespace (U+0009..U+000D, U+001C..U+001F) UNION every BMP character whose
// general category is SPACE_SEPARATOR (Zs), LINE_SEPARATOR (Zl) or PARAGRAPH_SEPARATOR (Zp)
// — Character.isSpaceChar. (isSpaceChar includes the no-break spaces that isWhitespace
// excludes, so the union is simply controls + Zs/Zl/Zp.)
inline constexpr char16_t STRINGUTIL_WHITESPACE_CODE_UNITS[] = {
    // --- Character.isWhitespace control chars ---
    0x0009,  // CHARACTER TABULATION
    0x000A,  // LINE FEED
    0x000B,  // LINE TABULATION
    0x000C,  // FORM FEED
    0x000D,  // CARRIAGE RETURN
    0x001C,  // INFORMATION SEPARATOR FOUR (FILE SEPARATOR)
    0x001D,  // INFORMATION SEPARATOR THREE (GROUP SEPARATOR)
    0x001E,  // INFORMATION SEPARATOR TWO (RECORD SEPARATOR)
    0x001F,  // INFORMATION SEPARATOR ONE (UNIT SEPARATOR)
    // --- SPACE_SEPARATOR (Zs) ---
    0x0020,  // SPACE
    0x00A0,  // NO-BREAK SPACE
    0x1680,  // OGHAM SPACE MARK
    0x2000,  // EN QUAD
    0x2001,  // EM QUAD
    0x2002,  // EN SPACE
    0x2003,  // EM SPACE
    0x2004,  // THREE-PER-EM SPACE
    0x2005,  // FOUR-PER-EM SPACE
    0x2006,  // SIX-PER-EM SPACE
    0x2007,  // FIGURE SPACE
    0x2008,  // PUNCTUATION SPACE
    0x2009,  // THIN SPACE
    0x200A,  // HAIR SPACE
    0x202F,  // NARROW NO-BREAK SPACE
    0x205F,  // MEDIUM MATHEMATICAL SPACE
    0x3000,  // IDEOGRAPHIC SPACE
    // --- LINE_SEPARATOR (Zl) ---
    0x2028,  // LINE SEPARATOR
    // --- PARAGRAPH_SEPARATOR (Zp) ---
    0x2029,  // PARAGRAPH SEPARATOR
};
