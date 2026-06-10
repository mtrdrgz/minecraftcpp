// Ground-truth generator for net.minecraft.core.AxisCycle (Minecraft 26.1.2).
// Calls the REAL net.minecraft.core.AxisCycle enum (NONE/FORWARD/BACKWARD) and
// the REAL net.minecraft.core.Direction.Axis (X/Y/Z) and emits, as tab-separated
// rows to STDOUT (the runner captures stdout into axis_cycle.tsv):
//
//   ORD       <cycleName>                         ordinal
//   AXORD     <axisName>                           ordinal
//   BETWEEN   <fromOrdinal> <toOrdinal>            betweenOrdinal
//   CYCLEI    <cycleOrdinal> <x> <y> <z> <axisOrd> resultInt
//   CYCLED    <cycleOrdinal> <xBits> <yBits> <zBits> <axisOrd>  resultDoubleBits
//   CYCLEAX   <cycleOrdinal> <axisOrd>             resultAxisOrdinal
//   INVERSE   <cycleOrdinal>                       inverseOrdinal
//
// AxisCycle / Direction.Axis are pure (no registry); the bootstrap guard is
// purely defensive in case class init touches anything.
//
//   tools/run_groundtruth.ps1 -Tool AxisCycleParity -Out mcpp/build/axis_cycle.tsv

import net.minecraft.core.AxisCycle;
import net.minecraft.core.Direction;

public class AxisCycleParity {
    static final java.io.PrintStream O = System.out;

    static String db(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    // Reflectively read AxisCycle.VALUES (public static final AxisCycle[]).
    static AxisCycle[] cycleValues() throws Exception {
        java.lang.reflect.Field f = AxisCycle.class.getField("VALUES");
        f.setAccessible(true);
        return (AxisCycle[]) f.get(null);
    }

    // Reflectively read Direction.Axis.VALUES (public static final Direction.Axis[]).
    static Direction.Axis[] axisValues() throws Exception {
        java.lang.reflect.Field f = Direction.Axis.class.getField("VALUES");
        f.setAccessible(true);
        return (Direction.Axis[]) f.get(null);
    }

    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // AxisCycle / Direction.Axis do not need the bootstrap; ignore.
        }

        AxisCycle[] CYCLES = cycleValues();          // NONE, FORWARD, BACKWARD (ordinals 0,1,2)
        Direction.Axis[] AXES = axisValues();        // X, Y, Z (ordinals 0,1,2)

        // (1) Enum ordinals — lock the ordering the TSV exchanges depend on.
        for (AxisCycle c : CYCLES) {
            O.println("ORD\t" + c.name() + "\t" + c.ordinal());
        }
        for (Direction.Axis a : AXES) {
            O.println("AXORD\t" + a.name() + "\t" + a.ordinal());
        }

        // (2) between(from, to) — every (from, to) pair over the three axes.
        for (Direction.Axis from : AXES) {
            for (Direction.Axis to : AXES) {
                AxisCycle r = AxisCycle.between(from, to);
                O.println("BETWEEN\t" + from.ordinal() + "\t" + to.ordinal() + "\t" + r.ordinal());
            }
        }

        // (3) inverse() and cycle(Axis) — per cycle constant.
        for (AxisCycle c : CYCLES) {
            O.println("INVERSE\t" + c.ordinal() + "\t" + c.inverse().ordinal());
            for (Direction.Axis a : AXES) {
                O.println("CYCLEAX\t" + c.ordinal() + "\t" + a.ordinal() + "\t" + c.cycle(a).ordinal());
            }
        }

        // (4) cycle(int x, y, z, axis) and cycle(double x, y, z, axis).
        // Finite, physical coordinate triples: small distinguishable ints plus a
        // few block-scale magnitudes (positive and negative). The whole point of
        // a permutation is which of {x,y,z} lands where, so distinct values per
        // slot exercise every routing; magnitudes add wrap/identity coverage.
        int[] XS = { 0, 1, -1, 2, 7, -7, 13, -13, 100, -100, 1000, -1000, 32767, -32768, 123456, -123456 };
        int[] YS = { 0, 2, -2, 3, 11, -11, 17, -17, 200, -200, 2000, -2000, 16384, -16384, 654321, -654321 };
        int[] ZS = { 0, 3, -3, 5, 19, -19, 23, -23, 300, -300, 3000, -3000, 8192, -8192, 246810, -246810 };

        for (AxisCycle c : CYCLES) {
            for (Direction.Axis axis : AXES) {
                for (int i = 0; i < XS.length; i++) {
                    int x = XS[i], y = YS[i], z = ZS[i];
                    int ri = c.cycle(x, y, z, axis);
                    O.println("CYCLEI\t" + c.ordinal() + "\t" + x + "\t" + y + "\t" + z
                              + "\t" + axis.ordinal() + "\t" + ri);

                    // Double overload: feed the same magnitudes plus a fractional
                    // tweak so the int and double paths are independently checked.
                    double dx = x + 0.5, dy = y + 0.25, dz = z + 0.125;
                    double rd = c.cycle(dx, dy, dz, axis);
                    O.println("CYCLED\t" + c.ordinal() + "\t" + db(dx) + "\t" + db(dy) + "\t" + db(dz)
                              + "\t" + axis.ordinal() + "\t" + db(rd));
                }
            }
        }
    }
}
