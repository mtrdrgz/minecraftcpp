// Ground-truth generator for net.minecraft.world.entity.AnimationState using the
// REAL decompiled 26.1.2 class. The C++ test (AnimationStateParityTest) replays the
// identical sequence of start()/startIfStopped()/animateWhen()/stop()/fastForward()/
// copyFrom() ops on a 1:1 port and must match every queried output bit-for-bit.
//
//   tools/run_groundtruth.ps1 -Tool AnimationStateParity -Out mcpp/build/squid_swim.tsv
//
// AnimationState is a stateful tick-based state machine over a single int field:
//   private static final int STOPPED = Integer.MIN_VALUE;
//   private int startTick = Integer.MIN_VALUE;
//
//   start(tickCount)              -> startTick = tickCount
//   startIfStopped(tickCount)     -> if (!isStarted()) start(tickCount)
//   animateWhen(condition, tick)  -> condition ? startIfStopped(tick) : stop()
//   stop()                        -> startTick = Integer.MIN_VALUE
//   fastForward(ticks, timeScale) -> if (isStarted()) startTick -= (int)(ticks*timeScale)
//   getTimeInMillis(ageInTicks)   -> (long)((ageInTicks - startTick) * 50.0F)
//   isStarted()                   -> startTick != Integer.MIN_VALUE
//   copyFrom(state)               -> startTick = state.startTick
//
// "squid_swim" is the canonical AnimationState use site (Squid.swimAnimation): it is
// started/stopped each tick from the entity's tickCount and queried during rendering
// via getTimeInMillis(ageInTicks). The op battery below mirrors that tick-driven
// start/stop/fast-forward cadence plus copyFrom (server->client sync) and the
// float->int / float->long saturating casts.
//
// startTick is private with NO public getter, so we reflect it (setAccessible) to
// cross-check the raw state in addition to the public accessors.
//
// Rows (all tab-separated):
//   SEQ   <seqId> <op>                     -- mutation applied, in order. Op encodings:
//                                              A:<tick>                start(tick)
//                                              I:<tick>                startIfStopped(tick)
//                                              W:<cond>:<tick>         animateWhen(cond,tick)  cond=0/1
//                                              X                       stop()
//                                              F:<ticks>:<scaleBits>   fastForward(ticks,scale)
//                                              C:<srcTick>             copyFrom(state{startTick=srcTick})
//   QSTATE <seqId> <afterOpIdx>            <startTickInt> <isStarted0/1>  -- raw field + isStarted()
//   QMS    <seqId> <afterOpIdx> <ageBits>  <timeInMillisLong>            -- getTimeInMillis(age)
//
// floats: %08x of Float.floatToRawIntBits. ints/longs: decimal. <afterOpIdx> =
// number of ops from the SEQ already applied; C++ rebuilds fresh, applies prefix,
// then queries.

import net.minecraft.world.entity.AnimationState;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.List;

public class AnimationStateParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // The private startTick field (no getter on AnimationState).
    static Field F_startTick;

    static void resolve() throws Exception {
        F_startTick = AnimationState.class.getDeclaredField("startTick");
        F_startTick.setAccessible(true);
    }

    static int getStartTick(AnimationState a) throws Exception { return F_startTick.getInt(a); }
    static void setStartTick(AnimationState a, int v) throws Exception { F_startTick.setInt(a, v); }

    // One mutation op, encoded for emission (SEQ row) and replay against the real object.
    static final class Op {
        final char kind;       // 'A' start, 'I' startIfStopped, 'W' animateWhen, 'X' stop,
                               // 'F' fastForward, 'C' copyFrom
        final int tick;        // start/startIfStopped/animateWhen tick, or fastForward ticks, or copyFrom srcTick
        final boolean cond;    // animateWhen condition
        final float scale;     // fastForward timeScale
        Op(char kind, int tick, boolean cond, float scale) {
            this.kind = kind; this.tick = tick; this.cond = cond; this.scale = scale;
        }
        String encode() {
            switch (kind) {
                case 'A': return "A:" + tick;
                case 'I': return "I:" + tick;
                case 'W': return "W:" + (cond ? 1 : 0) + ":" + tick;
                case 'X': return "X";
                case 'F': return "F:" + tick + ":" + f(scale);
                case 'C': return "C:" + tick;
                default:  return "X";
            }
        }
    }

    static Op start(int t)              { return new Op('A', t, false, 0f); }
    static Op startIfStopped(int t)     { return new Op('I', t, false, 0f); }
    static Op animateWhen(boolean c, int t) { return new Op('W', t, c, 0f); }
    static Op stop()                    { return new Op('X', 0, false, 0f); }
    static Op fastForward(int n, float s) { return new Op('F', n, false, s); }
    static Op copyFrom(int srcTick)     { return new Op('C', srcTick, false, 0f); }

    // Apply a single op to a real AnimationState.
    static void apply(AnimationState a, Op op) throws Exception {
        switch (op.kind) {
            case 'A': a.start(op.tick); break;
            case 'I': a.startIfStopped(op.tick); break;
            case 'W': a.animateWhen(op.cond, op.tick); break;
            case 'X': a.stop(); break;
            case 'F': a.fastForward(op.tick, op.scale); break;
            case 'C': {
                AnimationState src = new AnimationState();
                setStartTick(src, op.tick);  // build a source with a chosen startTick
                a.copyFrom(src);
                break;
            }
            default: a.stop(); break;
        }
    }

    // Rebuild a fresh object and apply the first `count` ops of the sequence.
    static AnimationState build(List<Op> seq, int count) throws Exception {
        AnimationState a = new AnimationState();
        for (int i = 0; i < count && i < seq.size(); i++) apply(a, seq.get(i));
        return a;
    }

    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {
        }
        resolve();

        // ── A battery of realistic op sequences mirroring tick-driven entity animation. ──
        List<List<Op>> sequences = new ArrayList<>();

        // 0: squid swim cadence — animateWhen(true) each tick keeps it started at the
        //    first started tick; flipping condition false stops it.
        {
            List<Op> s = new ArrayList<>();
            for (int t = 100; t < 110; t++) s.add(animateWhen(true, t));   // started at 100, stays
            for (int t = 110; t < 113; t++) s.add(animateWhen(false, t));  // stops
            for (int t = 113; t < 118; t++) s.add(animateWhen(true, t));   // restarts at 113
            sequences.add(s);
        }
        // 1: explicit start then queried over many ages (drives getTimeInMillis).
        {
            List<Op> s = new ArrayList<>();
            s.add(start(0));
            sequences.add(s);
        }
        // 2: startIfStopped semantics — first wins, later calls are no-ops until stop.
        {
            List<Op> s = new ArrayList<>();
            s.add(startIfStopped(50));
            s.add(startIfStopped(60));   // no-op (already started)
            s.add(startIfStopped(70));   // no-op
            s.add(stop());
            s.add(startIfStopped(80));   // now takes effect
            sequences.add(s);
        }
        // 3: fastForward with the canonical positive timeScale (shifts startTick back).
        {
            List<Op> s = new ArrayList<>();
            s.add(start(1000));
            s.add(fastForward(5, 1.0f));     // startTick -= 5
            s.add(fastForward(10, 0.5f));    // startTick -= (int)(10*0.5)=5
            s.add(fastForward(3, 2.0f));     // startTick -= 6
            s.add(fastForward(7, 0.0f));     // startTick -= 0
            sequences.add(s);
        }
        // 4: fastForward on a STOPPED state is a no-op (guard) then after start.
        {
            List<Op> s = new ArrayList<>();
            s.add(fastForward(100, 1.0f));   // no-op: not started
            s.add(start(200));
            s.add(fastForward(100, 1.0f));   // startTick -= 100 -> 100
            sequences.add(s);
        }
        // 5: fastForward fractional/odd scales exercising (int)(ticks*scale) truncation.
        {
            List<Op> s = new ArrayList<>();
            s.add(start(500));
            s.add(fastForward(3, 0.3f));     // (int)(0.9000001)=0
            s.add(fastForward(3, 0.4f));     // (int)(1.2)=1
            s.add(fastForward(7, 0.7f));     // (int)(4.9)=4
            s.add(fastForward(9, 1.5f));     // (int)(13.5)=13
            s.add(fastForward(11, 1.3f));    // (int)(14.3...)=14
            sequences.add(s);
        }
        // 6: copyFrom (server->client sync): copy a started, then a stopped (MIN_VALUE) src.
        {
            List<Op> s = new ArrayList<>();
            s.add(start(7));
            s.add(copyFrom(12345));          // copy a started src
            s.add(copyFrom(Integer.MIN_VALUE)); // copy a stopped src -> becomes stopped
            s.add(copyFrom(-42));            // copy a (negative-but-not-MIN) started src
            sequences.add(s);
        }
        // 7: negative and large startTicks (tickCount is a long-running int counter).
        {
            List<Op> s = new ArrayList<>();
            s.add(start(-1000));
            s.add(stop());
            s.add(start(2000000000));        // near Integer.MAX_VALUE region
            s.add(start(-2000000000));
            sequences.add(s);
        }
        // 8: animateWhen toggling — exercises the start-once / stop branches densely.
        {
            List<Op> s = new ArrayList<>();
            boolean[] conds = {true, true, false, true, false, false, true, true};
            for (int i = 0; i < conds.length; i++) s.add(animateWhen(conds[i], 300 + i));
            sequences.add(s);
        }
        // 9: fastForward whose product overflows int range -> (int) saturates (JLS 5.1.3),
        //    then the int subtraction wraps two's-complement.
        {
            List<Op> s = new ArrayList<>();
            s.add(start(0));
            s.add(fastForward(1000000, 1.0e6f));   // 1e12 -> (int) saturates to INT_MAX
            s.add(start(0));
            s.add(fastForward(1000000, -1.0e6f));  // -1e12 -> (int) saturates to INT_MIN
            sequences.add(s);
        }

        // Ages probed for getTimeInMillis (the render ageInTicks; partials included).
        float[] ages = {
            0.0f, 1.0f, 50.0f, 100.0f, 100.5f, 113.25f, 200.0f, 1000.0f,
            1000.75f, -1000.0f, 12345.5f, 200000000.0f, -200000000.0f, 0.5f, 7.0f
        };

        for (int seqId = 0; seqId < sequences.size(); seqId++) {
            List<Op> seq = sequences.get(seqId);

            // Emit the op stream so the C++ side replays the identical mutations.
            for (Op op : seq) {
                O.println("SEQ\t" + seqId + "\t" + op.encode());
            }

            // Query at every prefix length 0..N (fresh, then after each op).
            for (int idx = 0; idx <= seq.size(); idx++) {
                AnimationState a = build(seq, idx);
                O.println("QSTATE\t" + seqId + "\t" + idx + "\t" + getStartTick(a)
                          + "\t" + (a.isStarted() ? 1 : 0));

                for (float age : ages) {
                    AnimationState aq = build(seq, idx);
                    O.println("QMS\t" + seqId + "\t" + idx + "\t" + f(age)
                              + "\t" + aq.getTimeInMillis(age));
                }
            }
        }
    }
}
