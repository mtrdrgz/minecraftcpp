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
