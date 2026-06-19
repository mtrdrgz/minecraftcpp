// Ground-truth generator for net.minecraft.world.item.enchantment.LevelBasedValue
// (26.1.2), using the REAL decompiled records. Every implementation's
// `float calculate(int level)` is pure (no RandomSource / world), so we drive
// each record directly and emit the int level plus the resulting float as a raw
// IEEE-754 bit pattern. The C++ test (LevelBasedValueParityTest) rebuilds the
// identical node tree and must match BIT-FOR-BIT.
//
//   tools/run_groundtruth.ps1 -Tool LevelBasedValueParity -Out mcpp/build/level_based_value.tsv
//
// Row format:  <TAG>\t<level:int>\t<result:float-bits>
// where <TAG> identifies which LevelBasedValue instance produced the row (the
// C++ side constructs the matching tree per TAG). Bootstrap is run only to be
// safe against static init in Mth/registries; calculate() itself touches no
// registry.

import java.util.List;
import net.minecraft.world.item.enchantment.LevelBasedValue;

public class LevelBasedValueParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static void row(String tag, LevelBasedValue v, int level) {
        O.println(tag + "\t" + level + "\t" + f(v.calculate(level)));
    }

    // Sweep a battery of int levels per instance: the natural enchantment range,
    // boundary values (0, 1), negatives (Linear/LevelsSquared do signed int math),
    // and large magnitudes to exercise int overflow in Mth.square / (level-1).
    static final int[] LEVELS = {
        Integer.MIN_VALUE, -1000000, -100, -10, -3, -2, -1, 0, 1, 2, 3, 4, 5,
        6, 7, 8, 9, 10, 15, 20, 50, 100, 255, 256, 1000, 46340, 46341, 65535,
        65536, 100000, 1000000, 0x40000000, Integer.MAX_VALUE
    };

    static void sweep(String tag, LevelBasedValue v) {
        for (int lvl : LEVELS) row(tag, v, lvl);
    }

    // Lookup.calculate does `level <= values.size() ? values.get(level - 1) : ...`,
    // so for level <= 0 (or any level in [MIN, size] whose level-1 is out of
    // [0,size)) the REAL Java throws ArrayIndexOutOfBoundsException. That path is
    // outside Lookup's contract (enchant levels are >= 1) and is UB on both sides,
    // so it is not a meaningful byte-parity comparison. Sweep Lookup over its
    // valid domain only (level >= 1).
    static final int[] LOOKUP_LEVELS = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 20, 50, 100, 255, 256, 1000,
        46340, 46341, 65535, 65536, 100000, 1000000, 0x40000000, Integer.MAX_VALUE
    };

    static void sweepLookup(String tag, LevelBasedValue v) {
        for (int lvl : LOOKUP_LEVELS) row(tag, v, lvl);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // --- Constant ---
        sweep("CONST_0", LevelBasedValue.constant(0.0F));
        sweep("CONST_2_5", LevelBasedValue.constant(2.5F));
        sweep("CONST_NEG", LevelBasedValue.constant(-7.25F));
        sweep("CONST_FRAC", LevelBasedValue.constant(0.1F));

        // --- Linear: base + perLevelAboveFirst*(level-1) ---
        sweep("LIN_1_1", LevelBasedValue.perLevel(1.0F));            // perLevel(p,p)
        sweep("LIN_2_3", LevelBasedValue.perLevel(2.0F, 3.0F));
        sweep("LIN_5_05", LevelBasedValue.perLevel(5.0F, 0.5F));
        sweep("LIN_NEG", LevelBasedValue.perLevel(-3.0F, -1.5F));
        sweep("LIN_BIG", LevelBasedValue.perLevel(0.0F, 100000.0F)); // int (level-1) * big

        // --- LevelsSquared: Mth.square(level)[int] + added ---
        sweep("SQ_0", new LevelBasedValue.LevelsSquared(0.0F));
        sweep("SQ_1_5", new LevelBasedValue.LevelsSquared(1.5F));
        sweep("SQ_NEG", new LevelBasedValue.LevelsSquared(-2.0F));

        // --- Clamped: Mth.clamp(inner.calc, min, max) ---
        sweep("CLAMP_LIN", new LevelBasedValue.Clamped(LevelBasedValue.perLevel(2.0F, 3.0F), 1.0F, 20.0F));
        sweep("CLAMP_SQ", new LevelBasedValue.Clamped(new LevelBasedValue.LevelsSquared(0.0F), -5.0F, 50.0F));
        sweep("CLAMP_CONST", new LevelBasedValue.Clamped(LevelBasedValue.constant(100.0F), -10.0F, 10.0F));
        // min>max is rejected by the codec validator but calculate() still runs it;
        // value<min path wins -> always min. Test the raw arithmetic.
        sweep("CLAMP_INVERT", new LevelBasedValue.Clamped(LevelBasedValue.perLevel(1.0F), 10.0F, 0.0F));

        // --- Fraction: den==0 ? 0 : num/den ---
        sweep("FRAC_LIN", new LevelBasedValue.Fraction(LevelBasedValue.perLevel(10.0F, 1.0F), LevelBasedValue.perLevel(1.0F)));
        sweep("FRAC_ZERODEN", new LevelBasedValue.Fraction(LevelBasedValue.constant(5.0F), new LevelBasedValue.LevelsSquared(0.0F)));
        sweep("FRAC_CONST", new LevelBasedValue.Fraction(LevelBasedValue.constant(1.0F), LevelBasedValue.constant(3.0F)));
        sweep("FRAC_NEG", new LevelBasedValue.Fraction(LevelBasedValue.constant(-1.0F), LevelBasedValue.perLevel(2.0F, -1.0F)));

        // --- Exponent: (float)Math.pow(base.calc, power.calc) ---
        sweep("EXP_2_LIN", new LevelBasedValue.Exponent(LevelBasedValue.constant(2.0F), LevelBasedValue.perLevel(0.0F, 1.0F)));
        sweep("EXP_LIN_2", new LevelBasedValue.Exponent(LevelBasedValue.perLevel(1.0F), LevelBasedValue.constant(2.0F)));
        sweep("EXP_HALF", new LevelBasedValue.Exponent(new LevelBasedValue.LevelsSquared(0.0F), LevelBasedValue.constant(0.5F)));
        sweep("EXP_NEGBASE", new LevelBasedValue.Exponent(LevelBasedValue.constant(-2.0F), LevelBasedValue.perLevel(0.0F, 1.0F)));

        // --- Lookup: level<=size ? values[level-1] : fallback.calc ---
        sweepLookup("LOOK_3", LevelBasedValue.lookup(List.of(1.0F, 4.0F, 9.0F), LevelBasedValue.constant(99.0F)));
        sweepLookup("LOOK_FALLLIN", LevelBasedValue.lookup(List.of(0.5F, 1.5F), LevelBasedValue.perLevel(2.0F, 1.0F)));
        sweepLookup("LOOK_1", LevelBasedValue.lookup(List.of(7.0F), LevelBasedValue.constant(0.0F)));

        // --- Nested composite to exercise tree composition ---
        // clamp( fraction( squared, linear ), 0, 100 )
        LevelBasedValue nested = new LevelBasedValue.Clamped(
            new LevelBasedValue.Fraction(
                new LevelBasedValue.LevelsSquared(2.0F),
                LevelBasedValue.perLevel(1.0F, 1.0F)),
            0.0F, 100.0F);
        sweep("NEST_CFSL", nested);

        O.flush();
    }
}
