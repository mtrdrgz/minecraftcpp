import net.minecraft.client.particle.Particle;

// Ground truth for mcpp/src/client/particle/LifetimeAlpha.h. Certifies the PURE
// alpha-over-lifetime math of net.minecraft.client.particle.Particle.LifetimeAlpha
// (Minecraft 26.1.2) by driving the REAL record — never replicating its body here.
//
// The record is public (public record LifetimeAlpha(float,float,float,float)) inside the
// public abstract class Particle, so it constructs and dispatches directly. No GL, no
// ClientLevel, no registry — fully deterministic float math.
//
// Row families (all values produced by the REAL method/accessors):
//
//   ALPHA   startBits(f) endBits(f) startAgeBits(f) endAgeBits(f) age lifetime ptBits(f) | outBits(f)
//        Real LifetimeAlpha.currentAlphaForAge(age, lifetime, partialTickTime). Sweeps
//        equal/unequal start-vs-end alphas, varied normalized-age windows, the full
//        in-range plus out-of-range ages (factor clamps in the real method), and a few
//        partialTick fractions. Covers the Mth.equal short-circuit, the inverseLerp
//        window math, and the clampedLerp output blend.
//
//   OPAQUE  startBits(f) endBits(f) startAgeBits(f) endAgeBits(f) | out(0/1)
//        Real LifetimeAlpha.isOpaque() over the same alpha grid (>=1.0F on both ends).
//
// Floats are emitted as raw IEEE-754 bits (%08x) for bit-exact exchange. All inputs are
// finite, physical values (no NaN/Inf/neg-zero) so libm divergence cannot occur.
public class LifetimeAlphaParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    public static void main(String[] args) throws Exception {
        // Plain, physically meaningful alpha endpoints (incl. equal pairs to hit the
        // Mth.equal short-circuit) and normalized-age windows.
        float[] alphas = { 0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1.0f, 0.333333f, 0.666667f, 0.123456f };
        // Normalized-age window endpoints [start, end] used by inverseLerp; keep start < end
        // for the physical case and also exercise a couple of non-default windows.
        float[][] windows = {
            {0.0f, 1.0f}, {0.0f, 0.5f}, {0.5f, 1.0f}, {0.25f, 0.75f}
        };
        int[] ages = { 0, 1, 3, 5, 10, 15, 20, 30, 40 };
        int[] lifetimes = { 1, 4, 20, 100 };
        float[] partials = { 0.0f, 0.25f, 0.5f, 0.75f };

        long n = 0;

        // ALPHA: drive the real currentAlphaForAge across the grid.
        for (float sa : alphas) {
            for (float ea : alphas) {
                for (float[] w : windows) {
                    Particle.LifetimeAlpha la = new Particle.LifetimeAlpha(sa, ea, w[0], w[1]);
                    for (int lifetime : lifetimes) {
                        for (int age : ages) {
                            for (float pt : partials) {
                                float out = la.currentAlphaForAge(age, lifetime, pt);
                                O.println("ALPHA\t" + f(sa) + "\t" + f(ea) + "\t" + f(w[0]) + "\t" + f(w[1])
                                          + "\t" + age + "\t" + lifetime + "\t" + f(pt) + "\t" + f(out));
                                n++;
                            }
                        }
                    }
                }
            }
        }

        // OPAQUE: drive the real isOpaque(). Window endpoints do not affect it, so use a
        // single representative window to avoid redundant rows.
        for (float sa : alphas) {
            for (float ea : alphas) {
                Particle.LifetimeAlpha la = new Particle.LifetimeAlpha(sa, ea, 0.0f, 1.0f);
                int op = la.isOpaque() ? 1 : 0;
                O.println("OPAQUE\t" + f(sa) + "\t" + f(ea) + "\t" + f(0.0f) + "\t" + f(1.0f) + "\t" + op);
                n++;
            }
        }

        System.err.println("rows=" + n);
    }
}
