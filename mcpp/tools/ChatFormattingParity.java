// Ground truth for net.minecraft.ChatFormatting (Minecraft 26.1.2).
//
// ChatFormatting is a 22-value pure-data enum (16 colors, 5 format codes, RESET).
// We drive the REAL net.minecraft.ChatFormatting enum: all queried accessors
// (getChar / getId / isFormat / isColor / getColor / getName / getSerializedName)
// and the static lookups (getByCode / getById / getByName) are PUBLIC, so we call
// them directly. The private `name` ctor field is read via reflection only to
// assert it equals name() (a sanity check, not an emitted column).
//
// Row TAGs (tab-separated; ints/bools decimal, strings raw):
//   CF <ordinal> <name> <code> <isFormat> <isColor> <id> <hasColor> <color> <serialized>
//        one row per ChatFormatting constant.
//        <name>       = ChatFormatting.name()
//        <code>       = getChar() as a decimal char code
//        <isFormat>   = isFormat() (0/1)
//        <isColor>    = isColor()  (0/1)
//        <id>         = getId()
//        <hasColor>   = getColor()!=null (0/1)
//        <color>      = getColor() (0 when null; gated by <hasColor>)
//        <serialized> = getSerializedName() (== getName())
//   BYCODE <code> <resultOrdinal>
//        getByCode((char)code).ordinal(); -1 when null.
//   BYID <id> <resultOrdinal>
//        getById(id).ordinal(); -1 when null.
//   BYNAME <name> <resultOrdinal>
//        getByName(name).ordinal(); -1 when null. <name> is the raw input string.
//
// The C++ test rebuilds the identical table/lookups and compares BIT-FOR-BIT.

import java.lang.reflect.Field;
import net.minecraft.ChatFormatting;

public class ChatFormattingParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Private instance field `name` (first ctor arg) — sanity-checked vs name().
        Field nameF = ChatFormatting.class.getDeclaredField("name");
        nameF.setAccessible(true);

        // ----- one CF row per enum constant -----
        for (ChatFormatting f : ChatFormatting.values()) {
            String privName = (String) nameF.get(f);
            if (!privName.equals(f.name())) {
                throw new IllegalStateException("name field != name(): " + privName + " vs " + f.name());
            }
            char code = f.getChar();
            boolean isFormat = f.isFormat();
            boolean isColor = f.isColor();
            int id = f.getId();
            Integer color = f.getColor();      // @Nullable Integer
            boolean hasColor = color != null;
            int colorVal = hasColor ? color.intValue() : 0;
            String serialized = f.getSerializedName();
            // sanity: getSerializedName() == getName()
            if (!serialized.equals(f.getName())) {
                throw new IllegalStateException("serialized != getName() for " + f.name());
            }
            O.println("CF\t" + f.ordinal() + "\t" + f.name() + "\t" + ((int) code)
                      + "\t" + (isFormat ? 1 : 0) + "\t" + (isColor ? 1 : 0)
                      + "\t" + id + "\t" + (hasColor ? 1 : 0) + "\t" + colorVal
                      + "\t" + serialized);
        }

        // ----- getByCode over a battery of chars -----
        // Every code char (lower + upper), plus assorted misses (digits/letters/punct).
        java.util.LinkedHashSet<Character> codes = new java.util.LinkedHashSet<>();
        for (char c = '0'; c <= '9'; c++) codes.add(c);
        for (char c = 'a'; c <= 'z'; c++) codes.add(c);
        for (char c = 'A'; c <= 'Z'; c++) codes.add(c);
        char[] extra = { ' ', '!', '#', '§', '_', '-', '*', '.' };
        for (char c : extra) codes.add(c);
        for (char c : codes) {
            ChatFormatting r = ChatFormatting.getByCode(c);
            O.println("BYCODE\t" + ((int) c) + "\t" + (r == null ? -1 : r.ordinal()));
        }

        // ----- getById over a battery of int keys -----
        java.util.LinkedHashSet<Integer> ids = new java.util.LinkedHashSet<>();
        for (int i = -3; i <= 20; i++) ids.add(i);   // straddles [0,15] window, negatives -> RESET
        ids.add(Integer.MIN_VALUE);
        ids.add(Integer.MIN_VALUE + 1);
        ids.add(Integer.MAX_VALUE);
        ids.add(Integer.MAX_VALUE - 1);
        ids.add(-100000);
        ids.add(100000);
        ids.add(16);
        ids.add(100);
        for (int id : ids) {
            ChatFormatting r = ChatFormatting.getById(id);
            O.println("BYID\t" + id + "\t" + (r == null ? -1 : r.ordinal()));
        }

        // ----- getByName over a battery of strings -----
        // cleanName = toLowerCase(ROOT).replaceAll("[^a-z]",""); keys are cleanName(name).
        // Exercise: exact names, lower/upper, with/without underscore, with stray
        // punctuation/digits (cleaned away), partial/misses, empty.
        String[] names = {
            "BLACK", "black", "Black",
            "DARK_BLUE", "dark_blue", "darkblue", "DARKBLUE", "Dark Blue", "dark-blue", "d4ark_blue",
            "DARK_GREEN", "dark green", "DARK_AQUA", "dark_red", "DARK_PURPLE",
            "GOLD", "gold", "GRAY", "gray", "DARK_GRAY", "darkgray",
            "BLUE", "GREEN", "AQUA", "RED", "LIGHT_PURPLE", "lightpurple", "light_purple",
            "YELLOW", "WHITE", "white",
            "OBFUSCATED", "obfuscated", "BOLD", "bold", "STRIKETHROUGH", "strikethrough",
            "UNDERLINE", "underline", "ITALIC", "italic", "RESET", "reset",
            // cleanName collapses these to existing keys:
            "  reset  ", "r-e-s-e-t", "RESET123", "123reset456",
            // misses:
            "", "   ", "12345", "----", "nope", "purple", "light", "dark",
            "magenta", "orange", "darkbluee", " darkblue "
        };
        for (String nm : names) {
            ChatFormatting r = ChatFormatting.getByName(nm);
            O.println("BYNAME\t" + nm + "\t" + (r == null ? -1 : r.ordinal()));
        }

        O.flush();
    }
}
