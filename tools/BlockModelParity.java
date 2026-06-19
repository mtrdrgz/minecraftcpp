// Ground truth for the C++ block-model pipeline (mcpp/src/render/model/).
//
// Runs the REAL 26.1.2 client model loading + baking headlessly against
// client.jar's assets and dumps the BAKED QUADS per blockstate:
//   - blockstate JSON parse  : BlockStateModelDispatcher.CODEC (variants/multipart)
//   - model JSON parse       : CuboidModel.fromStream (Gson deserializers)
//   - parent resolution      : ModelDiscovery / ResolvedModel
//   - geometry baking        : UnbakedCuboidGeometry / FaceBakery / QuadCollection
//
// The only stub is the texture atlas: a MaterialBaker handing out IDENTITY
// sprites (one full-bleed 16x16 opaque sprite per material, u0=0,u1=1,v0=0,
// v1=1) so that TextureAtlasSprite.getU/getV are the identity mapping and the
// baked UVs come out in sprite-local space. Geometry (positions, winding,
// rotations, uv-lock, cullfaces, tint/shade/emission) is untouched vanilla
// code. The opaque image short-circuits SpriteContents.computeTransparency so
// no pixel data influences anything.
//
// Output (TSV, floats as raw IEEE-754 bits in hex):
//   S  <blockId>  <prop=value,...>          one per block state
//   P  <segIdx>   <optIdx>  <weight>  <ao>  one per baked model part
//   Q  <bucket>   <dir>  <tint>  <shade>  <emission>  <sprite>
//   V  <xbits>  <ybits>  <zbits>  <ubits>  <vbits>    4 per quad
//
// "seg" enumerates the BlockStateModels contributing to a state (variants: a
// single seg; multipart: one per matched selector, in selector order). "opt"
// enumerates weighted options within a seg (JSON order). Buckets follow
// QuadCollection.getAll() order: unculled, north, south, east, west, up, down.
//
// Usage: BlockModelParity <client.jar> [out.tsv]

import com.google.gson.JsonElement;
import com.google.gson.JsonParseException;
import com.mojang.serialization.JsonOps;
import net.minecraft.client.renderer.block.dispatch.BlockModelRotation;
import net.minecraft.client.renderer.block.dispatch.BlockStateModel;
import net.minecraft.client.renderer.block.dispatch.BlockStateModelDispatcher;
import net.minecraft.client.renderer.block.dispatch.BlockStateModelPart;
import net.minecraft.client.renderer.block.dispatch.SingleVariant;
import net.minecraft.client.renderer.block.dispatch.WeightedVariants;
import net.minecraft.client.renderer.texture.TextureAtlas;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import net.minecraft.client.renderer.texture.SpriteContents;
import net.minecraft.client.resources.metadata.animation.FrameSize;
import net.minecraft.client.resources.model.ModelBaker;
import net.minecraft.client.resources.model.ModelDebugName;
import net.minecraft.client.resources.model.ModelDiscovery;
import net.minecraft.client.resources.model.ResolvedModel;
import net.minecraft.client.resources.model.SimpleModelWrapper;
import net.minecraft.client.resources.model.UnbakedModel;
import net.minecraft.client.resources.model.cuboid.CuboidModel;
import net.minecraft.client.resources.model.cuboid.MissingCuboidModel;
import net.minecraft.client.resources.model.geometry.BakedQuad;
import net.minecraft.client.resources.model.sprite.Material;
import net.minecraft.client.resources.model.sprite.MaterialBaker;
import net.minecraft.client.model.geom.builders.UVPair;
import net.minecraft.core.Direction;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.util.StrictJsonParser;
import net.minecraft.util.random.Weighted;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.Property;
import org.joml.Vector3fc;

import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class BlockModelParity {

    // ── battery ──────────────────────────────────────────────────────────────
    static final String[] BATTERY = {
        "stone", "dirt", "grass_block",            // full cubes (+tint, weighted variants)
        "oak_slab",                                 // half blocks, top/bottom/double
        "oak_stairs",                               // x/y rotations + uvlock
        "oak_fence",                                // multipart, AND-less conditions
        "cobblestone_wall",                         // multipart with OR'd values
        "glass_pane",                               // multipart cutout
        "short_grass", "seagrass", "kelp",          // cross / tinted cross
        "oak_leaves",                               // cutout cube with tint
        "farmland",                                 // partial-height cube
        "snow",                                     // layered heights
        "water", "lava",                            // fluid specials: no model geometry
        "torch", "wall_torch",                      // small elements + rotation
        "oak_door",                                 // tall multi-state
        "oak_trapdoor",                             // rotated half-slab states
    };

    // ── identity sprites ─────────────────────────────────────────────────────
    static final class IdentitySprite extends TextureAtlasSprite {
        IdentitySprite(SpriteContents contents) {
            super(TextureAtlas.LOCATION_BLOCKS, contents, contents.width(), contents.height(), 0, 0, 0);
        }
    }

    static final class IdentityMaterials implements MaterialBaker {
        private final com.mojang.blaze3d.platform.NativeImage image;
        private final Map<Identifier, Material.Baked> cache = new HashMap<>();

        IdentityMaterials() {
            this.image = new com.mojang.blaze3d.platform.NativeImage(16, 16, true);
            this.image.fillRect(0, 0, 16, 16, 0xFFFFFFFF); // opaque => transparency short-circuit
        }

        private Material.Baked sprite(Identifier name, boolean forceTranslucent) {
            Material.Baked plain = this.cache.computeIfAbsent(name,
                n -> new Material.Baked(new IdentitySprite(new SpriteContents(n, new FrameSize(16, 16), this.image)), false));
            return forceTranslucent ? new Material.Baked(plain.sprite(), true) : plain;
        }

        @Override
        public Material.Baked get(Material material, ModelDebugName name) {
            return sprite(material.sprite(), material.forceTranslucent());
        }

        @Override
        public Material.Baked reportMissingReference(String reference, ModelDebugName name) {
            return sprite(Identifier.withDefaultNamespace("missingno"), false);
        }
    }

    // ── headless ModelBaker ──────────────────────────────────────────────────
    static final class Baker implements ModelBaker {
        final Map<Identifier, ResolvedModel> models;
        final ResolvedModel missing;
        final MaterialBaker materials = new IdentityMaterials();
        final Map<SharedOperationKey<?>, Object> opCache = new ConcurrentHashMap<>();
        BlockStateModelPart missingPart;

        Baker(Map<Identifier, ResolvedModel> models, ResolvedModel missing) {
            this.models = models;
            this.missing = missing;
        }

        @Override
        public ResolvedModel getModel(Identifier location) {
            ResolvedModel m = this.models.get(location);
            return m != null ? m : this.missing;
        }

        @Override
        public BlockStateModelPart missingBlockModelPart() {
            if (this.missingPart == null) {
                this.missingPart = SimpleModelWrapper.bake(this, MissingCuboidModel.LOCATION, BlockModelRotation.IDENTITY);
            }
            return this.missingPart;
        }

        @Override
        public MaterialBaker materials() {
            return this.materials;
        }

        @Override
        public Interner interner() {
            return new Interner() { // pass-through: interning only dedups references
                @Override public Vector3fc vector(Vector3fc v) { return v; }
                @Override public BakedQuad.MaterialInfo materialInfo(BakedQuad.MaterialInfo m) { return m; }
            };
        }

        @SuppressWarnings("unchecked")
        @Override
        public <T> T compute(SharedOperationKey<T> key) {
            return (T) this.opCache.computeIfAbsent(key, k -> key.compute(this));
        }
    }

    // ── reflection helpers (structural, deterministic part enumeration) ──────
    static Object getField(Object o, Class<?> cls, String name) throws Exception {
        Field f = cls.getDeclaredField(name);
        f.setAccessible(true);
        return f.get(o);
    }

    /** One weighted option inside a seg. */
    record Opt(int weight, BlockStateModelPart part) {}

    @SuppressWarnings("unchecked")
    static List<Opt> flattenSeg(BlockStateModel model) throws Exception {
        List<Opt> out = new ArrayList<>();
        if (model instanceof SingleVariant sv) {
            out.add(new Opt(1, (BlockStateModelPart) getField(sv, SingleVariant.class, "model")));
        } else if (model instanceof WeightedVariants wv) {
            var list = (net.minecraft.util.random.WeightedList<BlockStateModel>) getField(wv, WeightedVariants.class, "list");
            for (Weighted<BlockStateModel> w : list.unwrap()) {
                List<Opt> sub = flattenSeg(w.value()); // entries are SingleVariants per codec
                for (Opt opt : sub) out.add(new Opt(w.weight(), opt.part()));
            }
        } else {
            throw new IllegalStateException("unexpected BlockStateModel: " + model.getClass());
        }
        return out;
    }

    @SuppressWarnings("unchecked")
    static List<BlockStateModel> segments(BlockStateModel model, BlockState state) throws Exception {
        Class<?> multiPart = Class.forName("net.minecraft.client.renderer.block.dispatch.multipart.MultiPartModel");
        if (multiPart.isInstance(model)) {
            Object shared = getField(model, multiPart, "shared");
            Method select = shared.getClass().getDeclaredMethod("selectModels", BlockState.class);
            select.setAccessible(true);
            return (List<BlockStateModel>) select.invoke(shared, state);
        }
        return List.of(model);
    }

    // ── dump ─────────────────────────────────────────────────────────────────
    static final Direction[] BUCKETS = { null, Direction.NORTH, Direction.SOUTH, Direction.EAST, Direction.WEST, Direction.UP, Direction.DOWN };
    static final String[] BUCKET_NAMES = { "-", "north", "south", "east", "west", "up", "down" };

    static String hex(float f) {
        return Integer.toHexString(Float.floatToRawIntBits(f));
    }

    @SuppressWarnings("unchecked")
    static <T extends Comparable<T>> String propString(BlockState state) {
        StringBuilder sb = new StringBuilder();
        for (Property<?> p : state.getProperties()) {
            if (sb.length() > 0) sb.append(',');
            Property<T> pt = (Property<T>) p;
            sb.append(pt.getName()).append('=').append(pt.getName(state.getValue(pt)));
        }
        return sb.length() == 0 ? "-" : sb.toString();
    }

    public static void main(String[] args) throws Exception {
        String jarPath = args.length > 0 ? args[0] : "26.1.2/client.jar";
        String outPath = args.length > 1 ? args[1] : "mcpp/build/block_model_cases.tsv";

        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // 1. load every model JSON from the jar with the real Gson deserializer
        Map<Identifier, UnbakedModel> unbaked = new HashMap<>();
        Map<String, JsonElement> blockstateJson = new HashMap<>();
        try (ZipFile zip = new ZipFile(jarPath)) {
            Enumeration<? extends ZipEntry> entries = zip.entries();
            while (entries.hasMoreElements()) {
                ZipEntry e = entries.nextElement();
                String n = e.getName();
                if (n.startsWith("assets/minecraft/models/") && n.endsWith(".json")) {
                    String path = n.substring("assets/minecraft/models/".length(), n.length() - ".json".length());
                    try (var reader = new InputStreamReader(zip.getInputStream(e), StandardCharsets.UTF_8)) {
                        unbaked.put(Identifier.withDefaultNamespace(path), CuboidModel.fromStream(reader));
                    }
                } else if (n.startsWith("assets/minecraft/blockstates/") && n.endsWith(".json")) {
                    String name = n.substring("assets/minecraft/blockstates/".length(), n.length() - ".json".length());
                    try (var reader = new InputStreamReader(zip.getInputStream(e), StandardCharsets.UTF_8)) {
                        blockstateJson.put(name, StrictJsonParser.parse(reader));
                    }
                }
            }
        }
        System.err.println("models loaded: " + unbaked.size() + ", blockstates: " + blockstateJson.size());

        // 2. instantiate the battery blockstate definitions (real codec + selectors)
        Map<BlockState, BlockStateModel.UnbakedRoot> roots = new IdentityLinkedMap();
        List<BlockState> battery = new ArrayList<>();
        for (String name : BATTERY) {
            JsonElement json = blockstateJson.get(name);
            if (json == null) throw new IllegalStateException("missing blockstate json: " + name);
            Block block = BuiltInRegistries.BLOCK.getValue(Identifier.withDefaultNamespace(name));
            StateDefinition<Block, BlockState> def = block.getStateDefinition();
            BlockStateModelDispatcher dispatcher =
                BlockStateModelDispatcher.CODEC.parse(JsonOps.INSTANCE, json).getOrThrow(JsonParseException::new);
            Map<BlockState, BlockStateModel.UnbakedRoot> byState = dispatcher.instantiate(def, () -> name);
            roots.putAll(byState);
            battery.addAll(def.getPossibleStates());
        }

        // 3. resolve model parents (real ModelDiscovery)
        ModelDiscovery discovery = new ModelDiscovery(unbaked, MissingCuboidModel.missingModel());
        for (BlockStateModel.UnbakedRoot root : roots.values()) discovery.addRoot(root);
        Map<Identifier, ResolvedModel> resolved = discovery.resolve();
        Baker baker = new Baker(resolved, discovery.missingModel());

        // 4. bake + dump
        int states = 0, quads = 0;
        try (PrintWriter out = new PrintWriter(outPath, StandardCharsets.UTF_8)) {
            for (BlockState state : battery) {
                BlockStateModel.UnbakedRoot root = roots.get(state);
                if (root == null) throw new IllegalStateException("no model root for " + state);
                BlockStateModel model = root.bake(state, baker);
                out.println("S\t" + BuiltInRegistries.BLOCK.getKey(state.getBlock()) + "\t" + propString(state));
                states++;
                List<BlockStateModel> segs = segments(model, state);
                for (int si = 0; si < segs.size(); si++) {
                    List<Opt> opts = flattenSeg(segs.get(si));
                    for (int oi = 0; oi < opts.size(); oi++) {
                        Opt opt = opts.get(oi);
                        BlockStateModelPart part = opt.part();
                        out.println("P\t" + si + "\t" + oi + "\t" + opt.weight() + "\t" + (part.useAmbientOcclusion() ? 1 : 0));
                        for (int b = 0; b < BUCKETS.length; b++) {
                            for (BakedQuad q : part.getQuads(BUCKETS[b])) {
                                out.println("Q\t" + BUCKET_NAMES[b]
                                    + "\t" + q.direction().getSerializedName()
                                    + "\t" + q.materialInfo().tintIndex()
                                    + "\t" + (q.materialInfo().shade() ? 1 : 0)
                                    + "\t" + q.materialInfo().lightEmission()
                                    + "\t" + q.materialInfo().sprite().contents().name());
                                quads++;
                                for (int v = 0; v < 4; v++) {
                                    Vector3fc pos = q.position(v);
                                    long uv = q.packedUV(v);
                                    out.println("V\t" + hex(pos.x()) + "\t" + hex(pos.y()) + "\t" + hex(pos.z())
                                        + "\t" + hex(UVPair.unpackU(uv)) + "\t" + hex(UVPair.unpackV(uv)));
                                }
                            }
                        }
                    }
                }
            }
        }
        System.err.println("dumped " + states + " states, " + quads + " quads -> " + outPath);
    }

    /** Identity-keyed map preserving insertion (BlockState instances are singletons). */
    static final class IdentityLinkedMap extends LinkedHashMap<BlockState, BlockStateModel.UnbakedRoot> {
    }
}
