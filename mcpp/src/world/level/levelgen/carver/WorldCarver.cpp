#include "WorldCarver.h"

#include "../Aquifer.h"
#include "../FloatProvider.h"
#include "../VerticalAnchor.h"
#include "../WorldGenerationContext.h"
#include "../heightproviders/HeightProvider.h"
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
constexpr float SIN_SCALE_F = 10430.378f;

int floorToInt(double v) {
    return static_cast<int>(std::floor(v));
}

float mthSin(float v) {
    static const std::array<float, 65536> table = [] {
        std::array<float, 65536> out{};
        for (int i = 0; i < static_cast<int>(out.size()); ++i) {
            out[static_cast<std::size_t>(i)] = static_cast<float>(std::sin(i * PI_D * 2.0 / 65536.0));
        }
        return out;
    }();
    const auto idx = static_cast<std::int32_t>(v * SIN_SCALE_F);
    return table[static_cast<std::size_t>(static_cast<std::uint32_t>(idx) & 65535U)];
}

float mthCos(float v) {
    static const std::array<float, 65536> table = [] {
        std::array<float, 65536> out{};
        for (int i = 0; i < static_cast<int>(out.size()); ++i) {
            out[static_cast<std::size_t>(i)] = static_cast<float>(std::sin(i * PI_D * 2.0 / 65536.0));
        }
        return out;
    }();
    const auto idx = static_cast<std::int32_t>(v * SIN_SCALE_F + 16384.0f);
    return table[static_cast<std::size_t>(static_cast<std::uint32_t>(idx) & 65535U)];
}

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
    if (hasGrass && chunk.getBlock(x, y - 1, z) == dirt && context.topMaterial) {
        const bool underFluid = *carved == water || *carved == lava;
        std::optional<std::uint32_t> top = context.topMaterial(
            chunk,
            x,
            y - 1,
            z,
            underFluid);
        if (top) {
            chunk.setBlock(x, y - 1, z, *top);
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
    Skip&& shouldSkip
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
                carved |= carveBlock(context, config, chunk, worldX, worldY, worldZ, aquifer, hasGrass);
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
    double floorLevel
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
        xRota += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 2.0f;
        yRota += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 4.0f;

        if (currentStep == splitPoint && thickness > 1.0f) {
            createCaveTunnel(context, config, chunk, random.nextLong(), aquifer, x, y, z,
                             horizontalRadiusMultiplier, verticalRadiusMultiplier,
                             random.nextFloat() * 0.5f + 0.5f,
                             horizontalRotation - PI_F / 2.0f, verticalRotation / 3.0f,
                             currentStep, dist, 1.0, mask, floorLevel);
            createCaveTunnel(context, config, chunk, random.nextLong(), aquifer, x, y, z,
                             horizontalRadiusMultiplier, verticalRadiusMultiplier,
                             random.nextFloat() * 0.5f + 0.5f,
                             horizontalRotation + PI_F / 2.0f, verticalRotation / 3.0f,
                             currentStep, dist, 1.0, mask, floorLevel);
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
                       });
    }
}

float caveThickness(RandomSource& random) {
    float thickness = random.nextFloat() * 2.0f + random.nextFloat();
    if (random.nextInt(10) == 0) {
        thickness *= random.nextFloat() * random.nextFloat() * 3.0f + 1.0f;
    }
    return thickness;
}

bool carveCave(
    const CarvingContext& context,
    const CaveCarverConfiguration& config,
    LevelChunk& chunk,
    RandomSource& random,
    Aquifer& aquifer,
    const ChunkPos& sourceChunkPos,
    CarvingMask& mask
) {
    const int maxDistance = (4 * 2 - 1) * 16;
    const int caveCount = random.nextInt(random.nextInt(random.nextInt(15) + 1) + 1);

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
                           });
            tunnels += random.nextInt(4);
        }

        for (int i = 0; i < tunnels; ++i) {
            const float horizontalRotation = random.nextFloat() * (PI_F * 2.0f);
            const float verticalRotation = (random.nextFloat() - 0.5f) / 4.0f;
            const float thickness = caveThickness(random);
            const int distance = maxDistance - random.nextInt(maxDistance / 4);
            createCaveTunnel(context, config, chunk, random.nextLong(), aquifer, x, y, z,
                             horizontalRadiusMultiplier, verticalRadiusMultiplier, thickness,
                             horizontalRotation, verticalRotation, 0, distance, 1.0, mask, floorLevel);
        }
    }

    return true;
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
        xRota += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 2.0f;
        yRota += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 4.0f;
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
                       });
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

} // namespace

void applyOverworldCarvers(
    LevelChunk& chunk,
    std::int64_t worldSeed,
    const NoiseGeneratorSettings& settings,
    const NoiseRouter& router,
    std::shared_ptr<PositionalRandomFactory> aquiferRandom,
    const std::function<int(int, int)>& preliminarySurface,
    const TopMaterialGetter& topMaterial
) {
    const int minY = settings.noiseSettings.minY;
    const int height = settings.noiseSettings.height;
    CarvingContext context{
        WorldGenerationContext(minY, height),
        minY,
        height,
        topMaterial
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

} // namespace mc::levelgen::carver
