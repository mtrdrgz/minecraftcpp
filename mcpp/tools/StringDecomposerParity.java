import net.minecraft.ChatFormatting;
import net.minecraft.network.chat.Style;
import net.minecraft.network.chat.TextColor;
import net.minecraft.util.FormattedCharSink;
import net.minecraft.util.StringDecomposer;

// Ground truth for mcpp/src/util/StringDecomposer.h. Drives the REAL
// net.minecraft.util.StringDecomposer over fixed strings (containing section-sign '§'
// legacy format codes) and dumps the (position, style, codePoint) sink stream emitted by
// the public static iterate / iterateBackwards / iterateFormatted methods, plus the rebuilt
// string from filterBrokenSurrogates.
//
// All driven methods are public static and take a public FormattedCharSink functional
// interface — no reflection needed. The only observable Style state these methods can
// produce is the legacy color + the five legacy flags (the running style is built solely by
// Style.applyLegacyFormat from a start style), so the sink dumps exactly those.
//
// Encodings (to keep every code unit / control char exact):
//   strings   -> concatenation of %04x per UTF-16 code unit; "EMPTY" => "".
//   styleSpec -> the legacy format code chars applied (in order) to Style.EMPTY to build the
//                start style; "EMPTY" => Style.EMPTY itself. (e.g. "c" red, "cl" red+bold.)
//   emit tuple-> pos:colorFlag:colorVal:bold:italic:underlined:strikethrough:obfuscated:cp
//                colorFlag is 1 if style.getColor()!=null else 0; colorVal is its getValue()
//                (decimal) or -1 when null. flags are 0/1. All other ints decimal.
//   stream    -> the emit tuples joined by ';' (empty string when nothing was emitted).
//
// Row formats (TAG \t inputs... \t outputs...):
//   IF   caseId inputHex offset curStyleSpec resetStyleSpec ret count stream   (iterateFormatted 5-arg)
//   IT   caseId inputHex styleSpec ret count stream                            (iterate)
//   IB   caseId inputHex styleSpec ret count stream                            (iterateBackwards)
//   FBS  caseId inputHex outHex                                                (filterBrokenSurrogates)
public class StringDecomposerParity {
    static final java.io.PrintStream O = System.out;

    static String enc(String s) {
        if (s == null) return "EMPTY";
        if (s.isEmpty()) return "EMPTY";
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < s.length(); i++) b.append(String.format("%04x", (int) s.charAt(i)));
        return b.toString();
    }

    // Build a Style by applying each legacy code char (in order) to Style.EMPTY, exactly the
    // way the C++ side builds its DecomposerStyle. "EMPTY" => Style.EMPTY.
    static Style buildStyle(String spec) {
        Style s = Style.EMPTY;
        if (spec.equals("EMPTY")) return s;
        for (int i = 0; i < spec.length(); i++) {
            ChatFormatting f = ChatFormatting.getByCode(spec.charAt(i));
            if (f == ChatFormatting.RESET) {
                s = Style.EMPTY;
            } else if (f != null) {
                s = s.applyLegacyFormat(f);
            }
        }
        return s;
    }

    // A sink that records every (pos, style, codePoint) emission into a tuple stream.
    static final class Recorder implements FormattedCharSink {
        final StringBuilder sb = new StringBuilder();
        int count = 0;

        @Override
        public boolean accept(int position, Style style, int codepoint) {
            if (count > 0) sb.append(';');
            TextColor c = style.getColor();
            int colorFlag = (c != null) ? 1 : 0;
            int colorVal = (c != null) ? c.getValue() : -1;
            sb.append(position).append(':')
              .append(colorFlag).append(':')
              .append(colorVal).append(':')
              .append(style.isBold() ? 1 : 0).append(':')
              .append(style.isItalic() ? 1 : 0).append(':')
              .append(style.isUnderlined() ? 1 : 0).append(':')
              .append(style.isStrikethrough() ? 1 : 0).append(':')
              .append(style.isObfuscated() ? 1 : 0).append(':')
              .append(codepoint);
            count++;
            return true;  // never abort: certify the full stream
        }

        String stream() { return sb.length() == 0 ? "EMPTY" : sb.toString(); }
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // -------- fixed-string fixtures (finite/physical inputs) ------------------------
        // Mixed plain, multi-code colors/formats, RESET, broken/trailing § codes, surrogate
        // pairs (emoji), lone high/low surrogates, control chars, unknown § codes.
        String[] fixtures = {
            "",                                   // empty
            "hello",                              // plain
            "§cred",                         // §c then "red"
            "§credtext",                     // color persists
            "§lbold",                        // §l bold flag
            "§l§obi",                   // bold + italic stack
            "§cred§rreset",             // §r resets
            "§cred§lboth",              // color then add bold (kept)
            "§lbold§cthencolor",        // bold then color (color clears flags!)
            "a§",                            // dangling § at end (break)
            "§",                             // lone § (break immediately)
            "§z?",                           // unknown code 'z' (getByCode null) -> kept literal? no: i++ skips
            "§Cupper",                       // uppercase code 'C' -> lowercased to 'c'
            "§k§l§m§n§oALL", // all five flags
            "x§cy§dz",                  // two colors
            "tab\there",                          // embedded control char (tab)
            "bell",                         // control char 0x07
            "😀",                       // emoji surrogate pair (U+1F600)
            "a😀b",                     // pair in the middle
            "\ud83d",                             // lone high surrogate (trailing -> 65533)
            "\udc00",                             // lone low surrogate -> 65533 (feedChar)
            "\ud83dx",                            // high surrogate followed by non-low -> 65533
            "x\udc00y",                           // low surrogate in middle -> 65533
            "§c😀§lZ",        // color, emoji pair, then bold
            "§r§r",                     // double reset
            "§0§1§2§3",       // several colors in a row
            "§caa§bb§cc",          // alternating colors
            "no codes here at all",               // plain sentence
            "trail§l",                       // dangling §l at very end (i+1<size true; applies l then loop ends)
            "§§cdouble",                // §§ then color: first § sees next '§'(167) not a code -> getByCode(167)=null
        };

        int caseId = 0;

        // -------- iterateFormatted (5-arg full variant) --------------------------------
        // offsets and (currentStyle, resetStyle) start states to exercise both args.
        int[] offsets = {0, 1, 2};
        String[][] stylePairs = {
            {"EMPTY", "EMPTY"},
            {"c", "EMPTY"},      // current red, reset to empty
            {"l", "c"},          // current bold, reset to red
            {"cl", "EMPTY"},     // current red+bold
            {"k", "l"},          // current obfuscated, reset to bold
        };
        for (String s : fixtures) {
            for (int off : offsets) {
                if (off > s.length()) continue;  // offset must be a valid index range start
                for (String[] sp : stylePairs) {
                    Style cur = buildStyle(sp[0]);
                    Style rst = buildStyle(sp[1]);
                    Recorder rec = new Recorder();
                    boolean ret = StringDecomposer.iterateFormatted(s, off, cur, rst, rec);
                    O.println("IF\t" + (caseId++) + "\t" + enc(s) + "\t" + off + "\t" + sp[0]
                              + "\t" + sp[1] + "\t" + (ret ? 1 : 0) + "\t" + rec.count + "\t"
                              + rec.stream());
                }
            }
        }

        // -------- iterate (forward, no format codes) -----------------------------------
        // Style codes here are inert (iterate never parses §) but verify the passed style
        // flows unchanged to every emit.
        String[] iterStyles = {"EMPTY", "c", "cl", "klmno"};
        for (String s : fixtures) {
            for (String ss : iterStyles) {
                Style st = buildStyle(ss);
                Recorder rec = new Recorder();
                boolean ret = StringDecomposer.iterate(s, st, rec);
                O.println("IT\t" + (caseId++) + "\t" + enc(s) + "\t" + ss + "\t" + (ret ? 1 : 0)
                          + "\t" + rec.count + "\t" + rec.stream());
            }
        }

        // -------- iterateBackwards ------------------------------------------------------
        for (String s : fixtures) {
            for (String ss : iterStyles) {
                Style st = buildStyle(ss);
                Recorder rec = new Recorder();
                boolean ret = StringDecomposer.iterateBackwards(s, st, rec);
                O.println("IB\t" + (caseId++) + "\t" + enc(s) + "\t" + ss + "\t" + (ret ? 1 : 0)
                          + "\t" + rec.count + "\t" + rec.stream());
            }
        }

        // -------- filterBrokenSurrogates ------------------------------------------------
        for (String s : fixtures) {
            String out = StringDecomposer.filterBrokenSurrogates(s);
            O.println("FBS\t" + (caseId++) + "\t" + enc(s) + "\t" + enc(out));
        }
    }
}
