// Ground truth for the block-model JSON deserializers (CuboidModel.GSON: CuboidModel/
// CuboidModelElement/CuboidFace deserializers, CuboidModel.java:31-95). Parses each model JSON via
// the PUBLIC CuboidModel.fromStream, bakes its geometry through the REAL UnbakedCuboidGeometry.bake
// (Bootstrap so Sheets/MaterialInfo.of is headless-safe; GL-free MaterialBaker maps face texture
// keys -> known sprites), and resolves its texture slots. Emits the JSON (base64) + the baked quad
// buckets + parsed metadata so the C++ re-parses the SAME JSON and compares.
//
//   tools/run_groundtruth.ps1 -Tool CuboidModelJsonParity -Out mcpp/build/cuboid_model_json.tsv

import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.List;
import net.minecraft.client.renderer.block.dispatch.ModelState;
import net.minecraft.client.renderer.texture.SpriteContents;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import net.minecraft.client.resources.metadata.animation.FrameSize;
import net.minecraft.client.resources.model.ModelBaker;
import net.minecraft.client.resources.model.ModelDebugName;
import net.minecraft.client.resources.model.cuboid.CuboidFace;
import net.minecraft.client.resources.model.cuboid.CuboidModel;
import net.minecraft.client.resources.model.cuboid.CuboidModelElement;
import net.minecraft.client.resources.model.cuboid.UnbakedCuboidGeometry;
import net.minecraft.client.resources.model.geometry.BakedQuad;
import net.minecraft.client.resources.model.geometry.QuadCollection;
import net.minecraft.client.resources.model.sprite.Material;
import net.minecraft.client.resources.model.sprite.MaterialBaker;
import net.minecraft.client.resources.model.sprite.TextureSlots;
import net.minecraft.core.Direction;
import net.minecraft.resources.Identifier;
import com.mojang.blaze3d.platform.NativeImage;
import org.joml.Vector3fc;

public class CuboidModelJsonParity {
    static final java.io.PrintStream O = System.out;
    static String f(float x) { return String.format("%08x", Float.floatToRawIntBits(x)); }
    static String l(long x) { return String.format("%016x", x); }

    static TextureAtlasSprite[] SPRITES;
    static TextureAtlasSprite makeSprite(int aw, int ah, int x, int y, int pad, int cw, int ch) {
        NativeImage img = new NativeImage(cw, ch, false);
        SpriteContents c = new SpriteContents(Identifier.withDefaultNamespace("probe"), new FrameSize(cw, ch), img);
        return new TextureAtlasSprite(Identifier.withDefaultNamespace("blocks"), c, aw, ah, x, y, pad) {};
    }
    static TextureAtlasSprite spriteFor(String key) {
        int i = key.startsWith("s") ? (key.charAt(1) - '0') : 0;
        return SPRITES[i % SPRITES.length];
    }

    static final ModelState IDENTITY_STATE = new ModelState() {};
    static final ModelDebugName NAME = () -> "test";
    static final MaterialBaker MATERIALS = new MaterialBaker() {
        public Material.Baked get(Material m, ModelDebugName n) { return new Material.Baked(spriteFor(m.sprite().getPath()), m.forceTranslucent()); }
        public Material.Baked reportMissingReference(String r, ModelDebugName n) { return new Material.Baked(spriteFor("s0"), false); }
        public Material.Baked resolveSlot(TextureSlots s, String id, ModelDebugName n) { return new Material.Baked(spriteFor(id), false); }
    };
    static final ModelBaker.Interner INTERNER = new ModelBaker.Interner() {
        public Vector3fc vector(Vector3fc v) { return v; }
        public BakedQuad.MaterialInfo materialInfo(BakedQuad.MaterialInfo m) { return m; }
    };
    static final ModelBaker MODEL_BAKER = new ModelBaker() {
        public net.minecraft.client.resources.model.ResolvedModel getModel(Identifier l) { throw new UnsupportedOperationException(); }
        public net.minecraft.client.renderer.block.dispatch.BlockStateModelPart missingBlockModelPart() { throw new UnsupportedOperationException(); }
        public MaterialBaker materials() { return MATERIALS; }
        public ModelBaker.Interner interner() { return INTERNER; }
        public <T> T compute(ModelBaker.SharedOperationKey<T> k) { throw new UnsupportedOperationException(); }
    };

    static void emit(String jsonStr, List<String> texLookups) {
        CuboidModel model = CuboidModel.fromStream(new java.io.StringReader(jsonStr));
        O.println("CASE");
        O.println("JSON\t" + Base64.getEncoder().encodeToString(jsonStr.getBytes(StandardCharsets.UTF_8)));

        Boolean ao = model.ambientOcclusion();
        var guiLight = model.guiLight();
        Identifier parent = model.parent();
        O.println("META\t" + (ao != null ? 1 : 0) + "\t" + (ao != null && ao ? 1 : 0)
            + "\t" + (guiLight != null ? 1 : 0) + "\t" + (guiLight != null ? guiLight.ordinal() : -1)
            + "\t" + (parent != null ? 1 : 0) + "\t" + (parent != null ? parent.toString() : "-"));

        List<CuboidModelElement> elements = model.geometry() != null
            ? ((UnbakedCuboidGeometry) model.geometry()).elements()
            : List.of();
        O.println("ELEMMETA\t" + elements.size());
        for (CuboidModelElement e : elements) {
            StringBuilder sb = new StringBuilder("EM\t").append(e.shade() ? 1 : 0).append('\t').append(e.lightEmission());
            for (Direction d : Direction.values()) {
                CuboidFace cf = e.faces().get(d);
                sb.append('\t').append(cf != null ? cf.tintIndex() : -1);
            }
            O.println(sb);
        }

        // texture-slot resolution (parseTextureMap -> Resolver -> getMaterial per lookup name)
        TextureSlots resolved = new TextureSlots.Resolver().addLast(model.textureSlots()).resolve(NAME);
        O.println("TEX\t" + texLookups.size());
        for (String name : texLookups) {
            Material m = resolved.getMaterial(name);
            if (m == null) O.println("TL\t" + name + "\t0\t-\t0");
            else O.println("TL\t" + name + "\t1\t" + m.sprite().toString() + "\t" + (m.forceTranslucent() ? 1 : 0));
        }

        // bake geometry -> quad buckets
        QuadCollection qc = elements.isEmpty()
            ? QuadCollection.EMPTY
            : UnbakedCuboidGeometry.bake(elements, TextureSlots.EMPTY, MODEL_BAKER, IDENTITY_STATE, NAME);
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

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        SPRITES = new TextureAtlasSprite[]{
            makeSprite(256, 256, 0, 0, 0, 16, 16),    // s0: u0=0,u1=.0625,v0=0,v1=.0625
            makeSprite(256, 256, 16, 0, 0, 16, 16),   // s1: u0=.0625,u1=.125,v0=0,v1=.0625
            makeSprite(256, 256, 0, 16, 0, 16, 16),   // s2: u0=0,u1=.0625,v0=.0625,v1=.125
        };

        // 1: full cube, no uv, parent + textures(refs+values) + ao + gui_light.
        emit("""
        {
          "parent": "block/cube",
          "ambientocclusion": false,
          "gui_light": "front",
          "textures": { "particle": "#side", "side": "block/stone", "top": "block/oak_log_top" },
          "elements": [ { "from": [0,0,0], "to": [16,16,16], "faces": {
            "down": {"texture":"s0"}, "up": {"texture":"s1"}, "north": {"texture":"s2"},
            "south": {"texture":"s0"}, "west": {"texture":"s1"}, "east": {"texture":"s2"} } } ]
        }""", List.of("particle", "side", "top"));

        // 2: single-axis rotation + explicit uv + cullface + tintindex + face rotation.
        emit("""
        {
          "elements": [ {
            "from": [2,3,4], "to": [14,12,10],
            "rotation": {"origin":[8,8,8], "axis":"y", "angle":22.5, "rescale": true},
            "faces": {
              "up": {"texture":"s0", "uv":[2,4,14,12], "rotation":90, "tintindex":1},
              "down": {"texture":"s2", "cullface":"down"},
              "north": {"texture":"s1", "uv":[16,0,0,16], "rotation":270} } } ]
        }""", List.of());

        // 3: euler x/y/z rotation + shade false + light_emission.
        emit("""
        {
          "elements": [ {
            "from": [1,1,1], "to": [15,15,15],
            "rotation": {"origin":[8,0,8], "x":45, "y":0, "z":0},
            "shade": false, "light_emission": 7,
            "faces": { "up": {"texture":"s1"}, "west": {"texture":"s0","tintindex":3} } } ]
        }""", List.of());

        // 4: multi-element model.
        emit("""
        {
          "elements": [
            { "from": [0,0,0], "to": [16,8,16], "faces": {
                "up": {"texture":"s0"}, "south": {"texture":"s1","cullface":"south"} } },
            { "from": [4,8,4], "to": [12,16,12], "faces": {
                "down": {"texture":"s2","cullface":"down","uv":[0,0,8,8],"rotation":180},
                "east": {"texture":"s1"} } }
          ]
        }""", List.of());

        // 5: degenerate flat element (from.z==to.z) -> only Z faces; force_translucent texture.
        emit("""
        {
          "textures": { "trans": {"sprite":"block/glass","force_translucent":true}, "plain": "block/dirt" },
          "elements": [ { "from": [0,0,8], "to": [16,16,8], "faces": {
            "north": {"texture":"s0"}, "south": {"texture":"s1"}, "up": {"texture":"s2"} } } ]
        }""", List.of("trans", "plain"));

        // 6: gui_light side, no elements (parent-only model) -> EMPTY geometry.
        emit("""
        { "parent": "item/generated", "gui_light": "side",
          "textures": { "layer0": "item/diamond_sword" } }
        """, List.of("layer0"));
    }
}
