// Ground truth for the PURE state-machine subset of
// net.minecraft.world.effect.MobEffectInstance (Minecraft 26.1.2).
//
// Drives the REAL class and emits self-describing rows: each row carries the
// full INPUT spec AND the expected OUTPUT, so the C++ port reads inputs from the
// TSV, recomputes with its own code, and compares to the expected output.
//
// Methods exercised (all PURE — no world/level/registry-value access):
//   * constructor amplifier clamp  Mth.clamp(amplifier,0,255)
//   * update(takeOver)             — hidden-effect promotion state machine
//   * withScaledDuration(float)    — Math.max(Mth.floor(d*scale),1)
//   * tickDownDuration + downgradeToHiddenEffect (over a hidden chain)
//   * endsWithin / isInfiniteDuration / isShorterDurationThan / hasRemainingDuration
//   * hashCode()
//
// Instances are built with the canonical PUBLIC constructor using a REAL
// registered Holder<MobEffect> (MobEffects.SPEED / .SLOWNESS); the pure logic
// only uses the effect through Holder.equals, so its identity is all that
// matters. We print the real Holder.hashCode() as the hashCode() seed so the
// C++ side mixes the identical seed. Private fields (duration, amplifier,
// ambient, visible, showIcon, hiddenEffect) are read via reflection.
//
// Encoding: floats = 8-hex Float.floatToRawIntBits; bools = 0/1; ints decimal.
//
// An "effspec" is a self-contained instance encoded as a fixed token run:
//   <effId> <dur> <amp> <ambient> <visible> <showIcon> <hiddenDepth> [<hidden effspec>...]
// where hiddenDepth is 0 or 1 (we never build deeper-than-needed input chains;
// chains of any depth are still produced/flattened on OUTPUT).
//
// Output state of an instance is flattened node-by-node and terminated by "END":
//   <effId> <dur> <amp> <ambient> <visible> <showIcon> <hashCode> ... END
//
// Row tags (tab-separated):
//   CTOR   <effId> <dur> <amp> <ambient> <visible> <showIcon> | <seedHash> <flatOut>
//   ENDS   <effId> <dur> <ticks> <expectedBool>
//   INF    <effId> <dur> <expectedBool>
//   HASREM <effId> <dur> <expectedBool>
//   SCALE  <effId> <dur> <amp> <ambient> <visible> <showIcon> <scale8> | <seedHash> <flatOut>
//   TICK   <effIn effspec> | <expectChanged> <seedHash> <flatOut>     (tickDown+downgrade once)
//   UPDATE <cur effspec> <<< <takeOver effspec> | <changed> <seedHash> <flatOut>

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.core.Holder;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.effect.MobEffects;

public class MobEffectInstanceParity {
    static final java.io.PrintStream O = System.out;

    static Field fDuration, fAmplifier, fAmbient, fVisible, fShowIcon, fHidden;
    static Constructor<MobEffectInstance> ctor;
    static Method mUpdate, mWithScaled, mTickDown, mDowngrade, mEndsWithin,
            mIsInfinite, mHasRemaining;

    static Holder<MobEffect> SPEED, SLOWNESS;

    static long effId(Holder<MobEffect> h) {
        if (h == SPEED) return 1L;
        if (h == SLOWNESS) return 2L;
        return 0L;
    }
    static long effHash(Holder<MobEffect> h) { return h.hashCode(); }
    static Holder<MobEffect> holderOf(long id) { return id == 2L ? SLOWNESS : SPEED; }

    static String fb(float f) { return String.format("%08x", Float.floatToRawIntBits(f)); }

    @SuppressWarnings("unchecked")
    static void initReflection() throws Exception {
        Class<MobEffectInstance> C = MobEffectInstance.class;
        fDuration = C.getDeclaredField("duration");
        fAmplifier = C.getDeclaredField("amplifier");
        fAmbient = C.getDeclaredField("ambient");
        fVisible = C.getDeclaredField("visible");
        fShowIcon = C.getDeclaredField("showIcon");
        fHidden = C.getDeclaredField("hiddenEffect");
        for (Field f : new Field[]{fDuration, fAmplifier, fAmbient, fVisible, fShowIcon, fHidden}) {
            f.setAccessible(true);
        }
        ctor = (Constructor<MobEffectInstance>) C.getDeclaredConstructor(
                Holder.class, int.class, int.class, boolean.class, boolean.class,
                boolean.class, MobEffectInstance.class);
        ctor.setAccessible(true);

        mUpdate = C.getMethod("update", MobEffectInstance.class);
        mWithScaled = C.getMethod("withScaledDuration", float.class);
        mTickDown = C.getDeclaredMethod("tickDownDuration");
        mTickDown.setAccessible(true);
        mDowngrade = C.getDeclaredMethod("downgradeToHiddenEffect");
        mDowngrade.setAccessible(true);
        mEndsWithin = C.getMethod("endsWithin", int.class);
        mIsInfinite = C.getMethod("isInfiniteDuration");
        mHasRemaining = C.getDeclaredMethod("hasRemainingDuration");
        mHasRemaining.setAccessible(true);
    }

    static MobEffectInstance make(long effId, int dur, int amp, boolean ambient,
                                  boolean visible, boolean showIcon,
                                  MobEffectInstance hidden) throws Exception {
        return ctor.newInstance(holderOf(effId), dur, amp, ambient, visible, showIcon, hidden);
    }

    // ── effspec INPUT encoding ──────────────────────────────────────────────
    // Append the input encoding of an instance (used for inputs we hand to C++).
    static String specOf(long effId, int dur, int amp, boolean ambient,
                         boolean visible, boolean showIcon, String hiddenSpec) {
        // hiddenSpec is either "0" (no hidden) or "1 <inner spec>".
        return effId + "\t" + dur + "\t" + amp + "\t" + (ambient?1:0) + "\t"
             + (visible?1:0) + "\t" + (showIcon?1:0) + "\t" + hiddenSpec;
    }

    // ── flattened OUTPUT state ──────────────────────────────────────────────
    static String flatOut(MobEffectInstance inst) throws Exception {
        StringBuilder sb = new StringBuilder();
        sb.append(effHash(inst.getEffect())); // seed hash (root effect)
        MobEffectInstance node = inst;
        int guard = 0;
        while (node != null && guard++ < 64) {
            sb.append('\t').append(effId(node.getEffect()))
              .append('\t').append(fDuration.getInt(node))
              .append('\t').append(fAmplifier.getInt(node))
              .append('\t').append(fAmbient.getBoolean(node) ? 1 : 0)
              .append('\t').append(fVisible.getBoolean(node) ? 1 : 0)
              .append('\t').append(fShowIcon.getBoolean(node) ? 1 : 0)
              .append('\t').append(node.hashCode());
            node = (MobEffectInstance) fHidden.get(node);
        }
        sb.append('\t').append("END");
        return sb.toString();
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        initReflection();

        SPEED = MobEffects.SPEED;
        SLOWNESS = MobEffects.SLOWNESS;

        // ── 1. CTOR (amplifier clamp + hashCode + base state) ───────────────
        int[] amps = {Integer.MIN_VALUE, -100, -1, 0, 1, 5, 100, 254, 255, 256, 1000, Integer.MAX_VALUE};
        int[] ctorDurs = {-1, 0, 1, 100, Integer.MAX_VALUE, Integer.MIN_VALUE};
        boolean[] bools = {false, true};
        for (long eid : new long[]{1L, 2L}) {
            for (int amp : amps) {
                for (int dur : ctorDurs) {
                    MobEffectInstance m = make(eid, dur, amp, false, true, true, null);
                    O.println("CTOR\t" + eid + "\t" + dur + "\t" + amp + "\t0\t1\t1\t|\t" + flatOut(m));
                }
            }
        }
        // Boolean permutations (hashCode field mixing).
        for (boolean amb : bools) for (boolean vis : bools) for (boolean ic : bools) {
            MobEffectInstance m = make(2L, 200, 3, amb, vis, ic, null);
            O.println("CTOR\t2\t200\t3\t" + (amb?1:0) + "\t" + (vis?1:0) + "\t" + (ic?1:0)
                    + "\t|\t" + flatOut(m));
        }

        // ── 2. ENDS / INF / HASREM ──────────────────────────────────────────
        int[] eDurs = {-1, 0, 1, 5, 10, 100, Integer.MAX_VALUE, Integer.MIN_VALUE};
        int[] eTicks = {-1, 0, 1, 5, 10, Integer.MAX_VALUE, Integer.MIN_VALUE};
        for (int d : eDurs) {
            MobEffectInstance m = make(1L, d, 0, false, true, true, null);
            O.println("INF\t1\t" + d + "\t" + (((Boolean) mIsInfinite.invoke(m)) ? 1 : 0));
            O.println("HASREM\t1\t" + d + "\t" + (((Boolean) mHasRemaining.invoke(m)) ? 1 : 0));
            for (int t : eTicks) {
                boolean r = (Boolean) mEndsWithin.invoke(m, t);
                O.println("ENDS\t1\t" + d + "\t" + t + "\t" + (r ? 1 : 0));
            }
        }

        // ── 3. SCALE (withScaledDuration — floor traps) ─────────────────────
        float[] scales = {0.0f, 1.0f, 0.5f, 0.25f, 0.1f, 2.0f, 3.7f, 0.999999f,
                          1.0000001f, -0.5f, -1.0f, 100.0f, 0.0009f,
                          Float.MIN_VALUE, Float.MAX_VALUE,
                          Math.nextUp(1.0f), Math.nextDown(1.0f), 0.3333333f};
        int[] sDurs = {-1, 0, 1, 2, 3, 7, 19, 20, 100, 199, 200, 600, 1234567,
                       Integer.MAX_VALUE, Integer.MIN_VALUE, -7};
        for (int d : sDurs) {
            for (float s : scales) {
                MobEffectInstance base = make(1L, d, 2, true, false, true, null);
                MobEffectInstance scaled = (MobEffectInstance) mWithScaled.invoke(base, s);
                O.println("SCALE\t1\t" + d + "\t2\t1\t0\t1\t" + fb(s) + "\t|\t" + flatOut(scaled));
            }
        }

        // ── 4. TICK (tickDownDuration + downgradeToHiddenEffect, once) ──────
        // Inputs carry a hidden chain of depth 0, 1, or 2 (encoded inline).
        // hidden depth 2 chain: root hides h1 hides h2.
        tickRow(make(1L, 3, 2, false, true, true,
                make(1L, 2, 1, false, true, true,
                        make(1L, 1, 0, false, true, true, null))),
                "1\t3\t2\t0\t1\t1\t1\t1\t2\t1\t0\t1\t1\t1\t1\t1\t0\t0\t1\t1\t0");
        tickRow(make(1L, 1, 2, false, true, true,
                make(1L, 5, 1, false, true, true, null)),
                "1\t1\t2\t0\t1\t1\t1\t1\t5\t1\t0\t1\t1\t0");
        tickRow(make(1L, 0, 2, false, true, true,
                make(1L, 7, 1, false, true, true, null)),
                "1\t0\t2\t0\t1\t1\t1\t1\t7\t1\t0\t1\t1\t0");
        tickRow(make(1L, 5, 0, false, true, true, null), "1\t5\t0\t0\t1\t1\t0");
        tickRow(make(1L, 1, 0, false, true, true, null), "1\t1\t0\t0\t1\t1\t0");
        tickRow(make(1L, -1, 0, false, true, true, null), "1\t-1\t0\t0\t1\t1\t0");
        tickRow(make(1L, 0, 0, false, true, true, null), "1\t0\t0\t0\t1\t1\t0");

        // ── 5. UPDATE (full promotion state machine) ────────────────────────
        // a..o as documented in the original GT.
        upd(make(1L,100,1,false,true,true,null), "1\t100\t1\t0\t1\t1\t0",
            make(1L,50,3,false,true,true,null),  "1\t50\t3\t0\t1\t1\t0");
        upd(make(1L,100,1,false,true,true,null), "1\t100\t1\t0\t1\t1\t0",
            make(1L,300,3,false,true,true,null), "1\t300\t3\t0\t1\t1\t0");
        upd(make(1L,100,2,false,true,true,null), "1\t100\t2\t0\t1\t1\t0",
            make(1L,300,2,false,true,true,null), "1\t300\t2\t0\t1\t1\t0");
        upd(make(1L,300,2,false,true,true,null), "1\t300\t2\t0\t1\t1\t0",
            make(1L,100,2,false,true,true,null), "1\t100\t2\t0\t1\t1\t0");
        upd(make(1L,100,5,false,true,true,null), "1\t100\t5\t0\t1\t1\t0",
            make(1L,300,2,false,true,true,null), "1\t300\t2\t0\t1\t1\t0");
        upd(make(1L,100,5,false,true,true,make(1L,80,2,false,true,true,null)),
            "1\t100\t5\t0\t1\t1\t1\t1\t80\t2\t0\t1\t1\t0",
            make(1L,300,2,false,true,true,null), "1\t300\t2\t0\t1\t1\t0");
        upd(make(1L,-1,1,false,true,true,null), "1\t-1\t1\t0\t1\t1\t0",
            make(1L,100,3,false,true,true,null), "1\t100\t3\t0\t1\t1\t0");
        upd(make(1L,100,1,false,true,true,null), "1\t100\t1\t0\t1\t1\t0",
            make(1L,-1,3,false,true,true,null), "1\t-1\t3\t0\t1\t1\t0");
        upd(make(1L,-1,2,false,true,true,null), "1\t-1\t2\t0\t1\t1\t0",
            make(1L,-1,2,false,true,true,null), "1\t-1\t2\t0\t1\t1\t0");
        upd(make(1L,100,2,true,true,true,null), "1\t100\t2\t1\t1\t1\t0",
            make(1L,100,2,false,false,false,null), "1\t100\t2\t0\t0\t0\t0");
        upd(make(1L,100,2,false,true,true,null), "1\t100\t2\t0\t1\t1\t0",
            make(1L,100,2,true,true,true,null), "1\t100\t2\t1\t1\t1\t0");
        upd(make(1L,100,2,false,true,true,null), "1\t100\t2\t0\t1\t1\t0",
            make(1L,100,2,false,true,true,null), "1\t100\t2\t0\t1\t1\t0");
        upd(make(1L,100,1,false,true,true,make(1L,90,0,false,true,true,null)),
            "1\t100\t1\t0\t1\t1\t1\t1\t90\t0\t0\t1\t1\t0",
            make(1L,50,5,false,true,true,null), "1\t50\t5\t0\t1\t1\t0");
        upd(make(1L,100,5,false,true,true,null), "1\t100\t5\t0\t1\t1\t0",
            make(1L,100,2,false,true,true,null), "1\t100\t2\t0\t1\t1\t0");
        upd(make(1L,0,0,false,true,true,null), "1\t0\t0\t0\t1\t1\t0",
            make(1L,0,1,false,true,true,null), "1\t0\t1\t0\t1\t1\t0");
    }

    // tickRow: run tickDownDuration() then downgradeToHiddenEffect() once;
    // emit input spec + (changed, seedHash, flatOut). spec is the exact same
    // token run the C++ side parses to rebuild the input instance.
    static void tickRow(MobEffectInstance inst, String inputSpec) throws Exception {
        mTickDown.invoke(inst);
        boolean dg = (Boolean) mDowngrade.invoke(inst);
        O.println("TICK\t" + inputSpec + "\t|\t" + (dg ? 1 : 0) + "\t" + flatOut(inst));
    }

    // upd: run current.update(takeOver); emit both input specs + (changed, out).
    static void upd(MobEffectInstance current, String curSpec,
                    MobEffectInstance takeOver, String toSpec) throws Exception {
        boolean changed = (Boolean) mUpdate.invoke(current, takeOver);
        O.println("UPDATE\t" + curSpec + "\t<<<\t" + toSpec + "\t|\t"
                + (changed ? 1 : 0) + "\t" + flatOut(current));
    }
}
