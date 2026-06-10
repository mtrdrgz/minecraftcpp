// Reference value generator for the C++ net.minecraft.util.valueproviders.TrapezoidFloat
// port (verified against mcpp/src/world/level/levelgen/FloatProvider.h ::
// mc::valueproviders::TrapezoidFloat).
//
// Runs the REAL decompiled net.minecraft.util.valueproviders.TrapezoidFloat from
// client.jar so the emitted sample sequences are exact ground truth. The sole
// behaviour under test is TrapezoidFloat.sample(RandomSource):
//
//     float range        = this.max - this.min;
//     float plateauStart = (range - this.plateau) / 2.0F;
//     float plateauEnd   = range - plateauStart;
//     return this.min + random.nextFloat() * plateauEnd + random.nextFloat() * plateauStart;
//
// Two nextFloat() draws per sample, in source order (the JVM evaluates the `+`
// operands left-to-right so the multiplier of plateauEnd is drawn first). We drive
// it with BOTH a REAL LegacyRandomSource and a REAL XoroshiroRandomSource seeded per
// row, and the C++ test seeds the matching mc::levelgen sources IDENTICALLY.
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/TrapezoidFloatParity.java
//   java  -cp <out>;26.1.2/client.jar TrapezoidFloatParity > trapezoid_float.tsv
//
// Rows are tab-separated. Floats are emitted as raw 32-bit hex (%08x of
// Float.floatToRawIntBits) so the comparison is bit-for-bit:
//
//   TF  <rng>  <min_bits>  <max_bits>  <plateau_bits>  <seed>  <s0..s7 bits>
//        TrapezoidFloat.of(min,max,plateau).sample(rng(seed)) drawn 8 times in
//        sequence from one continuously-advancing RNG (no reseed between draws).
//        <rng> is "L" for LegacyRandomSource, "X" for XoroshiroRandomSource.
//
// O is captured at class load so any bootstrap chatter on stdout stays out of the TSV.
public class TrapezoidFloatParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) {
        return String.format("%08x", Float.floatToRawIntBits(v));
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> tfClass =
            Class.forName("net.minecraft.util.valueproviders.TrapezoidFloat");

        // TrapezoidFloat.of(float, float, float) is public static; open it anyway.
        java.lang.reflect.Method of =
            tfClass.getDeclaredMethod("of", float.class, float.class, float.class);
        of.setAccessible(true);

        // sample(RandomSource) is declared on the record; resolve by name+arity so we
        // don't depend on exact signature spelling, then open it.
        java.lang.reflect.Method sample = null;
        for (java.lang.reflect.Method m : tfClass.getDeclaredMethods()) {
            if (m.getName().equals("sample") && m.getParameterCount() == 1) {
                sample = m;
                break;
            }
        }
        sample.setAccessible(true);

        // FINITE / PHYSICAL battery. The codec validates max>=min and plateau<=(max-min),
        // and plateau>=0 in practice (a negative plateau is never produced by data). We
        // honour that here: max>min, 0<=plateau<=(max-min). Includes plateau==0 (pure
        // triangular: plateauStart==plateauEnd==range/2), plateau==full-span (rectangle:
        // plateauStart==0), and intermediate trapezoids. Values mirror real data ranges
        // (offsets/heights/scales), including negative mins.
        // Each row: {min, max, plateau}.
        float[][] cfgs = new float[][] {
            { 0.0f,   1.0f,   0.0f   },   // triangular, unit
            { 0.0f,   1.0f,   1.0f   },   // rectangle, unit (plateau == full span)
            { 0.0f,   1.0f,   0.5f   },
            { 0.0f,   10.0f,  0.0f   },
            { 0.0f,   10.0f,  4.0f   },   // matches the existing height/float gate config
            { 0.0f,   10.0f,  10.0f  },   // rectangle
            { -1.0f,  1.0f,   0.0f   },   // symmetric, triangular
            { -1.0f,  1.0f,   2.0f   },   // symmetric, rectangle
            { -1.0f,  1.0f,   0.5f   },
            { -5.0f,  5.0f,   0.0f   },
            { -5.0f,  5.0f,   3.0f   },
            { 2.0f,   8.0f,   2.0f   },
            { 3.0f,   3.5f,   0.25f  },   // tiny span
            { 100.0f, 200.0f, 50.0f  },   // large magnitudes
            { -100.0f,-50.0f, 20.0f  },   // wholly negative
            { 0.25f,  0.75f,  0.1f   },
            { 0.0f,   0.0001f,0.0f   },   // very tiny span, triangular
            { -3.5f,  7.25f,  4.0f   },
        };

        long[] seeds = { 0L, 1L, 2L, 42L, 123456789L, -1L, -987654321L,
                         2147483647L, -1234567890123456789L, 8675309L };

        for (float[] c : cfgs) {
            float min = c[0];
            float max = c[1];
            float plateau = c[2];
            Object provider = of.invoke(null, min, max, plateau);

            for (long seed : seeds) {
                // Legacy RNG row.
                {
                    net.minecraft.world.level.levelgen.LegacyRandomSource rng =
                        new net.minecraft.world.level.levelgen.LegacyRandomSource(seed);
                    StringBuilder sb = new StringBuilder();
                    sb.append("TF\tL\t").append(f(min)).append('\t').append(f(max))
                      .append('\t').append(f(plateau)).append('\t').append(seed);
                    for (int i = 0; i < 8; i++) {
                        float s = (Float) sample.invoke(provider, rng);
                        sb.append('\t').append(f(s));
                    }
                    O.println(sb.toString());
                }
                // Xoroshiro RNG row (the worldgen default source family).
                {
                    net.minecraft.world.level.levelgen.XoroshiroRandomSource rng =
                        new net.minecraft.world.level.levelgen.XoroshiroRandomSource(seed);
                    StringBuilder sb = new StringBuilder();
                    sb.append("TF\tX\t").append(f(min)).append('\t').append(f(max))
                      .append('\t').append(f(plateau)).append('\t').append(seed);
                    for (int i = 0; i < 8; i++) {
                        float s = (Float) sample.invoke(provider, rng);
                        sb.append('\t').append(f(s));
                    }
                    O.println(sb.toString());
                }
            }
        }
    }
}
