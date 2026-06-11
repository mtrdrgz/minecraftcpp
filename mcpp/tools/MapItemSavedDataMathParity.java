// Ground-truth generator for the PURE STATIC coordinate/decoration math of
// net.minecraft.world.level.saveddata.maps.MapItemSavedData (26.1.2).
//
//   tools/run_groundtruth.ps1 -Tool MapItemSavedDataMathParity -Out mcpp/build/map_item_saved_data_math.tsv
//
// All five ported functions are driven against the REAL class. Where the source
// helper is a *static* method (isInsideMap, clampMapCoordinate) it is invoked on
// the REAL class via reflection (setAccessible + invoke(null, ...)). Where it is a
// *public static factory* that wraps the math in a world-coupled object
// (createFresh), GT calls it through reflection but with a dummy ResourceKey and
// reads the resulting public centerX/centerZ fields — no Level is touched, the
// object is never ticked. Where the helper is a private *instance* method whose
// only world dependency is a dimension comparison (calculateRotation), the
// world-free Overworld branch is REPLICATED verbatim below (this.dimension !=
// Level.NETHER), mirroring PlayerXpParity's handling of getXpNeededForNextLevel.
// The same applies to addDecoration's per-axis delta-from-center expression.
//
// TSV rows (tab-separated), dispatched by leading TAG in the C++ test. Floats are
// emitted as raw IEEE-754 bits (Float.floatToRawIntBits) so the compare is exact:
//   CENTER  <originXbits:int> <originYbits:int> <scale:int> <centerX:int> <centerZ:int>
//   INSIDE  <xdBits:int> <ydBits:int> <result:0|1>
//   CLAMP   <deltaBits:int> <result:int(byte)>
//   ROT     <yRotBits:long> <result:int(byte)>
//   DELTA   <worldBits:long> <centerBits:long> <scale:int> <resultBits:int>

import java.lang.reflect.Method;

public class MapItemSavedDataMathParity {
    static final java.io.PrintStream O = System.out;

    // Verbatim copy of MapItemSavedData.calculateRotation's NON-Nether branch
    // (lines 335-336). The only thing dropped is the `this.dimension == Level.NETHER
    // && level != null` guard, which selects the time-coupled branch we do not port.
    static byte calculateRotationOverworld(double yRot) {
        double adjustedYRot = yRot < 0.0 ? yRot - 8.0 : yRot + 8.0;
        return (byte) (adjustedYRot * 16.0 / 360.0);
    }

    // Verbatim copy of the per-axis delta computed at the head of
    // MapItemSavedData.addDecoration (lines 275-277). scale is the instance's byte
    // scale; world/center are the addDecoration xPos/zPos (double) and centerX/Z.
    static float decorationDeltaFromCenter(double world, double center, int scale) {
        int scaling = 1 << scale;
        return (float) (world - center) / scaling;
    }

    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // Static reflective calls below may still work.
        }

        Class<?> C = net.minecraft.world.level.saveddata.maps.MapItemSavedData.class;

        // ---- CENTER: createFresh(originX, originY, scale, ...) center math ----
        // Drive the REAL public static factory reflectively and read centerX/centerZ.
        // Signature: createFresh(double, double, byte, boolean, boolean, ResourceKey<Level>)
        Method createFresh = C.getDeclaredMethod(
            "createFresh", double.class, double.class, byte.class,
            boolean.class, boolean.class, net.minecraft.resources.ResourceKey.class);
        createFresh.setAccessible(true);
        // Level.OVERWORLD is a public static ResourceKey<Level>; reuse it (no world).
        net.minecraft.resources.ResourceKey<?> dim = net.minecraft.world.level.Level.OVERWORLD;

        double[] ORIGINS = {
            -100000000.0, -16777216.0, -100000.5, -64.0, -64.0000001, -63.5,
            -1.0, -0.5, 0.0, 0.5, 1.0, 63.0, 63.5, 64.0, 127.0, 128.0, 129.0,
            191.0, 192.0, 1000.0, 1000.5, 100000.25, 16777216.0, 100000000.0,
            2000000000.0, -2000000000.0
        };
        int[] SCALES = {0, 1, 2, 3, 4};
        for (double ox : ORIGINS) {
            for (double oy : ORIGINS) {
                for (int sc : SCALES) {
                    Object inst = createFresh.invoke(
                        null, ox, oy, (byte) sc, true, false, dim);
                    int cx = C.getField("centerX").getInt(inst);
                    int cz = C.getField("centerZ").getInt(inst);
                    O.println("CENTER\t" + Double.doubleToRawLongBits(ox)
                        + "\t" + Double.doubleToRawLongBits(oy)
                        + "\t" + sc + "\t" + cx + "\t" + cz);
                }
            }
        }

        // ---- INSIDE: isInsideMap(float, float) (private static) ----
        Method isInsideMap = C.getDeclaredMethod("isInsideMap", float.class, float.class);
        isInsideMap.setAccessible(true);
        float[] DELTAS = {
            Float.NEGATIVE_INFINITY, -1000f, -320f, -64f, -63.5f, -63.0001f, -63f,
            -62.9999f, -1f, -0.5f, 0f, 0.5f, 1f, 62.9999f, 63f, 63.0001f, 63.5f,
            64f, 320f, 1000f, Float.POSITIVE_INFINITY, Float.NaN
        };
        for (float xd : DELTAS) {
            for (float yd : DELTAS) {
                boolean r = (Boolean) isInsideMap.invoke(null, xd, yd);
                O.println("INSIDE\t" + Float.floatToRawIntBits(xd)
                    + "\t" + Float.floatToRawIntBits(yd) + "\t" + (r ? 1 : 0));
            }
        }

        // ---- CLAMP: clampMapCoordinate(float) (private static) ----
        Method clampMapCoordinate = C.getDeclaredMethod("clampMapCoordinate", float.class);
        clampMapCoordinate.setAccessible(true);
        float[] CLAMP_IN = {
            Float.NEGATIVE_INFINITY, -1000f, -100f, -64f, -63.5f, -63.0001f, -63f,
            -62.9999f, -62.5f, -10.25f, -1.7f, -1f, -0.75f, -0.5f, -0.25f, -0.0001f,
            0f, 0.0001f, 0.24f, 0.25f, 0.49f, 0.5f, 0.7f, 1f, 1.7f, 10.25f, 31.4f,
            62.4999f, 62.5f, 62.7499f, 62.75f, 62.9999f, 63f, 63.0001f, 63.5f, 64f,
            100f, 1000f, Float.POSITIVE_INFINITY, Float.NaN
        };
        for (float d : CLAMP_IN) {
            byte r = (Byte) clampMapCoordinate.invoke(null, d);
            O.println("CLAMP\t" + Float.floatToRawIntBits(d) + "\t" + (int) r);
        }

        // ---- ROT: calculateRotation Overworld branch (replicated) ----
        double[] YROTS = {
            -360.0, -359.9, -180.0, -90.0, -45.0, -22.5, -11.25, -8.0001, -8.0,
            -7.9999, -1.0, -0.0001, 0.0, 0.0001, 1.0, 7.9999, 8.0, 8.0001, 11.25,
            22.5, 45.0, 90.0, 180.0, 270.0, 350.0, 359.0, 359.9, 360.0, 720.0,
            -720.0, 1000.0, -1000.0
        };
        for (double y : YROTS) {
            byte r = calculateRotationOverworld(y);
            O.println("ROT\t" + Double.doubleToRawLongBits(y) + "\t" + (int) r);
        }

        // ---- DELTA: addDecoration per-axis delta-from-center (replicated) ----
        double[] WORLD = {
            -100000000.0, -1000.5, -129.0, -64.0, -1.0, 0.0, 1.0, 63.5, 64.0,
            128.0, 1000.25, 100000000.0
        };
        double[] CENTER = {-1000.0, -64.0, 0.0, 64.0, 1000.0};
        for (double w : WORLD) {
            for (double c : CENTER) {
                for (int sc : SCALES) {
                    float r = decorationDeltaFromCenter(w, c, sc);
                    O.println("DELTA\t" + Double.doubleToRawLongBits(w)
                        + "\t" + Double.doubleToRawLongBits(c)
                        + "\t" + sc + "\t" + Float.floatToRawIntBits(r));
                }
            }
        }
    }
}
