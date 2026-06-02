// End-to-end integration demo: generate the `patch_grass_plain` placed feature
// on a flat test world and confirm grass is placed on the surface.
//
// This wires together pieces each verified 1:1 against the real decompiled code
// (WorldgenRandom, IntProvider, placement modifiers incl. heightmap & noise
// counts, BlockStateProvider, BlockTags + VegetationBlock.canSurvive,
// SimpleBlockFeature) through the ported PlacedFeature.place composition. It is
// a self-consistency check that the integrated pipeline produces vegetation
// (full end-to-end vs Java needs datapack tag binding, which is the documented
// boundary). Needs the local block-tag data dir as argv[1].

#include "../IntProvider.h"
#include "../RandomSource.h"
#include "../placement/HeightmapPlacement.h"
#include "../placement/NoiseCountPlacement.h"
#include "../placement/PlacedFeature.h"
#include "../placement/PlacementModifier.h"
#include "../../block/BlockBehaviour.h"
#include "../../block/BlockTags.h"
#include "Feature.h"
#include "stateproviders/BlockStateProvider.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace mc::levelgen;
using namespace mc::levelgen::placement;
using namespace mc::levelgen::feature;
using mc::levelgen::feature::stateproviders::SimpleStateProvider;
using mc::block::BlockTags;

namespace {

constexpr int kSurface = 64;

// Flat test world: grass_block below y=64, air at/above.
class FlatWorld final : public WorldGenLevel {
public:
    explicit FlatWorld(const BlockTags& tags) : m_tags(tags) {}

    int getHeight(Heightmap::Types, int, int) const override { return kSurface; }
    int getMinY() const override { return -64; }
    std::string getBlockState(BlockPos pos) const override {
        return pos.y < kSurface ? "minecraft:grass_block" : "minecraft:air";
    }
    bool isEmptyBlock(BlockPos pos) const override { return getBlockState(pos) == "minecraft:air"; }
    bool canSurvive(const std::string& state, BlockPos pos) const override {
        // Dispatch by plant family (VegetationBlock/DoublePlant/DryVegetation).
        return mc::block::canSurvive(state, getBlockState(BlockPos{ pos.x, pos.y - 1, pos.z }), m_tags);
    }
    void setBlock(BlockPos pos, const std::string& state, int) override { writes.push_back({ pos, state }); }

    struct Write { BlockPos pos; std::string state; };
    std::vector<Write> writes;

private:
    const BlockTags& m_tags;
};

} // namespace

int main(int argc, char** argv) {
    const std::string tagDir = argc > 1 ? argv[1] : "26.1.2/data/minecraft/tags/block";
    BlockTags tags;
    try {
        tags = BlockTags::loadFromDirectory(tagDir);
    } catch (const std::exception& ex) {
        std::cerr << "load error: " << ex.what() << '\n';
        return 2;
    }

    // The `grass` configured feature: simple_block(simple_state short_grass).
    SimpleBlockConfiguration grassConfig{ SimpleStateProvider::of("minecraft:short_grass"), false };
    PlacedFeature::FeaturePlacer grassFeature = [&grassConfig](WorldGenLevel& lvl, RandomSource& rnd, BlockPos pos) {
        SimpleBlockFeature feature;
        FeaturePlaceContext<SimpleBlockConfiguration> ctx{ &lvl, &rnd, pos, &grassConfig };
        return feature.place(ctx);
    };

    // patch_grass_plain placement chain (1:1 with the data file).
    std::vector<std::shared_ptr<const PlacementModifier>> placement = {
        std::make_shared<NoiseThresholdCountPlacement>(-0.8, 5, 10),
        std::make_shared<InSquarePlacement>(),
        std::make_shared<HeightmapPlacement>(Heightmap::Types::WORLD_SURFACE_WG),
        std::make_shared<BiomeFilter>([](WorldGenLevel&, BlockPos) { return true; }), // single-biome
        std::make_shared<CountPlacement>(mc::valueproviders::ConstantInt::of(32)),
        std::make_shared<RandomOffsetPlacement>(mc::valueproviders::TrapezoidInt::of(-7, 7, 0),
                                                mc::valueproviders::TrapezoidInt::of(-3, 3, 0)),
        std::make_shared<BlockPredicateFilter>([&tags](WorldGenLevel& lvl, BlockPos pos) {
            return tags.isInTag(lvl.getBlockState(pos), "minecraft:air"); // matching_block_tag air
        }),
    };
    PlacedFeature grass(grassFeature, placement);

    FlatWorld world(tags);
    // Population RNG, seeded like ChunkGenerator.applyBiomeDecoration.
    WorldgenRandom random(std::make_shared<XoroshiroRandomSource>(0));
    const BlockPos origin{ 0, 0, 0 };
    const long long decorationSeed = random.setDecorationSeed(42LL, origin.x, origin.z);
    random.setFeatureSeed(decorationSeed, 0, 9 /* VEGETAL_DECORATION */);

    const bool placedAny = grass.place(world, random, origin, -64, 384);

    bool ok = placedAny && !world.writes.empty();
    int badState = 0, badY = 0;
    for (const auto& w : world.writes) {
        if (w.state != "minecraft:short_grass") ++badState;
        if (w.pos.y != kSurface) ++badY;
    }
    ok = ok && badState == 0 && badY == 0;

    std::cout << "patch_grass_plain demo: placed=" << (placedAny ? "true" : "false")
              << " grass_written=" << world.writes.size()
              << " wrong_state=" << badState << " wrong_y=" << badY << '\n';
    if (!world.writes.empty()) {
        const auto& w = world.writes.front();
        std::cout << "  e.g. " << w.state << " @ " << w.pos.x << "," << w.pos.y << "," << w.pos.z << '\n';
    }
    // --- Double plant (tall_grass): each placement writes two blocks (lower+upper) ---
    SimpleBlockConfiguration tallGrassConfig{ SimpleStateProvider::of("minecraft:tall_grass"), false };
    PlacedFeature::FeaturePlacer tallGrassFeature = [&tallGrassConfig](WorldGenLevel& lvl, RandomSource& rnd, BlockPos pos) {
        SimpleBlockFeature feature;
        FeaturePlaceContext<SimpleBlockConfiguration> ctx{ &lvl, &rnd, pos, &tallGrassConfig };
        return feature.place(ctx);
    };
    PlacedFeature tallGrass(tallGrassFeature, placement);

    FlatWorld tgWorld(tags);
    WorldgenRandom tgRandom(std::make_shared<XoroshiroRandomSource>(0));
    const long long tgSeed = tgRandom.setDecorationSeed(42LL, origin.x, origin.z);
    tgRandom.setFeatureSeed(tgSeed, 1, 9);
    const bool tgPlaced = tallGrass.place(tgWorld, tgRandom, origin, -64, 384);

    int lower = 0, upper = 0, tgBad = 0;
    for (const auto& w : tgWorld.writes) {
        if (w.state == "minecraft:tall_grass[half=lower]" && w.pos.y == kSurface) ++lower;
        else if (w.state == "minecraft:tall_grass[half=upper]" && w.pos.y == kSurface + 1) ++upper;
        else ++tgBad;
    }
    const bool tgOk = tgPlaced && lower > 0 && lower == upper && tgBad == 0;
    std::cout << "patch_tall_grass demo: placed=" << (tgPlaced ? "true" : "false")
              << " lower=" << lower << " upper=" << upper << " bad=" << tgBad << '\n';

    ok = ok && tgOk;
    if (!ok) {
        std::cerr << "Vegetation demo FAILED\n";
        return 1;
    }
    std::cout << "Vegetation demo passed: grass + tall_grass (double plant) generated on the surface\n";
    return 0;
}
