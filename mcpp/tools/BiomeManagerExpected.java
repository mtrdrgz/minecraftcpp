import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.MessageDigest;

public class BiomeManagerExpected {
    private record Quart(int x, int y, int z) {}

    private static long obfuscateSeed(long seed) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        byte[] input = ByteBuffer.allocate(Long.BYTES).order(ByteOrder.LITTLE_ENDIAN).putLong(seed).array();
        byte[] hash = digest.digest(input);
        return ByteBuffer.wrap(hash, 0, Long.BYTES).order(ByteOrder.LITTLE_ENDIAN).getLong();
    }

    private static long lcgNext(long rval, long c) {
        rval *= rval * 6364136223846793005L + 1442695040888963407L;
        return rval + c;
    }

    private static double getFiddle(long rval) {
        double uniform = Math.floorMod(rval >> 24, 1024) / 1024.0;
        return (uniform - 0.5) * 0.9;
    }

    private static double getFiddledDistance(long seed, int xRandom, int yRandom, int zRandom,
                                             double distanceX, double distanceY, double distanceZ) {
        long rval = seed;
        rval = lcgNext(rval, xRandom);
        rval = lcgNext(rval, yRandom);
        rval = lcgNext(rval, zRandom);
        rval = lcgNext(rval, xRandom);
        rval = lcgNext(rval, yRandom);
        rval = lcgNext(rval, zRandom);
        double fiddleX = getFiddle(rval);
        rval = lcgNext(rval, seed);
        double fiddleY = getFiddle(rval);
        rval = lcgNext(rval, seed);
        double fiddleZ = getFiddle(rval);
        return square(distanceZ + fiddleZ) + square(distanceY + fiddleY) + square(distanceX + fiddleX);
    }

    private static double square(double value) {
        return value * value;
    }

    private static Quart selectQuart(long biomeZoomSeed, int blockX, int blockY, int blockZ) {
        int absX = blockX - 2;
        int absY = blockY - 2;
        int absZ = blockZ - 2;
        int parentX = absX >> 2;
        int parentY = absY >> 2;
        int parentZ = absZ >> 2;
        double fractX = (absX & 3) / 4.0;
        double fractY = (absY & 3) / 4.0;
        double fractZ = (absZ & 3) / 4.0;
        int minI = 0;
        double minFiddledDistance = Double.POSITIVE_INFINITY;
        for (int i = 0; i < 8; i++) {
            boolean xEven = (i & 4) == 0;
            boolean yEven = (i & 2) == 0;
            boolean zEven = (i & 1) == 0;
            int cornerX = xEven ? parentX : parentX + 1;
            int cornerY = yEven ? parentY : parentY + 1;
            int cornerZ = zEven ? parentZ : parentZ + 1;
            double distanceX = xEven ? fractX : fractX - 1.0;
            double distanceY = yEven ? fractY : fractY - 1.0;
            double distanceZ = zEven ? fractZ : fractZ - 1.0;
            double next = getFiddledDistance(biomeZoomSeed, cornerX, cornerY, cornerZ, distanceX, distanceY, distanceZ);
            if (minFiddledDistance > next) {
                minI = i;
                minFiddledDistance = next;
            }
        }
        int biomeX = (minI & 4) == 0 ? parentX : parentX + 1;
        int biomeY = (minI & 2) == 0 ? parentY : parentY + 1;
        int biomeZ = (minI & 1) == 0 ? parentZ : parentZ + 1;
        return new Quart(biomeX, biomeY, biomeZ);
    }

    public static void main(String[] args) throws Exception {
        long[] seeds = {0L, 1L, -1L, 12345L, 987654321L, -98765432123456789L};
        for (long seed : seeds) {
            System.out.printf("seed %d -> %d%n", seed, obfuscateSeed(seed));
        }

        long[] fiddles = {0L, 1L, -1L, 123456789L, -98765432123456789L};
        for (long value : fiddles) {
            System.out.printf("fiddle %d -> %.17g%n", value, getFiddle(value));
        }

        long zoom = obfuscateSeed(12345L);
        System.out.printf("distance -> %.17g%n", getFiddledDistance(zoom, -3, 7, 19, 0.25, -0.5, 0.75));
        int[][] positions = {
            {0, 64, 0},
            {1, 64, 1},
            {-17, 70, 33},
            {128, -32, -65},
            {300000, 80, -300000}
        };
        for (int[] pos : positions) {
            Quart q = selectQuart(zoom, pos[0], pos[1], pos[2]);
            System.out.printf("quart %d,%d,%d -> %d,%d,%d%n", pos[0], pos[1], pos[2], q.x, q.y, q.z);
        }
    }
}
