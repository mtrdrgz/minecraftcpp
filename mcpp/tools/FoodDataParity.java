// Ground-truth generator for net.minecraft.world.food.FoodData (26.1.2) — the stateful
// hunger model. We exercise the world-independent surface of the REAL class:
//
//   - private void add(int food, float saturation)    (invoked via reflection)
//   - public  void addExhaustion(float amount)         (invoked directly)
//   - the world-independent block of tick(ServerPlayer), FoodData.java:35-42
//
// The full tick(ServerPlayer) needs a live ServerPlayer/ServerLevel/Difficulty/GameRules,
// so it cannot be invoked here. For the TICK op we replicate EXACTLY the 8 source lines of
// FoodData.java:35-42, but operating on the REAL FoodData instance's actual private fields
// (read/written via reflection) — so field defaults, types and accumulated state all come
// from the genuine object. `peaceful` == (difficulty == Difficulty.PEACEFUL).
//
// UNIFORM ROW SCHEMA (every op applies to the instance of its sequence id, then we dump
// the post-state). The C++ side keeps one FoodData per seqId and replays IDENTICAL ops:
//
//   <OP>\t<seqId>\t<argA>\t<argB>\t<foodLevel>\t<satBits>\t<exhBits>\t<tickTimer>
//
// OP / argA / argB:
//   NEW   -      -          construct a fresh FoodData (default fields)
//   ADD   <int> <satBits>   call add(food=argA, saturation=bitsToFloat(argB))
//   EXH   <exhBits> -       call addExhaustion(bitsToFloat(argA))
//   TICK  <0|1>  -          world-independent tick; argA = peaceful flag
// foodLevel/tickTimer decimal; saturation/exhaustion as raw IEEE-754 float bits (%08x).
//
//   tools/run_groundtruth.ps1 -Tool FoodDataParity -Out mcpp/build/food_data.tsv

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.world.food.FoodData;

public class FoodDataParity {
    static final java.io.PrintStream O = System.out;
    static String fb(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static Field FOOD, SAT, EXH, TIMER;
    static Method ADD;

    static int   food(FoodData d)  throws Exception { return FOOD.getInt(d); }
    static float sat(FoodData d)   throws Exception { return SAT.getFloat(d); }
    static float exh(FoodData d)   throws Exception { return EXH.getFloat(d); }
    static int   timer(FoodData d) throws Exception { return TIMER.getInt(d); }

    static void row(String op, int seqId, String a, String b, FoodData d) throws Exception {
        O.println(op + "\t" + seqId + "\t" + a + "\t" + b
            + "\t" + food(d) + "\t" + fb(sat(d)) + "\t" + fb(exh(d)) + "\t" + timer(d));
    }

    static FoodData newInst(int seqId) throws Exception {
        Constructor<FoodData> c = FoodData.class.getDeclaredConstructor();
        c.setAccessible(true);
        FoodData d = c.newInstance();
        row("NEW", seqId, "-", "-", d);
        return d;
    }

    static void doAdd(int seqId, FoodData d, int f, float s) throws Exception {
        ADD.invoke(d, f, s);
        row("ADD", seqId, Integer.toString(f), fb(s), d);
    }

    static void doExh(int seqId, FoodData d, float e) throws Exception {
        d.addExhaustion(e);
        row("EXH", seqId, fb(e), "-", d);
    }

    // Replicates VERBATIM net.minecraft.world.food.FoodData.tick lines 35-42 on the real
    // instance's private fields. peaceful == (difficulty == Difficulty.PEACEFUL).
    static void doTick(int seqId, FoodData d, boolean peaceful) throws Exception {
        float exhaustionLevel = exh(d);
        float saturationLevel = sat(d);
        int   foodLevel       = food(d);
        if (exhaustionLevel > 4.0F) {
            exhaustionLevel -= 4.0F;
            if (saturationLevel > 0.0F) {
                saturationLevel = Math.max(saturationLevel - 1.0F, 0.0F);
            } else if (!peaceful) {  // difficulty != Difficulty.PEACEFUL
                foodLevel = Math.max(foodLevel - 1, 0);
            }
        }
        EXH.setFloat(d, exhaustionLevel);
        SAT.setFloat(d, saturationLevel);
        FOOD.setInt(d, foodLevel);
        row("TICK", seqId, peaceful ? "1" : "0", "-", d);
    }

    // (food, saturation) pairs for add(); covers normal eat, over-cap, negatives, fractions.
    static final int[]   FOODS = { 0, 1, 2, 3, 4, 5, 6, 8, 20, 25, -1, -5, -20, -100, 100, 7 };
    static final float[] SATS  = { 0.0f, 0.5f, 1.0f, 2.4f, 5.0f, 6.0f, 12.8f, 20.0f, 0.1f,
                                   -1.0f, -3.5f, -20.0f, 100.0f, 0.3333333f };
    // exhaustion increments for addExhaustion(); covers <4, ==4, >4, the 40 cap, negatives.
    static final float[] EXHS  = { 0.0f, 0.005f, 0.05f, 0.1f, 0.5f, 1.0f, 3.0f, 4.0f, 4.0001f,
                                   5.0f, 6.0f, 10.0f, 39.5f, 40.0f, 45.0f, -1.0f, -10.0f, 0.025f };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        FOOD  = FoodData.class.getDeclaredField("foodLevel");        FOOD.setAccessible(true);
        SAT   = FoodData.class.getDeclaredField("saturationLevel");  SAT.setAccessible(true);
        EXH   = FoodData.class.getDeclaredField("exhaustionLevel");  EXH.setAccessible(true);
        TIMER = FoodData.class.getDeclaredField("tickTimer");        TIMER.setAccessible(true);
        ADD   = FoodData.class.getDeclaredMethod("add", int.class, float.class); ADD.setAccessible(true);

        int seq = 0;

        // 0) Fresh-construct: prove the default field values (20 / 5.0F / 0 / 0).
        newInst(seq++);

        // 1) Single add() over the full grid, each on a FRESH FoodData (isolates the formula).
        for (int f : FOODS) for (float s : SATS) {
            FoodData d = newInst(seq++);
            doAdd(seq - 1, d, f, s);
        }

        // 2) Single addExhaustion() over the grid, fresh each time (clamp-at-40 behaviour).
        for (float e : EXHS) {
            FoodData d = newInst(seq++);
            doExh(seq - 1, d, e);
        }

        // 3) Single world-independent tick on a fresh instance, both difficulties (no-op:
        //    fresh exhaustion is 0); the real drain is covered by the stateful sequences.
        for (boolean peaceful : new boolean[]{ false, true }) {
            FoodData d = newInst(seq++);
            doTick(seq - 1, d, peaceful);
        }

        // 4) STATEFUL SEQUENCES — one FoodData per seqId, interleaved add/addExhaustion/tick.

        // Seq A — eat then burn it off; NORMAL difficulty so foodLevel decrements at sat==0.
        {
            int s = seq++; FoodData d = newInst(s);
            doAdd(s, d, 6, 6.0f);
            doExh(s, d, 5.0f);
            doTick(s, d, false);
            doExh(s, d, 4.5f);
            doTick(s, d, false);
            for (int k = 0; k < 12; k++) { doExh(s, d, 5.0f); doTick(s, d, false); }
        }

        // Seq B — same drain but PEACEFUL: foodLevel must NOT decrement once saturation is 0.
        {
            int s = seq++; FoodData d = newInst(s);
            doAdd(s, d, 0, 0.0f);
            for (int k = 0; k < 10; k++) { doExh(s, d, 5.0f); doTick(s, d, true); }
        }

        // Seq C — over-cap eat + saturation clamped to foodLevel + negative add (rotten).
        {
            int s = seq++; FoodData d = newInst(s);
            doAdd(s, d, 100, 100.0f);   // foodLevel caps at 20, sat caps at 20
            doAdd(s, d, -3, -2.0f);
            doAdd(s, d, 0, 50.0f);      // sat re-clamped to current foodLevel
            doExh(s, d, 40.0f);         // clamp at 40
            doExh(s, d, 10.0f);         // stays 40
            doTick(s, d, false);
            doExh(s, d, -100.0f);       // negative -> below 4
            doTick(s, d, false);        // no-op (exh<=4)
        }

        // Seq D — fractional accumulation: a tiny addExhaustion then a tick, repeated.
        {
            int s = seq++; FoodData d = newInst(s);
            doAdd(s, d, 20, 20.0f);
            for (int k = 0; k < 50; k++) { doExh(s, d, 0.05f); doTick(s, d, false); }
        }

        // Seq E — eat small amounts repeatedly toward the cap with saturation pressure.
        {
            int s = seq++; FoodData d = newInst(s);
            doAdd(s, d, -19, -5.0f);    // foodLevel=1, sat clamped to 1
            for (int k = 0; k < 25; k++) doAdd(s, d, 1, 0.3f);
            doExh(s, d, 4.25f);
            doTick(s, d, false);
        }
    }
}
