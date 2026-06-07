// Ground-truth generator for per-biome OVERWORLD surface rules, ISOLATED from the
// base-terrain/aquifer stage.
//
// The base terrain column (pre-surface: stone / water / air) is biome-independent
// and is emitted once per (seed, column) as BASE rows. For each biome it then
// forces that biome over that exact base terrain and runs the real vanilla
// SurfaceSystem.buildSurface, emitting the resulting column as SURF rows.
//
// The C++ BiomeSurfaceParityTest loads the BASE column into a chunk (so terrain is
// byte-identical to vanilla, independent of any C++ fillFromNoise/aquifer FP
// divergence) and runs the ported SurfaceSystem with the same forced biome. Any
// mismatch is therefore purely a surface-rule port bug. This certifies the
// biome-dependent surface (sand/terracotta bands/snow/gravel/mud/...) one biome
// at a time.
//
//   tools/run_groundtruth.sh BiomeSurfaceParity mcpp/build/biome_surface.tsv
//
// Rows:
//   BASE  seed  blockX  blockZ  y  block_id
//   SURF  biome seed  blockX  blockZ  y  block_id

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
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
import net.minecraft.world.level.levelgen.Aquifer;
import net.minecraft.world.level.levelgen.Beardifier;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator;
import net.minecraft.world.level.levelgen.NoiseChunk;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
import net.minecraft.world.level.levelgen.blending.Blender;

public class BiomeSurfaceParity {
    static Registry<Biome> copyBiomeRegistry(HolderLookup.RegistryLookup<Biome> source) {
        MappedRegistry<Biome> registry = new MappedRegistry<>(Registries.BIOME, Lifecycle.stable());
        source.listElements().forEach(h -> registry.register(h.key(), h.value(), RegistrationInfo.BUILT_IN));
        return registry.freeze();
    }

    static Aquifer.FluidPicker fluidPicker(NoiseGeneratorSettings settings) {
        Aquifer.FluidStatus lavaStatus = new Aquifer.FluidStatus(-54, Blocks.LAVA.defaultBlockState());
        int seaLevel = settings.seaLevel();
        Aquifer.FluidStatus seaStatus = new Aquifer.FluidStatus(seaLevel, settings.defaultFluid());
        return (x, y, z) -> y < Math.min(-54, seaLevel) ? lavaStatus : seaStatus;
    }

    public static void main(String[] args) {
        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        HolderLookup.Provider provider = VanillaRegistries.createLookup();
        Registry<Biome> biomeRegistry = copyBiomeRegistry(provider.lookupOrThrow(Registries.BIOME));
        RegistryAccess registries = new RegistryAccess.ImmutableRegistryAccess(List.of(biomeRegistry)).freeze();

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
        final int minY = height.getMinY();
        final int maxY = minY + height.getHeight();

        List<Holder<Biome>> forced = new ArrayList<>(biomeSource.possibleBiomes());
        forced.sort((a, b) -> a.unwrapKey().orElseThrow().identifier().toString()
            .compareTo(b.unwrapKey().orElseThrow().identifier().toString()));

        long[] seeds = { 0L, 123456789L };
        int[][] columns = { {0, 0}, {15, 15}, {16, 16}, {-1, -1}, {100, -200}, {1000, 1000} };

        for (long seed : seeds) {
            RandomState randomState = RandomState.create(provider, NoiseGeneratorSettings.OVERWORLD, seed);

            // 1) Build the biome-independent base terrain for each test column once.
            Map<Long, BlockState[]> baseColumns = new HashMap<>();
            for (int[] c : columns) {
                NoiseColumn col = generator.getBaseColumn(c[0], c[1], height, randomState);
                BlockState[] arr = new BlockState[maxY - minY];
                for (int y = minY; y < maxY; y++) {
                    arr[y - minY] = col.getBlock(y);
                    out.println("BASE\t" + seed + "\t" + c[0] + "\t" + c[1] + "\t" + y + "\t"
                        + col.getBlock(y).getBlock().builtInRegistryHolder().key().identifier());
                }
                baseColumns.put(ChunkPos.pack(c[0], c[1]), arr);
            }

            // 2) For each biome, rebuild fresh chunks from the cached base terrain,
            //    force the biome, run buildSurface, emit the result.
            for (Holder<Biome> biomeHolder : forced) {
                String biomeId = biomeHolder.unwrapKey().orElseThrow().identifier().toString();
                BiomeManager.NoiseBiomeSource constant = (qx, qy, qz) -> biomeHolder;
                BiomeManager forcedManager = new BiomeManager(constant, BiomeManager.obfuscateSeed(seed));

                Map<Long, ProtoChunk> chunks = new HashMap<>();
                for (int[] c : columns) {
                    int chunkX = Math.floorDiv(c[0], 16);
                    int chunkZ = Math.floorDiv(c[1], 16);
                    long ckey = ChunkPos.pack(chunkX, chunkZ);
                    ProtoChunk chunk = chunks.get(ckey);
                    if (chunk == null) {
                        chunk = new ProtoChunk(new ChunkPos(chunkX, chunkZ), UpgradeData.EMPTY, height,
                            PalettedContainerFactory.create(registries), null);
                        chunks.put(ckey, chunk);
                    }
                    // Fill this test column from the cached base terrain.
                    BlockState[] base = baseColumns.get(ChunkPos.pack(c[0], c[1]));
                    BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
                    for (int y = minY; y < maxY; y++) {
                        BlockState s = base[y - minY];
                        if (!s.is(Blocks.AIR)) chunk.setBlockState(pos.set(c[0], y, c[1]), s, 0);
                    }
                }
                for (ProtoChunk chunk : chunks.values()) {
                    Heightmap.primeHeightmaps(chunk, EnumSet.of(Heightmap.Types.WORLD_SURFACE_WG, Heightmap.Types.OCEAN_FLOOR_WG));
                    chunk.getOrCreateNoiseChunk(cc -> NoiseChunk.forChunk(cc, randomState, Beardifier.EMPTY,
                        settings, fluidPicker(settings), Blender.empty()));
                    generator.buildSurface(chunk, context, randomState, null, forcedManager, biomeRegistry, Blender.empty());
                }
                for (int[] c : columns) {
                    int chunkX = Math.floorDiv(c[0], 16);
                    int chunkZ = Math.floorDiv(c[1], 16);
                    ProtoChunk chunk = chunks.get(ChunkPos.pack(chunkX, chunkZ));
                    BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
                    for (int y = minY; y < maxY; y++) {
                        out.println("SURF\t" + biomeId + "\t" + seed + "\t" + c[0] + "\t" + c[1] + "\t" + y + "\t"
                            + chunk.getBlockState(pos.set(c[0], y, c[1])).getBlock().builtInRegistryHolder().key().identifier());
                    }
                }
            }
        }
    }
}
