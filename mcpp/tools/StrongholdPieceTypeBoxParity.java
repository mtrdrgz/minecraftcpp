// Ground-truth generator for the PURE box-construction + validity layer of the
// REAL decompiled 26.1.2 class
//   net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces
//
// Each StrongholdPieces.<X>.createPiece(...) builds a candidate BoundingBox via
// BoundingBox.orientBox(footX,footY,footZ, <consts>, direction) and accepts it iff
// StrongholdPieces.StrongholdPiece.isOkBox(box) (== box.minY() > 10) AND there is
// no colliding piece. This tool drives the REAL Java for the pure half:
//   * the box   = REAL BoundingBox.orientBox(...) with each type's constants,
//   * the okBox  = REAL StrongholdPiece.isOkBox(box) (protected static, reflection),
// and ALSO cross-checks each type's constants by reflectively invoking the REAL
// <X>.createPiece(...) with a stub StructurePieceAccessor and confirming the box of
// the returned piece equals orientBox(...). The C++ test recomputes the box + okBox
// from StrongholdPieceTypeBox.h and must match every field exactly.
//
//   tools/run_groundtruth.ps1 -Tool StrongholdPieceTypeBoxParity -Out mcpp/build/stronghold_piece_type_box.tsv
//
// Output rows (tab-separated, tag first):
//   BOX  <typeIdx> <footX> <footY> <footZ> <dirOrd>  <minX> <minY> <minZ> <maxX> <maxY> <maxZ> <okBox 0|1>
// dirOrd uses net.minecraft.core.Direction.ordinal(): DOWN0 UP1 NORTH2 SOUTH3 WEST4 EAST5.
// Only the four horizontals are emitted (the only ones real stronghold gen passes).
//
// Bootstrap is required because the createPiece cross-check allocates real
// StructurePiece subtypes that reference registered StructurePieceType constants.

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Proxy;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.StructurePieceAccessor;

public class StrongholdPieceTypeBoxParity {
    static final java.io.PrintStream OUT = System.out;

    // Per-type orientBox(...) constants, copied verbatim from StrongholdPieces.java.
    // Index == StrongholdPiece enum ordinal in StrongholdPieceTypeBox.h.
    // {offX, offY, offZ, width, height, depth}
    static final int[][] SPECS = {
        { -1, -1, 0,  5,  5,  7 }, // 0  ChestCorridor       .java:258
        { -4, -3, 0, 10,  9, 11 }, // 1  FiveCrossing        .java:448
        { -1, -1, 0,  5,  5,  5 }, // 2  LeftTurn            .java:542
        { -4, -1, 0, 14, 11, 15 }, // 3  LibraryTall         .java:601
        { -4, -1, 0, 14,  6, 15 }, // 4  LibraryShort        .java:603 (fallback)
        { -4, -1, 0, 11,  8, 16 }, // 5  PortalRoom          .java:785
        { -1, -1, 0,  9,  5, 11 }, // 6  PrisonHall          .java:914
        { -1, -1, 0,  5,  5,  5 }, // 7  RightTurn           .java:999
        { -4, -1, 0, 11,  7, 11 }, // 8  RoomCrossing        .java:1066
        { -1, -7, 0,  5, 11,  5 }, // 9  StairsDown          .java:1242
        { -1, -1, 0,  5,  5,  7 }, // 10 Straight            .java:1349
        { -1, -7, 0,  5, 11,  8 }, // 11 StraightStairsDown  .java:1411
    };

    // Simple-class-name of each piece type (an inner class of StrongholdPieces),
    // used only for the createPiece(...) cross-check. LibraryShort shares Library's
    // createPiece (no separate factory) and is cross-checked via LibraryTall's box.
    static final String[] NAMES = {
        "ChestCorridor", "FiveCrossing", "LeftTurn", "Library", "Library",
        "PortalRoom", "PrisonHall", "RightTurn", "RoomCrossing", "StairsDown",
        "Straight", "StraightStairsDown"
    };

    static final Direction[] HORIZ = { Direction.NORTH, Direction.SOUTH, Direction.WEST, Direction.EAST };

    public static void main(String[] args) throws Exception {
        // Constructing an inverted BoundingBox (possible for negative-offset specs)
        // logs an ERROR via log4j whose default appender writes to the stdout FD and
        // would pollute the TSV. Silence the root logger before any logging occurs.
        org.apache.logging.log4j.core.config.Configurator.setRootLevel(org.apache.logging.log4j.Level.OFF);

        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // REAL StrongholdPieces.StrongholdPiece.isOkBox(BoundingBox) — protected
        // static on the abstract inner base class. Reach it by name + setAccessible.
        Class<?> baseCls = Class.forName(
            "net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces$StrongholdPiece");
        Method isOkBox = baseCls.getDeclaredMethod("isOkBox", BoundingBox.class);
        isOkBox.setAccessible(true);

        // Foot positions chosen to exercise both sides of the isOkBox(minY>10) gate
        // (including the exact boundary) and inverted-bounds normalization. The
        // offY values across types are {-1,-3,-7,0}, so feet that put minY at 10/11
        // for several offsets pin the strict `> 10` boundary.
        int[][] foots = {
            { 0, 64, 0 }, { 5, 48, -3 }, { -7, 30, 9 }, { 16, 11, 16 },
            { 0, 12, 0 },  // offY=-1 -> minY=11 (ok); offY=-3 -> minY=9 (not ok)
            { -2, 14, 4 }, // offY=-3 -> minY=11 (ok); offY=-7 -> minY=7 (not ok)
            { 3, 11, -5 }, // offY=-1 -> minY=10 (not ok, boundary); offY=0 -> 11 (ok)
            { 1, 18, 2 },  // offY=-7 -> minY=11 (ok)
            { 100, 200, -100 }, { -50, 17, -50 }
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

        // Cross-check: drive the REAL <X>.createPiece(...) for one valid (high) foot
        // per type/direction and assert the returned piece's box equals orientBox(...)
        // for that type's SPEC. This proves the SPECS constants are exactly what Java
        // uses. For Library, the no-collision stub returns the TALL box (index 3), so
        // LibraryShort (index 4) is cross-checked against SPEC[3]'s factory but its
        // own SPEC[4] box is still emitted in the BOX rows above. Failures print as
        // XCHK_FAIL rows (the C++ test skips unknown tags — diagnostic only).
        crossCheckCreatePiece();
    }

    // A StructurePieceAccessor that never reports a collision (so createPiece's box
    // survives to the returned piece) and ignores added pieces.
    static StructurePieceAccessor stubAccessor() {
        return (StructurePieceAccessor) Proxy.newProxyInstance(
            StrongholdPieceTypeBoxParity.class.getClassLoader(),
            new Class<?>[] { StructurePieceAccessor.class },
            (proxy, method, mArgs) -> {
                switch (method.getName()) {
                    case "findCollisionPiece": return null; // no collisions
                    case "addPiece": return null;           // void
                    default: return null;
                }
            });
    }

    static void crossCheckCreatePiece() {
        StructurePieceAccessor acc = stubAccessor();
        RandomSource rng = RandomSource.create(1234L);
        int footX = 0, footY = 64, footZ = 0; // high -> isOkBox always true; Library -> tall box
        for (int t = 0; t < NAMES.length; t++) {
            String simple = NAMES[t];
            // The SPEC whose box createPiece is expected to produce: Library's factory
            // yields the TALL box (index 3); every other type yields its own SPEC.
            int specIdx = simple.equals("Library") ? 3 : t;
            try {
                Class<?> cls = Class.forName(
                    "net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces$" + simple);
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
                    int[] s = SPECS[specIdx];
                    BoundingBox want = BoundingBox.orientBox(footX, footY, footZ,
                        s[0], s[1], s[2], s[3], s[4], s[5], dir);
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
