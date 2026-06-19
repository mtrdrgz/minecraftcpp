#include "../../../../assets/AssetManager.h"
#include "../../../../assets/AssetPack.h"
#include "../../block/BlockTags.h"
#include "BiomeFeatures.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

std::vector<std::pair<std::string, std::string>> readJsonEntries(std::string_view prefix) {
    std::vector<std::pair<std::string, std::string>> entries;
    auto& assets = mc::AssetManager::instance();
    for (const std::string& path : assets.list(prefix)) {
        if (!path.ends_with(".json")) {
            continue;
        }
        std::vector<uint8_t> bytes = assets.readRaw(path);
        if (!bytes.empty()) {
            entries.emplace_back(path, std::string(bytes.begin(), bytes.end()));
        }
    }
    return entries;
}

} // namespace

int main() {
    if (!mc::AssetPack::init()) {
        std::cerr << "AssetPack::init failed\n";
        return 1;
    }

    const auto biomeEntries = readJsonEntries("data/minecraft/worldgen/biome/");
    const auto tagEntries = readJsonEntries("data/minecraft/tags/block/");
    if (biomeEntries.size() != 65) {
        std::cerr << "Expected 65 embedded biome json files, got " << biomeEntries.size() << "\n";
        return 1;
    }
    if (tagEntries.size() != 244) {
        std::cerr << "Expected 244 embedded block tag json files, got " << tagEntries.size() << "\n";
        return 1;
    }

    const auto biomeFeatures = mc::levelgen::feature::BiomeFeatures::loadFromJsonEntries(biomeEntries);
    const auto blockTags = mc::block::BlockTags::loadFromJsonEntries(tagEntries);
    if (biomeFeatures.biomeCount() != 65 || blockTags.tagCount() != 244) {
        std::cerr << "Embedded worldgen parse mismatch: biomes=" << biomeFeatures.biomeCount()
                  << " tags=" << blockTags.tagCount() << "\n";
        return 1;
    }

    mc::AssetPack::shutdown();
    std::cout << "Embedded worldgen assets passed: 65 biomes, 244 block tags\n";
    return 0;
}
