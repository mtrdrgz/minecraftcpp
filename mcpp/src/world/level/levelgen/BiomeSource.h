#pragma once

#include "Climate.h"
#include "NoiseRouter.h"
#include <string>

namespace mc::levelgen {

class BiomeSource {
public:
    explicit BiomeSource(const NoiseRouter& router);

    std::string getBiomeAt(int blockX, int blockY, int blockZ) const;
    std::string getNoiseBiome(int quartX, int quartY, int quartZ) const;
    bool hasCompleteOverworldPreset() const noexcept { return m_overworldPresetComplete; }
    size_t parameterCount() const noexcept { return m_parameters.values().size(); }

private:
    NoiseRouter m_router;
    Climate::ParameterList<std::string> m_parameters;
    bool m_overworldPresetComplete = false;
};

} // namespace mc::levelgen
