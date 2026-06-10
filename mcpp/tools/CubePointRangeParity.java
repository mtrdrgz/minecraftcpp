import net.minecraft.world.phys.shapes.CubePointRange;

// Ground truth for mcpp/src/world/phys/shapes/DoubleList.h (class mc::CubePointRange).
// Emits tab-separated rows from the REAL net.minecraft.world.phys.shapes.CubePointRange,
// which extends it.unimi.dsi.fastutil.doubles.AbstractDoubleList.
//
// CubePointRange.java (Minecraft 26.1.2):
//   ctor(int parts): throws IllegalArgumentException("Need at least 1 part") if parts <= 0.
//   getDouble(int index) = (double)index / parts
//   size()              = parts + 1
//
// CubePointRange and its methods are public, so no reflection is needed.
//
// Row formats (TAG \t inputs... \t outputs...), doubles as %016x of raw long bits:
//   CTOR      parts | constructible(0/1)
//   SIZE      parts | size(int, decimal)
//   GET       parts index | getDouble(double, %016x raw bits)
public class CubePointRangeParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    // Try to build a CubePointRange; returns null if the ctor throws (parts <= 0).
    static CubePointRange tryMake(int parts) {
        try {
            return new CubePointRange(parts);
        } catch (IllegalArgumentException e) {
            return null;
        }
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Constructor invariant: parts <= 0 throws; parts >= 1 succeeds.
        // Representative finite ints around the throw boundary and typical voxel sizes.
        int[] ctorParts = {
            Integer.MIN_VALUE, -1000000, -256, -16, -2, -1, 0,
            1, 2, 3, 4, 5, 8, 15, 16, 32, 64, 100, 128, 255, 256, 1000, 1000000,
            Integer.MAX_VALUE
        };
        for (int parts : ctorParts) {
            CubePointRange r = tryMake(parts);
            O.println("CTOR\t" + parts + "\t" + (r != null ? 1 : 0));
        }

        // Valid part counts: cover the voxel-shape range (1..16 are the common block grid
        // resolutions) plus the lcm grids DiscreteCubeMerger produces (up to 256) and a few
        // larger values. CubePointRange(parts) requires parts >= 1.
        int[] validParts = {
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
            20, 24, 30, 32, 40, 48, 60, 64, 96, 100, 120, 128, 200, 240, 256,
            512, 1000, 1024, 65536, 1000000, Integer.MAX_VALUE
        };

        for (int parts : validParts) {
            CubePointRange r = new CubePointRange(parts);

            // size() = parts + 1 (two's-complement wrap at Integer.MAX_VALUE).
            O.println("SIZE\t" + parts + "\t" + r.size());

            // getDouble(index) for every valid index 0..parts when parts is small, plus a
            // representative spread for large parts so the TSV stays finite.
            if (parts <= 256) {
                for (int index = 0; index <= parts; index++) {
                    O.println("GET\t" + parts + "\t" + index + "\t" + d(r.getDouble(index)));
                }
            } else {
                // Endpoints, midpoints, and a handful of off-grid indices for large parts.
                int[] idx = {
                    0, 1, 2, 3, parts / 4, parts / 3, parts / 2,
                    (2 * parts) / 3, (3 * parts) / 4, parts - 2, parts - 1, parts
                };
                for (int index : idx) {
                    O.println("GET\t" + parts + "\t" + index + "\t" + d(r.getDouble(index)));
                }
            }
        }
    }
}
