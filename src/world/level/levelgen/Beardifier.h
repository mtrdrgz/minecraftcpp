#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.Beardifier — the density-function
// term that adapts terrain to structure pieces (the `minecraft:beardifier` slot the
// noise router adds via DensityFunctions.add(finalDensity, BeardifierMarker)).
//
// Per chunk, NoiseChunk builds a Beardifier from the structure starts whose
// terrainAdaptation() != NONE (Beardifier.forStructuresInChunk); compute() then adds
// a per-block contribution that raises a "beard" of terrain under floating pieces
// and carves terrain above buried ones, so houses meet the ground on slopes.
//
// This header is the pure algorithm (kernel + contributions + compute). The
// per-chunk piece collection (forStructuresInChunk) is built by the structure
// runtime, which feeds Rigid/Junction lists here. EMPTY / affectedBox==nullopt ->
// compute returns 0, so terrain with no nearby beard-adapting structure is
// byte-unchanged (the certified no-structure terrain parity is preserved).

#include "Mth.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mc::levelgen {

// Minimal axis-aligned box (the only BoundingBox surface the Beardifier needs).
// Self-contained so this header pulls no structure-system types (those carry a
// clashing structure::Vec3i). Callers convert their own box into this.
struct BeardBox {
    std::int32_t minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0;
    bool isInside(int x, int y, int z) const {
        return x >= minX && x <= maxX && y >= minY && y <= maxY && z >= minZ && z <= maxZ;
    }
    BeardBox inflatedBy(int a) const {
        return {minX - a, minY - a, minZ - a, maxX + a, maxY + a, maxZ + a};
    }
    static BeardBox encapsulating(const BeardBox& a, const BeardBox& b) {
        return {std::min(a.minX, b.minX), std::min(a.minY, b.minY), std::min(a.minZ, b.minZ),
                std::max(a.maxX, b.maxX), std::max(a.maxY, b.maxY), std::max(a.maxZ, b.maxZ)};
    }
};

// TerrainAdjustment.java — structure terrain adaptation kinds.
enum class TerrainAdjustment { NONE, BURY, BEARD_THIN, BEARD_BOX, ENCAPSULATE };

inline TerrainAdjustment terrainAdjustmentByName(const std::string& s) {
    if (s == "bury") return TerrainAdjustment::BURY;
    if (s == "beard_thin") return TerrainAdjustment::BEARD_THIN;
    if (s == "beard_box") return TerrainAdjustment::BEARD_BOX;
    if (s == "encapsulate") return TerrainAdjustment::ENCAPSULATE;
    return TerrainAdjustment::NONE;
}

class Beardifier {
public:
    static constexpr int BEARD_KERNEL_RADIUS = 12;

    // Beardifier.Rigid(box, terrainAdjustment, groundLevelDelta).
    struct Rigid {
        BeardBox box{};
        TerrainAdjustment terrainAdjustment = TerrainAdjustment::NONE;
        int groundLevelDelta = 0;
    };

    // JigsawJunction(sourceX, sourceGroundY, sourceZ) — only the fields compute() uses.
    struct Junction {
        int sourceX = 0;
        int sourceGroundY = 0;
        int sourceZ = 0;
    };

    Beardifier() = default;  // EMPTY
    Beardifier(std::vector<Rigid> pieces, std::vector<Junction> junctions,
               std::optional<BeardBox> affectedBox)
        : m_pieces(std::move(pieces)), m_junctions(std::move(junctions)),
          m_affectedBox(affectedBox) {}

    bool isEmpty() const { return !m_affectedBox.has_value(); }

    // Beardifier.compute(FunctionContext).
    double compute(int blockX, int blockY, int blockZ) const {
        if (!m_affectedBox) return 0.0;
        const auto& box = *m_affectedBox;
        if (!box.isInside(blockX, blockY, blockZ)) return 0.0;

        double noiseValue = 0.0;
        for (const Rigid& rigid : m_pieces) {
            const auto& b = rigid.box;
            int dx = std::max(0, std::max(b.minX - blockX, blockX - b.maxX));
            int dz = std::max(0, std::max(b.minZ - blockZ, blockZ - b.maxZ));
            int groundY = b.minY + rigid.groundLevelDelta;
            int dyToGround = blockY - groundY;

            int dy = 0;
            switch (rigid.terrainAdjustment) {
                case TerrainAdjustment::NONE: dy = 0; break;
                case TerrainAdjustment::BURY:
                case TerrainAdjustment::BEARD_THIN: dy = dyToGround; break;
                case TerrainAdjustment::BEARD_BOX:
                    dy = std::max(0, std::max(groundY - blockY, blockY - b.maxY)); break;
                case TerrainAdjustment::ENCAPSULATE:
                    dy = std::max(0, std::max(b.minY - blockY, blockY - b.maxY)); break;
            }

            switch (rigid.terrainAdjustment) {
                case TerrainAdjustment::NONE: break;
                case TerrainAdjustment::BURY:
                    noiseValue += getBuryContribution(dx, dy / 2.0, dz); break;
                case TerrainAdjustment::BEARD_THIN:
                case TerrainAdjustment::BEARD_BOX:
                    noiseValue += getBeardContribution(dx, dy, dz, dyToGround) * 0.8; break;
                case TerrainAdjustment::ENCAPSULATE:
                    noiseValue += getBuryContribution(dx / 2.0, dy / 2.0, dz / 2.0) * 0.8; break;
            }
        }

        for (const Junction& junction : m_junctions) {
            int dx = blockX - junction.sourceX;
            int dy = blockY - junction.sourceGroundY;
            int dz = blockZ - junction.sourceZ;
            noiseValue += getBeardContribution(dx, dy, dz, dy) * 0.4;
        }
        return noiseValue;
    }

private:
    std::vector<Rigid> m_pieces;
    std::vector<Junction> m_junctions;
    std::optional<BeardBox> m_affectedBox;

    static double getBuryContribution(double dx, double dy, double dz) {
        double distance = mc::levelgen::mth::length(dx, dy, dz);
        return mc::levelgen::mth::clampedMapD(distance, 0.0, 6.0, 1.0, 0.0);
    }

    static bool isInKernelRange(int xi) { return xi >= 0 && xi < 24; }

    static double getBeardContribution(int dx, int dy, int dz, int yToGround) {
        int xi = dx + 12;
        int yi = dy + 12;
        int zi = dz + 12;
        if (isInKernelRange(xi) && isInKernelRange(yi) && isInKernelRange(zi)) {
            double dyWithOffset = yToGround + 0.5;
            double distanceSqr = mc::levelgen::mth::lengthSquared(static_cast<double>(dx), dyWithOffset, static_cast<double>(dz));
            double value = -dyWithOffset * mc::levelgen::mth::fastInvSqrt(distanceSqr / 2.0) / 2.0;
            return value * static_cast<double>(kernel()[zi * 24 * 24 + xi * 24 + yi]);
        }
        return 0.0;
    }

    // computeBeardContribution(dx, dy, dz) = computeBeardContribution(dx, dy+0.5, dz).
    static double computeBeardContribution(int dx, double dy, int dz) {
        double distanceSqr = mc::levelgen::mth::lengthSquared(static_cast<double>(dx), dy, static_cast<double>(dz));
        return std::pow(M_E, -distanceSqr / 16.0);
    }

    // BEARD_KERNEL: float[13824], kernel[zi*576 + xi*24 + yi] = (float)computeBeardContribution(xi-12, yi-12, zi-12).
    static const std::array<float, 13824>& kernel() {
        static const std::array<float, 13824> k = [] {
            std::array<float, 13824> a{};
            for (int zi = 0; zi < 24; ++zi)
                for (int xi = 0; xi < 24; ++xi)
                    for (int yi = 0; yi < 24; ++yi)
                        a[zi * 24 * 24 + xi * 24 + yi] =
                            static_cast<float>(computeBeardContribution(xi - 12, (yi - 12) + 0.5, zi - 12));
            return a;
        }();
        return k;
    }
};

} // namespace mc::levelgen
