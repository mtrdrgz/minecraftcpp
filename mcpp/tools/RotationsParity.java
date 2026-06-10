// Ground-truth generator for net.minecraft.core.Rotations (26.1.2) using the REAL
// decompiled class. Rotations is a record(float x, float y, float z) whose canonical
// constructor normalizes each component:
//     v = !Float.isInfinite(v) && !Float.isNaN(v) ? v % 360.0F : 0.0F;
// We emit, for each finite input triple, the post-normalization x()/y()/z() accessors.
// We also dump Mth.wrapDegrees(float) (the older getWrapped* helper) so the C++ gate
// re-verifies the reused, certified mc::levelgen::mth::wrapDegrees against the real Mth.
//
// All floats are exchanged as raw IEEE-754 bits (%08x) so the gate is bit-exact.
// FINITE/PHYSICAL inputs only (no NaN/Inf/-0.0) — the non-finite branch only differs in
// sign/NaN bits and is covered structurally by the constructor's literal guard.
//
//   tools/run_groundtruth.ps1 -Tool RotationsParity -Out mcpp/build/rotations.tsv

import net.minecraft.core.Rotations;
import net.minecraft.util.Mth;

public class RotationsParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // Finite, physically-plausible rotation angles (degrees) plus boundary cases that
    // exercise the % 360.0F normalization (multiples of 360, just under/over, negatives).
    static final float[] ANGLES = {
        0.0f, 1.0f, -1.0f, 45.0f, 90.0f, -90.0f, 135.0f, 180.0f, -180.0f, 270.0f, -270.0f,
        359.0f, 359.9f, 360.0f, 360.1f, 361.0f, 720.0f, 720.5f, -360.0f, -359.5f, -720.0f,
        540.3f, -540.3f, 1234.5678f, -1234.5678f, 30.5f, 60.25f, 89.999f, 120.0f, 150.75f,
        0.0001f, 12345.6789f, -12345.6789f, 100.25f, -100.75f, 33.333f, 0.16666f, 7.5f
    };

    public static void main(String[] args) throws Exception {
        // Rotations has no registry/world coupling, so no Bootstrap is required.
        // Exercise the constructor normalization + record accessors across a battery of
        // distinct (x,y,z) triples drawn from ANGLES (rotate the index per component).
        int n = ANGLES.length;
        for (int i = 0; i < n; i++) {
            float ax = ANGLES[i];
            float ay = ANGLES[(i + 7) % n];
            float az = ANGLES[(i + 13) % n];
            Rotations r = new Rotations(ax, ay, az);
            // ROT <inX> <inY> <inZ> <outX> <outY> <outZ>
            O.println("ROT\t" + f(ax) + "\t" + f(ay) + "\t" + f(az) + "\t"
                       + f(r.x()) + "\t" + f(r.y()) + "\t" + f(r.z()));
        }
        // Also a dense diagonal sweep so each accessor sees the same value (catches any
        // per-component asymmetry in the normalization).
        for (float a : ANGLES) {
            Rotations r = new Rotations(a, a, a);
            O.println("ROT\t" + f(a) + "\t" + f(a) + "\t" + f(a) + "\t"
                       + f(r.x()) + "\t" + f(r.y()) + "\t" + f(r.z()));
        }

        // Mth.wrapDegrees(float) — the helper behind the older getWrapped* getters.
        for (float a : ANGLES) {
            O.println("WRAPDEG_F\t" + f(a) + "\t" + f(Mth.wrapDegrees(a)));
        }
    }
}
