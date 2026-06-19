// Ground-truth generator for the PURE static helper
//   net.minecraft.world.entity.animal.happyghast.HappyGhast$HappyGhastLookControl
//       .wrapDegrees90(float angle)
// of Minecraft 26.1.2.
//
// wrapDegrees90 is `public static` but declared inside the PRIVATE inner class
// HappyGhastLookControl, so we reach it PURELY REFLECTIVELY (no instance needed —
// it is static, and no Bootstrap is required because the body touches only floats).
// We NEVER replicate the body here; every emitted value comes from the real method.
//
//   tools/run_groundtruth.ps1 -Tool HappyGhastWrapDegrees90Parity -Out mcpp/build/happy_ghast_wrap_degrees90.tsv
//
// TSV rows (one TAG):
//   WRAP90  <angleBits>  <resultBits>
// where both fields are the raw IEEE-754 int32 bit patterns (Float.floatToRawIntBits)
// of the float input / output, emitted as decimal int32. Using raw bits makes the
// comparison bit-exact (and lossless for NaN payloads / signed zero).

@SuppressWarnings({"deprecation", "unchecked"})
public class HappyGhastWrapDegrees90Parity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Resolve the private inner class and its static wrapDegrees90(float) method.
        Class<?> lookControl = Class.forName(
            "net.minecraft.world.entity.animal.happyghast.HappyGhast$HappyGhastLookControl");
        java.lang.reflect.Method m = lookControl.getDeclaredMethod("wrapDegrees90", float.class);
        m.setAccessible(true);

        // Build a finite-but-thorough set of input angles. The interesting behaviour
        // lives at the quarter-turn boundaries (+/-45, +/-90, multiples thereof) and
        // either side of them, so we sweep a fine grid plus the exact thresholds and
        // a few special IEEE-754 values.
        java.util.LinkedHashSet<Float> inputs = new java.util.LinkedHashSet<>();

        // Fine sweep over several full turns, step 0.5 deg, both signs.
        for (float a = -720.0F; a <= 720.0F; a += 0.5F) {
            inputs.add(a);
        }
        // Exact boundary / multiple-of-90 / multiple-of-45 thresholds where the
        // >= 45 and < -45 branches flip.
        float[] thresholds = {
            -360.0F, -315.0F, -270.0F, -225.0F, -180.0F, -135.0F, -90.0F,
            -45.0F, -44.999996F, -45.000004F, -0.0F, 0.0F,
            44.999996F, 45.0F, 45.000004F, 90.0F, 135.0F, 180.0F,
            225.0F, 270.0F, 315.0F, 360.0F,
            89.999996F, 90.000008F, -89.999996F, -90.000008F,
        };
        for (float a : thresholds) inputs.add(a);

        // A spread of arbitrary non-grid magnitudes (positive and negative),
        // including large values where float granularity matters.
        float[] misc = {
            1.0F, -1.0F, 12.34F, -12.34F, 47.5F, -47.5F, 89.9F, -89.9F,
            123.456F, -123.456F, 359.9F, -359.9F, 1000.0F, -1000.0F,
            8192.5F, -8192.5F, 100000.0F, -100000.0F,
            1.0E7F, -1.0E7F, 1.0E20F, -1.0E20F,
            (float) Math.PI, (float) -Math.PI,
            Float.MIN_VALUE, -Float.MIN_VALUE,
            3.4028235E38F, -3.4028235E38F,  // +/- Float.MAX_VALUE
        };
        for (float a : misc) inputs.add(a);

        // Special values: signed zero, infinities, NaN — exercise the propagation traps.
        inputs.add(Float.POSITIVE_INFINITY);
        inputs.add(Float.NEGATIVE_INFINITY);
        inputs.add(Float.NaN);

        for (Float a : inputs) {
            float in = a;
            float out = (float) m.invoke(null, in);
            O.println("WRAP90\t" + Float.floatToRawIntBits(in) + "\t" + Float.floatToRawIntBits(out));
        }
    }
}
