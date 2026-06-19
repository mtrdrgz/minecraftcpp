// Ground-truth generator for the one pure static helper of
//   net.minecraft.world.entity.AgeableMob (Minecraft 26.1.2):
//
//   public static int getSpeedUpSecondsWhenFeeding(final int ticksUntilAdult) {
//      return (int)(ticksUntilAdult / 20 * 0.1F);   // int-div, *float, truncate
//   }
//
//   tools/run_groundtruth.ps1 -Tool AgeableMobParity -Out mcpp/build/ageable_mob.tsv
//
// TSV rows (tab-separated), dispatched by leading TAG in the C++ test:
//   FEED <ticksUntilAdult:int> <result:int>
//
// REFLECTION NOTE:
//   getSpeedUpSecondsWhenFeeding(int) is STATIC and is invoked on the REAL
//   AgeableMob class via reflection (Method.setAccessible(true), invoke(null,t)).
//   No AgeableMob instance, Level, or registry is constructed: the method body
//   touches no instance/world state, so the static call is a faithful 1:1 of
//   what vanilla runs. We never replicate the body on the Java side — every
//   value below comes straight out of the real class.

import java.lang.reflect.Method;

public class AgeableMobParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Some net.minecraft classes trip "Not bootstrapped" at class load.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // If bootstrap is unavailable, the static reflective call still works.
        }

        // Real static method on the REAL class.
        Method m = net.minecraft.world.entity.AgeableMob.class
                .getDeclaredMethod("getSpeedUpSecondsWhenFeeding", int.class);
        m.setAccessible(true);

        // Build a dense, trap-targeting set of inputs:
        //   * every residue around the /20 integer-division boundary (k*20-1..+1),
        //   * the float-truncation edges of q*0.1F (q where q*0.1F crosses an int),
        //   * negatives (int-div truncates toward zero, asymmetric vs floor),
        //   * INT_MIN/INT_MAX and large magnitudes (float rounding of the product).
        java.util.TreeSet<Integer> set = new java.util.TreeSet<>();

        // A REPRESENTATIVE battery (was a 20.8M-row full sweep — overkill: the product
        // t/20*0.1F is always well within int range, so result can never saturate and the
        // arithmetic is IEEE-identical; a strided + edge-dense set covers every trap).
        // Small contiguous span -2000..2000 catches all near-zero /20 and *0.1F edges.
        for (int t = -2000; t <= 2000; t++) set.add(t);

        // Boundaries around multiples of 20 (the integer-division residue), k in +/-3000.
        for (long k = -3000; k <= 3000; k += 1) {
            long base = k * 20L;
            for (long d = -1; d <= 1; d++) {
                long v = base + d;
                if (v >= Integer.MIN_VALUE && v <= Integer.MAX_VALUE) set.add((int) v);
            }
        }

        // Boundaries where q*0.1F crosses an integer: q ~= 10*n, q = t/20, so t ~= 200*n.
        for (long n = -3000; n <= 3000; n += 1) {
            long base = 200L * n;
            for (long d = -5; d <= 5; d++) {
                long v = base + d;
                if (v >= Integer.MIN_VALUE && v <= Integer.MAX_VALUE) set.add((int) v);
            }
        }

        // Strided sweep across the FULL int range (catches any large-magnitude float rounding).
        for (long v = Integer.MIN_VALUE; v <= Integer.MAX_VALUE; v += 200003L) set.add((int) v);

        // Explicit extremes and large magnitudes for float rounding of the product.
        int[] EXTREMES = {
            Integer.MIN_VALUE, Integer.MIN_VALUE + 1, Integer.MIN_VALUE + 19,
            -2000000000, -1000000019, -200000001, -20000001, -2000001,
            -1, 0, 1,
            2000001, 20000001, 200000001, 1000000019, 2000000000,
            Integer.MAX_VALUE - 19, Integer.MAX_VALUE - 1, Integer.MAX_VALUE
        };
        for (int v : EXTREMES) set.add(v);

        for (int t : set) {
            int got = (Integer) m.invoke(null, t);
            O.println("FEED\t" + t + "\t" + got);
        }
    }
}
