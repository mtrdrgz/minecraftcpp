// Ground-truth generator for net.minecraft.world.scores.ReadOnlyScoreInfo — the
// PORTABLE, self-contained subset of the interface: its two pure data accessors
//
//     int value();
//     boolean isLocked();
//
// ReadOnlyScoreInfo is an interface; its only concrete net.minecraft implementation
// is net.minecraft.world.scores.Score. We therefore exercise the interface contract
// through a REAL Score instance UPCAST to ReadOnlyScoreInfo, reading value()/isLocked()
// strictly via the interface type. The mc::world::scores::Score C++ port (which
// `implements ReadOnlyScoreInfo` 1:1) must reproduce every emitted state bit-for-bit
// (value compared as a decimal int32, locked as 0/1).
//
// DELIBERATELY NOT exercised (NOT pure — registry/component/network/codec coupled,
// and so NOT ported; listed as unported in the C++ gate):
//   - NumberFormat numberFormat()                          (net...chat.numbers.NumberFormat, registry)
//   - MutableComponent formatValue(NumberFormat)           (Component formatting)
//   - static MutableComponent safeFormatValue(info, fmt)   (Component formatting)
// The "getScoreText"/"displayName" helpers named in the assignment do NOT exist on
// ReadOnlyScoreInfo (they live on Objective/PlayerTeam/ScoreHolder and are Component-
// coupled), so there is nothing pure to port from them here.
//
//   tools/run_groundtruth.ps1 -Tool ReadOnlyScoreInfoParity -Out mcpp/build/readonly_score_info.tsv
//
// TAGs emitted (rows are TAB-separated <TAG>\t<inputs>\t<outputs>):
//   INIT                          <value> <locked>   fresh `new Score()` viewed as ReadOnlyScoreInfo
//   SET_V  <arg>                  <value> <locked>   after score.value(arg), read via interface
//   SET_L  <arg(0/1)>             <value> <locked>   after score.setLocked(arg!=0), read via interface
//   REPLAY <id> <step> <op> <arg> <value> <locked>   interface-observed state after each step

import net.minecraft.world.scores.ReadOnlyScoreInfo;
import net.minecraft.world.scores.Score;

public class ReadOnlyScoreInfoParity {
    static final java.io.PrintStream O = System.out;

    // Read the two pure accessors STRICTLY through the ReadOnlyScoreInfo interface.
    static String st(ReadOnlyScoreInfo s) {
        // value: decimal int32 ; locked: 0/1
        return s.value() + "\t" + (s.isLocked() ? 1 : 0);
    }

    public static void main(String[] args) throws Exception {
        // Score/ReadOnlyScoreInfo touch no registry, but bootstrapping is harmless and
        // guards against any static-init order surprises in net.minecraft.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {}

        // ── 1) Constructor defaults observed via the interface: value==0, locked==true. ──
        {
            ReadOnlyScoreInfo s = new Score();
            O.println("INIT\t" + st(s));
        }

        // ── 2) value(int) over a finite int32 battery, each from a FRESH instance so the
        //       prior locked==true default is observed alongside the set, read via interface.
        //       Java int assignment is a plain copy (no arithmetic / no wrap math); we still
        //       probe int32 extremes for completeness. FINITE/PHYSICAL ints only.
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
            ReadOnlyScoreInfo r = s;           // upcast — read via interface
            O.println("SET_V\t" + v + "\t" + st(r));
        }

        // ── 3) setLocked(boolean) from a fresh instance (default true), read via interface. ──
        for (int b = 0; b <= 1; b++) {
            Score s = new Score();
            s.setLocked(b != 0);
            ReadOnlyScoreInfo r = s;
            O.println("SET_L\t" + b + "\t" + st(r));
        }

        // ── 4) REPLAY: deterministic interleaved sequences on ONE instance, emitting the
        //       interface-observed state after EVERY step. Verifies value() and isLocked()
        //       are fully independent fields with no cross-effect, and that repeated sets
        //       overwrite (not accumulate — there is no add()).
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
            ReadOnlyScoreInfo r = s;           // same identity, read via interface each step
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
                O.println("REPLAY\t" + id + "\t" + step + "\t" + op + "\t" + arg + "\t" + st(r));
            }
        }
    }
}
