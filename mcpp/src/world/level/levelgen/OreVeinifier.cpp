#include "OreVeinifier.h"
#include "../block/Blocks.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace mc::levelgen {

namespace {
    uint32_t state(std::string_view name, uint32_t fallback = 0) {
        return getDefaultBlockStateId(name, fallback);
    }

    double clampedMap(double value, double fromMin, double fromMax, double toMin, double toMax) {
        double mapped = toMin + (value - fromMin) * (toMax - toMin) / (fromMax - fromMin);
        return std::max(std::min(toMin, toMax), std::min(std::max(toMin, toMax), mapped));
    }
}

OreVeinifier::OreVeinifier(
    DensityFunctionPtr veinToggle,
    DensityFunctionPtr veinRidged,
    DensityFunctionPtr veinGap,
    std::shared_ptr<PositionalRandomFactory> oreVeinsPositionalRandomFactory
) : m_veinToggle(std::move(veinToggle)),
    m_veinRidged(std::move(veinRidged)),
    m_veinGap(std::move(veinGap)),
    m_oreVeinsPositionalRandomFactory(std::move(oreVeinsPositionalRandomFactory)),
    m_copper{ state("copper_ore"), state("raw_copper_block"), state("granite"), 0, 50 },
    m_iron{ state("deepslate_iron_ore"), state("raw_iron_block"), state("tuff"), -60, -8 } {
}

std::optional<uint32_t> OreVeinifier::compute(const DensityFunctionContext& context) const {
    const double oreVeininessNoiseValue = m_veinToggle->compute(context);
    const int posY = context.blockY;
    const VeinType& veinType = oreVeininessNoiseValue > 0.0 ? m_copper : m_iron;
    const double veininessRidged = std::abs(oreVeininessNoiseValue);
    const int distanceFromTop = veinType.maxY - posY;
    const int distanceFromBottom = posY - veinType.minY;
    if (distanceFromBottom < 0 || distanceFromTop < 0) {
        return std::nullopt;
    }

    const int distanceFromEdge = std::min(distanceFromTop, distanceFromBottom);
    const double edgeRoundoff = clampedMap(distanceFromEdge, 0.0, 20.0, -0.2, 0.0);
    if (veininessRidged + edgeRoundoff < 0.4) {
        return std::nullopt;
    }

    std::shared_ptr<RandomSource> positionalRandom = m_oreVeinsPositionalRandomFactory->at(context.blockX, posY, context.blockZ);
    if (positionalRandom->nextFloat() > 0.7F) {
        return std::nullopt;
    }
    if (m_veinRidged->compute(context) >= 0.0) {
        return std::nullopt;
    }

    const double richness = clampedMap(veininessRidged, 0.4, 0.6, 0.1, 0.3);
    if (positionalRandom->nextFloat() < richness && m_veinGap->compute(context) > -0.3) {
        return positionalRandom->nextFloat() < 0.02F ? veinType.rawOreBlock : veinType.ore;
    }
    return veinType.filler;
}

} // namespace mc::levelgen
