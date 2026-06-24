#include "WorldCarver.h"

#include "../Aquifer.h"
#include "../FloatProvider.h"
#include "../VerticalAnchor.h"
#include "../WorldGenerationContext.h"
#include "../heightproviders/HeightProvider.h"
#include "../Mth.h"
#include "../../block/BlockState.h"
#include "../../block/Blocks.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace mc::levelgen::carver {
namespace {

using mc::levelgen::heightproviders::HeightProviderPtr;
using mc::levelgen::heightproviders::UniformHeight;
using mc::valueproviders::ConstantFloat;
using mc::valueproviders::FloatProviderPtr;
using mc::valueproviders::TrapezoidFloat;
using mc::valueproviders::UniformFloat;

constexpr float PI_F = 3.14159265358979323846f;
constexpr double PI_D = 3.14159265358979323846264338327950288;

int floorToInt(double v) {
    return mc::levelgen::mth::floor(v);
}

// Java Mth.sin/cos take a double (float args widen). Delegate to the shared 1:1
// table — the previous local copy used a truncated float scale (10430.378f) and a
// different table expression, which diverges from Java by ULPs at some args.
float mthSin(float v) { return mc::levelgen::mth::sin(static_cast<double>(v)); }
float mthCos(float v) { return mc::levelgen::mth::cos(static_cast<double>(v)); }

float randomBetween(RandomSource& random, float min, float maxExclusive) {
    return random.nextFloat() * (maxExclusive - min) + min;
}

std::uint32_t state(std::string_view name, std::uint32_t fallback = 0) {
    return getDefaultBlockStateId(name, fallback);
}

const std::unordered_set<std::uint32_t>& overworldReplaceables() {
    static const std::unordered_set<std::uint32_t> ids = [] {
        const char* names[] = {
            "andesite", "black_terracotta", "blue_terracotta", "brown_terracotta", "calcite",
            "coarse_dirt", "copper_ore", "cyan_terracotta", "deepslate", "deepslate_copper_ore",
            "deepslate_iron_ore", "diorite", "dirt", "granite", "grass_block", "gravel",
            "gray_terracotta", "green_terracotta", "iron_ore", "light_blue_terracotta",
            "light_gray_terracotta", "lime_terracotta", "magenta_terracotta", "moss_block",
            "mud", "muddy_mangrove_roots", "mycelium", "orange_terracotta", "packed_ice",
            "pale_moss_block", "pink_terracotta", "podzol", "powder_snow", "purple_terracotta",
            "raw_copper_block", "raw_iron_block", "red_sand", "red_sandstone", "red_terracotta",
            "rooted_dirt", "sand", "sandstone", "snow", "snow_block", "stone",
            "suspicious_gravel", "suspicious_sand", "terracotta", "tuff", "water",
            "white_terracotta", "yellow_terracotta"
        };
        std::unordered_set<std::uint32_t> out;
        for (const char* name : names) {
            out.insert(state(name, 0));
        }
        return out;
    }();
    return ids;
}

// #minecraft:nether_carver_replaceables (NetherWorldCarver.java:20 + the tag
// expansion). Verbatim from
// 26.1.2/data/minecraft/tags/block/nether_carver_replaceables.json.
const std::unordered_set<std::uint32_t>& netherReplaceables() {
    static const std::unordered_set<std::uint32_t> ids = [] {
        const char* names[] = {
            // base_stone_overworld
            "stone", "granite", "diorite", "andesite", "tuff", "deepslate",
            // base_stone_nether
            "netherrack", "basalt", "blackstone",
            // substrate_overworld
            "dirt", "grass_block", "sand", "red_sand", "gravel", "sandstone", "red_sandstone",
            // nylium
            "crimson_nylium", "warped_nylium",
            // wart_blocks
            "nether_wart_block", "warped_wart_block",
            // individual
            "soul_sand", "soul_soil"
        };
        std::unordered_set<std::uint32_t> out;
        for (const char* name : names) {
            out.insert(state(name, 0));
        }
        return out;
    }();
    return ids;
}

class CarvingMask {
public:
    CarvingMask(int height, int minY)
        : m_minY(minY), m_words((static_cast<std::size_t>(256 * height) + 63) / 64) {}

    bool get(int x, int y, int z) const {
        const std::size_t bit = static_cast<std::size_t>((x & 15) | ((z & 15) << 4) | ((y - m_minY) << 8));
        return (m_words[bit / 64] & (1ULL << (bit & 63))) != 0;
    }

    void set(int x, int y, int z) {
        const std::size_t bit = static_cast<std::size_t>((x & 15) | ((z & 15) << 4) | ((y - m_minY) << 8));
        m_words[bit / 64] |= 1ULL << (bit & 63);
    }

private:
    int m_minY = 0;
    std::vector<std::uint64_t> m_words;
};

struct CarvingContext {
    WorldGenerationContext world;
    int minGenY = -64;
    int genDepth = 384;
    TopMaterialGetter topMaterial;
    // chunk.markPosForPostprocessing sink (WorldCarver.java:149,157); null = drop.
    std::vector<mc::BlockPos>* fluidUpdateMarks = nullptr;
};

struct CarverConfiguration {
    float probability = 0.0f;
    HeightProviderPtr y;
    FloatProviderPtr yScale;
    VerticalAnchorPtr lavaLevel;
};

struct CaveCarverConfiguration : CarverConfiguration {
    FloatProviderPtr horizontalRadiusMultiplier;
    FloatProviderPtr verticalRadiusMultiplier;
    FloatProviderPtr floorLevel;
};

struct CanyonShapeConfiguration {
    FloatProviderPtr distanceFactor;
    FloatProviderPtr thickness;
    int widthSmoothness = 3;
    FloatProviderPtr horizontalRadiusFactor;
    float verticalRadiusDefaultFactor = 1.0f;
    float verticalRadiusCenterFactor = 0.0f;
};

struct CanyonCarverConfiguration : CarverConfiguration {
    FloatProviderPtr verticalRotation;
    CanyonShapeConfiguration shape;
};

bool canReach(const ChunkPos& chunkPos, double x, double z, int currentStep, int totalSteps, float thickness) {
    const double xMid = chunkPos.x * 16 + 8;
    const double zMid = chunkPos.z * 16 + 8;
    const double xd = x - xMid;
    const double zd = z - zMid;
    const double remaining = totalSteps - currentStep;
    const double rr = thickness + 2.0f + 16.0f;
    return xd * xd + zd * zd - remaining * remaining <= rr * rr;
}

std::optional<std::uint32_t> carveState(
    const CarvingContext& context,
    const CarverConfiguration& config,
    int x,
    int y,
    int z,
    Aquifer& aquifer
) {
    if (y <= config.lavaLevel->resolveY(context.world)) {
        return state("lava", 0);
    }
    return aquifer.computeSubstance(DensityFunctionContext{ x, y, z }, 0.0);
}

// Function-pointer type for the carveBlock override. Matches the overworld
// carveBlock and the nether netherCarveBlock signatures exactly.
using CarveBlockFn = bool(*)(const CarvingContext&, const CarverConfiguration&,
                             LevelChunk&, int, int, int, Aquifer&, bool&);

bool carveBlock(
    const CarvingContext& context,
    const CarverConfiguration& config,
    LevelChunk& chunk,
    int x,
    int y,
    int z,
    Aquifer& aquifer,
    bool& hasGrass
) {
    static const std::uint32_t grassBlock = state("grass_block", 0);
    static const std::uint32_t mycelium = state("mycelium", 0);
    static const std::uint32_t dirt = state("dirt", 0);
    static const std::uint32_t water = state("water", 0);
    static const std::uint32_t lava = state("lava", 0);

    const std::uint32_t old = chunk.getBlock(x, y, z);
    if (old == grassBlock || old == mycelium) {
        hasGrass = true;
    }

    if (overworldReplaceables().count(old) == 0) {
        return false;
    }

    std::optional<std::uint32_t> carved = carveState(context, config, x, y, z, aquifer);
    if (!carved) {
        return false;
    }

    chunk.setBlock(x, y, z, *carved);
    // WorldCarver.java:147-149: carved fluid blocks are marked for FULL-status
    // postprocessing when the aquifer requests a fluid update. The flag is
    // STATEFUL: the lava-level branch of carveState does not touch the aquifer,
    // so the previous computeSubstance's value applies — exactly as in Java.
    const bool carvedFluid = *carved == water || *carved == lava;
    if (context.fluidUpdateMarks != nullptr && aquifer.shouldScheduleFluidUpdate() && carvedFluid) {
        context.fluidUpdateMarks->push_back(mc::BlockPos{ x, y, z });
    }
    if (hasGrass && chunk.getBlock(x, y - 1, z) == dirt && context.topMaterial) {
        const bool underFluid = carvedFluid;
        std::optional<std::uint32_t> top = context.topMaterial(
            chunk,
            x,
            y - 1,
            z,
            underFluid);
        if (top) {
            chunk.setBlock(x, y - 1, z, *top);
            // WorldCarver.java:155-158: a fluid top material is marked too.
            if (context.fluidUpdateMarks != nullptr && (*top == water || *top == lava)) {
                context.fluidUpdateMarks->push_back(mc::BlockPos{ x, y - 1, z });
            }
        }
    }

    return true;
}

template <typename Skip>
bool carveEllipsoid(
    const CarvingContext& context,
    const CarverConfiguration& config,
    LevelChunk& chunk,
    Aquifer& aquifer,
    double x,
    double y,
    double z,
    double horizontalRadius,
    double verticalRadius,
    CarvingMask& mask,
    Skip&& shouldSkip,
    CarveBlockFn carveBlockFn
) {
    const ChunkPos chunkPos = chunk.pos();
    const double centerX = chunkPos.x * 16 + 8;
    const double centerZ = chunkPos.z * 16 + 8;
    const double maxDelta = 16.0 + horizontalRadius * 2.0;
    if (std::abs(x - centerX) > maxDelta || std::abs(z - centerZ) > maxDelta) {
        return false;
    }

    const int chunkMinX = chunkPos.x * 16;
    const int chunkMinZ = chunkPos.z * 16;
    const int minXIndex = std::max(floorToInt(x - horizontalRadius) - chunkMinX - 1, 0);
    const int maxXIndex = std::min(floorToInt(x + horizontalRadius) - chunkMinX, 15);
    const int minY = std::max(floorToInt(y - verticalRadius) - 1, context.minGenY + 1);
    const int protectedBlocksOnTop = 7;
    const int maxY = std::min(floorToInt(y + verticalRadius) + 1, context.minGenY + context.genDepth - 1 - protectedBlocksOnTop);
    const int minZIndex = std::max(floorToInt(z - horizontalRadius) - chunkMinZ - 1, 0);
    const int maxZIndex = std::min(floorToInt(z + horizontalRadius) - chunkMinZ, 15);
    bool carved = false;

    for (int xIndex = minXIndex; xIndex <= maxXIndex; ++xIndex) {
        const int worldX = chunkMinX + xIndex;
        const double xd = (worldX + 0.5 - x) / horizontalRadius;
        for (int zIndex = minZIndex; zIndex <= maxZIndex; ++zIndex) {
            const int worldZ = chunkMinZ + zIndex;
            const double zd = (worldZ + 0.5 - z) / horizontalRadius;
            if (xd * xd + zd * zd >= 1.0) {
                continue;
            }
            bool hasGrass = false;
            for (int worldY = maxY; worldY > minY; --worldY) {
                const double yd = (worldY - 0.5 - y) / verticalRadius;
                if (shouldSkip(context, xd, yd, zd, worldY) || mask.get(xIndex, worldY, zIndex)) {
                    continue;
                }
                mask.set(xIndex, worldY, zIndex);
                carved |= carveBlockFn(context, config, chunk, worldX, worldY, worldZ, aquifer, hasGrass);
            }
        }
    }

    return carved;
}

bool caveShouldSkip(double xd, double yd, double zd, double floorLevel) {
    return yd <= floorLevel || xd * xd + yd * yd + zd * zd >= 1.0;
}

void createCaveTunnel(
    const CarvingContext& context,
    const CaveCarverConfiguration& config,
    LevelChunk& chunk,
    std::int64_t tunnelSeed,
    Aquifer& aquifer,
    double x,
    double y,
    double z,
    double horizontalRadiusMultiplier,
    double verticalRadiusMultiplier,
    float thickness,
    float horizontalRotation,
    float verticalRotation,
    int step,
    int dist,
    double yScale,
    CarvingMask& mask,
    double floorLevel,
    CarveBlockFn carveBlockFn
) {
    SingleThreadedRandomSource random(tunnelSeed);
    const int splitPoint = random.nextInt(dist / 2) + dist / 4;
    const bool steep = random.nextInt(6) == 0;
    float yRota = 0.0f;
    float xRota = 0.0f;

    for (int currentStep = step; currentStep < dist; ++currentStep) {
        const double horizontalRadius = 1.5 + mthSin(static_cast<float>(PI_F * currentStep / dist)) * thickness;
        const double verticalRadius = horizontalRadius * yScale;
        const float cosX = mthCos(verticalRotation);
        x += mthCos(horizontalRotation) * cosX;
        y += mthSin(verticalRotation);
        z += mthSin(horizontalRotation) * cosX;
        verticalRotation *= steep ? 0.92f : 0.7f;
        verticalRotation += xRota * 0.1f;
        horizontalRotation += yRota * 0.1f;
        xRota *= 0.9f;
        yRota *= 0.75f;
        // RNG draw order must match Java exactly: subtraction operands are unsequenced
        // in C++ (GCC may reorder them), so draw all three floats into locals in Java's
        // source order before combining (a-b is not commutative, so order matters).
        const float xRotaR1 = random.nextFloat();
        const float xRotaR2 = random.nextFloat();
        const float xRotaR3 = random.nextFloat();
        xRota += (xRotaR1 - xRotaR2) * xRotaR3 * 2.0f;
        const float yRotaR1 = random.nextFloat();
        const float yRotaR2 = random.nextFloat();
        const float yRotaR3 = random.nextFloat();
        yRota += (yRotaR1 - yRotaR2) * yRotaR3 * 4.0f;

        if (currentStep == splitPoint && thickness > 1.0f) {
            // Java (CaveWorldCarver.createTunnel) evaluates each recursive call's
            // arguments strictly LEFT-TO-RIGHT: nextLong() (the child seed) is drawn
            // BEFORE nextFloat() (the child thickness). C++ leaves function-argument
            // evaluation order unspecified and GCC evaluates right-to-left, which would
            // draw the thickness float before the seed long — swapping the RNG draw
            // order and corrupting BOTH child tunnels' seed and thickness. Hoist into
            // locals in source order so the draw order matches Java on every compiler.
            const std::int64_t childSeed1 = random.nextLong();
            const float childThickness1 = random.nextFloat() * 0.5f + 0.5f;
            createCaveTunnel(context, config, chunk, childSeed1, aquifer, x, y, z,
                             horizontalRadiusMultiplier, verticalRadiusMultiplier,
                             childThickness1,
                             horizontalRotation - PI_F / 2.0f, verticalRotation / 3.0f,
                             currentStep, dist, 1.0, mask, floorLevel, carveBlockFn);
            const std::int64_t childSeed2 = random.nextLong();
            const float childThickness2 = random.nextFloat() * 0.5f + 0.5f;
            createCaveTunnel(context, config, chunk, childSeed2, aquifer, x, y, z,
                             horizontalRadiusMultiplier, verticalRadiusMultiplier,
                             childThickness2,
                             horizontalRotation + PI_F / 2.0f, verticalRotation / 3.0f,
                             currentStep, dist, 1.0, mask, floorLevel, carveBlockFn);
            return;
        }

        if (random.nextInt(4) == 0) {
            continue;
        }
        if (!canReach(chunk.pos(), x, z, currentStep, dist, thickness)) {
            return;
        }
        carveEllipsoid(context, config, chunk, aquifer, x, y, z,
                       horizontalRadius * horizontalRadiusMultiplier,
                       verticalRadius * verticalRadiusMultiplier,
                       mask,
                       [floorLevel](const CarvingContext&, double xd, double yd, double zd, int) {
                           return caveShouldSkip(xd, yd, zd, floorLevel);
                       }, carveBlockFn);
    }
}

float caveThickness(RandomSource& random) {
    float thickness = random.nextFloat() * 2.0f + random.nextFloat();
    if (random.nextInt(10) == 0) {
        thickness *= random.nextFloat() * random.nextFloat() * 3.0f + 1.0f;
    }
    return thickness;
}

// NetherWorldCarver.getThickness — (nextFloat()*2 + nextFloat()) * 2.
float netherCaveThickness(RandomSource& random) {
    return (random.nextFloat() * 2.0f + random.nextFloat()) * 2.0f;
}

// NetherWorldCarver.carveBlock (NetherWorldCarver.java:38-62). Places LAVA
// below minY+31, CAVE_AIR above. No aquifer, no grass/topMaterial.
bool netherCarveBlock(
    const CarvingContext& context,
    const CarverConfiguration& /*config*/,
    LevelChunk& chunk,
    int x,
    int y,
    int z,
    Aquifer& /*aquifer*/,
    bool& /*hasGrass*/
) {
    static const std::uint32_t lava = state("lava", 0);
    static const std::uint32_t caveAir = state("cave_air", 0);

    const std::uint32_t old = chunk.getBlock(x, y, z);
    if (netherReplaceables().count(old) == 0) {
        return false;
    }
    if (y <= context.minGenY + 31) {
        chunk.setBlock(x, y, z, lava);
    } else {
        chunk.setBlock(x, y, z, caveAir);
    }
    return true;
}

// Generic carveCave parameterized by the CaveWorldCarver overrides.
//   caveBound: getCaveBound() (overworld=15, nether=10)
//   thicknessFn: getThickness(random) (overworld/nether formulas)
//   tunnelYScale: getYScale() (overworld=1.0, nether=5.0)
//   carveBlockFn: carveBlock (overworld) or netherCarveBlock
bool carveCaveGeneric(
    const CarvingContext& context,
    const CaveCarverConfiguration& config,
    LevelChunk& chunk,
    RandomSource& random,
    Aquifer& aquifer,
    const ChunkPos& sourceChunkPos,
    CarvingMask& mask,
    int caveBound,
    float (*thicknessFn)(RandomSource&),
    double tunnelYScale,
    CarveBlockFn carveBlockFn
) {
    const int maxDistance = (4 * 2 - 1) * 16;
    const int caveCount = random.nextInt(random.nextInt(random.nextInt(caveBound) + 1) + 1);

    for (int cave = 0; cave < caveCount; ++cave) {
        const double x = sourceChunkPos.x * 16 + random.nextInt(16);
        const double y = config.y->sample(random, context.world);
        const double z = sourceChunkPos.z * 16 + random.nextInt(16);
        const double horizontalRadiusMultiplier = config.horizontalRadiusMultiplier->sample(random);
        const double verticalRadiusMultiplier = config.verticalRadiusMultiplier->sample(random);
        const double floorLevel = config.floorLevel->sample(random);
        int tunnels = 1;
        if (random.nextInt(4) == 0) {
            const double yScale = config.yScale->sample(random);
            const float thickness = 1.0f + random.nextFloat() * 6.0f;
            const double horizontalRadius = 1.5 + mthSin(PI_F / 2.0f) * thickness;
            const double verticalRadius = horizontalRadius * yScale;
            carveEllipsoid(context, config, chunk, aquifer, x + 1.0, y, z,
                           horizontalRadius, verticalRadius, mask,
                           [floorLevel](const CarvingContext&, double xd, double yd, double zd, int) {
                               return caveShouldSkip(xd, yd, zd, floorLevel);
                           }, carveBlockFn);
            tunnels += random.nextInt(4);
        }

        for (int i = 0; i < tunnels; ++i) {
            const float horizontalRotation = random.nextFloat() * (PI_F * 2.0f);
            const float verticalRotation = (random.nextFloat() - 0.5f) / 4.0f;
            const float thickness = thicknessFn(random);
            const int distance = maxDistance - random.nextInt(maxDistance / 4);
            createCaveTunnel(context, config, chunk, random.nextLong(), aquifer, x, y, z,
                             horizontalRadiusMultiplier, verticalRadiusMultiplier, thickness,
                             horizontalRotation, verticalRotation, 0, distance, tunnelYScale,
                             mask, floorLevel, carveBlockFn);
        }
    }

    return true;
}

// Overworld carveCave — thin wrapper around carveCaveGeneric with overworld defaults.
bool carveCave(
    const CarvingContext& context,
    const CaveCarverConfiguration& config,
    LevelChunk& chunk,
    RandomSource& random,
    Aquifer& aquifer,
    const ChunkPos& sourceChunkPos,
    CarvingMask& mask
) {
    return carveCaveGeneric(context, config, chunk, random, aquifer, sourceChunkPos, mask,
                            /*caveBound*/ 15, /*thicknessFn*/ caveThickness,
                            /*tunnelYScale*/ 1.0, /*carveBlockFn*/ carveBlock);
}

// Nether carveCave — NetherWorldCarver overrides: caveBound=10, nether thickness,
// yScale=5.0, netherCarveBlock (lava below minY+31, cave_air above).
bool carveNetherCave(
    const CarvingContext& context,
    const CaveCarverConfiguration& config,
    LevelChunk& chunk,
    RandomSource& random,
    Aquifer& aquifer,
    const ChunkPos& sourceChunkPos,
    CarvingMask& mask
) {
    return carveCaveGeneric(context, config, chunk, random, aquifer, sourceChunkPos, mask,
                            /*caveBound*/ 10, /*thicknessFn*/ netherCaveThickness,
                            /*tunnelYScale*/ 5.0, /*carveBlockFn*/ netherCarveBlock);
}

std::vector<float> initCanyonWidthFactors(const CarvingContext& context, const CanyonCarverConfiguration& config, RandomSource& random) {
    std::vector<float> widthFactorPerHeight(static_cast<std::size_t>(context.genDepth));
    float widthFactor = 1.0f;
    for (int yIndex = 0; yIndex < context.genDepth; ++yIndex) {
        if (yIndex == 0 || random.nextInt(config.shape.widthSmoothness) == 0) {
            widthFactor = 1.0f + random.nextFloat() * random.nextFloat();
        }
        widthFactorPerHeight[static_cast<std::size_t>(yIndex)] = widthFactor * widthFactor;
    }
    return widthFactorPerHeight;
}

double updateCanyonVerticalRadius(const CanyonCarverConfiguration& config, RandomSource& random, double verticalRadius, float distance, float currentStep) {
    const float verticalMultiplier = 1.0f - std::abs(0.5f - currentStep / distance) * 2.0f;
    const float factor = config.shape.verticalRadiusDefaultFactor + config.shape.verticalRadiusCenterFactor * verticalMultiplier;
    return factor * verticalRadius * randomBetween(random, 0.75f, 1.0f);
}

void doCanyonCarve(
    const CarvingContext& context,
    const CanyonCarverConfiguration& config,
    LevelChunk& chunk,
    std::int64_t tunnelSeed,
    Aquifer& aquifer,
    double x,
    double y,
    double z,
    float thickness,
    float horizontalRotation,
    float verticalRotation,
    int step,
    int distance,
    double yScale,
    CarvingMask& mask
) {
    SingleThreadedRandomSource random(tunnelSeed);
    const std::vector<float> widthFactorPerHeight = initCanyonWidthFactors(context, config, random);
    float yRota = 0.0f;
    float xRota = 0.0f;

    for (int currentStep = step; currentStep < distance; ++currentStep) {
        double horizontalRadius = 1.5 + mthSin(currentStep * PI_F / distance) * thickness;
        double verticalRadius = horizontalRadius * yScale;
        horizontalRadius *= config.shape.horizontalRadiusFactor->sample(random);
        verticalRadius = updateCanyonVerticalRadius(config, random, verticalRadius, static_cast<float>(distance), static_cast<float>(currentStep));
        const float xc = mthCos(verticalRotation);
        const float xs = mthSin(verticalRotation);
        x += mthCos(horizontalRotation) * xc;
        y += xs;
        z += mthSin(horizontalRotation) * xc;
        verticalRotation *= 0.7f;
        verticalRotation += xRota * 0.05f;
        horizontalRotation += yRota * 0.05f;
        xRota *= 0.8f;
        yRota *= 0.5f;
        // RNG draw order must match Java exactly: subtraction operands are unsequenced
        // in C++ (GCC may reorder them), so draw all three floats into locals in Java's
        // source order before combining (a-b is not commutative, so order matters).
        const float xRotaR1 = random.nextFloat();
        const float xRotaR2 = random.nextFloat();
        const float xRotaR3 = random.nextFloat();
        xRota += (xRotaR1 - xRotaR2) * xRotaR3 * 2.0f;
        const float yRotaR1 = random.nextFloat();
        const float yRotaR2 = random.nextFloat();
        const float yRotaR3 = random.nextFloat();
        yRota += (yRotaR1 - yRotaR2) * yRotaR3 * 4.0f;
        if (random.nextInt(4) == 0) {
            continue;
        }
        if (!canReach(chunk.pos(), x, z, currentStep, distance, thickness)) {
            return;
        }
        carveEllipsoid(context, config, chunk, aquifer, x, y, z, horizontalRadius, verticalRadius, mask,
                       [&widthFactorPerHeight](const CarvingContext& ctx, double xd, double yd, double zd, int y1) {
                           const int yIndex = y1 - ctx.minGenY;
                           return (xd * xd + zd * zd) * widthFactorPerHeight[static_cast<std::size_t>(yIndex - 1)]
                                  + yd * yd / 6.0 >= 1.0;
                       }, carveBlock);
    }
}

bool carveCanyon(
    const CarvingContext& context,
    const CanyonCarverConfiguration& config,
    LevelChunk& chunk,
    RandomSource& random,
    Aquifer& aquifer,
    const ChunkPos& sourceChunkPos,
    CarvingMask& mask
) {
    const int maxDistance = (4 * 2 - 1) * 16;
    const double x = sourceChunkPos.x * 16 + random.nextInt(16);
    const int y = config.y->sample(random, context.world);
    const double z = sourceChunkPos.z * 16 + random.nextInt(16);
    const float horizontalRotation = random.nextFloat() * (PI_F * 2.0f);
    const float verticalRotation = config.verticalRotation->sample(random);
    const double yScale = config.yScale->sample(random);
    const float thickness = config.shape.thickness->sample(random);
    const int distance = static_cast<int>(maxDistance * config.shape.distanceFactor->sample(random));
    doCanyonCarve(context, config, chunk, random.nextLong(), aquifer, x, y, z, thickness,
                  horizontalRotation, verticalRotation, 0, distance, yScale, mask);
    return true;
}

CaveCarverConfiguration caveConfig() {
    CaveCarverConfiguration c;
    c.probability = 0.15f;
    c.y = std::make_shared<UniformHeight>(VerticalAnchors::aboveBottom(8), VerticalAnchors::absolute(180));
    c.yScale = UniformFloat::of(0.1f, 0.9f);
    c.lavaLevel = VerticalAnchors::aboveBottom(8);
    c.horizontalRadiusMultiplier = UniformFloat::of(0.7f, 1.4f);
    c.verticalRadiusMultiplier = UniformFloat::of(0.8f, 1.3f);
    c.floorLevel = UniformFloat::of(-1.0f, -0.4f);
    return c;
}

CaveCarverConfiguration caveExtraUndergroundConfig() {
    CaveCarverConfiguration c = caveConfig();
    c.probability = 0.07f;
    c.y = std::make_shared<UniformHeight>(VerticalAnchors::aboveBottom(8), VerticalAnchors::absolute(47));
    return c;
}

CanyonCarverConfiguration canyonConfig() {
    CanyonCarverConfiguration c;
    c.probability = 0.01f;
    c.y = std::make_shared<UniformHeight>(VerticalAnchors::absolute(10), VerticalAnchors::absolute(67));
    c.yScale = ConstantFloat::of(3.0f);
    c.lavaLevel = VerticalAnchors::aboveBottom(8);
    c.verticalRotation = UniformFloat::of(-0.125f, 0.125f);
    c.shape.distanceFactor = UniformFloat::of(0.75f, 1.0f);
    c.shape.thickness = TrapezoidFloat::of(0.0f, 6.0f, 2.0f);
    c.shape.widthSmoothness = 3;
    c.shape.horizontalRadiusFactor = UniformFloat::of(0.75f, 1.0f);
    c.shape.verticalRadiusDefaultFactor = 1.0f;
    c.shape.verticalRadiusCenterFactor = 0.0f;
    return c;
}

// Configured carver minecraft:nether_cave (26.1.2/data/minecraft/worldgen/
// configured_carver/nether_cave.json). Verbatim values from the JSON.
CaveCarverConfiguration netherCaveConfig() {
    CaveCarverConfiguration c;
    c.probability = 0.2f;
    c.y = std::make_shared<UniformHeight>(VerticalAnchors::absolute(0),
                                          VerticalAnchors::belowTop(1));
    c.yScale = ConstantFloat::of(0.5f);
    c.lavaLevel = VerticalAnchors::aboveBottom(10);
    c.horizontalRadiusMultiplier = ConstantFloat::of(1.0f);
    c.verticalRadiusMultiplier = ConstantFloat::of(1.0f);
    c.floorLevel = ConstantFloat::of(-0.7f);
    return c;
}

} // namespace

void applyOverworldCarvers(
    LevelChunk& chunk,
    std::int64_t worldSeed,
    const NoiseGeneratorSettings& settings,
    const NoiseRouter& router,
    std::shared_ptr<PositionalRandomFactory> aquiferRandom,
    const std::function<int(int, int)>& preliminarySurface,
    const TopMaterialGetter& topMaterial,
    std::vector<mc::BlockPos>* fluidUpdateMarks
) {
    const int minY = settings.noiseSettings.minY;
    const int height = settings.noiseSettings.height;
    CarvingContext context{
        WorldGenerationContext(minY, height),
        minY,
        height,
        topMaterial,
        fluidUpdateMarks
    };
    CarvingMask mask(height, minY);

    auto fluidPicker = Aquifer::createFluidPicker(settings);
    std::unique_ptr<Aquifer> aquifer = settings.isAquifersEnabled()
        ? Aquifer::create(preliminarySurface, chunk.pos().x * 16, chunk.pos().z * 16,
                          router, std::move(aquiferRandom), minY, height, std::move(fluidPicker))
        : Aquifer::createDisabled(std::move(fluidPicker));

    const CaveCarverConfiguration cave = caveConfig();
    const CaveCarverConfiguration caveExtra = caveExtraUndergroundConfig();
    const CanyonCarverConfiguration canyon = canyonConfig();
    WorldgenRandom random(std::make_shared<LegacyRandomSource>(0));

    // Diagnostic-only gate: MCPP_CARVER_ONLY=0|1|2 runs a single carver index so a
    // parity harness can isolate which carver diverges. Unset = all (production).
    const char* onlyEnv = std::getenv("MCPP_CARVER_ONLY");
    const int onlyIdx = onlyEnv ? std::atoi(onlyEnv) : -1;

    for (int dx = -8; dx <= 8; ++dx) {
        for (int dz = -8; dz <= 8; ++dz) {
            const ChunkPos source{ chunk.pos().x + dx, chunk.pos().z + dz };

            random.setLargeFeatureSeed(worldSeed + 0, source.x, source.z);
            if (random.nextFloat() <= cave.probability && (onlyIdx < 0 || onlyIdx == 0)) {
                carveCave(context, cave, chunk, random, *aquifer, source, mask);
            }

            random.setLargeFeatureSeed(worldSeed + 1, source.x, source.z);
            if (random.nextFloat() <= caveExtra.probability && (onlyIdx < 0 || onlyIdx == 1)) {
                carveCave(context, caveExtra, chunk, random, *aquifer, source, mask);
            }

            random.setLargeFeatureSeed(worldSeed + 2, source.x, source.z);
            if (random.nextFloat() <= canyon.probability && (onlyIdx < 0 || onlyIdx == 2)) {
                carveCanyon(context, canyon, chunk, random, *aquifer, source, mask);
            }
        }
    }

    chunk.computeHeightmap();
    chunk.meshDirty = true;
}

// Applies the vanilla nether configured carver: minecraft:nether_cave.
// The nether has only ONE carver (no canyon, no cave_extra). The carver uses
// NetherWorldCarver overrides: caveBound=10, nether thickness formula,
// yScale=5.0, carveBlock placing LAVA below minY+31 / CAVE_AIR above.
//
// Java: ChunkGenerator.applyCarvers for the nether dimension iterates the
// nether's configured carvers list (which contains only nether_cave). The
// scan window is 17×17 source chunks (getRange()=4 → 2*4+1=9 per side, but
// Java iterates -8..8 = 17 — matches overworld).
void applyNetherCarvers(
    LevelChunk& chunk,
    std::int64_t worldSeed,
    const NoiseGeneratorSettings& settings,
    const NoiseRouter& router,
    std::shared_ptr<PositionalRandomFactory> aquiferRandom,
    const std::function<int(int, int)>& preliminarySurface,
    const TopMaterialGetter& topMaterial,
    std::vector<mc::BlockPos>* fluidUpdateMarks
) {
    const int minY = settings.noiseSettings.minY;
    const int height = settings.noiseSettings.height;
    CarvingContext context{
        WorldGenerationContext(minY, height),
        minY,
        height,
        topMaterial,
        fluidUpdateMarks
    };
    CarvingMask mask(height, minY);

    auto fluidPicker = Aquifer::createFluidPicker(settings);
    std::unique_ptr<Aquifer> aquifer = settings.isAquifersEnabled()
        ? Aquifer::create(preliminarySurface, chunk.pos().x * 16, chunk.pos().z * 16,
                          router, std::move(aquiferRandom), minY, height, std::move(fluidPicker))
        : Aquifer::createDisabled(std::move(fluidPicker));

    const CaveCarverConfiguration netherCave = netherCaveConfig();
    WorldgenRandom random(std::make_shared<LegacyRandomSource>(0));

    for (int dx = -8; dx <= 8; ++dx) {
        for (int dz = -8; dz <= 8; ++dz) {
            const ChunkPos source{ chunk.pos().x + dx, chunk.pos().z + dz };

            // Java: setLargeFeatureSeed with the carver's seed offset (0 for the
            // first/only nether carver). The isStartChunk gate uses the carver's
            // probability (0.2 for nether_cave).
            random.setLargeFeatureSeed(worldSeed + 0, source.x, source.z);
            if (random.nextFloat() <= netherCave.probability) {
                carveNetherCave(context, netherCave, chunk, random, *aquifer, source, mask);
            }
        }
    }

    chunk.computeHeightmap();
    chunk.meshDirty = true;
}

} // namespace mc::levelgen::carver
