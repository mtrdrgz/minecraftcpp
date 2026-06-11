// Ground-truth generator for
// net.minecraft.world.entity.projectile.EyeOfEnder.updateDeltaMovement
// (EyeOfEnder.java:133-145) — the per-tick steering velocity update for an Eye of
// Ender homing toward its stronghold target. Private static; reached via reflection.
// Pure (only Vec3 math + Mth.lerp), but loading EyeOfEnder triggers EntityType
// static init -> BuiltInRegistries, so Bootstrap is required. We capture O at class
// load (before Bootstrap), keeping the TSV on stdout clean.
//
//   tools/run_groundtruth.ps1 -Tool EyeOfEnderMovementParity -Out mcpp/build/eye_of_ender_movement.tsv

import java.lang.reflect.Method;
import net.minecraft.world.entity.projectile.EyeOfEnder;
import net.minecraft.world.phys.Vec3;

public class EyeOfEnderMovementParity {
    static final java.io.PrintStream O = System.out;
    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    // Representative oldMovement / position / target triples. Cover:
    //   * straight-line homing (target far away horizontally)
    //   * the horizontalLength < 1.0 slow-down branch (target within a block)
    //   * target directly above / below (vertical step sign both ways)
    //   * negative coordinates and asymmetric x/z
    //   * a degenerate target == position (horizontalLength == 0 -> wantedSpeed/0)
    static final double[][] OLD = {
        {0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.5, 0.0, 0.5},
        {-0.3, 0.2, 0.8},
        {1.2, -0.4, -0.7},
        {0.0, 0.5, 0.0},
        {0.05, -0.05, 0.05},
        {2.0, 0.0, -1.0},
    };
    static final double[][] POS = {
        {0.0, 64.0, 0.0},
        {10.5, 70.0, -20.5},
        {-100.0, 60.0, 100.0},
        {3.0, 65.0, 3.0},
        {0.25, 64.0, 0.25},
    };
    static final double[][] TGT = {
        {50.0, 64.0, 50.0},     // far horizontal, same height
        {0.5, 64.0, 0.5},       // within a block of an origin position
        {10.5, 90.0, -20.5},    // directly above one POS
        {10.5, 40.0, -20.5},    // directly below one POS
        {-100.0, 60.0, 100.0},  // equal to one POS (horizontalLength can be 0)
        {3.7, 64.0, 2.1},       // close, slow-down branch for POS {3,65,3}
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Method m = EyeOfEnder.class.getDeclaredMethod("updateDeltaMovement", Vec3.class, Vec3.class, Vec3.class);
        m.setAccessible(true);

        for (double[] o : OLD) for (double[] p : POS) for (double[] t : TGT) {
            Vec3 oldMv = new Vec3(o[0], o[1], o[2]);
            Vec3 pos = new Vec3(p[0], p[1], p[2]);
            Vec3 tgt = new Vec3(t[0], t[1], t[2]);
            Vec3 r = (Vec3) m.invoke(null, oldMv, pos, tgt);
            O.println("UPD"
                + "\t" + d(o[0]) + "\t" + d(o[1]) + "\t" + d(o[2])
                + "\t" + d(p[0]) + "\t" + d(p[1]) + "\t" + d(p[2])
                + "\t" + d(t[0]) + "\t" + d(t[1]) + "\t" + d(t[2])
                + "\t" + d(r.x) + "\t" + d(r.y) + "\t" + d(r.z));
        }
    }
}
