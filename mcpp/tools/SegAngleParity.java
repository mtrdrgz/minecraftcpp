// Ground-truth generator for net.minecraft.util.SegmentedAnglePrecision (26.1.2).
// Drives the REAL class: constructs instances at several bit precisions and dumps
// the derived fields (mask/precision/degreeToAngle/angleToDegree, via reflection)
// plus every public method over a finite battery of physical inputs.
//
// Output: tab-separated <TAG>\t<inputs...>\t<outputs...>. Floats are emitted as the
// raw IEEE-754 bits (%08x of Float.floatToRawIntBits); ints/bools decimal.
//
//   tools/run_groundtruth.ps1 -Tool SegAngleParity -Out mcpp/build/seg_angle.tsv

import java.lang.reflect.Field;
import net.minecraft.core.Direction;
import net.minecraft.util.SegmentedAnglePrecision;

public class SegAngleParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // bit precisions to exercise: min(2), the in-game value(4), some mid + max(30).
    static final int[] PRECISIONS = { 2, 3, 4, 5, 6, 8, 12, 16, 24, 30 };

    // finite/physical binary-angle inputs (no NaN/Inf — pure ints).
    static final int[] BINARY_ANGLES = {
        0, 1, 2, 3, 4, 5, 7, 8, 12, 15, 16, 31, 32, 63, 64, 100, 127, 128, 255, 256,
        500, 1000, 4095, 4096, 32767, 65535, 65536, -1, -2, -3, -4, -8, -16, -100,
        -255, -256, -4096, -65536, 1 << 29, (1 << 30) - 1, Integer.MAX_VALUE,
        Integer.MIN_VALUE
    };

    // finite/physical degrees (turns/fractions of turns, signed; covers wrap).
    static final float[] DEGREES = {
        0.0f, 1.0f, 22.5f, 45.0f, 89.0f, 90.0f, 90.5f, 135.0f, 179.0f, 180.0f,
        180.5f, 181.0f, 225.0f, 270.0f, 315.0f, 359.0f, 359.9f, 360.0f, 361.0f,
        405.0f, 450.0f, 540.0f, 720.0f, 723.7f, 1000.0f, 11.25f, 33.75f, 7.5f,
        -1.0f, -45.0f, -90.0f, -180.0f, -270.0f, -360.0f, -722.0f, 0.5f, 0.1f,
        123.456f, -123.456f
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Field fMask = SegmentedAnglePrecision.class.getDeclaredField("mask"); fMask.setAccessible(true);
        Field fPrec = SegmentedAnglePrecision.class.getDeclaredField("precision"); fPrec.setAccessible(true);
        Field fD2A  = SegmentedAnglePrecision.class.getDeclaredField("degreeToAngle"); fD2A.setAccessible(true);
        Field fA2D  = SegmentedAnglePrecision.class.getDeclaredField("angleToDegree"); fA2D.setAccessible(true);

        Direction[] dirs = Direction.values();

        for (int p : PRECISIONS) {
            SegmentedAnglePrecision s = new SegmentedAnglePrecision(p);

            // Derived fields + getMask().
            int mask = fMask.getInt(s);
            int prec = fPrec.getInt(s);
            float d2a = fD2A.getFloat(s);
            float a2d = fA2D.getFloat(s);
            // FIELDS  p  mask  precision  degreeToAngle  angleToDegree  getMask()
            O.println("FIELDS\t" + p + "\t" + mask + "\t" + prec + "\t" + f(d2a) + "\t" + f(a2d) + "\t" + s.getMask());

            // fromDirection over all 6 directions. Inputs we pass to C++: data2d + isVertical.
            for (Direction d : dirs) {
                boolean vert = d.getAxis().isVertical();
                int data2d = d.get2DDataValue();
                int out = s.fromDirection(d);
                // FROMDIR  p  data2d  isVertical(0/1)  out
                O.println("FROMDIR\t" + p + "\t" + data2d + "\t" + (vert ? 1 : 0) + "\t" + out);
            }

            // normalize / toDegreesWithTurns / toDegrees / isSameAxis over binary angles.
            for (int ba : BINARY_ANGLES) {
                int norm = s.normalize(ba);
                float tdwt = s.toDegreesWithTurns(ba);
                float td = s.toDegrees(ba);
                // NORM  p  ba  normalize  toDegreesWithTurns(bits)  toDegrees(bits)
                O.println("NORM\t" + p + "\t" + ba + "\t" + norm + "\t" + f(tdwt) + "\t" + f(td));
            }
            for (int a : BINARY_ANGLES) {
                for (int b : BINARY_ANGLES) {
                    boolean same = s.isSameAxis(a, b);
                    // SAMEAXIS  p  a  b  same(0/1)
                    O.println("SAMEAXIS\t" + p + "\t" + a + "\t" + b + "\t" + (same ? 1 : 0));
                }
            }

            // fromDegreesWithTurns / fromDegrees over degrees.
            for (float deg : DEGREES) {
                int fdwt = s.fromDegreesWithTurns(deg);
                int fd = s.fromDegrees(deg);
                // FROMDEG  p  degrees(bits)  fromDegreesWithTurns  fromDegrees
                O.println("FROMDEG\t" + p + "\t" + f(deg) + "\t" + fdwt + "\t" + fd);
            }
        }
    }
}
