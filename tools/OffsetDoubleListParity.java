import it.unimi.dsi.fastutil.doubles.DoubleArrayList;
import it.unimi.dsi.fastutil.doubles.DoubleList;
import net.minecraft.world.phys.shapes.CubePointRange;
import net.minecraft.world.phys.shapes.OffsetDoubleList;

// Ground truth for mcpp/src/world/phys/shapes/DoubleList.h (class mc::OffsetDoubleList).
// Emits tab-separated rows from the REAL net.minecraft.world.phys.shapes.OffsetDoubleList,
// which extends it.unimi.dsi.fastutil.doubles.AbstractDoubleList.
//
// OffsetDoubleList.java (Minecraft 26.1.2):
//   ctor(DoubleList delegate, double offset): stores both verbatim.
//   getDouble(int index) = delegate.getDouble(index) + offset
//   size()              = delegate.size()
//
// OffsetDoubleList and its methods are public, so no reflection is needed.
// Delegate lists exercised here:
//   * fastutil DoubleArrayList  — arbitrary array-backed sample values.
//   * net.minecraft CubePointRange — (double)i/parts grid (real VoxelShape delegate).
//   * nested OffsetDoubleList    — offset-of-offset composition.
//
// Row formats (TAG \t inputs... \t outputs...), doubles as %016x of raw long bits:
//   ARR_SIZE   listId offsetBits           | size(int, decimal)
//   ARR_GET    listId offsetBits index      | getDouble(double, %016x raw bits)
//   CPR_SIZE   parts  offsetBits            | size(int, decimal)
//   CPR_GET    parts  offsetBits index       | getDouble(double, %016x raw bits)
//   NEST_GET   parts  o1Bits o2Bits index    | getDouble(double, %016x raw bits)
public class OffsetDoubleListParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }
    static long b(double v) { return Double.doubleToRawLongBits(v); }

    // Array-backed delegate sample sets. Index in this array == listId.
    static final double[][] ARRAYS = {
        { 0.0 },
        { 0.0, 1.0 },
        { 0.0, 0.5, 1.0 },
        { 0.0, 0.0625, 0.125, 0.1875, 0.25, 0.3125, 0.375, 0.4375, 0.5,
          0.5625, 0.625, 0.6875, 0.75, 0.8125, 0.875, 0.9375, 1.0 },     // 16ths grid
        { -1.0, -0.5, -0.25, 0.0, 0.25, 0.5, 1.0, 2.0, 3.0 },
        { 0.1, 0.2, 0.3, 0.7, 0.9, 1.3, 2.7, 3.14159265358979 },
        { 100.0, 200.5, 300.25, 1000.0, 12345.6789 },
        { -1024.0, -512.0, -1.0, 0.0, 1.0, 512.0, 1024.0, 1e6, 1e9 },
    };

    // Offsets to apply (verbatim doubles; getDouble adds exactly these via plain +).
    static final double[] OFFSETS = {
        0.0, 1.0, -1.0, 0.5, -0.5, 0.25, -0.25, 0.0625, -0.0625,
        2.0, -2.0, 16.0, -16.0, 0.1, -0.1, 0.3333333333333333, -0.3333333333333333,
        3.14159265358979, -2.718281828459045, 1000.0, -1000.0, 1e6, -1e6, 1e-7,
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // --- Array-backed delegate ---
        for (int listId = 0; listId < ARRAYS.length; listId++) {
            double[] base = ARRAYS[listId];
            for (double offset : OFFSETS) {
                DoubleList delegate = new DoubleArrayList(base);
                OffsetDoubleList off = new OffsetDoubleList(delegate, offset);
                O.println("ARR_SIZE\t" + listId + "\t" + b(offset) + "\t" + off.size());
                for (int index = 0; index < base.length; index++) {
                    O.println("ARR_GET\t" + listId + "\t" + b(offset) + "\t" + index
                              + "\t" + d(off.getDouble(index)));
                }
            }
        }

        // --- CubePointRange delegate (the real VoxelShape coordinate grid) ---
        int[] cprParts = { 1, 2, 3, 4, 5, 8, 16, 32, 64, 100, 128, 256 };
        for (int parts : cprParts) {
            for (double offset : OFFSETS) {
                CubePointRange delegate = new CubePointRange(parts);
                OffsetDoubleList off = new OffsetDoubleList(delegate, offset);
                O.println("CPR_SIZE\t" + parts + "\t" + b(offset) + "\t" + off.size());
                for (int index = 0; index <= parts; index++) {
                    O.println("CPR_GET\t" + parts + "\t" + b(offset) + "\t" + index
                              + "\t" + d(off.getDouble(index)));
                }
            }
        }

        // --- Nested OffsetDoubleList (offset-of-offset over a CubePointRange) ---
        int[] nestParts = { 1, 2, 4, 8, 16 };
        double[] nestO1 = { 0.0, 0.5, 1.0, -0.25, 16.0 };
        double[] nestO2 = { 0.0, -0.5, 0.125, 2.0, -1000.0 };
        for (int parts : nestParts) {
            for (double o1 : nestO1) {
                for (double o2 : nestO2) {
                    CubePointRange base = new CubePointRange(parts);
                    OffsetDoubleList inner = new OffsetDoubleList(base, o1);
                    OffsetDoubleList outer = new OffsetDoubleList(inner, o2);
                    for (int index = 0; index <= parts; index++) {
                        O.println("NEST_GET\t" + parts + "\t" + b(o1) + "\t" + b(o2)
                                  + "\t" + index + "\t" + d(outer.getDouble(index)));
                    }
                }
            }
        }
    }
}
