// Ground-truth generator for the PURE transform math of
// net.minecraft.client.model.geom.ModelPart (Minecraft 26.1.2), mirrored by
// client/model/geom/ModelPart.h.
//
// Drives the REAL classes:
//   * net.minecraft.client.model.geom.ModelPart  (rotateBy / storePose / loadPose /
//     setRotation / offsetPos / offsetRotation / offsetScale)
//   * net.minecraft.client.model.geom.PartPose   (offsetAndRotation / scaled / translated)
//   * org.joml.Matrix3f / Vector3f / Quaternionf  (via ModelPart.rotateBy internals)
//
// ModelPart is GL-free for this subset: rotateBy/storePose/loadPose touch only the
// float pose fields + org.joml math, never a VertexConsumer or GL context. The
// constructor wants cubes+children lists, so we pass empty ones via reflection-free
// public ctor. Pose fields (x..zScale, xRot..zRot) are public, read directly.
//
// JOML runs under DEFAULT options (joml.useMathFma=false, joml.fastmath=false), matching
// the certified Joml.h conventions. We bootstrap SharedConstants/Bootstrap defensively in
// case any static init is required (ModelPart itself does not touch GL on load).
//
// Floats exchanged as raw IEEE-754 bits (%08x of Float.floatToRawIntBits).
//
//   mcpp/tools/run_groundtruth.ps1 -Tool ModelPartParity -Out mcpp/build/model_part.tsv

import java.util.List;
import java.util.Map;

import net.minecraft.client.model.geom.ModelPart;
import net.minecraft.client.model.geom.PartPose;
import org.joml.Quaternionf;
import org.joml.Vector3f;

public class ModelPartParity {
    static final java.io.PrintStream O = System.out;
    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // 9-field pose dump (x,y,z, xRot,yRot,zRot, xScale,yScale,zScale).
    static String pose(ModelPart p) {
        return b(p.x) + "\t" + b(p.y) + "\t" + b(p.z) + "\t"
             + b(p.xRot) + "\t" + b(p.yRot) + "\t" + b(p.zRot) + "\t"
             + b(p.xScale) + "\t" + b(p.yScale) + "\t" + b(p.zScale);
    }
    static String pp(PartPose p) {
        return b(p.x()) + "\t" + b(p.y()) + "\t" + b(p.z()) + "\t"
             + b(p.xRot()) + "\t" + b(p.yRot()) + "\t" + b(p.zRot()) + "\t"
             + b(p.xScale()) + "\t" + b(p.yScale()) + "\t" + b(p.zScale());
    }

    static ModelPart newPart() {
        return new ModelPart(List.of(), Map.of());
    }

    // Representative euler-angle starting poses (radians) — zeros, axis-aligned 90s,
    // PI, mixed signs, off-axis, large. FINITE only.
    static final float[][] ROTS = {
        {0f, 0f, 0f},
        {0.5f, 0.5f, 0.5f},
        {1.5707964f, 0f, 0f},          // +90 about x
        {0f, 1.5707964f, 0f},          // +90 about y
        {0f, 0f, 1.5707964f},          // +90 about z
        {3.1415927f, 0f, 0f},          // +180 about x (gimbal-ish)
        {0.7853982f, -0.7853982f, 0.3926991f},
        {-1.2f, 0.85f, 2.4f},
        {0.123f, -2.9f, 1.7f},
        {-0.5f, -0.5f, -0.5f},
        {2.5f, 1.0f, -3.0f},
        {0.05f, 0.05f, 0.05f},
    };

    // Representative quaternions to rotate by (unit + non-unit + pure axis + signed).
    static final float[][] QS = {
        {0f, 0f, 0f, 1f},                 // identity
        {0.5f, 0.5f, 0.5f, 0.5f},         // unit
        {-0.5f, 0.5f, -0.5f, 0.5f},       // unit, mixed signs
        {1f, 0f, 0f, 0f},                 // 180 about x
        {0f, 1f, 0f, 0f},                 // 180 about y
        {0f, 0f, 1f, 0f},                 // 180 about z
        {0.125f, 0.375f, -0.625f, 0.75f},
        {0.5f, 0f, 0f, 0.5f},             // non-unit
        {-1.5f, 0.5f, 2.25f, -0.75f},     // non-unit, signed
        {0.0625f, 0.0625f, 0.0625f, 0.0625f},
    };

    // Vector3f offsets for offsetPos/offsetRotation/offsetScale.
    static final float[][] OFFS = {
        {0f, 0f, 0f},
        {1.5f, -2.25f, 0.75f},
        {-0.5f, 0.25f, -3.0f},
        {16.0f, 8.0f, -4.0f},
    };

    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {
            // ModelPart's pure-math subset needs no registries; bootstrap is best-effort.
        }

        // rotateBy(Quaternionf): for every (starting euler pose) x (quaternion).
        for (int r = 0; r < ROTS.length; r++) {
            for (int q = 0; q < QS.length; q++) {
                ModelPart p = newPart();
                p.setRotation(ROTS[r][0], ROTS[r][1], ROTS[r][2]);
                p.rotateBy(new Quaternionf(QS[q][0], QS[q][1], QS[q][2], QS[q][3]));
                O.println("ROTBY\t" + r + "\t" + q + "\t" + pose(p));
            }
        }

        // storePose: pose -> PartPose round trip.
        for (int r = 0; r < ROTS.length; r++) {
            ModelPart p = newPart();
            p.setPos(ROTS[r][2] * 3.0f, ROTS[r][0] - 1.0f, ROTS[r][1]);
            p.setRotation(ROTS[r][0], ROTS[r][1], ROTS[r][2]);
            PartPose sp = p.storePose();
            O.println("STORE\t" + r + "\t" + pp(sp));
        }

        // loadPose: PartPose (with non-unit scale) -> ModelPart fields.
        float[] scales = {1.0f, 0.5f, 2.0f, -1.25f};
        for (int r = 0; r < ROTS.length; r++) {
            float s = scales[r % scales.length];
            PartPose lp = PartPose.offsetAndRotation(
                ROTS[r][0], ROTS[r][1] + 4.0f, ROTS[r][2] - 2.0f,
                ROTS[r][0], ROTS[r][1], ROTS[r][2]).scaled(s);
            ModelPart p = newPart();
            p.loadPose(lp);
            O.println("LOAD\t" + r + "\t" + pose(p));
        }

        // offsetPos / offsetRotation / offsetScale.
        for (int r = 0; r < ROTS.length; r++) {
            for (int o = 0; o < OFFS.length; o++) {
                Vector3f off = new Vector3f(OFFS[o][0], OFFS[o][1], OFFS[o][2]);

                ModelPart pp = newPart();
                pp.setPos(ROTS[r][0], ROTS[r][1], ROTS[r][2]);
                pp.offsetPos(off);
                O.println("OFFPOS\t" + r + "\t" + o + "\t" + pose(pp));

                ModelPart pr = newPart();
                pr.setRotation(ROTS[r][0], ROTS[r][1], ROTS[r][2]);
                pr.offsetRotation(off);
                O.println("OFFROT\t" + r + "\t" + o + "\t" + pose(pr));

                ModelPart ps = newPart();
                ps.loadPose(PartPose.offsetAndRotation(0, 0, 0, 0, 0, 0));
                ps.offsetScale(off);
                O.println("OFFSCL\t" + r + "\t" + o + "\t" + pose(ps));
            }
        }
    }
}
