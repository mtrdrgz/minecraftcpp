// Parity test for the BlockTags resolver and the VegetationBlock canSurvive rule.
// Ground truth: the canonical data/minecraft/tags/block/*.json, cross-checked by
// tools/block_tags_reference.py (an independent Python resolver).
//
//   default        -> hardcoded checks (SUPPORTS_VEGETATION + canSurvive matrix)
//   --dump <dir>   -> normalised "tag<TAB>members" view for the Python diff

#include "BlockBehaviour.h"
#include "BlockTags.h"

#include <iostream>
#include <set>
#include <string>
#include <vector>

using namespace mc::block;

namespace {

bool g_ok = true;
void check(bool cond, const std::string& label) {
    if (!cond) { g_ok = false; std::cerr << "FAIL: " << label << '\n'; }
}

std::string join(const std::set<std::string>& s) {
    std::string out;
    for (const auto& e : s) {
        if (!out.empty()) out += ',';
        out += e;
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::string dir = "26.1.2/data/minecraft/tags/block";
    bool dumpMode = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--dump") dumpMode = true;
        else dir = a;
    }

    BlockTags tags;
    try {
        tags = BlockTags::loadFromDirectory(dir);
    } catch (const std::exception& ex) {
        std::cerr << "load error: " << ex.what() << '\n';
        return 2;
    }

    if (dumpMode) {
        for (const std::string& tag : tags.tagNames()) {
            std::cout << tag << '\t' << join(tags.members(tag)) << '\n';
        }
        return 0;
    }

    // SUPPORTS_VEGETATION resolves to exactly these 11 blocks (the canSurvive
    // ground for grass/flowers in 26.1.2).
    const std::set<std::string> expectedSV = {
        "minecraft:coarse_dirt", "minecraft:dirt", "minecraft:farmland", "minecraft:grass_block",
        "minecraft:moss_block", "minecraft:mud", "minecraft:muddy_mangrove_roots", "minecraft:mycelium",
        "minecraft:pale_moss_block", "minecraft:podzol", "minecraft:rooted_dirt"
    };
    check(tags.members("minecraft:supports_vegetation") == expectedSV,
          "SUPPORTS_VEGETATION == 11 expected blocks (got " + join(tags.members("minecraft:supports_vegetation")) + ")");

    // VegetationBlock.canSurvive(below) matrix (short_grass / flowers).
    struct Case { const char* below; bool expected; };
    const Case cases[] = {
        { "minecraft:grass_block", true }, { "minecraft:dirt", true }, { "minecraft:coarse_dirt", true },
        { "minecraft:podzol", true }, { "minecraft:mycelium", true }, { "minecraft:moss_block", true },
        { "minecraft:mud", true }, { "minecraft:rooted_dirt", true }, { "minecraft:farmland", true },
        { "minecraft:stone", false }, { "minecraft:sand", false }, { "minecraft:gravel", false },
        { "minecraft:water", false }, { "minecraft:air", false }, { "minecraft:netherrack", false },
        { "minecraft:snow_block", false }, { "minecraft:stone_bricks", false },
    };
    for (const auto& c : cases) {
        check(vegetationBlockCanSurvive(c.below, tags) == c.expected,
              std::string("vegetationBlockCanSurvive(") + c.below + ") == " + (c.expected ? "true" : "false"));
        // DoublePlant (tall_grass) shares the VegetationBlock ground rule.
        check(canSurvive("minecraft:tall_grass", c.below, tags) == c.expected,
              std::string("canSurvive(tall_grass, ") + c.below + ")");
    }
    check(isDoublePlant("minecraft:tall_grass") && isDoublePlant("minecraft:large_fern") &&
              isDoublePlant("minecraft:sunflower") && !isDoublePlant("minecraft:short_grass"),
          "isDoublePlant set");

    // bush / sweet_berry_bush / firefly_bush are VegetationBlock -> SUPPORTS_VEGETATION.
    for (const char* plant : { "minecraft:bush", "minecraft:sweet_berry_bush", "minecraft:firefly_bush",
                               "minecraft:dandelion", "minecraft:poppy", "minecraft:lily_of_the_valley" }) {
        check(canSurvive(plant, "minecraft:grass_block", tags), std::string(plant) + " survives on grass_block");
        check(!canSurvive(plant, "minecraft:stone", tags), std::string(plant) + " not on stone");
    }
    // pumpkin / melon are full blocks: canSurvive is always true (ground gated by placement).
    check(canSurvive("minecraft:pumpkin", "minecraft:stone", tags) && canSurvive("minecraft:melon", "minecraft:air", tags),
          "full-block plants (pumpkin/melon) always canSurvive");

    // DryVegetationBlock (dead_bush / dry grass): below in SUPPORTS_DRY_VEGETATION
    // (sand, red_sand, terracotta family, + the SUPPORTS_VEGETATION grounds).
    const Case dryCases[] = {
        { "minecraft:sand", true }, { "minecraft:red_sand", true }, { "minecraft:terracotta", true },
        { "minecraft:white_terracotta", true }, { "minecraft:grass_block", true }, { "minecraft:dirt", true },
        { "minecraft:stone", false }, { "minecraft:gravel", false }, { "minecraft:water", false },
        { "minecraft:moss_block", true },
    };
    for (const auto& c : dryCases) {
        check(dryVegetationBlockCanSurvive(c.below, tags) == c.expected,
              std::string("dryVegetationBlockCanSurvive(") + c.below + ") == " + (c.expected ? "true" : "false"));
        check(canSurvive("minecraft:dead_bush", c.below, tags) == c.expected,
              std::string("canSurvive(dead_bush, ") + c.below + ")");
    }

    if (!g_ok) {
        std::cerr << "BlockTags / canSurvive parity checks FAILED\n";
        return 1;
    }
    std::cout << "BlockTags / canSurvive parity checks passed (" << tags.tagCount() << " tags)\n";
    return 0;
}
