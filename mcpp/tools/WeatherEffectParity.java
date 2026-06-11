// Ground-truth generator for the PURE geometry math of
// net.minecraft.client.renderer.WeatherEffectRenderer (Minecraft 26.1.2):
//
//   * the constructor's precomputed columnSizeX / columnSizeZ tables
//     (read reflectively from a real `new WeatherEffectRenderer()`), and
//   * the private renderInstances() loop body, driven through the REAL method by
//     passing a single-column list and a java.lang.reflect.Proxy VertexConsumer
//     that captures the emitted vertices. We NEVER replicate the body Java-side.
//
// Floats are emitted as raw IEEE-754 bits (Float.floatToRawIntBits); ints decimal.
// Zero javac stderr is required, so VertexConsumer's many methods are dispatched
// reflectively in the proxy handler (no @Override of an evolving interface).
//
//   tools/run_groundtruth.ps1 -Tool WeatherEffectParity -Out mcpp/build/weather_effect.tsv

import com.mojang.blaze3d.vertex.VertexConsumer;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.client.renderer.WeatherEffectRenderer;

public class WeatherEffectParity {
    static final java.io.PrintStream O = System.out;

    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    // Captures the (x,y,z,u,v,color,light) emitted per vertex by renderInstances.
    static final class Capture implements InvocationHandler {
        final List<float[]> verts = new ArrayList<>(); // [x,y,z,u,v]
        int color, light;

        @Override
        public Object invoke(final Object proxy, final Method method, final Object[] args) {
            switch (method.getName()) {
                case "addVertex":
                    // addVertex(float x, float y, float z)
                    if (args != null && args.length == 3 && args[0] instanceof Float) {
                        verts.add(new float[]{(Float) args[0], (Float) args[1], (Float) args[2], 0.0F, 0.0F});
                    }
                    return proxy;
                case "setUv":
                    // setUv(float u, float v) — annotate the most recent vertex
                    if (args != null && args.length == 2 && args[0] instanceof Float) {
                        float[] last = verts.get(verts.size() - 1);
                        last[3] = (Float) args[0];
                        last[4] = (Float) args[1];
                    }
                    return proxy;
                case "setColor":
                    // setColor(int color)
                    if (args != null && args.length == 1) {
                        color = (Integer) args[0];
                    }
                    return proxy;
                case "setLight":
                    // setLight(int packedLightCoords)
                    if (args != null && args.length == 1) {
                        light = (Integer) args[0];
                    }
                    return proxy;
                case "hashCode":
                    return System.identityHashCode(proxy);
                case "equals":
                    return proxy == args[0];
                case "toString":
                    return "CaptureVertexConsumer";
                default:
                    return proxy; // any other chained call returns the consumer
            }
        }
    }

    public static void main(final String[] args) throws Exception {
        WeatherEffectRenderer renderer = new WeatherEffectRenderer();

        // ── 1) Constructor tables (read reflectively). ──
        Field fX = WeatherEffectRenderer.class.getDeclaredField("columnSizeX");
        Field fZ = WeatherEffectRenderer.class.getDeclaredField("columnSizeZ");
        fX.setAccessible(true);
        fZ.setAccessible(true);
        float[] tableX = (float[]) fX.get(renderer);
        float[] tableZ = (float[]) fZ.get(renderer);
        for (int i = 0; i < tableX.length; i++) {
            O.println("TBL\t" + i + "\t" + b(tableX[i]) + "\t" + b(tableZ[i]));
        }

        // ── 2) renderInstances loop body, driven through the REAL private method. ──
        Method renderInstances = WeatherEffectRenderer.class.getDeclaredMethod(
            "renderInstances", VertexConsumer.class, List.class, net.minecraft.world.phys.Vec3.class,
            float.class, int.class, float.class);
        renderInstances.setAccessible(true);

        // ColumnInstance record: (int x, int z, int bottomY, int topY, float uOffset, float vOffset, int lightCoords)
        java.lang.reflect.Constructor<?> colCtor = WeatherEffectRenderer.ColumnInstance.class
            .getDeclaredConstructor(int.class, int.class, int.class, int.class, float.class, float.class, int.class);
        colCtor.setAccessible(true);

        // Finite, physically plausible inputs. Columns are expressed as a (dx,dz)
        // offset from the camera's block position so the in-game invariant holds:
        // renderInstances indexes columnSizeX/Z at
        //   (columnZ - floor(camZ) + 16) * 32 + (columnX - floor(camX) + 16)
        // which is in [0,1024) iff dx,dz are in [-16,15] (the 32x32 window). In-game
        // extractRenderState only ever adds columns within `radius` (<=~10) of the
        // camera, so this is exactly the physical regime. We sweep dx,dz in [-15,15].
        int[][] OFFS = {            // dx, dz, bottomY, topY, light
            {0, 0, 60, 80, 0xF000F0},
            {3, -5, 62, 70, 0x0A0B0C},
            {-7, 11, -10, 5, 0x00FF00},
            {15, -15, 0, 320, 0xABCDEF},
            {-9, -9, 100, 130, 0x123456},
            {8, 8, -64, -40, 0x00000F},
            {-16, 15, 71, 91, 0xFFFFFF}, // dx = -16 (window edge)
            {-1, 1, 64, 64, 0x808080},   // bottomY == topY (degenerate height)
            {14, -3, 50, 200, 0x0F0F0F},
            {-15, 0, -32, 96, 0x7F00FF},
        };
        float[][] UVOFF = { {0.0F, 0.0F}, {-3.5F, 0.25F}, {12.75F, -7.0F} };
        double[][] CAM = {
            {0.5, 64.0, 0.5},
            {3.2, 70.5, -4.8},
            {-7.1, -3.0, 11.9},
            {123.25, 200.0, -456.5},
            {15.999, 64.0, -15.001},
        };
        float[] MAXA = { 1.0F, 0.8F };
        int[] RADII = { 5, 10, 8 };
        float[] INTENS = { 1.0F, 0.5F, 0.137F };

        for (double[] cam : CAM) {
            int camBlockX = net.minecraft.util.Mth.floor(cam[0]);
            int camBlockZ = net.minecraft.util.Mth.floor(cam[2]);
            net.minecraft.world.phys.Vec3 camVec = new net.minecraft.world.phys.Vec3(cam[0], cam[1], cam[2]);
            for (int[] off : OFFS) {
                int columnX = camBlockX + off[0];
                int columnZ = camBlockZ + off[1];
                int bottomY = off[2], topY = off[3], light = off[4];
                for (float[] uv : UVOFF) {
                    Object instance = colCtor.newInstance(columnX, columnZ, bottomY, topY, uv[0], uv[1], light);
                    List<Object> single = new ArrayList<>();
                    single.add(instance);
                    for (float maxAlpha : MAXA) {
                        for (int radius : RADII) {
                            for (float intensity : INTENS) {
                                Capture cap = new Capture();
                                VertexConsumer vc = (VertexConsumer) Proxy.newProxyInstance(
                                    VertexConsumer.class.getClassLoader(),
                                    new Class<?>[]{VertexConsumer.class}, cap);
                                renderInstances.invoke(renderer, vc, single, camVec, maxAlpha, radius, intensity);
                                // Exactly 4 vertices expected per column.
                                StringBuilder sb = new StringBuilder("RC");
                                sb.append('\t').append(columnX).append('\t').append(columnZ)
                                  .append('\t').append(bottomY).append('\t').append(topY)
                                  .append('\t').append(b(uv[0])).append('\t').append(b(uv[1]))
                                  .append('\t').append(light)
                                  .append('\t').append(d(cam[0])).append('\t').append(d(cam[1])).append('\t').append(d(cam[2]))
                                  .append('\t').append(b(maxAlpha)).append('\t').append(radius).append('\t').append(b(intensity));
                                sb.append('\t').append(cap.color).append('\t').append(cap.light);
                                for (int vi = 0; vi < cap.verts.size(); vi++) {
                                    float[] vtx = cap.verts.get(vi);
                                    sb.append('\t').append(b(vtx[0])).append('\t').append(b(vtx[1])).append('\t').append(b(vtx[2]))
                                      .append('\t').append(b(vtx[3])).append('\t').append(b(vtx[4]));
                                }
                                O.println(sb.toString());
                            }
                        }
                    }
                }
            }
        }
    }
}
