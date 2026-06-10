// Ground-truth generator for net.minecraft.util.CubicSpline (26.1.2) — the worldgen
// spline. We drive apply(float) directly: the coordinate `I` is an IDENTITY
// BoundedFloatFunction<Float> (apply(x)->x), so CubicSpline.apply(Float) evaluates
// the spline at the raw float we pass. We build several fixed splines (Constant +
// hand-chosen Multipoints, including a NESTED multipoint whose child value is itself
// a multipoint, and derivatives both zero and non-zero), then sweep a thorough
// battery of float coordinates. Each row: <TAG>\t<coordHexBits>\t<resultHexBits>,
// where TAG selects which spline. The C++ test (CubicSplineParityTest) rebuilds the
// SAME splines via CubicSplineFloat.h and must match bit-for-bit.
//
// Floats are exchanged as raw IEEE-754 bits (Float.floatToRawIntBits).
//
//   tools/run_groundtruth.ps1 -Tool CubicSplineParity -Out mcpp/build/cubic_spline.tsv

import net.minecraft.util.BoundedFloatFunction;
import net.minecraft.util.CubicSpline;

public class CubicSplineParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // IDENTITY coordinate: apply(x) -> x, so CubicSpline.apply(Float) uses the raw
    // float we pass as the spline input. Matches BoundedFloatFunction.IDENTITY but we
    // build a fresh one to be explicit.
    static final BoundedFloatFunction<Float> COORD = BoundedFloatFunction.createUnlimited(x -> x);

    // ---- Fixed splines, all coordinate = COORD --------------------------------

    // K: a Constant spline (apply ignores coordinate, returns the constant).
    static CubicSpline<Float, BoundedFloatFunction<Float>> splineConst() {
        return CubicSpline.constant(7.5f);
    }

    // M1: a flat-derivative multipoint (all derivatives 0). Pure piecewise-linear
    // blend collapses to Mth.lerp with a=b=0 in interior; ends linearExtend with d=0.
    static CubicSpline<Float, BoundedFloatFunction<Float>> splineFlat() {
        return CubicSpline.builder(COORD)
            .addPoint(-1.0f, -2.0f)
            .addPoint(0.0f, 0.0f)
            .addPoint(0.5f, 4.0f)
            .addPoint(2.0f, 1.0f)
            .build();
    }

    // M2: a multipoint with NON-ZERO derivatives (true Hermite interior + sloped
    // linear extension past both ends).
    static CubicSpline<Float, BoundedFloatFunction<Float>> splineDeriv() {
        return CubicSpline.builder(COORD)
            .addPoint(-3.0f, 5.0f, -1.5f)
            .addPoint(-0.25f, -1.0f, 0.0f)
            .addPoint(1.0f, 2.5f, 3.0f)
            .addPoint(4.0f, -6.0f, -0.75f)
            .build();
    }

    // M3: a single-point multipoint (start == lastIndex == 0 for input >= loc, and
    // start < 0 below it) with a non-zero derivative -> both linearExtend branches.
    static CubicSpline<Float, BoundedFloatFunction<Float>> splineSingle() {
        return CubicSpline.builder(COORD)
            .addPoint(0.0f, 1.0f, 2.0f)
            .build();
    }

    // M4: a NESTED multipoint — one of the value points is itself a Multipoint spline
    // (sharing COORD), the others are constants, with mixed derivatives. Exercises the
    // recursive f.apply(c) path.
    static CubicSpline<Float, BoundedFloatFunction<Float>> splineNested() {
        CubicSpline<Float, BoundedFloatFunction<Float>> inner = CubicSpline.builder(COORD)
            .addPoint(-2.0f, 10.0f, 1.0f)
            .addPoint(3.0f, -4.0f, -2.0f)
            .build();
        // NOTE: addPoint(location, sampler) is the only PUBLIC overload that takes a
        // child spline, and it fixes the derivative to 0.0f (the private 3-arg overload
        // that allows a non-zero derivative on a sampler is not callable from here). So
        // the nested point's derivative is 0; the C++ test mirrors this exactly.
        return CubicSpline.builder(COORD)
            .addPoint(-5.0f, 0.5f, 0.5f)
            .addPoint(0.0f, inner)
            .addPoint(6.0f, -3.0f, 0.0f)
            .build();
    }

    static void sweep(String tag, CubicSpline<Float, BoundedFloatFunction<Float>> s) {
        // Representative + edge coordinates.
        float[] pts = {
            Float.NEGATIVE_INFINITY, -1.0e30f, -100.0f, -10.0f, -5.0f, -4.5f, -3.0f, -2.5f,
            -2.0f, -1.5f, -1.0f, -0.75f, -0.5f, -0.25f, -0.125f, -0.0f, 0.0f, 0.125f, 0.25f,
            0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 10.0f, 100.0f,
            1.0e30f, Float.POSITIVE_INFINITY
        };
        for (float x : pts) O.println(tag + "\t" + f(x) + "\t" + f(s.apply(x)));
        // Fine sweep across the interesting domain [-6, 7].
        for (int i = -6000; i <= 7000; i += 13) {
            float x = i / 1000.0f;
            O.println(tag + "\t" + f(x) + "\t" + f(s.apply(x)));
        }
    }

    public static void main(String[] args) throws Exception {
        CubicSpline<Float, BoundedFloatFunction<Float>> k = splineConst();
        CubicSpline<Float, BoundedFloatFunction<Float>> flat = splineFlat();
        CubicSpline<Float, BoundedFloatFunction<Float>> deriv = splineDeriv();
        CubicSpline<Float, BoundedFloatFunction<Float>> single = splineSingle();
        CubicSpline<Float, BoundedFloatFunction<Float>> nested = splineNested();

        sweep("K", k);
        sweep("FLAT", flat);
        sweep("DERIV", deriv);
        sweep("SINGLE", single);
        sweep("NESTED", nested);
    }
}
