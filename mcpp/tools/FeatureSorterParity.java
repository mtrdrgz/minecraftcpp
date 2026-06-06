// Ground-truth generator for net.minecraft.world.level.biome.FeatureSorter.
//
// This emits the vanilla StepFeatureData order for all 26.1.2 biomes using a
// deterministic lexicographic biome-source order. The C++ test rebuilds the
// same graph from data/minecraft/worldgen/biome/*.json and compares each
// step-local feature index, which is the value fed to
// WorldgenRandom.setFeatureSeed(decorationSeed, index, step).
//
//   tools/run_groundtruth.ps1 -Tool FeatureSorterParity -Out mcpp/build/feature_sorter.tsv

import java.util.Comparator;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import net.minecraft.SharedConstants;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.FeatureSorter;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;

public class FeatureSorterParity {
    public static void main(String[] args) {
        final java.io.PrintStream out = System.out;
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();
        HolderLookup.Provider holders = VanillaRegistries.createLookup();

        HolderLookup.RegistryLookup<Biome> biomes = holders.lookupOrThrow(Registries.BIOME);
        HolderLookup.RegistryLookup<PlacedFeature> placedFeatures = holders.lookupOrThrow(Registries.PLACED_FEATURE);

        Map<PlacedFeature, String> featureKeys = new IdentityHashMap<>();
        placedFeatures.listElements().forEach(holder ->
            featureKeys.put(holder.value(), holder.key().identifier().toString())
        );

        List<Holder.Reference<Biome>> biomeSources = biomes.listElements()
            .sorted(Comparator.comparing(holder -> holder.key().identifier().toString()))
            .toList();
        biomeSources.forEach(holder -> out.println("BIOME\t" + holder.key().identifier()));

        List<FeatureSorter.StepFeatureData> featuresPerStep = FeatureSorter.buildFeaturesPerStep(
            biomeSources,
            holder -> holder.value().getGenerationSettings().features(),
            true
        );

        for (int step = 0; step < featuresPerStep.size(); ++step) {
            List<PlacedFeature> features = featuresPerStep.get(step).features();
            for (int index = 0; index < features.size(); ++index) {
                String key = featureKeys.get(features.get(index));
                if (key == null) {
                    throw new IllegalStateException("Unregistered placed feature at step " + step + " index " + index);
                }
                out.println("STEP\t" + step + "\t" + index + "\t" + key);
            }
        }
    }
}
