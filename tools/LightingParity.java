// Ground-truth generator for the PURE rendering math in
// com.mojang.blaze3d.platform.Lighting (Minecraft 26.1.2): the directional
// diffuse-light setup. The C++ port lives in mcpp/src/render/Lighting.h.
//
// Lighting's CONSTRUCTOR allocates a GpuBuffer (real GL/GPU) so it cannot be
// instantiated here. But Lighting's *class initialization* is GL-free — the only
// static work is six normalized Vector3f constants plus a Std140SizeCalculator
// (pure int math). So we:
//   1. read the six private static DIFFUSE_LIGHT_* / INVENTORY_DIFFUSE_LIGHT_* /
//      NETHER_DIFFUSE_LIGHT_* Vector3f constants from the REAL class via
//      reflection (this is the real class' own computed output), and
//   2. reproduce the constructor's pose-matrix transforms with the REAL
//      org.joml library, using the byte-for-byte source expressions, to derive
//      the per-Entry light directions written to the UBO.
//   3. read UBO_SIZE from the real class.
//
// Floats are emitted as raw IEEE-754 bits (Float.floatToRawIntBits), ints as
// decimal. The C++ test recomputes from render/Lighting.h and compares bit-exact.
//
//   tools/run_groundtruth.ps1 -Tool LightingParity -Out mcpp/build/lighting.tsv

import java.lang.reflect.Field;
import org.joml.Matrix4f;
import org.joml.Vector3f;

public class LightingParity {
    static final java.io.PrintStream O = System.out;
    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String v3(Vector3f v) { return b(v.x) + "\t" + b(v.y) + "\t" + b(v.z); }
    static void row(String tag, Vector3f v) { O.println(tag + "\t" + v3(v)); }

    static Vector3f getVec(Class<?> c, String name) throws Exception {
        Field f = c.getDeclaredField(name);
        f.setAccessible(true);
        return (Vector3f) f.get(null);
    }
    static int getInt(Class<?> c, String name) throws Exception {
        Field f = c.getDeclaredField(name);
        f.setAccessible(true);
        return f.getInt(null);
    }

    public static void main(String[] args) throws Exception {
        // Boot the registries/SharedConstants like the other parity tools (harmless
        // here, but keeps the harness uniform and proves no GL is pulled in).
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> L = Class.forName("com.mojang.blaze3d.platform.Lighting");

        // (1) The six raw normalized constants, straight from the REAL class.
        Vector3f d0 = getVec(L, "DIFFUSE_LIGHT_0");
        Vector3f d1 = getVec(L, "DIFFUSE_LIGHT_1");
        Vector3f n0 = getVec(L, "NETHER_DIFFUSE_LIGHT_0");
        Vector3f n1 = getVec(L, "NETHER_DIFFUSE_LIGHT_1");
        Vector3f i0 = getVec(L, "INVENTORY_DIFFUSE_LIGHT_0");
        Vector3f i1 = getVec(L, "INVENTORY_DIFFUSE_LIGHT_1");
        row("DIFFUSE0", d0);
        row("DIFFUSE1", d1);
        row("NETHER0", n0);
        row("NETHER1", n1);
        row("INV0", i0);
        row("INV1", i1);

        // (3) UBO_SIZE from the real class (Std140 putVec3 + putVec3).
        O.println("UBO_SIZE\t" + getInt(L, "UBO_SIZE"));

        // (2) Reproduce the constructor's pose-matrix transforms with real JOML,
        // exactly as Lighting.java writes them.
        Matrix4f flatPose = new Matrix4f().rotationY((float) (-Math.PI / 8)).rotateX((float) (Math.PI * 3.0 / 4.0));
        row("ITEMS_FLAT0", flatPose.transformDirection(d0, new Vector3f()));
        row("ITEMS_FLAT1", flatPose.transformDirection(d1, new Vector3f()));

        Matrix4f item3DPose = new Matrix4f()
            .scaling(1.0F, -1.0F, 1.0F)
            .rotateYXZ(1.0821041F, 3.2375858F, 0.0F)
            .rotateYXZ((float) (-Math.PI / 8), (float) (Math.PI * 3.0 / 4.0), 0.0F);
        row("ITEMS_3D0", item3DPose.transformDirection(d0, new Vector3f()));
        row("ITEMS_3D1", item3DPose.transformDirection(d1, new Vector3f()));

        // ENTITY_IN_UI: written with inventory constants directly.
        row("ENTITY_IN_UI0", i0);
        row("ENTITY_IN_UI1", i1);

        // PLAYER_SKIN: inventory constants through an identity Matrix4f.
        Matrix4f playerSkinPose = new Matrix4f();
        row("PLAYER_SKIN0", playerSkinPose.transformDirection(i0, new Vector3f()));
        row("PLAYER_SKIN1", playerSkinPose.transformDirection(i1, new Vector3f()));

        // updateLevel: raw level constants.
        row("LEVEL_DEFAULT0", d0);
        row("LEVEL_DEFAULT1", d1);
        row("LEVEL_NETHER0", n0);
        row("LEVEL_NETHER1", n1);
    }
}
