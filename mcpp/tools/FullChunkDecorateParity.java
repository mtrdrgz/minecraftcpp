// Fixed-order full-chunk DECORATION ground truth (scheduler-independent).
//
// Generates a 5x5 block of vanilla chunks (terrain + surface + carvers, exactly
// as FullChunkParity), fills real noise biomes, then runs the REAL vanilla
// decoration pipeline (ChunkGenerator.applyBiomeDecoration, reimplemented
// verbatim minus the structure sub-loop, which is gated off by
// generate-structures=false) over the inner 3x3 in a CALLER-SPECIFIED chunk
// order, all writing into ONE shared multi-chunk WorldGenLevel that faithfully
// mirrors WorldGenRegion semantics: getHeight returns stored+1, and
// ensureCanWrite gates max(|dx|,|dz|) <= blockStateWriteRadius(=1) from the
// currently-decorating (center) chunk -- so cross-chunk feature spill lands
// exactly as in vanilla. Then it dumps EVERY block of center chunk C in the
// FullChunkParity TSV format (seed blockX blockZ y block_id), so it can be diffed
// directly against the no-structures server .mca dump (ServerChunkDump).
//
// Purpose: certify the C++ decoration port is 1:1 for ANY fixed order, decoupled
// from the server's chunk-generation scheduler -- and find the order under which
// this ground truth matches the server .mca byte-for-byte.
//
//   tools/run_groundtruth.ps1 -Tool FullChunkDecorateParity -Out mcpp/build/deco_gt.tsv -ToolArgs "1 0 0 xz"
//     args: <seed> <centerChunkX> <centerChunkZ> <order> [debug] [watch:x,y,z;...]
//       order in {xz, zx, perm:dx,dz;dx,dz;...}
//       xz = x outer / z inner over the inner 3x3 (ChunkGenerationTask.scheduleLayer
//            iteration order, ChunkGenerationTask.java:120-121; NOTE ForceLoadCommand /
//            ChunkPos.rangeClosed is z-outer/x-inner — but each generation TASK's layer
//            is xz, which is what decides decoration order)
//       zx = z outer / x inner (raster)
//       perm: = explicit turn order, deltas relative to the center, |delta| <= 2 (or 3 —
//            the build radius adapts). This is how the real server order is reproduced.
//
// SERVER DECORATION ORDER (seed 1, proven byte-exact on 6 forest + 6 ocean chunks):
//   The seed-1 world spawn is chunk (10,10) (level.dat spawn pos 160,70,160;
//   Climate.Sampler.findSpawnPosition -> block 163,0,170). World CREATION
//   (MinecraftServer.setInitialSpawn -> PlayerSpawnFinder.getSpawnPosInChunk ->
//   level.getChunk(10,10) at FULL) therefore decorates the spawn 3x3 (9..11)^2 FIRST
//   ("phase 1"), before any forceload: empirically in xz order EXCEPT (10,11) decorates
//   first: (10,11),(9,9),(9,10),(9,11),(10,9),(10,10),(11,9),(11,10),(11,11).
//   All later chunks ("phase 2", the forceload-rect promotion cascade) decorate AFTER
//   all of phase 1, in xz order among themselves. Chunks outside any spawn influence
//   (the 6 ocean GT chunks) are pure phase 2 = plain xz, which is why the legacy 3x3 xz
//   mode certified them. Residual: 2 cells in (9,11) (see session notes) from
//   second-order phase-2 interleaving inside the uncertified neighbour turns (8,11)/(9,12).
import com.mojang.serialization.Lifecycle;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import java.nio.file.Files;
import java.nio.file.Path;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.HolderSet;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.QuartPos;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.RegistrationInfo;
import net.minecraft.core.SectionPos;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.tags.TagLoader;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.NoiseColumn;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.WorldOptions;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureStart;
import java.util.function.Predicate;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeManager;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.biome.FeatureSorter;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.LevelChunkSection;
import net.minecraft.world.level.chunk.PalettedContainerFactory;
import net.minecraft.world.level.chunk.ProtoChunk;
import net.minecraft.world.level.chunk.UpgradeData;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.levelgen.Aquifer;
import net.minecraft.world.level.levelgen.Beardifier;
import net.minecraft.world.level.levelgen.GenerationStep;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator;
import net.minecraft.world.level.levelgen.NoiseChunk;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.RandomSupport;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;
import net.minecraft.world.level.levelgen.blending.Blender;
import net.minecraft.world.level.levelgen.carver.CarvingContext;
import net.minecraft.world.level.levelgen.carver.ConfiguredWorldCarver;
import net.minecraft.world.level.levelgen.feature.ConfiguredFeature;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;

public class FullChunkDecorateParity {
    // Real stderr, captured at class load BEFORE Bootstrap.bootStrap() wraps System.err
    // into the Mojang logger (which would re-route debug lines onto stdout as INFO).
    static final java.io.PrintStream ERR = System.err;
    // ----- shared state set up in main(), read by decorate()/the proxy -----
    static Map<Long, ProtoChunk> CHUNKS;
    static int CUR_CX, CUR_CZ;            // currently-decorating (center) chunk
    static NoiseBasedChunkGenerator GEN;
    static BiomeSource BIOMES;
    static WorldGenLevel LEVEL;
    static List<FeatureSorter.StepFeatureData> FEATURE_LIST;
    static long SEED;
    static int MIN_SECTION_Y;
    static int DBG_CX, DBG_CZ;            // center chunk to debug-log features for
    static boolean DEBUG = false;
    static Map<PlacedFeature, String> PF_ID;
    static String CUR_FEATURE_ID = "?";   // feature currently placing (for setBlock attribution)
    static int CUR_STEP = -1;
    static boolean POSTPROCESSING = false; // FULL-promotion writes hit a LevelChunk: no re-marking
    // WorldGenRegion.random (WorldGenRegion.java:77,86): a DETERMINISTIC positional random,
    // created per region (= per decorated chunk per step) at the center chunk's world position:
    //   level.getChunkSource().randomState().getOrCreateRandomFactory(
    //       Identifier.withDefaultNamespace("worldgen_region_random"))
    //     .at(center.getPos().getWorldPosition())            // = (minBlockX, 0, minBlockZ)
    // getRandom() (WorldGenRegion.java:386-388) returns that instance. decorate() re-creates it
    // when the decorating chunk switches, exactly like the server building a fresh WorldGenRegion
    // for each chunk's FEATURES step.
    static net.minecraft.world.level.levelgen.PositionalRandomFactory REGION_RANDOM_FACTORY;
    static RandomSource REGION_RANDOM;
    // forensic watch list: poll these cells after every placed feature (catches raw
    // section writes that bypass setBlock, e.g. OreFeature via BulkSectionAccess)
    static List<BlockPos> WATCH = new ArrayList<>();
    static BlockState[] WATCH_LAST;

    static void watchPoll(String where) {
        for (int i = 0; i < WATCH.size(); i++) {
            BlockPos p = WATCH.get(i);
            ProtoChunk c = chunkAt(p.getX() >> 4, p.getZ() >> 4);
            BlockState bs = c == null ? null : c.getBlockState(p);
            if (bs != WATCH_LAST[i]) {
                ERR.println("CELLCHG\t" + where + "\t" + p.getX() + "," + p.getY() + "," + p.getZ()
                    + "\t" + (WATCH_LAST[i] == null ? "?" : blockId(WATCH_LAST[i])) + " -> " + (bs == null ? "?" : blockId(bs)));
                WATCH_LAST[i] = bs;
            }
        }
    }

    static long ckey(int cx, int cz) { return ChunkPos.pack(cx, cz); }
    static ProtoChunk chunkAt(int cx, int cz) { return CHUNKS.get(ckey(cx, cz)); }

    // ============================ setup (copied verbatim from FullChunkParity) ============================
    static String tagIdFromPath(Path root, Path file) {
        String rel = root.relativize(file).toString().replace('\\', '/');
        return "minecraft:" + rel.substring(0, rel.length() - ".json".length());
    }
    static String entryId(JsonElement element) {
        if (element.isJsonPrimitive()) return element.getAsString();
        JsonObject obj = element.getAsJsonObject();
        if (obj.has("id")) return obj.get("id").getAsString();
        throw new IllegalArgumentException("unsupported tag entry: " + element);
    }
    static void resolveBlockTag(String id, Map<String, List<String>> rawTags,
            Map<String, List<Holder<Block>>> resolved, LinkedHashSet<String> resolving) {
        if (resolved.containsKey(id)) return;
        if (!resolving.add(id)) throw new IllegalStateException("cyclic block tag: " + id);
        LinkedHashSet<Holder<Block>> values = new LinkedHashSet<>();
        for (String entry : rawTags.getOrDefault(id, List.of())) {
            if (entry.startsWith("#")) {
                String nested = entry.substring(1);
                resolveBlockTag(nested, rawTags, resolved, resolving);
                values.addAll(resolved.getOrDefault(nested, List.of()));
            } else {
                Holder<Block> holder = BuiltInRegistries.BLOCK.get(
                    ResourceKey.create(Registries.BLOCK, Identifier.parse(entry))
                ).orElseThrow(() -> new IllegalStateException("unknown block in tag " + id + ": " + entry));
                values.add(holder);
            }
        }
        resolving.remove(id);
        resolved.put(id, List.copyOf(values));
    }
    static void bindVanillaBlockTags() throws Exception {
        Path root = Path.of("26.1.2", "data", "minecraft", "tags", "block");
        Map<String, List<String>> rawTags = new HashMap<>();
        try (var stream = Files.walk(root)) {
            for (Path file : stream.filter(p -> p.toString().endsWith(".json")).toList()) {
                JsonObject obj = JsonParser.parseString(Files.readString(file)).getAsJsonObject();
                JsonArray values = obj.getAsJsonArray("values");
                List<String> entries = new ArrayList<>();
                for (JsonElement value : values) entries.add(entryId(value));
                rawTags.put(tagIdFromPath(root, file), entries);
            }
        }
        Map<String, List<Holder<Block>>> resolved = new HashMap<>();
        for (String id : rawTags.keySet()) resolveBlockTag(id, rawTags, resolved, new LinkedHashSet<>());
        Map<TagKey<Block>, List<Holder<Block>>> pending = new HashMap<>();
        for (Map.Entry<String, List<Holder<Block>>> e : resolved.entrySet())
            pending.put(TagKey.create(Registries.BLOCK, Identifier.parse(e.getKey())), e.getValue());
        BuiltInRegistries.BLOCK.prepareTagReload(new TagLoader.LoadResult<>(Registries.BLOCK, pending)).apply();
    }
    // Fluid tags must be bound too: block placement-survival checks consult them during
    // worldgen (e.g. TallSeagrassBlock.canSurvive requires fluidState.is(FluidTags.WATER);
    // unbound tags silently test false and tall seagrass never places).
    static void bindVanillaFluidTags() throws Exception {
        Path root = Path.of("26.1.2", "data", "minecraft", "tags", "fluid");
        Map<String, List<String>> rawTags = new HashMap<>();
        try (var stream = Files.walk(root)) {
            for (Path file : stream.filter(p -> p.toString().endsWith(".json")).toList()) {
                JsonObject obj = JsonParser.parseString(Files.readString(file)).getAsJsonObject();
                JsonArray values = obj.getAsJsonArray("values");
                List<String> entries = new ArrayList<>();
                for (JsonElement value : values) entries.add(entryId(value));
                rawTags.put(tagIdFromPath(root, file), entries);
            }
        }
        Map<String, List<Holder<net.minecraft.world.level.material.Fluid>>> resolved = new HashMap<>();
        for (String id : rawTags.keySet()) {
            LinkedHashSet<Holder<net.minecraft.world.level.material.Fluid>> values = new LinkedHashSet<>();
            java.util.ArrayDeque<String> work = new java.util.ArrayDeque<>(rawTags.get(id));
            java.util.Set<String> seenNested = new HashSet<>();
            while (!work.isEmpty()) {
                String entry = work.poll();
                if (entry.startsWith("#")) {
                    String nested = entry.substring(1);
                    if (seenNested.add(nested)) work.addAll(rawTags.getOrDefault(nested, List.of()));
                } else {
                    values.add(BuiltInRegistries.FLUID.get(ResourceKey.create(Registries.FLUID, Identifier.parse(entry)))
                        .orElseThrow(() -> new IllegalStateException("unknown fluid in tag " + id + ": " + entry)));
                }
            }
            resolved.put(id, List.copyOf(values));
        }
        Map<TagKey<net.minecraft.world.level.material.Fluid>, List<Holder<net.minecraft.world.level.material.Fluid>>> pending = new HashMap<>();
        for (var e : resolved.entrySet())
            pending.put(TagKey.create(Registries.FLUID, Identifier.parse(e.getKey())), e.getValue());
        BuiltInRegistries.FLUID.prepareTagReload(new TagLoader.LoadResult<>(Registries.FLUID, pending)).apply();
    }
    static Registry<Biome> copyBiomeRegistry(HolderLookup.RegistryLookup<Biome> source) {
        MappedRegistry<Biome> registry = new MappedRegistry<>(Registries.BIOME, Lifecycle.stable());
        source.listElements().forEach(holder -> registry.register(holder.key(), holder.value(), RegistrationInfo.BUILT_IN));
        return registry.freeze();
    }
    static Aquifer.FluidPicker fluidPicker(NoiseGeneratorSettings settings) {
        Aquifer.FluidStatus lavaStatus = new Aquifer.FluidStatus(-54, Blocks.LAVA.defaultBlockState());
        int seaLevel = settings.seaLevel();
        Aquifer.FluidStatus seaStatus = new Aquifer.FluidStatus(seaLevel, settings.defaultFluid());
        return (x, y, z) -> y < Math.min(-54, seaLevel) ? lavaStatus : seaStatus;
    }
    // A StructureManager that reports no structures (generate-structures=false), so the
    // Beardifier is EMPTY and no level access is needed during fillFromNoise.
    static StructureManager noStructuresManager() {
        return new StructureManager(null, new WorldOptions(SEED, false, false), null) {
            @Override public List<StructureStart> startsForStructure(ChunkPos pos, Predicate<Structure> matcher) { return List.of(); }
        };
    }
    static void applyCarvers(NoiseBasedChunkGenerator generator, BiomeSource biomeSource, RegistryAccess registries,
            LevelHeightAccessor height, NoiseGeneratorSettings settings, long seed, RandomState randomState,
            BiomeManager biomeManager, ProtoChunk chunk) {
        WorldgenRandom random = new WorldgenRandom(new LegacyRandomSource(RandomSupport.generateUniqueSeed()));
        BiomeManager correctBiomeManager = biomeManager.withDifferentSource(
            (quartX, quartY, quartZ) -> biomeSource.getNoiseBiome(quartX, quartY, quartZ, randomState.sampler()));
        NoiseChunk noiseChunk = chunk.getOrCreateNoiseChunk(c -> NoiseChunk.forChunk(c, randomState, Beardifier.EMPTY,
            settings, fluidPicker(settings), Blender.empty()));
        CarvingContext context = new CarvingContext(generator, registries, height, noiseChunk, randomState, settings.surfaceRule());
        Aquifer aquifer = noiseChunk.aquifer();
        ChunkPos center = chunk.getPos();
        for (int dx = -8; dx <= 8; dx++) {
            for (int dz = -8; dz <= 8; dz++) {
                ChunkPos sourcePos = new ChunkPos(center.x() + dx, center.z() + dz);
                Holder<Biome> sourceBiome = biomeSource.getNoiseBiome(
                    QuartPos.fromBlock(sourcePos.getMinBlockX()), 0, QuartPos.fromBlock(sourcePos.getMinBlockZ()), randomState.sampler());
                Iterable<Holder<ConfiguredWorldCarver<?>>> carvers = sourceBiome.value().getGenerationSettings().getCarvers();
                int index = 0;
                for (Holder<ConfiguredWorldCarver<?>> carverHolder : carvers) {
                    ConfiguredWorldCarver<?> carver = carverHolder.value();
                    random.setLargeFeatureSeed(seed + index, sourcePos.x(), sourcePos.z());
                    if (carver.isStartChunk(random))
                        carver.carve(context, chunk, correctBiomeManager::getBiome, random, aquifer, sourcePos, chunk.getOrCreateCarvingMask());
                    index++;
                }
            }
        }
    }

    static String blockId(BlockState s) { return s.getBlock().builtInRegistryHolder().key().identifier().toString(); }

    // ============================ main ============================
    public static void main(String[] args) throws Exception {
        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        bindVanillaBlockTags();
        bindVanillaFluidTags();

        SEED = args.length > 0 ? Long.parseLong(args[0]) : 1L;
        final int Cx = args.length > 1 ? Integer.parseInt(args[1]) : 0;
        final int Cz = args.length > 2 ? Integer.parseInt(args[2]) : 0;
        final String order = args.length > 3 ? args[3] : "xz";
        DEBUG = args.length > 4 && args[4].equalsIgnoreCase("debug");
        // optional 6th arg: watch:x,y,z;x,y,z;...  -> log every block change at these cells
        // (catches BulkSectionAccess writes, e.g. ores, that bypass WorldGenLevel.setBlock)
        if (args.length > 5 && args[5].startsWith("watch:")) {
            for (String c : args[5].substring(6).split(";")) {
                String[] p = c.split(",");
                WATCH.add(new BlockPos(Integer.parseInt(p[0]), Integer.parseInt(p[1]), Integer.parseInt(p[2])));
            }
            WATCH_LAST = new BlockState[WATCH.size()];
        }

        HolderLookup.Provider provider = VanillaRegistries.createLookup();
        Registry<Biome> biomeRegistry = copyBiomeRegistry(provider.lookupOrThrow(Registries.BIOME));
        RegistryAccess registries = new RegistryAccess.ImmutableRegistryAccess(List.of(biomeRegistry)).freeze();

        Holder<NoiseGeneratorSettings> settingsHolder =
            provider.lookupOrThrow(Registries.NOISE_SETTINGS).getOrThrow(NoiseGeneratorSettings.OVERWORLD);
        NoiseGeneratorSettings settings = settingsHolder.value();
        Holder<MultiNoiseBiomeSourceParameterList> overworldPreset =
            provider.lookupOrThrow(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST)
                .getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD);
        BIOMES = MultiNoiseBiomeSource.createFromPreset(overworldPreset);
        GEN = new NoiseBasedChunkGenerator(BIOMES, settingsHolder);
        LevelHeightAccessor height = LevelHeightAccessor.create(settings.noiseSettings().minY(), settings.noiseSettings().height());
        WorldGenerationContext surfaceContext = new WorldGenerationContext(GEN, height);
        final int minY = height.getMinY();
        final int maxY = minY + height.getHeight();
        MIN_SECTION_Y = SectionPos.blockToSectionCoord(minY);

        RandomState randomState = RandomState.create(provider, NoiseGeneratorSettings.OVERWORLD, SEED);

        // ---- build the chunk block: biomes, then terrain + surface + carvers ----
        // (sized so that "perm:" orders may decorate any chunk with |delta| <= decoRadius:
        // each decorated chunk needs its full ring-1 neighbourhood built at CARVERS, exactly
        // like the server, where the creation/forceload tasks decorate a wider area than
        // the 3x3 around the dumped chunk.)
        StructureManager noStructures = noStructuresManager();
        CHUNKS = new HashMap<>();
        // Chunk-cache-first BiomeManager, mirroring WorldGenRegion.getBiomeManager(): the
        // vanilla SURFACE step passes region.getBiomeManager() to the surface system
        // (NoiseBasedChunkGenerator.buildSurface:267-278), whose zoomed lookups resolve to
        // the CHUNK-CACHED noise biome (LevelReader.getNoiseBiome -> ChunkAccess).
        BiomeManager chunkCacheBiomeManager = new BiomeManager((qx, qy, qz) -> {
            ProtoChunk cc = chunkAt(QuartPos.toSection(qx), QuartPos.toSection(qz));
            return cc != null ? cc.getNoiseBiome(qx, qy, qz)
                              : BIOMES.getNoiseBiome(qx, qy, qz, randomState.sampler());
        }, BiomeManager.obfuscateSeed(SEED));
        // PASS A — the vanilla BIOMES step (ChunkStatusTasks.generateBiomes ->
        // NoiseBasedChunkGenerator.doCreateBiomes:93-97): fill the chunk's noise-biome
        // palette from the NoiseChunk's CACHED climate sampler (router noises wrapped
        // through the NoiseChunk caches, NoiseChunk.cachedClimateSampler:168-178), NOT the
        // raw randomState.sampler(): at climate boundaries (e.g. beach vs stony_shore) the
        // two disagree, flipping the surface material (sand/sandstone vs stone) and biome
        // gates. Radius 4 = one ring beyond the built terrain so zoomed biome lookups from
        // ring-3 chunks still resolve against a filled chunk, exactly as on the server
        // (SURFACE step depends on BIOMES at radius 1, ChunkPyramid).
        // Build radius adapts to the requested decoration set: every decorated chunk needs
        // its ring-1 terrain (CARVERS) built, and every built chunk needs ring-1 biomes.
        int decoRadius = 1;
        if (order.startsWith("perm:")) {
            for (String t : order.substring(5).split(";")) {
                String[] p = t.split(",");
                decoRadius = Math.max(decoRadius, Math.max(Math.abs(Integer.parseInt(p[0])), Math.abs(Integer.parseInt(p[1]))));
            }
        }
        final int terrainRadius = decoRadius + 1;
        final int biomeRadius = terrainRadius + 1;
        java.lang.reflect.Method cachedClimateSampler = NoiseChunk.class.getDeclaredMethod(
            "cachedClimateSampler", net.minecraft.world.level.levelgen.NoiseRouter.class, java.util.List.class);
        cachedClimateSampler.setAccessible(true);
        for (int dx = -biomeRadius; dx <= biomeRadius; dx++) for (int dz = -biomeRadius; dz <= biomeRadius; dz++) {
            int ncx = Cx + dx, ncz = Cz + dz;
            ProtoChunk chunk = new ProtoChunk(new ChunkPos(ncx, ncz), UpgradeData.EMPTY, height,
                PalettedContainerFactory.create(registries), null);
            // Same NoiseChunk the server's BIOMES step creates (createNoiseChunk:99-103,
            // structures off => Beardifier EMPTY); fillFromNoise/buildSurface/carvers all
            // reuse this exact instance via getOrCreateNoiseChunk, as on the server.
            NoiseChunk noiseChunk = chunk.getOrCreateNoiseChunk(c -> NoiseChunk.forChunk(c, randomState,
                Beardifier.EMPTY, settings, fluidPicker(settings), Blender.empty()));
            chunk.fillBiomesFromNoise(BIOMES, (net.minecraft.world.level.biome.Climate.Sampler)
                cachedClimateSampler.invoke(noiseChunk, randomState.router(), settings.spawnTarget()));
            chunk.setPersistedStatus(ChunkStatus.BIOMES);
            CHUNKS.put(ckey(ncx, ncz), chunk);
        }
        // PASS B — NOISE + SURFACE + CARVERS for the terrain square.
        for (int dx = -terrainRadius; dx <= terrainRadius; dx++) for (int dz = -terrainRadius; dz <= terrainRadius; dz++) {
            int ncx = Cx + dx, ncz = Cz + dz;
            ProtoChunk chunk = chunkAt(ncx, ncz);
            // Real NOISE step. fillFromNoise fills the whole chunk via the full-width (16)
            // NoiseChunk + aquifer and primes WORLD_SURFACE_WG/OCEAN_FLOOR_WG.
            GEN.fillFromNoise(Blender.empty(), randomState, noStructures, chunk).get();
            GEN.buildSurface(chunk, surfaceContext, randomState, null, chunkCacheBiomeManager, biomeRegistry, Blender.empty());
            applyCarvers(GEN, BIOMES, registries, height, settings, SEED, randomState, chunkCacheBiomeManager, chunk);
            // NOTE: the non-WG heightmaps are primed PER CHUNK at its own decoration turn
            // (inside decorate(), exactly as ChunkStatusTasks.generateFeatures does), NOT
            // here at build time.
            // At the FEATURES step the server's chunk has highestGeneratedStatus >= BIOMES (it
            // passed CREATE_BIOMES); ProtoChunk.getNoiseBiome guards on that. Match it so the
            // chunk-cache biome path used by the decoration BiomeManager works.
            chunk.setPersistedStatus(ChunkStatus.CARVERS);
        }

        // ---- shared multi-chunk WorldGenLevel (WorldGenRegion-faithful) ----
        // The proxy's getBiome must mirror WorldGenRegion exactly: its BiomeManager's
        // NoiseBiomeSource is the region itself -> LevelReader.getNoiseBiome -> the chunk's
        // CACHED noise biome, which CLAMPS quartY to the chunk's section range (ChunkAccess
        // .getNoiseBiome). The build-time biomeManager above calls BIOMES.getNoiseBiome
        // directly (unclamped) which is correct for buildSurface (chunks not yet biome-filled
        // there) but WRONG for decoration. Build a dedicated chunk-cache-first manager now
        // that all 5x5 chunks are filled (fillBiomesFromNoise above).
        BiomeManager decoBiomeManager = new BiomeManager((qx, qy, qz) -> {
            ProtoChunk cc = chunkAt(QuartPos.toSection(qx), QuartPos.toSection(qz));
            return cc != null ? cc.getNoiseBiome(qx, qy, qz)
                              : BIOMES.getNoiseBiome(qx, qy, qz, randomState.sampler());
        }, BiomeManager.obfuscateSeed(SEED));
        LEVEL = makeMultiProxy(height, registries, randomState, decoBiomeManager, minY, maxY);
        REGION_RANDOM_FACTORY = randomState.getOrCreateRandomFactory(Identifier.withDefaultNamespace("worldgen_region_random"));

        // ---- global feature ordering (identical to ChunkGenerator.featuresPerStep) ----
        FEATURE_LIST = FeatureSorter.buildFeaturesPerStep(
            List.copyOf(BIOMES.possibleBiomes()), b -> b.value().getGenerationSettings().features(), true);

        DBG_CX = Cx; DBG_CZ = Cz;
        if (DEBUG) {
            PF_ID = new HashMap<>();
            provider.lookupOrThrow(Registries.PLACED_FEATURE).listElements()
                .forEach(h -> PF_ID.put(h.value(), h.key().identifier().toString()));
        }

        // ---- decorate the inner 3x3 in the caller-specified order ----
        int[] ds = { -1, 0, 1 };
        if (order.startsWith("perm:")) {
            // explicit turn order: perm:dx,dz;dx,dz;... (deltas relative to center)
            for (String t : order.substring(5).split(";")) {
                String[] p = t.split(",");
                decorate(Cx + Integer.parseInt(p[0]), Cz + Integer.parseInt(p[1]));
            }
        } else if (order.equalsIgnoreCase("zx")) {
            for (int dz : ds) for (int dx : ds) decorate(Cx + dx, Cz + dz);
        } else { // "xz" default: x outer, z inner (ForceLoadCommand order)
            for (int dx : ds) for (int dz : ds) decorate(Cx + dx, Cz + dz);
        }

        // ---- FULL-promotion post-processing for the center chunk (LevelChunk.
        // postProcessGeneration 1:1): for each position marked during generation, the
        // server runs one fluid tick + a block tick for LiquidBlocks (whose only effect
        // is BubbleColumnBlock.updateColumn) or updateFromNeighbourShapes otherwise.
        // The fluid SPREAD tick needs a real ServerLevel; it is a logged hard no-op here
        // (count printed; zero observed block deltas from it on all certified chunks).
        postProcessChunk(chunkAt(Cx, Cz));

        // ---- dump center chunk C (FullChunkParity TSV order: lx outer, lz inner, y asc) ----
        ProtoChunk c = chunkAt(Cx, Cz);
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
        for (int lx = 0; lx < 16; lx++) {
            int bx = Cx * 16 + lx;
            for (int lz = 0; lz < 16; lz++) {
                int bz = Cz * 16 + lz;
                for (int y = minY; y < maxY; y++)
                    out.println(SEED + "\t" + bx + "\t" + bz + "\t" + y + "\t" + blockId(c.getBlockState(pos.set(bx, y, bz))));
            }
        }
    }

    // LevelChunk.postProcessGeneration (LevelChunk.java:566) mirrored for the worldgen
    // harness. Marked positions come from WorldGenRegion.setBlock's getPostProcessPos
    // marking + features'/carvers' explicit markPosForPostprocessing.
    static void postProcessChunk(ProtoChunk c) {
        CUR_CX = c.getPos().x(); CUR_CZ = c.getPos().z(); // allow updateColumn's setBlock writes
        POSTPROCESSING = true;
        it.unimi.dsi.fastutil.shorts.ShortList[] pp = c.getPostProcessing();
        if (pp == null) return;
        int skippedFluidTicks = 0;
        for (int sectionIndex = 0; sectionIndex < pp.length; sectionIndex++) {
            it.unimi.dsi.fastutil.shorts.ShortList list = pp[sectionIndex];
            if (list == null) continue;
            for (int i = 0; i < list.size(); i++) {
                BlockPos bp = ProtoChunk.unpackOffsetCoordinates(list.getShort(i), c.getSectionYFromSectionIndex(sectionIndex), c.getPos());
                BlockState bs = c.getBlockState(bp);
                net.minecraft.world.level.material.FluidState fs = bs.getFluidState();
                if (!fs.isEmpty() && !(bs.getBlock() instanceof net.minecraft.world.level.block.LiquidBlock)) skippedFluidTicks++; // waterlogged-style
                if (bs.getBlock() instanceof net.minecraft.world.level.block.LiquidBlock) {
                    // fluid spread tick (ServerLevel-typed) — logged hard no-op:
                    skippedFluidTicks++;
                    // LiquidBlock.tick == if shouldBubbleColumnOccupy(state) -> updateColumn
                    // (LiquidBlock.java:162-167, 191-193), both replicated exactly:
                    if (fs.is(net.minecraft.tags.FluidTags.BUBBLE_COLUMN_CAN_OCCUPY) && fs.isSource() && fs.isFull()) {
                        BlockState below = LEVEL.getBlockState(bp.below());
                        net.minecraft.world.level.block.BubbleColumnBlock.updateColumn(Blocks.BUBBLE_COLUMN, LEVEL, bp, below);
                    }
                } else {
                    BlockState ns = net.minecraft.world.level.block.Block.updateFromNeighbourShapes(bs, LEVEL, bp);
                    if (ns != bs) LEVEL.setBlock(bp, ns, 276);
                }
            }
        }
        POSTPROCESSING = false;
        if (skippedFluidTicks > 0)
            ERR.println("POSTPROCESS chunk=" + c.getPos() + " fluid-spread ticks skipped (no ServerLevel): " + skippedFluidTicks);
    }

    // ChunkGenerator.applyBiomeDecoration (26.1.2 ChunkGenerator.java:314) reimplemented
    // verbatim, minus the structure sub-loop (generate-structures=false => never runs;
    // feature seeds are independent of structure indices). The center chunk is (ncx,ncz).
    static void decorate(int ncx, int ncz) {
        CUR_CX = ncx; CUR_CZ = ncz;
        // Fresh per-region random for this chunk's FEATURES step (WorldGenRegion ctor, :86).
        REGION_RANDOM = REGION_RANDOM_FACTORY.at(new ChunkPos(ncx, ncz).getWorldPosition());
        if (DEBUG) ERR.println("TURN\t" + ncx + "," + ncz);
        // ChunkStatusTasks.generateFeatures: prime the non-WG heightmaps at the START of
        // this chunk's FEATURES step — the frozen snapshot must include spill already
        // written by earlier-decorated neighbours (e.g. tree canopy over the border).
        Heightmap.primeHeightmaps(chunkAt(ncx, ncz), EnumSet.of(Heightmap.Types.MOTION_BLOCKING,
            Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, Heightmap.Types.OCEAN_FLOOR, Heightmap.Types.WORLD_SURFACE));
        SectionPos sectionPos = SectionPos.of(new ChunkPos(ncx, ncz), MIN_SECTION_Y);
        BlockPos origin = sectionPos.origin();
        WorldgenRandom random = new WorldgenRandom(new XoroshiroRandomSource(RandomSupport.generateUniqueSeed()));
        long decorationSeed = random.setDecorationSeed(SEED, origin.getX(), origin.getZ());

        Set<Holder<Biome>> possibleBiomes = new HashSet<>();
        ChunkPos.rangeClosed(sectionPos.chunk(), 1).forEach(cp -> {
            ChunkAccess cir = chunkAt(cp.x(), cp.z());
            for (LevelChunkSection section : cir.getSections()) section.getBiomes().getAll(possibleBiomes::add);
        });
        possibleBiomes.retainAll(BIOMES.possibleBiomes());

        if (DEBUG && ncx == DBG_CX && ncz == DBG_CZ) {
            TreeSet<String> ids = new TreeSet<>();
            for (Holder<Biome> b : possibleBiomes) ids.add(b.unwrapKey().map(k -> k.identifier().toString()).orElse("?"));
            ERR.println("DBG possibleBiomes(" + ncx + "," + ncz + ") n=" + ids.size() + " " + ids);
        }

        int featureStepCount = FEATURE_LIST.size();
        int generationSteps = Math.max(GenerationStep.Decoration.values().length, featureStepCount);
        for (int stepIndex = 0; stepIndex < generationSteps; stepIndex++) {
            if (stepIndex < featureStepCount) {
                Set<Integer> possibleFeaturesThisStep = new TreeSet<>();
                for (Holder<Biome> biome : possibleBiomes) {
                    List<HolderSet<PlacedFeature>> featuresInBiome = biome.value().getGenerationSettings().features();
                    if (stepIndex < featuresInBiome.size()) {
                        HolderSet<PlacedFeature> featuresInBiomeThisStep = featuresInBiome.get(stepIndex);
                        FeatureSorter.StepFeatureData sfd = FEATURE_LIST.get(stepIndex);
                        featuresInBiomeThisStep.stream().map(Holder::value)
                            .forEach(f -> possibleFeaturesThisStep.add(sfd.indexMapping().applyAsInt(f)));
                    }
                }
                int[] indexArray = new int[possibleFeaturesThisStep.size()];
                int k = 0; for (int gi : possibleFeaturesThisStep) indexArray[k++] = gi;
                Arrays.sort(indexArray); // TreeSet already sorted; keep verbatim with vanilla
                FeatureSorter.StepFeatureData sfd = FEATURE_LIST.get(stepIndex);
                for (int globalIndexOfFeature : indexArray) {
                    PlacedFeature feature = sfd.features().get(globalIndexOfFeature);
                    random.setFeatureSeed(decorationSeed, globalIndexOfFeature, stepIndex);
                    if (DEBUG) { CUR_STEP = stepIndex; CUR_FEATURE_ID = PF_ID.getOrDefault(feature, "?#" + globalIndexOfFeature); }
                    if (DEBUG && CUR_FEATURE_ID.contains("lake_lava")) {
                        WorldgenRandom r2 = new WorldgenRandom(new XoroshiroRandomSource(1L));
                        r2.setFeatureSeed(decorationSeed, globalIndexOfFeature, stepIndex);
                        net.minecraft.world.level.levelgen.placement.PlacementContext pctx =
                            new net.minecraft.world.level.levelgen.placement.PlacementContext(LEVEL, GEN, java.util.Optional.of(feature));
                        java.util.List<BlockPos> cur = new java.util.ArrayList<>(java.util.List.of(origin));
                        String stages = "";
                        for (net.minecraft.world.level.levelgen.placement.PlacementModifier mod : feature.placement()) {
                            java.util.List<BlockPos> next = new java.util.ArrayList<>();
                            for (BlockPos p : cur) mod.getPositions(pctx, r2, p).forEach(next::add);
                            stages += " " + mod.getClass().getSimpleName() + "=" + next.size()
                                + (next.isEmpty() ? "" : "@" + next.get(0).getX() + "," + next.get(0).getY() + "," + next.get(0).getZ());
                            cur = next;
                            if (cur.isEmpty()) break;
                        }
                        ERR.println("LAKEPROBE chunk=(" + ncx + "," + ncz + ") " + CUR_FEATURE_ID
                            + " final=" + (cur.isEmpty() ? "REJECT" : cur.get(0).toString()) + " |" + stages);
                    }
                    boolean ok = feature.placeWithBiomeCheck(LEVEL, GEN, random, origin);
                    if (!WATCH.isEmpty()) watchPoll("turn=" + ncx + "," + ncz + " step=" + stepIndex + " gi=" + globalIndexOfFeature
                        + " " + (PF_ID != null ? PF_ID.getOrDefault(feature, "?") : "?"));
                    if (DEBUG && ncx == DBG_CX && ncz == DBG_CZ) {
                        String id = PF_ID.getOrDefault(feature, "?#" + globalIndexOfFeature);
                        if (id.contains("lake") || id.contains("seagrass") || id.contains("spring") || id.contains("kelp"))
                            ERR.println("DBG step=" + stepIndex + " gi=" + globalIndexOfFeature + " ok=" + ok + " " + id);
                    }
                }
            }
        }
    }

    // Dynamic-Proxy WorldGenLevel over the 5x5 chunk map, mirroring WorldGenRegion.
    static WorldGenLevel makeMultiProxy(LevelHeightAccessor height, RegistryAccess registries,
            RandomState randomState, BiomeManager biomeManager, int minY, int maxY) {
        final BlockState AIR = Blocks.AIR.defaultBlockState();
        InvocationHandler h = new InvocationHandler() {
            @Override public Object invoke(Object proxy, Method m, Object[] a) throws Throwable {
                String n = m.getName();
                switch (n) {
                    case "getBlockState": { BlockPos p = (BlockPos) a[0]; ProtoChunk c = chunkAt(p.getX() >> 4, p.getZ() >> 4); BlockState bs = c != null ? c.getBlockState(p) : AIR;
                        if (DEBUG && CUR_CX==DBG_CX && CUR_CZ==DBG_CZ && CUR_FEATURE_ID.equals("minecraft:seagrass_deep")) ERR.println("SGB " + p.getX() + "," + p.getY() + "," + p.getZ() + "=" + bs.getBlock().builtInRegistryHolder().key().identifier());
                        return bs; }
                    case "getFluidState": { BlockPos p = (BlockPos) a[0]; ProtoChunk c = chunkAt(p.getX() >> 4, p.getZ() >> 4); return c != null ? c.getFluidState(p) : AIR.getFluidState(); }
                    case "isStateAtPosition": { BlockPos p = (BlockPos) a[0]; ProtoChunk c = chunkAt(p.getX() >> 4, p.getZ() >> 4); BlockState st = c != null ? c.getBlockState(p) : AIR; @SuppressWarnings("unchecked") java.util.function.Predicate<BlockState> pr = (java.util.function.Predicate<BlockState>) a[1]; return pr.test(st); }
                    case "isFluidAtPosition": { BlockPos p = (BlockPos) a[0]; ProtoChunk c = chunkAt(p.getX() >> 4, p.getZ() >> 4); BlockState st = c != null ? c.getBlockState(p) : AIR; @SuppressWarnings("unchecked") java.util.function.Predicate<net.minecraft.world.level.material.FluidState> pr = (java.util.function.Predicate<net.minecraft.world.level.material.FluidState>) a[1]; return pr.test(st.getFluidState()); }
                    case "setBlock": {
                        BlockPos p = (BlockPos) a[0];
                        if (!ensureCanWrite(p)) return false;
                        ProtoChunk c = chunkAt(p.getX() >> 4, p.getZ() >> 4);
                        if (c == null) return false;
                        if (DEBUG && (p.getX() >> 4) == DBG_CX && (p.getZ() >> 4) == DBG_CZ)
                            ERR.println("PUT\t" + CUR_CX + "," + CUR_CZ + "\t" + CUR_STEP + "\t" + CUR_FEATURE_ID + "\t" + p.getX() + "\t" + p.getY() + "\t" + p.getZ()
                                + "\t" + ((BlockState) a[1]).getBlock().builtInRegistryHolder().key().identifier());
                        BlockState st = (BlockState) a[1];
                        int flags = (a.length > 2 && a[2] instanceof Integer) ? (Integer) a[2] : 0;
                        c.setBlockState(p, st, flags);
                        // WorldGenRegion.setBlock block-entity handling (ProtoChunk branch):
                        // record a DUMMY nbt so getBlockEntity can lazily create the real one.
                        if (st.hasBlockEntity()) {
                            net.minecraft.nbt.CompoundTag tag = new net.minecraft.nbt.CompoundTag();
                            tag.putInt("x", p.getX()); tag.putInt("y", p.getY()); tag.putInt("z", p.getZ());
                            tag.putString("id", "DUMMY");
                            c.setBlockEntityNbt(tag);
                        }
                        // WorldGenRegion.setBlock: unless flag 16, ask the state for a
                        // post-process pos and mark it (drives postProcessGeneration at FULL,
                        // e.g. magma marking the water above for bubble-column formation).
                        if ((flags & 16) == 0 && !POSTPROCESSING) {
                            BlockPos pp = st.getPostProcessPos((net.minecraft.world.level.BlockGetter) proxy, p);
                            if (pp != null) {
                                ProtoChunk pc = chunkAt(pp.getX() >> 4, pp.getZ() >> 4);
                                if (pc != null) pc.markPosForPostprocessing(pp);
                            }
                        }
                        return true;
                    }
                    case "getChunk": {
                        if (a.length >= 2 && a[0] instanceof Integer && a[1] instanceof Integer)
                            return chunkAt((Integer) a[0], (Integer) a[1]);
                        if (a.length >= 1 && a[0] instanceof ChunkPos) { ChunkPos cp = (ChunkPos) a[0]; return chunkAt(cp.x(), cp.z()); }
                        if (a.length >= 1 && a[0] instanceof BlockPos) { BlockPos p = (BlockPos) a[0]; return chunkAt(p.getX() >> 4, p.getZ() >> 4); }
                        throw new UnsupportedOperationException("getChunk/" + (a == null ? 0 : a.length));
                    }
                    case "getHeight":
                        if (a != null && a.length == 3) {
                            int x = (Integer) a[1], z = (Integer) a[2];
                            int hv = chunkAt(x >> 4, z >> 4).getHeight((Heightmap.Types) a[0], x & 15, z & 15) + 1; // WorldGenRegion +1
                            if (DEBUG && CUR_CX==DBG_CX && CUR_CZ==DBG_CZ && CUR_FEATURE_ID.equals("minecraft:seagrass_deep")) ERR.println("SGH " + a[0] + " " + x + "," + z + "=" + hv);
                            return hv;
                        }
                        return height.getHeight();
                    case "ensureCanWrite": return ensureCanWrite((BlockPos) a[0]);
                    // WorldGenRegion.getBlockEntity verbatim: lazily realize the DUMMY-tagged
                    // block entity so e.g. RandomizableContainer.setBlockEntityLootTable finds
                    // the container and consumes its random.nextLong() exactly like the server.
                    case "getBlockEntity": {
                        BlockPos p = (BlockPos) a[0];
                        ProtoChunk c = chunkAt(p.getX() >> 4, p.getZ() >> 4);
                        if (c == null) return null;
                        net.minecraft.world.level.block.entity.BlockEntity be = c.getBlockEntity(p);
                        if (be != null) return be;
                        net.minecraft.nbt.CompoundTag tag = c.getBlockEntityNbt(p);
                        BlockState state = c.getBlockState(p);
                        if (tag != null) {
                            if ("DUMMY".equals(tag.getStringOr("id", ""))) {
                                if (!state.hasBlockEntity()) return null;
                                be = ((net.minecraft.world.level.block.EntityBlock) state.getBlock()).newBlockEntity(p, state);
                            } else {
                                be = net.minecraft.world.level.block.entity.BlockEntity.loadStatic(p, state, tag, registries);
                            }
                            if (be != null) { c.setBlockEntity(be); return be; }
                        }
                        return null;
                    }
                    case "getMinY": return minY;
                    case "getMaxY": return maxY - 1;
                    case "getMinSectionY": return MIN_SECTION_Y;
                    case "getSeaLevel": return GEN.getSeaLevel();
                    // INITIALIZE_LIGHT runs AFTER the FEATURES step, so during decoration the
                    // light engine holds no data and every brightness query returns 0.
                    case "getBrightness": return 0;
                    case "getRawBrightness": return 0;
                    case "getMaxLocalRawBrightness": return 0;
                    case "getSeed": return SEED;
                    case "getBiomeManager": return biomeManager;
                    case "getBiome": return biomeManager.getBiome((BlockPos) a[0]);
                    case "getNoiseBiome": { int qx = (Integer) a[0], qy = (Integer) a[1], qz = (Integer) a[2]; ProtoChunk c = chunkAt(QuartPos.toSection(qx), QuartPos.toSection(qz)); return c != null ? c.getNoiseBiome(qx, qy, qz) : BIOMES.getNoiseBiome(qx, qy, qz, randomState.sampler()); }
                    case "getUncachedNoiseBiome": return BIOMES.getNoiseBiome((Integer) a[0], (Integer) a[1], (Integer) a[2], randomState.sampler());
                    case "registryAccess": return registries;
                    case "getRandom": return REGION_RANDOM; // WorldGenRegion.java:386-388
                    case "setCurrentlyGenerating": return null;
                    case "isOutsideBuildHeight":
                        if (a[0] instanceof BlockPos) { int y = ((BlockPos) a[0]).getY(); return y < minY || y >= maxY; }
                        { int y = (Integer) a[0]; return y < minY || y >= maxY; }
                    case "scheduleTick": case "neighborChanged": case "addFreshEntity":
                    case "playSound": case "levelEvent": case "gameEvent": case "markPosForPostprocessing":
                    case "blockUpdated": case "neighborShapeChanged": case "updateNeighborsAt":
                        return null;
                    case "toString": return "MultiChunkWorldGenLevel";
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

    // WorldGenRegion.ensureCanWrite: Chebyshev distance from the center (currently-
    // decorating) chunk must be <= blockStateWriteRadius (FEATURES = 1).
    static boolean ensureCanWrite(BlockPos pos) {
        int chunkX = SectionPos.blockToSectionCoord(pos.getX());
        int chunkZ = SectionPos.blockToSectionCoord(pos.getZ());
        return Math.abs(CUR_CX - chunkX) <= 1 && Math.abs(CUR_CZ - chunkZ) <= 1;
    }
}
