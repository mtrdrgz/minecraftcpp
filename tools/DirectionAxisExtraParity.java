import net.minecraft.core.Direction;
import java.util.Iterator;

// Ground-truth emitter for the still-unported "extra" surface of
// net.minecraft.core.Direction (Minecraft 26.1.2). Emits tab-separated rows
// consumed by DirectionAxisExtraParityTest.cpp.
//
// Direction ordinals (Direction.java:33-38): DOWN=0,UP=1,NORTH=2,SOUTH=3,WEST=4,EAST=5.
// Direction.Axis ordinals (Direction.java:402-480): X=0,Y=1,Z=2.
// Direction.AxisDirection ordinals (Direction.java:546-548): POSITIVE=0,NEGATIVE=1.
// Direction.Plane ordinals (Direction.java:576-578): HORIZONTAL=0,VERTICAL=1.
// All ints/booleans as decimal; doubles as raw long bits via Double.doubleToRawLongBits.
//
// TAGS:
//   ICHOOSE  <axisOrd> <x> <y> <z> <Axis.choose(int)>
//   DCHOOSE  <axisOrd> <xbits> <ybits> <zbits> <Axis.choose(double) bits>
//   AXFLAG   <axisOrd> <isHorizontal 0/1> <isVertical 0/1> <getPositive ord> <getNegative ord> <getPlane ord>
//   AXDIR    <dirOrd> <getAxis ord> <getAxisDirection ord>
//   PLANE    <planeOrd> <length> <axisCount>
//   PFACE    <planeOrd> <index> <face ord>     (faces in iterator() order)
//   PAXIS    <planeOrd> <index> <axis ord>
//   PTEST    <planeOrd> <dirOrd> <test 0/1>
//   NEAREST  <x> <y> <z> <orElseOrd|-1> <result ord|-1>
public class DirectionAxisExtraParity {
    static final java.io.PrintStream O = System.out;

    static int ord(Direction d) { return d.ordinal(); }
    static int ord(Direction.Axis a) { return a.ordinal(); }
    static int ord(Direction.AxisDirection a) { return a.ordinal(); }
    static int ord(Direction.Plane p) { return p.ordinal(); }

    static final Direction[] ALL = Direction.values(); // DOWN..EAST (0..5)
    static final Direction.Axis[] AXES = Direction.Axis.values(); // X,Y,Z (0..2)
    static final Direction.Plane[] PLANES = Direction.Plane.values(); // HORIZONTAL,VERTICAL

    public static void main(String[] args) throws Exception {
        // Plain enum; bootstrap defensively in case classloading touches registries.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — Direction is a plain enum and does not require it
        }

        // ---- Axis.choose(int x,y,z) — thorough int battery incl. negatives/edges ----
        int[] iv = {
            Integer.MIN_VALUE, Integer.MIN_VALUE + 1, -2000000001, -1000000, -65536,
            -1024, -128, -7, -1, 0, 1, 7, 128, 1024, 65536, 1000000, 2000000001,
            Integer.MAX_VALUE - 1, Integer.MAX_VALUE
        };
        // Use a handful of distinct (x,y,z) triples so each axis pick is observable.
        int[][] itrip = {
            {-7, 13, 1000001}, {Integer.MIN_VALUE, 0, Integer.MAX_VALUE},
            {1, -1, 0}, {65536, -65536, 12345}, {0, 0, 0},
            {Integer.MAX_VALUE, Integer.MIN_VALUE, -1}
        };
        for (Direction.Axis a : AXES) {
            for (int[] t : itrip) {
                O.println("ICHOOSE\t" + ord(a) + "\t" + t[0] + "\t" + t[1] + "\t" + t[2]
                        + "\t" + a.choose(t[0], t[1], t[2]));
            }
            // also single-value triples to exercise every battery value on the chosen slot
            for (int v : iv) {
                O.println("ICHOOSE\t" + ord(a) + "\t" + v + "\t" + (v + 1) + "\t" + (v - 1)
                        + "\t" + a.choose(v, v + 1, v - 1));
            }
        }

        // ---- Axis.choose(double x,y,z) — finite/physical doubles only ----
        double[][] dtrip = {
            {-3.5, 2.25, 0.0}, {1234.5678, -9.0e9, 1e-9},
            {0.0, -0.0, 1.0}, {3.14159265358979, 2.718281828, 1.41421356},
            {-1e308, 1e308, 123456.789}, {0.1, 0.2, 0.3}
        };
        for (Direction.Axis a : AXES) {
            for (double[] t : dtrip) {
                double r = a.choose(t[0], t[1], t[2]);
                O.println("DCHOOSE\t" + ord(a)
                        + "\t" + hx(t[0]) + "\t" + hx(t[1]) + "\t" + hx(t[2])
                        + "\t" + hx(r));
            }
        }

        // ---- Axis flags / getPositive / getNegative / getPlane ----
        for (Direction.Axis a : AXES) {
            O.println("AXFLAG\t" + ord(a)
                    + "\t" + (a.isHorizontal() ? 1 : 0)
                    + "\t" + (a.isVertical() ? 1 : 0)
                    + "\t" + ord(a.getPositive())
                    + "\t" + ord(a.getNegative())
                    + "\t" + ord(a.getPlane()));
        }

        // ---- per-Direction: getAxis / getAxisDirection ----
        for (Direction d : ALL) {
            O.println("AXDIR\t" + ord(d) + "\t" + ord(d.getAxis()) + "\t" + ord(d.getAxisDirection()));
        }

        // The Plane.axis[] field is private — reach it via reflection (its length is
        // the real ground truth for the axisCount, no fabricated helper).
        java.lang.reflect.Field axisField = Direction.Plane.class.getDeclaredField("axis");
        axisField.setAccessible(true);

        // ---- Plane: length + axis count ----
        for (Direction.Plane p : PLANES) {
            Direction.Axis[] axisArr = (Direction.Axis[]) axisField.get(p);
            O.println("PLANE\t" + ord(p) + "\t" + p.length() + "\t" + axisArr.length);
        }

        // ---- Plane faces via iterator() (declaration order) ----
        for (Direction.Plane p : PLANES) {
            int idx = 0;
            for (Iterator<Direction> it = p.iterator(); it.hasNext(); ) {
                Direction f = it.next();
                O.println("PFACE\t" + ord(p) + "\t" + idx + "\t" + ord(f));
                idx++;
            }
        }

        // ---- Plane axes (via reflection on the private axis[] field) ----
        for (Direction.Plane p : PLANES) {
            Direction.Axis[] axisArr = (Direction.Axis[]) axisField.get(p);
            for (int i = 0; i < axisArr.length; i++) {
                O.println("PAXIS\t" + ord(p) + "\t" + i + "\t" + ord(axisArr[i]));
            }
        }

        // ---- Plane.test over all 6 directions for both planes ----
        for (Direction.Plane p : PLANES) {
            for (Direction d : ALL) {
                O.println("PTEST\t" + ord(p) + "\t" + ord(d) + "\t" + (p.test(d) ? 1 : 0));
            }
        }

        // ---- Direction.getNearest(int x,y,z, orElse) — finite int battery ----
        int[] nv = {
            Integer.MIN_VALUE, Integer.MIN_VALUE + 1, -1000000, -100, -16, -3, -2, -1, 0,
            1, 2, 3, 16, 100, 1000000, Integer.MAX_VALUE - 1, Integer.MAX_VALUE
        };
        // orElse encodings to test the null/non-null branches.
        Direction[] orElses = { null, Direction.DOWN, Direction.UP, Direction.NORTH,
                                Direction.SOUTH, Direction.WEST, Direction.EAST };
        // Exercise a representative cross-product (kept finite): each component over nv,
        // with a few orElse values, focusing on tie / dominance cases.
        int[][] ntrip = {
            {0,0,0}, {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1},
            {5,3,1}, {1,3,5}, {3,5,1}, {2,2,2}, {2,2,1}, {2,1,2}, {1,2,2},
            {-5,3,-1}, {-1,-3,-5}, {-3,5,1}, {Integer.MIN_VALUE, 0, 1},
            {Integer.MAX_VALUE, Integer.MIN_VALUE, 0}, {7,7,8}, {8,7,7}, {7,8,7},
            {Integer.MIN_VALUE, Integer.MIN_VALUE, Integer.MAX_VALUE}
        };
        for (int[] t : ntrip) {
            for (Direction oe : orElses) {
                int oeOrd = (oe == null) ? -1 : ord(oe);
                Direction r = Direction.getNearest(t[0], t[1], t[2], oe);
                int rOrd = (r == null) ? -1 : ord(r);
                O.println("NEAREST\t" + t[0] + "\t" + t[1] + "\t" + t[2] + "\t" + oeOrd + "\t" + rOrd);
            }
        }
        // Also a denser single-axis sweep with null orElse to catch abs/edge behaviour.
        for (int x : nv) {
            Direction r = Direction.getNearest(x, 0, 0, null);
            O.println("NEAREST\t" + x + "\t0\t0\t-1\t" + ((r == null) ? -1 : ord(r)));
        }
        for (int y : nv) {
            Direction r = Direction.getNearest(0, y, 0, Direction.NORTH);
            O.println("NEAREST\t0\t" + y + "\t0\t" + ord(Direction.NORTH) + "\t" + ((r == null) ? -1 : ord(r)));
        }
        for (int z : nv) {
            Direction r = Direction.getNearest(0, 0, z, null);
            O.println("NEAREST\t0\t0\t" + z + "\t-1\t" + ((r == null) ? -1 : ord(r)));
        }
    }

    static String hx(double d) { return String.format("%016x", Double.doubleToRawLongBits(d)); }
}
