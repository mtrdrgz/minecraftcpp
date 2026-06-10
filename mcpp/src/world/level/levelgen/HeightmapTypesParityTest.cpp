// Parity test for net.minecraft.world.level.levelgen.Heightmap.Types (the ENUM
// PART ONLY). Ground truth: tools/HeightmapTypesParity.java vs the real enum's
// real private fields + public accessors.
//
// Verifies, bit-for-bit, every Types constant's
//   (ordinal, id, serializedName, getSerializationKey, usage.ordinal, usage.name,
//    sendToClient, keepAfterWorldgen, isOpaque() predicate category)
// and the full Types.BY_ID (ByIdMap.continuous / ZERO) lookup over a battery of
// int keys.
//
// The isOpaque() predicate BODY is block-registry-coupled and is NOT part of this
// gate (see unportedMethods in the header). We certify which predicate category
// each constant uses, exactly as the GT classifies by reference identity.
//
//   heightmap_types_parity --cases mcpp/build/heightmap_types.tsv

#include "HeightmapTypes.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace hm = mc::levelgen::heightmap;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Java ints printed as signed decimal; parse via long long then narrow.
int i(const std::string& s) { return static_cast<int>(std::stoll(s)); }

// Mirror the GT's usage-name mapping (Heightmap.Usage.name()).
const char* usageName(hm::Usage u) {
    switch (u) {
        case hm::Usage::WORLDGEN: return "WORLDGEN";
        case hm::Usage::LIVE_WORLD: return "LIVE_WORLD";
        case hm::Usage::CLIENT: return "CLIENT";
    }
    return "?";
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: heightmap_types_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != std::stoll(exp)) fail(l + " got=" + std::to_string(got));
    };
    auto eqS = [&](const std::string& got, const std::string& exp, const std::string& l) {
        if (got != exp) fail(l + " got=" + got);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // tolerate CRLF
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "CONST") {
            // CONST <ordinal> <id> <serializedName> <getSerializationKey>
            //       <usageOrdinal> <usageName> <sendToClient> <keepAfterWorldgen>
            //       <opaqueCategory>
            const int ordinal = i(p[1]);
            if (ordinal < 0 || ordinal >= hm::TYPES_COUNT) {
                fail(line + " ordinal-out-of-range");
                continue;
            }
            const hm::TypesData& d = hm::TYPES[static_cast<std::size_t>(ordinal)];
            eqI(d.id, p[2], line);
            eqS(std::string(hm::getSerializedName(d)), p[3], line);
            eqS(std::string(hm::getSerializationKey(d)), p[4], line);
            eqI(static_cast<int>(d.usage), p[5], line);
            eqS(usageName(d.usage), p[6], line);
            eqI(hm::sendToClient(d) ? 1 : 0, p[7], line);
            eqI(hm::keepAfterWorldgen(d) ? 1 : 0, p[8], line);
            eqI(static_cast<int>(hm::isOpaque(d)), p[9], line);
        } else if (t == "BYID") {
            // BYID <id> <resolvedOrdinal>; byId(id) -> resolved TypesData, ordinal == id.
            const int id = i(p[1]);
            const hm::TypesData& d = hm::byId(id);
            // TypesData carries no separate ordinal; declaration order == id order,
            // so the resolved id is the ordinal.
            eqI(d.id, p[2], line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "HeightmapTypes cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
