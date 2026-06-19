// Ground-truth generator for the C++ mc::biome::ClimateMath port. Drives the REAL
// decompiled net.minecraft.world.level.biome.Climate from the jar, so every emitted
// value is bit-exact vanilla.
//
//   mcpp/tools/run_groundtruth.ps1 -Tool ClimateMathParity -Out mcpp/build/climate_math.tsv
//
// Covers ONLY the pure, RandomSource-free, registry-free primitives:
//   * Climate.quantizeCoord(float)      (public static)
//   * Climate.unquantizeCoord(long)     (public static)
//   * Climate.target(...)               (public static -> TargetPoint record)
//   * Climate.Parameter.point/span/distance  (public)
//   * Climate.ParameterPoint.fitness    (PRIVATE -> reflection)
//   * Climate.parameters(...)           (public static)
//
// Row formats (TAB-separated; floats as 8-hex raw IEEE bits via floatToRawIntBits,
// longs decimal so the C++ compare is exact):
//   Q    <coordBits>                         <long quantizeCoord>
//   U    <long coord>                         <float bits unquantizeCoord>
//   T    <tBits> <hBits> <cBits> <eBits> <dBits> <wBits>   <t> <h> <c> <e> <d> <w>   (6 longs)
//   PD   <pMin> <pMax> <targetLong>           <long Parameter.distance(long)>
//   PDP  <aMin> <aMax> <bMin> <bMax>          <long Parameter.distance(Parameter)>
//   PT   <minBits> <maxBits>                  <pMin> <pMax>     (Parameter.span(float,float))
//   FIT  <pMin..wMax (12 longs)> <offsetLong> <tt th tc te td tw (6 target longs)>  <long fitness>
import java.io.PrintStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

import net.minecraft.world.level.biome.Climate;

public class ClimateMathParity {
    static final PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // The float coords we feed quantizeCoord / target / Parameter.span. These exercise
    // the truncation-toward-zero trap: values whose *10000 product lands just under/over
    // an integer, exact halves, negatives, and the full vanilla [-2,2] climate range and
    // a little beyond.
    static final float[] COORDS = {
        -2.0f, -1.99995f, -1.5f, -1.00005f, -1.0f, -0.99999f, -0.75f, -0.50005f, -0.5f,
        -0.30003f, -0.12345f, -0.00010001f, -0.00005f, -0.00001f, 0.0f,
        0.00001f, 0.00005f, 0.00010001f, 0.0001f, 0.12345f, 0.15f, 0.2f, 0.30003f,
        0.5f, 0.50005f, 0.55f, 0.75f, 0.9999f, 1.0f, 1.00005f, 1.5f, 1.99995f, 2.0f,
        // sub-ULP-of-1/10000 boundaries to catch float*float rounding before the cast:
        0.123449f, 0.123451f, -0.123449f, -0.123451f, 0.99995f, -0.99995f
    };

    // Longs fed to unquantizeCoord, including |coord| >= 2^24 where (float)coord loses
    // precision before the divide.
    static final long[] LONGS = {
        0L, 1L, -1L, 5000L, -5000L, 9999L, 10000L, -10000L, 12345L, -12345L,
        20000L, -20000L, 16777215L, 16777216L, 16777217L, -16777217L,
        100000000L, -100000000L, 1234567890L, -1234567890L,
        Long.MAX_VALUE, Long.MIN_VALUE, 33554431L, 33554433L
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // ---- Climate.ParameterPoint.fitness is private: reflect it. ----
        Method fitness = null;
        for (Method m : Climate.ParameterPoint.class.getDeclaredMethods()) {
            if (m.getName().equals("fitness")) { fitness = m; break; }
        }
        if (fitness == null) throw new IllegalStateException("fitness method not found");
        fitness.setAccessible(true);

        // ---- Q: quantizeCoord ----
        for (float c : COORDS) {
            long q = Climate.quantizeCoord(c);
            O.println("Q\t" + f(c) + "\t" + q);
        }

        // ---- U: unquantizeCoord ----
        for (long l : LONGS) {
            float u = Climate.unquantizeCoord(l);
            O.println("U\t" + l + "\t" + f(u));
        }

        // ---- T: target(...) -> TargetPoint (6 quantized longs) ----
        // Sweep 6-tuples drawn from COORDS at strided offsets so each field differs.
        int n = COORDS.length;
        for (int i = 0; i < n; i++) {
            float t = COORDS[i];
            float h = COORDS[(i + 5) % n];
            float co = COORDS[(i + 11) % n];
            float e = COORDS[(i + 17) % n];
            float d = COORDS[(i + 23) % n];
            float w = COORDS[(i + 29) % n];
            Climate.TargetPoint tp = Climate.target(t, h, co, e, d, w);
            O.println("T\t" + f(t) + "\t" + f(h) + "\t" + f(co) + "\t" + f(e) + "\t" + f(d) + "\t" + f(w)
                + "\t" + tp.temperature() + "\t" + tp.humidity() + "\t" + tp.continentalness()
                + "\t" + tp.erosion() + "\t" + tp.depth() + "\t" + tp.weirdness());
        }

        // ---- PT: Parameter.span(float,float) construction (point() = span(v,v)) ----
        // COORDS is sorted ascending, so pairing index i with a later index j>=i keeps
        // mn<=mx (Parameter.span throws when min>max).
        for (int i = 0; i < n; i++) {
            float mn = COORDS[i];
            int j = Math.min(n - 1, i + 4);
            float mx = COORDS[j];
            if (mn > mx) { float tmp = mn; mn = mx; mx = tmp; } // defensive (COORDS not strictly sorted past index 32)
            Climate.Parameter p = Climate.Parameter.span(mn, mx);
            O.println("PT\t" + f(mn) + "\t" + f(mx) + "\t" + p.min() + "\t" + p.max());
            // point():
            Climate.Parameter pp = Climate.Parameter.point(COORDS[i]);
            O.println("PT\t" + f(COORDS[i]) + "\t" + f(COORDS[i]) + "\t" + pp.min() + "\t" + pp.max());
        }

        // ---- PD: Parameter.distance(long) ----
        // Build representative quantized intervals and probe targets inside/below/above,
        // including extreme longs to exercise the signed-subtraction branch.
        long[][] params = {
            {0L, 0L}, {-10000L, 10000L}, {1234L, 5678L}, {-5000L, -1000L},
            {5000L, 5000L}, {-20000L, -19999L}, {19999L, 20000L},
            {Long.MIN_VALUE / 2, Long.MAX_VALUE / 2}
        };
        long[] targets = {
            0L, -1L, 1L, 1234L, 5678L, -10000L, 10000L, -10001L, 10001L,
            5000L, -5000L, 100000L, -100000L, Long.MAX_VALUE, Long.MIN_VALUE,
            19999L, 20000L, -19999L, -20000L
        };
        for (long[] pr : params) {
            Climate.Parameter p = makeParameter(pr[0], pr[1]);
            for (long tg : targets) {
                long dd = p.distance(tg);
                O.println("PD\t" + pr[0] + "\t" + pr[1] + "\t" + tg + "\t" + dd);
            }
        }

        // ---- PDP: Parameter.distance(Parameter) ----
        for (long[] a : params) {
            Climate.Parameter pa = makeParameter(a[0], a[1]);
            for (long[] b : params) {
                Climate.Parameter pb = makeParameter(b[0], b[1]);
                long dd = pa.distance(pb);
                O.println("PDP\t" + a[0] + "\t" + a[1] + "\t" + b[0] + "\t" + b[1] + "\t" + dd);
            }
        }

        // ---- FIT: ParameterPoint.fitness(TargetPoint) ----
        // Use real-ish quantized parameter points plus deliberately huge ones so the
        // squared-distance sum overflows int64 (must WRAP, not saturate).
        long[][] ppDefs = {
            // {tMin,tMax, hMin,hMax, cMin,cMax, eMin,eMax, dMin,dMax, wMin,wMax, offset}
            {0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0},
            {-3000,3000, -1000,5000, -10000,0, 0,10000, -2000,2000, -500,500, 1000},
            {-15000,15000, -15000,15000, -15000,15000, -15000,15000, -15000,15000, -15000,15000, 0},
            // Huge magnitudes -> distances ~1e9, squares ~1e18, sum overflows long:
            {1000000000L,1000000000L, 1000000000L,1000000000L, 1000000000L,1000000000L,
             1000000000L,1000000000L, 1000000000L,1000000000L, 1000000000L,1000000000L, 1000000000L},
            {3037000499L,3037000499L, 0,0, 0,0, 0,0, 0,0, 0,0, 0}, // ~sqrt(Long.MAX): single-term overflow edge
        };
        long[][] tgtDefs = {
            {0,0,0,0,0,0},
            {1000,-1000,5000,-5000,2000,-2000},
            {-15000,15000,-15000,15000,-15000,15000},
            {0,0,0,0,0,0},
            {-1000000000L,1000000000L,-1000000000L,1000000000L,-1000000000L,1000000000L},
        };
        for (long[] pp : ppDefs) {
            Object point = makeParameterPoint(pp);
            for (long[] tg : tgtDefs) {
                Climate.TargetPoint t = makeTargetPoint(tg);
                long fit = (Long) fitness.invoke(point, t);
                StringBuilder sb = new StringBuilder("FIT");
                for (long v : pp) sb.append('\t').append(v);
                for (long v : tg) sb.append('\t').append(v);
                sb.append('\t').append(fit);
                O.println(sb.toString());
            }
        }
    }

    // Build a Climate.Parameter directly from two longs (canonical record constructor).
    static Climate.Parameter makeParameter(long min, long max) throws Exception {
        Constructor<Climate.Parameter> ctor = Climate.Parameter.class.getDeclaredConstructor(long.class, long.class);
        ctor.setAccessible(true);
        return ctor.newInstance(min, max);
    }

    static Climate.TargetPoint makeTargetPoint(long[] v) throws Exception {
        Constructor<Climate.TargetPoint> ctor = Climate.TargetPoint.class.getDeclaredConstructor(
            long.class, long.class, long.class, long.class, long.class, long.class);
        ctor.setAccessible(true);
        return ctor.newInstance(v[0], v[1], v[2], v[3], v[4], v[5]);
    }

    // ppDef = {tMin,tMax, hMin,hMax, cMin,cMax, eMin,eMax, dMin,dMax, wMin,wMax, offset}
    static Object makeParameterPoint(long[] pp) throws Exception {
        Constructor<Climate.ParameterPoint> ctor = Climate.ParameterPoint.class.getDeclaredConstructor(
            Climate.Parameter.class, Climate.Parameter.class, Climate.Parameter.class,
            Climate.Parameter.class, Climate.Parameter.class, Climate.Parameter.class, long.class);
        ctor.setAccessible(true);
        return ctor.newInstance(
            makeParameter(pp[0], pp[1]), makeParameter(pp[2], pp[3]), makeParameter(pp[4], pp[5]),
            makeParameter(pp[6], pp[7]), makeParameter(pp[8], pp[9]), makeParameter(pp[10], pp[11]),
            pp[12]);
    }
}
