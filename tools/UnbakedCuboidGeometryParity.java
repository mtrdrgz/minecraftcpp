// Ground truth for net.minecraft.client.resources.model.cuboid.UnbakedCuboidGeometry.bake
// (UnbakedCuboidGeometry.java:22-74) — the element-level block-model bake. Drives the REAL
// bake with a GL-free MaterialBaker that resolves each face.texture() key to a known sprite
// (Bootstrap.bootStrap() is called so MaterialInfo.of -> Sheets/RenderType init works headless;
// the MaterialInfo is metadata, never compared). Emits the QuadCollection buckets per model.
//
//   tools/run_groundtruth.ps1 -Tool UnbakedCuboidGeometryParity -Out mcpp/build/unbaked_cuboid.tsv
//
// Per model case (multi-line; floats %08x rawIntBits, longs %016x, ints decimal):
//   CASE
//   ELEM  fx fy fz  tx ty tz  hasElem axis angleHex rescale  ox oy oz
//   FACE  facing  u0 u1 v0 v1  hasUv minU minV maxU maxV  uvRot tintIndex cullDir
//   ...(more FACE for this element, then more ELEM/FACE)...
//   OUT   bucketKey count          # bucketKey -1 = unculled, 0..5 = culled by Direction ordinal
//   QUAD  dir  p0x..p3z(12)  uv0..uv3(4)
//   ...(count QUAD lines; 7 OUT sections: -1,0,1,2,3,4,5)...
//   END

import com.mojang.math.Quadrant;
import java.util.EnumMap;
import java.util.List;
import java.util.Map;
import net.minecraft.client.renderer.block.dispatch.ModelState;
import net.minecraft.client.renderer.texture.SpriteContents;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import net.minecraft.client.resources.metadata.animation.FrameSize;
import net.minecraft.client.resources.model.ModelBaker;
import net.minecraft.client.resources.model.ModelDebugName;
import net.minecraft.client.resources.model.cuboid.CuboidFace;
import net.minecraft.client.resources.model.cuboid.CuboidModelElement;
import net.minecraft.client.resources.model.cuboid.CuboidRotation;
import net.minecraft.client.resources.model.cuboid.UnbakedCuboidGeometry;
import net.minecraft.client.resources.model.geometry.BakedQuad;
import net.minecraft.client.resources.model.geometry.QuadCollection;
import net.minecraft.client.resources.model.sprite.Material;
import net.minecraft.client.resources.model.sprite.MaterialBaker;
import net.minecraft.client.resources.model.sprite.TextureSlots;
import net.minecraft.core.Direction;
import net.minecraft.resources.Identifier;
import com.mojang.blaze3d.platform.NativeImage;
import org.joml.Vector3f;
import org.joml.Vector3fc;

public class UnbakedCuboidGeometryParity {
    static final java.io.PrintStream O = System.out;
    static String f(float x) { return String.format("%08x", Float.floatToRawIntBits(x)); }
    static String l(long x) { return String.format("%016x", x); }

    static TextureAtlasSprite[] SPRITES;

    static TextureAtlasSprite makeSprite(int atlasW, int atlasH, int x, int y, int padding, int cw, int ch) {
        NativeImage img = new NativeImage(cw, ch, false);
        SpriteContents contents = new SpriteContents(Identifier.withDefaultNamespace("probe"), new FrameSize(cw, ch), img);
        return new TextureAtlasSprite(Identifier.withDefaultNamespace("blocks"), contents, atlasW, atlasH, x, y, padding) {};
    }
    static TextureAtlasSprite spriteFor(String key) {
        return SPRITES[Integer.parseInt(key.substring(1)) % SPRITES.length];
    }

    static final ModelState IDENTITY_STATE = new ModelState() {};
    static final ModelDebugName NAME = () -> "test";

    static final MaterialBaker MATERIALS = new MaterialBaker() {
        @Override public Material.Baked get(final Material material, final ModelDebugName name) {
            return new Material.Baked(spriteFor(material.sprite().getPath()), material.forceTranslucent());
        }
        @Override public Material.Baked reportMissingReference(final String reference, final ModelDebugName name) {
            return new Material.Baked(spriteFor("s0"), false);
        }
        @Override public Material.Baked resolveSlot(final TextureSlots slots, final String id, final ModelDebugName name) {
            return new Material.Baked(spriteFor(id), false);
        }
    };

    static final ModelBaker.Interner INTERNER = new ModelBaker.Interner() {
        @Override public Vector3fc vector(final Vector3fc v) { return v; }
        @Override public BakedQuad.MaterialInfo materialInfo(final BakedQuad.MaterialInfo m) { return m; }
    };

    static final ModelBaker MODEL_BAKER = new ModelBaker() {
        @Override public net.minecraft.client.resources.model.ResolvedModel getModel(final Identifier location) { throw new UnsupportedOperationException(); }
        @Override public net.minecraft.client.renderer.block.dispatch.BlockStateModelPart missingBlockModelPart() { throw new UnsupportedOperationException(); }
        @Override public MaterialBaker materials() { return MATERIALS; }
        @Override public ModelBaker.Interner interner() { return INTERNER; }
        @Override public <T> T compute(final ModelBaker.SharedOperationKey<T> key) { throw new UnsupportedOperationException(); }
    };

    // ── builders for a model case ──
    static Vector3f v(float a, float b, float c) { return new Vector3f(a, b, c); }

    static CuboidFace face(String texKey, Direction cull, int tint, CuboidFace.UVs uv, Quadrant rot) {
        return new CuboidFace(cull, tint, texKey, uv, rot);
    }

    static CuboidModelElement elem(Vector3f from, Vector3f to, CuboidRotation rot, Map<Direction, CuboidFace> faces) {
        return new CuboidModelElement(from, to, faces, rot, true, 0);
    }

    static void emit(List<CuboidModelElement> elements) {
        QuadCollection qc = UnbakedCuboidGeometry.bake(elements, TextureSlots.EMPTY, MODEL_BAKER, IDENTITY_STATE, NAME);
        O.println("CASE");
        for (CuboidModelElement e : elements) {
            boolean hasElem = e.rotation() != null;
            int axis = 0; float angle = 0f; boolean rescale = false; Vector3fc og = v(0, 0, 0);
            // (axis/angle/rescale recovered from the SingleAxisRotation we built; carried via fields below)
            EmitElem meta = ELEM_META.get(e);
            if (meta != null) { axis = meta.axis; angle = meta.angle; rescale = meta.rescale; }
            if (hasElem) og = e.rotation().origin();
            O.println("ELEM\t" + f(e.from().x()) + "\t" + f(e.from().y()) + "\t" + f(e.from().z())
                + "\t" + f(e.to().x()) + "\t" + f(e.to().y()) + "\t" + f(e.to().z())
                + "\t" + (hasElem ? 1 : 0) + "\t" + axis + "\t" + f(angle) + "\t" + (rescale ? 1 : 0)
                + "\t" + f(og.x()) + "\t" + f(og.y()) + "\t" + f(og.z()));
            for (Direction d : Direction.values()) {
                CuboidFace cf = e.faces().get(d);
                if (cf == null) continue;
                TextureAtlasSprite sp = spriteFor(cf.texture());
                CuboidFace.UVs uv = cf.uvs();
                boolean hasUv = uv != null;
                float mnU = hasUv ? uv.minU() : 0f, mnV = hasUv ? uv.minV() : 0f, mxU = hasUv ? uv.maxU() : 0f, mxV = hasUv ? uv.maxV() : 0f;
                int cull = cf.cullForDirection() == null ? -1 : cf.cullForDirection().ordinal();
                O.println("FACE\t" + d.ordinal()
                    + "\t" + f(sp.getU0()) + "\t" + f(sp.getU1()) + "\t" + f(sp.getV0()) + "\t" + f(sp.getV1())
                    + "\t" + (hasUv ? 1 : 0) + "\t" + f(mnU) + "\t" + f(mnV) + "\t" + f(mxU) + "\t" + f(mxV)
                    + "\t" + cf.rotation().shift + "\t" + cf.tintIndex() + "\t" + cull);
            }
        }
        emitBucket(-1, qc.getQuads(null));
        for (Direction d : Direction.values()) emitBucket(d.ordinal(), qc.getQuads(d));
        O.println("END");
    }

    static void emitBucket(int key, List<BakedQuad> quads) {
        O.println("OUT\t" + key + "\t" + quads.size());
        for (BakedQuad q : quads) {
            StringBuilder sb = new StringBuilder("QUAD\t").append(q.direction().ordinal());
            for (int i = 0; i < 4; i++) {
                Vector3fc p = q.position(i);
                sb.append('\t').append(f(p.x())).append('\t').append(f(p.y())).append('\t').append(f(p.z()));
            }
            for (int i = 0; i < 4; i++) sb.append('\t').append(l(q.packedUV(i)));
            O.println(sb);
        }
    }

    // element rotation metadata (axis/angle/rescale) recovered by identity map
    record EmitElem(int axis, float angle, boolean rescale) {}
    static final Map<CuboidModelElement, EmitElem> ELEM_META = new java.util.IdentityHashMap<>();

    static CuboidRotation rot(int axis, float angle, boolean rescale, Vector3f origin) {
        return new CuboidRotation(origin, new CuboidRotation.SingleAxisRotation(Direction.Axis.values()[axis], angle), rescale);
    }

    static Map<Direction, CuboidFace> faces() { return new EnumMap<>(Direction.class); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        SPRITES = new TextureAtlasSprite[]{
            makeSprite(256, 256, 0, 0, 0, 16, 16),
            makeSprite(512, 256, 48, 32, 0, 16, 16),
            makeSprite(1024, 512, 160, 96, 1, 14, 14),
        };

        Quadrant[] Q = Quadrant.values();

        // Case 1: full cube, all 6 faces, no cull, no explicit uv, varied textures + rotations.
        {
            Map<Direction, CuboidFace> fs = faces();
            for (Direction d : Direction.values())
                fs.put(d, face("s" + (d.ordinal() % 3), null, -1, null, Q[d.ordinal() % 4]));
            emit(List.of(elem(v(0, 0, 0), v(16, 16, 16), null, fs)));
        }
        // Case 2: full cube, all faces CULLED toward their own facing, explicit uvs.
        {
            Map<Direction, CuboidFace> fs = faces();
            for (Direction d : Direction.values())
                fs.put(d, face("s1", d, d.ordinal(), new CuboidFace.UVs(2, 4, 14, 12), Q[(d.ordinal() + 1) % 4]));
            emit(List.of(elem(v(0, 0, 0), v(16, 16, 16), null, fs)));
        }
        // Case 3: sub-box, subset of faces, mix cull/uncull + uv/no-uv.
        {
            Map<Direction, CuboidFace> fs = faces();
            fs.put(Direction.UP, face("s0", null, -1, null, Q[0]));
            fs.put(Direction.DOWN, face("s2", Direction.DOWN, 0, new CuboidFace.UVs(0, 0, 16, 16), Q[2]));
            fs.put(Direction.NORTH, face("s1", Direction.NORTH, -1, null, Q[3]));
            fs.put(Direction.EAST, face("s0", null, 1, new CuboidFace.UVs(16, 0, 0, 16), Q[1]));
            emit(List.of(elem(v(2, 3, 4), v(14, 12, 10), null, fs)));
        }
        // Case 4: degenerate flat panel (from.z == to.z) -> only Z faces should draw.
        {
            Map<Direction, CuboidFace> fs = faces();
            for (Direction d : Direction.values())
                fs.put(d, face("s" + (d.ordinal() % 3), null, -1, null, Q[0]));
            emit(List.of(elem(v(0, 0, 8), v(16, 16, 8), null, fs)));   // flat in Z
        }
        // Case 5: degenerate flat panel (from.y == to.y) -> only Y faces should draw, with cull.
        {
            Map<Direction, CuboidFace> fs = faces();
            for (Direction d : Direction.values())
                fs.put(d, face("s1", d, -1, new CuboidFace.UVs(1, 1, 15, 15), Q[d.ordinal() % 4]));
            emit(List.of(elem(v(0, 6, 0), v(16, 6, 16), null, fs)));   // flat in Y
        }
        // Case 6: element rotation (winding SKIPPED), several axes/angles/origins.
        int[] axes = {0, 1, 2};
        float[] angles = {22.5f, 45f, -45f};
        boolean[] rescales = {false, true};
        Vector3f[] origins = {v(8, 8, 8), v(0, 0, 0), v(8, 0, 8)};
        int oc = 0;
        for (int ax : axes) {
            for (float ang : angles) {
                for (boolean rs : rescales) {
                    Map<Direction, CuboidFace> fs = faces();
                    fs.put(Direction.UP, face("s0", null, -1, null, Q[0]));
                    fs.put(Direction.NORTH, face("s1", Direction.NORTH, -1, new CuboidFace.UVs(3, 5, 13, 11), Q[2]));
                    fs.put(Direction.WEST, face("s2", null, 2, null, Q[1]));
                    CuboidRotation cr = rot(ax, ang, rs, origins[oc % origins.length]);
                    oc++;
                    CuboidModelElement e = elem(v(2, 2, 2), v(14, 14, 14), cr, fs);
                    ELEM_META.put(e, new EmitElem(ax, ang, rs));
                    emit(List.of(e));
                }
            }
        }
        // Case 7: multi-element model (two boxes), mixed cull/uncull, to exercise bucket ordering.
        {
            Map<Direction, CuboidFace> a = faces();
            a.put(Direction.UP, face("s0", null, -1, null, Q[0]));
            a.put(Direction.SOUTH, face("s1", Direction.SOUTH, -1, null, Q[1]));
            Map<Direction, CuboidFace> b = faces();
            b.put(Direction.DOWN, face("s2", Direction.DOWN, 0, new CuboidFace.UVs(0, 0, 8, 8), Q[3]));
            b.put(Direction.SOUTH, face("s0", Direction.SOUTH, -1, null, Q[2]));
            b.put(Direction.EAST, face("s1", null, 5, null, Q[0]));
            emit(List.of(
                elem(v(0, 0, 0), v(16, 8, 16), null, a),
                elem(v(4, 8, 4), v(12, 16, 12), null, b)));
        }

        System.out.flush();
    }
}
