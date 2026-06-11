// Ground-truth generator for the PURE child-piece foot-position + facing geometry
// of the REAL decompiled 26.1.2 class
//   net.minecraft.world.level.levelgen.structure.structures.NetherFortressPieces
//       .NetherBridgePiece.generateChildForward(...)
//       .NetherBridgePiece.generateChildLeft(...)
//       .NetherBridgePiece.generateChildRight(...)
//
// These three protected base methods map the PARENT piece's (boundingBox,
// orientation) plus per-call (xOff/yOff/zOff) into the CHILD foot position
// (footX,footY,footZ) and the CHILD facing Direction, then forward them down to
// the chosen child's createPiece(...) which builds the child box via
// BoundingBox.orientBox(footX,footY,footZ, off.., dim.., childDir).
//
// To observe that pure mapping byte-exactly we drive the REAL methods but force
// the chosen child to ALWAYS be the RNG-free BridgeEndFiller:
//   * We build a real BridgeCrossing as the parent (public ctor takes
//     (genDepth, BoundingBox, Direction)).
//   * We build a real StartPiece (public ctor) and then EMPTY its
//     availableBridgePieces + availableCastlePieces lists by reflection. With the
//     piece-weight list empty, NetherBridgePiece.generatePiece computes
//     totalWeight == -1, never enters the weighted-selection loop (so it draws NO
//     RandomSource values), and falls through to
//     BridgeEndFiller.createPiece(...). That createPiece consumes no RNG before
//     building its box, so the whole call is deterministic.
//   * A stub StructurePieceAccessor (java.lang.reflect.Proxy) reports no
//     collisions and ignores addPiece.
// The returned piece is the BridgeEndFiller anchored at (footX,footY,footZ) with
// the child Direction; its box == orientBox(foot.., -1,-3,0, 5,10,8, childDir).
// BridgeEndFiller.createPiece returns null when isOkBox(box) (box.minY() > 10)
// fails, so a null return is the observable signal that the child foot's minY is
// too low — also reproduced on the C++ side.
//
// Output rows (tab-separated, tag first):
//   FWD <pMinX> <pMinY> <pMinZ> <pMaxX> <pMaxY> <pMaxZ> <pOrdDir> <xOff> <yOff>
//         <present 0|1> <cMinX> <cMinY> <cMinZ> <cMaxX> <cMaxY> <cMaxZ> <cOrdDir>
//   LFT <pMinX..pMaxZ> <pOrdDir> <yOff> <zOff> <present> <cMinX..cMaxZ> <cOrdDir>
//   RGT <pMinX..pMaxZ> <pOrdDir> <yOff> <zOff> <present> <cMinX..cMaxZ> <cOrdDir>
// present==0 means the REAL method returned null (child box not built / not okBox);
// in that case the six child-box ints and child-dir are emitted as 0 and ignored
// by the C++ test, which only checks present-ness in that case. dir ordinals use
// net.minecraft.core.Direction.ordinal(): DOWN0 UP1 NORTH2 SOUTH3 WEST4 EAST5.
//
//   tools/run_groundtruth.ps1 -Tool NetherFortressChildOffsetParity -Out mcpp/build/nether_fortress_child_offset.tsv

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.List;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.StructurePieceAccessor;

@SuppressWarnings({"deprecation", "unchecked"})
public class NetherFortressChildOffsetParity {
    static final java.io.PrintStream OUT = System.out;

    static final Direction[] HORIZ = { Direction.NORTH, Direction.SOUTH, Direction.WEST, Direction.EAST };

    static Class<?> bridgeCls;        // NetherFortressPieces$NetherBridgePiece (abstract)
    static Class<?> startCls;         // NetherFortressPieces$StartPiece
    static Class<?> crossingCls;      // NetherFortressPieces$BridgeCrossing (public, concrete parent)
    static Method genFwd, genLeft, genRight;
    static StructurePieceAccessor stubAcc;
    static RandomSource rng;

    public static void main(String[] args) throws Exception {
        // Constructing inverted BoundingBoxes (negative offsets) logs an ERROR via
        // log4j to the stdout FD and would pollute the TSV. Silence it first.
        org.apache.logging.log4j.core.config.Configurator.setRootLevel(org.apache.logging.log4j.Level.OFF);

        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        bridgeCls = Class.forName(
            "net.minecraft.world.level.levelgen.structure.structures.NetherFortressPieces$NetherBridgePiece");
        startCls = Class.forName(
            "net.minecraft.world.level.levelgen.structure.structures.NetherFortressPieces$StartPiece");
        crossingCls = Class.forName(
            "net.minecraft.world.level.levelgen.structure.structures.NetherFortressPieces$BridgeCrossing");

        genFwd = bridgeCls.getDeclaredMethod("generateChildForward",
            startCls, StructurePieceAccessor.class, RandomSource.class, int.class, int.class, boolean.class);
        genLeft = bridgeCls.getDeclaredMethod("generateChildLeft",
            startCls, StructurePieceAccessor.class, RandomSource.class, int.class, int.class, boolean.class);
        genRight = bridgeCls.getDeclaredMethod("generateChildRight",
            startCls, StructurePieceAccessor.class, RandomSource.class, int.class, int.class, boolean.class);
        genFwd.setAccessible(true);
        genLeft.setAccessible(true);
        genRight.setAccessible(true);

        stubAcc = stubAccessor();
        rng = RandomSource.create(987654321L);

        // Parent boxes: chosen so the child foot's minY straddles the isOkBox
        // (minY > 10) boundary on BridgeEndFiller (off = {-1,-3,0}, dim = {5,10,8}),
        // and so the box-construction exercises both low and high coordinates and
        // two's-complement wrap is not reached (finite, in-range positions).
        int[][] parentBoxes = {
            {   0,  64,   0,   4,  73,  18 },
            {   5,  48,  -3,   9,  57,  15 },
            { -16,  30,   9, -12,  39,  27 },
            {  16,  20,  16,  20,  29,  34 },
            {   0,  14,   0,   4,  23,  18 },  // child minY near 11 boundary
            {  -8,  13,   4,  -4,  22,  22 },  // boundary low side
            { 100,  90,-100, 104,  99, -82 },
            { -50,  16, -50, -46,  25, -32 },
        };
        // (xOff, yOff) pairs used by generateChildForward; the real callers pass
        // values like (8,3),(3,8),(1,3),(5,3),(2,0),(1,0),(6,2),(5,11).
        int[][] fwdOffsets = { {8,3}, {3,8}, {1,3}, {5,3}, {2,0}, {1,0}, {6,2}, {5,11}, {0,0} };
        // (yOff, zOff) pairs used by generateChildLeft / generateChildRight; real
        // callers pass (3,8),(0,zOff),(0,1),(0,2),(6,2).
        int[][] sideOffsets = { {3,8}, {0,2}, {0,1}, {6,2}, {0,7}, {2,0}, {0,0} };

        for (int[] pb : parentBoxes) {
            BoundingBox parentBox = new BoundingBox(pb[0], pb[1], pb[2], pb[3], pb[4], pb[5]);
            for (Direction pDir : HORIZ) {
                Object parent = newCrossing(parentBox, pDir);
                Object start = newEmptiedStart();

                for (int[] off : fwdOffsets) {
                    Object child = genFwd.invoke(parent, start, stubAcc, rng, off[0], off[1], false);
                    emit("FWD", parentBox, pDir, off[0], off[1], child);
                }
                for (int[] off : sideOffsets) {
                    Object child = genLeft.invoke(parent, start, stubAcc, rng, off[0], off[1], false);
                    emit("LFT", parentBox, pDir, off[0], off[1], child);
                }
                for (int[] off : sideOffsets) {
                    Object child = genRight.invoke(parent, start, stubAcc, rng, off[0], off[1], false);
                    emit("RGT", parentBox, pDir, off[0], off[1], child);
                }
            }
        }
    }

    static void emit(String tag, BoundingBox p, Direction pDir, int a, int b, Object child) {
        StructurePiece sp = (StructurePiece) child;
        int present = sp == null ? 0 : 1;
        int cMinX = 0, cMinY = 0, cMinZ = 0, cMaxX = 0, cMaxY = 0, cMaxZ = 0, cDir = 0;
        if (sp != null) {
            BoundingBox c = sp.getBoundingBox();
            cMinX = c.minX(); cMinY = c.minY(); cMinZ = c.minZ();
            cMaxX = c.maxX(); cMaxY = c.maxY(); cMaxZ = c.maxZ();
            Direction o = sp.getOrientation();
            cDir = o == null ? -1 : o.ordinal();
        }
        OUT.println(tag
            + "\t" + p.minX() + "\t" + p.minY() + "\t" + p.minZ()
            + "\t" + p.maxX() + "\t" + p.maxY() + "\t" + p.maxZ()
            + "\t" + pDir.ordinal() + "\t" + a + "\t" + b
            + "\t" + present
            + "\t" + cMinX + "\t" + cMinY + "\t" + cMinZ
            + "\t" + cMaxX + "\t" + cMaxY + "\t" + cMaxZ + "\t" + cDir);
    }

    // A real BridgeCrossing parent at the given box + orientation. Public ctor:
    //   BridgeCrossing(int genDepth, BoundingBox boundingBox, Direction direction)
    static Object newCrossing(BoundingBox box, Direction dir) throws Exception {
        return crossingCls.getConstructor(int.class, BoundingBox.class, Direction.class)
            .newInstance(0, box, dir);
    }

    // A real StartPiece with its piece-weight lists EMPTIED so generatePiece draws
    // no RNG and always yields BridgeEndFiller. Public ctor: StartPiece(RandomSource,
    // int west, int north) — draws one nextInt(4) for its own orientation, which is
    // irrelevant here (we only use it as the startPiece arg / for the 112-gate
    // anchor, and the gate outcome is BridgeEndFiller either way).
    static Object newEmptiedStart() throws Exception {
        Object start = startCls.getConstructor(RandomSource.class, int.class, int.class)
            .newInstance(RandomSource.create(1L), 0, 0);
        clearList(start, "availableBridgePieces");
        clearList(start, "availableCastlePieces");
        return start;
    }

    static void clearList(Object start, String field) throws Exception {
        Field f = startCls.getDeclaredField(field);
        f.setAccessible(true);
        ((List<Object>) f.get(start)).clear();
    }

    static StructurePieceAccessor stubAccessor() {
        return (StructurePieceAccessor) Proxy.newProxyInstance(
            NetherFortressChildOffsetParity.class.getClassLoader(),
            new Class<?>[] { StructurePieceAccessor.class },
            (proxy, method, mArgs) -> {
                switch (method.getName()) {
                    case "findCollisionPiece": return null; // no collisions
                    case "addPiece": return null;           // void
                    default: return null;
                }
            });
    }
}
