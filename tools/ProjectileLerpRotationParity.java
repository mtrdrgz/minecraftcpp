// Ground-truth generator for
// net.minecraft.world.entity.projectile.Projectile.lerpRotation
// (Projectile.java:333-343) — the static per-tick rotation smoother that un-wraps the
// previous angle into the target's +/-180-degree neighbourhood (two while loops over
// +/-360) and lerps 20% toward it. protected static; reached via reflection. Pure
// (two floats in, one float out, only Mth.lerp), but loading Projectile triggers
// Entity static init -> BuiltInRegistries, so Bootstrap is required. We capture O at
// class load (before Bootstrap), keeping the TSV on stdout clean.
//
//   tools/run_groundtruth.ps1 -Tool ProjectileLerpRotationParity -Out mcpp/build/projectile_lerp_rotation.tsv

import java.lang.reflect.Method;
import net.minecraft.world.entity.projectile.Projectile;

public class ProjectileLerpRotationParity {
    static final java.io.PrintStream O = System.out;
    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // Representative rotO (previous angle) values: in-range, out-of-range, negatives.
    static final float[] ROT_O = {
        0.0f, 45.0f, 90.0f, 179.0f, 180.0f, -180.0f, -179.0f, -90.0f,
        359.0f, 360.0f, 720.0f, -360.0f, -720.0f, 1000.0f, -1000.0f,
        12.5f, -33.3f, 175.0f, -175.0f, 0.1f,
    };
    // Representative rot (target angle) values: span the +/-180 wrap boundaries so the
    // while loops fire 0, 1, and multiple times, plus exact +/-180 boundary hits and a
    // far target (rot - rotO ~ 540) that loops more than once.
    static final float[] ROT = {
        0.0f, 90.0f, -90.0f, 180.0f, -180.0f, 181.0f, -181.0f, 270.0f, -270.0f,
        359.0f, 360.0f, 540.0f, -540.0f, 720.0f, -720.0f,
        1.0f, -1.0f, 179.999f, -179.999f, 33.7f, -33.7f, 1000.0f, -1000.0f,
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Method m = Projectile.class.getDeclaredMethod("lerpRotation", float.class, float.class);
        m.setAccessible(true);

        for (float a : ROT_O) for (float b : ROT) {
            float r = (Float) m.invoke(null, a, b);
            O.println("LERP" + "\t" + f(a) + "\t" + f(b) + "\t" + f(r));
        }
    }
}
