// Ground-truth driver for the REAL net.minecraft.world.level.levelgen.structure
//   .structures.WoodlandMansionPieces$MansionGrid (private static nested class).
//
// We construct MansionGrid(RandomSource) for many seeds, then reflectively read
// the five resulting SimpleGrids (baseGrid, thirdFloorGrid, floorRooms[0..2]) and
// dump every cell. NOTHING about the algorithm is replicated Java-side — we drive
// the real constructor and only read its output fields.
//
// TSV row per seed (leading TAG=MANSION):
//   MANSION  <seed> <entranceX> <entranceY> <605 ints: base[121] third[121] f0[121] f1[121] f2[121]>
// All ints printed in decimal. Grid is Java int[width][height], flattened x*11+y.
//
// CRITICAL: must emit ZERO javac stderr (run_groundtruth treats any NOTE/warning as
// fatal). Hence @SuppressWarnings and no sun.misc.Unsafe import.
@SuppressWarnings({"deprecation", "unchecked"})
public final class WoodlandMansionGridLayoutParity {
    public static void main(String[] args) throws Exception {
        // Bootstrap.bootStrap() redirects System.out to a logger, so capture a
        // PrintStream bound to the ORIGINAL stdout file descriptor first and emit
        // every TSV row through it.
        java.io.PrintStream out = new java.io.PrintStream(
            new java.io.FileOutputStream(java.io.FileDescriptor.out), true, "US-ASCII");

        // Silence any logging before Bootstrap.
        try {
            Class<?> cfg = Class.forName("org.apache.logging.log4j.core.config.Configurator");
            Class<?> level = Class.forName("org.apache.logging.log4j.Level");
            Object off = level.getField("OFF").get(null);
            cfg.getMethod("setRootLevel", level).invoke(null, off);
        } catch (Throwable ignored) {
        }

        Class<?> sharedConstants = Class.forName("net.minecraft.SharedConstants");
        sharedConstants.getMethod("tryDetectVersion").invoke(null);
        Class<?> bootstrap = Class.forName("net.minecraft.server.Bootstrap");
        bootstrap.getMethod("bootStrap").invoke(null);

        Class<?> randomSourceClass = Class.forName("net.minecraft.util.RandomSource");
        java.lang.reflect.Method create = randomSourceClass.getMethod("create", long.class);

        // private static class MansionGrid  (binary name uses '$').
        Class<?> mansionGridClass =
            Class.forName("net.minecraft.world.level.levelgen.structure.structures.WoodlandMansionPieces$MansionGrid");
        java.lang.reflect.Constructor<?> mansionCtor =
            mansionGridClass.getDeclaredConstructor(randomSourceClass);
        mansionCtor.setAccessible(true);

        Class<?> simpleGridClass =
            Class.forName("net.minecraft.world.level.levelgen.structure.structures.WoodlandMansionPieces$SimpleGrid");

        java.lang.reflect.Field fBase = mansionGridClass.getDeclaredField("baseGrid");
        java.lang.reflect.Field fThird = mansionGridClass.getDeclaredField("thirdFloorGrid");
        java.lang.reflect.Field fRooms = mansionGridClass.getDeclaredField("floorRooms");
        java.lang.reflect.Field fEx = mansionGridClass.getDeclaredField("entranceX");
        java.lang.reflect.Field fEy = mansionGridClass.getDeclaredField("entranceY");
        fBase.setAccessible(true);
        fThird.setAccessible(true);
        fRooms.setAccessible(true);
        fEx.setAccessible(true);
        fEy.setAccessible(true);

        java.lang.reflect.Field fGrid = simpleGridClass.getDeclaredField("grid");
        java.lang.reflect.Field fW = simpleGridClass.getDeclaredField("width");
        java.lang.reflect.Field fH = simpleGridClass.getDeclaredField("height");
        fGrid.setAccessible(true);
        fW.setAccessible(true);
        fH.setAccessible(true);

        // A spread of finite seeds; mansion layout has plenty of branchy RNG paths.
        long[] seeds = new long[]{
            0L, 1L, 2L, 3L, 7L, 42L, 100L, 256L, 1024L, 65535L,
            123456789L, -1L, -42L, -100000L, 2147483647L, -2147483648L,
            9999999999L, -9999999999L, 4815162342L, 31337L,
            1000000007L, 88888888L, 555L, 7777777L, 13L,
            999L, 12321L, 54321L, 161803398L, 271828182L
        };

        StringBuilder sb = new StringBuilder();
        // Extend the seed set deterministically to exercise more RNG paths (~2000 rows).
        long s = 0x9E3779B97F4A7C15L;
        java.util.ArrayList<Long> seedList = new java.util.ArrayList<>();
        for (long sd : seeds) seedList.add(sd);
        long x = 1L;
        for (int i = 0; i < 1970; i++) {
            // SplitMix64-ish walk purely to pick varied finite seeds; not part of the gate.
            x += s;
            long z = x;
            z = (z ^ (z >>> 30)) * 0xBF58476D1CE4E5B9L;
            z = (z ^ (z >>> 27)) * 0x94D049BB133111EBL;
            z = z ^ (z >>> 31);
            seedList.add(z);
        }

        for (long seed : seedList) {
            Object random = create.invoke(null, seed);
            Object mansion;
            try {
                mansion = mansionCtor.newInstance(random);
            } catch (java.lang.reflect.InvocationTargetException e) {
                Throwable cause = e.getCause();
                throw (cause instanceof Exception) ? (Exception) cause : new RuntimeException(cause);
            }

            int ex = fEx.getInt(mansion);
            int ey = fEy.getInt(mansion);

            Object base = fBase.get(mansion);
            Object third = fThird.get(mansion);
            Object[] rooms = (Object[]) fRooms.get(mansion);

            sb.setLength(0);
            sb.append("MANSION\t").append(seed).append('\t').append(ex).append('\t').append(ey);
            appendGrid(sb, base, fGrid, fW, fH);
            appendGrid(sb, third, fGrid, fW, fH);
            appendGrid(sb, rooms[0], fGrid, fW, fH);
            appendGrid(sb, rooms[1], fGrid, fW, fH);
            appendGrid(sb, rooms[2], fGrid, fW, fH);
            out.println(sb.toString());
        }
        out.flush();
    }

    private static void appendGrid(StringBuilder sb, Object simpleGrid,
                                   java.lang.reflect.Field fGrid,
                                   java.lang.reflect.Field fW,
                                   java.lang.reflect.Field fH) throws Exception {
        int[][] grid = (int[][]) fGrid.get(simpleGrid);
        int w = fW.getInt(simpleGrid);
        int h = fH.getInt(simpleGrid);
        for (int gx = 0; gx < w; gx++) {
            for (int gy = 0; gy < h; gy++) {
                sb.append('\t').append(grid[gx][gy]);
            }
        }
    }
}
