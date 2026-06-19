// Ground-truth generator for the C++ port of the pure attribute-generation
// helpers of net.minecraft.world.entity.animal.equine.AbstractHorse (26.1.2):
//
//   static float  generateMaxHealth(IntUnaryOperator op)        // 15 + nextInt(8) + nextInt(9)
//   static double generateJumpStrength(DoubleSupplier p)        // 0.4F + 3*(p*0.2)
//   static double generateSpeed(DoubleSupplier p)               // (0.45F + 3*(p*0.3)) * 0.25
//   static double createOffspringAttribute(pa, pb, min, max, RandomSource random)
//
// We invoke the REAL decompiled methods (they are protected/package-private and
// static, so reflectively) against the REAL decompiled RandomSource
// implementations (Legacy/SingleThreaded/Xoroshiro), seeded identically to the
// C++ mc::levelgen RandomSource types. Because every helper consumes the
// RandomSource stream deterministically, the emitted sequence is exact ground
// truth: the C++ port must reproduce it bit-for-bit.
//
// In the live randomizeAttributes() the IntUnaryOperator is `random::nextInt`
// and the DoubleSupplier is `random::nextDouble`, so we pass exactly those
// method references. createOffspringAttribute is driven with a RandomSource
// directly. We also emit rows over the live MIN_*/MAX_* range constants (read
// reflectively from the class) so the real breeding path is covered.
//
//   javac -cp 26.1.2/client.jar;26.1.2/libs/* -d <out> mcpp/tools/AbstractHorseParity.java
//   java  -cp <out>;26.1.2/client.jar;26.1.2/libs/* AbstractHorseParity > horse.tsv
//
// Floats are emitted as Float.floatToRawIntBits, doubles as
// Double.doubleToRawLongBits, so the C++ comparison is bit-for-bit exact.
//
// NOTE: invoking the static helpers reflectively requires no instance; we still
// call SharedConstants/Bootstrap because the AbstractHorse class static
// initializer touches Attributes/registries.

import java.lang.reflect.Method;
import java.util.function.DoubleSupplier;
import java.util.function.IntUnaryOperator;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.animal.equine.AbstractHorse;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.SingleThreadedRandomSource;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;

public class AbstractHorseParity {
    static final java.io.PrintStream O = System.out;

    static String hf(float f)  { return Integer.toHexString(Float.floatToRawIntBits(f)); }
    static String hd(double d) { return Long.toHexString(Double.doubleToRawLongBits(d)); }

    static RandomSource make(String kind, long seed) {
        switch (kind) {
            case "legacy": return new LegacyRandomSource(seed);
            case "single": return new SingleThreadedRandomSource(seed);
            case "xoro":   return new XoroshiroRandomSource(seed);
            default: throw new IllegalArgumentException(kind);
        }
    }

    // Reflective handles to the real static helpers.
    static Method M_MAXHEALTH, M_JUMP, M_SPEED, M_OFFSPRING;

    static void resolve() throws Exception {
        M_MAXHEALTH = AbstractHorse.class.getDeclaredMethod("generateMaxHealth", IntUnaryOperator.class);
        M_JUMP      = AbstractHorse.class.getDeclaredMethod("generateJumpStrength", DoubleSupplier.class);
        M_SPEED     = AbstractHorse.class.getDeclaredMethod("generateSpeed", DoubleSupplier.class);
        M_OFFSPRING = AbstractHorse.class.getDeclaredMethod(
            "createOffspringAttribute", double.class, double.class, double.class, double.class, RandomSource.class);
        M_MAXHEALTH.setAccessible(true);
        M_JUMP.setAccessible(true);
        M_SPEED.setAccessible(true);
        M_OFFSPRING.setAccessible(true);
    }

    static float genMaxHealth(IntUnaryOperator op) throws Exception {
        return (Float) M_MAXHEALTH.invoke(null, op);
    }
    static double genJump(DoubleSupplier p) throws Exception {
        return (Double) M_JUMP.invoke(null, p);
    }
    static double genSpeed(DoubleSupplier p) throws Exception {
        return (Double) M_SPEED.invoke(null, p);
    }
    static double genOffspring(double pa, double pb, double min, double max, RandomSource r) throws Exception {
        return (Double) M_OFFSPRING.invoke(null, pa, pb, min, max, r);
    }

    // HEALTH <kind> <seed> <count> <bits...>: `count` consecutive rolls of
    // generateMaxHealth(random::nextInt) over one fresh RandomSource.
    static void health(String kind, long seed, int count) throws Exception {
        RandomSource r = make(kind, seed);
        StringBuilder sb = new StringBuilder("HEALTH\t").append(kind).append('\t').append(seed).append('\t').append(count);
        for (int i = 0; i < count; i++) sb.append('\t').append(hf(genMaxHealth(r::nextInt)));
        O.println(sb);
    }

    // JUMP <kind> <seed> <count> <bits...>
    static void jump(String kind, long seed, int count) throws Exception {
        RandomSource r = make(kind, seed);
        StringBuilder sb = new StringBuilder("JUMP\t").append(kind).append('\t').append(seed).append('\t').append(count);
        for (int i = 0; i < count; i++) sb.append('\t').append(hd(genJump(r::nextDouble)));
        O.println(sb);
    }

    // SPEED <kind> <seed> <count> <bits...>
    static void speed(String kind, long seed, int count) throws Exception {
        RandomSource r = make(kind, seed);
        StringBuilder sb = new StringBuilder("SPEED\t").append(kind).append('\t').append(seed).append('\t').append(count);
        for (int i = 0; i < count; i++) sb.append('\t').append(hd(genSpeed(r::nextDouble)));
        O.println(sb);
    }

    // OFFSPRING <kind> <seed> <paBits> <pbBits> <minBits> <maxBits> <count> <bits...>:
    // `count` consecutive createOffspringAttribute() calls with fixed params over
    // one fresh RandomSource (each call draws nextDouble() three times).
    static void offspring(String kind, long seed, double pa, double pb, double min, double max, int count) throws Exception {
        RandomSource r = make(kind, seed);
        StringBuilder sb = new StringBuilder("OFFSPRING\t").append(kind).append('\t').append(seed)
            .append('\t').append(hd(pa)).append('\t').append(hd(pb))
            .append('\t').append(hd(min)).append('\t').append(hd(max)).append('\t').append(count);
        for (int i = 0; i < count; i++) sb.append('\t').append(hd(genOffspring(pa, pb, min, max, r)));
        O.println(sb);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        resolve();

        String[] kinds = { "legacy", "single", "xoro" };
        long[] seeds = {
            0L, 1L, 2L, 7L, 42L, 123456789L, -1L, -987654321L,
            2147483647L, -2147483648L, 1234567890123456789L,
            -1234567890123456789L, 8675309L, -42L, 1000000007L
        };

        for (String kind : kinds) {
            for (long seed : seeds) {
                health(kind, seed, 24);
                jump(kind, seed, 24);
                speed(kind, seed, 24);
            }
        }

        // createOffspringAttribute over a spread of finite parent/range params.
        // Includes the REAL live attribute ranges used by setOffspringAttributes
        // (MAX_HEALTH=15..31, MOVEMENT_SPEED and JUMP_STRENGTH min/max derived
        // from the generators) plus degenerate/edge cases (equal parents, parents
        // outside the range to exercise the clamp, asymmetric ranges, negatives).
        // Parent values are deliberately driven both inside and outside [min,max]
        // so both the clamp and the >max / <min reflection branches fire.
        double[][] params = {
            // {pa, pb, min, max}
            { 20.0, 25.0, 15.0, 31.0 },     // MAX_HEALTH live-ish range
            { 15.0, 31.0, 15.0, 31.0 },     // extremes, full spread
            { 31.0, 15.0, 15.0, 31.0 },     // swapped extremes
            { 23.0, 23.0, 15.0, 31.0 },     // equal parents
            { 10.0, 40.0, 15.0, 31.0 },     // both outside -> clamp both
            { 0.1125, 0.3375, 0.1125, 0.3375 }, // MOVEMENT_SPEED-shaped range
            { 0.2, 0.2, 0.1125, 0.3375 },
            { 0.4, 1.0, 0.4, 1.0 },          // JUMP_STRENGTH-shaped range
            { 0.4, 0.4, 0.4, 1.0 },
            { 0.7, 0.55, 0.4, 1.0 },
            { -5.0, 5.0, -10.0, 10.0 },      // symmetric about zero (abs/sign)
            { -1.0, -8.0, -10.0, -2.0 },     // wholly negative range
            { 1000.0, -1000.0, -1.0, 1.0 },  // far-outside both, tiny range
            { 1.5, 2.5, 0.0, 1.0 },          // both above max
            { -2.0, -3.0, 0.0, 1.0 },        // both below min
            { 0.123456789, 0.987654321, 0.0, 1.0 },
            { 100.25, 100.75, 100.0, 101.0 },
        };

        // A handful of seeds is enough for the offspring sweep (the RNG draw is
        // 3x nextDouble per call); use the first several for broad coverage.
        long[] offSeeds = { 0L, 1L, 42L, -1L, 123456789L, -987654321L, 8675309L, 2147483647L };
        for (String kind : kinds) {
            for (long seed : offSeeds) {
                for (double[] p : params) {
                    offspring(kind, seed, p[0], p[1], p[2], p[3], 12);
                }
            }
        }
    }
}
