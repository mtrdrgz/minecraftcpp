import net.minecraft.client.renderer.entity.EndCrystalRenderer;

// Ground truth for mcpp/src/client/renderer/entity/EndCrystalBeamMath.h. Certifies the
// PURE beam-height oscillator of net.minecraft.client.renderer.entity.EndCrystalRenderer
// in Minecraft 26.1.2:
//
//   public static float getY(final float timeInTicks)        (EndCrystalRenderer.java:50)
//
// getY reads no `this` state and no world: its output is a pure function of the single
// float `timeInTicks` (an end-crystal's age in ticks + partialTicks). This driver calls
// the REAL static EndCrystalRenderer.getY over a broad sweep of finite, physically
// plausible inputs and emits the raw IEEE-754 bits of the result, so the C++ getY can be
// compared BIT-FOR-BIT. The method body is NEVER replicated Java-side — we drive the real
// class.
//
//   GETY  timeInTicksBits(f) | outBits(f)
//
// Floats are emitted as raw IEEE-754 bits (%08x) for bit-exact exchange.
public class EndCrystalBeamParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static void emit(float timeInTicks) {
        float out = EndCrystalRenderer.getY(timeInTicks);
        O.println("GETY\t" + f(timeInTicks) + "\t" + f(out));
    }

    public static void main(String[] args) throws Exception {
        // EndCrystalRenderer's <clinit> builds an Identifier; bootstrap so that and any
        // transitive static init resolve exactly as in-game (does not affect getY's math).
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // A dense walk across the first sine period at sub-tick resolution: ages 0..~600
        // ticks (the table index repeats with period 2*PI/0.2 ~= 31.4 ticks, so this spans
        // ~19 full oscillations), stepped by 0.05 ticks to exercise many partialTick phases.
        for (int i = 0; i <= 12000; i++) {
            emit(i * 0.05F);
        }

        // Larger ages: an end crystal can live a long time. Sweep big tick counts (where
        // the float * 0.2F multiply starts to lose low bits) plus fractional partials, to
        // confirm the float->double widening into the sin table stays bit-exact.
        float[] bigBases = {
            1000.0F, 2500.0F, 5000.0F, 12345.0F, 50000.0F, 123456.0F, 1000000.0F, 8388608.0F
        };
        float[] partials = { 0.0F, 0.1F, 0.25F, 0.3333333F, 0.5F, 0.6666667F, 0.75F, 0.9F, 0.99F };
        for (float base : bigBases) {
            for (float p : partials) {
                emit(base + p);
            }
        }

        // Hand-picked edge phases relative to the period (T = 2*PI/0.2 ~= 31.41593 ticks):
        // quarter/half/three-quarter points where Mth.sin's table quantization is most
        // visible, plus a few negative inputs (getY is plain float math with no clamp, so
        // negative timeInTicks is still a well-defined evaluation).
        float[] edges = {
            0.0F, 7.853982F, 15.707963F, 23.561945F, 31.415926F, 39.269909F,
            -0.05F, -1.0F, -7.853982F, -31.415926F, 0.001F, -0.001F,
            3.1415927F, 6.2831855F, 0.2F, 1.6F
        };
        for (float e : edges) {
            emit(e);
        }
    }
}
