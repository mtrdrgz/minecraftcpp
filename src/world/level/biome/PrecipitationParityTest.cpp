// Parity test for net.minecraft.world.level.biome.Biome.Precipitation.
// Ground truth: tools/PrecipitationParity.java vs the real enum. For each
// emitted constant we recompute ordinal()/name()/getSerializedName() with the
// C++ port (Precipitation.h) and compare exactly (strings + ints).
//
//   precipitation_parity --cases mcpp/build/precipitation.tsv

#include "Precipitation.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::biome::Precipitation;
using mc::biome::kPrecipitationValues;
using mc::biome::kPrecipitationCount;
using mc::biome::precipitationOrdinal;
using mc::biome::precipitationName;
using mc::biome::precipitationSerializedName;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// Map a Java enum name() to the C++ Precipitation constant, so we drive the C++
// recompute from the GT's identity (rather than assuming the GT's row order).
bool nameToConstant(const std::string& name, Precipitation& out) {
    for (std::size_t k = 0; k < kPrecipitationCount; ++k) {
        if (name == precipitationName(kPrecipitationValues[k])) {
            out = kPrecipitationValues[k];
            return true;
        }
    }
    return false;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: precipitation_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "PREC") {
            // PREC  ordinal  name  serializedName
            const long long expOrdinal = std::stoll(p[1]);
            const std::string& expName = p[2];
            const std::string& expSerialized = p[3];

            Precipitation c;
            if (!nameToConstant(expName, c)) { fail(line + " (unknown C++ constant for name)"); continue; }

            if (precipitationOrdinal(c) != static_cast<int>(expOrdinal))
                fail(line + " got ordinal=" + std::to_string(precipitationOrdinal(c)));
            if (std::string(precipitationName(c)) != expName)
                fail(line + " got name=" + precipitationName(c));
            if (std::string(precipitationSerializedName(c)) != expSerialized)
                fail(line + " got serialized=" + precipitationSerializedName(c));
        } else if (t == "COUNT") {
            // COUNT  values().length
            const long long expCount = std::stoll(p[1]);
            if (static_cast<long long>(kPrecipitationCount) != expCount)
                fail(line + " got count=" + std::to_string(kPrecipitationCount));
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "Precipitation cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
