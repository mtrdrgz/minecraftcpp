// Parity test for mc::saveddata::maps::MapDecorationType /
// MapDecorationTypes built-in registrations vs Java ground truth.
//
// Reads the TSV emitted by MapDecorationTypeParity.java and compares
// value-for-value (and bit-for-bit where applicable; all fields here are
// ints/bools/strings so plain equality is exact).
#include "world/level/saveddata/maps/MapDecorationType.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace maps = mc::saveddata::maps;

static std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: MapDecorationTypeParityTest --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0;
    long mism = 0;
    std::string line;
    // Track which built-in entries the GT actually visited, in order, to also verify
    // iteration order and total count match the ported array.
    std::size_t entryIndex = 0;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        std::vector<std::string> p = split_tabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "CONST") {
            // CONST <NO_MAP_COLOR>
            ++cases;
            int expected = std::stoi(p[1]);
            if (maps::NO_MAP_COLOR != expected) {
                ++mism;
                std::cerr << "CONST mismatch NO_MAP_COLOR expected=" << expected
                          << " got=" << maps::NO_MAP_COLOR << "\n";
            }
        } else if (tag == "ENTRY") {
            // ENTRY <regNs> <regPath> <assetNs> <assetPath> <showOnItemFrame>
            //       <mapColor> <explorationMapElement> <trackCount> <hasMapColor>
            ++cases;
            const std::string& regNs       = p[1];
            const std::string& regPath      = p[2];
            const std::string& assetNs      = p[3];
            const std::string& assetPath    = p[4];
            int showOnItemFrame             = std::stoi(p[5]);
            int mapColor                    = std::stoi(p[6]);
            int explorationMapElement       = std::stoi(p[7]);
            int trackCount                  = std::stoi(p[8]);
            int hasMapColor                 = std::stoi(p[9]);

            // Order check: GT iteration order must match declaration order.
            if (entryIndex >= maps::BUILTIN_MAP_DECORATION_TYPES.size()) {
                ++mism;
                std::cerr << "ENTRY mismatch: GT has more entries than ported ("
                          << regPath << ")\n";
                continue;
            }
            const maps::MapDecorationEntry& e = maps::BUILTIN_MAP_DECORATION_TYPES[entryIndex];
            ++entryIndex;

            bool ok = true;
            // Both registration id and asset id use the "minecraft" namespace.
            if (regNs != std::string(maps::DEFAULT_NAMESPACE)) ok = false;
            if (assetNs != std::string(maps::DEFAULT_NAMESPACE)) ok = false;
            if (regPath != std::string(e.name)) ok = false;
            if (assetPath != std::string(e.type.assetName)) ok = false;
            if ((showOnItemFrame != 0) != e.type.showOnItemFrame) ok = false;
            if (mapColor != e.type.mapColor) ok = false;
            if ((explorationMapElement != 0) != e.type.explorationMapElement) ok = false;
            if ((trackCount != 0) != e.type.trackCount) ok = false;
            // hasMapColor() is recomputed on the C++ side and must match GT.
            if ((hasMapColor != 0) != e.type.hasMapColor()) ok = false;

            if (!ok) {
                ++mism;
                std::cerr << "ENTRY mismatch idx=" << (entryIndex - 1)
                          << " reg=" << regNs << ":" << regPath
                          << " asset=" << assetNs << ":" << assetPath
                          << " showOnItemFrame=" << showOnItemFrame
                          << " mapColor=" << mapColor
                          << " explorationMapElement=" << explorationMapElement
                          << " trackCount=" << trackCount
                          << " hasMapColor=" << hasMapColor
                          << "  (ported: " << std::string(maps::DEFAULT_NAMESPACE) << ":" << std::string(e.name)
                          << " asset=" << std::string(e.type.assetName)
                          << " soif=" << e.type.showOnItemFrame
                          << " col=" << e.type.mapColor
                          << " expl=" << e.type.explorationMapElement
                          << " tc=" << e.type.trackCount
                          << " hmc=" << e.type.hasMapColor() << ")\n";
            }
        }
        // unknown tags ignored
    }

    // Count check: every ported entry must have been covered by a GT row.
    if (entryIndex != maps::BUILTIN_MAP_DECORATION_TYPES.size()) {
        ++mism;
        std::cerr << "COUNT mismatch: ported=" << maps::BUILTIN_MAP_DECORATION_TYPES.size()
                  << " GT-visited=" << entryIndex << "\n";
    }

    std::cout << "MapDecorationType cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
