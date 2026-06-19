// Ground-truth generator for vanilla overworld terrain columns after
// buildSurface + applyCarvers, but before structures/features/decoration.
//
// This calls the real configured world carvers from the 26.1.2 Java code over
// the same 17x17 source-chunk area used by NoiseBasedChunkGenerator.applyCarvers.
//
//   tools/run_groundtruth.ps1 -Tool CarvedTerrainColumnParity -Out mcpp/build/carved_terrain_columns.tsv
//
// Each row: seed  blockX  blockZ  y  block_id
import com.mojang.serialization.Lifecycle;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.QuartPos;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.RegistrationInfo;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.tags.TagLoader;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.NoiseColumn;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeManager;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.PalettedContainerFactory;
import net.minecraft.world.level.chunk.ProtoChunk;
import net.minecraft.world.level.chunk.UpgradeData;
import net.minecraft.world.level.dimension.DimensionType;
import net.minecraft.world.level.levelgen.Aquifer;
import net.minecraft.world.level.levelgen.Beardifier;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator;
import net.minecraft.world.level.levelgen.NoiseChunk;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.RandomSupport;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.blending.Blender;
import net.minecraft.world.level.levelgen.carver.CarvingContext;
import net.minecraft.world.level.levelgen.carver.ConfiguredWorldCarver;

public class CarvedTerrainColumnParity {
    static String tagIdFromPath(Path root, Path file) {
        String rel = root.relativize(file).toString().replace('\\', '/');
        return "minecraft:" + rel.substring(0, rel.length() - ".json".length());
    }

    static String entryId(JsonElement element) {
        if (element.isJsonPrimitive()) {
            return element.getAsString();
        }
        JsonObject obj = element.getAsJsonObject();
        if (obj.has("id")) {
            return obj.get("id").getAsString();
        }
        throw new IllegalArgumentException("unsupported tag entry: " + element);
    }

    static void resolveBlockTag(
        String id,
        Map<String, List<String>> rawTags,
        Map<String, List<Holder<Block>>> resolved,
        LinkedHashSet<String> resolving
    ) {
        if (resolved.containsKey(id)) {
            return;
        }
        if (!resolving.add(id)) {
            throw new IllegalStateException("cyclic block tag: " + id);
        }

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
                for (JsonElement value : values) {
                    entries.add(entryId(value));
                }
                rawTags.put(tagIdFromPath(root, file), entries);
            }
        }

        Map<String, List<Holder<Block>>> resolved = new HashMap<>();
        for (String id : rawTags.keySet()) {
            resolveBlockTag(id, rawTags, resolved, new LinkedHashSet<>());
        }

        Map<TagKey<Block>, List<Holder<Block>>> pending = new HashMap<>();
        for (Map.Entry<String, List<Holder<Block>>> e : resolved.entrySet()) {
            pending.put(TagKey.create(Registries.BLOCK, Identifier.parse(e.getKey())), e.getValue());
        }
        BuiltInRegistries.BLOCK.prepareTagReload(new TagLoader.LoadResult<>(Registries.BLOCK, pending)).apply();
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

    static ProtoChunk makeChunk(
        NoiseBasedChunkGenerator generator,
        RandomState randomState,
        NoiseGeneratorSettings settings,
        RegistryAccess registries,
        LevelHeightAccessor height,
        int chunkX,
        int chunkZ
    ) {
        ProtoChunk chunk = new ProtoChunk(
            new ChunkPos(chunkX, chunkZ),
            UpgradeData.EMPTY,
            height,
            PalettedContainerFactory.create(registries),
            null
        );
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
        for (int lx = 0; lx < 16; lx++) {
            int blockX = chunkX * 16 + lx;
            for (int lz = 0; lz < 16; lz++) {
                int blockZ = chunkZ * 16 + lz;
                NoiseColumn column = generator.getBaseColumn(blockX, blockZ, height, randomState);
                for (int y = height.getMinY(); y < height.getMinY() + height.getHeight(); y++) {
                    BlockState state = column.getBlock(y);
                    if (!state.is(Blocks.AIR)) {
                        chunk.setBlockState(pos.set(blockX, y, blockZ), state, 0);
                    }
                }
            }
        }
        Heightmap.primeHeightmaps(chunk, EnumSet.of(Heightmap.Types.WORLD_SURFACE_WG, Heightmap.Types.OCEAN_FLOOR_WG));
        chunk.getOrCreateNoiseChunk(c -> NoiseChunk.forChunk(
            c,
            randomState,
            Beardifier.EMPTY,
            settings,
            fluidPicker(settings),
            Blender.empty()
        ));
        return chunk;
    }

    static void applyCarvers(
        NoiseBasedChunkGenerator generator,
        BiomeSource biomeSource,
        RegistryAccess registries,
        LevelHeightAccessor height,
        NoiseGeneratorSettings settings,
        long seed,
        RandomState randomState,
        BiomeManager biomeManager,
        ProtoChunk chunk
    ) {
        WorldgenRandom random = new WorldgenRandom(new LegacyRandomSource(RandomSupport.generateUniqueSeed()));
        BiomeManager correctBiomeManager = biomeManager.withDifferentSource(
            (quartX, quartY, quartZ) -> biomeSource.getNoiseBiome(quartX, quartY, quartZ, randomState.sampler())
        );
        NoiseChunk noiseChunk = chunk.getOrCreateNoiseChunk(c -> NoiseChunk.forChunk(
            c,
            randomState,
            Beardifier.EMPTY,
            settings,
            fluidPicker(settings),
            Blender.empty()
        ));
        CarvingContext context = new CarvingContext(generator, registries, height, noiseChunk, randomState, settings.surfaceRule());
        Aquifer aquifer = noiseChunk.aquifer();
        ChunkPos center = chunk.getPos();

        for (int dx = -8; dx <= 8; dx++) {
            for (int dz = -8; dz <= 8; dz++) {
                ChunkPos sourcePos = new ChunkPos(center.x() + dx, center.z() + dz);
                Holder<Biome> sourceBiome = biomeSource.getNoiseBiome(
                    QuartPos.fromBlock(sourcePos.getMinBlockX()),
                    0,
                    QuartPos.fromBlock(sourcePos.getMinBlockZ()),
                    randomState.sampler()
                );
                Iterable<Holder<ConfiguredWorldCarver<?>>> carvers =
                    sourceBiome.value().getGenerationSettings().getCarvers();
                int index = 0;
                String onlyEnv = System.getenv("MCPP_CARVER_ONLY");
                int onlyIdx = onlyEnv == null ? -1 : Integer.parseInt(onlyEnv.trim());
                for (Holder<ConfiguredWorldCarver<?>> carverHolder : carvers) {
                    ConfiguredWorldCarver<?> carver = carverHolder.value();
                    random.setLargeFeatureSeed(seed + index, sourcePos.x(), sourcePos.z());
                    boolean start = carver.isStartChunk(random);
                    if (start && (onlyIdx < 0 || index == onlyIdx)) {
                        carver.carve(context, chunk, correctBiomeManager::getBiome, random, aquifer, sourcePos, chunk.getOrCreateCarvingMask());
                    }
                    index++;
                }
            }
        }
    }

    public static void main(String[] args) {
        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        try {
            bindVanillaBlockTags();
        } catch (Exception e) {
            throw new RuntimeException("failed to bind vanilla block tags", e);
        }
        HolderLookup.Provider provider = VanillaRegistries.createLookup();
        Registry<Biome> biomeRegistry = copyBiomeRegistry(provider.lookupOrThrow(Registries.BIOME));
        RegistryAccess registries = new RegistryAccess.ImmutableRegistryAccess(java.util.List.of(biomeRegistry)).freeze();

        Holder<NoiseGeneratorSettings> settingsHolder =
            provider.lookupOrThrow(Registries.NOISE_SETTINGS).getOrThrow(NoiseGeneratorSettings.OVERWORLD);
        NoiseGeneratorSettings settings = settingsHolder.value();
        Holder<MultiNoiseBiomeSourceParameterList> overworldPreset =
            provider.lookupOrThrow(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST)
                .getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD);
        BiomeSource biomeSource = MultiNoiseBiomeSource.createFromPreset(overworldPreset);
        NoiseBasedChunkGenerator generator = new NoiseBasedChunkGenerator(biomeSource, settingsHolder);
        LevelHeightAccessor height = LevelHeightAccessor.create(settings.noiseSettings().minY(), settings.noiseSettings().height());
        WorldGenerationContext surfaceContext = new WorldGenerationContext(generator, height);

        long[] seeds = { 0L, 1L, 123456789L, -987654321L };
        int[][] columns = {
            {0, 0}, {1, 1}, {15, 15}, {16, 16}, {-1, -1}, {-16, 31},
            {100, -200}, {-50, 300}, {1000, 1000}, {-1234, 5678},
            {37, 37}, {8, -8}, {-300, -300}, {2048, -2048}
        };

        for (long seed : seeds) {
            RandomState randomState = RandomState.create(provider, NoiseGeneratorSettings.OVERWORLD, seed);
            BiomeManager biomeManager = new BiomeManager(
                (quartX, quartY, quartZ) -> biomeSource.getNoiseBiome(quartX, quartY, quartZ, randomState.sampler()),
                BiomeManager.obfuscateSeed(seed)
            );
            java.util.HashMap<Long, ProtoChunk> chunks = new java.util.HashMap<>();
            for (int[] c : columns) {
                int chunkX = Math.floorDiv(c[0], 16);
                int chunkZ = Math.floorDiv(c[1], 16);
                long key = ChunkPos.pack(chunkX, chunkZ);
                ProtoChunk chunk = chunks.get(key);
                if (chunk == null) {
                    chunk = makeChunk(generator, randomState, settings, registries, height, chunkX, chunkZ);
                    generator.buildSurface(chunk, surfaceContext, randomState, null, biomeManager, biomeRegistry, Blender.empty());
                    applyCarvers(generator, biomeSource, registries, height, settings, seed, randomState, biomeManager, chunk);
                    String dumpFile = System.getenv("MCPP_DUMP_FILE");
                    if (dumpFile != null && seed == 1L && chunkX == 0 && chunkZ == 0) {
                        try (java.io.PrintStream dp = new java.io.PrintStream(new java.io.FileOutputStream(dumpFile))) {
                            BlockPos.MutableBlockPos dpos = new BlockPos.MutableBlockPos();
                            for (int lx = 0; lx < 16; lx++) {
                                for (int lz = 0; lz < 16; lz++) {
                                    for (int yy = height.getMinY(); yy < height.getMinY() + height.getHeight(); yy++) {
                                        String b = chunk.getBlockState(dpos.set(lx, yy, lz)).getBlock().builtInRegistryHolder().key().identifier().toString();
                                        if (b.equals("minecraft:air") || b.equals("minecraft:water") || b.equals("minecraft:lava")) {
                                            dp.println(lx + "\t" + yy + "\t" + lz + "\t" + b);
                                        }
                                    }
                                }
                            }
                        } catch (Exception e) { throw new RuntimeException(e); }
                    }
                    chunks.put(key, chunk);
                }
                BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
                for (int y = height.getMinY(); y < height.getMinY() + height.getHeight(); y++) {
                    String block = chunk.getBlockState(pos.set(c[0], y, c[1]))
                        .getBlock().builtInRegistryHolder().key().identifier().toString();
                    out.println(seed + "\t" + c[0] + "\t" + c[1] + "\t" + y + "\t" + block);
                }
            }
        }
    }
}
