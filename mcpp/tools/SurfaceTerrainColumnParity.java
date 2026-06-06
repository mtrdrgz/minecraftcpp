// Ground-truth generator for vanilla overworld terrain columns after
// buildSurface, but before carvers/structures/features/decoration.
//
// It fills a ProtoChunk from the real getBaseColumn() result, installs a real
// NoiseChunk with Beardifier.EMPTY, then calls the @VisibleForTesting
// NoiseBasedChunkGenerator.buildSurface(...) overload.
//
//   tools/run_groundtruth.ps1 -Tool SurfaceTerrainColumnParity -Out mcpp/build/surface_terrain_columns.tsv
//
// Each row: seed  blockX  blockZ  y  block_id
import java.util.EnumSet;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.RegistrationInfo;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import com.mojang.serialization.Lifecycle;
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
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.PalettedContainerFactory;
import net.minecraft.world.level.chunk.ProtoChunk;
import net.minecraft.world.level.chunk.UpgradeData;
import net.minecraft.world.level.dimension.DimensionType;
import net.minecraft.world.level.levelgen.Aquifer;
import net.minecraft.world.level.levelgen.Beardifier;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator;
import net.minecraft.world.level.levelgen.NoiseChunk;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
import net.minecraft.world.level.levelgen.blending.Blender;

public class SurfaceTerrainColumnParity {
    static Registry<Biome> copyBiomeRegistry(HolderLookup.RegistryLookup<Biome> source) {
        MappedRegistry<Biome> registry = new MappedRegistry<>(Registries.BIOME, Lifecycle.stable());
        source.listElements().forEach(holder -> registry.register(holder.key(), holder.value(), RegistrationInfo.BUILT_IN));
        return registry.freeze();
    }

    static Aquifer.FluidPicker fluidPicker(NoiseGeneratorSettings settings) {
        Aquifer.FluidStatus lavaStatus = new Aquifer.FluidStatus(-54, Blocks.LAVA.defaultBlockState());
        int seaLevel = settings.seaLevel();
        Aquifer.FluidStatus seaStatus = new Aquifer.FluidStatus(seaLevel, settings.defaultFluid());
        Aquifer.FluidStatus emptyStatus = new Aquifer.FluidStatus(DimensionType.MIN_Y * 2, Blocks.AIR.defaultBlockState());
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

    public static void main(String[] args) {
        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
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
        WorldGenerationContext context = new WorldGenerationContext(generator, height);

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
                    generator.buildSurface(chunk, context, randomState, null, biomeManager, biomeRegistry, Blender.empty());
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
