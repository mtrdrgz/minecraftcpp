// Ground-truth generator for the PURE (value-only) transform density functions of
// net.minecraft.world.level.levelgen.DensityFunctions (26.1.2). These are the unary/
// binary functions whose output depends ONLY on an input value (or on blockY for the
// y-gradient), never on noise or world state:
//
//   Mapped:        abs, square, cube, half_negative, quarter_negative, invert, squeeze
//   Clamp(min,max)
//   RangeChoice(input, minInclusive, maxExclusive, whenInRange, whenOutOfRange)
//   YClampedGradient(fromY, toY, fromValue, toValue)   -> Mth.clampedMap(blockY,...)
//   Constant(value)
//   add / mul / min / max of two Constants
//
// We drive the REAL DensityFunctions factory methods and call .compute() on a
// SinglePointContext. For the value-in transforms the "input" is a constant density
// function DensityFunctions.constant(x), so compute() funnels x straight through the
// transform code (Mapped.create does NOT constant-fold; Clamp/RangeChoice keep a real
// node). For YClampedGradient the value comes from context.blockY().
//
// Each row: <TAG>\t<inputs...>\t<outputHexBits>.  Doubles are exchanged as raw
// IEEE-754 bits (Double.doubleToRawLongBits) so the C++ gate is bit-for-bit. The C++
// test (DensityFnsParityTest) rebuilds the SAME nodes via the engine DensityFunction.h
// port and must match every bit.
//
//   mcpp/tools/run_groundtruth.ps1 -Tool DensityFnsParity -Out mcpp/build/density_fns.tsv
//
// The Mapped.Type enum is package-private, but the DensityFunction interface exposes
// public default instance methods (abs/square/cube/halfNegative/quarterNegative/
// squeeze/invert/clamp) that build the very same DensityFunctions.Mapped / Clamp nodes
// via DensityFunctions.map(...). We use those — no reflection needed.

import net.minecraft.world.level.levelgen.DensityFunction;
import net.minecraft.world.level.levelgen.DensityFunctions;

public class DensityFnsParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    // A reusable zero-coordinate context for the value-in transforms (blockX/Y/Z all 0,
    // so any non-y context dependence is irrelevant for these pure functions).
    static DensityFunction.FunctionContext ctx(int y) {
        return new DensityFunction.SinglePointContext(0, y, 0);
    }
    static final DensityFunction.FunctionContext ZERO_CTX = ctx(0);

    static double compute(DensityFunction f, int y) {
        return f.compute(ctx(y));
    }

    // Set the Log4j root level to OFF reflectively (Configurator.setRootLevel(Level.OFF))
    // so DensityFunctions' "non-overlapping inputs" WARNs never reach stdout/stderr and
    // can't corrupt the TSV. No-op if log4j-core isn't on the classpath.
    static void silenceLog4j() {
        try {
            Class<?> levelCls = Class.forName("org.apache.logging.log4j.Level");
            Object off = levelCls.getField("OFF").get(null);
            Class<?> cfg = Class.forName("org.apache.logging.log4j.core.config.Configurator");
            cfg.getMethod("setRootLevel", levelCls).invoke(null, off);
        } catch (Throwable ignored) {
            // best-effort; the C++ test also skips any non-tab-tagged line.
        }
    }

    // Apply the named Mapped transform to a constant-value input via the public default
    // instance methods (each delegates to DensityFunctions.map(this, Mapped.Type.<NAME>)).
    static DensityFunction mapped(String name, double v) {
        DensityFunction in = DensityFunctions.constant(v);
        switch (name) {
            case "abs":              return in.abs();
            case "square":           return in.square();
            case "cube":             return in.cube();
            case "half_negative":    return in.halfNegative();
            case "quarter_negative": return in.quarterNegative();
            case "squeeze":          return in.squeeze();
            case "invert":           return in.invert();
            default: throw new IllegalArgumentException("unknown mapped: " + name);
        }
    }

    public static void main(String[] args) throws Exception {
        // DensityFunctions.<clinit> builds a registry-dispatch CODEC, so it requires the
        // game to be bootstrapped. Bootstrap installs a Log4j redirect on System.out that
        // prefixes every line — AND TwoArgumentSimpleFunction.create logs a WARN through
        // Log4j for two non-overlapping constants (every two-constant MIN/MAX). To keep
        // the TSV pristine we (1) capture the real stdout BEFORE bootStrap and emit the
        // TSV only through it, and (2) silence Log4j (root level OFF) before any WARN can
        // fire (reflective, so the tool needs no compile-time log4j dependency).
        final java.io.PrintStream out = System.out;
        silenceLog4j();
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        silenceLog4j();

        // --- input sweeps -------------------------------------------------------
        // Finite, physical density-ish values plus boundaries for the clamps/squeeze.
        double[] vals = {
            0.0, 1.0, -1.0, 0.5, -0.5, 0.25, -0.25, 0.75, -0.75,
            2.0, -2.0, 3.5, -3.5, 0.3333333333333333, -0.6666666666666666,
            0.9999999999, -0.9999999999, 1.0000001, -1.0000001,
            123.456, -123.456, 1000000.0, -1000000.0, 0.0001, -0.0001,
            0.6666666666666666, 0.3333333333333333, 7.0, -7.0, 42.0, -42.0,
        };

        // Mapped transforms (value-in). invert uses only non-zero inputs (1/0 = Inf).
        String[] mappedNames = { "abs", "square", "cube", "half_negative", "quarter_negative", "squeeze", "invert" };
        for (String name : mappedNames) {
            for (double v : vals) {
                if (name.equals("invert") && v == 0.0) continue; // 1/0 = Infinity (skip per finite-only rule)
                DensityFunction f = mapped(name, v);
                out.println("MAP_" + name.toUpperCase() + "\t" + d(v) + "\t" + d(compute(f, 0)));
            }
        }

        // Clamp(min, max): value-in, clamped to [min,max].
        double[][] clampRanges = {
            {-1.0, 1.0}, {0.0, 1.0}, {-2.0, 2.0}, {-0.5, 0.5}, {-100.0, 100.0},
            {-1000000.0, 1000000.0}, {0.25, 0.75}, {-3.5, 3.5},
        };
        for (double[] r : clampRanges) {
            for (double v : vals) {
                DensityFunction f = DensityFunctions.constant(v).clamp(r[0], r[1]);
                out.println("CLAMP\t" + d(v) + "\t" + d(r[0]) + "\t" + d(r[1]) + "\t" + d(compute(f, 0)));
            }
        }

        // RangeChoice(input, minIncl, maxExcl, inRange, outOfRange): selects inRange when
        // minIncl <= input < maxExcl, else outOfRange. Use distinct constants so the
        // branch is observable. inRange=7.0, outOfRange=-9.0.
        double[][] rcRanges = {
            {-1.0, 1.0}, {0.0, 1.0}, {-0.5, 0.5}, {0.25, 0.75}, {-2.0, 2.0},
        };
        for (double[] r : rcRanges) {
            for (double v : vals) {
                DensityFunction f = DensityFunctions.rangeChoice(
                    DensityFunctions.constant(v), r[0], r[1],
                    DensityFunctions.constant(7.0), DensityFunctions.constant(-9.0));
                out.println("RANGE\t" + d(v) + "\t" + d(r[0]) + "\t" + d(r[1]) + "\t" + d(compute(f, 0)));
            }
        }

        // YClampedGradient(fromY, toY, fromValue, toValue) -> Mth.clampedMap(blockY,...).
        // Sweep blockY across, below and above [fromY,toY]; include boundary Y == fromY
        // and Y == toY AND y strictly past toY/before fromY (the clamped regions where
        // clampedLerp short-circuits to the endpoint EXACTLY — a clamp-t-then-lerp form
        // returns the endpoint +/- 1 ULP, the discrepancy this row family certifies).
        // (fromY != toY only; fromY==toY makes Mth.clampedMap divide by zero -> NaN at
        // y==fromY, a degenerate/non-physical config out of scope for a finite gate.)
        int[][] yGradConfigs = {
            {0, 128}, {-64, 320}, {-64, 0}, {0, 256}, {10, 20}, {-32, 32}, {64, 192},
        };
        // fromValue/toValue pairs chosen so the lerp endpoint does NOT reconstruct
        // exactly (0.3+(0.9-0.3)=0.9000000000000001 != 0.9) — proves the endpoint
        // short-circuit, not a coincidental round-trip.
        double[][] yGradValues = {
            {0.0, 1.0}, {1.0, 0.0}, {-1.0, 1.0}, {0.3, 0.9}, {0.9, 0.3},
            {-0.5, 0.5}, {0.0, 16.0}, {1.5, -2.5},
        };
        for (int[] cfg : yGradConfigs) {
            int fromY = cfg[0], toY = cfg[1];
            int[] ySweep = {
                fromY - 100, fromY - 1, fromY, fromY + 1,
                fromY + (toY - fromY) / 4, (fromY + toY) / 2, fromY + 3 * (toY - fromY) / 4,
                toY - 1, toY, toY + 1, toY + 100,
            };
            for (double[] vv : yGradValues) {
                DensityFunction f = DensityFunctions.yClampedGradient(fromY, toY, vv[0], vv[1]);
                for (int y : ySweep) {
                    out.println("YGRAD\t" + fromY + "\t" + toY + "\t" + d(vv[0]) + "\t" + d(vv[1])
                            + "\t" + y + "\t" + d(compute(f, y)));
                }
            }
        }

        // Constant(value).
        for (double v : vals) {
            DensityFunction f = DensityFunctions.constant(v);
            out.println("CONST\t" + d(v) + "\t" + d(compute(f, 0)));
        }

        // add / mul / min / max of two constants. Avoid the 0.0*negative -0.0 sign edge
        // for MUL by skipping pairs where a==0 and b<0 (and vice versa); those diverge
        // only in the sign bit of zero and are out of scope per the finite-only rule.
        double[] aVals = { 0.0, 1.0, -1.0, 0.5, -0.5, 2.0, -2.0, 0.25, -0.75, 7.0, -7.0, 123.456 };
        double[] bVals = { 0.0, 1.0, -1.0, 0.5, -0.5, 2.0, -2.0, 0.25, -0.75, 9.0, -9.0, -42.0 };
        for (double a : aVals) {
            for (double b : bVals) {
                DensityFunction ca = DensityFunctions.constant(a);
                DensityFunction cb = DensityFunctions.constant(b);
                out.println("ADD\t" + d(a) + "\t" + d(b) + "\t" + d(DensityFunctions.add(ca, cb).compute(ZERO_CTX)));
                boolean mulZeroSign = (a == 0.0 && b < 0.0) || (b == 0.0 && a < 0.0);
                if (!mulZeroSign) {
                    out.println("MUL\t" + d(a) + "\t" + d(b) + "\t" + d(DensityFunctions.mul(ca, cb).compute(ZERO_CTX)));
                }
                out.println("MIN\t" + d(a) + "\t" + d(b) + "\t" + d(DensityFunctions.min(ca, cb).compute(ZERO_CTX)));
                out.println("MAX\t" + d(a) + "\t" + d(b) + "\t" + d(DensityFunctions.max(ca, cb).compute(ZERO_CTX)));
            }
        }
    }
}
