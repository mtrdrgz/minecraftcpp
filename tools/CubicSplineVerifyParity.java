// Ground-truth generator for net.minecraft.util.CubicSpline (26.1.2) — VERIFY gate
// for the engine's C++ port (mcpp/src/world/level/levelgen/CubicSpline.cpp/.h, the
// DensityFunction-coupled implementation), as distinct from the standalone
// CubicSplineFloat.h gate (cubic_spline_parity).
//
// We drive the REAL net.minecraft.util.CubicSpline.apply(C) directly. The coordinate
// `I` is a BoundedFloatFunction<Float> IDENTITY (apply(x)->x, min=-inf, max=+inf), so
// CubicSpline.apply(Float) evaluates the spline at the raw float we pass. We build a
// battery of fixed splines exercising every apply() branch:
//   - Constant.apply                         (returns the constant, coordinate ignored)
//   - Multipoint.apply start<0  (below first location) -> linearExtend at index 0
//   - Multipoint.apply start==lastIndex (above last) -> linearExtend at lastIndex
//   - Multipoint.apply interior -> Hermite blend Mth.lerp(t,y1,y2)+t*(1-t)*Mth.lerp(t,a,b)
//   - zero AND non-zero derivatives (linearExtend's d==0 vs d!=0 branches)
//   - a single-point Multipoint (start<0 below, start==lastIndex==0 at/above)
//   - a NESTED Multipoint whose child value is itself a Multipoint (recursive apply)
// then sweep a thorough, FINITE/PHYSICAL battery of float coordinates (plus the
// edge +/-inf / +/-large floats, which apply() handles deterministically).
//
// Each row: <TAG>\t<coordHexBits>\t<resultHexBits>, where TAG selects which spline.
// Floats are exchanged as raw IEEE-754 bits (Float.floatToRawIntBits) so the C++
// gate is bit-for-bit. The C++ test (CubicSplineVerifyParityTest) rebuilds the SAME
// splines via the engine's CubicSpline::builder/constant and must match every row.
//
//   tools/run_groundtruth.ps1 -Tool CubicSplineVerifyParity -Out mcpp/build/cubic_spline_verify.tsv

import net.minecraft.util.BoundedFloatFunction;
import net.minecraft.util.CubicSpline;

public class CubicSplineVerifyParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // IDENTITY coordinate: apply(x) -> x (minValue=-inf, maxValue=+inf). This is exactly
    // BoundedFloatFunction.IDENTITY; we build a fresh unlimited identity to be explicit.
    static final BoundedFloatFunction<Float> COORD = BoundedFloatFunction.createUnlimited(x -> x);

    // ---- Fixed splines, all coordinate = COORD --------------------------------

    // K: a Constant spline (apply ignores the coordinate, returns the constant).
    static CubicSpline<Float, BoundedFloatFunction<Float>> splineConst() {
        return CubicSpline.constant(3.25f);
    }

    // FLAT: a multipoint with ALL-ZERO derivatives. Interior collapses to Mth.lerp
    // (a=b=0); both ends linearExtend with derivative 0 -> flat clamp to the edge value.
    static CubicSpline<Float, BoundedFloatFunction<Float>> splineFlat() {
        return CubicSpline.builder(COORD)
            .addPoint(-4.0f, 7.0f)
            .addPoint(-1.0f, -2.0f)
            .addPoint(1.5f, 3.0f)
            .addPoint(5.0f, -5.0f)
            .build();
    }

    // DERIV: a multipoint with NON-ZERO derivatives (true Hermite interior + sloped
    // linear extension past both ends).
    static CubicSpline<Float, BoundedFloatFunction<Float>> splineDeriv() {
        return CubicSpline.builder(COORD)
            .addPoint(-3.0f, 4.0f, -1.25f)
            .addPoint(-0.5f, -2.0f, 0.5f)
            .addPoint(2.0f, 6.0f, -2.0f)
            .addPoint(4.5f, -1.0f, 1.75f)
            .build();
    }

    // MIXED: alternating zero / non-zero derivatives, so adjacent intervals mix the
    // two linearExtend branches and Hermite coefficients with a zero endpoint slope.
    static CubicSpline<Float, BoundedFloatFunction<Float>> splineMixed() {
        return CubicSpline.builder(COORD)
            .addPoint(-2.0f, 1.0f, 0.0f)
            .addPoint(0.0f, 5.0f, 2.5f)
            .addPoint(3.0f, -3.0f, 0.0f)
            .addPoint(6.0f, 2.0f, -1.0f)
            .build();
    }

    // SINGLE: a single-point multipoint with a NON-ZERO derivative. For input < 0 the
    // start<0 branch fires (linearExtend at 0); for input >= 0, start==lastIndex==0
    // also fires linearExtend at 0 — both branches, slope applied either side.
    static CubicSpline<Float, BoundedFloatFunction<Float>> splineSingle() {
        return CubicSpline.builder(COORD)
            .addPoint(0.0f, 1.0f, 2.0f)
            .build();
    }

    // SINGLE0: a single-point multipoint with ZERO derivative -> constant everywhere
    // (linearExtend returns value unchanged on both sides).
    static CubicSpline<Float, BoundedFloatFunction<Float>> splineSingle0() {
        return CubicSpline.builder(COORD)
            .addPoint(2.5f, -4.0f, 0.0f)
            .build();
    }

    // NESTED: a multipoint one of whose value points is ITSELF a Multipoint (sharing
    // COORD), exercising the recursive f.apply(c) path inside the interior blend. The
    // public addPoint(location, sampler) overload fixes that child point's derivative
    // to 0.0f (the private 3-arg sampler overload is not callable here) — the C++ test
    // mirrors this exactly.
    static CubicSpline<Float, BoundedFloatFunction<Float>> splineNested() {
        CubicSpline<Float, BoundedFloatFunction<Float>> inner = CubicSpline.builder(COORD)
            .addPoint(-2.0f, 10.0f, 1.0f)
            .addPoint(3.0f, -4.0f, -2.0f)
            .build();
        return CubicSpline.builder(COORD)
            .addPoint(-5.0f, 0.5f, 0.5f)
            .addPoint(0.0f, inner)
            .addPoint(6.0f, -3.0f, 0.0f)
            .build();
    }

    static void sweep(String tag, CubicSpline<Float, BoundedFloatFunction<Float>> s) {
        // Representative + edge coordinates (finite physical inputs plus the
        // deterministic +/-inf and +/-large-float edges apply() must still handle).
        float[] pts = {
            Float.NEGATIVE_INFINITY, -1.0e30f, -100.0f, -10.0f, -6.0f, -5.0f, -4.5f, -4.0f,
            -3.0f, -2.5f, -2.0f, -1.5f, -1.0f, -0.75f, -0.5f, -0.25f, -0.125f, -0.0f, 0.0f,
            0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 4.5f, 5.0f, 6.0f,
            7.0f, 10.0f, 100.0f, 1.0e30f, Float.POSITIVE_INFINITY
        };
        for (float x : pts) O.println(tag + "\t" + f(x) + "\t" + f(s.apply(x)));
        // Fine sweep across the interesting domain [-7, 8] (step 0.011 to land on many
        // non-power-of-two fractions and cross every knot).
        for (int i = -7000; i <= 8000; i += 11) {
            float x = i / 1000.0f;
            O.println(tag + "\t" + f(x) + "\t" + f(s.apply(x)));
        }
    }

    public static void main(String[] args) throws Exception {
        sweep("K", splineConst());
        sweep("FLAT", splineFlat());
        sweep("DERIV", splineDeriv());
        sweep("MIXED", splineMixed());
        sweep("SINGLE", splineSingle());
        sweep("SINGLE0", splineSingle0());
        sweep("NESTED", splineNested());
    }
}
