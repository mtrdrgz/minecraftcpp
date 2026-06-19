#pragma once

#include "DensityFunction.h"
#include "RandomSource.h"

#include <cstdint>
#include <memory>
#include <optional>

namespace mc::levelgen {

class OreVeinifier {
public:
    OreVeinifier(
        DensityFunctionPtr veinToggle,
        DensityFunctionPtr veinRidged,
        DensityFunctionPtr veinGap,
        std::shared_ptr<PositionalRandomFactory> oreVeinsPositionalRandomFactory);

    std::optional<uint32_t> compute(const DensityFunctionContext& context) const;

private:
    struct VeinType {
        uint32_t ore = 0;
        uint32_t rawOreBlock = 0;
        uint32_t filler = 0;
        int minY = 0;
        int maxY = 0;
    };

    DensityFunctionPtr m_veinToggle;
    DensityFunctionPtr m_veinRidged;
    DensityFunctionPtr m_veinGap;
    std::shared_ptr<PositionalRandomFactory> m_oreVeinsPositionalRandomFactory;
    VeinType m_copper;
    VeinType m_iron;
};

} // namespace mc::levelgen
