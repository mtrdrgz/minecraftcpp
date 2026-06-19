// Ground-truth generator for net.minecraft.world.phys.Vec3 (the double-precision
// movement/physics/raytrace vector). Pure; no Bootstrap. The C++ port (world/phys/
// Vec3.h) must match bit-for-bit (doubles/floats as raw IEEE-754 bits).
//
//   tools/run_groundtruth.ps1 -Tool Vec3Parity -Out mcpp/build/vec3.tsv

import net.minecraft.core.Direction;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.Vec3;

public class Vec3Parity {
    static final java.io.PrintStream O = System.out;
    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }
    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String v3(Vec3 v) { return d(v.x) + "\t" + d(v.y) + "\t" + d(v.z); }

    static final double[][] VS = {
        {0,0,0}, {1,2,3}, {-1.5,0.5,2.25}, {10,-20,30}, {0.1,0.2,0.3},
        {-3.7,8.2,-0.001}, {100,0,-100}, {0.5,0.5,0.5}, {-0.0001,0.0,1e6}, {2.5,-2.5,2.5}
    };
    static final float[] RADS = { 0f, 0.5f, 1.5707964f, 3.1415927f, -0.7853982f, 2.5f, -3.0f, 0.123f };
    static final float[] ANGLES = { 0f, 45f, 90f, 180f, -90f, 30f, 135f, -22.5f };

    public static void main(String[] args) {
        Direction[] DIRS = Direction.values();
        for (double[] a : VS) {
            Vec3 va = new Vec3(a[0], a[1], a[2]);
            O.println("NORMALIZE\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + v3(va.normalize()));
            O.println("LENGTH\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + d(va.length()));
            O.println("LENGTHSQR\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + d(va.lengthSqr()));
            O.println("HDIST\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + d(va.horizontalDistance()));
            O.println("REVERSE\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + v3(va.reverse()));
            O.println("HORIZ\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + v3(va.horizontal()));
            O.println("ROTCW90\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + v3(va.rotateClockwise90()));
            O.println("ISFINITE\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + (va.isFinite() ? 1 : 0));
            O.println("SCALE\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + d(2.5) + "\t" + v3(va.scale(2.5)));
            for (int ax = 0; ax < 3; ax++) O.println("GET\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + ax + "\t" + d(va.get(net.minecraft.core.Direction.Axis.values()[ax])));
            for (int ax = 0; ax < 3; ax++) O.println("WITH\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + ax + "\t" + d(7.5) + "\t" + v3(va.with(net.minecraft.core.Direction.Axis.values()[ax], 7.5)));
            for (Direction dir : DIRS) O.println("RELATIVE\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + dir.ordinal() + "\t" + d(3.5) + "\t" + v3(va.relative(dir, 3.5)));
            for (float r : RADS) {
                O.println("XROT\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + f(r) + "\t" + v3(va.xRot(r)));
                O.println("YROT\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + f(r) + "\t" + v3(va.yRot(r)));
                O.println("ZROT\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + f(r) + "\t" + v3(va.zRot(r)));
            }
            // rotation() — atan2/asin
            Vec2 rot = va.rotation();
            O.println("ROTATION\t" + d(a[0])+"\t"+d(a[1])+"\t"+d(a[2]) + "\t" + f(rot.x) + "\t" + f(rot.y));
        }
        for (double[] a : VS) for (double[] b : VS) {
            Vec3 va = new Vec3(a[0],a[1],a[2]), vb = new Vec3(b[0],b[1],b[2]);
            String in = d(a[0])+"\t"+d(a[1])+"\t"+d(a[2])+"\t"+d(b[0])+"\t"+d(b[1])+"\t"+d(b[2]);
            O.println("ADD\t" + in + "\t" + v3(va.add(vb)));
            O.println("SUB\t" + in + "\t" + v3(va.subtract(vb)));
            O.println("MUL\t" + in + "\t" + v3(va.multiply(vb)));
            O.println("DOT\t" + in + "\t" + d(va.dot(vb)));
            O.println("CROSS\t" + in + "\t" + v3(va.cross(vb)));
            O.println("VECTORTO\t" + in + "\t" + v3(va.vectorTo(vb)));
            O.println("DISTSQR\t" + in + "\t" + d(va.distanceToSqr(vb)));
            O.println("DISTTO\t" + in + "\t" + d(va.distanceTo(vb)));
            O.println("PROJ\t" + in + "\t" + v3(va.projectedOn(vb)));
            O.println("LERP\t" + in + "\t" + d(0.35) + "\t" + v3(va.lerp(vb, 0.35)));
        }
        for (float rx : ANGLES) for (float ry : ANGLES) {
            O.println("DIRFROMROT\t" + f(rx) + "\t" + f(ry) + "\t" + v3(Vec3.directionFromRotation(rx, ry)));
            for (double[] dd : new double[][]{ {1,0,0}, {0.5,1,-0.5} })
                O.println("APPLYLOCAL\t" + f(rx) + "\t" + f(ry) + "\t" + d(dd[0])+"\t"+d(dd[1])+"\t"+d(dd[2]) + "\t" + v3(Vec3.applyLocalCoordinatesToRotation(new Vec2(rx, ry), new Vec3(dd[0],dd[1],dd[2]))));
        }
    }
}
