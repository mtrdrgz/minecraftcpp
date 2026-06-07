// Ground-truth generator for biome DECORATION features (the blocks each feature
// places), isolated from base-terrain FP divergence.
//
// For each (seed, chunk) it builds the vanilla pre-decoration chunk (terrain +
// surface), emits it as PRE rows, then runs the REAL vanilla PlacedFeature
// pipeline for a chosen placed_feature over a dynamic-Proxy WorldGenLevel backed
// by that chunk, and emits every block the feature writes as PUT rows.
//
// The C++ BiomeDecorationParityTest loads PRE into a LevelChunk and runs the
// ported feature with the identical decoration seed, then compares PUT. Terrain
// comes from PRE, so any mismatch is purely a feature/placement port bug.
//
//   tools/run_groundtruth.sh BiomeDecorationParity mcpp/build/decoration_<feature>.tsv <feature> <biome>
//
// Rows:
//   PRE  seed chunkX chunkZ x y z block_id
//   PUT  seed chunkX chunkZ feature x y z block_id

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import net.minecraft.tags.TagKey;
import net.minecraft.tags.TagLoader;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.world.level.block.Block;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;
import com.mojang.serialization.Lifecycle;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.RegistrationInfo;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.resources.ResourceKey;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.NoiseColumn;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.PalettedContainerFactory;
import net.minecraft.world.level.chunk.ProtoChunk;
import net.minecraft.world.level.chunk.UpgradeData;
import net.minecraft.world.level.levelgen.Aquifer;
import net.minecraft.world.level.levelgen.Beardifier;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator;
import net.minecraft.world.level.levelgen.NoiseChunk;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;
import net.minecraft.world.level.levelgen.blending.Blender;
import net.minecraft.world.level.levelgen.feature.ConfiguredFeature;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;

public class BiomeDecorationParity {
    static Registry<Biome> copyBiomeRegistry(HolderLookup.RegistryLookup<Biome> source) {
        MappedRegistry<Biome> registry = new MappedRegistry<>(Registries.BIOME, Lifecycle.stable());
        source.listElements().forEach(h -> registry.register(h.key(), h.value(), RegistrationInfo.BUILT_IN));
        return registry.freeze();
    }

    static Aquifer.FluidPicker fluidPicker(NoiseGeneratorSettings settings) {
        Aquifer.FluidStatus lava = new Aquifer.FluidStatus(-54, Blocks.LAVA.defaultBlockState());
        int sea = settings.seaLevel();
        Aquifer.FluidStatus seaS = new Aquifer.FluidStatus(sea, settings.defaultFluid());
        return (x, y, z) -> y < Math.min(-54, sea) ? lava : seaS;
    }

    static String tagIdFromPath(Path root, Path file) {
        String rel = root.relativize(file).toString().replace('\\', '/');
        return "minecraft:" + rel.substring(0, rel.length() - ".json".length());
    }
    static String entryId(JsonElement e) {
        if (e.isJsonPrimitive()) return e.getAsString();
        JsonObject o = e.getAsJsonObject();
        if (o.has("id")) return o.get("id").getAsString();
        throw new IllegalArgumentException("bad tag entry: " + e);
    }
    static void resolveBlockTag(String id, Map<String,List<String>> raw,
            Map<String,List<Holder<Block>>> resolved, LinkedHashSet<String> resolving) {
        if (resolved.containsKey(id)) return;
        if (!resolving.add(id)) throw new IllegalStateException("cyclic tag " + id);
        LinkedHashSet<Holder<Block>> vals = new LinkedHashSet<>();
        for (String entry : raw.getOrDefault(id, List.of())) {
            if (entry.startsWith("#")) { String n=entry.substring(1); resolveBlockTag(n,raw,resolved,resolving); vals.addAll(resolved.getOrDefault(n,List.of())); }
            else vals.add(BuiltInRegistries.BLOCK.get(ResourceKey.create(Registries.BLOCK, Identifier.parse(entry)))
                .orElseThrow(() -> new IllegalStateException("unknown block " + entry)));
        }
        resolving.remove(id);
        resolved.put(id, List.copyOf(vals));
    }
    static void bindVanillaBlockTags() throws Exception {
        Path root = Path.of("26.1.2","data","minecraft","tags","block");
        Map<String,List<String>> raw = new HashMap<>();
        try (var st = Files.walk(root)) {
            for (Path f : st.filter(p -> p.toString().endsWith(".json")).toList()) {
                JsonObject o = JsonParser.parseString(Files.readString(f)).getAsJsonObject();
                JsonArray values = o.getAsJsonArray("values");
                List<String> entries = new ArrayList<>();
                if (values != null) for (JsonElement v : values) entries.add(entryId(v));
                raw.put(tagIdFromPath(root,f), entries);
            }
        }
        Map<String,List<Holder<Block>>> resolved = new HashMap<>();
        for (String id : raw.keySet()) resolveBlockTag(id, raw, resolved, new LinkedHashSet<>());
        Map<TagKey<Block>,List<Holder<Block>>> pending = new HashMap<>();
        for (var e : resolved.entrySet()) pending.put(TagKey.create(Registries.BLOCK, Identifier.parse(e.getKey())), e.getValue());
        BuiltInRegistries.BLOCK.prepareTagReload(new TagLoader.LoadResult<>(Registries.BLOCK, pending)).apply();
    }

    public static void main(String[] args) throws Exception {
        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        bindVanillaBlockTags();

        String featureId = args.length > 0 ? args[0] : "minecraft:patch_grass_plain";
        String biomeId = args.length > 1 ? args[1] : "minecraft:plains";

        HolderLookup.Provider provider = VanillaRegistries.createLookup();
        Registry<Biome> biomeRegistry = copyBiomeRegistry(provider.lookupOrThrow(Registries.BIOME));
        RegistryAccess registries = new RegistryAccess.ImmutableRegistryAccess(List.of(biomeRegistry)).freeze();

        Holder<NoiseGeneratorSettings> settingsHolder =
            provider.lookupOrThrow(Registries.NOISE_SETTINGS).getOrThrow(NoiseGeneratorSettings.OVERWORLD);
        NoiseGeneratorSettings settings = settingsHolder.value();
        Holder<MultiNoiseBiomeSourceParameterList> preset =
            provider.lookupOrThrow(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST)
                .getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD);
        BiomeSource biomeSource = MultiNoiseBiomeSource.createFromPreset(preset);
        final NoiseBasedChunkGenerator generator = new NoiseBasedChunkGenerator(biomeSource, settingsHolder);
        LevelHeightAccessor height = LevelHeightAccessor.create(settings.noiseSettings().minY(), settings.noiseSettings().height());
        final int minY = height.getMinY();
        final int maxY = minY + height.getHeight();

        HolderLookup.RegistryLookup<PlacedFeature> placedFeatures = provider.lookupOrThrow(Registries.PLACED_FEATURE);
        // Tree mode: "tree:minecraft:oak" wraps the configured feature in a synthetic
        // surface placement [count(10), in_square, heightmap OCEAN_FLOOR] and places it
        // directly (no biome filter), so the tree feature can be certified in isolation.
        final boolean treeMode = featureId.startsWith("tree:");
        final PlacedFeature placed;
        if (treeMode) {
            String cfgId = featureId.substring("tree:".length());
            Holder<net.minecraft.world.level.levelgen.feature.ConfiguredFeature<?, ?>> cf =
                provider.lookupOrThrow(Registries.CONFIGURED_FEATURE)
                    .getOrThrow(ResourceKey.create(Registries.CONFIGURED_FEATURE, Identifier.parse(cfgId)));
            placed = new PlacedFeature(cf, List.of(
                net.minecraft.world.level.levelgen.placement.CountPlacement.of(10),
                net.minecraft.world.level.levelgen.placement.InSquarePlacement.spread(),
                net.minecraft.world.level.levelgen.placement.HeightmapPlacement.onHeightmap(Heightmap.Types.OCEAN_FLOOR)));
        } else {
            placed = placedFeatures.getOrThrow(
                ResourceKey.create(Registries.PLACED_FEATURE, Identifier.parse(featureId))).value();
        }
        final Holder<Biome> forcedBiome = provider.lookupOrThrow(Registries.BIOME)
            .getOrThrow(ResourceKey.create(Registries.BIOME, Identifier.parse(biomeId)));

        long[] seeds = { 0L, 123456789L };
        final int WANT = 16; // land chunks to keep per seed (output or not)

        for (long seed : seeds) {
            RandomState randomState = RandomState.create(provider, NoiseGeneratorSettings.OVERWORLD, seed);
            int kept = 0;
            outer:
            for (int cx = 0; cx < 40 && kept < WANT; cx++)
            for (int cz = 0; cz < 40 && kept < WANT; cz++) {
                ProtoChunk chunk = new ProtoChunk(new ChunkPos(cx, cz), UpgradeData.EMPTY, height,
                    PalettedContainerFactory.create(registries), null);
                BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
                for (int lx = 0; lx < 16; lx++) for (int lz = 0; lz < 16; lz++) {
                    int bx = cx * 16 + lx, bz = cz * 16 + lz;
                    NoiseColumn col = generator.getBaseColumn(bx, bz, height, randomState);
                    for (int y = minY; y < maxY; y++) {
                        BlockState s = col.getBlock(y);
                        if (!s.is(Blocks.AIR)) chunk.setBlockState(pos.set(bx, y, bz), s, 0);
                    }
                }
                Heightmap.primeHeightmaps(chunk, EnumSet.of(Heightmap.Types.WORLD_SURFACE_WG, Heightmap.Types.OCEAN_FLOOR_WG));
                NoiseChunk nc = chunk.getOrCreateNoiseChunk(c -> NoiseChunk.forChunk(c, randomState, Beardifier.EMPTY,
                    settings, fluidPicker(settings), Blender.empty()));
                generator.buildSurface(chunk, new WorldGenerationContext(generator, height), randomState, null,
                    new net.minecraft.world.level.biome.BiomeManager((qx, qy, qz) -> forcedBiome,
                        net.minecraft.world.level.biome.BiomeManager.obfuscateSeed(seed)),
                    biomeRegistry, Blender.empty());
                Heightmap.primeHeightmaps(chunk, EnumSet.of(Heightmap.Types.WORLD_SURFACE_WG,
                    Heightmap.Types.OCEAN_FLOOR_WG, Heightmap.Types.MOTION_BLOCKING));

                // Snapshot pre-decoration state (buffer PRE; only emitted if PUT>0).
                java.util.HashMap<Long, BlockState> before = new java.util.HashMap<>();
                StringBuilder preBuf = new StringBuilder();
                for (int lx = 0; lx < 16; lx++) for (int lz = 0; lz < 16; lz++) {
                    int bx = cx * 16 + lx, bz = cz * 16 + lz;
                    for (int y = minY; y < maxY; y++) {
                        BlockState s = chunk.getBlockState(pos.set(bx, y, bz));
                        if (!s.is(Blocks.AIR)) {
                            before.put(BlockPos.asLong(bx, y, bz), s);
                            preBuf.append("PRE\t").append(seed).append('\t').append(cx).append('\t').append(cz)
                                .append('\t').append(bx).append('\t').append(y).append('\t').append(bz)
                                .append('\t').append(blockId(s)).append('\n');
                        }
                    }
                }

                WorldGenLevel level = makeProxyLevel(chunk, height, registries, forcedBiome, minY, maxY, cx, cz);
                if (System.getenv("DECO_DEBUG") != null) {
                    int hx = cx*16+4, hz = cz*16+4;
                    int wswg = chunk.getHeight(Heightmap.Types.WORLD_SURFACE_WG, hx, hz);
                    System.err.println("DBG seed="+seed+" chunk="+cx+","+cz+" WSWG("+hx+","+hz+")="+wswg
                        +" top="+blockId(chunk.getBlockState(new BlockPos(hx, wswg-1, hz)))
                        +" at="+blockId(chunk.getBlockState(new BlockPos(hx, wswg, hz))));
                }
                WorldgenRandom random = new WorldgenRandom(new XoroshiroRandomSource(seed));
                long deco = random.setDecorationSeed(seed, cx * 16, cz * 16);
                random.setFeatureSeed(deco, 0, 0);
                boolean ok = treeMode
                    ? placed.place(level, generator, random, new BlockPos(cx * 16, 0, cz * 16))
                    : placed.placeWithBiomeCheck(level, generator, random, new BlockPos(cx * 16, 0, cz * 16));
                if (System.getenv("DECO_DEBUG") != null) System.err.println("DBG place ok="+ok);

                // Collect PUT (changed/added blocks).
                StringBuilder putBuf = new StringBuilder();
                int putCount = 0;
                for (int lx = 0; lx < 16; lx++) for (int lz = 0; lz < 16; lz++) {
                    int bx = cx * 16 + lx, bz = cz * 16 + lz;
                    for (int y = minY; y < maxY; y++) {
                        BlockState s = chunk.getBlockState(pos.set(bx, y, bz));
                        long key = BlockPos.asLong(bx, y, bz);
                        BlockState old = before.get(key);
                        boolean changed = (old == null) ? !s.is(Blocks.AIR) : (old != s);
                        if (changed) { putBuf.append("PUT\t").append(seed).append('\t').append(cx).append('\t').append(cz)
                            .append('\t').append(featureId).append('\t').append(bx).append('\t').append(y).append('\t').append(bz)
                            .append('\t').append(blockId(s)).append('\n'); putCount++; }
                    }
                }
                out.print(preBuf); out.print(putBuf); kept++;
            }
        }
    }

    static String blockId(BlockState s) {
        return s.getBlock().builtInRegistryHolder().key().identifier().toString();
    }

    // Dynamic-Proxy WorldGenLevel backed by the ProtoChunk: implement the abstract
    // methods features touch; route interface default methods through invokeDefault;
    // throw (named) for anything unexpected so gaps are obvious.
    static WorldGenLevel makeProxyLevel(ProtoChunk chunk, LevelHeightAccessor height,
            RegistryAccess registries, Holder<Biome> forcedBiome, int minY, int maxY, int cx, int cz) {
        final net.minecraft.world.level.block.state.BlockState AIR = Blocks.AIR.defaultBlockState();
        InvocationHandler h = new InvocationHandler() {
            @Override public Object invoke(Object proxy, Method m, Object[] a) throws Throwable {
                String n = m.getName();
                switch (n) {
                    case "getBlockState": { BlockPos p=(BlockPos)a[0]; return (p.getX()>>4==cx && p.getZ()>>4==cz) ? chunk.getBlockState(p) : AIR; }
                    case "isStateAtPosition": { BlockPos p=(BlockPos)a[0]; BlockState st=(p.getX()>>4==cx && p.getZ()>>4==cz)?chunk.getBlockState(p):AIR; @SuppressWarnings("unchecked") java.util.function.Predicate<BlockState> pr=(java.util.function.Predicate<BlockState>)a[1]; return pr.test(st); }
                    case "isFluidAtPosition": { BlockPos p=(BlockPos)a[0]; BlockState st=(p.getX()>>4==cx && p.getZ()>>4==cz)?chunk.getBlockState(p):AIR; @SuppressWarnings("unchecked") java.util.function.Predicate<net.minecraft.world.level.material.FluidState> pr=(java.util.function.Predicate<net.minecraft.world.level.material.FluidState>)a[1]; return pr.test(st.getFluidState()); }
                    case "getFluidState": return chunk.getFluidState((BlockPos) a[0]);
                    case "setBlock": {
                        BlockPos p=(BlockPos)a[0];
                        if (p.getX()>>4==cx && p.getZ()>>4==cz) chunk.setBlockState(p, (BlockState) a[1], 0);
                        return true;
                    }
                    case "getChunk":
                        if (a.length >= 2 && a[0] instanceof Integer) return chunk; // (x,z) and variants
                        return chunk;
                    case "getHeight":
                        if (a != null && a.length == 3) return chunk.getHeight((Heightmap.Types) a[0], (Integer) a[1], (Integer) a[2]);
                        return height.getHeight();
                    case "getMinY": return height.getMinY();
                    case "getBiome": return forcedBiome;
                    case "registryAccess": return registries;
                    case "getRandom": return RandomSource.create();
                    case "ensureCanWrite": return true;
                    case "isOutsideBuildHeight":
                        if (a[0] instanceof BlockPos) { int y = ((BlockPos) a[0]).getY(); return y < minY || y >= maxY; }
                        { int y = (Integer) a[0]; return y < minY || y >= maxY; }
                    case "scheduleTick": case "neighborChanged": case "addFreshEntity":
                    case "playSound": case "levelEvent": case "gameEvent": case "markPosForPostprocessing":
                        return null;
                    case "toString": return "ProxyWorldGenLevel";
                    case "hashCode": return System.identityHashCode(proxy);
                    case "equals": return proxy == a[0];
                    default:
                        if (m.isDefault()) return InvocationHandler.invokeDefault(proxy, m, a);
                        throw new UnsupportedOperationException("WorldGenLevel." + n + "/" + (a == null ? 0 : a.length));
                }
            }
        };
        return (WorldGenLevel) Proxy.newProxyInstance(WorldGenLevel.class.getClassLoader(),
            new Class<?>[] { WorldGenLevel.class }, h);
    }
}
