// Ground-truth generator for net.minecraft.world.level.block.RedStoneWireBlock's
// static power->color table: the public accessor getColorForPower(int) backed by the
// private COLORS[16] table built in the class initializer.
//
// PURE math (no world/registry/GL in the table build): per power level i in 0..15,
//   float power = i / 15.0F;
//   float red   = power * 0.6F + (power > 0.0F ? 0.4F : 0.3F);
//   float green = Mth.clamp(power*power*0.7F - 0.5F, 0.0F, 1.0F);
//   float blue  = Mth.clamp(power*power*0.6F - 0.7F, 0.0F, 1.0F);
//   COLORS[i]   = ARGB.colorFromFloat(1.0F, red, green, blue);
//
// We drive the REAL class two ways and cross-check:
//   (a) getColorForPower(i)          — the public accessor on the real class
//   (b) the private COLORS[] field   — dumped via reflection
// Both must agree with the C++ port. Colors emitted as decimal ints (ARGB packed).
//
//   tools/run_groundtruth.ps1 -Tool RedstoneWireColorParity -Out mcpp/build/redstone_wire_color.tsv

import java.lang.reflect.Field;
import net.minecraft.world.level.block.RedStoneWireBlock;

public class RedstoneWireColorParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // (a) public accessor — exercises the full valid index domain 0..15.
        for (int p = 0; p <= 15; p++) {
            O.println("GETCOLOR\t" + p + "\t" + RedStoneWireBlock.getColorForPower(p));
        }

        // (b) raw private table via reflection (independent confirmation of the build).
        Field colorsF = RedStoneWireBlock.class.getDeclaredField("COLORS");
        colorsF.setAccessible(true);
        int[] colors = (int[]) colorsF.get(null);
        O.println("TABLELEN\t" + colors.length);
        for (int i = 0; i < colors.length; i++) {
            O.println("TABLE\t" + i + "\t" + colors[i]);
        }
    }
}
