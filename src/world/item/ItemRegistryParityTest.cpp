// Item REGISTRY parity vs the real 26.1.2 BuiltInRegistries.ITEM
// (tools/ItemRegistryParity.java ground truth). Compares the engine's item
// table (mcpp/src/assets/items.json) per id: name, max stack, max damage,
// BlockItem linkage, food marker.
//
//   item_registry_parity [--cases mcpp/build/item_registry.tsv]
//                        [--items mcpp/src/assets/items.json]
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::string casesPath = "build/item_registry.tsv";
    std::string itemsPath = "src/assets/items.json";
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];
        if (std::string(argv[i]) == "--items") itemsPath = argv[i + 1];
    }

    std::ifstream jf(itemsPath, std::ios::binary);
    if (!jf) { std::cerr << "cannot open " << itemsPath << "\n"; return 2; }
    nlohmann::json engine;
    jf >> engine;
    const auto& items = engine.at("items");

    struct Truth { std::string name; int stack, damage; std::string block; bool food; };
    std::vector<Truth> truth;
    {
        std::ifstream f(casesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }
        std::string line;
        auto strip = [](std::string s) { return s.rfind("minecraft:", 0) == 0 ? s.substr(10) : s; };
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream ss(line);
            std::string kind; std::getline(ss, kind, '\t');
            if (kind != "ITEM") continue;
            std::string id, key, stack, damage, block, food;
            std::getline(ss, id, '\t'); std::getline(ss, key, '\t'); std::getline(ss, stack, '\t');
            std::getline(ss, damage, '\t'); std::getline(ss, block, '\t'); std::getline(ss, food, '\t');
            truth.push_back({ strip(key), std::stoi(stack), std::stoi(damage),
                              block == "-" ? "" : strip(block), food == "1" });
        }
    }

    std::size_t mismatches = 0;
    const std::size_t n = std::min(items.size(), truth.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto& e = items[i];
        const Truth& t = truth[i];
        if (e.at("name").get<std::string>() != t.name
            || e.at("max_stack").get<int>() != t.stack
            || e.at("max_damage").get<int>() != t.damage
            || e.at("block").get<std::string>() != t.block
            || e.at("food").get<bool>() != t.food) {
            if (mismatches < 5)
                std::cerr << "MISMATCH id=" << i << " engine=" << e.at("name").get<std::string>()
                          << " truth=" << t.name << "\n";
            ++mismatches;
        }
    }
    if (items.size() != truth.size()) ++mismatches;

    std::cout << "ItemRegistry engine_items=" << items.size()
              << " truth_items=" << truth.size()
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
