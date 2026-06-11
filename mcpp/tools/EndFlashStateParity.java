import java.lang.reflect.Field;
import net.minecraft.client.renderer.EndFlashState;

// Ground truth for mcpp/src/client/renderer/EndFlashState.h. Certifies the PURE,
// GL-free End-dimension flash math of the REAL
// net.minecraft.client.renderer.EndFlashState (Minecraft 26.1.2).
//
// EndFlashState is deterministic CPU math: tick(clockTime) advances a flash
// schedule (offset/duration drawn from a SingleThreadedRandomSource seeded with
// clockTime/600), a sine-shaped intensity, and two random flash angles. No GL,
// GPU, window or resource is touched, so the class loads and runs headless.
//
// Strategy: drive a single EndFlashState instance through a deterministic SEQUENCE
// of clockTimes (so the cached-schedule path and the redraw path are both
// exercised in order). After each tick we read every private field by reflection
// and emit:
//
//   TICK  <clockTime>  | <flashSeed> <offset> <duration> <xAngleBits>
//                        <yAngleBits> <intensityBits> <oldIntensityBits> <started>
//
// then a few getIntensity(partialTicks) probes against the live (old,new) pair:
//
//   GETI  <clockTime> <partialBits(f)>  | <getIntensityBits(f)>
//
// All floats are raw IEEE-754 bits (%08x); longs/ints are decimal; `started` is 0/1.
// Every emitted value comes from the REAL class — fields via reflection, intensity
// via the real getIntensity(float).
public class EndFlashStateParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static long  getLong(Field fld, Object o)  throws Exception { return fld.getLong(o); }
    static int   getInt(Field fld, Object o)   throws Exception { return fld.getInt(o); }
    static float getFloat(Field fld, Object o) throws Exception { return fld.getFloat(o); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> cls = EndFlashState.class;
        Field fSeed     = cls.getDeclaredField("flashSeed");
        Field fOffset   = cls.getDeclaredField("offset");
        Field fDuration = cls.getDeclaredField("duration");
        Field fInt      = cls.getDeclaredField("intensity");
        Field fOldInt   = cls.getDeclaredField("oldIntensity");
        Field fXAngle   = cls.getDeclaredField("xAngle");
        Field fYAngle   = cls.getDeclaredField("yAngle");
        for (Field fld : new Field[]{ fSeed, fOffset, fDuration, fInt, fOldInt, fXAngle, fYAngle }) {
            fld.setAccessible(true);
        }

        // partialTicks probes for getIntensity (lerp between old & new intensity).
        float[] partials = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.333333f, 0.95f };

        // A representative SEQUENCE of clockTimes, exercised in order on ONE instance:
        //   * tick 0 (first bucket, forces an initial schedule draw at seed 0 -- note
        //     flashSeed default is also 0, so the FIRST tick must still draw because
        //     offset/duration are 0; we cover that edge explicitly below by also
        //     stepping within the bucket),
        //   * several ticks inside bucket 0 (no redraw -> intensity-only advance),
        //   * crossing into buckets 1, 2, ... (redraw),
        //   * sweeping a whole 600-tick window to walk the sine ramp up/down,
        //   * large clockTimes (negative-aware / overflow-aware long math),
        //   * revisiting a bucket to confirm the schedule is recomputed not cached
        //     when the seed differs.
        java.util.List<Long> seq = new java.util.ArrayList<>();
        // Walk the first bucket coarsely to see intensity stay 0 until offset, then ramp.
        for (long t = 0; t <= 600; t += 17) seq.add(t);
        // Cross into bucket 1 and walk it.
        for (long t = 600; t <= 1200; t += 23) seq.add(t);
        // Jump across several buckets.
        long[] jumps = { 1801, 2400, 3000, 3601, 5000, 7777, 12000, 99999, 123456,
                         600000, 1000000, 600L * 99999L + 137 };
        for (long t : jumps) seq.add(t);
        // Fine sweep through a single mid bucket to capture the full sine shape.
        long base = 600L * 4242L;
        for (long t = base; t <= base + 600; t += 7) seq.add(t);
        // Non-monotonic revisits (seed differs from current -> must redraw).
        long[] revisits = { 600L * 10L + 50, 600L * 3L + 10, 600L * 10L + 400,
                            600L * 0L + 5, 600L * 7L + 599 };
        for (long t : revisits) seq.add(t);

        EndFlashState state = new EndFlashState();
        for (long clockTime : seq) {
            state.tick(clockTime);

            long  seed     = getLong(fSeed, state);
            int   offset   = getInt(fOffset, state);
            int   duration = getInt(fDuration, state);
            float intensity    = getFloat(fInt, state);
            float oldIntensity = getFloat(fOldInt, state);
            float xAngle = getFloat(fXAngle, state);
            float yAngle = getFloat(fYAngle, state);
            int started = state.flashStartedThisTick() ? 1 : 0;

            O.println("TICK\t" + clockTime + "\t" + seed + "\t" + offset + "\t" + duration
                + "\t" + f(xAngle) + "\t" + f(yAngle) + "\t" + f(intensity)
                + "\t" + f(oldIntensity) + "\t" + started);

            for (float partial : partials) {
                O.println("GETI\t" + clockTime + "\t" + f(partial) + "\t"
                    + f(state.getIntensity(partial)));
            }
        }
    }
}
