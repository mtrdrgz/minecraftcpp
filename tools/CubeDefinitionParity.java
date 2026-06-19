// Ground-truth generator for the PURE, GL-free cube vertex-builder math of
// net.minecraft.client.model.geom.ModelPart.Cube / .Polygon / .Vertex
// (Minecraft 26.1.2), mirrored by client/model/geom/CubeDefinition.h.
//
// Drives the REAL classes:
//   * net.minecraft.client.model.geom.ModelPart.Cube     (public ctor — builds
//     the 8 grown/mirrored corners + the up-to-6 face polygons)
//   * net.minecraft.client.model.geom.ModelPart.Polygon  (vertices[] + normal)
//   * net.minecraft.client.model.geom.ModelPart.Vertex   (x,y,z,u,v; worldX/Y/Z)
//   * net.minecraft.core.Direction.getUnitVec3f()         (the face normal)
//   * net.minecraft.client.model.geom.builders.CubeDeformation.extend (grow math)
//
// The Cube ctor is GL-free: it only computes floats and Direction unit vectors,
// never a VertexConsumer or GL context (compile() does, but we never call it).
// Cube.polygons is a public field; Polygon.vertices()/normal() are record
// accessors; Vertex.x()/y()/z()/u()/v() are record accessors. All reflection-free.
//
// Floats exchanged as raw IEEE-754 bits (%08x of Float.floatToRawIntBits).
//
//   mcpp/tools/run_groundtruth.ps1 -Tool CubeDefinitionParity -Out mcpp/build/cube_definition.tsv

import java.util.EnumSet;
import java.util.Set;

import net.minecraft.client.model.geom.ModelPart;
import net.minecraft.client.model.geom.builders.CubeDeformation;
import net.minecraft.core.Direction;
import org.joml.Vector3fc;

public class CubeDefinitionParity {
    static final java.io.PrintStream O = System.out;
    static String b(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // Representative cube batteries: covers grow=0, uniform grow, anisotropic
    // grow, mirror on/off, integer + fractional offsets/dims, partial face sets,
    // and the texScale!=64 path (so u/v remap divisors differ from 1).
    // {minX,minY,minZ, w,h,d, growX,growY,growZ, xTexOffs,yTexOffs, xTexSize,yTexSize, mirror, faceMask}
    // faceMask bits: 1=DOWN 2=UP 4=NORTH 8=SOUTH 16=WEST 32=EAST (all=63).
    static final float[][] CUBES = {
        {-4f,-4f,-4f, 8f,8f,8f, 0f,0f,0f, 0f,0f, 64f,64f, 0f, 63f},          // plain box, all faces
        {-4f,-4f,-4f, 8f,8f,8f, 0f,0f,0f, 0f,0f, 64f,64f, 1f, 63f},          // same, mirrored
        {-2f,0f,-2f, 4f,12f,4f, 0.25f,0.25f,0.25f, 16f,16f, 64f,64f, 0f, 63f}, // uniform grow
        {-2f,0f,-2f, 4f,12f,4f, 0.25f,0.5f,1.0f, 16f,16f, 64f,64f, 0f, 63f},  // anisotropic grow
        {-2f,0f,-2f, 4f,12f,4f, 0.25f,0.5f,1.0f, 16f,16f, 64f,64f, 1f, 63f},  // anisotropic grow, mirror
        {-3.5f,1.5f,-0.5f, 7f,3f,5f, 0f,0f,0f, 40f,16f, 64f,32f, 0f, 63f},    // frac offs, texScale 64x32
        {-3.5f,1.5f,-0.5f, 7f,3f,5f, 0.1f,0.2f,0.3f, 40f,16f, 128f,128f, 1f, 63f}, // frac+grow+mirror+128
        {0f,0f,0f, 1f,1f,1f, 0f,0f,0f, 0f,0f, 16f,16f, 0f, 63f},             // unit cube, small texScale
        {0f,0f,0f, 2f,2f,2f, -0.5f,-0.5f,-0.5f, 0f,0f, 64f,64f, 0f, 63f},    // negative grow (shrink)
        {-4f,-4f,-4f, 8f,8f,8f, 0f,0f,0f, 0f,0f, 64f,64f, 0f, 3f},           // only DOWN+UP
        {-4f,-4f,-4f, 8f,8f,8f, 0f,0f,0f, 0f,0f, 64f,64f, 0f, 60f},          // N+S+W+E only
        {-4f,-4f,-4f, 8f,8f,8f, 0f,0f,0f, 0f,0f, 64f,64f, 1f, 21f},          // DOWN+NORTH+WEST, mirror
        {-1f,-2f,-3f, 2f,4f,6f, 0.05f,0f,0.15f, 8f,8f, 64f,64f, 0f, 63f},    // mixed grow, asym dims
        {-1f,-2f,-3f, 2f,4f,6f, 0.05f,0f,0.15f, 8f,8f, 64f,64f, 1f, 63f},    // same, mirror
    };

    // Build the Set<Direction> from the face mask.
    static Set<Direction> faces(int mask) {
        EnumSet<Direction> s = EnumSet.noneOf(Direction.class);
        if ((mask & 1) != 0)  s.add(Direction.DOWN);
        if ((mask & 2) != 0)  s.add(Direction.UP);
        if ((mask & 4) != 0)  s.add(Direction.NORTH);
        if ((mask & 8) != 0)  s.add(Direction.SOUTH);
        if ((mask & 16) != 0) s.add(Direction.WEST);
        if ((mask & 32) != 0) s.add(Direction.EAST);
        return s;
    }

    // CubeDeformation.growX/Y/Z are package-private; read them reflectively.
    static final java.lang.reflect.Field GX, GY, GZ;
    static {
        try {
            GX = CubeDeformation.class.getDeclaredField("growX");
            GY = CubeDeformation.class.getDeclaredField("growY");
            GZ = CubeDeformation.class.getDeclaredField("growZ");
            GX.setAccessible(true); GY.setAccessible(true); GZ.setAccessible(true);
        } catch (NoSuchFieldException e) {
            throw new ExceptionInInitializerError(e);
        }
    }
    static float gx(CubeDeformation d) { try { return GX.getFloat(d); } catch (Exception e) { throw new RuntimeException(e); } }
    static float gy(CubeDeformation d) { try { return GY.getFloat(d); } catch (Exception e) { throw new RuntimeException(e); } }
    static float gz(CubeDeformation d) { try { return GZ.getFloat(d); } catch (Exception e) { throw new RuntimeException(e); } }

    static void dumpCube(int idx, ModelPart.Cube cube) {
        // Cube box fields (un-grown).
        O.println("BOX\t" + idx + "\t" + b(cube.minX) + "\t" + b(cube.minY) + "\t" + b(cube.minZ)
            + "\t" + b(cube.maxX) + "\t" + b(cube.maxY) + "\t" + b(cube.maxZ)
            + "\t" + cube.polygons.length);
        for (int pi = 0; pi < cube.polygons.length; pi++) {
            ModelPart.Polygon poly = cube.polygons[pi];
            Vector3fc n = poly.normal();
            // POLY <cube> <polyIndex> <nx> <ny> <nz>
            O.println("POLY\t" + idx + "\t" + pi + "\t" + b(n.x()) + "\t" + b(n.y()) + "\t" + b(n.z()));
            ModelPart.Vertex[] vs = poly.vertices();
            for (int vi = 0; vi < vs.length; vi++) {
                ModelPart.Vertex v = vs[vi];
                // VTX <cube> <polyIndex> <vertIndex> <x> <y> <z> <u> <v> <worldX> <worldY> <worldZ>
                O.println("VTX\t" + idx + "\t" + pi + "\t" + vi
                    + "\t" + b(v.x()) + "\t" + b(v.y()) + "\t" + b(v.z())
                    + "\t" + b(v.u()) + "\t" + b(v.v())
                    + "\t" + b(v.worldX()) + "\t" + b(v.worldY()) + "\t" + b(v.worldZ()));
            }
        }
    }

    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {
            // Cube/Polygon/Vertex pure math needs no registries; bootstrap is best-effort.
        }

        for (int i = 0; i < CUBES.length; i++) {
            float[] c = CUBES[i];
            float minX = c[0], minY = c[1], minZ = c[2];
            float w = c[3], h = c[4], d = c[5];
            float gx = c[6], gy = c[7], gz = c[8];
            int xTexOffs = (int) c[9], yTexOffs = (int) c[10];
            float xTexSize = c[11], yTexSize = c[12];
            boolean mirror = c[13] != 0f;
            int mask = (int) c[14];

            ModelPart.Cube cube = new ModelPart.Cube(
                xTexOffs, yTexOffs, minX, minY, minZ, w, h, d,
                gx, gy, gz, mirror, xTexSize, yTexSize, faces(mask));
            dumpCube(i, cube);
        }

        // CubeDeformation.extend math (pure float adds). idx offset by 1000 so
        // the C++ test can key on the GROW tag, not the cube index.
        float[][] defs = {
            {0f,0f,0f}, {0.25f,0.25f,0.25f}, {-1f,0.5f,2.25f}, {0.1f,0.2f,0.3f},
        };
        float[] scalarFactors = {0f, 0.5f, -0.75f, 1.5f};
        float[][] vecFactors = {{0f,0f,0f}, {0.1f,-0.2f,0.3f}, {1.5f,2.5f,-3.5f}};
        for (int di = 0; di < defs.length; di++) {
            CubeDeformation base = new CubeDeformation(defs[di][0], defs[di][1], defs[di][2]);
            for (int fi = 0; fi < scalarFactors.length; fi++) {
                CubeDeformation e = base.extend(scalarFactors[fi]);
                O.println("GROW1\t" + di + "\t" + fi + "\t" + b(gx(e)) + "\t" + b(gy(e)) + "\t" + b(gz(e)));
            }
            for (int fi = 0; fi < vecFactors.length; fi++) {
                CubeDeformation e = base.extend(vecFactors[fi][0], vecFactors[fi][1], vecFactors[fi][2]);
                O.println("GROW3\t" + di + "\t" + fi + "\t" + b(gx(e)) + "\t" + b(gy(e)) + "\t" + b(gz(e)));
            }
        }
    }
}
