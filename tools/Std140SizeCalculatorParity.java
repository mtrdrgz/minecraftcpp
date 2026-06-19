// Ground-truth generator for com.mojang.blaze3d.buffers.Std140SizeCalculator
// (Minecraft 26.1.2) using the REAL decompiled class. Std140SizeCalculator is a
// stateful builder that computes the byte size of a std140 UBO layout: each
// put<Type>() aligns the running size up to the type's std140 base alignment
// (via net.minecraft.util.Mth.roundToward) and then advances it by the type's
// std140 size. Pure 32-bit integer arithmetic; no GL — but we run the standard
// bootstrap defensively anyway (guarded; this class needs none).
//
//   mcpp/tools/run_groundtruth.ps1 -Tool Std140SizeCalculatorParity -Out mcpp/build/std140_size_calculator.tsv
//
// We script a sequence of ops per scenario (each op is one of the builder methods,
// or an explicit "align <n>"), apply them in order to a fresh REAL instance, and
// after EACH op emit get(). The C++ test re-drives an identical sequence on its
// port and compares every running size exactly (decimal int32). Emitting the
// size after every op — not just the final get() — pins down the per-step
// alignment behaviour, not merely the total.
//
// TSV row format (tab-separated), dispatched by leading TAG in the C++ test:
//   SEQ  <n>  <op_0> <size_0>  <op_1> <size_1> ... <op_{n-1}> <size_{n-1}>
// where <op_k> is the op token (e.g. "putVec3", or "align:8") and <size_k> is
// get() after applying op_k. All sizes decimal int32.

import com.mojang.blaze3d.buffers.Std140SizeCalculator;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

public class Std140SizeCalculatorParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Std140SizeCalculator is pure and needs no bootstrap; guard anyway in
        // case class init ever pulls something in (it does not for this class).
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — not required for Std140SizeCalculator.
        }

        // Each scenario is an ordered list of op tokens. "align:N" calls align(N);
        // any other token is the corresponding zero-arg builder method name. We
        // exercise: every put type in isolation (from offset 0), each type stacked
        // after a 4-byte scalar (forcing a real alignment jump), vec3's trailing
        // padding (vec3 then float, ivec3 then int), mat4 alignment, redundant
        // explicit align() (no-op when already aligned), and several real renderer
        // UBO layouts so the canonical sizes are pinned. Also a couple of explicit
        // odd align() values (1, 2, 3, 5, 16) to exercise Mth.roundToward directly.
        List<String[]> scenarios = new ArrayList<>();

        // -- each put type alone, from a fresh (size==0) builder --
        scenarios.add(new String[]{"putFloat"});
        scenarios.add(new String[]{"putInt"});
        scenarios.add(new String[]{"putVec2"});
        scenarios.add(new String[]{"putIVec2"});
        scenarios.add(new String[]{"putVec3"});
        scenarios.add(new String[]{"putIVec3"});
        scenarios.add(new String[]{"putVec4"});
        scenarios.add(new String[]{"putIVec4"});
        scenarios.add(new String[]{"putMat4f"});

        // -- scalar then each type: forces alignment from offset 4 --
        scenarios.add(new String[]{"putFloat", "putVec2"});   // 4 -> align 8 -> 16
        scenarios.add(new String[]{"putFloat", "putIVec2"});
        scenarios.add(new String[]{"putFloat", "putVec3"});   // 4 -> align 16 -> 32
        scenarios.add(new String[]{"putFloat", "putVec4"});
        scenarios.add(new String[]{"putFloat", "putMat4f"});  // 4 -> align 16 -> 80
        scenarios.add(new String[]{"putInt", "putIVec3"});
        scenarios.add(new String[]{"putInt", "putIVec4"});

        // -- vec3/ivec3 trailing-pad quirk: a following scalar re-aligns to 4 --
        scenarios.add(new String[]{"putVec3", "putFloat"});   // 16 -> align 4 (noop) -> 20
        scenarios.add(new String[]{"putIVec3", "putInt"});    // 16 -> 20
        scenarios.add(new String[]{"putVec3", "putVec3"});    // 16 -> align 16 (noop) -> 32
        scenarios.add(new String[]{"putFloat", "putVec3", "putFloat"}); // 4->16->32->36

        // -- vec2 packing of two floats vs misalignment --
        scenarios.add(new String[]{"putFloat", "putFloat", "putVec2"}); // 4->8->align8(noop)->16
        scenarios.add(new String[]{"putVec2", "putFloat"});  // 8 -> align4(noop) -> 12
        scenarios.add(new String[]{"putVec2", "putVec2"});   // 8 -> 16

        // -- mat4 stress + alignment after mat4 --
        scenarios.add(new String[]{"putMat4f", "putFloat"});  // 64 -> 68
        scenarios.add(new String[]{"putMat4f", "putVec3"});   // 64 -> align16(noop) -> 80
        scenarios.add(new String[]{"putFloat", "putMat4f", "putFloat"}); // 4->80->84

        // -- explicit align() exercising Mth.roundToward at odd multiples --
        scenarios.add(new String[]{"putFloat", "align:1"});   // 4 -> 4 (noop)
        scenarios.add(new String[]{"putFloat", "align:2"});   // 4 -> 4
        scenarios.add(new String[]{"putFloat", "align:3"});   // 4 -> roundToward(4,3)=6
        scenarios.add(new String[]{"putFloat", "align:5"});   // 4 -> roundToward(4,5)=5
        scenarios.add(new String[]{"putFloat", "align:16"});  // 4 -> 16
        scenarios.add(new String[]{"putInt", "putInt", "putInt", "align:16"}); // 12 -> 16
        scenarios.add(new String[]{"putFloat", "putFloat", "putFloat", "align:8"}); // 12 -> 16

        // -- real renderer UBO layouts (pin the canonical sizes) --
        // CloudRenderer.UBO_SIZE = putVec4().putVec3().putVec3()
        scenarios.add(new String[]{"putVec4", "putVec3", "putVec3"});
        // FogRenderer.FOG_UBO_SIZE = putVec4() + 6 * putFloat()
        scenarios.add(new String[]{"putVec4", "putFloat", "putFloat", "putFloat",
                                   "putFloat", "putFloat", "putFloat"});
        // Lightmap.LIGHTMAP_UBO_SIZE (putVec3 x2 + putFloat x several) — a
        // representative mixed layout (exact ordering pinned regardless).
        scenarios.add(new String[]{"putVec3", "putFloat", "putVec3", "putFloat",
                                   "putFloat", "putFloat"});

        // -- a long mixed chain that wraps through many alignment states --
        scenarios.add(new String[]{
            "putFloat", "putVec2", "putInt", "putVec3", "putMat4f", "putIVec2",
            "putVec4", "putFloat", "putIVec3", "putInt", "putVec3", "putMat4f"
        });

        Method mGet = Std140SizeCalculator.class.getMethod("get");

        for (String[] ops : scenarios) {
            Std140SizeCalculator calc = new Std140SizeCalculator();
            StringBuilder sb = new StringBuilder();
            int n = 0;
            for (String op : ops) {
                if (op.startsWith("align:")) {
                    int a = Integer.parseInt(op.substring("align:".length()));
                    Method mAlign = Std140SizeCalculator.class.getMethod("align", int.class);
                    mAlign.invoke(calc, a);
                } else {
                    Method m = Std140SizeCalculator.class.getMethod(op);
                    m.invoke(calc);
                }
                int size = (int) mGet.invoke(calc);
                sb.append('\t').append(op).append('\t').append(size);
                n++;
            }
            O.println("SEQ\t" + n + sb.toString());
        }
    }
}
