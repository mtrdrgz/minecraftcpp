// Ground truth for net.minecraft.network.chat.TextColor. Drives the REAL parseColor / fromRgb /
// fromLegacyFormat / serialize. parseColor returns a DataResult; we emit error vs (value, serialize).
//
//   tools/run_groundtruth.ps1 -Tool TextColorParity -Out mcpp/build/text_color.tsv
//
// Rows:
//   PARSE  <inputB64> <isError> <value> <serializeB64>
//   RGB    <value>    <maskedValue> <serializeB64>
//   LEGACY <ordinal>  <isNull> <value> <serializeB64>

import com.mojang.serialization.DataResult;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import net.minecraft.ChatFormatting;
import net.minecraft.network.chat.TextColor;
import net.minecraft.server.Bootstrap;
import net.minecraft.SharedConstants;

public class TextColorParity {
    static final java.io.PrintStream O = System.out;
    static String b64(String s) { return Base64.getEncoder().encodeToString(s.getBytes(StandardCharsets.UTF_8)); }

    static final String[] PARSE_INPUTS = {
        "#000000", "#FFFFFF", "#ffffff", "#FF0000", "#1a2b3c", "#abc", "#0", "#000abc",
        "#1000000", "#-1", "#", "#GG", "#  ", "#7fffffff", "#80000000", "#FFFFFFFF", "#+10",
        "red", "white", "black", "dark_blue", "gold", "light_purple", "blue", "yellow",
        "RED", "Red", "reset", "obfuscated", "bold", "notacolor", "", "dark_aqua", " red"
    };
    static final int[] RGB_VALUES = {
        0, 255, 16777215, 16777216, -1, 0x123456, 0xFF123456, 0x01000000, 11141120
    };

    public static void main(String[] args) {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        for (String in : PARSE_INPUTS) {
            DataResult<TextColor> r = TextColor.parseColor(in);
            var opt = r.result();
            if (opt.isEmpty()) {
                O.println("PARSE\t" + b64(in) + "\t1\t0\t" + b64(""));
            } else {
                TextColor tc = opt.get();
                O.println("PARSE\t" + b64(in) + "\t0\t" + tc.getValue() + "\t" + b64(tc.serialize()));
            }
        }

        for (int v : RGB_VALUES) {
            TextColor tc = TextColor.fromRgb(v);
            O.println("RGB\t" + v + "\t" + tc.getValue() + "\t" + b64(tc.serialize()));
        }

        for (ChatFormatting f : ChatFormatting.values()) {
            TextColor tc = TextColor.fromLegacyFormat(f);
            if (tc == null) O.println("LEGACY\t" + f.ordinal() + "\t1\t0\t" + b64(""));
            else O.println("LEGACY\t" + f.ordinal() + "\t0\t" + tc.getValue() + "\t" + b64(tc.serialize()));
        }
    }
}
