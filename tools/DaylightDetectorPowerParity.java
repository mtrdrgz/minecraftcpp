// Ground truth for the pure analog-power computation of
// net.minecraft.world.level.block.DaylightDetectorBlock (Minecraft Java Edition
// 26.1.2).
//
//   DaylightDetectorBlock.updateSignalStrength(...)   DaylightDetectorBlock.java:59-75
//
// updateSignalStrength is `private static` and reads/writes the world
// (level.getEffectiveSkyBrightness, level.environmentAttributes().getValue(
// SUN_ANGLE), state.getValue(INVERTED), level.setBlock). Its world reads only
// surface three scalars — the effective sky brightness (int), the SUN_ANGLE
// environment attribute (float, degrees) and the INVERTED property (bool) — and
// the rest is a pure scalar pipeline. We exercise that pipeline by feeding those
// three scalars directly and composing them EXACTLY as the method does, with the
// two NON-trivial operations taken from the REAL classes via reflection:
//
//   * the cosine  -> net.minecraft.util.Mth.cos(double)  (the table-based LUT,
//     NOT java.lang.Math.cos), resolved reflectively and invoked.
//   * the rounding -> java.lang.Math.round(float)         (the JDK bit-twiddle),
//     invoked directly. (Math.round and Mth.clamp are the canonical JDK/real
//     routines; we call them, we do not re-implement them.)
//
// The remaining glue (deg->rad factor, the offset selection, the 0.2F smoothing,
// `15 - target`) is the trivial scalar composition of updateSignalStrength,
// mirrored exactly with the same FLOAT/INT types so the LUT index and the
// rounding input are bit-identical.
//
// We bootstrap SharedConstants/Bootstrap so Mth's SIN table builds exactly as
// in-game, and resolve Mth.cos / Mth.clamp by reflection against the REAL class.
//
// Output: each row carries the int sky brightness, the SUN_ANGLE in DEGREES as
// 8-hex raw int bits (Float.floatToRawIntBits), the inverted flag (0/1) and the
// expected power as a decimal int. The C++ side recomputes and compares exactly.
//
// Row tag (tab-separated):
//   PWR   <skyBrightness>   <sunAngleDegBits8>   <inverted0/1>   <power>
import java.lang.reflect.Method;

public class DaylightDetectorPowerParity {
    static final java.io.PrintStream O = System.out;

    static String fb(float f) { return String.format("%08x", Float.floatToRawIntBits(f)); }

    // The REAL Mth.cos (LUT) and Mth.clamp(int,int,int), resolved reflectively.
    static Method MTH_COS;
    static Method MTH_CLAMP;

    static float realMthCos(double x) throws Exception {
        return (Float) MTH_COS.invoke(null, x);
    }
    static int realMthClampInt(int v, int min, int max) throws Exception {
        return (Integer) MTH_CLAMP.invoke(null, v, min, max);
    }

    // Exact 1:1 mirror of the scalar core of DaylightDetectorBlock.updateSignalStrength,
    // delegating the cosine to the REAL Mth.cos and the rounding to the REAL
    // java.lang.Math.round(float).
    static int computePower(int effectiveSkyBrightness, float sunAngleDegrees, boolean inverted) throws Exception {
        int target = effectiveSkyBrightness;
        float sunAngle = sunAngleDegrees * (float)(Math.PI / 180.0);

        if (inverted) {
            target = 15 - target;
        } else if (target > 0) {
            float offset = sunAngle < (float) Math.PI ? 0.0F : (float)(Math.PI * 2);
            sunAngle += (offset - sunAngle) * 0.2F;
            target = Math.round(target * realMthCos((double) sunAngle));
        }

        return realMthClampInt(target, 0, 15);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> mth = Class.forName("net.minecraft.util.Mth");
        MTH_COS = mth.getMethod("cos", double.class);
        MTH_COS.setAccessible(true);
        MTH_CLAMP = mth.getMethod("clamp", int.class, int.class, int.class);
        MTH_CLAMP.setAccessible(true);

        // Effective sky brightness domain: getBrightness(SKY) in 0..15 minus
        // getSkyDarken() in 0..11 -> -11..15. Sweep the full reachable range plus
        // the clamp boundaries.
        int[] skyVals = {
            -11, -10, -5, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
        };

        // SUN_ANGLE is an ANGLE_DEGREES attribute; the in-game value cycles
        // 0..360. Sweep a fine grid over [0,360) plus the radian-comparison
        // boundary neighbours (degrees that map sunAngle near (float)Math.PI),
        // plus a few out-of-canonical-range probes (negative / >360) to exercise
        // the LUT's (long)& 65535 wraparound and the offset branch on both sides.
        java.util.ArrayList<Float> angles = new java.util.ArrayList<>();
        for (int d = 0; d < 360; d++) angles.add((float) d);           // every integer degree
        for (int h = 0; h < 360; h++) angles.add(h + 0.5F);            // half-degree offsets
        // Boundary around 180 deg (sunAngle == (float)Math.PI) at fine resolution.
        for (float d = 179.0F; d <= 181.0F; d += 0.05F) angles.add(d);
        // Out-of-canonical probes.
        float[] extra = { -360.0F, -180.0F, -90.0F, -0.5F, 360.0F, 540.0F, 720.0F, 359.99F, 0.0001F };
        for (float e : extra) angles.add(e);

        for (int sky : skyVals) {
            for (float deg : angles) {
                for (int inv = 0; inv <= 1; inv++) {
                    boolean inverted = inv == 1;
                    int power = computePower(sky, deg, inverted);
                    O.println("PWR\t" + sky + "\t" + fb(deg) + "\t" + inv + "\t" + power);
                }
            }
        }
    }
}
