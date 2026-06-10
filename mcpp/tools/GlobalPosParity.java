// Ground-truth generator for net.minecraft.core.GlobalPos (Minecraft 26.1.2),
// calling the REAL decompiled classes. Emits tab-separated rows to STDOUT; the
// runner captures stdout into global_pos.tsv.
//
//   tools/run_groundtruth.ps1 -Tool GlobalPosParity -Out mcpp/build/global_pos.tsv
//
// Ported surface (byte-deterministic):
//   * GlobalPos.toString()        -> raw string
//   * GlobalPos.equals(other)     -> 0/1
//   * GlobalPos.isCloseEnough(..) -> 0/1
//   * BlockPos.asLong()           -> long (packed pos)
//   * BlockPos.hashCode()         -> int (Vec3i hashCode, deterministic)
//   * BlockPos.distChessboard()   -> int
// SKIPPED: GlobalPos.hashCode() — record hashCode folds in ResourceKey's identity
//   hashCode (no equals/hashCode override on ResourceKey), which is non-deterministic
//   across JVM runs and cannot be byte-matched. Codecs are out of scope.

import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.Level;

public class GlobalPosParity {
    static final java.io.PrintStream O = System.out;

    // Dimension identifiers (namespace, path). Mix of vanilla + custom namespaces.
    static final String[][] DIMS = {
        {"minecraft", "overworld"},
        {"minecraft", "the_nether"},
        {"minecraft", "the_end"},
        {"minecraft", "custom_dim"},
        {"mymod", "skyland"},
        {"another_mod", "deep.cave"},
    };

    static final int[] XS = { 0, 1, -1, 15, 16, -16, 100, -100, 1000, -1000,
        33554431, -33554432, 30000000, -30000000, 12345, -67890, 2097151, -2097152 };
    static final int[] YS = { 0, 1, -1, 15, 16, -16, 63, 64, -64, 255, 256, -320,
        2047, -2048, 319, 384 };

    static ResourceKey<Level> dimKey(String ns, String path) {
        return ResourceKey.create(Registries.DIMENSION, Identifier.fromNamespaceAndPath(ns, path));
    }

    public static void main(String[] args) throws Exception {
        // Registries.<clinit> / Level constants pull in bootstrapped state.
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // 1) Single-instance accessors: toString, asLong, hashCode of pos.
        for (String[] d : DIMS) {
            ResourceKey<Level> dim = dimKey(d[0], d[1]);
            for (int x : XS) for (int y : YS) for (int z : XS) {
                BlockPos pos = new BlockPos(x, y, z);
                GlobalPos gp = GlobalPos.of(dim, pos);
                // GP_STR: ns path x y z  ->  toString (raw, may contain spaces, no tabs)
                O.println("GP_STR\t" + d[0] + "\t" + d[1] + "\t" + x + "\t" + y + "\t" + z
                    + "\t" + gp.toString());
                // BP_ASLONG / BP_HASH / on the pos
                O.println("BP_ASLONG\t" + x + "\t" + y + "\t" + z + "\t" + pos.asLong());
                O.println("BP_HASH\t" + x + "\t" + y + "\t" + z + "\t" + pos.hashCode());
            }
        }

        // 2) distChessboard over BlockPos pairs (used by isCloseEnough).
        int[] CX = { 0, 1, -1, 16, -16, 100, -100, 1000, -1000, 2097151, -2097152 };
        int[] CY = { 0, 1, -1, 64, -64, 319, -64, 256 };
        for (int ax : CX) for (int ay : CY) for (int az : CX)
            for (int bx : CX) for (int by : CY) for (int bz : CX) {
                BlockPos a = new BlockPos(ax, ay, az);
                BlockPos b = new BlockPos(bx, by, bz);
                O.println("BP_CHESS\t" + ax + "\t" + ay + "\t" + az + "\t"
                    + bx + "\t" + by + "\t" + bz + "\t" + a.distChessboard(b));
            }

        // 3) equals over GlobalPos pairs (dimension same/different x pos same/different).
        String[][] EDIMS = { {"minecraft","overworld"}, {"minecraft","the_nether"},
            {"mymod","skyland"} };
        int[][] EPOS = { {0,0,0}, {1,2,3}, {1,2,3}, {-5,64,-5}, {100,-100,100} };
        for (String[] da : EDIMS) for (int[] pa : EPOS)
            for (String[] db : EDIMS) for (int[] pb : EPOS) {
                GlobalPos ga = GlobalPos.of(dimKey(da[0], da[1]), new BlockPos(pa[0], pa[1], pa[2]));
                GlobalPos gb = GlobalPos.of(dimKey(db[0], db[1]), new BlockPos(pb[0], pb[1], pb[2]));
                O.println("GP_EQ\t" + da[0] + "\t" + da[1] + "\t" + pa[0] + "\t" + pa[1] + "\t" + pa[2]
                    + "\t" + db[0] + "\t" + db[1] + "\t" + pb[0] + "\t" + pb[1] + "\t" + pb[2]
                    + "\t" + (ga.equals(gb) ? 1 : 0));
            }

        // 4) isCloseEnough(dimension, pos, maxDistance).
        int[] MAXD = { 0, 1, 5, 16, 64, 1000 };
        for (String[] da : EDIMS) for (int[] pa : new int[][]{ {0,0,0}, {100,64,-100} })
            for (String[] db : EDIMS) for (int[] pb : new int[][]{ {0,0,0}, {3,3,3}, {100,64,-84}, {1100,64,-100} })
                for (int md : MAXD) {
                    GlobalPos ga = GlobalPos.of(dimKey(da[0], da[1]), new BlockPos(pa[0], pa[1], pa[2]));
                    boolean r = ga.isCloseEnough(dimKey(db[0], db[1]), new BlockPos(pb[0], pb[1], pb[2]), md);
                    O.println("GP_CLOSE\t" + da[0] + "\t" + da[1] + "\t" + pa[0] + "\t" + pa[1] + "\t" + pa[2]
                        + "\t" + db[0] + "\t" + db[1] + "\t" + pb[0] + "\t" + pb[1] + "\t" + pb[2]
                        + "\t" + md + "\t" + (r ? 1 : 0));
                }
    }
}
