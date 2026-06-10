// Ground truth for net.minecraft.client.resources.model.cuboid.FaceBakery.bakeQuad
// (the interner overload, FaceBakery.java:73-124) — the full single-face bake:
// per-vertex position+UV, the sprite-UV lerp, UVPair.pack, calculateFacing, the
// elementRotation-gated recalculateWinding, and the UP-fallback direction.
//
//   tools/run_groundtruth.ps1 -Tool FaceBakeryBakeQuadParity -Out mcpp/build/face_bakery_bakequad.tsv
//
// The matrices are NOT serialized as raw floats; instead the model/uv transforms are
// the certified rotM(i) = Matrix4f.rotation(Quaternionf(QS[i])) and the element rotation
// is the certified SingleAxisRotation(axis,angle)+rescale — both already byte-exact in
// the C++ port (face_bakery_parity). The gate passes the generator INDICES + the booleans
// (hasModel = transformation()!=IDENTITY, hasUvTransform = !MatrixUtil.isIdentity(uvT)) so
// the C++ rebuilds the identical matrices. The sprite is a GL-free real TextureAtlasSprite;
// we emit its exact u0/u1/v0/v1 so getU/getV stays atlas-free.
//
// Row (tab-separated, all floats %08x rawIntBits, longs %016x, ints decimal):
//   BQ  facing  fx fy fz  tx ty tz  minU minV maxU maxV  uvRot  u0 u1 v0 v1
//       hasModel modelIdx  hasUvT uvIdx  hasElem elemAxis elemAngle elemRescale  ox oy oz
//       outDir  p0x p0y p0z p1x p1y p1z p2x p2y p2z p3x p3y p3z  uv0 uv1 uv2 uv3

import com.mojang.blaze3d.platform.NativeImage;
import com.mojang.math.MatrixUtil;
import com.mojang.math.Quadrant;
import com.mojang.math.Transformation;
import net.minecraft.client.renderer.block.dispatch.ModelState;
import net.minecraft.client.renderer.texture.SpriteContents;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import net.minecraft.client.resources.metadata.animation.FrameSize;
import net.minecraft.client.resources.model.ModelBaker;
import net.minecraft.client.resources.model.cuboid.CuboidFace;
import net.minecraft.client.resources.model.cuboid.CuboidRotation;
import net.minecraft.client.resources.model.cuboid.FaceBakery;
import net.minecraft.client.resources.model.geometry.BakedQuad;
import net.minecraft.core.Direction;
import net.minecraft.resources.Identifier;
import org.joml.Matrix4f;
import org.joml.Matrix4fc;
import org.joml.Quaternionf;
import org.joml.Vector3f;
import org.joml.Vector3fc;

public class FaceBakeryBakeQuadParity {
    static final java.io.PrintStream O = System.out;
    static final StringBuilder SB = new StringBuilder();

    static String f(float x) { return String.format("%08x", Float.floatToRawIntBits(x)); }
    static String l(long x) { return String.format("%016x", x); }

    // The certified quaternion table (must match FaceBakeryBakeQuadParityTest.cpp QS[]).
    static final float[][] QS = {
        {0, 0, 0, 1}, {0.5f, 0.5f, 0.5f, 0.5f}, {0.5f, 0, 0, 0.5f},
        {-0.5f, 0.5f, -0.5f, 0.5f}, {0.125f, 0.375f, -0.625f, 0.75f}
    };
    static Matrix4f rotM(int i) {
        return new Matrix4f().rotation(new Quaternionf(QS[i][0], QS[i][1], QS[i][2], QS[i][3]));
    }

    // Identity ModelState (all defaults): transformation()==IDENTITY, inverseFaceTransformation==NO_TRANSFORM.
    static final ModelState IDENTITY_STATE = new ModelState() {};

    static ModelState stateOf(final Transformation tf, final Matrix4fc inv) {
        return new ModelState() {
            @Override public Transformation transformation() { return tf; }
            @Override public Matrix4fc inverseFaceTransformation(final Direction face) { return inv; }
        };
    }

    static final ModelBaker.Interner INTERNER = new ModelBaker.Interner() {
        @Override public Vector3fc vector(final Vector3fc v) { return v; }
        @Override public BakedQuad.MaterialInfo materialInfo(final BakedQuad.MaterialInfo m) { return m; }
    };

    // A GL-free real sprite with chosen atlas geometry; getU0/getU1/getV0/getV1 are the
    // real constructor formula's bounds. Only sprite() is read by bakeQuad.
    static TextureAtlasSprite makeSprite(int atlasW, int atlasH, int x, int y, int padding, int cw, int ch) {
        NativeImage img = new NativeImage(cw, ch, false);
        SpriteContents contents = new SpriteContents(Identifier.withDefaultNamespace("probe"), new FrameSize(cw, ch), img);
        return new TextureAtlasSprite(Identifier.withDefaultNamespace("blocks"), contents, atlasW, atlasH, x, y, padding) {};
    }

    static BakedQuad.MaterialInfo matInfoOf(TextureAtlasSprite sprite) {
        // layer/itemRenderType are never read by bakeQuad; pass null (avoids GL Sheets/RenderType).
        return new BakedQuad.MaterialInfo(sprite, null, null, -1, true, 0);
    }

    static int skipped = 0;

    static void emit(
        Direction facing, Vector3f from, Vector3f to, CuboidFace.UVs uvs, Quadrant uvRot,
        TextureAtlasSprite sprite, ModelState state, CuboidRotation element,
        // metadata to echo for the C++ side
        int modelIdx, int uvIdx, int elemAxis, float elemAngle, boolean elemRescale) {

        BakedQuad.MaterialInfo mat = matInfoOf(sprite);
        BakedQuad q;
        try {
            q = FaceBakery.bakeQuad(INTERNER, from, to, uvs, uvRot, mat, facing, state, element);
        } catch (IllegalStateException ex) {
            // Vanilla recalculateWinding throws "Can't find vertex to swap" for non-axis-aligned
            // faces — an input the real model loader never produces (rotations are 90° steps).
            // Such inputs are non-physical for this gate; skip (mirrors prior non-physical trims).
            skipped++;
            return;
        }

        boolean hasModel = state.transformation() != Transformation.IDENTITY;
        boolean hasUvT = !MatrixUtil.isIdentity(state.inverseFaceTransformation(facing));
        boolean hasElem = element != null;
        Vector3fc ox = hasElem ? element.origin() : new Vector3f();

        SB.setLength(0);
        SB.append("BQ\t").append(facing.ordinal());
        SB.append('\t').append(f(from.x())).append('\t').append(f(from.y())).append('\t').append(f(from.z()));
        SB.append('\t').append(f(to.x())).append('\t').append(f(to.y())).append('\t').append(f(to.z()));
        SB.append('\t').append(f(uvs.minU())).append('\t').append(f(uvs.minV()))
          .append('\t').append(f(uvs.maxU())).append('\t').append(f(uvs.maxV()));
        SB.append('\t').append(uvRot.shift);
        SB.append('\t').append(f(sprite.getU0())).append('\t').append(f(sprite.getU1()))
          .append('\t').append(f(sprite.getV0())).append('\t').append(f(sprite.getV1()));
        SB.append('\t').append(hasModel ? 1 : 0).append('\t').append(modelIdx);
        SB.append('\t').append(hasUvT ? 1 : 0).append('\t').append(uvIdx);
        SB.append('\t').append(hasElem ? 1 : 0).append('\t').append(elemAxis)
          .append('\t').append(f(elemAngle)).append('\t').append(elemRescale ? 1 : 0);
        SB.append('\t').append(f(ox.x())).append('\t').append(f(ox.y())).append('\t').append(f(ox.z()));
        // outputs
        SB.append('\t').append(q.direction().ordinal());
        for (int i = 0; i < 4; i++) {
            Vector3fc p = q.position(i);
            SB.append('\t').append(f(p.x())).append('\t').append(f(p.y())).append('\t').append(f(p.z()));
        }
        for (int i = 0; i < 4; i++) SB.append('\t').append(l(q.packedUV(i)));
        O.println(SB);
    }

    public static void main(String[] args) throws Exception {
        TextureAtlasSprite s0 = makeSprite(256, 256, 0, 0, 0, 16, 16);
        TextureAtlasSprite s1 = makeSprite(512, 256, 48, 32, 0, 16, 16);
        TextureAtlasSprite s2 = makeSprite(1024, 512, 160, 96, 1, 14, 14);
        TextureAtlasSprite[] sprites = {s0, s1, s2};

        // A range of element boxes (model space [0,16]).
        Vector3f[][] boxes = {
            {new Vector3f(0, 0, 0), new Vector3f(16, 16, 16)},   // full cube
            {new Vector3f(2, 3, 4), new Vector3f(14, 12, 10)},   // sub-box
            {new Vector3f(5, 0, 5), new Vector3f(11, 16, 11)},   // pillar
            {new Vector3f(0, 6, 0), new Vector3f(16, 10, 16)},   // slab-ish
        };
        // UV rectangles (model uv space [0,16]); include flipped/rotated rects.
        CuboidFace.UVs[] uvSet = {
            new CuboidFace.UVs(0, 0, 16, 16),
            new CuboidFace.UVs(2, 4, 14, 12),
            new CuboidFace.UVs(16, 0, 0, 16),     // U-flipped
            new CuboidFace.UVs(0, 16, 16, 0),     // V-flipped
            new CuboidFace.UVs(6, 6, 10, 10),
        };
        Quadrant[] quads = Quadrant.values();
        Direction[] dirs = Direction.values();

        // Family A: identity ModelState, no element rotation (winding APPLIED). Sweep
        // every facing × uvRotation × uv × box × sprite-ish subset.
        int sidx = 0;
        for (Direction d : dirs) {
            for (Vector3f[] box : boxes) {
                for (CuboidFace.UVs uv : uvSet) {
                    for (Quadrant qr : quads) {
                        TextureAtlasSprite sp = sprites[sidx % sprites.length];
                        sidx++;
                        emit(d, box[0], box[1], uv, qr, sp, IDENTITY_STATE, null, -1, -1, 0, 0f, false);
                    }
                }
            }
        }

        // Family B: element rotation present (winding SKIPPED — no recalculateWinding throw),
        // so this case family freely exercises BOTH element rotation AND arbitrary model
        // transforms (modelIdx in {-1=none, 1, 3}). Certifies bakeVertexPosition's element +
        // model paths, the winding-skip branch, and calculateFacing on fully-rotated geometry.
        int[] axes = {0, 1, 2};          // X, Y, Z (Direction.Axis ordinals)
        float[] angles = {22.5f, 45f, -45f, 30f};
        boolean[] rescales = {false, true};
        int[] modelIdxs = {-1, 1, 3};
        Vector3f[] origins = {new Vector3f(8, 8, 8), new Vector3f(0, 0, 0), new Vector3f(8, 0, 8)};
        int oCounter = 0;
        for (Direction d : dirs) {
            for (int ax : axes) {
                for (float ang : angles) {
                    for (boolean rs : rescales) {
                        for (int mi : modelIdxs) {
                            Vector3f org = origins[oCounter % origins.length];
                            oCounter++;
                            CuboidRotation.RotationValue rv =
                                new CuboidRotation.SingleAxisRotation(Direction.Axis.values()[ax], ang);
                            CuboidRotation cr = new CuboidRotation(org, rv, rs);
                            ModelState st = (mi < 0)
                                ? IDENTITY_STATE
                                : stateOf(new Transformation(rotM(mi)), ModelState.NO_TRANSFORM);
                            TextureAtlasSprite sp = sprites[(ax + d.ordinal()) % sprites.length];
                            emit(d, boxes[1][0], boxes[1][1], uvSet[1], quads[ax % 4], sp, st, cr,
                                 mi, -1, ax, ang, rs);
                        }
                    }
                }
            }
        }

        // Family C: identity model transform + non-identity UV transform, no element
        // (winding APPLIED on clean axis-aligned positions — no throw — while the per-vertex
        // UV goes through the hasUvTransform branch end-to-end). uvIdx in 1..4 (rotM is
        // non-identity, so MatrixUtil.isIdentity == false -> hasUvTransform true).
        for (Direction d : dirs) {
            for (int box = 0; box < boxes.length; box++) {
                for (int ui = 1; ui <= 4; ui++) {
                    ModelState st = stateOf(Transformation.IDENTITY, rotM(ui));
                    TextureAtlasSprite sp = sprites[(box + ui) % sprites.length];
                    emit(d, boxes[box][0], boxes[box][1], uvSet[ui % uvSet.length], quads[(ui + 1) % 4], sp, st, null,
                         -1, ui, 0, 0f, false);
                }
            }
        }

        System.err.println("FaceBakeryBakeQuadParity: skipped " + skipped + " non-physical cases");
    }
}
