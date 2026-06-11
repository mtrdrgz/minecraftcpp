// Ground-truth generator for the PURE template-placement-position math nested in
// the REAL decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.IglooPieces
//     -> public static class IglooPiece
//          private static BlockPos makePosition(Identifier templateLocation,
//                                               BlockPos position, int depth)
//
// makePosition is self-contained integer geometry:
//   return position.offset(OFFSETS.get(templateLocation)).below(depth);
// NO world writes, NO RandomSource, NO registry/datapack (OFFSETS is a hard-coded
// ImmutableMap of compile-time BlockPos constants in IglooPieces). The method is
// private static, so we invoke it via reflection; the IglooPieces <clinit> builds
// only Identifier/BlockPos constants, but we Bootstrap first to match the harness.
//
//   tools/run_groundtruth.ps1 -Tool IglooPiecePositionParity -Out mcpp/build/igloo_piece_position.tsv
//
// Line format (all ints decimal):
//   POS  <templateKey> <px> <py> <pz> <depth> <ox> <oy> <oz>
//        -- makePosition(Identifier.parse(templateKey), BlockPos(px,py,pz), depth)
//           result (ox,oy,oz) from the REAL class. templateKey is the full
//           "minecraft:igloo/<...>" id string.

import java.lang.reflect.Method;

import net.minecraft.core.BlockPos;
import net.minecraft.resources.Identifier;

public class IglooPiecePositionParity {
    static final java.io.PrintStream OUT = System.out;

    static Method M_makePosition;

    // The three real template keys (IglooPieces.java:35-37 -> Identifier
    // .withDefaultNamespace("igloo/top" | "igloo/middle" | "igloo/bottom")).
    static final String[] KEYS = {
        "minecraft:igloo/top",
        "minecraft:igloo/middle",
        "minecraft:igloo/bottom",
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> outer = Class.forName(
            "net.minecraft.world.level.levelgen.structure.structures.IglooPieces");
        Class<?> pieceCls = null;
        for (Class<?> c : outer.getDeclaredClasses()) {
            if (c.getSimpleName().equals("IglooPiece")) pieceCls = c;
        }
        if (pieceCls == null) throw new IllegalStateException("IglooPiece not found");

        M_makePosition = pieceCls.getDeclaredMethod(
            "makePosition", Identifier.class, BlockPos.class, int.class);
        M_makePosition.setAccessible(true);

        // Representative position battery: origin, axis-aligned probes, a few
        // mixed/negative/large coords (the real call site passes a chunk-derived
        // BlockPos, but the math is pure so we probe wider to pin the adds).
        int[][] positions = {
            {0, 0, 0},
            {1, 64, 1}, {-1, 64, -1}, {7, 70, 11}, {-8, 90, -13},
            {16, 0, 16}, {-16, 256, -16}, {100, -64, -100},
            {123456, 200, -654321}, {-2000000000, 0, 2000000000},
        };
        // depth battery: the real call sites use depth*3 / i*3 with
        // depth = random.nextInt(8)+4 (so 12..33) and i in 0..depth-2 (0,3,6,...);
        // also include 0 (the top piece) and negative/large to pin the subtraction.
        int[] depths = {0, 1, 3, 6, 9, 12, 33, -5, 1000, -2000000000};

        for (String key : KEYS) {
            Identifier id = Identifier.parse(key);
            for (int[] p : positions) {
                for (int d : depths) {
                    emit(key, id, p[0], p[1], p[2], d);
                }
            }
        }

        OUT.flush();
    }

    static void emit(String key, Identifier id, int px, int py, int pz, int depth) throws Exception {
        BlockPos out = (BlockPos) M_makePosition.invoke(null, id, new BlockPos(px, py, pz), depth);
        OUT.println("POS\t" + key + "\t" + px + "\t" + py + "\t" + pz + "\t" + depth
            + "\t" + out.getX() + "\t" + out.getY() + "\t" + out.getZ());
    }
}
