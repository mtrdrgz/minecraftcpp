import net.minecraft.util.StringUtil;

// Ground truth for mcpp/src/util/StringUtil.h. Emits tab-separated rows from the REAL
// net.minecraft.util.StringUtil methods (all the ported ones are public static — no
// reflection needed).
//
// String fields are encoded as the concatenation of %04x per UTF-16 code unit (so any
// code unit, including control / supplementary-surrogate units, round-trips exactly).
// Sentinels: "NULL" => a null reference, "EMPTY" => the empty string "".
//
// Row formats (TAG \t inputs... \t outputs...):
//   WSCP        cp                              (cp in 0..0xFFFF where isWhitespace(cp)==true)
//   ISWS        cp           bool               (isWhitespace, representative boundary cps)
//   ISBLANK     strHex       bool               (isBlank)
//   ISNULLOREMPTY strHex     bool               (isNullOrEmpty)
//   TRUNC       strHex maxLen addDots(0/1) outHex (truncateStringIfNecessary)
//   LINECOUNT   strHex       int                (lineCount)
//   TRIMCHAT    strHex       outHex             (trimChatMessage)
//   ALLOWEDCHAR ch           bool               (isAllowedChatCharacter)
public class StringUtilParity {
    static final java.io.PrintStream O = System.out;

    static String enc(String s) {
        if (s == null) return "NULL";
        if (s.isEmpty()) return "EMPTY";
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < s.length(); i++) {
            b.append(String.format("%04x", (int) s.charAt(i)));
        }
        return b.toString();
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // ---- whitespace table dump: every BMP code unit classified as whitespace -------
        for (int cp = 0; cp <= 0xFFFF; cp++) {
            if (StringUtil.isWhitespace(cp)) {
                O.println("WSCP\t" + cp);
            }
        }

        // ---- isWhitespace spot checks across the whole BMP + boundaries ---------------
        int[] wsProbe = {
            -1, 0, 8, 9, 10, 11, 12, 13, 14, 27, 28, 29, 30, 31, 32, 33,
            0x7F, 0x84, 0x85, 0x86, 0xA0, 0xA1, 0x1680, 0x1FFF, 0x2000, 0x200A,
            0x200B, 0x2027, 0x2028, 0x2029, 0x202A, 0x202F, 0x205F, 0x2060, 0x3000,
            0x3001, 0xFEFF, 0xFFFF, 0x41, 0x61, 0x30, 0x4E00
        };
        for (int cp : wsProbe) {
            O.println("ISWS\t" + cp + "\t" + (StringUtil.isWhitespace(cp) ? 1 : 0));
        }

        // ---- string fixtures (used by isBlank / isNullOrEmpty / truncate / lineCount) --
        String[] fixtures = {
            null,
            "",
            " ",
            "  ",
            "\t",
            "\t \n\r",
            " ",                 // NO-BREAK SPACE (whitespace via isSpaceChar)
            "  ",           // EM SPACE + THIN SPACE
            "　",                 // IDEOGRAPHIC SPACE
            "  \t  ",       // all-whitespace incl line separator
            "a",
            " a ",
            "abc",
            "abc ",
            " abc",
            "hello world",
            "line1\nline2",
            "line1\r\nline2",
            "line1\rline2",
            "a\r\n\r\nb",
            "\n",
            "\r",
            "\r\n",
            "\n\n\n",
            "",           // VT + FF (vertical whitespace for \v)
            "abc",
            "xy",               // NEL: \v matches it (lineCount) but \x85 is NOT
                                      //      isWhitespace (it's not Zs/Zl/Zp nor a control)
            "trailing\n",
            "  ",           // LS + PS
            "mix\r\n end",
            "The quick brown fox jumps over the lazy dog.",
            "0123456789",
            "edgeexactly16chr",        // length 16
            "😀",            // surrogate pair (emoji) -> 2 code units
            "tab\tinside",
            "​",                  // ZERO WIDTH SPACE: NOT whitespace
            "  ​  "               // not blank (has ZWSP in the middle)
        };

        // build a long string for truncation boundary tests
        StringBuilder lb = new StringBuilder();
        for (int i = 0; i < 300; i++) lb.append((char) ('a' + (i % 26)));
        String longStr = lb.toString();

        // ---- isBlank / isNullOrEmpty ---------------------------------------------------
        for (String s : fixtures) {
            O.println("ISBLANK\t" + enc(s) + "\t" + (StringUtil.isBlank(s) ? 1 : 0));
            O.println("ISNULLOREMPTY\t" + enc(s) + "\t" + (StringUtil.isNullOrEmpty(s) ? 1 : 0));
        }
        O.println("ISBLANK\t" + enc(longStr) + "\t" + (StringUtil.isBlank(longStr) ? 1 : 0));
        O.println("ISNULLOREMPTY\t" + enc(longStr) + "\t" + (StringUtil.isNullOrEmpty(longStr) ? 1 : 0));

        // ---- lineCount -----------------------------------------------------------------
        for (String s : fixtures) {
            if (s == null) continue;  // lineCount dereferences s; null is not a valid input
            O.println("LINECOUNT\t" + enc(s) + "\t" + StringUtil.lineCount(s));
        }
        O.println("LINECOUNT\t" + enc(longStr) + "\t" + StringUtil.lineCount(longStr));

        // ---- truncateStringIfNecessary -------------------------------------------------
        String[] truncStrs = {
            "", "a", "ab", "abc", "abcd", "abcde", "hello world", "exactly10!",
            longStr, "😀😀",  // surrogate pairs: substring cuts on code units
            "0123456789"
        };
        int[] maxLens = { 0, 1, 2, 3, 4, 5, 6, 10, 16, 255, 256, 300, 1000 };
        for (String s : truncStrs) {
            if (s == null) continue;
            for (int m : maxLens) {
                for (int d = 0; d <= 1; d++) {
                    boolean addDots = d == 1;
                    String out = StringUtil.truncateStringIfNecessary(s, m, addDots);
                    O.println("TRUNC\t" + enc(s) + "\t" + m + "\t" + d + "\t" + enc(out));
                }
            }
        }

        // ---- trimChatMessage (truncate w/ 256, no dots) --------------------------------
        for (String s : truncStrs) {
            if (s == null) continue;
            O.println("TRIMCHAT\t" + enc(s) + "\t" + enc(StringUtil.trimChatMessage(s)));
        }
        // a >256 string specifically for trimChatMessage
        StringBuilder cb = new StringBuilder();
        for (int i = 0; i < 500; i++) cb.append((char) ('A' + (i % 26)));
        String chat = cb.toString();
        O.println("TRIMCHAT\t" + enc(chat) + "\t" + enc(StringUtil.trimChatMessage(chat)));

        // ---- isAllowedChatCharacter ----------------------------------------------------
        int[] chProbe = {
            -1, 0, 31, 32, 33, 64, 126, 127, 128, 165, 166, 167, 168, 200,
            0x7F, 0xA7, 0x100, 0x2028, 0xFFFF, 0x10000, 65, 97, 48
        };
        for (int ch : chProbe) {
            O.println("ALLOWEDCHAR\t" + ch + "\t" + (StringUtil.isAllowedChatCharacter(ch) ? 1 : 0));
        }
    }
}
