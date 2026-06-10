// Reference value generator for the C++ mc::biome::BiomeTemperature port. Runs the
// REAL decompiled net.minecraft.world.level.biome.Biome from the jar so the emitted
// temperatures are exact ground truth.
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/BiomeTemperatureParity.java
//   java  -cp <out>;26.1.2/client.jar BiomeTemperatureParity > biome_temperature.tsv
//
// We construct a REAL Biome via the public Biome.BiomeBuilder (temperature +
// temperatureModifier + minimal required effects/EMPTY settings), then reflectively
// invoke the PRIVATE methods getHeightAdjustedTemperature(BlockPos,int) and
// getTemperature(BlockPos,int). Those run vanilla's actual arithmetic against the
// REAL static PerlinSimplexNoise fields (TEMPERATURE_NOISE / FROZEN_TEMPERATURE_NOISE /
// BIOME_INFO_NOISE), so the values are bit-exact ground truth.
//
// Rows (all floats as raw IEEE bits, 8 hex, so the C++ compare is exact):
//   HAT\t<baseTempBits>\t<modifier:0=NONE,1=FROZEN>\t<x>\t<y>\t<z>\t<seaLevel>\t<valueBits>
//   TMP\t<baseTempBits>\t<modifier>\t<x>\t<y>\t<z>\t<seaLevel>\t<valueBits>
// modifier emitted decimal; x/y/z/seaLevel decimal; baseTemp & value as float bits.
import java.io.PrintStream;
import java.lang.reflect.Method;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeGenerationSettings;
import net.minecraft.world.level.biome.BiomeSpecialEffects;
import net.minecraft.world.level.biome.MobSpawnSettings;

public class BiomeTemperatureParity {
    static final PrintStream O = System.out;

    static String f(float v) {
        return String.format("%08x", Float.floatToRawIntBits(v));
    }

    static Biome buildBiome(float temperature, Biome.TemperatureModifier modifier) {
        BiomeSpecialEffects effects = new BiomeSpecialEffects.Builder().waterColor(4159204).build();
        return new Biome.BiomeBuilder()
            .hasPrecipitation(true)
            .temperature(temperature)
            .temperatureAdjustment(modifier)
            .downfall(0.5F)
            .specialEffects(effects)
            .mobSpawnSettings(MobSpawnSettings.EMPTY)
            .generationSettings(BiomeGenerationSettings.EMPTY)
            .build();
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Method mHat = Biome.class.getDeclaredMethod("getHeightAdjustedTemperature", BlockPos.class, int.class);
        mHat.setAccessible(true);
        Method mTmp = Biome.class.getDeclaredMethod("getTemperature", BlockPos.class, int.class);
        mTmp.setAccessible(true);

        // Finite/physical base temperatures spanning the vanilla biome range and a bit beyond.
        float[] baseTemps = {
            -0.7F, -0.5F, -0.3F, 0.0F, 0.05F, 0.1F, 0.15F, 0.2F, 0.25F,
            0.3F, 0.4F, 0.5F, 0.7F, 0.75F, 0.8F, 0.9F, 0.95F, 1.0F, 1.25F, 2.0F
        };

        Biome.TemperatureModifier[] modifiers = {
            Biome.TemperatureModifier.NONE, Biome.TemperatureModifier.FROZEN
        };

        // World coordinates: include the cold-biome ranges, near-spawn, large offsets,
        // negatives (FROZEN noise is symmetric-sensitive) and ice-patch-prone spots.
        int[] xs = { 0, 1, 7, 8, 13, 16, 31, 64, 100, 127, 200, 255, 511, 1000, -1, -7, -16, -64, -255, -1000 };
        int[] zs = { 0, 3, 8, 11, 17, 32, 50, 80, 128, 200, 300, 512, 999, -2, -8, -32, -100, -300, -777, -1500 };

        // Block Y across build height incl. below/at/above the snowLevel = seaLevel+17 boundary.
        int[] ys = { -64, 0, 32, 62, 63, 70, 79, 80, 81, 100, 128, 160, 200, 256, 319 };

        int[] seaLevels = { 63, 0, 50, 32 };

        for (float baseTemp : baseTemps) {
            for (int mi = 0; mi < modifiers.length; mi++) {
                for (int x : xs) {
                    for (int z : zs) {
                        for (int seaLevel : seaLevels) {
                            // getTemperature memoizes in a per-thread cache keyed by pos.asLong()
                            // (x,y,z) ONLY -- seaLevel is NOT in the key (Biome.java:124-139). A real
                            // level/dimension has exactly ONE seaLevel, so getTemperature(pos,seaLevel)
                            // is never called for the same pos under two seaLevels. This battery DOES
                            // sweep 4 seaLevels per pos, which would let the cache return a value
                            // computed under a PRIOR (different) seaLevel -- a NONPHYSICAL stale hit.
                            // Build a fresh Biome (fresh cache) per seaLevel so each pos is computed
                            // under its own seaLevel exactly as real code does. The noise statics are
                            // class-level static finals, so this does not change the math.
                            Biome biome = buildBiome(baseTemp, modifiers[mi]);
                            for (int y : ys) {
                                BlockPos pos = new BlockPos(x, y, z);
                                float hat = (Float) mHat.invoke(biome, pos, seaLevel);
                                float tmp = (Float) mTmp.invoke(biome, pos, seaLevel);
                                O.println("HAT\t" + f(baseTemp) + "\t" + mi + "\t" + x + "\t" + y + "\t" + z
                                    + "\t" + seaLevel + "\t" + f(hat));
                                O.println("TMP\t" + f(baseTemp) + "\t" + mi + "\t" + x + "\t" + y + "\t" + z
                                    + "\t" + seaLevel + "\t" + f(tmp));
                            }
                        }
                    }
                }
            }
        }
    }
}
