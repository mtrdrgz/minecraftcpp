// Reference value generator for verifying the C++ port of
// net.minecraft.world.level.levelgen.MarsagliaPolarGaussian (the Marsaglia
// polar nextGaussian used by every RandomSource).
//
// We exercise the REAL decompiled MarsagliaPolarGaussian against the REAL
// decompiled RandomSource implementations (LegacyRandomSource,
// SingleThreadedRandomSource, XoroshiroRandomSource), seeded identically to the
// C++ mc::levelgen RandomSource types. The MarsagliaPolarGaussian draws from a
// RandomSource via nextDouble(), so the resulting Gaussian sequence is exact
// ground truth for the C++ `gaussian()` helper (== MarsagliaPolarGaussian).
//
// Each RandomSource type's own nextGaussian() ALSO routes through the identical
// polar method, so we emit both the standalone-MarsagliaPolarGaussian sequence
// and the RandomSource.nextGaussian() sequence; both must match the single C++
// implementation, which proves the port is the genuine polar method, seeded and
// stepped identically (including the cached nextNextGaussian / reset() path).
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/MarsagliaGaussianParity.java
//   java  -cp <out>;26.1.2/client.jar MarsagliaGaussianParity > marsaglia_gaussian.tsv
//
// Doubles are emitted as raw IEEE bits (Double.doubleToRawLongBits) so the C++
// comparison is bit-for-bit exact.
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.MarsagliaPolarGaussian;
import net.minecraft.world.level.levelgen.SingleThreadedRandomSource;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;

public class MarsagliaGaussianParity {
    static final java.io.PrintStream O = System.out;

    static String hx(double d) {
        return Long.toHexString(Double.doubleToRawLongBits(d));
    }

    // Build a fresh RandomSource of the requested kind, seeded with `seed`.
    static RandomSource make(String kind, long seed) {
        switch (kind) {
            case "legacy":  return new LegacyRandomSource(seed);
            case "single":  return new SingleThreadedRandomSource(seed);
            case "xoro":    return new XoroshiroRandomSource(seed);
            default: throw new IllegalArgumentException(kind);
        }
    }

    // Emit a MARS row: a standalone MarsagliaPolarGaussian over a freshly seeded
    // RandomSource, drawing `count` Gaussians. Tab-separated:
    //   MARS <kind> <seed> <count> <g0bits> <g1bits> ... <g(count-1)bits>
    static void mars(String kind, long seed, int count) {
        RandomSource rs = make(kind, seed);
        MarsagliaPolarGaussian g = new MarsagliaPolarGaussian(rs);
        StringBuilder sb = new StringBuilder();
        sb.append("MARS\t").append(kind).append('\t').append(seed).append('\t').append(count);
        for (int i = 0; i < count; i++) {
            sb.append('\t').append(hx(g.nextGaussian()));
        }
        O.println(sb.toString());
    }

    // Emit a MARSRESET row: draw `before` Gaussians, call reset() (clears the
    // cached nextNextGaussian — the haveNextNextGaussian path), then draw
    // `after` more. The reset can DROP a cached value, so the post-reset stream
    // diverges from the no-reset stream and exercises the reset() port.
    //   MARSRESET <kind> <seed> <before> <after> <g...>
    static void marsReset(String kind, long seed, int before, int after) {
        RandomSource rs = make(kind, seed);
        MarsagliaPolarGaussian g = new MarsagliaPolarGaussian(rs);
        StringBuilder sb = new StringBuilder();
        sb.append("MARSRESET\t").append(kind).append('\t').append(seed)
          .append('\t').append(before).append('\t').append(after);
        for (int i = 0; i < before; i++) {
            sb.append('\t').append(hx(g.nextGaussian()));
        }
        g.reset();
        for (int i = 0; i < after; i++) {
            sb.append('\t').append(hx(g.nextGaussian()));
        }
        O.println(sb.toString());
    }

    // Emit a RS row: the RandomSource's OWN nextGaussian() (which routes through
    // the identical polar method with the same cached-value behaviour).
    //   RS <kind> <seed> <count> <g...>
    static void rs(String kind, long seed, int count) {
        RandomSource rs = make(kind, seed);
        StringBuilder sb = new StringBuilder();
        sb.append("RS\t").append(kind).append('\t').append(seed).append('\t').append(count);
        for (int i = 0; i < count; i++) {
            sb.append('\t').append(hx(rs.nextGaussian()));
        }
        O.println(sb.toString());
    }

    public static void main(String[] args) throws Exception {
        // Bootstrap in case any static init is required.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // Gaussian path needs no registries; ignore if bootstrap is unavailable.
        }

        String[] kinds = { "legacy", "single", "xoro" };
        // FINITE / PHYSICAL seeds only (no special bit patterns needed — these are
        // plain longs feeding the RNG): include 0, small, large, negative, and a
        // couple of arbitrary world-seed-shaped values.
        long[] seeds = {
            0L, 1L, 2L, 7L, 42L, 123456789L, -1L, -987654321L,
            2147483647L, -2147483648L, 1234567890123456789L,
            -1234567890123456789L, 8675309L, -42L, 1000000007L
        };

        for (String kind : kinds) {
            for (long seed : seeds) {
                // Long sequences capture both halves of the cached-value pairing
                // (each loop iteration alternates fresh-pair vs cached return).
                mars(kind, seed, 16);
                rs(kind, seed, 16);
                // reset() variations: drop a cached value at an odd offset.
                marsReset(kind, seed, 1, 4);   // reset after 1 draw (a cached value pending)
                marsReset(kind, seed, 2, 4);   // reset after 2 draws (no cached value pending)
                marsReset(kind, seed, 3, 5);   // odd before -> cached value dropped by reset
            }
        }
    }
}
