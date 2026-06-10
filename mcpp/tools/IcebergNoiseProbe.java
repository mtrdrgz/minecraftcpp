// Throwaway probe: print the REAL icebergSurface/icebergPillar/icebergPillarRoof
// NormalNoise values (RandomState.getOrCreateNoise) at given block columns, plus the
// real Biome.shouldMeltFrozenOceanIcebergSlightly flag — ground truth for the C++
// SurfaceSystem.frozenOceanExtension port.
//   java IcebergNoiseProbe <seed> x,z [x,z ...]
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.Noises;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.synth.NormalNoise;

public class IcebergNoiseProbe {
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        HolderLookup.Provider provider = VanillaRegistries.createLookup();
        long seed = Long.parseLong(args[0]);
        RandomState randomState = RandomState.create(provider, NoiseGeneratorSettings.OVERWORLD, seed);
        NormalNoise surf = randomState.getOrCreateNoise(Noises.ICEBERG_SURFACE);
        NormalNoise pillar = randomState.getOrCreateNoise(Noises.ICEBERG_PILLAR);
        NormalNoise roof = randomState.getOrCreateNoise(Noises.ICEBERG_PILLAR_ROOF);
        Biome frozenOcean = provider.lookupOrThrow(Registries.BIOME)
            .getOrThrow(net.minecraft.world.level.biome.Biomes.FROZEN_OCEAN).value();
        Biome deepFrozen = provider.lookupOrThrow(Registries.BIOME)
            .getOrThrow(net.minecraft.world.level.biome.Biomes.DEEP_FROZEN_OCEAN).value();
        int seaLevel = 63;
        for (int i = 1; i < args.length; i++) {
            String[] p = args[i].split(",");
            int x = Integer.parseInt(p[0]), z = Integer.parseInt(p[1]);
            double s = surf.getValue(x, 0.0, z);
            double pl = pillar.getValue(x * 1.28, 0.0, z * 1.28);
            double rf = roof.getValue(x * 1.17, 0.0, z * 1.17);
            BlockPos pos = new BlockPos(x, seaLevel, z);
            System.out.println("PROBE\t" + x + "\t" + z
                + "\tsurf=" + s + "\tpillar=" + pl + "\troof=" + rf
                + "\ticeberg=" + Math.min(Math.abs(s * 8.25), pl * 15.0)
                + "\tmeltFO=" + (frozenOcean.shouldMeltFrozenOceanIcebergSlightly(pos, seaLevel) ? 1 : 0)
                + "\tmeltDFO=" + (deepFrozen.shouldMeltFrozenOceanIcebergSlightly(pos, seaLevel) ? 1 : 0));
        }
    }
}
