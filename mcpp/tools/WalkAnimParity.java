// Ground-truth generator for net.minecraft.world.entity.WalkAnimationState using
// the REAL decompiled 26.1.2 class. The C++ test (WalkAnimParityTest) replays the
// identical sequence of update()/setSpeed()/stop() ops on a 1:1 port and must match
// every queried output bit-for-bit; floats are exchanged as raw IEEE-754 bit
// patterns (%08x of Float.floatToRawIntBits).
//
//   tools/run_groundtruth.ps1 -Tool WalkAnimParity -Out mcpp/build/walk_anim.tsv
//
// The class is a stateful float accumulator:
//   update(targetSpeed, factor, positionScale):
//     speedOld = speed;
//     speed    = speed + (targetSpeed - speed) * factor;
//     position = position + speed;
//     positionScale = positionScale;
//   setSpeed(s): speed = s;
//   stop(): speedOld = speed = position = 0;
//   speed()                  -> speed
//   speed(partial)           -> Math.min(Mth.lerp(partial, speedOld, speed), 1)
//   position()               -> position * positionScale
//   position(partial)        -> (position - speed*(1-partial)) * positionScale
//   isMoving()               -> speed > 1e-5
//
// speedOld and position have no Java getters, so we never read them directly: we
// drive the public mutators and query only the public accessors. The op stream is
// emitted as a SEQ header (so the C++ side replays the exact same mutations) plus
// one query row per probe.
//
// Rows (all tab-separated):
//   SEQ    <seqId> <op>                          -- mutation applied, in order:
//                                                     U:<tBits>:<fBits>:<psBits>  (update)
//                                                     S:<sBits>                   (setSpeed)
//                                                     X                           (stop)
//   QSPEED <seqId> <afterOpIdx>                 <speedBits>            -- speed()
//   QSPEEDP<seqId> <afterOpIdx> <partialBits>   <speedPartialBits>    -- speed(partial)
//   QPOS   <seqId> <afterOpIdx>                 <positionBits>        -- position()
//   QPOSP  <seqId> <afterOpIdx> <partialBits>   <positionPartialBits> -- position(partial)
//   QMOVE  <seqId> <afterOpIdx>                 <isMoving>            -- isMoving() (0/1)
//
// <afterOpIdx> = number of ops from the SEQ already applied when the query is taken
// (0 = fresh state, N = after all N ops). The C++ side rebuilds a fresh object,
// applies the first <afterOpIdx> ops, then queries.

import net.minecraft.world.entity.WalkAnimationState;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

public class WalkAnimParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // One mutation op encoded both for emission (SEQ row) and replay against the
    // real object.
    static final class Op {
        final char kind;          // 'U' update, 'S' setSpeed, 'X' stop
        final float t, fac, ps;   // update args
        final float s;            // setSpeed arg
        Op(float t, float fac, float ps) { this.kind = 'U'; this.t = t; this.fac = fac; this.ps = ps; this.s = 0; }
        Op(float s) { this.kind = 'S'; this.s = s; this.t = 0; this.fac = 0; this.ps = 0; }
        Op() { this.kind = 'X'; this.s = 0; this.t = 0; this.fac = 0; this.ps = 0; }

        String encode() {
            switch (kind) {
                case 'U': return "U:" + f(t) + ":" + f(fac) + ":" + f(ps);
                case 'S': return "S:" + f(s);
                default:  return "X";
            }
        }
    }

    static Op upd(float t, float fac, float ps) { return new Op(t, fac, ps); }
    static Op set(float s) { return new Op(s); }
    static Op stop() { return new Op(); }

    // Apply a single op to the real WalkAnimationState. stop() / setSpeed are
    // public; update is public too.
    static void apply(WalkAnimationState w, Op op) {
        switch (op.kind) {
            case 'U': w.update(op.t, op.fac, op.ps); break;
            case 'S': w.setSpeed(op.s); break;
            default:  w.stop(); break;
        }
    }

    // Reflectively resolve the overloaded public accessors so we read the REAL
    // class behaviour without touching private fields.
    static Method M_speed0, M_speedP, M_pos0, M_posP, M_isMoving;

    static void resolve() throws Exception {
        M_speed0   = WalkAnimationState.class.getMethod("speed");
        M_speedP   = WalkAnimationState.class.getMethod("speed", float.class);
        M_pos0     = WalkAnimationState.class.getMethod("position");
        M_posP     = WalkAnimationState.class.getMethod("position", float.class);
        M_isMoving = WalkAnimationState.class.getMethod("isMoving");
    }

    // Rebuild a fresh object and apply the first `count` ops of the sequence.
    static WalkAnimationState build(List<Op> seq, int count) {
        WalkAnimationState w = new WalkAnimationState();
        for (int i = 0; i < count && i < seq.size(); i++) apply(w, seq.get(i));
        return w;
    }

    static float qSpeed(WalkAnimationState w) throws Exception { return (Float) M_speed0.invoke(w); }
    static float qSpeedP(WalkAnimationState w, float p) throws Exception { return (Float) M_speedP.invoke(w, p); }
    static float qPos(WalkAnimationState w) throws Exception { return (Float) M_pos0.invoke(w); }
    static float qPosP(WalkAnimationState w, float p) throws Exception { return (Float) M_posP.invoke(w, p); }
    static boolean qMove(WalkAnimationState w) throws Exception { return (Boolean) M_isMoving.invoke(w); }

    public static void main(String[] args) throws Exception {
        // WalkAnimationState has no static init that needs bootstrapping, but be
        // defensive in case Mth/Math reference helpers down the line.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {
        }
        resolve();

        // ── A battery of realistic op sequences. ──
        // Values mirror how LivingEntity actually drives it: factor 0.4F is the
        // canonical smoothing constant, targetSpeed is horizontal limb speed,
        // positionScale is the per-entity scale (1.0F default, others for scaled mobs).
        List<List<Op>> sequences = new ArrayList<>();

        // 0: canonical ramp-up to a walk then ramp-down (factor 0.4, scale 1.0).
        {
            List<Op> s = new ArrayList<>();
            for (int i = 0; i < 12; i++) s.add(upd(0.55F, 0.4F, 1.0F));   // accelerate toward 0.55
            for (int i = 0; i < 12; i++) s.add(upd(0.0F, 0.4F, 1.0F));    // decelerate to ~0
            sequences.add(s);
        }
        // 1: sprint-class higher target, then stop() mid-stream, then resume.
        {
            List<Op> s = new ArrayList<>();
            for (int i = 0; i < 8; i++) s.add(upd(0.9F, 0.4F, 1.0F));
            s.add(stop());
            for (int i = 0; i < 6; i++) s.add(upd(0.4F, 0.4F, 1.0F));
            sequences.add(s);
        }
        // 2: setSpeed injection (server sync path) interleaved with updates.
        {
            List<Op> s = new ArrayList<>();
            s.add(upd(0.3F, 0.4F, 1.0F));
            s.add(set(0.75F));
            s.add(upd(0.3F, 0.4F, 1.0F));
            s.add(set(0.0F));
            s.add(upd(0.6F, 0.4F, 1.0F));
            for (int i = 0; i < 5; i++) s.add(upd(0.6F, 0.4F, 1.0F));
            sequences.add(s);
        }
        // 3: varied positionScale (scaled mobs / baby mobs) — exercises position()*scale.
        {
            List<Op> s = new ArrayList<>();
            float[] scales = {1.0F, 0.5F, 1.5F, 0.25F, 2.0F, 0.75F};
            for (int i = 0; i < 18; i++) s.add(upd(0.5F, 0.4F, scales[i % scales.length]));
            sequences.add(s);
        }
        // 4: varied smoothing factors (overshoot region: factor 1.0 jumps fully,
        //    factor 1.3 overshoots — both are legal float math, exercise lerp).
        {
            List<Op> s = new ArrayList<>();
            float[] facs = {0.4F, 1.0F, 0.1F, 0.7F, 1.3F, 0.5F};
            for (int i = 0; i < 18; i++) s.add(upd(0.6F, facs[i % facs.length], 1.0F));
            sequences.add(s);
        }
        // 5: tiny speeds straddling the isMoving() threshold (1e-5F).
        {
            List<Op> s = new ArrayList<>();
            s.add(upd(2.0e-5F, 1.0F, 1.0F));   // speed = 2e-5 -> moving
            s.add(upd(5.0e-6F, 1.0F, 1.0F));   // speed = 5e-6 -> not moving
            s.add(upd(1.0e-5F, 1.0F, 1.0F));   // speed = exactly 1e-5 -> NOT moving (strict >)
            s.add(upd(1.00001e-5F, 1.0F, 1.0F)); // just above -> moving
            s.add(set(1.0e-5F));               // exactly threshold via setSpeed
            s.add(set(0.0F));
            sequences.add(s);
        }
        // 6: long steady walk to let position accumulate large, then partials.
        {
            List<Op> s = new ArrayList<>();
            for (int i = 0; i < 40; i++) s.add(upd(0.65F, 0.4F, 1.0F));
            sequences.add(s);
        }
        // 7: immediate stop on fresh, repeated stops, then a single update.
        {
            List<Op> s = new ArrayList<>();
            s.add(stop());
            s.add(stop());
            s.add(upd(0.5F, 0.4F, 1.0F));
            s.add(stop());
            sequences.add(s);
        }
        // 8: fractional targets and scales mixing — general coverage.
        {
            List<Op> s = new ArrayList<>();
            float[] t  = {0.123F, 0.456F, 0.789F, 0.234F, 0.987F};
            float[] sc = {1.0F, 1.0625F, 0.9375F, 1.25F, 0.8F};
            for (int i = 0; i < 20; i++) s.add(upd(t[i % t.length], 0.4F, sc[i % sc.length]));
            sequences.add(s);
        }

        // Partials probed at each query point (the render partialTick in [0,1]).
        float[] partials = {0.0F, 0.25F, 0.5F, 0.75F, 1.0F, 0.1F, 0.9F, 0.333333F, 0.666667F};

        for (int seqId = 0; seqId < sequences.size(); seqId++) {
            List<Op> seq = sequences.get(seqId);

            // Emit the op stream so the C++ side replays the identical mutations.
            for (Op op : seq) {
                O.println("SEQ\t" + seqId + "\t" + op.encode());
            }

            // Query at every prefix length 0..N (fresh, then after each op).
            for (int idx = 0; idx <= seq.size(); idx++) {
                WalkAnimationState w = build(seq, idx);

                O.println("QSPEED\t" + seqId + "\t" + idx + "\t" + f(qSpeed(w)));
                O.println("QPOS\t" + seqId + "\t" + idx + "\t" + f(qPos(w)));
                O.println("QMOVE\t" + seqId + "\t" + idx + "\t" + (qMove(w) ? 1 : 0));

                for (float p : partials) {
                    // Rebuild for each partial query so partials never alias state
                    // (the accessors are const, but rebuild keeps the C++ replay
                    // contract dead simple and identical).
                    WalkAnimationState ws = build(seq, idx);
                    O.println("QSPEEDP\t" + seqId + "\t" + idx + "\t" + f(p) + "\t" + f(qSpeedP(ws, p)));
                    WalkAnimationState wp = build(seq, idx);
                    O.println("QPOSP\t" + seqId + "\t" + idx + "\t" + f(p) + "\t" + f(qPosP(wp, p)));
                }
            }
        }
    }
}
