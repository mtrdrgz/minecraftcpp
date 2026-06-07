// Ground-truth generator for structure placement (which chunks are structure
// chunks for a given world seed) using the REAL decompiled 26.1.2 code:
// net.minecraft.world.level.levelgen.structure.placement.StructurePlacement
// (RandomSpreadStructurePlacement) via ChunkGeneratorStructureState.
//
// The C++ test (StructurePlacementParityTest) rebuilds placement from
// data/minecraft/worldgen/structure_set/*.json and must agree on every positive
// chunk over the scanned grid, for every random_spread set and seed.
//
//   tools/run_groundtruth.ps1 -Tool StructurePlacementParity -Out mcpp/build/structure_placement.tsv
//
// Rows:
//   SEED   <seed>
//   COUNT  <seed>  <setId>  <numPositives>
//   HIT    <seed>  <setId>  <chunkX>  <chunkZ>

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.chunk.ChunkGeneratorStructureState;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.structure.StructureSet;
import net.minecraft.world.level.levelgen.structure.placement.RandomSpreadStructurePlacement;
import net.minecraft.world.level.levelgen.structure.placement.StructurePlacement;

public class StructurePlacementParity {
    static final int MIN = -80;
    static final int MAX = 80; // exclusive

    public static void main(String[] args) {
        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        HolderLookup.Provider holders = VanillaRegistries.createLookup();

        HolderLookup.RegistryLookup<StructureSet> setLookup = holders.lookupOrThrow(Registries.STRUCTURE_SET);
        HolderLookup.RegistryLookup<MultiNoiseBiomeSourceParameterList> presets =
            holders.lookupOrThrow(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST);
        BiomeSource biomeSource =
            MultiNoiseBiomeSource.createFromPreset(presets.getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD));

        long[] seeds = { 0L, 1L, 42L, 123456789L };

        // Stable, sorted list of (id, holder) for all structure sets.
        List<net.minecraft.core.Holder.Reference<StructureSet>> setHolders = new ArrayList<>();
        setLookup.listElements().forEach(setHolders::add);
        setHolders.sort((a, b) -> a.key().identifier().toString().compareTo(b.key().identifier().toString()));

        for (long seed : seeds) {
            out.println("SEED\t" + seed);
            RandomState randomState = RandomState.create(holders, NoiseGeneratorSettings.OVERWORLD, seed);
            // createForFlat with no overrides avoids createForNormal's
            // hasBiomesForStructureSet biome-tag binding (unrelated to placement
            // math). getLevelSeed() still returns `seed`, which is all
            // isStructureChunk / exclusion zones consult for random_spread.
            ChunkGeneratorStructureState state =
                ChunkGeneratorStructureState.createForFlat(randomState, seed, biomeSource, java.util.stream.Stream.empty());

            for (net.minecraft.core.Holder.Reference<StructureSet> holder : setHolders) {
                String id = holder.key().identifier().toString();
                StructurePlacement placement = holder.value().placement();
                if (!(placement instanceof RandomSpreadStructurePlacement)) {
                    continue; // concentric_rings (strongholds) handled separately
                }
                int count = 0;
                StringBuilder hits = new StringBuilder();
                for (int x = MIN; x < MAX; ++x) {
                    for (int z = MIN; z < MAX; ++z) {
                        if (placement.isStructureChunk(state, x, z)) {
                            hits.append("HIT\t").append(seed).append('\t').append(id)
                                .append('\t').append(x).append('\t').append(z).append('\n');
                            ++count;
                        }
                    }
                }
                out.println("COUNT\t" + seed + "\t" + id + "\t" + count);
                out.print(hits);
            }
        }
    }
}
