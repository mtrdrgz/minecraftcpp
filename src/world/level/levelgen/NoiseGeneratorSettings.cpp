#include "NoiseGeneratorSettings.h"
#include "../block/Blocks.h"

namespace mc::levelgen {

namespace {
    uint32_t state(std::string_view name, uint32_t fallback = 0) {
        return getDefaultBlockStateId(name, fallback);
    }

    NoiseGeneratorSettings make(
        NoiseSettings noiseSettings,
        uint32_t defaultBlock,
        uint32_t defaultFluid,
        int seaLevel,
        bool disableMobGeneration,
        bool aquifersEnabled,
        bool oreVeinsEnabled,
        bool useLegacyRandomSource
    ) {
        NoiseGeneratorSettings settings;
        settings.noiseSettings = noiseSettings;
        settings.defaultBlock = defaultBlock;
        settings.defaultFluid = defaultFluid;
        settings.seaLevel = seaLevel;
        settings.disableMobGeneration = disableMobGeneration;
        settings.aquifersEnabled = aquifersEnabled;
        settings.oreVeinsEnabled = oreVeinsEnabled;
        settings.useLegacyRandomSource = useLegacyRandomSource;
        return settings;
    }
}

bool NoiseGeneratorSettings::isAquifersEnabled() const {
    return aquifersEnabled;
}

bool NoiseGeneratorSettings::areOreVeinsEnabled() const {
    return oreVeinsEnabled;
}

NoiseGeneratorSettings::RandomAlgorithm NoiseGeneratorSettings::getRandomSource() const {
    return useLegacyRandomSource ? RandomAlgorithm::Legacy : RandomAlgorithm::Xoroshiro;
}

NoiseGeneratorSettings NoiseGeneratorSettings::overworld() {
    NoiseGeneratorSettings s = make(NoiseSettings::overworld(), state("stone"), state("water"), 63, false, true, true, false);
    s.largeBiomesFlag = false;
    s.amplifiedFlag = false;
    return s;
}

// Java: NoiseGeneratorSettings.overworld(context, isAmplified=false, largeBiomes=true).
NoiseGeneratorSettings NoiseGeneratorSettings::largeBiomes() {
    NoiseGeneratorSettings s = overworld();
    s.largeBiomesFlag = true;
    return s;
}

// Java: NoiseGeneratorSettings.overworld(context, isAmplified=true, largeBiomes=false).
NoiseGeneratorSettings NoiseGeneratorSettings::amplified() {
    NoiseGeneratorSettings s = overworld();
    s.amplifiedFlag = true;
    return s;
}

NoiseGeneratorSettings NoiseGeneratorSettings::nether() {
    return make(NoiseSettings::nether(), state("netherrack"), state("lava"), 32, false, false, false, true);
}

NoiseGeneratorSettings NoiseGeneratorSettings::end() {
    return make(NoiseSettings::end(), state("end_stone"), state("air"), 0, true, false, false, true);
}

NoiseGeneratorSettings NoiseGeneratorSettings::caves() {
    return make(NoiseSettings::caves(), state("stone"), state("water"), 32, false, false, false, true);
}

NoiseGeneratorSettings NoiseGeneratorSettings::floatingIslands() {
    return make(NoiseSettings::floatingIslands(), state("stone"), state("water"), -64, false, false, false, true);
}

NoiseGeneratorSettings NoiseGeneratorSettings::dummy() {
    return make(NoiseSettings::overworld(), state("stone"), state("air"), 63, true, false, false, false);
}

} // namespace mc::levelgen
