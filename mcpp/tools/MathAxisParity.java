// Ground-truth generator for com.mojang.math.Axis (Minecraft 26.1.2).
// Calls the REAL com.mojang.math.Axis constants (XN/XP/YN/YP/ZN/ZP) and emits
// the four Quaternionf components (x,y,z,w) as raw IEEE-754 float bits, for both
// rotation(radians) and rotationDegrees(deg).
//
// Axis builds the quaternion via org.joml.Quaternionf.rotationX/Y/Z, which use
// org.joml.Math.sin == (float)java.lang.Math.sin((double)x) (FASTMATH=false) and
// Math.cosFromSin. These go through libm, so the C++ gate must tolerate nothing
// beyond the bit-exact match the repo's Joml.h jsin/cosFromSin already deliver.
//
//   tools/run_groundtruth.ps1 -Tool MathAxisParity -Out mcpp/build/math_axis.tsv
//
// Classpath needs the joml jar (Axis imports org.joml). Axis itself is pure (no
// registry bootstrap required), but we add the bootstrap guard defensively in
// case class init pulls anything in.

import com.mojang.math.Axis;
import org.joml.Quaternionf;

public class MathAxisParity {
    static final java.io.PrintStream O = System.out;

    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static String q(Quaternionf qq) {
        return b(qq.x) + "\t" + b(qq.y) + "\t" + b(qq.z) + "\t" + b(qq.w);
    }

    // The six providers, with a tag for each.
    static final String[] TAGS = { "XN", "XP", "YN", "YP", "ZN", "ZP" };
    static final Axis[]   AXES = { Axis.XN, Axis.XP, Axis.YN, Axis.YP, Axis.ZN, Axis.ZP };

    public static void main(String[] args) throws Exception {
        // Defensive: some net.minecraft class init triggers registries. Axis is
        // pure, but harmless to guard. (O captured at class-load, above.)
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // Axis does not need the bootstrap; ignore if unavailable.
        }

        // A thorough battery of radian inputs: zeros, small, the canonical
        // fractions of pi, full turns, negatives, and a few odd magnitudes.
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

        // Degree inputs covering the values the model pipeline actually uses
        // (multiples of 22.5 / 90) plus edges and negatives and odd values.
        float[] DEG = {
            0.0f, -0.0f, 22.5f, -22.5f, 45.0f, -45.0f, 67.5f, -67.5f,
            90.0f, -90.0f, 135.0f, -135.0f, 180.0f, -180.0f, 225.0f, -225.0f,
            270.0f, -270.0f, 315.0f, -315.0f, 360.0f, -360.0f,
            1.0f, -1.0f, 30.0f, -30.0f, 60.0f, -60.0f, 120.0f, -120.0f,
            0.5f, 7.3f, -7.3f, 123.456f, -123.456f, 720.0f, -720.0f,
            0.001f, 1000.0f, -1000.0f
        };

        for (int i = 0; i < TAGS.length; i++) {
            Axis ax = AXES[i];
            String tg = TAGS[i];
            for (float a : RAD) {
                O.println("ROT\t" + tg + "\t" + b(a) + "\t" + q(ax.rotation(a)));
            }
            for (float d : DEG) {
                O.println("ROTDEG\t" + tg + "\t" + b(d) + "\t" + q(ax.rotationDegrees(d)));
            }
        }
    }
}
