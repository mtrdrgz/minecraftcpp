// Ground truth for net.minecraft.world.entity.EntityAttachments (Minecraft 26.1.2).
//
// Exercises the REAL class via its public API only:
//   EntityAttachments.builder().attach(...).build(w, h)
//   EntityAttachments.createDefault(w, h)
//   scale(x, y, z)
//   getClamped(attachment, index, rotY)   -> transformPoint (yRot via Mth table)
//   get(attachment, index, rotY)          -> transformPoint (yRot via Mth table)
//   getNullable(attachment, index, rotY)  -> null when out of range
//   getAverage(attachment)                -> sum then scale(1.0F / size)
//
// transformPoint(point, rotY) is private static, but it is fully covered by
// get/getClamped (which call it), so we never reflect into it.
//
// Output captured at class-load via O = System.out before bootstrap, so engine
// bootstrap chatter never pollutes the TSV. floats -> 8 hex (raw int bits),
// doubles -> 16 hex (raw long bits) for BIT-exact C++ comparison.
//
// Row tags (tab-separated):
//   CLAMPED  <cfg> <attach_ord> <index> <rotY8> <x16> <y16> <z16>
//   GET      <cfg> <attach_ord> <index> <rotY8> <x16> <y16> <z16>
//   NULL     <cfg> <attach_ord> <index> <rotY8> <isnull:0|1> <x16> <y16> <z16>
//   AVG      <cfg> <attach_ord> <x16> <y16> <z16>
//   POINT    <cfg> <attach_ord> <count> [<x16> <y16> <z16>]*    (raw stored points,
//                                                                via getClamped rotY=0)
//
// <cfg> is a stable string key the C++ side replays to rebuild the same
// EntityAttachments. See the C++ test for the matching builders.
import java.util.List;
import net.minecraft.world.entity.EntityAttachment;
import net.minecraft.world.entity.EntityAttachments;
import net.minecraft.world.phys.Vec3;

public class EntityAttachmentsParity {
    static final java.io.PrintStream O = System.out;
    static final StringBuilder OUT = new StringBuilder();

    static String f(float v)  { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    static final EntityAttachment[] A = EntityAttachment.values();

    // A battery of yRot angles (degrees) — finite/physical only.
    static final float[] ROTS = {
        0.0f, 90.0f, 180.0f, 270.0f, 45.0f, -45.0f, 30.0f, -30.0f,
        360.0f, 22.5f, 135.0f, -135.0f, 1.0f, -1.0f, 359.0f, 0.5f,
        12.34f, 178.9f, -270.0f, 720.0f
    };

    // Indices to probe (covers in-range, clamp-below, clamp-above, exactly-size).
    static final int[] IDX = { -3, -1, 0, 1, 2, 3, 4, 5, 7, 100 };

    // (width, height) batteries for default/builder construction.
    static final float[][] WH = {
        {0.6f, 1.8f}, {0.0f, 0.0f}, {1.0f, 1.0f}, {0.98f, 1.95f},
        {2.0f, 2.0f}, {0.25f, 0.25f}, {16.0f, 16.0f}, {0.4f, 0.7f},
        {3.5f, 0.5f}, {1234.5f, 6789.0f}, {0.3f, 0.30000001f}
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // ── 1) DEFAULT-constructed attachments over the WH battery ──
        for (float[] p : WH) {
            float w = p[0], h = p[1];
            String cfg = "def_" + Integer.toHexString(Float.floatToRawIntBits(w))
                       + "_" + Integer.toHexString(Float.floatToRawIntBits(h));
            EntityAttachments at = EntityAttachments.createDefault(w, h);
            emitAll(cfg, at);
        }

        // ── 2) BUILDER multi-point configurations ──
        // (a) PASSENGER with 3 explicit points; others fall back.
        {
            float w = 0.6f, h = 1.8f;
            EntityAttachments at = EntityAttachments.builder()
                .attach(EntityAttachment.PASSENGER, 0.0f, 1.0f, 0.0f)
                .attach(EntityAttachment.PASSENGER, 0.5f, 1.2f, -0.25f)
                .attach(EntityAttachment.PASSENGER, -0.5f, 0.8f, 0.25f)
                .build(w, h);
            emitAll("b_pass3", at);
        }
        // (b) Every type explicitly attached, multiple points each.
        {
            EntityAttachments at = EntityAttachments.builder()
                .attach(EntityAttachment.PASSENGER, 0.1f, 0.2f, 0.3f)
                .attach(EntityAttachment.PASSENGER, 1.1f, 1.2f, 1.3f)
                .attach(EntityAttachment.VEHICLE, -1.0f, 0.0f, 2.0f)
                .attach(EntityAttachment.NAME_TAG, 0.0f, 2.5f, 0.0f)
                .attach(EntityAttachment.NAME_TAG, 0.0f, 2.7f, 0.0f)
                .attach(EntityAttachment.NAME_TAG, 0.0f, 2.9f, 0.0f)
                .attach(EntityAttachment.NAME_TAG, 0.0f, 3.1f, 0.0f)
                .attach(EntityAttachment.WARDEN_CHEST, 3.3f, -4.4f, 5.5f)
                .build(1.0f, 2.0f);
            emitAll("b_all", at);
        }
        // (c) Single explicit point on WARDEN_CHEST via the Vec3 overload (others fallback).
        {
            EntityAttachments at = EntityAttachments.builder()
                .attach(EntityAttachment.WARDEN_CHEST, new Vec3(7.0, 8.0, 9.0))
                .build(0.9f, 1.9f);
            emitAll("b_warden1", at);
        }

        // ── 3) SCALE applied to builder/default configs ──
        {
            EntityAttachments base = EntityAttachments.builder()
                .attach(EntityAttachment.PASSENGER, 0.1f, 0.2f, 0.3f)
                .attach(EntityAttachment.PASSENGER, 1.1f, 1.2f, 1.3f)
                .attach(EntityAttachment.VEHICLE, -1.0f, 0.5f, 2.0f)
                .attach(EntityAttachment.WARDEN_CHEST, 3.3f, -4.4f, 5.5f)
                .build(1.0f, 2.0f);
            float[][] scales = {
                {1.0f, 1.0f, 1.0f}, {2.0f, 0.5f, 3.0f}, {0.0f, 0.0f, 0.0f},
                {1.5f, 1.5f, 1.5f}, {-1.0f, 2.0f, -0.5f}, {0.3f, 0.7f, 1.1f}
            };
            int si = 0;
            for (float[] s : scales) {
                EntityAttachments scaled = base.scale(s[0], s[1], s[2]);
                emitAll("b_scale" + (si++), scaled);
            }
        }
        // default-config scaled too
        {
            EntityAttachments base = EntityAttachments.createDefault(0.6f, 1.8f);
            EntityAttachments scaled = base.scale(2.0f, 0.5f, 2.0f);
            emitAll("def_scale", scaled);
        }

        O.print(OUT);
    }

    // Emit POINT (raw stored points), CLAMPED, GET, NULL, AVG for every attachment
    // type the config has, across the index + rot batteries.
    static void emitAll(String cfg, EntityAttachments at) {
        for (int ao = 0; ao < A.length; ao++) {
            EntityAttachment a = A[ao];

            // Number of stored points: probe getClamped at a huge index (clamps to last)
            // and getNullable upward until null to recover the count. getAverage exists
            // for any non-empty list (every built type is non-empty), so use it as a
            // presence check guarded by try.
            int count = pointCount(at, a);

            // POINT rows: the raw stored points (getClamped rotY=0 == identity yRot,
            // which returns the point unchanged because cos(0)=1, sin(0)=0).
            OUT.append("POINT\t").append(cfg).append('\t').append(ao).append('\t').append(count);
            for (int i = 0; i < count; i++) {
                Vec3 v = at.getClamped(a, i, 0.0f);
                OUT.append('\t').append(d(v.x)).append('\t').append(d(v.y)).append('\t').append(d(v.z));
            }
            OUT.append('\n');

            // AVG (only meaningful for non-empty; every built type is non-empty).
            if (count > 0) {
                Vec3 avg = at.getAverage(a);
                OUT.append("AVG\t").append(cfg).append('\t').append(ao).append('\t')
                   .append(d(avg.x)).append('\t').append(d(avg.y)).append('\t').append(d(avg.z)).append('\n');
            }

            for (float rotY : ROTS) {
                for (int idx : IDX) {
                    // CLAMPED — always valid for non-empty lists.
                    if (count > 0) {
                        Vec3 c = at.getClamped(a, idx, rotY);
                        OUT.append("CLAMPED\t").append(cfg).append('\t').append(ao).append('\t')
                           .append(idx).append('\t').append(f(rotY)).append('\t')
                           .append(d(c.x)).append('\t').append(d(c.y)).append('\t').append(d(c.z)).append('\n');
                    }

                    // NULL / GET — getNullable returns null outside [0, size).
                    Vec3 n = at.getNullable(a, idx, rotY);
                    boolean isNull = (n == null);
                    OUT.append("NULL\t").append(cfg).append('\t').append(ao).append('\t')
                       .append(idx).append('\t').append(f(rotY)).append('\t')
                       .append(isNull ? 1 : 0);
                    if (!isNull) {
                        OUT.append('\t').append(d(n.x)).append('\t').append(d(n.y)).append('\t').append(d(n.z));
                    }
                    OUT.append('\n');

                    // GET throws when null; only emit the value when present.
                    if (!isNull) {
                        Vec3 g = at.get(a, idx, rotY);
                        OUT.append("GET\t").append(cfg).append('\t').append(ao).append('\t')
                           .append(idx).append('\t').append(f(rotY)).append('\t')
                           .append(d(g.x)).append('\t').append(d(g.y)).append('\t').append(d(g.z)).append('\n');
                    }
                }
            }
        }
    }

    // Recover the point count using only the public API: getNullable returns non-null
    // for index in [0, size) and null at size and beyond.
    static int pointCount(EntityAttachments at, EntityAttachment a) {
        int n = 0;
        while (at.getNullable(a, n, 0.0f) != null) {
            n++;
            if (n > 1000) break;  // safety
        }
        return n;
    }
}
