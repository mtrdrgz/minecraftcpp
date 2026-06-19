// Ground-truth generator for net.minecraft.world.scores.Score — the portable,
// self-contained subset of the class: the stateful int `value` and boolean
// `locked` (which defaults to true), driven through their real public accessors
// value(int) / value() / setLocked(boolean) / isLocked() on a REAL instance of
// net.minecraft.world.scores.Score.
//
// The mc::world::scores::Score C++ port must reproduce every emitted state
// bit-for-bit (value compared as a decimal int32, locked as 0/1).
//
// We deliberately do NOT exercise display()/numberFormat()/pack()/Packed.MAP_CODEC:
// those carry Component / NumberFormat / Codec dependencies (registry + bootstrap +
// serialization) that are out of scope for this bounded gate and are NOT ported.
//
//   tools/run_groundtruth.ps1 -Tool ScoreParity -Out mcpp/build/score.tsv
//
// TAGs emitted:
//   INIT                         <value> <locked>   fresh `new Score()` state
//   SET_V  <arg>                 <value> <locked>   after score.value(arg)
//   SET_L  <arg(0/1)>            <value> <locked>   after score.setLocked(arg!=0)
//   REPLAY <id> <step> <op> <arg> <value> <locked>  state after each step of a sequence

import net.minecraft.world.scores.Score;

public class ScoreParity {
    static final java.io.PrintStream O = System.out;

    static String st(Score s) {
        // value: decimal int32 ; locked: 0/1
        return s.value() + "\t" + (s.isLocked() ? 1 : 0);
    }

    public static void main(String[] args) throws Exception {
        // Score itself touches no registry, but bootstrapping is harmless and
        // guards against any static-init order surprises in net.minecraft.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {}

        // ── 1) Constructor defaults: value==0, locked==true. ──────────────────
        {
            Score s = new Score();
            O.println("INIT\t" + st(s));
        }

        // ── 2) value(int) over a finite battery, each from a FRESH instance so
        //       the prior locked==true default is observed alongside the set.
        //       Java int assignment is a plain copy (no arithmetic / no wrap math),
        //       but we still probe the int32 extremes for completeness.
        int[] VALS = {
            0, 1, -1, 2, -2, 7, -7, 100, -100,
            127, 128, -128, 255, 256, 32767, -32768, 65535, 65536,
            1000000, -1000000,
            2147483647, -2147483648,
            2147483646, -2147483647,
            1431655765, -1431655766,
            -559038737  // 0xDEADBEEF
        };
        for (int v : VALS) {
            Score s = new Score();
            s.value(v);
            O.println("SET_V\t" + v + "\t" + st(s));
        }

        // ── 3) setLocked(boolean) from a fresh instance (default true). ────────
        for (int b = 0; b <= 1; b++) {
            Score s = new Score();
            s.setLocked(b != 0);
            O.println("SET_L\t" + b + "\t" + st(s));
        }

        // ── 4) REPLAY: deterministic interleaved sequences on ONE instance,
        //       emitting state after EVERY step. Verifies that value() and
        //       isLocked() are fully independent fields with no cross-effect and
        //       that repeated sets overwrite (not accumulate — there is no add()).
        // Each op token: V=value(arg) , L=setLocked(arg!=0).
        String[][] OPS = {
            // id 0: alternate sets + lock toggles
            { "V:5", "L:0", "V:5", "V:-3", "L:1", "V:0", "L:0", "V:2147483647", "L:1", "V:-2147483648" },
            // id 1: lock churn while value stays put, then big jumps
            { "L:0", "L:0", "L:1", "L:1", "L:0", "V:42", "V:42", "V:-42", "L:1", "V:0" },
            // id 2: monotone-looking value walk (NO accumulation — pure overwrite)
            { "V:1", "V:2", "V:3", "V:100", "V:99", "V:-1", "V:-2", "V:-3", "V:65536", "V:0" },
        };
        for (int id = 0; id < OPS.length; id++) {
            Score s = new Score();
            String[] seq = OPS[id];
            for (int step = 0; step < seq.length; step++) {
                String tok = seq[step];
                char op = tok.charAt(0);
                int arg = Integer.parseInt(tok.substring(2));
                if (op == 'V') {
                    s.value(arg);
                } else { // 'L'
                    s.setLocked(arg != 0);
                }
                O.println("REPLAY\t" + id + "\t" + step + "\t" + op + "\t" + arg + "\t" + st(s));
            }
        }
    }
}
