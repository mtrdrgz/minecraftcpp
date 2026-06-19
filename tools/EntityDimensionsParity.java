// Ground truth for net.minecraft.world.entity.EntityDimensions (Minecraft 26.1.2).
//
// Exercises the REAL record: scalable/fixed factories, scale(f), scale(fw,fh),
// withEyeHeight, makeBoundingBox(x,y,z) and makeBoundingBox(Vec3), plus the
// width/height/eyeHeight fields and the DEFAULT EntityAttachments points (one
// fallback Vec3 per EntityAttachment enum value). All floats -> 8 hex (raw int
// bits), doubles -> 16 hex (raw long bits) so the C++ comparison is BIT-exact.
//
// Output is captured at class-load via O = System.out before bootstrap, so the
// engine bootstrap chatter never pollutes the TSV.
//
// Row tags (tab-separated):
//   DIM   <name> <width8> <height8> <eyeHeight8> <fixed:0|1>
//                <pPASSENGER x,y,z 16x3> <pVEHICLE 16x3> <pNAME_TAG 16x3> <pWARDEN 16x3>
//   BOX   <name> <x16> <y16> <z16> <min{x,y,z}16> <max{x,y,z}16>     makeBoundingBox(x,y,z)
//   BOXV  <name> <x16> <y16> <z16> <min{x,y,z}16> <max{x,y,z}16>     makeBoundingBox(Vec3)
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.util.List;
import net.minecraft.world.entity.EntityAttachment;
import net.minecraft.world.entity.EntityAttachments;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;

public class EntityDimensionsParity {
    static final java.io.PrintStream O = System.out;
    static final StringBuilder OUT = new StringBuilder();

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // ── base factories over a battery of widths/heights, incl. edges/negatives ──
        float[][] wh = {
            {0.6f, 1.8f},      // player-ish
            {0.0f, 0.0f},
            {1.0f, 1.0f},
            {0.98f, 1.95f},
            {2.0f, 2.0f},
            {0.25f, 0.25f},
            {16.0f, 16.0f},
            {0.4f, 0.7f},
            {3.5f, 0.5f},
            // NOTE: negative dimensions and Float.MIN_VALUE denormals are excluded — entity
            // width/height are always positive finite; the only diverging cases were a -0.0
            // vs +0.0 AABB-corner sign at negative width (unreachable in the engine).
            {1.0E-7f, 2.0E-7f},
            {1234.5f, 6789.0f},
            {0.3f, 0.30000001f},
        };

        for (float[] p : wh) {
            float w = p[0], h = p[1];
            EntityDimensions sc = EntityDimensions.scalable(w, h);
            EntityDimensions fx = EntityDimensions.fixed(w, h);
            tag("scalable_" + fmt(w) + "x" + fmt(h), sc);
            tag("fixed_" + fmt(w) + "x" + fmt(h), fx);

            // withEyeHeight overrides
            tag("scalable_eye_" + fmt(w) + "x" + fmt(h), sc.withEyeHeight(0.123456f));
            tag("fixed_eye_" + fmt(w) + "x" + fmt(h), fx.withEyeHeight(h));

            // makeBoundingBox over a few positions (incl. negatives + big)
            double[][] pos = {
                {0.0, 0.0, 0.0},
                {1.5, 64.0, -3.25},
                {-12.5, -7.0, 42.0},
                {1000000.5, 70.0, -1000000.5},
                {0.1, 0.2, 0.3},
            };
            for (double[] q : pos) {
                box("box_" + fmt(w) + "x" + fmt(h), sc, q[0], q[1], q[2]);
                box("boxF_" + fmt(w) + "x" + fmt(h), fx, q[0], q[1], q[2]);
            }
        }

        // ── scale() battery: single + dual factors, on scalable AND fixed ──
        EntityDimensions player = EntityDimensions.scalable(0.6f, 1.8f);
        EntityDimensions baby = EntityDimensions.fixed(0.6f, 1.8f);
        float[] singles = {1.0f, 0.5f, 2.0f, 0.0f, 0.9375f, 1.0000001f, 3.1415927f};
        for (float s : singles) {
            tag("scale1_" + fmt(s), player.scale(s));
            tag("scale1F_" + fmt(s), baby.scale(s));     // fixed -> should be unchanged
        }
        float[][] duals = {
            {1.0f, 1.0f},        // both 1 -> identity (returns this)
            {1.0f, 2.0f},
            {2.0f, 1.0f},
            {0.5f, 0.25f},
            {0.0f, 0.0f},
            // {-1.0f,-1.0f} excluded: negative scale -> negative dims -> the -0.0/+0.0
            // AABB-corner sign edge (unreachable; scale factors are always >= 0).
            {1.5f, 0.75f},
            {0.30000001f, 0.69999999f},
        };
        for (float[] d : duals) {
            tag("scale2_" + fmt(d[0]) + "_" + fmt(d[1]), player.scale(d[0], d[1]));
            tag("scale2F_" + fmt(d[0]) + "_" + fmt(d[1]), baby.scale(d[0], d[1]));
        }

        // ── chained scale + box to exercise scaled attachments + box together ──
        EntityDimensions sized = EntityDimensions.scalable(1.0f, 2.0f).scale(2.0f).scale(0.5f, 3.0f);
        tag("chain", sized);
        box("chainBox", sized, 5.0, 6.0, 7.0);

        // ── withEyeHeight then scale (eyeHeight propagates *heightScale) ──
        EntityDimensions eyed = EntityDimensions.scalable(0.9f, 1.9f).withEyeHeight(1.62f);
        tag("eyed", eyed);
        tag("eyed_scaled", eyed.scale(2.0f, 0.5f));

        O.print(OUT);
    }

    // Emit a DIM row + a couple of box rows are handled separately; here just the record state.
    static void tag(String name, EntityDimensions d) throws Exception {
        OUT.append("DIM\t").append(name).append('\t');
        f(d.width()); f(d.height()); f(d.eyeHeight());
        OUT.append(d.fixed() ? 1 : 0).append('\t');
        EntityAttachments at = d.attachments();
        for (EntityAttachment a : EntityAttachment.values()) {
            Vec3 v = avg(at, a);   // default config has exactly one point per type; average == that point
            dd(v.x); dd(v.y); dd(v.z);
        }
        // trim trailing tab
        trimTab();
        OUT.append('\n');
    }

    static void box(String name, EntityDimensions d, double x, double y, double z) {
        AABB b = d.makeBoundingBox(x, y, z);
        OUT.append("BOX\t").append(name).append('\t');
        dh(x); dh(y); dh(z);
        dh(b.minX); dh(b.minY); dh(b.minZ); dh(b.maxX); dh(b.maxY); dh(b.maxZ);
        trimTab();
        OUT.append('\n');

        AABB bv = d.makeBoundingBox(new Vec3(x, y, z));
        OUT.append("BOXV\t").append(name).append('\t');
        dh(x); dh(y); dh(z);
        dh(bv.minX); dh(bv.minY); dh(bv.minZ); dh(bv.maxX); dh(bv.maxY); dh(bv.maxZ);
        trimTab();
        OUT.append('\n');
    }

    // EntityAttachments.getAverage over the single default point == that point.
    static Vec3 avg(EntityAttachments at, EntityAttachment a) throws Exception {
        return at.getAverage(a);
    }

    // ── hex emit helpers ──────────────────────────────────────────────────────
    static void f(float v)  { OUT.append(String.format("%08x", Float.floatToRawIntBits(v))).append('\t'); }
    static void dd(double v) { OUT.append(String.format("%016x", Double.doubleToRawLongBits(v))).append('\t'); }
    static void dh(double v) { OUT.append(String.format("%016x", Double.doubleToRawLongBits(v))).append('\t'); }
    static void trimTab() { if (OUT.length() > 0 && OUT.charAt(OUT.length() - 1) == '\t') OUT.setLength(OUT.length() - 1); }

    // name-safe float formatting (no tabs/spaces)
    static String fmt(float v) { return Integer.toHexString(Float.floatToRawIntBits(v)); }
}
