import java.util.OptionalInt;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.block.SelectableSlotContainer;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;

// Ground-truth emitter for the pure slot-picking geometry of
//   net.minecraft.world.level.block.SelectableSlotContainer (Minecraft 26.1.2):
//     default OptionalInt getHitSlot(BlockHitResult, Direction)         :17-23
//     private static Optional<Vec2> getRelativeHitCoordinatesForBlockFace :25-44
//     private static int getSection(float, int)                          :46-50
//
// It drives the REAL interface bytecode. getHitSlot is a `default` method that
// needs an instance supplying getRows()/getColumns(); we supply ONLY those two
// trivial accessors via a local class — every line of geometry (the relative-hit
// face math, getSection float-division, the row/column section ladder, and the
// empty-on-wrong-face / empty-on-DOWN/UP cases) executes inside the REAL
// interface default + private-static methods, NOT a re-implementation. Driving
// getHitSlot transitively exercises getRelativeHitCoordinatesForBlockFace and
// getSection, so all three methods are covered in one shot.
//
// Direction ordinals (Direction.java:33-38): DOWN=0,UP=1,NORTH=2,SOUTH=3,WEST=4,EAST=5.
// Doubles emitted as raw long bits via Double.doubleToRawLongBits; ints decimal.
//
// TAG:
//   SLOT  <hitDirOrd> <bx> <by> <bz> <locXbits> <locYbits> <locZbits>
//         <blockFacingOrd> <rows> <columns> <present(0|1)> <slot>
public class SelectableSlotContainerParity {
    static final java.io.PrintStream O = System.out;

    static String hx(double d) { return String.format("%016x", Double.doubleToRawLongBits(d)); }

    // Minimal concrete implementor: supplies ONLY rows/columns. All slot math is
    // the REAL default/private-static interface code.
    static final class Grid implements SelectableSlotContainer {
        final int rows;
        final int columns;
        Grid(int rows, int columns) { this.rows = rows; this.columns = columns; }
        @Override public int getRows() { return rows; }
        @Override public int getColumns() { return columns; }
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Direction[] dirs = Direction.values();

        // Grid sizes: the vanilla chiseled bookshelf (2x3) plus assorted shapes so
        // the row/column getSection ladders (sectionSize = 16/maxSections) and the
        // column + row*columns packing are exercised across divisors that do and
        // don't divide 16 evenly.
        int[][] grids = {
            {2, 3}, {1, 1}, {1, 2}, {3, 3}, {4, 4}, {2, 2},
            {1, 16}, {16, 1}, {3, 5}, {5, 3}, {2, 7}, {1, 3}
        };

        // Block positions: include origin, negatives, and large magnitudes so the
        // integer BlockPos.relative(dir) step and the double subtraction
        // (location - hitBlockPos) are exercised away from 0 (the relativeX/Y/Z
        // recovery must cancel the block coordinate exactly).
        int[][] blockPositions = {
            {0, 0, 0}, {1, 2, 3}, {-1, -2, -3}, {5, -7, 11},
            {100, 64, -100}, {-30000000, 0, 30000000}, {7, 7, 7}
        };

        // Hit-location offsets WITHIN/around the struck face. The interesting
        // coordinate runs across the [0,1] face and slightly outside it; held
        // coordinates pin the other axes. We sweep the in-face offset densely so
        // every section boundary (k/columns, k/rows) and the floor/clamp edges are
        // straddled, including just-below/just-above each boundary and out-of-face
        // values (<0, >1) that clamp to slot 0 / last.
        double[] offsets = {
            0.0, 1.0, 0.5,
            0.0625, 0.125, 0.1875, 0.25, 0.3125, 0.375, 0.4375,
            0.5625, 0.625, 0.6875, 0.75, 0.8125, 0.875, 0.9375,
            // thirds / fifths / sevenths — section boundaries for 3,5,7 columns
            0.3333333, 0.6666667, 0.2, 0.4, 0.6, 0.8,
            0.142857, 0.285714, 0.428571, 0.571429, 0.714286, 0.857143,
            // just inside / just outside each side of a few boundaries
            0.33333331, 0.33333334, 0.49999997, 0.50000003,
            0.99999994, 1.00000006, -0.00000006, 0.00000006,
            // out-of-face (clamp) and odd in-betweens
            -0.25, 1.25, 0.03125, 0.96875, 0.123456, 0.789012,
            0.4999999, 0.5000001, 0.0001, 0.9999
        };

        // Held value for the non-swept in-face axis.
        double[] holds = { 0.5, 0.25, 0.0, 0.999 };

        for (int[] g : grids) {
            Grid grid = new Grid(g[0], g[1]);
            for (int[] bp : blockPositions) {
                int bx = bp[0], by = bp[1], bz = bp[2];
                for (Direction hitDir : dirs) {
                    // Drive matching facing (the in-face path), plus a couple of
                    // mismatched facings so the "blockFacing != hitDirection ->
                    // empty" short-circuit is covered too.
                    Direction[] facings = { hitDir, dirs[(hitDir.ordinal() + 1) % 6], dirs[(hitDir.ordinal() + 3) % 6] };
                    for (Direction facing : facings) {
                        // The two in-face axes depend on the face. We build the
                        // precise hit Vec3 from the block pos + the +1 step along
                        // the hit face (the face plane) + the in-face offsets, but
                        // the helper itself recovers the relative coords, so we can
                        // simply feed location = blockPos + step + (offsetU, v...)
                        // mapped onto the correct two in-face axes.
                        for (double u : offsets) {
                            for (double held : holds) {
                                emit(grid, hitDir, bx, by, bz, facing, u, held);
                            }
                        }
                    }
                }
            }
        }
    }

    // Build the precise hit Vec3 so that, on the struck face, the two in-face
    // coordinates carry (u, held). The "depth" axis (along the face normal) is set
    // to the face plane (block coord + the outward step), which is what a real ray
    // hit produces; the helper ignores depth precision beyond the relative
    // subtraction, so any plausible plane value works. Both U and HELD are also
    // swapped so each in-face axis gets the full offset sweep.
    static void emit(Grid grid, Direction hitDir, int bx, int by, int bz, Direction facing,
                     double u, double held) throws Exception {
        // For the struck face we place the location at blockPos + the face's
        // outward unit step on the depth axis, and u/held on the two in-face axes.
        // The exact mapping per face mirrors getRelativeHitCoordinatesForBlockFace's
        // axis usage (NORTH/SOUTH use X,Y; WEST/EAST use Z,Y; DOWN/UP unused -> the
        // helper returns empty regardless, but we still feed real coords).
        double locX, locY, locZ;
        switch (hitDir) {
            case NORTH: // depth = Z (step -1), in-face = X,Y
                locX = bx + u; locY = by + held; locZ = bz + 0.0; break;
            case SOUTH: // depth = Z (step +1), in-face = X,Y
                locX = bx + u; locY = by + held; locZ = bz + 1.0; break;
            case WEST:  // depth = X (step -1), in-face = Z,Y
                locX = bx + 0.0; locY = by + held; locZ = bz + u; break;
            case EAST:  // depth = X (step +1), in-face = Z,Y
                locX = bx + 1.0; locY = by + held; locZ = bz + u; break;
            case DOWN:  // depth = Y (step -1); helper returns empty for DOWN/UP
                locX = bx + u; locY = by + 0.0; locZ = bz + held; break;
            case UP:
            default:    // depth = Y (step +1)
                locX = bx + u; locY = by + 1.0; locZ = bz + held; break;
        }

        BlockHitResult hit = new BlockHitResult(new Vec3(locX, locY, locZ), hitDir, new BlockPos(bx, by, bz), false);
        OptionalInt slot = grid.getHitSlot(hit, facing);
        int present = slot.isPresent() ? 1 : 0;
        int value = slot.isPresent() ? slot.getAsInt() : 0;
        O.println("SLOT\t" + hitDir.ordinal() + "\t" + bx + "\t" + by + "\t" + bz
            + "\t" + hx(locX) + "\t" + hx(locY) + "\t" + hx(locZ)
            + "\t" + facing.ordinal() + "\t" + grid.rows + "\t" + grid.columns
            + "\t" + present + "\t" + value);
    }
}
