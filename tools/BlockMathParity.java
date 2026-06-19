// Ground-truth generator for net.minecraft.core.BlockMath — the UV-lock face
// transformation used by the block-model baking pipeline. Calls the REAL
// net.minecraft methods (BlockMath.getFaceTransformation, plus reflection to dump
// the two private VANILLA_UV_TRANSFORM_* maps), emitting raw IEEE-754 matrix bits
// for the C++ core/BlockMath.h port to compare bit-for-bit.
//
// Inputs are built from EXACT-FLOAT quaternions (new Matrix4f().rotation(quat)), so
// no sin/cos enters the input side; the precomputed maps DO use rotateY/rotateX with
// PI angles, which go through org.joml.Math.sin == (float)Math.sin((double)x) and so
// reproduce identically in the C++ joml::jsin / cosFromSin port.
//
//   tools/run_groundtruth.ps1 -Tool BlockMathParity -Out mcpp/build/block_math.tsv

import java.lang.reflect.Field;
import java.util.Map;

import com.mojang.math.Transformation;
import net.minecraft.core.BlockMath;
import net.minecraft.core.Direction;

import org.joml.Matrix4f;
import org.joml.Matrix4fc;
import org.joml.Quaternionf;

public class BlockMathParity {
    static final java.io.PrintStream O = System.out;

    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static String m4(Matrix4fc m) {
        return b(m.m00()) + "\t" + b(m.m01()) + "\t" + b(m.m02()) + "\t" + b(m.m03()) + "\t"
             + b(m.m10()) + "\t" + b(m.m11()) + "\t" + b(m.m12()) + "\t" + b(m.m13()) + "\t"
             + b(m.m20()) + "\t" + b(m.m21()) + "\t" + b(m.m22()) + "\t" + b(m.m23()) + "\t"
             + b(m.m30()) + "\t" + b(m.m31()) + "\t" + b(m.m32()) + "\t" + b(m.m33());
    }

    // Exact-float quaternion components (no normalization needed for bit-exactness);
    // identical to the C++ test's QS table.
    static final float[][] QS = {
        {0,0,0,1}, {0.5f,0.5f,0.5f,0.5f}, {0.5f,0,0,0.5f}, {0,0.25f,0,0.75f},
        {-0.5f,0.5f,-0.5f,0.5f}, {0.25f,-0.25f,0.5f,1f}, {1,0,0,0}, {0,1,0,0},
        {0.125f,0.375f,-0.625f,0.75f}, {-0.5f,-0.5f,0.5f,0.5f}, {0.7071068f,0,0,0.7071068f},
        {0,0,0.38268343f,0.9238795f}
    };

    // Pure-translation inputs (not identity -> getFaceTransformation runs the full path).
    static final float[][] TS = { {1,0,0}, {0.5f,-2,3}, {-1.25f,0.75f,10} };

    // Direction enum constants in declaration order (matches C++ Dir).
    static final Direction[] DIRS = {
        Direction.DOWN, Direction.UP, Direction.NORTH, Direction.SOUTH, Direction.WEST, Direction.EAST
    };
    static String dn(Direction d) { return d.name(); }

    @SuppressWarnings("unchecked")
    static Map<Direction, Transformation> readMap(String fieldName) throws Exception {
        Field f = BlockMath.class.getDeclaredField(fieldName);
        f.setAccessible(true);
        return (Map<Direction, Transformation>) f.get(null);
    }

    public static void main(String[] args) throws Exception {
        // BlockMath touches no registry/bootstrap state, but harmless to be safe.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {}

        // ── 1) Pin the two precomputed maps directly (per Direction). ──────────
        Map<Direction, Transformation> l2g = readMap("VANILLA_UV_TRANSFORM_LOCAL_TO_GLOBAL");
        Map<Direction, Transformation> g2l = readMap("VANILLA_UV_TRANSFORM_GLOBAL_TO_LOCAL");
        for (Direction d : DIRS) {
            Transformation tl = l2g.get(d);
            O.println("L2G\t" + dn(d) + "\t" + m4(tl.getMatrix()));
            Transformation tg = g2l.get(d);
            O.println("G2L\t" + dn(d) + "\t" + m4(tg.getMatrix()));
        }

        // ── 2) getFaceTransformation over rotation inputs × all 6 sides. ──────
        for (int i = 0; i < QS.length; i++) {
            Transformation in = new Transformation(
                new Matrix4f().rotation(new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3])));
            for (Direction side : DIRS) {
                Transformation out = BlockMath.getFaceTransformation(in, side);
                O.println("FACE_ROT\t" + i + "\t" + dn(side) + "\t" + m4(out.getMatrix()));
            }
        }

        // ── 3) getFaceTransformation over pure-translation inputs × all 6 sides.
        for (int i = 0; i < TS.length; i++) {
            Transformation in = new Transformation(
                new Matrix4f().translation(TS[i][0], TS[i][1], TS[i][2]));
            for (Direction side : DIRS) {
                Transformation out = BlockMath.getFaceTransformation(in, side);
                O.println("FACE_TRANS\t" + i + "\t" + dn(side) + "\t" + m4(out.getMatrix()));
            }
        }

        // ── 4) Identity input short-circuits (returns input unchanged). ────────
        for (Direction side : DIRS) {
            Transformation in = new Transformation(new Matrix4f()); // identity
            Transformation out = BlockMath.getFaceTransformation(in, side);
            O.println("FACE_ID\t" + dn(side) + "\t" + m4(out.getMatrix()));
        }

        // ── 5) Direction.getApproximateNearest over a finite battery. ─────────
        float[][] NRM = {
            {0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0},
            {0.7f,0.1f,0.2f}, {-0.3f,-0.8f,0.5f}, {0.5f,0.5f,0.5f}, {-0.5f,-0.5f,-0.5f},
            {0.001f,0.002f,-0.0015f}, {0,0,0}, {-0.4f,0.4f,0.4f}, {0.9f,-0.1f,0.0f},
            {0.25f,0.25f,-0.25f}, {-0.6f,0.6f,-0.6f}, {1e-30f,2e-30f,3e-30f}
        };
        for (float[] n : NRM) {
            Direction d = Direction.getApproximateNearest(n[0], n[1], n[2]);
            O.println("NEAREST\t" + b(n[0]) + "\t" + b(n[1]) + "\t" + b(n[2]) + "\t" + dn(d));
        }
    }
}
