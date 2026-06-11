// Ground-truth generator for the PURE billboard-quad geometry produced by
// net.minecraft.client.renderer.state.level.QuadParticleRenderState
// (renderRotatedQuad / renderVertex). Drives the REAL class via reflection and
// captures the four emitted vertices through a no-op VertexConsumer (the sink is
// not GL — it just records position/uv/color/light).
//
// Emits a TSV consumed by render/state/level/QuadParticleRenderStateParityTest.cpp.
// Floats are written as Float.floatToRawIntBits (decimal int) so the C++ side can
// std::bit_cast and compare bit-for-bit.
//
//   mcpp/tools/run_groundtruth.ps1 -Tool QuadParticleRenderStateParity -Out mcpp/build/quad_particle.tsv
//
// Row per particle (CASE), then one VTX row per corner (4):
//   CASE  <id>
//   VTX   <id> <corner 0..3> <xBits> <yBits> <zBits> <uBits> <vBits> <color> <light>

import com.mojang.blaze3d.vertex.VertexConsumer;
import java.lang.reflect.Method;

public final class QuadParticleRenderStateParity {

    // Captures the vertices renderVertex() pushes. addVertex/setUv/setColor/setLight
    // are chained per vertex; setLight delegates to setUv2(packed&65535, packed>>16&65535),
    // which we re-pack to recover the original light int.
    static final class CapturingConsumer implements VertexConsumer {
        float x, y, z, u, v;
        int color, light;
        int finished = 0;
        final float[][] out = new float[4][5];   // x,y,z,u,v per corner
        final int[][] outi = new int[4][2];       // color,light per corner

        public VertexConsumer addVertex(float x, float y, float z) {
            this.x = x; this.y = y; this.z = z; return this;
        }
        public VertexConsumer setColor(int r, int g, int b, int a) {
            this.color = (a << 24) | (r << 16) | (g << 8) | b; return this;
        }
        public VertexConsumer setColor(int c) { this.color = c; return this; }
        public VertexConsumer setUv(float u, float v) { this.u = u; this.v = v; return this; }
        public VertexConsumer setUv1(int u, int v) { return this; }
        public VertexConsumer setUv2(int u, int v) {
            // setLight(packed) -> setUv2(packed&65535, packed>>16&65535); re-pack.
            this.light = (u & 65535) | ((v & 65535) << 16);
            // setLight is the last call in the chain -> commit this vertex.
            out[finished][0] = this.x; out[finished][1] = this.y; out[finished][2] = this.z;
            out[finished][3] = this.u; out[finished][4] = this.v;
            outi[finished][0] = this.color; outi[finished][1] = this.light;
            finished++;
            return this;
        }
        public VertexConsumer setNormal(float x, float y, float z) { return this; }
        public VertexConsumer setLineWidth(float w) { return this; }
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> cls = Class.forName(
            "net.minecraft.client.renderer.state.level.QuadParticleRenderState");
        Object inst = cls.getDeclaredConstructor().newInstance();
        Method m = cls.getDeclaredMethod("renderRotatedQuad",
            VertexConsumer.class,
            float.class, float.class, float.class,        // x,y,z
            float.class, float.class, float.class, float.class, // xRot,yRot,zRot,wRot
            float.class,                                  // scale
            float.class, float.class, float.class, float.class, // u0,u1,v0,v1
            int.class, int.class);                        // color, lightCoords
        m.setAccessible(true);

        StringBuilder sb = new StringBuilder();
        int id = 0;

        // ── representative battery ────────────────────────────────────────────
        // Camera-facing unit quaternions over a sweep of axis-angle rotations
        // (the LOOKAT_XYZ / LOOKAT_Y modes feed normalised quaternions), plus a
        // few non-normalised ones to exercise the 1/(xx+yy+zz+ww) factor.
        float[][] quats = new float[][] {
            { 0f, 0f, 0f, 1f },                                 // identity
            { 0f, 0f, 0f, -1f },                                // identity (negated w)
            { 0.70710677f, 0f, 0f, 0.70710677f },              // 90deg about X
            { 0f, 0.70710677f, 0f, 0.70710677f },              // 90deg about Y
            { 0f, 0f, 0.70710677f, 0.70710677f },              // 90deg about Z (roll)
            { 0.5f, 0.5f, 0.5f, 0.5f },                        // 120deg about (1,1,1)
            { 0.27059805f, 0.27059805f, 0.65328145f, 0.65328148f }, // mixed
            { 0.18301269f, -0.18301269f, 0.6830127f, 0.6830127f },  // mixed signs
            { -0.35355338f, 0.14644662f, 0.35355338f, 0.8535534f }, // arbitrary
            { 0.1f, 0.2f, 0.3f, 0.9f },                        // non-unit
            { 2.0f, -1.0f, 0.5f, 1.0f },                       // far-from-unit (exercises k)
            { 1e-4f, 1e-4f, 1e-4f, 1.0f },                     // near identity
        };
        float[][] poss = new float[][] {
            { 0f, 0f, 0f },
            { 12.5f, -3.25f, 7.0f },
            { -128.0f, 64.0f, -255.5f },
            { 0.015625f, -0.5f, 100.0f },
            { 1234.5f, 5678.25f, -9012.75f },
        };
        float[] scales = new float[] { 0.1f, 0.2f, 1.0f, 2.5f, 0.017320509f };
        // (u0,u1,v0,v1) sprite uv windows
        float[][] uvs = new float[][] {
            { 0.0f, 1.0f, 0.0f, 1.0f },
            { 0.125f, 0.1875f, 0.5f, 0.5625f },
            { 0.33333334f, 0.6666667f, 0.1f, 0.9f },
        };
        int[] colors = new int[] { 0xFFFFFFFF, 0x80FF8040, 0x00000000, 0x12345678 };
        int[] lights = new int[] { 0x00F000F0, 0x000F0000, 0x00FFFFFF, 0 };

        for (float[] q : quats) {
            for (float[] p : poss) {
                for (float s : scales) {
                    float[] uv = uvs[id % uvs.length];
                    int color = colors[id % colors.length];
                    int light = lights[id % lights.length];
                    CapturingConsumer cc = new CapturingConsumer();
                    m.invoke(inst, cc,
                        p[0], p[1], p[2],
                        q[0], q[1], q[2], q[3],
                        s,
                        uv[0], uv[1], uv[2], uv[3],
                        color, light);
                    sb.append("CASE\t").append(id).append('\n');
                    for (int c = 0; c < 4; c++) {
                        sb.append("VTX\t").append(id).append('\t').append(c)
                          .append('\t').append(Float.floatToRawIntBits(cc.out[c][0]))
                          .append('\t').append(Float.floatToRawIntBits(cc.out[c][1]))
                          .append('\t').append(Float.floatToRawIntBits(cc.out[c][2]))
                          .append('\t').append(Float.floatToRawIntBits(cc.out[c][3]))
                          .append('\t').append(Float.floatToRawIntBits(cc.out[c][4]))
                          .append('\t').append(cc.outi[c][0])
                          .append('\t').append(cc.outi[c][1])
                          .append('\n');
                    }
                    id++;
                }
            }
        }
        System.out.print(sb);
    }
}
