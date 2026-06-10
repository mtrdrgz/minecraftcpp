// VERIFY ground truth for net.minecraft.world.entity.EntityDimensions (Minecraft 26.1.2).
//
// This is an INDEPENDENT re-verification of the existing certified C++ port
// (mcpp/src/world/entity/EntityDimensions.h, already gated by
// entity_dimensions_parity). It exercises the REAL record via its public API:
//   - EntityDimensions.scalable(w, h)        (scalable factory, fixed=false)
//   - EntityDimensions.fixed(w, h)           (fixed factory, fixed=true)
//   - EntityDimensions.scale(f)              (uniform scale)
//   - EntityDimensions.scale(fw, fh)         (anisotropic scale)
//   - EntityDimensions.withEyeHeight(f)      (eye-height override)
//   - EntityDimensions.makeBoundingBox(x,y,z) / makeBoundingBox(Vec3)
//   - the DEFAULT EntityAttachments points (one fallback Vec3 per enum value)
//
// Unlike the original (which encodes inputs in the row NAME), this tool emits the
// raw float/double INPUTS as explicit columns, so the C++ side rebuilds each
// EntityDimensions directly from the row fields (no name parsing) — making this a
// genuinely independent gate.
//
// Bit-exact wire format: floats -> %08x (Float.floatToRawIntBits),
// doubles -> %016x (Double.doubleToRawLongBits), ints/bools decimal.
//
// Row tags (tab-separated; <op> column drives the C++ reconstruction):
//   DIM  <op> <a8> <b8> <c8> | <width8> <height8> <eyeHeight8> <fixed:0|1>
//             <pPASSENGER{x,y,z}16x3> <pVEHICLE 16x3> <pNAME_TAG 16x3> <pWARDEN 16x3>
//   BOX  <op> <a8> <b8> <c8> | <x16> <y16> <z16> <min{x,y,z}16> <max{x,y,z}16>
//   BOXV <op> <a8> <b8> <c8> | <x16> <y16> <z16> <min{x,y,z}16> <max{x,y,z}16>
//
// <op> is one of:
//   SCAL  a=w b=h            -> scalable(w,h)
//   FIX   a=w b=h            -> fixed(w,h)
//   SCAL_EYE a=w b=h c=eye   -> scalable(w,h).withEyeHeight(eye)
//   FIX_EYE  a=w b=h c=eye   -> fixed(w,h).withEyeHeight(eye)
//   SCAL_S1  a=w b=h c=s     -> scalable(w,h).scale(s)
//   FIX_S1   a=w b=h c=s     -> fixed(w,h).scale(s)
// and the two-factor / chained ops which carry their inputs in the op-specific
// columns documented inline below.
//
// Output is captured at class-load via O = System.out before bootstrap, so the
// engine bootstrap chatter never pollutes the TSV.

import net.minecraft.world.entity.EntityAttachment;
import net.minecraft.world.entity.EntityAttachments;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;

@SuppressWarnings({"unchecked", "deprecation"})
public class EntityDimensionsVerifyParity {
    static final java.io.PrintStream O = System.out;
    static final StringBuilder OUT = new StringBuilder();

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // ── battery of finite, physically-plausible widths/heights ──
        float[][] wh = {
            {0.6f, 1.8f},        // player
            {0.0f, 0.0f},        // degenerate point
            {1.0f, 1.0f},
            {0.98f, 1.95f},      // sneaking player
            {2.0f, 2.0f},
            {0.25f, 0.25f},
            {16.0f, 16.0f},      // ender dragon-ish
            {0.4f, 0.7f},
            {3.5f, 0.5f},        // wide/short
            {1.0E-7f, 2.0E-7f},  // tiny finite
            {1234.5f, 6789.0f},  // huge finite
            {0.3f, 0.30000001f}, // rounding-sensitive neighbours
        };

        double[][] positions = {
            {0.0, 0.0, 0.0},
            {1.5, 64.0, -3.25},
            {-12.5, -7.0, 42.0},
            {1000000.5, 70.0, -1000000.5},
            {0.1, 0.2, 0.3},
        };

        for (float[] p : wh) {
            float w = p[0], h = p[1];
            EntityDimensions sc = EntityDimensions.scalable(w, h);
            EntityDimensions fx = EntityDimensions.fixed(w, h);

            dim("SCAL", w, h, 0.0f, sc);
            dim("FIX", w, h, 0.0f, fx);

            // withEyeHeight overrides
            float eye = 0.123456f;
            dim("SCAL_EYE", w, h, eye, sc.withEyeHeight(eye));
            dim("FIX_EYE", w, h, h, fx.withEyeHeight(h));

            // makeBoundingBox over several positions (incl. negative + huge)
            for (double[] q : positions) {
                box("SCAL", w, h, 0.0f, sc, q[0], q[1], q[2]);
                box("FIX", w, h, 0.0f, fx, q[0], q[1], q[2]);
            }
        }

        // ── single-factor scale battery, on scalable AND fixed ──
        float[][] singleScales = {
            {0.6f, 1.8f, 1.0f},          // identity (returns this)
            {0.6f, 1.8f, 0.5f},
            {0.6f, 1.8f, 2.0f},
            {0.6f, 1.8f, 0.0f},
            {0.6f, 1.8f, 0.9375f},
            {0.6f, 1.8f, 1.0000001f},
            {0.6f, 1.8f, 3.1415927f},
            {1.0f, 2.0f, 1.5f},
        };
        for (float[] s : singleScales) {
            float w = s[0], h = s[1], f = s[2];
            dim("SCAL_S1", w, h, f, EntityDimensions.scalable(w, h).scale(f));
            // fixed -> scale is a no-op (returns this)
            dim("FIX_S1", w, h, f, EntityDimensions.fixed(w, h).scale(f));
        }

        // ── two-factor scale battery. op columns: a=w b=h ; the two factors are
        //    emitted as the box-side c-column pair via dim2(). ──
        float[][] dualScales = {
            {0.6f, 1.8f, 1.0f, 1.0f},    // both 1 -> identity (returns this)
            {0.6f, 1.8f, 1.0f, 2.0f},
            {0.6f, 1.8f, 2.0f, 1.0f},
            {0.6f, 1.8f, 0.5f, 0.25f},
            {0.6f, 1.8f, 0.0f, 0.0f},
            {0.6f, 1.8f, 1.5f, 0.75f},
            {0.6f, 1.8f, 0.30000001f, 0.69999999f},
            {1.0f, 2.0f, 3.0f, 0.5f},
        };
        for (float[] d : dualScales) {
            float w = d[0], h = d[1], fw = d[2], fh = d[3];
            dim2("SCAL_S2", w, h, fw, fh, EntityDimensions.scalable(w, h).scale(fw, fh));
            dim2("FIX_S2", w, h, fw, fh, EntityDimensions.fixed(w, h).scale(fw, fh));
        }

        // ── chained scale + box (exercise scaled attachments + box together) ──
        EntityDimensions chained = EntityDimensions.scalable(1.0f, 2.0f).scale(2.0f).scale(0.5f, 3.0f);
        dimRaw("CHAIN", 0.0f, 0.0f, 0.0f, chained);
        boxRaw("CHAIN", 0.0f, 0.0f, 0.0f, chained, 5.0, 6.0, 7.0);

        // ── withEyeHeight then scale (eyeHeight propagates *heightScale) ──
        EntityDimensions eyed = EntityDimensions.scalable(0.9f, 1.9f).withEyeHeight(1.62f);
        dimRaw("EYED", 0.0f, 0.0f, 0.0f, eyed);
        dimRaw("EYED_SCALED", 0.0f, 0.0f, 0.0f, eyed.scale(2.0f, 0.5f));

        O.print(OUT);
    }

    // DIM row: op + (a,b,c) input floats + record state.
    static void dim(String op, float a, float b, float c, EntityDimensions d) throws Exception {
        OUT.append("DIM\t").append(op).append('\t');
        f(a); f(b); f(c);
        emitState(d);
    }

    // DIM row for two-factor scale: a=w b=h then the two factors stuffed into the
    // record-state preamble is NOT possible, so we widen the input block to 4 floats.
    static void dim2(String op, float w, float h, float fw, float fh, EntityDimensions d) throws Exception {
        OUT.append("DIM\t").append(op).append('\t');
        f(w); f(h); f(fw); f(fh);
        emitState(d);
    }

    // DIM row with no meaningful op inputs (chained / pre-built).
    static void dimRaw(String op, float a, float b, float c, EntityDimensions d) throws Exception {
        OUT.append("DIM\t").append(op).append('\t');
        f(a); f(b); f(c);
        emitState(d);
    }

    static void emitState(EntityDimensions d) throws Exception {
        f(d.width()); f(d.height()); f(d.eyeHeight());
        OUT.append(d.fixed() ? 1 : 0).append('\t');
        EntityAttachments at = d.attachments();
        // default config has exactly one point per type; getAverage == that point.
        for (EntityAttachment a : EntityAttachment.values()) {
            Vec3 v = at.getAverage(a);
            dd(v.x); dd(v.y); dd(v.z);
        }
        trimTab();
        OUT.append('\n');
    }

    static void box(String op, float a, float b, float c, EntityDimensions d, double x, double y, double z) {
        emitBox("BOX", op, a, b, c, d.makeBoundingBox(x, y, z), x, y, z);
        emitBox("BOXV", op, a, b, c, d.makeBoundingBox(new Vec3(x, y, z)), x, y, z);
    }

    static void boxRaw(String op, float a, float b, float c, EntityDimensions d, double x, double y, double z) {
        emitBox("BOX", op, a, b, c, d.makeBoundingBox(x, y, z), x, y, z);
        emitBox("BOXV", op, a, b, c, d.makeBoundingBox(new Vec3(x, y, z)), x, y, z);
    }

    static void emitBox(String tag, String op, float a, float b, float c, AABB box, double x, double y, double z) {
        OUT.append(tag).append('\t').append(op).append('\t');
        f(a); f(b); f(c);
        dd(x); dd(y); dd(z);
        dd(box.minX); dd(box.minY); dd(box.minZ);
        dd(box.maxX); dd(box.maxY); dd(box.maxZ);
        trimTab();
        OUT.append('\n');
    }

    // ── hex emit helpers ──
    static void f(float v)   { OUT.append(String.format("%08x", Float.floatToRawIntBits(v))).append('\t'); }
    static void dd(double v) { OUT.append(String.format("%016x", Double.doubleToRawLongBits(v))).append('\t'); }
    static void trimTab()    { if (OUT.length() > 0 && OUT.charAt(OUT.length() - 1) == '\t') OUT.setLength(OUT.length() - 1); }
}
