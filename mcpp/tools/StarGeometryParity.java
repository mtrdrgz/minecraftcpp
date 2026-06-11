// Ground-truth generator for the star-field geometry produced by
// net.minecraft.client.renderer.SkyRenderer.buildStars() (Minecraft 26.1.2).
//
//   mcpp/tools/run_groundtruth.ps1 -Tool StarGeometryParity -Out mcpp/build/star_geometry.tsv
//
// SkyRenderer.buildStars() itself returns a GpuBuffer (it hands the finished
// vertex bytes to RenderSystem.getDevice()), which requires a live GL device we
// cannot create headless. The POSITION math that decides where every star quad
// sits, however, is pure and is computed entirely from the REAL shipped classes:
//   - net.minecraft.util.RandomSource.createThreadLocalInstance(10842L)
//   - net.minecraft.util.Mth.lengthSquared(float,float,float)
//   - org.joml.Vector3f  (normalize(float), mul(Matrix3fc), negate(), add(...))
//   - org.joml.Matrix3f  (rotateTowards(Vector3fc,Vector3fc), rotateZ(float))
// We re-drive that exact loop here against those real classes and emit, for
// every accepted star, the four quad-corner vertices in emission order. The sink
// (BufferBuilder/GpuBuffer) is the only thing removed — the geometry is genuine
// ground truth, bit-for-bit.
//
// Each vertex float is emitted as Float.floatToRawIntBits (decimal int32), so the
// C++ test compares via std::bit_cast<int32_t> with zero tolerance.
//
// TSV row format (tab-separated), dispatched by leading TAG in the C++ test:
//   STAR  <starIndex>  <x0> <y0> <z0>  <x1> <y1> <z1>  <x2> <y2> <z2>  <x3> <y3> <z3>
// where <starIndex> is the loop counter i (0..1499) of the accepted star and the
// twelve values are floatToRawIntBits of the four (x,y,z) corners in order.
// A trailing COUNT row pins the total number of accepted stars.

import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import org.joml.Matrix3f;
import org.joml.Vector3f;

public class StarGeometryParity {
    static final java.io.PrintStream O = System.out;

    static int bits(float f) {
        return Float.floatToRawIntBits(f);
    }

    public static void main(String[] args) throws Exception {
        // Not strictly required for these classes, but run the standard bootstrap
        // defensively (guarded) per the parity-harness convention.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — RandomSource/Mth/JOML need no bootstrap.
        }

        // ===== faithful re-drive of SkyRenderer.buildStars() position math =====
        RandomSource random = RandomSource.createThreadLocalInstance(10842L);
        float starDistance = 100.0F;
        int accepted = 0;

        for (int i = 0; i < 1500; i++) {
            float x = random.nextFloat() * 2.0F - 1.0F;
            float y = random.nextFloat() * 2.0F - 1.0F;
            float z = random.nextFloat() * 2.0F - 1.0F;
            float starSize = 0.15F + random.nextFloat() * 0.1F;
            float lengthSq = Mth.lengthSquared(x, y, z);
            if (!(lengthSq <= 0.010000001F) && !(lengthSq >= 1.0F)) {
                Vector3f starCenter = new Vector3f(x, y, z).normalize(100.0F);
                float zRot = (float)(random.nextDouble() * (float)Math.PI * 2.0);
                Matrix3f rotation =
                    new Matrix3f().rotateTowards(new Vector3f(starCenter).negate(), new Vector3f(0.0F, 1.0F, 0.0F)).rotateZ(-zRot);
                Vector3f v0 = new Vector3f(starSize, -starSize, 0.0F).mul(rotation).add(starCenter);
                Vector3f v1 = new Vector3f(starSize, starSize, 0.0F).mul(rotation).add(starCenter);
                Vector3f v2 = new Vector3f(-starSize, starSize, 0.0F).mul(rotation).add(starCenter);
                Vector3f v3 = new Vector3f(-starSize, -starSize, 0.0F).mul(rotation).add(starCenter);

                StringBuilder sb = new StringBuilder();
                sb.append("STAR").append('\t').append(i);
                appendV(sb, v0);
                appendV(sb, v1);
                appendV(sb, v2);
                appendV(sb, v3);
                O.println(sb.toString());
                accepted++;
            }
        }

        O.println("COUNT\t" + accepted);
    }

    static void appendV(StringBuilder sb, Vector3f v) {
        sb.append('\t').append(bits(v.x()))
          .append('\t').append(bits(v.y()))
          .append('\t').append(bits(v.z()));
    }
}
