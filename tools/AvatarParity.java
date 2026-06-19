// Ground-truth generator for the bounded, portable math surface of the Avatar
// assignment. net.minecraft.world.entity.Avatar itself is registry/world/entity-
// data/component coupled (LivingEntity, EntityDimensions, SynchedEntityData,
// EntityType, Level, ResolvableProfile, Pose) and is NOT a standalone bit-exact
// math gate. The actual deliverable is the gate target `vec2_done_parity` over
// TSV `vec2_done.tsv`, which re-certifies net.minecraft.world.phys.Vec2 — the
// float-precision 2D rotation/math vector — against the existing C++ port
// (mcpp/src/world/phys/Vec2.h). This is an INDEPENDENT second gate alongside the
// pre-existing vec2_parity, with a fresh FINITE/PHYSICAL-only input battery.
//
// Pure; no Bootstrap needed. Floats emitted as raw IEEE-754 bits (%08x).
//
//   tools/run_groundtruth.ps1 -Tool AvatarParity -Out mcpp/build/vec2_done.tsv

import net.minecraft.world.phys.Vec2;

public class AvatarParity {
    static final java.io.PrintStream O = System.out;
    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String v2(Vec2 v) { return f(v.x) + "\t" + f(v.y); }

    // FINITE / PHYSICAL inputs only (no NaN / +-Infinity / -0.0): zeros, ones,
    // units, negatives, fractions, mixed/asymmetric magnitudes, sub-1e-4 lengths
    // (to exercise both branches of normalized()), and the public constants.
    static final float[][] VS = {
        {0f, 0f}, {1f, 1f}, {1f, 0f}, {0f, 1f}, {-1f, 0f}, {0f, -1f},
        {2f, 3f}, {-2.5f, 4.5f}, {6.5f, -1.25f}, {15f, -30f},
        {0.3f, 0.7f}, {-4.2f, 9.6f}, {250f, -250f}, {0.75f, 0.25f},
        {-0.00003f, 0.00002f}, {2.5e5f, -2.5e5f}, {5.5f, -5.5f},
        {0.00004f, 0.00004f},           // length ~5.66e-5 < 1e-4 -> ZERO branch
        {0.0001f, 0f},                  // length exactly 1e-4 -> NOT < 1e-4
        {0.00012f, 0f},                 // length 1.2e-4 -> non-ZERO branch
        {3.4e38f, 1.0e38f}, {1.4e-30f, 2.8e-30f},
        {321.789f, 654.123f}, {-12.5f, 0.0625f}, {0.001f, 0.002f},
        {45f, 60f}, {-90f, 180f}, {1234.5f, -678.9f}, {7e-5f, 7e-5f}, {9.9e-5f, 0f}
    };
    static final float[] SCALARS = { 0f, 1f, -1f, 3.5f, 0.25f, -2.0f, 64f, 1e-4f, -0.125f, 0.5f };
    static final float[] ADDENDS = { 0f, 1f, -1f, 0.75f, -3.25f, 16f, 0.5f };

    public static void main(String[] args) throws Exception {
        for (float[] a : VS) {
            Vec2 va = new Vec2(a[0], a[1]);
            String in = f(a[0]) + "\t" + f(a[1]);
            O.println("LENGTH\t"     + in + "\t" + f(va.length()));
            O.println("LENGTHSQ\t"   + in + "\t" + f(va.lengthSquared()));
            O.println("NORMALIZED\t" + in + "\t" + v2(va.normalized()));
            O.println("NEGATED\t"    + in + "\t" + v2(va.negated()));
            for (float s : SCALARS)
                O.println("SCALE\t" + in + "\t" + f(s) + "\t" + v2(va.scale(s)));
            for (float v : ADDENDS)
                O.println("ADDF\t" + in + "\t" + f(v) + "\t" + v2(va.add(v)));
        }
        // Binary ops over all ordered pairs.
        for (float[] a : VS) for (float[] b : VS) {
            Vec2 va = new Vec2(a[0], a[1]), vb = new Vec2(b[0], b[1]);
            String in = f(a[0]) + "\t" + f(a[1]) + "\t" + f(b[0]) + "\t" + f(b[1]);
            O.println("ADD\t"     + in + "\t" + v2(va.add(vb)));
            O.println("DOT\t"     + in + "\t" + f(va.dot(vb)));
            O.println("DISTSQR\t" + in + "\t" + f(va.distanceToSqr(vb)));
            O.println("EQUALS\t"  + in + "\t" + (va.equals(vb) ? 1 : 0));
        }
        // Public constants — verify the C++ literals match the real fields.
        O.println("CONST\tZERO\t"       + v2(Vec2.ZERO));
        O.println("CONST\tONE\t"        + v2(Vec2.ONE));
        O.println("CONST\tUNIT_X\t"     + v2(Vec2.UNIT_X));
        O.println("CONST\tNEG_UNIT_X\t" + v2(Vec2.NEG_UNIT_X));
        O.println("CONST\tUNIT_Y\t"     + v2(Vec2.UNIT_Y));
        O.println("CONST\tNEG_UNIT_Y\t" + v2(Vec2.NEG_UNIT_Y));
        O.println("CONST\tMAX\t"        + v2(Vec2.MAX));
        O.println("CONST\tMIN\t"        + v2(Vec2.MIN));
    }
}
