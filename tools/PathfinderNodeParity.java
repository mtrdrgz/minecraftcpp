import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

// Ground-truth emitter for net.minecraft.world.level.pathfinder.Node pure helpers.
// Emits tab-separated rows: <TAG>\t<inputs...>\t<outputs...>.
//   floats  -> %08x (Float.floatToRawIntBits)
//   ints    -> decimal (createHash returns an int; emitted as decimal)
// Calls the REAL net.minecraft Node + core BlockPos via reflection.
public class PathfinderNodeParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> nodeC = Class.forName("net.minecraft.world.level.pathfinder.Node");
        Class<?> bposC = Class.forName("net.minecraft.core.BlockPos");

        Constructor<?> nodeCtor = nodeC.getConstructor(int.class, int.class, int.class);
        nodeCtor.setAccessible(true);
        Constructor<?> bposCtor = bposC.getConstructor(int.class, int.class, int.class);
        bposCtor.setAccessible(true);

        Method createHash = nodeC.getMethod("createHash", int.class, int.class, int.class);
        createHash.setAccessible(true);
        Method distanceTo_N = nodeC.getMethod("distanceTo", nodeC);
        Method distanceTo_B = nodeC.getMethod("distanceTo", bposC);
        Method distanceToXZ = nodeC.getMethod("distanceToXZ", nodeC);
        Method distanceToSqr_N = nodeC.getMethod("distanceToSqr", nodeC);
        Method distanceToSqr_B = nodeC.getMethod("distanceToSqr", bposC);
        Method distanceManhattan_N = nodeC.getMethod("distanceManhattan", nodeC);
        Method distanceManhattan_B = nodeC.getMethod("distanceManhattan", bposC);

        // Finite/physical coordinate set: world block coords incl. negatives, zero,
        // chunk/region boundaries, vertical world bounds, and the bit-mask edges of
        // createHash (15-bit field at 32767/32768).
        int[] coords = {
            0, 1, -1, 7, 8, 15, 16, 17, -16, -17,
            127, 128, 255, 256, 1000, -1000,
            32766, 32767, 32768, -32767, -32768,
            64, -64, 320, -64 + 0, 384, -2032,           // vertical-ish + region
            30000000, -30000000                            // world border magnitude
        };

        // ---- createHash: dense sweep over the structured coordinate set ----
        for (int x : coords) {
            for (int y : coords) {
                for (int z : coords) {
                    int h = (int) createHash.invoke(null, x, y, z);
                    O.println("HASH\t" + x + "\t" + y + "\t" + z + "\t" + h);
                }
            }
        }

        // ---- distance helpers: pairs of Nodes / Node+BlockPos ----
        // Pick a manageable but thorough cross product of base/target points.
        int[][] pts = {
            {0,0,0}, {1,2,3}, {-1,-2,-3}, {10,5,-7}, {-10,-5,7},
            {16,64,16}, {-16,320,-16}, {255,128,-255}, {1000,-50,1000},
            {30000000,200,-30000000}, {-32768,0,32767}, {5,5,5}, {100,-100,100},
            {7,8,9}, {-7,-8,-9}, {123,-45,678}, {-999,12,-345}
        };

        for (int[] a : pts) {
            Object na = nodeCtor.newInstance(a[0], a[1], a[2]);
            for (int[] b : pts) {
                Object nb = nodeCtor.newInstance(b[0], b[1], b[2]);
                Object bp = bposCtor.newInstance(b[0], b[1], b[2]);

                float dTo   = (float) distanceTo_N.invoke(na, nb);
                float dToB  = (float) distanceTo_B.invoke(na, bp);
                float dXZ   = (float) distanceToXZ.invoke(na, nb);
                float dSqr  = (float) distanceToSqr_N.invoke(na, nb);
                float dSqrB = (float) distanceToSqr_B.invoke(na, bp);
                float dMan  = (float) distanceManhattan_N.invoke(na, nb);
                float dManB = (float) distanceManhattan_B.invoke(na, bp);

                String key = a[0]+"\t"+a[1]+"\t"+a[2]+"\t"+b[0]+"\t"+b[1]+"\t"+b[2];
                O.println("DIST\t" + key + "\t" + f(dTo));
                O.println("DISTB\t" + key + "\t" + f(dToB));
                O.println("DISTXZ\t" + key + "\t" + f(dXZ));
                O.println("DISTSQR\t" + key + "\t" + f(dSqr));
                O.println("DISTSQRB\t" + key + "\t" + f(dSqrB));
                O.println("MAN\t" + key + "\t" + f(dMan));
                O.println("MANB\t" + key + "\t" + f(dManB));
            }
        }
    }
}
