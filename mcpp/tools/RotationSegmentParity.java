// Ground-truth generator for
// net.minecraft.world.level.block.state.properties.RotationSegment (26.1.2).
//
// Drives the REAL class (all public static methods) over a finite battery of physical
// inputs and dumps tab-separated <TAG>\t<inputs...>\t<outputs...> rows to STDOUT.
// Floats are emitted as the raw IEEE-754 bits (%08x of Float.floatToRawIntBits);
// ints/bools decimal. For convertToDirection's Optional<Direction>, we emit the
// Direction's get3DDataValue() on a hit or -1 when empty (matches the C++ port).
//
//   tools/run_groundtruth.ps1 -Tool RotationSegmentParity -Out mcpp/build/rotation_segment.tsv

import java.util.Optional;
import net.minecraft.core.Direction;
import net.minecraft.world.level.block.state.properties.RotationSegment;

public class RotationSegmentParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // finite/physical degrees (turns/fractions of turns, signed; covers wrap & the
    // >=180 -> -360 branch in toDegrees, and the 16 segment boundaries 22.5*k).
    static final float[] DEGREES = {
        0.0f, 1.0f, 11.25f, 22.5f, 33.75f, 45.0f, 67.5f, 89.0f, 90.0f, 90.5f,
        112.5f, 135.0f, 157.5f, 179.0f, 180.0f, 180.5f, 181.0f, 202.5f, 225.0f,
        247.5f, 270.0f, 292.5f, 315.0f, 337.5f, 359.0f, 359.9f, 360.0f, 361.0f,
        382.5f, 405.0f, 450.0f, 540.0f, 720.0f, 723.7f, 1000.0f, 7.5f, 0.5f, 0.1f,
        -1.0f, -22.5f, -45.0f, -90.0f, -180.0f, -270.0f, -360.0f, -722.0f,
        123.456f, -123.456f
    };

    // finite binary-angle / segment inputs for convertToDegrees & convertToDirection.
    static final int[] SEGMENTS = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 20, 31, 32,
        100, 127, 128, 255, 256, 1000, 65535, 65536, -1, -2, -4, -8, -12, -16,
        -100, -256, Integer.MAX_VALUE, Integer.MIN_VALUE
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // getMaxSegmentIndex()  -> mask (15 for SegmentedAnglePrecision(4)).
        // MAXIDX  getMaxSegmentIndex()
        O.println("MAXIDX\t" + RotationSegment.getMaxSegmentIndex());

        // convertToSegment(Direction) over all 6 directions. Inputs we pass to C++:
        // get2DDataValue() + axis.isVertical().
        for (Direction d : Direction.values()) {
            boolean vert = d.getAxis().isVertical();
            int data2d = d.get2DDataValue();
            int seg = RotationSegment.convertToSegment(d);
            // FROMDIR  data2d  isVertical(0/1)  convertToSegment
            O.println("FROMDIR\t" + data2d + "\t" + (vert ? 1 : 0) + "\t" + seg);
        }

        // convertToSegment(float degrees).
        for (float deg : DEGREES) {
            int seg = RotationSegment.convertToSegment(deg);
            // FROMDEG  degrees(bits)  convertToSegment
            O.println("FROMDEG\t" + f(deg) + "\t" + seg);
        }

        // convertToDegrees(int) and convertToDirection(int).
        for (int s : SEGMENTS) {
            float deg = RotationSegment.convertToDegrees(s);
            Optional<Direction> dir = RotationSegment.convertToDirection(s);
            int data3d = dir.map(Direction::get3DDataValue).orElse(-1);
            // TODEG  segment  convertToDegrees(bits)  convertToDirection(data3d|-1)
            O.println("TODEG\t" + s + "\t" + f(deg) + "\t" + data3d);
        }
    }
}
