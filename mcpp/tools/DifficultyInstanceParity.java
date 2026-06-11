import net.minecraft.world.Difficulty;
import net.minecraft.world.DifficultyInstance;

// Ground-truth dumper for net.minecraft.world.DifficultyInstance (MC 26.1.2).
// Drives the REAL public constructor and accessors; never re-implements the
// body Java-side. Emits tab-separated rows consumed by
// DifficultyInstanceParityTest.cpp.
//
// TAG (one per case):
//   INST <ordinal> <totalGameTime> <localGameTime>
//        <moonBrightnessBits> <effectiveDifficultyBits> <specialMultiplierBits>
//        <isHard> <isHarderThan_eff> <isHarderThan_lo> <isHarderThan_hi>
// where:
//   ordinal              = base.ordinal() (Difficulty.values() index)
//   totalGameTime        = decimal long fed to the ctor
//   localGameTime        = decimal long fed to the ctor
//   moonBrightnessBits   = Float.floatToRawIntBits(moonBrightness)  (input)
//   effectiveDifficultyBits = Float.floatToRawIntBits(getEffectiveDifficulty())
//   specialMultiplierBits   = Float.floatToRawIntBits(getSpecialMultiplier())
//   isHard               = isHard() as 0/1
//   isHarderThan_eff     = isHarderThan(getEffectiveDifficulty()) as 0/1
//   isHarderThan_lo      = isHarderThan(getEffectiveDifficulty() - 0.0001F) 0/1
//   isHarderThan_hi      = isHarderThan(getEffectiveDifficulty() + 0.0001F) 0/1
//
// Floats are emitted as raw IEEE-754 bit patterns (decimal int) so the C++ test
// compares bit-exact. All inputs are FINITE physical values.
public class DifficultyInstanceParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // DifficultyInstance is a plain immutable value object and needs no
        // registry bootstrap, but bootstrap defensively in case classloading
        // pulls in registry-touching dependencies.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — DifficultyInstance does not require it
        }

        // Finite sweeps chosen to exercise every branch of calculateDifficulty
        // and getSpecialMultiplier:
        //   * total times around the -72000 offset and the 1_440_000 cap.
        //   * local times around the 3_600_000 cap.
        //   * moon brightness 0..1 plus a couple of clamp-edge values.
        long[] totals = new long[]{
            0L, 1L, 71999L, 72000L, 72001L, 100000L, 720000L, 1000000L,
            1439999L, 1440000L, 1440001L, 1512000L, 5000000L, 100000000L,
            -1L, -100000L
        };
        long[] locals = new long[]{
            0L, 1L, 100000L, 1799999L, 1800000L, 1800001L, 3599999L,
            3600000L, 3600001L, 7200000L, 50000000L, -1L
        };
        float[] moons = new float[]{
            0.0F, 0.0625F, 0.125F, 0.25F, 0.375F, 0.5F, 0.625F, 0.75F,
            0.875F, 1.0F, -0.5F, 2.0F
        };

        for (Difficulty base : Difficulty.values()) {
            for (long total : totals) {
                for (long local : locals) {
                    for (float moon : moons) {
                        DifficultyInstance di = new DifficultyInstance(base, total, local, moon);
                        float eff = di.getEffectiveDifficulty();
                        float spec = di.getSpecialMultiplier();
                        O.println("INST"
                                + "\t" + base.ordinal()
                                + "\t" + total
                                + "\t" + local
                                + "\t" + Float.floatToRawIntBits(moon)
                                + "\t" + Float.floatToRawIntBits(eff)
                                + "\t" + Float.floatToRawIntBits(spec)
                                + "\t" + (di.isHard() ? 1 : 0)
                                + "\t" + (di.isHarderThan(eff) ? 1 : 0)
                                + "\t" + (di.isHarderThan(eff - 0.0001F) ? 1 : 0)
                                + "\t" + (di.isHarderThan(eff + 0.0001F) ? 1 : 0));
                    }
                }
            }
        }
    }
}
