// Ground truth for net.minecraft.world.level.CardinalLighting (per-face directional shading).
// Drives the REAL DEFAULT/NETHER records' byFace(direction) + up(). Pure float constants.
//
//   tools/run_groundtruth.ps1 -Tool CardinalLightingParity -Out mcpp/build/cardinal_lighting.tsv
//
// Row: CL \t <preset DEFAULT|NETHER> \t <dirOrdinal> \t <byFaceBits %08x> \t <upBits %08x>

import net.minecraft.core.Direction;
import net.minecraft.world.level.CardinalLighting;

public class CardinalLightingParity {
    static final java.io.PrintStream O = System.out;
    static String f(float x) { return String.format("%08x", Float.floatToRawIntBits(x)); }

    public static void main(String[] args) {
        CardinalLighting[] presets = {CardinalLighting.DEFAULT, CardinalLighting.NETHER};
        String[] names = {"DEFAULT", "NETHER"};
        for (int i = 0; i < presets.length; i++) {
            CardinalLighting c = presets[i];
            for (Direction d : Direction.values()) {
                O.println("CL\t" + names[i] + "\t" + d.ordinal() + "\t" + f(c.byFace(d)) + "\t" + f(c.up()));
            }
        }
    }
}
