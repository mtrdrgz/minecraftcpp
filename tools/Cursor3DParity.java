// Ground-truth generator for net.minecraft.core.Cursor3D (Minecraft 26.1.2)
// using the REAL decompiled class. Cursor3D is a pure 3D box iterator with a
// public constructor and public advance()/nextX()/nextY()/nextZ()/getNextType()
// — no reflection, no Bootstrap, no registries needed.
//
//   tools/run_groundtruth.ps1 -Tool Cursor3DParity -Out mcpp/build/cursor3d.tsv
//
// For each box (minX,minY,minZ,maxX,maxY,maxZ) we drive the iterator to
// completion. One STEP row per successful advance() captures the step ordinal,
// the (x,y,z) world coords advance() yields, and getNextType(). After the walk
// we emit one END row recording how many steps the iterator produced (this pins
// the end == width*height*depth count, including the empty-box / inverted-box
// degenerate cases). All values are integers in decimal.

import net.minecraft.core.Cursor3D;

public class Cursor3DParity {
    static final java.io.PrintStream O = System.out;

    // Box bounds chosen so width/height/depth span 1 (single cell, every coord a
    // boundary) up to several cells, with non-zero / negative origins and uneven
    // extents. Also includes degenerate boxes where exactly ONE axis is inverted
    // by one (width OR height OR depth == 0) so end == 0 and advance() yields
    // nothing on the first call (index==end==0 terminates immediately, matching
    // Java exactly).
    //
    // NONPHYSICAL boxes are intentionally EXCLUDED: any box whose extents multiply
    // to a NEGATIVE end (e.g. {0,0,0,0,-2,0} -> 1*-1*1 = -1, or all-axes-inverted
    // {3,3,3,1,1,1} -> (-1)^3 = -1). Cursor3D.advance() terminates only on the
    // exact equality index==end (Cursor3D.java:31); a negative end is never hit
    // until ~4.29e9 steps of 32-bit int wraparound, so the iterator does NOT stop
    // on the first call. Both this GT tool and the C++ test cap their walks at
    // 100000, producing a fictional, non-comparable END count for such inputs.
    // These boxes cannot occur in real gameplay: every Cursor3D construction site
    // guarantees max>=min on every axis -> width,height,depth>=1, end>=1
    // (BlockCollisions.java:55-61, SectionPos.java:248-250, ClientLevel.java:886).
    static final int[][] BOXES = {
        // {minX,minY,minZ, maxX,maxY,maxZ}
        {0, 0, 0, 0, 0, 0},        // 1x1x1 single cell -> corner
        {0, 0, 0, 1, 0, 0},        // 2x1x1
        {0, 0, 0, 1, 1, 1},        // 2x2x2 all corners
        {0, 0, 0, 2, 2, 2},        // 3x3x3 has an interior cell
        {0, 0, 0, 4, 3, 2},        // 5x4x3 uneven
        {-1, -1, -1, 1, 1, 1},     // 3x3x3 negative origin
        {-2, 0, 3, 0, 2, 5},       // 3x3x3 mixed origin
        {5, 7, 9, 9, 7, 11},       // height 1 (flat slab in y)
        {0, 0, 0, 3, 0, 3},        // 4x1x4 flat (every y a boundary)
        {10, 20, 30, 10, 20, 30},  // single cell at offset
        {-3, -4, -5, 2, 1, 0},     // 6x6x6 fully negative-spanning
        {0, 0, 0, 5, 5, 5},        // 6x6x6
        {0, 0, 0, 0, 4, 0},        // 1x5x1 column in y
        {0, 0, 0, 0, 0, 4},        // 1x1x5 column in z
        {7, 7, 7, 6, 7, 7},        // inverted in x by 1: width=0 -> end=0, terminates immediately
        {0, 0, 0, -1, 0, 0},       // inverted in x by 1: width=0 -> end=0, terminates immediately
        // NONPHYSICAL (removed): negative end never reached until int wraparound.
        //   {0, 0, 0, 0, -2, 0}  -> w=1,h=-1,d=1 -> end=-1 (~4.29e9 steps in real Java)
        //   {3, 3, 3, 1, 1, 1}   -> all axes inverted -> (-1)^3 = -1
        // See header comment; max>=min always holds at real call sites so end>=1.
    };

    public static void main(String[] args) throws Exception {
        for (int[] b : BOXES) {
            int minX = b[0], minY = b[1], minZ = b[2];
            int maxX = b[3], maxY = b[4], maxZ = b[5];

            Cursor3D c = new Cursor3D(minX, minY, minZ, maxX, maxY, maxZ);
            int step = 0;
            // Bound the loop so an unexpectedly huge/negative end can't run away.
            while (step < 100000 && c.advance()) {
                O.println("STEP\t" + minX + "\t" + minY + "\t" + minZ
                    + "\t" + maxX + "\t" + maxY + "\t" + maxZ
                    + "\t" + step
                    + "\t" + c.nextX() + "\t" + c.nextY() + "\t" + c.nextZ()
                    + "\t" + c.getNextType());
                step++;
            }

            O.println("END\t" + minX + "\t" + minY + "\t" + minZ
                + "\t" + maxX + "\t" + maxY + "\t" + maxZ
                + "\t" + step);
        }
    }
}
