import java.util.List;
import net.minecraft.SharedConstants;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;

public class FeatureSorterOverworldParity {
    public static void main(String[] args) {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();
        HolderLookup.Provider holders = VanillaRegistries.createLookup();
        HolderLookup.RegistryLookup<Biome> biomes = holders.lookupOrThrow(Registries.BIOME);
        HolderLookup.RegistryLookup<MultiNoiseBiomeSourceParameterList> presets =
            holders.lookupOrThrow(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST);

        BiomeSource source = MultiNoiseBiomeSource.createFromPreset(presets.getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD));
        List<Holder.Reference<Biome>> biomeSources = List.copyOf(source.possibleBiomes()).stream()
            .map(holder -> biomes.getOrThrow(holder.unwrapKey().orElseThrow()))
            .toList();

        System.out.println("MODE\tOVERWORLD_SOURCE");
        biomeSources.forEach(holder -> System.out.println("BIOME\t" + holder.key().identifier()));
    }
}
