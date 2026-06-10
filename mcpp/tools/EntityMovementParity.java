// Ground-truth generator for net.minecraft.world.entity.Entity.getInputVector (the
// WASD-input + yaw -> world-space movement delta; the core of player/entity
// moveRelative). Private static, reached via reflection. Pure; no Bootstrap.
//
//   tools/run_groundtruth.ps1 -Tool EntityMovementParity -Out mcpp/build/entity_movement.tsv

import java.lang.reflect.Method;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.Vec3;

public class EntityMovementParity {
    static final java.io.PrintStream O = System.out;
    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }
    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static final double[][] INPUTS = {
        {0,0,0}, {0,0,1}, {1,0,0}, {1,0,1}, {-1,0,1}, {0.5,0,0.5}, {0.7,0,0.7},
        {0,0,0.0001}, {2,0,0}, {-1,0,-1}, {0.3,1.0,-0.8}, {1,1,1}
    };
    static final float[] SPEEDS = { 0.0f, 0.1f, 0.2155f, 0.98f, 1.0f, 5.0f };
    static final float[] YAWS = { 0f, 45f, 90f, 135f, 180f, -90f, -45f, 270f, 22.5f, 360f, 33.3f };

    public static void main(String[] args) throws Exception {
        // Loading Entity pulls in BuiltInRegistries -> needs Bootstrap. O was captured
        // at class load (before this), so the TSV on stdout stays clean.
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Method m = Entity.class.getDeclaredMethod("getInputVector", Vec3.class, float.class, float.class);
        m.setAccessible(true);
        for (double[] in : INPUTS) for (float speed : SPEEDS) for (float yaw : YAWS) {
            Vec3 r = (Vec3) m.invoke(null, new Vec3(in[0], in[1], in[2]), speed, yaw);
            O.println("INPUTVEC\t" + d(in[0])+"\t"+d(in[1])+"\t"+d(in[2]) + "\t" + f(speed) + "\t" + f(yaw)
                + "\t" + d(r.x) + "\t" + d(r.y) + "\t" + d(r.z));
        }
    }
}
