// Ground-truth generator for com.mojang.math.Axis.of(Vector3f) (Minecraft 26.1.2).
//
// Drives the REAL com.mojang.math.Axis.of(vector) factory, which returns
//     angle -> new Quaternionf().rotationAxis(angle, vector)
// and emits the four Quaternionf components (x,y,z,w) as raw IEEE-754 float bits
// for both rotation(radians) and rotationDegrees(deg).
//
// This is the arbitrary-axis path (Quaternionf.rotationAxis, which normalizes the
// axis with org.joml.Math.invsqrt) — distinct from the X/Y/Z builders already
// covered by MathAxisParity.java. The math goes through libm (Math.sin/sqrt), so
// the C++ gate matches bit-for-bit via the Joml.h jsin/jinvsqrt/cosFromSin
// helpers — nothing looser is tolerated.
//
//   tools/run_groundtruth.ps1 -Tool MathAxisArbitraryParity -Out mcpp/build/math_axis_arbitrary.tsv
//
// Classpath needs the joml jar (Axis/Vector3f import org.joml). Axis.of is pure
// (no registry bootstrap required); the bootstrap guard is defensive only.
//
// TSV rows:
//   ROT     <axBits> <ayBits> <azBits>  <angleBits>  <xBits> <yBits> <zBits> <wBits>
//   ROTDEG  <axBits> <ayBits> <azBits>  <degBits>    <xBits> <yBits> <zBits> <wBits>

import com.mojang.math.Axis;
import org.joml.Quaternionf;
import org.joml.Vector3f;

public class MathAxisArbitraryParity {
    static final java.io.PrintStream O = System.out;

    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static String q(Quaternionf qq) {
        return b(qq.x) + "\t" + b(qq.y) + "\t" + b(qq.z) + "\t" + b(qq.w);
    }

    public static void main(String[] args) throws Exception {
        // Defensive: Axis.of is pure, but guard in case a class init pulls in
        // registries. (O captured at class-load, above.)
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // Axis.of does not need the bootstrap; ignore if unavailable.
        }

        // A battery of axis vectors: axis-aligned, diagonals, normalized and
        // unnormalized (rotationAxis renormalizes via invsqrt), small/large
        // magnitudes, mixed signs, and a few irrational components. NO zero
        // vector (invsqrt(0) = +Inf, a libm-divergent edge per the parity rules).
        float[][] AXES = {
            {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
            {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, -1.0f},
            {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, 1.0f},
            {0.5f, 0.5f, 0.70710677f}, {0.26726124f, 0.5345225f, 0.8017837f},
            {2.0f, 3.0f, 6.0f}, {-2.0f, 3.0f, -6.0f}, {10.0f, -5.0f, 2.0f},
            {0.1f, 0.2f, 0.3f}, {-0.1f, -0.2f, -0.3f},
            {100.0f, 0.5f, -0.25f}, {0.001f, 0.002f, 0.003f},
            {1.234f, -5.678f, 9.012f}, {0.57735026f, 0.57735026f, 0.57735026f},
            {3.0f, 4.0f, 0.0f}, {0.0f, 3.0f, 4.0f}, {5.0f, 0.0f, 12.0f}
        };

        // Radian inputs: zeros, small, the canonical fractions of pi, full turns,
        // negatives, and a few odd magnitudes (matches MathAxisParity's battery).
        float[] RAD = {
            0.0f, -0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 0.25f, 0.125f,
            0.3f, -0.3f, 1.234f, -1.234f, 2.0f, -2.0f, 2.5f, -2.5f, 3.0f, -3.0f,
            0.7853982f, -0.7853982f,    // pi/4
            1.5707964f, -1.5707964f,    // pi/2
            3.1415927f, -3.1415927f,    // pi
            6.2831855f, -6.2831855f,    // 2*pi
            4.712389f,  -4.712389f,     // 3*pi/2
            0.017453292f,               // 1 degree in radians
            0.123456f, -0.123456f, 0.001f, -0.001f, 10.0f, -10.0f,
            100.0f, -100.0f, 12.566371f // 4*pi
        };

        // Degree inputs covering pipeline multiples (22.5 / 90) plus edges,
        // negatives, and odd values.
        float[] DEG = {
            0.0f, -0.0f, 22.5f, -22.5f, 45.0f, -45.0f, 67.5f, -67.5f,
            90.0f, -90.0f, 135.0f, -135.0f, 180.0f, -180.0f, 225.0f, -225.0f,
            270.0f, -270.0f, 315.0f, -315.0f, 360.0f, -360.0f,
            1.0f, -1.0f, 30.0f, -30.0f, 60.0f, -60.0f, 120.0f, -120.0f,
            0.5f, 7.3f, -7.3f, 123.456f, -123.456f, 720.0f, -720.0f,
            0.001f, 1000.0f, -1000.0f
        };

        for (float[] v : AXES) {
            String ab = b(v[0]) + "\t" + b(v[1]) + "\t" + b(v[2]);
            Axis ax = Axis.of(new Vector3f(v[0], v[1], v[2]));
            for (float a : RAD) {
                O.println("ROT\t" + ab + "\t" + b(a) + "\t" + q(ax.rotation(a)));
            }
            for (float d : DEG) {
                O.println("ROTDEG\t" + ab + "\t" + b(d) + "\t" + q(ax.rotationDegrees(d)));
            }
        }
    }
}
