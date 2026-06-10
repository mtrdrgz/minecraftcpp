// Ground-truth generator for net.minecraft.util.SmoothDouble (camera smoothing).
// STATEFUL: each row is one getNewDeltaValue(targetDelta, time) call on a single
// instance. We emit, per call, the inputs and the returned double plus the full
// internal state (targetValue/remainingValue/lastAmount, read via reflection) so the
// C++ replay can verify both the return AND the carried state bit-for-bit.
//
// Every double is dumped as String.format("%016x", Double.doubleToRawLongBits(v)).
//
//   tools/run_groundtruth.ps1 -Tool SmoothDoubleParity -Out mcpp/build/smooth_double.tsv
//
// Pure: SmoothDouble depends only on Mth.lerp (plain arithmetic) and Math.signum, so
// no Bootstrap is needed. We call the real net.minecraft method directly; private
// fields are read by reflection. A RESET row marks a reset() between sequences.

import java.lang.reflect.Field;
import net.minecraft.util.SmoothDouble;

public class SmoothDoubleParity {
    static final java.io.PrintStream O = System.out;

    static Field F_TARGET, F_REMAIN, F_LAST;

    static String hx(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    // STEP <targetDeltaBits> <timeBits> <retBits> <targetValueBits> <remainingValueBits> <lastAmountBits>
    static void step(SmoothDouble s, int seq, double targetDelta, double time) throws Exception {
        double ret = s.getNewDeltaValue(targetDelta, time);
        double tv = F_TARGET.getDouble(s);
        double rv = F_REMAIN.getDouble(s);
        double la = F_LAST.getDouble(s);
        O.println("STEP\t" + seq + "\t" + hx(targetDelta) + "\t" + hx(time) + "\t"
                + hx(ret) + "\t" + hx(tv) + "\t" + hx(rv) + "\t" + hx(la));
    }

    static void reset(SmoothDouble s, int seq) throws Exception {
        s.reset();
        double tv = F_TARGET.getDouble(s);
        double rv = F_REMAIN.getDouble(s);
        double la = F_LAST.getDouble(s);
        O.println("RESET\t" + seq + "\t" + hx(tv) + "\t" + hx(rv) + "\t" + hx(la));
    }

    public static void main(String[] args) throws Exception {
        F_TARGET = SmoothDouble.class.getDeclaredField("targetValue");
        F_REMAIN = SmoothDouble.class.getDeclaredField("remainingValue");
        F_LAST   = SmoothDouble.class.getDeclaredField("lastAmount");
        F_TARGET.setAccessible(true);
        F_REMAIN.setAccessible(true);
        F_LAST.setAccessible(true);

        int seq = 0;

        // Seq 0: typical mouse-turn smoothing — constant smoothing time, varied deltas.
        {
            SmoothDouble s = new SmoothDouble();
            seq++;
            double[] deltas = { 5.0, 5.0, 5.0, 5.0, 5.0, 0.0, 0.0, -3.0, -3.0, -3.0, 10.0, -10.0 };
            for (double d : deltas) step(s, seq, d, 0.20000000298023224 /* (double)(float)0.2 */);
        }

        // Seq 1: vanilla-ish smoothing factor 1/3, alternating sign deltas.
        {
            SmoothDouble s = new SmoothDouble();
            seq++;
            double t = 1.0 / 3.0;
            double[] deltas = { 1.0, -1.0, 2.0, -2.0, 0.5, -0.5, 4.0, -8.0, 0.0, 0.0 };
            for (double d : deltas) step(s, seq, d, t);
        }

        // Seq 2: large time (>1) so remainingValue overshoots; tests the sign clamp.
        {
            SmoothDouble s = new SmoothDouble();
            seq++;
            double[] deltas = { 100.0, 100.0, -50.0, -200.0, 7.0, 7.0, 7.0 };
            for (double d : deltas) step(s, seq, d, 1.5);
        }

        // Seq 3: time == 0 (no remaining accumulation) then time == 1.
        {
            SmoothDouble s = new SmoothDouble();
            seq++;
            step(s, seq, 3.0, 0.0);
            step(s, seq, 3.0, 0.0);
            step(s, seq, 3.0, 1.0);
            step(s, seq, -6.0, 1.0);
            step(s, seq, 0.0, 1.0);
        }

        // Seq 4: tiny fractional deltas + tiny time (sub-pixel smoothing).
        {
            SmoothDouble s = new SmoothDouble();
            seq++;
            double[] deltas = { 0.01, 0.02, -0.005, 0.0001, -0.0001, 1.0E-8, -1.0E-8 };
            for (double d : deltas) step(s, seq, d, 0.05);
        }

        // Seq 5: reset() in the middle of a sequence — state must clear, then resume.
        {
            SmoothDouble s = new SmoothDouble();
            seq++;
            step(s, seq, 5.0, 0.2);
            step(s, seq, 5.0, 0.2);
            step(s, seq, 5.0, 0.2);
            reset(s, seq);
            step(s, seq, 5.0, 0.2);
            step(s, seq, 5.0, 0.2);
            reset(s, seq);
            step(s, seq, -7.0, 0.5);
        }

        // Seq 6: negative time (degenerate but well-defined arithmetic).
        {
            SmoothDouble s = new SmoothDouble();
            seq++;
            double[] deltas = { 4.0, 4.0, -4.0, -4.0, 2.0 };
            for (double d : deltas) step(s, seq, d, -0.3);
        }

        // Seq 7: alternating exactly-cancelling deltas around lastAmount sign flips.
        {
            SmoothDouble s = new SmoothDouble();
            seq++;
            double[] deltas = { 10.0, -20.0, 20.0, -20.0, 20.0, -10.0, 0.0, 0.0, 0.0 };
            for (double d : deltas) step(s, seq, d, 0.5);
        }

        // Seq 8: very large magnitudes (stress double precision, no overflow to inf).
        {
            SmoothDouble s = new SmoothDouble();
            seq++;
            double[] deltas = { 1.0E12, 1.0E12, -2.0E12, 1.0E13, -1.0E13 };
            for (double d : deltas) step(s, seq, d, 0.25);
        }

        // Seq 9: zero-only deltas (delta==0 -> signum==0 path, no clamp).
        {
            SmoothDouble s = new SmoothDouble();
            seq++;
            for (int k = 0; k < 5; k++) step(s, seq, 0.0, 0.2);
        }

        // Seq 10: a long run of constant target so it asymptotically settles.
        {
            SmoothDouble s = new SmoothDouble();
            seq++;
            for (int k = 0; k < 30; k++) step(s, seq, 1.0, 0.3333333432674408 /* (float)(1/3) */);
        }

        O.flush();
    }
}
