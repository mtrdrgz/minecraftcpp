#pragma once

#include "NoiseSettings.h"
#include <cstdint>

namespace mc::levelgen {

// Port of net.minecraft.world.level.levelgen.NoiseGeneratorSettings.
struct NoiseGeneratorSettings {
    enum class RandomAlgorithm {
        Legacy,
        Xoroshiro
    };

    NoiseSettings noiseSettings;
    uint32_t defaultBlock = 0;
    uint32_t defaultFluid = 0;
    int seaLevel = 63;
    bool disableMobGeneration = false;
    bool aquifersEnabled = false;
    bool oreVeinsEnabled = false;
    bool useLegacyRandomSource = false;
    // Overworld-only flags (Java's NoiseGeneratorSettings.overworld(context,
    // isAmplified, largeBiomes)). Used by makeRouter to pick the correct
    // NoiseRouterData::overworld(randomState, largeBiomes, amplified) variant.
    // Named with the `Flag` suffix to avoid clashing with the static factory
    // methods largeBiomes()/amplified() of the same name.
    bool largeBiomesFlag = false;
    bool amplifiedFlag = false;

    bool isAquifersEnabled() const;
    bool areOreVeinsEnabled() const;
    RandomAlgorithm getRandomSource() const;

    static NoiseGeneratorSettings overworld();
    static NoiseGeneratorSettings largeBiomes();
    static NoiseGeneratorSettings amplified();
    static NoiseGeneratorSettings nether();
    static NoiseGeneratorSettings end();
    static NoiseGeneratorSettings caves();
    static NoiseGeneratorSettings floatingIslands();
    static NoiseGeneratorSettings dummy();
};

} // namespace mc::levelgen
