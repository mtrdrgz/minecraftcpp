// Ground truth for the PURE surface of net.minecraft.network.chat.Style. Drives the REAL Style
// through a data-driven op stream (identical to the C++ side) and emits a STATE row after every op
// capturing every observable. The C++ style_parity test replays the SAME ops on its port and
// compares.
//
//   tools/run_groundtruth.ps1 -Tool StyleParity -Out mcpp/build/style.tsv
//
// Op stream is a fixed list of OP rows; we print:
//   OP    <op-encoding...>                                   (the instruction to replay)
//   STATE <bold> <italic> <underlined> <strikethrough> <obfuscated> <hasColor> <colorValue> <serB64>
// where the five booleans are is*() (0/1), hasColor is 0/1, colorValue is getColor().getValue()
// (or 0 when absent), serB64 is base64(getColor().serialize()) (or base64("") when absent).
//
// PARENTS: a fixed set of parent Styles built once, referenced by index in APPLYTO ops, to exercise
// the tri-state applyTo merge (null vs TRUE vs FALSE child over null/TRUE/FALSE parent).

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Base64;
import java.util.List;
import net.minecraft.ChatFormatting;
import net.minecraft.network.chat.Style;
import net.minecraft.network.chat.TextColor;
import net.minecraft.server.Bootstrap;
import net.minecraft.SharedConstants;

public class StyleParity {
    static final java.io.PrintStream O = System.out;
    static String b64(String s) { return Base64.getEncoder().encodeToString(s.getBytes(StandardCharsets.UTF_8)); }

    // A program is a sequence of ops applied to a "current" Style starting at EMPTY.
    // Each op is an Object[] whose first element is the op name; remaining are args.
    static List<Object[]> program() {
        List<Object[]> ops = new ArrayList<>();
        // --- with<Flag> tri-state coverage ---
        ops.add(new Object[]{"WBOLD", "T"});
        ops.add(new Object[]{"WITAL", "T"});
        ops.add(new Object[]{"WUNDER", "F"});       // FALSE is distinct from null
        ops.add(new Object[]{"WSTRIKE", "N"});      // setting null
        ops.add(new Object[]{"WOBF", "T"});
        ops.add(new Object[]{"WBOLD", "F"});        // flip TRUE->FALSE
        ops.add(new Object[]{"WBOLD", "N"});        // FALSE->null
        // --- color ---
        ops.add(new Object[]{"WCOL_TC", 0x123456}); // fromRgb
        ops.add(new Object[]{"WCOL_TC", 0xFF0000});
        ops.add(new Object[]{"WCOL_CF", ChatFormatting.RED.ordinal()});       // named color
        ops.add(new Object[]{"WCOL_CF", ChatFormatting.GOLD.ordinal()});
        ops.add(new Object[]{"WCOL_CF", ChatFormatting.BOLD.ordinal()});      // non-color CF -> null
        ops.add(new Object[]{"WCOL_CFNULL"});
        ops.add(new Object[]{"WCOL_TC", 0x00FF00});
        ops.add(new Object[]{"WCOL_TCNULL"});
        // rebuild a non-trivial style for the applyFormat tests
        ops.add(new Object[]{"WBOLD", "T"});
        ops.add(new Object[]{"WITAL", "F"});
        ops.add(new Object[]{"WCOL_TC", 0xABCDEF});
        // --- applyFormat: each format code, a color, RESET ---
        ops.add(new Object[]{"AFMT", ChatFormatting.OBFUSCATED.ordinal()});
        ops.add(new Object[]{"AFMT", ChatFormatting.BOLD.ordinal()});
        ops.add(new Object[]{"AFMT", ChatFormatting.STRIKETHROUGH.ordinal()});
        ops.add(new Object[]{"AFMT", ChatFormatting.UNDERLINE.ordinal()});
        ops.add(new Object[]{"AFMT", ChatFormatting.ITALIC.ordinal()});      // turns the FALSE italic into TRUE
        ops.add(new Object[]{"AFMT", ChatFormatting.BLUE.ordinal()});        // color branch: replaces color only
        ops.add(new Object[]{"AFMT", ChatFormatting.RESET.ordinal()});       // -> EMPTY
        // --- applyLegacyFormat: color branch must clear all 5 booleans to FALSE ---
        ops.add(new Object[]{"WBOLD", "T"});
        ops.add(new Object[]{"WITAL", "T"});
        ops.add(new Object[]{"WOBF", "T"});
        ops.add(new Object[]{"ALEG", ChatFormatting.DARK_GREEN.ordinal()});  // color: booleans -> FALSE, color set
        ops.add(new Object[]{"ALEG", ChatFormatting.BOLD.ordinal()});        // format code: bold -> TRUE
        ops.add(new Object[]{"ALEG", ChatFormatting.YELLOW.ordinal()});      // color again: bold back to FALSE
        ops.add(new Object[]{"ALEG", ChatFormatting.RESET.ordinal()});       // -> EMPTY
        // --- applyFormats varargs: order + RESET short-circuit ---
        ops.add(new Object[]{"AFMTS", new int[]{
            ChatFormatting.BOLD.ordinal(), ChatFormatting.RED.ordinal(), ChatFormatting.ITALIC.ordinal()}});
        ops.add(new Object[]{"AFMTS", new int[]{
            ChatFormatting.GREEN.ordinal(), ChatFormatting.BLUE.ordinal()}});  // last color wins
        ops.add(new Object[]{"AFMTS", new int[]{
            ChatFormatting.BOLD.ordinal(), ChatFormatting.RESET.ordinal(), ChatFormatting.ITALIC.ordinal()}}); // RESET short-circuits -> EMPTY
        ops.add(new Object[]{"AFMTS", new int[]{}});  // empty varargs: no-op (copy)
        // --- applyTo merges against parents ---
        ops.add(new Object[]{"WBOLD", "T"});
        ops.add(new Object[]{"WITAL", "F"});
        ops.add(new Object[]{"WCOL_TC", 0x112233});
        ops.add(new Object[]{"APPLYTO", 0});  // parent 0
        ops.add(new Object[]{"APPLYTO", 1});
        ops.add(new Object[]{"APPLYTO", 2});
        ops.add(new Object[]{"APPLYTO", 3});  // EMPTY parent
        ops.add(new Object[]{"RESET_STYLE"});
        ops.add(new Object[]{"APPLYTO", 1});  // EMPTY.applyTo(parent) -> parent
        ops.add(new Object[]{"WUNDER", "F"});
        ops.add(new Object[]{"APPLYTO", 0});  // child FALSE underline over parent's TRUE underline
        ops.add(new Object[]{"APPLYTO", 2});
        return ops;
    }

    // Parents referenced by APPLYTO index.
    static Style[] parents() {
        Style[] p = new Style[4];
        // parent 0: every field set, mix of TRUE/FALSE, with a color
        p[0] = Style.EMPTY
            .withBold(Boolean.TRUE)
            .withItalic(Boolean.TRUE)
            .withUnderlined(Boolean.TRUE)
            .withStrikethrough(Boolean.FALSE)
            .withObfuscated(Boolean.FALSE)
            .withColor(TextColor.fromRgb(0x654321));
        // parent 1: only some fields (others null), different color via ChatFormatting
        p[1] = Style.EMPTY
            .withBold(Boolean.FALSE)
            .withObfuscated(Boolean.TRUE)
            .withColor(ChatFormatting.AQUA);
        // parent 2: all FALSE, no color
        p[2] = Style.EMPTY
            .withBold(Boolean.FALSE)
            .withItalic(Boolean.FALSE)
            .withUnderlined(Boolean.FALSE)
            .withStrikethrough(Boolean.FALSE)
            .withObfuscated(Boolean.FALSE);
        // parent 3: EMPTY
        p[3] = Style.EMPTY;
        return p;
    }

    static Boolean tri(String s) {
        if (s.equals("T")) return Boolean.TRUE;
        if (s.equals("F")) return Boolean.FALSE;
        return null;  // "N"
    }

    static void emitState(Style s) {
        TextColor c = s.getColor();
        int hasColor = c != null ? 1 : 0;
        int val = c != null ? c.getValue() : 0;
        String ser = c != null ? c.serialize() : "";
        O.println("STATE\t"
            + (s.isBold() ? 1 : 0) + "\t"
            + (s.isItalic() ? 1 : 0) + "\t"
            + (s.isUnderlined() ? 1 : 0) + "\t"
            + (s.isStrikethrough() ? 1 : 0) + "\t"
            + (s.isObfuscated() ? 1 : 0) + "\t"
            + hasColor + "\t"
            + val + "\t"
            + b64(ser));
    }

    static String joinInts(int[] a) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < a.length; i++) { if (i > 0) sb.append(','); sb.append(a[i]); }
        return sb.toString();
    }

    public static void main(String[] args) {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        Style[] par = parents();
        Style cur = Style.EMPTY;
        emitState(cur);  // initial EMPTY state row

        for (Object[] op : program()) {
            String name = (String) op[0];
            switch (name) {
                case "WBOLD":   cur = cur.withBold(tri((String) op[1])); O.println("OP\tWBOLD\t" + op[1]); break;
                case "WITAL":   cur = cur.withItalic(tri((String) op[1])); O.println("OP\tWITAL\t" + op[1]); break;
                case "WUNDER":  cur = cur.withUnderlined(tri((String) op[1])); O.println("OP\tWUNDER\t" + op[1]); break;
                case "WSTRIKE": cur = cur.withStrikethrough(tri((String) op[1])); O.println("OP\tWSTRIKE\t" + op[1]); break;
                case "WOBF":    cur = cur.withObfuscated(tri((String) op[1])); O.println("OP\tWOBF\t" + op[1]); break;
                case "WCOL_TC": {
                    int v = (Integer) op[1];
                    cur = cur.withColor(TextColor.fromRgb(v));
                    O.println("OP\tWCOL_TC\t" + v);
                    break;
                }
                case "WCOL_TCNULL": cur = cur.withColor((TextColor) null); O.println("OP\tWCOL_TCNULL"); break;
                case "WCOL_CF": {
                    int ord = (Integer) op[1];
                    cur = cur.withColor(ChatFormatting.values()[ord]);
                    O.println("OP\tWCOL_CF\t" + ord);
                    break;
                }
                case "WCOL_CFNULL": cur = cur.withColor((ChatFormatting) null); O.println("OP\tWCOL_CFNULL"); break;
                case "AFMT": {
                    int ord = (Integer) op[1];
                    cur = cur.applyFormat(ChatFormatting.values()[ord]);
                    O.println("OP\tAFMT\t" + ord);
                    break;
                }
                case "ALEG": {
                    int ord = (Integer) op[1];
                    cur = cur.applyLegacyFormat(ChatFormatting.values()[ord]);
                    O.println("OP\tALEG\t" + ord);
                    break;
                }
                case "AFMTS": {
                    int[] ords = (int[]) op[1];
                    ChatFormatting[] fs = new ChatFormatting[ords.length];
                    for (int i = 0; i < ords.length; i++) fs[i] = ChatFormatting.values()[ords[i]];
                    cur = cur.applyFormats(fs);
                    O.println("OP\tAFMTS\t" + joinInts(ords));
                    break;
                }
                case "APPLYTO": {
                    int idx = (Integer) op[1];
                    cur = cur.applyTo(par[idx]);
                    O.println("OP\tAPPLYTO\t" + idx);
                    break;
                }
                case "RESET_STYLE": cur = Style.EMPTY; O.println("OP\tRESET_STYLE"); break;
                default: throw new IllegalStateException("unknown op " + name);
            }
            emitState(cur);
        }
    }
}
