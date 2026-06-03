#include "BiomeDecorator.h"

#include "../RandomSource.h"
#include "../IntProvider.h"
#include "../placement/PlacementModifier.h"
#include "../placement/HeightmapPlacement.h"
#include "../placement/NoiseCountPlacement.h"
#include "../placement/PlacedFeature.h"
#include "Feature.h"
#include "TreeGen.h"
#include "stateproviders/BlockStateProvider.h"
#include "stateproviders/NoiseBasedStateProviders.h"
#include "../../block/BlockBehaviour.h"
#include "../../block/BlockState.h"
#include "../../block/BlockStates.h"
#include "../../block/Blocks.h"

#include <map>
#include <memory>
#include <unordered_set>
#include <vector>

namespace mc::levelgen::feature {

using namespace mc::levelgen::placement;
using SP = stateproviders::SimpleStateProvider;
using NTP = stateproviders::NoiseThresholdProvider;
using mc::valueproviders::ConstantInt;
using mc::valueproviders::TrapezoidInt;

namespace {

// ── WorldGenLevel view over a single LevelChunk ──────────────────────────────
// Reads/writes the chunk as canonical block-state id strings; out-of-chunk
// positions read as air and drop on write (single-chunk decoration, no neighbour
// region yet). Carries the biome getter + the feature currently being placed so
// the minecraft:biome filter can check "does the biome at this pos list me".
struct ChunkWGL final : WorldGenLevel {
    LevelChunk* chunk = nullptr;
    const mc::block::BlockTags* tags = nullptr;
    const BiomeFeatures* biomeFeatures = nullptr;
    const std::function<std::string(int, int, int)>* biomeGetter = nullptr;
    int curStep = 0;
    std::string curFeatureKey;

    bool inChunk(BlockPos p) const {
        const ChunkPos cp = chunk->pos();
        return p.x >= cp.x * 16 && p.x < cp.x * 16 + 16 && p.z >= cp.z * 16 && p.z < cp.z * 16 + 16
               && p.y >= CHUNK_MIN_Y && p.y < CHUNK_MAX_Y;
    }

    int getMinY() const override { return CHUNK_MIN_Y; }

    int getHeight(Heightmap::Types, int x, int z) const override {
        const ChunkPos cp = chunk->pos();
        if (x < cp.x * 16 || x >= cp.x * 16 + 16 || z < cp.z * 16 || z >= cp.z * 16 + 16) return CHUNK_MIN_Y;
        const int h = chunk->heightmap(x & 15, z & 15); // top non-air block
        return h < CHUNK_MIN_Y ? CHUNK_MIN_Y : h + 1;   // first free space above
    }

    std::string nameAt(BlockPos p) const {
        if (!inChunk(p)) return "minecraft:air";
        const mc::BlockState* s = mc::getBlockState(chunk->getBlock(p.x, p.y, p.z));
        return (s && s->block) ? ("minecraft:" + s->block->name) : "minecraft:air";
    }

    std::string getBlockState(BlockPos p) const override { return nameAt(p); }
    bool isEmptyBlock(BlockPos p) const override { return nameAt(p) == "minecraft:air"; }

    bool canSurvive(const std::string& state, BlockPos p) const override {
        return mc::block::canSurvive(state, nameAt(BlockPos{ p.x, p.y - 1, p.z }), *tags);
    }

    void setBlock(BlockPos p, const std::string& state, int) override {
        if (!inChunk(p)) return; // clamp to this chunk
        // canonical state -> bare block name (registry keys carry no namespace).
        std::string name = mc::block::blockName(state);
        if (auto c = name.find(':'); c != std::string::npos) name = name.substr(c + 1);
        const uint32_t id = getDefaultBlockStateId(name, 0);
        if (id != 0) chunk->setBlock(p.x, p.y, p.z, id);
    }
};

// minecraft:biome filter: keep the position iff the biome there lists the feature
// currently being placed (exactly Java's per-biome gating).
bool biomeAllows(WorldGenLevel& level, BlockPos p) {
    auto& w = static_cast<ChunkWGL&>(level);
    const std::string biome = (*w.biomeGetter)(p.x, p.y, p.z);
    return w.biomeFeatures->biomeHasFeature(biome, w.curStep, w.curFeatureKey);
}

bool airFilter(WorldGenLevel& level, BlockPos p) {
    auto& w = static_cast<ChunkWGL&>(level);
    return w.tags->isInTag(level.getBlockState(p), "minecraft:air");
}

// minecraft:count with a weighted_list IntProvider (e.g. trees: {10:w9},{11:w1}).
// Mirrors SimpleWeightedRandomList.getRandomValue: nextInt(total), walk cumulative.
class WeightedCountPlacement final : public PlacementModifier {
public:
    explicit WeightedCountPlacement(std::vector<std::pair<int, int>> dist)
        : m_dist(std::move(dist)) {
        for (auto& d : m_dist) m_total += d.second;
    }
    std::vector<BlockPos> getPositions(PlacementContext*, RandomSource& r, BlockPos o) const override {
        int n = m_dist.empty() ? 0 : m_dist.back().first;
        if (m_total > 0) {
            int x = r.nextInt(m_total), cum = 0;
            for (auto& d : m_dist) { cum += d.second; if (x < cum) { n = d.first; break; } }
        }
        return std::vector<BlockPos>(n < 0 ? 0 : static_cast<std::size_t>(n), o);
    }
private:
    std::vector<std::pair<int, int>> m_dist;
    int m_total = 0;
};

// minecraft:surface_water_depth_filter — keep origin iff (surface - ocean_floor)
// water depth <= max. With the single-heightmap chunk view this is 0 on land.
class SurfaceWaterDepthFilter final : public PlacementModifier {
public:
    explicit SurfaceWaterDepthFilter(int maxDepth) : m_max(maxDepth) {}
    std::vector<BlockPos> getPositions(PlacementContext* ctx, RandomSource&, BlockPos o) const override {
        WorldGenLevel* l = ctx->getLevel();
        const int depth = l->getHeight(Heightmap::Types::WORLD_SURFACE_WG, o.x, o.z)
                          - l->getHeight(Heightmap::Types::OCEAN_FLOOR_WG, o.x, o.z);
        return depth <= m_max ? std::vector<BlockPos>{ o } : std::vector<BlockPos>{};
    }
private:
    int m_max;
};

// Tree placer for trees_birch_and_oak_leaf_litter: the vanilla random_selector
// (oak default / birch 20% / fancy oak 10% / rare fallen logs), placing one tree
// at the position via the ported TreeFeature (placeTree) over the chunk.
PlacedFeature::FeaturePlacer birchAndOakTreePlacer() {
    return [](WorldGenLevel& l, RandomSource& r, BlockPos p) -> bool {
        static const TreeConfig oak = makeOakConfig();
        static const TreeConfig birch = makeBirchConfig();
        static const TreeConfig fancy = makeFancyOakConfig();
        const TreeConfig* cfg;
        if (r.nextFloat() < 0.0025f) return false;        // fallen_birch_tree (not modelled)
        else if (r.nextFloat() < 0.2f) cfg = &birch;
        else if (r.nextFloat() < 0.1f) cfg = &fancy;
        else if (r.nextFloat() < 0.0125f) return false;   // fallen_oak_tree (not modelled)
        else cfg = &oak;
        auto& w = static_cast<ChunkWGL&>(l);
        const ChunkPos cp = w.chunk->pos();
        TreeWorld tw{ *w.chunk, cp.x * 16, cp.z * 16 };
        return placeTree(tw, r, p.x, p.y, p.z, *cfg);
    };
}

// Build a SimpleBlock feature placer for a given state provider.
PlacedFeature::FeaturePlacer simpleBlockPlacer(stateproviders::BlockStateProviderPtr provider) {
    return [cfg = std::make_shared<SimpleBlockConfiguration>(SimpleBlockConfiguration{ std::move(provider), false })](
               WorldGenLevel& l, RandomSource& r, BlockPos p) {
        SimpleBlockFeature f;
        FeaturePlaceContext<SimpleBlockConfiguration> c{ &l, &r, p, cfg.get() };
        return f.place(c);
    };
}

// The ported VEGETAL_DECORATION placed features, with chains copied 1:1 from
// data/minecraft/worldgen/placed_feature/{patch_grass_plain,flower_plains}.json.
std::map<std::string, std::shared_ptr<PlacedFeature>> buildRegistry() {
    std::map<std::string, std::shared_ptr<PlacedFeature>> reg;

    // patch_grass_plain -> configured "grass" = simple_block(short_grass)
    reg["minecraft:patch_grass_plain"] = std::make_shared<PlacedFeature>(
        simpleBlockPlacer(SP::of("minecraft:short_grass")),
        std::vector<std::shared_ptr<const PlacementModifier>>{
            std::make_shared<NoiseThresholdCountPlacement>(-0.8, 5, 10),
            std::make_shared<InSquarePlacement>(),
            std::make_shared<HeightmapPlacement>(Heightmap::Types::WORLD_SURFACE_WG),
            std::make_shared<BiomeFilter>(biomeAllows),
            std::make_shared<CountPlacement>(ConstantInt::of(32)),
            std::make_shared<RandomOffsetPlacement>(TrapezoidInt::of(-7, 7, 0), TrapezoidInt::of(-3, 3, 0)),
            std::make_shared<BlockPredicateFilter>(airFilter),
        });

    // flower_plains -> configured "flower_plain" = simple_block(noise_threshold_provider)
    auto flowerProvider = std::make_shared<NTP>(
        2345LL, NoiseParameters{ 0, { 1.0 } }, 0.005f, -0.8f, 0.33333334f, "minecraft:dandelion",
        std::vector<std::string>{ "minecraft:orange_tulip", "minecraft:red_tulip", "minecraft:pink_tulip", "minecraft:white_tulip" },
        std::vector<std::string>{ "minecraft:poppy", "minecraft:azure_bluet", "minecraft:oxeye_daisy", "minecraft:cornflower" });
    reg["minecraft:flower_plains"] = std::make_shared<PlacedFeature>(
        simpleBlockPlacer(flowerProvider),
        std::vector<std::shared_ptr<const PlacementModifier>>{
            std::make_shared<NoiseThresholdCountPlacement>(-0.8, 15, 4),
            std::make_shared<RarityFilter>(32),
            std::make_shared<InSquarePlacement>(),
            std::make_shared<HeightmapPlacement>(Heightmap::Types::MOTION_BLOCKING),
            std::make_shared<BiomeFilter>(biomeAllows),
            std::make_shared<CountPlacement>(ConstantInt::of(64)),
            std::make_shared<RandomOffsetPlacement>(TrapezoidInt::of(-6, 6, 0), TrapezoidInt::of(-2, 2, 0)),
            std::make_shared<BlockPredicateFilter>(airFilter),
        });

    // ── Forest features (the region is mostly forest) ───────────────────────

    // trees_birch_and_oak_leaf_litter: ~10 trees/chunk (weighted 9:1 -> 10/11),
    // chain from placed_feature JSON (no would_survive in the leaf-litter variant).
    reg["minecraft:trees_birch_and_oak_leaf_litter"] = std::make_shared<PlacedFeature>(
        birchAndOakTreePlacer(),
        std::vector<std::shared_ptr<const PlacementModifier>>{
            std::make_shared<WeightedCountPlacement>(std::vector<std::pair<int, int>>{ { 10, 9 }, { 11, 1 } }),
            std::make_shared<InSquarePlacement>(),
            std::make_shared<SurfaceWaterDepthFilter>(0),
            std::make_shared<HeightmapPlacement>(Heightmap::Types::OCEAN_FLOOR),
            std::make_shared<BiomeFilter>(biomeAllows),
        });

    // patch_grass_forest -> configured "grass" (short_grass); count 2 then count 32.
    reg["minecraft:patch_grass_forest"] = std::make_shared<PlacedFeature>(
        simpleBlockPlacer(SP::of("minecraft:short_grass")),
        std::vector<std::shared_ptr<const PlacementModifier>>{
            std::make_shared<CountPlacement>(ConstantInt::of(2)),
            std::make_shared<InSquarePlacement>(),
            std::make_shared<HeightmapPlacement>(Heightmap::Types::WORLD_SURFACE_WG),
            std::make_shared<BiomeFilter>(biomeAllows),
            std::make_shared<CountPlacement>(ConstantInt::of(32)),
            std::make_shared<RandomOffsetPlacement>(TrapezoidInt::of(-7, 7, 0), TrapezoidInt::of(-3, 3, 0)),
            std::make_shared<BlockPredicateFilter>(airFilter),
        });

    // flower_default: rarity_filter 32, in_square, heightmap, biome. Reuses the
    // standard overworld flower provider (dandelion/poppy/tulips/...).
    auto flowerProvider2 = std::make_shared<NTP>(
        2345LL, NoiseParameters{ 0, { 1.0 } }, 0.005f, -0.8f, 0.33333334f, "minecraft:dandelion",
        std::vector<std::string>{ "minecraft:orange_tulip", "minecraft:red_tulip", "minecraft:pink_tulip", "minecraft:white_tulip" },
        std::vector<std::string>{ "minecraft:poppy", "minecraft:azure_bluet", "minecraft:oxeye_daisy", "minecraft:cornflower" });
    reg["minecraft:flower_default"] = std::make_shared<PlacedFeature>(
        simpleBlockPlacer(flowerProvider2),
        std::vector<std::shared_ptr<const PlacementModifier>>{
            std::make_shared<RarityFilter>(32),
            std::make_shared<InSquarePlacement>(),
            std::make_shared<HeightmapPlacement>(Heightmap::Types::MOTION_BLOCKING),
            std::make_shared<BiomeFilter>(biomeAllows),
        });

    return reg;
}

const std::map<std::string, std::shared_ptr<PlacedFeature>>& registry() {
    static const std::map<std::string, std::shared_ptr<PlacedFeature>> reg = buildRegistry();
    return reg;
}

} // namespace

void applyBiomeDecoration(LevelChunk& chunk, std::int64_t worldSeed,
                          const std::function<std::string(int, int, int)>& biomeGetter,
                          const BiomeFeatures& biomeFeatures,
                          const mc::block::BlockTags& tags) {
    const ChunkPos cp = chunk.pos();
    const int minX = cp.x * 16, minZ = cp.z * 16;

    ChunkWGL level;
    level.chunk = &chunk;
    level.tags = &tags;
    level.biomeFeatures = &biomeFeatures;
    level.biomeGetter = &biomeGetter;

    // Population RNG, seeded exactly like ChunkGenerator.applyBiomeDecoration.
    WorldgenRandom random(std::make_shared<XoroshiroRandomSource>((int64_t)0));
    const long long decorationSeed = random.setDecorationSeed(worldSeed, minX, minZ);

    // Biomes present in this chunk (first-encounter order), used to assemble each
    // step's merged feature list (a stand-in for Java's cross-biome FeatureSorter;
    // exact for single-biome chunks, which is the common case).
    std::vector<std::string> present;
    std::unordered_set<std::string> seen;
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) {
            const int y = chunk.heightmap(x, z);
            if (y < CHUNK_MIN_Y) continue;
            std::string b = biomeGetter(minX + x, y, minZ + z);
            if (seen.insert(b).second && biomeFeatures.hasBiome(b)) present.push_back(std::move(b));
        }

    const BlockPos origin{ minX, 0, minZ };
    const auto& reg = registry();

    for (int step = 0; step < GenerationStep::COUNT; ++step) {
        // Merge the per-biome ordered feature lists for this step (dedup, keep order).
        std::vector<std::string> merged;
        std::unordered_set<std::string> mergedSeen;
        for (const std::string& b : present)
            for (const std::string& key : biomeFeatures.featuresForStep(b, step))
                if (mergedSeen.insert(key).second) merged.push_back(key);

        for (std::size_t index = 0; index < merged.size(); ++index) {
            // Every feature consumes its seed index, so ported features line up
            // with vanilla even when neighbours in the list aren't ported yet.
            random.setFeatureSeed(decorationSeed, static_cast<int>(index), step);
            auto it = reg.find(merged[index]);
            if (it == reg.end()) continue; // not ported yet
            level.curStep = step;
            level.curFeatureKey = merged[index];
            it->second->place(level, random, origin, CHUNK_MIN_Y, CHUNK_MAX_Y - CHUNK_MIN_Y);
        }
    }
}

} // namespace mc::levelgen::feature
