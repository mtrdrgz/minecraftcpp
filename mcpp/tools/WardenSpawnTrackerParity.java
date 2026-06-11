// Ground-truth generator for the pure integer state machine of
//   net.minecraft.world.entity.monster.warden.WardenSpawnTracker (Minecraft 26.1.2).
//
//   tools/run_groundtruth.ps1 -Tool WardenSpawnTrackerParity -Out mcpp/build/warden_spawn_tracker.tsv
//
// WHAT IS DRIVEN (and why it is a faithful 1:1, not a re-implementation):
//   WardenSpawnTracker is just three ints { ticksSinceLastWarning, warningLevel,
//   cooldownTicks }. We construct a REAL instance via its public (int,int,int)
//   constructor (NO Level/registry/entity is touched), then drive the REAL pure
//   methods on it via reflection and read back the three private fields after
//   each step. The world-coupled static entry point tryWarn(ServerLevel,...) is
//   never called. Every emitted number comes straight out of the real object.
//
// Methods exercised (all read no world/entity state):
//   tick()                 — public  : threshold branch + cooldown decrement
//   setWarningLevel(int)   — public  : Mth.clamp(v,0,4)
//   increaseWarningLevel() — private : cooldown-gated bump (reflected)
//   decreaseWarningLevel() — private : setWarningLevel(level-1) (reflected)
//   reset()                — public  : all three -> 0
//
// TSV rows (tab-separated), dispatched by leading TAG in the C++ test. Each row
// reports the FULL post-state so the C++ port must match every field:
//
//   STEP  <seed_t> <seed_w> <seed_c>  <opcode> <oparg>  <out_t> <out_w> <out_c>
//
// opcode/oparg encode the operation applied to the seeded instance:
//   0 -> tick()                  (oparg ignored)
//   1 -> setWarningLevel(oparg)
//   2 -> increaseWarningLevel()  (oparg ignored)
//   3 -> decreaseWarningLevel()  (oparg ignored)
//   4 -> reset()                 (oparg ignored)
//
// We also exercise multi-tick runs (TICKN) that step a single instance many times
// to catch the 12000-tick rollover edge and cooldown drain without re-seeding.
//
//   TICKN <seed_t> <seed_w> <seed_c> <n>  <out_t> <out_w> <out_c>

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

public class WardenSpawnTrackerParity {
    static final java.io.PrintStream O = System.out;

    static Class<?> CLS;
    static Constructor<?> CTOR;
    static Field F_TICKS, F_LEVEL, F_COOL;
    static Method M_TICK, M_SET, M_INC, M_DEC, M_RESET;

    static Object make(int t, int w, int c) throws Exception {
        return CTOR.newInstance(t, w, c);
    }
    static int ticks(Object o) throws Exception { return F_TICKS.getInt(o); }
    static int level(Object o) throws Exception { return F_LEVEL.getInt(o); }
    static int cool(Object o)  throws Exception { return F_COOL.getInt(o); }

    static void emitState(String tag, int st, int sw, int sc, int opcode, int oparg, Object o) throws Exception {
        O.println(tag + "\t" + st + "\t" + sw + "\t" + sc + "\t" + opcode + "\t" + oparg
                + "\t" + ticks(o) + "\t" + level(o) + "\t" + cool(o));
    }

    public static void main(String[] args) throws Exception {
        // WardenSpawnTracker's <clinit> builds a Codec; bootstrap to be safe.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {
        }

        CLS = Class.forName("net.minecraft.world.entity.monster.warden.WardenSpawnTracker");
        CTOR = CLS.getDeclaredConstructor(int.class, int.class, int.class);
        CTOR.setAccessible(true);

        F_TICKS = CLS.getDeclaredField("ticksSinceLastWarning");
        F_LEVEL = CLS.getDeclaredField("warningLevel");
        F_COOL  = CLS.getDeclaredField("cooldownTicks");
        for (Field f : new Field[]{F_TICKS, F_LEVEL, F_COOL}) f.setAccessible(true);

        M_TICK  = CLS.getDeclaredMethod("tick");
        M_SET   = CLS.getDeclaredMethod("setWarningLevel", int.class);
        M_INC   = CLS.getDeclaredMethod("increaseWarningLevel");
        M_DEC   = CLS.getDeclaredMethod("decreaseWarningLevel");
        M_RESET = CLS.getDeclaredMethod("reset");
        for (Method m : new Method[]{M_TICK, M_SET, M_INC, M_DEC, M_RESET}) m.setAccessible(true);

        // ---- Seed states: cover the trap edges -----------------------------
        // ticksSinceLastWarning around the 12000 rollover (<, ==, >),
        // warningLevel around the [0,4] clamp band (incl. out-of-band seeds),
        // cooldownTicks around the >0 gate and the 200 re-arm.
        List<int[]> seeds = new ArrayList<>();
        int[] tSeeds = {
            -5, -1, 0, 1, 2, 199, 200, 201,
            11998, 11999, 12000, 12001, 12002,
            23999, 24000, 24001, 1000000, Integer.MAX_VALUE
        };
        int[] wSeeds = { -3, -1, 0, 1, 2, 3, 4, 5, 7, 100 };
        int[] cSeeds = { -1, 0, 1, 2, 199, 200, 201, 1000 };
        for (int t : tSeeds)
            for (int w : wSeeds)
                for (int c : cSeeds)
                    seeds.add(new int[]{t, w, c});

        // Op arguments for setWarningLevel: walk across and beyond the clamp band.
        int[] setArgs = { Integer.MIN_VALUE, -100, -2, -1, 0, 1, 2, 3, 4, 5, 6, 100, Integer.MAX_VALUE };

        // ---- Single-op STEP rows ------------------------------------------
        for (int[] s : seeds) {
            int t = s[0], w = s[1], c = s[2];

            { Object o = make(t, w, c); M_TICK.invoke(o);  emitState("STEP", t, w, c, 0, 0, o); }
            for (int a : setArgs) { Object o = make(t, w, c); M_SET.invoke(o, a); emitState("STEP", t, w, c, 1, a, o); }
            { Object o = make(t, w, c); M_INC.invoke(o);   emitState("STEP", t, w, c, 2, 0, o); }
            { Object o = make(t, w, c); M_DEC.invoke(o);   emitState("STEP", t, w, c, 3, 0, o); }
            { Object o = make(t, w, c); M_RESET.invoke(o); emitState("STEP", t, w, c, 4, 0, o); }
        }

        // ---- Multi-tick TICKN rows (drive one instance n ticks) -----------
        // n chosen to land just before / at / just after the 12000 rollover and
        // to fully drain a 200-tick cooldown, with a long run that triggers
        // multiple rollovers + level decreases.
        int[][] tickRuns = {
            {0, 0, 0, 1}, {0, 0, 0, 199}, {0, 0, 0, 200}, {0, 0, 0, 201},
            {0, 4, 250, 250},
            {11999, 3, 0, 1}, {11999, 3, 0, 2}, {12000, 3, 0, 1}, {12001, 3, 0, 1},
            {0, 4, 0, 12000}, {0, 4, 0, 12001}, {0, 4, 0, 24001},
            {0, 4, 0, 60005},
            {11990, 4, 5, 25}, {11999, 1, 200, 5},
        };
        for (int[] r : tickRuns) {
            int t = r[0], w = r[1], c = r[2], n = r[3];
            Object o = make(t, w, c);
            for (int i = 0; i < n; i++) M_TICK.invoke(o);
            O.println("TICKN\t" + t + "\t" + w + "\t" + c + "\t" + n
                    + "\t" + ticks(o) + "\t" + level(o) + "\t" + cool(o));
        }
    }
}
