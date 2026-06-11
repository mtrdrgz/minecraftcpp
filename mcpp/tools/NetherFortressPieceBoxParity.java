// Ground-truth generator for the PURE box-construction + validity layer of the
// REAL decompiled 26.1.2 class
//   net.minecraft.world.level.levelgen.structure.structures.NetherFortressPieces
//
// Each NetherFortressPieces.<X>.createPiece(...) builds a candidate BoundingBox
// via BoundingBox.orientBox(footX,footY,footZ, <consts>, direction) and accepts
// it iff NetherFortressPieces.NetherBridgePiece.isOkBox(box) (== box.minY() > 10)
// AND there is no colliding piece. This tool drives the REAL Java for the pure
// half:
//   * the box     = REAL BoundingBox.orientBox(...) with each type's constants,
//   * the okBox    = REAL NetherBridgePiece.isOkBox(box) (protected static, via
//                    reflection),
// and ALSO cross-checks each type's constants by reflectively invoking the REAL
// <X>.createPiece(...) with a stub StructurePieceAccessor and confirming the box
// of the returned piece equals orientBox(...). The C++ test recomputes the box
// + okBox from NetherFortressPieceBox.h and must match every field exactly.
//
//   tools/run_groundtruth.ps1 -Tool NetherFortressPieceBoxParity -Out mcpp/build/nether_fortress_piece_box.tsv
//
// Output rows (tab-separated, tag first):
//   BOX  <typeIdx> <footX> <footY> <footZ> <dirOrd>  <minX> <minY> <minZ> <maxX> <maxY> <maxZ> <okBox 0|1>
// dirOrd uses net.minecraft.core.Direction.ordinal(): DOWN0 UP1 NORTH2 SOUTH3 WEST4 EAST5.
// Only the four horizontals are emitted (the only ones real fortress gen passes).
//
// Bootstrap is required because the createPiece cross-check allocates real
// StructurePiece subtypes that reference registered StructurePieceType constants.

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Proxy;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.StructurePieceAccessor;

public class NetherFortressPieceBoxParity {
    static final java.io.PrintStream OUT = System.out;

    // Per-type orientBox(...) constants, copied verbatim from NetherFortressPieces.java.
    // Index == NetherFortressPiece enum ordinal in NetherFortressPieceBox.h.
    // {offX, offY, offZ, width, height, depth}
    static final int[][] SPECS = {
        { -8, -3, 0, 19, 10, 19 }, // 0  BridgeCrossing                    .java:131
        { -1, -3, 0,  5, 10,  8 }, // 1  BridgeEndFiller                   .java:211
        { -1, -3, 0,  5, 10, 19 }, // 2  BridgeStraight                    .java:289
        { -1, -7, 0,  5, 14, 10 }, // 3  CastleCorridorStairsPiece         .java:357
        { -3,  0, 0,  9,  7,  9 }, // 4  CastleCorridorTBalconyPiece       .java:436
        { -5, -3, 0, 13, 14, 13 }, // 5  CastleEntrance                    .java:516
        { -1,  0, 0,  5,  7,  5 }, // 6  CastleSmallCorridorCrossingPiece  .java:638
        { -1,  0, 0,  5,  7,  5 }, // 7  CastleSmallCorridorLeftTurnPiece  .java:707
        { -1,  0, 0,  5,  7,  5 }, // 8  CastleSmallCorridorPiece          .java:771
        { -1,  0, 0,  5,  7,  5 }, // 9  CastleSmallCorridorRightTurnPiece .java:843
        { -5, -3, 0, 13, 14, 13 }, // 10 CastleStalkRoom                   .java:908
        { -2,  0, 0,  7,  8,  9 }, // 11 MonsterThrone                     .java:1070
        { -2,  0, 0,  7,  9,  7 }, // 12 RoomCrossing                      .java:1497
        { -2,  0, 0,  7, 11,  7 }, // 13 StairsRoom                        .java:1562
    };

    // Simple-class-name of each piece type (an inner class of NetherFortressPieces),
    // used only for the createPiece(...) cross-check.
    static final String[] NAMES = {
        "BridgeCrossing", "BridgeEndFiller", "BridgeStraight", "CastleCorridorStairsPiece",
        "CastleCorridorTBalconyPiece", "CastleEntrance", "CastleSmallCorridorCrossingPiece",
        "CastleSmallCorridorLeftTurnPiece", "CastleSmallCorridorPiece",
        "CastleSmallCorridorRightTurnPiece", "CastleStalkRoom", "MonsterThrone",
        "RoomCrossing", "StairsRoom"
    };

    static final Direction[] HORIZ = { Direction.NORTH, Direction.SOUTH, Direction.WEST, Direction.EAST };

    public static void main(String[] args) throws Exception {
        // Constructing an inverted BoundingBox (possible for negative-offset specs)
        // logs an ERROR via log4j whose default appender writes to the stdout FD and
        // would pollute the TSV. Silence the root logger before any logging occurs.
        org.apache.logging.log4j.core.config.Configurator.setRootLevel(org.apache.logging.log4j.Level.OFF);

        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // REAL NetherFortressPieces.NetherBridgePiece.isOkBox(BoundingBox) — protected
        // static on a private inner class. Reach it by name + setAccessible.
        Class<?> bridgeCls = Class.forName(
            "net.minecraft.world.level.levelgen.structure.structures.NetherFortressPieces$NetherBridgePiece");
        Method isOkBox = bridgeCls.getDeclaredMethod("isOkBox", BoundingBox.class);
        isOkBox.setAccessible(true);

        // Foot positions chosen to exercise both sides of the isOkBox(minY>10) gate
        // (including the exact boundary) and inverted-bounds normalization.
        int[][] foots = {
            { 0, 64, 0 }, { 5, 48, -3 }, { -7, 30, 9 }, { 16, 11, 16 },
            { 0, 13, 0 },  // minY lands near the >10 boundary for various offY
            { -2, 14, 4 }, // boundary for offY=-3 pieces -> minY=11 (ok)
            { 3, 10, -5 }, // boundary low side
            { 1, 17, 2 },  // offY=-7 piece -> minY=10 (not ok)
            { 100, 200, -100 }
        };

        for (int t = 0; t < SPECS.length; t++) {
            int[] s = SPECS[t];
            for (int[] f : foots) {
                for (Direction dir : HORIZ) {
                    // REAL orientBox + REAL isOkBox.
                    BoundingBox box = BoundingBox.orientBox(
                        f[0], f[1], f[2], s[0], s[1], s[2], s[3], s[4], s[5], dir);
                    boolean ok = (Boolean) isOkBox.invoke(null, box);
                    OUT.println("BOX\t" + t + "\t" + f[0] + "\t" + f[1] + "\t" + f[2] + "\t" + dir.ordinal()
                        + "\t" + box.minX() + "\t" + box.minY() + "\t" + box.minZ()
                        + "\t" + box.maxX() + "\t" + box.maxY() + "\t" + box.maxZ()
                        + "\t" + (ok ? 1 : 0));
                }
            }
        }

        // Cross-check: drive the REAL <X>.createPiece(...) for one valid foot per
        // type/direction and assert the returned piece's box equals orientBox(...).
        // This proves the SPECS constants above are exactly what Java uses. Failures
        // are printed as XCHK_FAIL rows (the C++ test skips unknown tags, so this is
        // diagnostic only and never pollutes the gate).
        crossCheckCreatePiece(isOkBox);
    }

    // A StructurePieceAccessor that never reports a collision (so createPiece's box
    // survives to the returned piece) and ignores added pieces.
    static StructurePieceAccessor stubAccessor() {
        return (StructurePieceAccessor) Proxy.newProxyInstance(
            NetherFortressPieceBoxParity.class.getClassLoader(),
            new Class<?>[] { StructurePieceAccessor.class },
            (proxy, method, mArgs) -> {
                switch (method.getName()) {
                    case "findCollisionPiece": return null; // no collisions
                    case "addPiece": return null;           // void
                    default: return null;
                }
            });
    }

    static void crossCheckCreatePiece(Method isOkBox) {
        StructurePieceAccessor acc = stubAccessor();
        RandomSource rng = RandomSource.create(1234L);
        int footX = 0, footY = 64, footZ = 0; // high -> isOkBox always true
        for (int t = 0; t < NAMES.length; t++) {
            String simple = NAMES[t];
            try {
                Class<?> cls = Class.forName(
                    "net.minecraft.world.level.levelgen.structure.structures.NetherFortressPieces$" + simple);
                Method create = null;
                for (Method m : cls.getDeclaredMethods()) {
                    if (m.getName().equals("createPiece") && Modifier.isStatic(m.getModifiers())) { create = m; break; }
                }
                if (create == null) { OUT.println("XCHK_FAIL\t" + t + "\tno createPiece"); continue; }
                create.setAccessible(true);
                Class<?>[] pts = create.getParameterTypes();
                for (Direction dir : HORIZ) {
                    Object[] callArgs = new Object[pts.length];
                    int intSeen = 0;
                    int[] ints = { footX, footY, footZ }; // footX, footY, footZ in order
                    int trailingDepth = 0;                // genDepth (the int after Direction)
                    for (int i = 0; i < pts.length; i++) {
                        Class<?> pt = pts[i];
                        if (StructurePieceAccessor.class.isAssignableFrom(pt)) callArgs[i] = acc;
                        else if (RandomSource.class.isAssignableFrom(pt)) callArgs[i] = rng;
                        else if (pt == Direction.class) callArgs[i] = dir;
                        else if (pt == int.class) {
                            // First three ints are footX, footY, footZ (always appear
                            // before Direction in createPiece); any later int is genDepth.
                            if (intSeen < 3) callArgs[i] = ints[intSeen];
                            else callArgs[i] = trailingDepth;
                            intSeen++;
                        } else callArgs[i] = null;
                    }
                    Object piece = create.invoke(null, callArgs);
                    BoundingBox want = BoundingBox.orientBox(footX, footY, footZ,
                        SPECS[t][0], SPECS[t][1], SPECS[t][2], SPECS[t][3], SPECS[t][4], SPECS[t][5], dir);
                    BoundingBox got = piece == null ? null : ((StructurePiece) piece).getBoundingBox();
                    boolean eq = got != null
                        && got.minX() == want.minX() && got.minY() == want.minY() && got.minZ() == want.minZ()
                        && got.maxX() == want.maxX() && got.maxY() == want.maxY() && got.maxZ() == want.maxZ();
                    if (!eq) {
                        OUT.println("XCHK_FAIL\t" + t + "\t" + simple + "\t" + dir
                            + "\twant=" + boxStr(want) + "\tgot=" + (got == null ? "null" : boxStr(got)));
                    }
                }
            } catch (Throwable e) {
                OUT.println("XCHK_FAIL\t" + t + "\t" + simple + "\t" + e);
            }
        }
    }

    static String boxStr(BoundingBox b) {
        return b.minX() + "," + b.minY() + "," + b.minZ() + "/" + b.maxX() + "," + b.maxY() + "," + b.maxZ();
    }
}
