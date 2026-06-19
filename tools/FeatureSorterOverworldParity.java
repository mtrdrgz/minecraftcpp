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
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.biome.FeatureSorter;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;

public class FeatureSorterOverworldParity {
    public static void main(String[] args) {
        final java.io.PrintStream out = System.out;
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();
        HolderLookup.Provider holders = VanillaRegistries.createLookup();
        HolderLookup.RegistryLookup<Biome> biomes = holders.lookupOrThrow(Registries.BIOME);
        HolderLookup.RegistryLookup<PlacedFeature> placedFeatures = holders.lookupOrThrow(Registries.PLACED_FEATURE);
        HolderLookup.RegistryLookup<MultiNoiseBiomeSourceParameterList> presets =
            holders.lookupOrThrow(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST);

        Map<PlacedFeature, String> featureKeys = new IdentityHashMap<>();
        placedFeatures.listElements().forEach(holder -> featureKeys.put(holder.value(), holder.key().identifier().toString()));

        BiomeSource source = MultiNoiseBiomeSource.createFromPreset(presets.getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD));
        List<Holder.Reference<Biome>> biomeSources = List.copyOf(source.possibleBiomes()).stream()
            .map(holder -> biomes.getOrThrow(holder.unwrapKey().orElseThrow()))
            .toList();

        out.println("MODE\tOVERWORLD_SOURCE");
        biomeSources.forEach(holder -> out.println("BIOME\t" + holder.key().identifier()));
        System.out.println("MODE\tOVERWORLD_SOURCE");
        biomeSources.forEach(holder -> System.out.println("BIOME\t" + holder.key().identifier()));

        List<FeatureSorter.StepFeatureData> featuresPerStep = FeatureSorter.buildFeaturesPerStep(
            biomeSources,
            holder -> holder.value().getGenerationSettings().features(),
            true);
        out.println("STEPS\t" + featuresPerStep.size());
        System.out.println("STEPS\t" + featuresPerStep.size());
        for (int step = 0; step < featuresPerStep.size(); ++step) {
            List<PlacedFeature> features = featuresPerStep.get(step).features();
            for (int index = 0; index < features.size(); ++index) {
                String key = featureKeys.get(features.get(index));
                if (key == null) throw new IllegalStateException("Unregistered placed feature");
                out.println("STEP\t" + step + "\t" + index + "\t" + key);
                System.out.println("STEP\t" + step + "\t" + index + "\t" + key);
            }
        }
    }
}
