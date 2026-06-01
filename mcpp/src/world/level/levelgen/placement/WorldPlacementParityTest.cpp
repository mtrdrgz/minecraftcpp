// Parity test for HeightmapPlacement / HeightRangePlacement against a stub world.
// Ground truth: tools/WorldPlacementParity.java (real modifiers + a Proxy
// WorldGenLevel using the same deterministic stub heightmap).
//
//   default        -> hardcoded self-checks
//   --cases <tsv>  -> verify every generated line

#include "../RandomSource.h"
#include "../heightproviders/HeightProvider.h"
#include "HeightmapPlacement.h"
#include "NoiseCountPlacement.h"
#include "PlacementContext.h"

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen;
using namespace mc::levelgen::placement;
using namespace mc::levelgen::heightproviders;
using mc::levelgen::VerticalAnchors::aboveBottom;
using mc::levelgen::VerticalAnchors::absolute;
using mc::levelgen::VerticalAnchors::belowTop;

namespace {

// Same deterministic stub as WorldPlacementParity.java.
class StubLevel final : public WorldGenLevel {
public:
    int getHeight(Heightmap::Types type, int x, int z) const override {
        const int idx = static_cast<int>(type);
        return ((x * 31 + z * 17 + idx * 7) & 127) - 64;
    }
    int getMinY() const override { return -64; }
};

StubLevel g_level;
PlacementContext g_ctx(&g_level, -64, 384);

HeightProviderPtr heightProvider(const std::string& name) {
    if (name == "const50") return std::make_shared<ConstantHeight>(absolute(50));
    if (name == "uni_full") return std::make_shared<UniformHeight>(aboveBottom(0), belowTop(0));
    if (name == "trap") return std::make_shared<TrapezoidHeight>(absolute(0), absolute(100), 20);
    return nullptr;
}

std::string posList(const std::vector<BlockPos>& positions) {
    std::string s;
    for (size_t i = 0; i < positions.size(); ++i) {
        if (i) s += ',';
        s += std::to_string(positions[i].x) + ":" + std::to_string(positions[i].y) + ":" + std::to_string(positions[i].z);
    }
    return s.empty() ? "-" : s;
}

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string kind;
    in >> kind;
    if (kind.empty()) return true;

    if (kind == "HMAP") {
        int typeIdx, ox, oy, oz, count;
        std::string posStr;
        in >> typeIdx >> ox >> oy >> oz >> count >> posStr;
        HeightmapPlacement m(static_cast<Heightmap::Types>(typeIdx));
        LegacyRandomSource r(0);
        const auto got = m.getPositions(&g_ctx, r, BlockPos{ ox, oy, oz });
        if (static_cast<int>(got.size()) != count || posList(got) != posStr) {
            err = "HMAP type " + std::to_string(typeIdx) + " origin " + std::to_string(ox) + "," + std::to_string(oz) +
                  " got " + posList(got) + " expected " + posStr;
            return false;
        }
        return true;
    }
    if (kind == "NCOUNT") {
        std::string name;
        int ox, oz, expectedCount;
        in >> name >> ox >> oz >> expectedCount;
        std::shared_ptr<PlacementModifier> m;
        if (name == "ntc_flower") m = std::make_shared<NoiseThresholdCountPlacement>(-0.8, 15, 4);
        else if (name == "nbc") m = std::make_shared<NoiseBasedCountPlacement>(160, 80.0, 0.3);
        else { err = "unknown noise-count " + name; return false; }
        LegacyRandomSource r(0);
        const int got = static_cast<int>(m->getPositions(&g_ctx, r, BlockPos{ ox, 64, oz }).size());
        if (got != expectedCount) {
            err = "NCOUNT " + name + " " + std::to_string(ox) + "," + std::to_string(oz) + " count " +
                  std::to_string(got) + "!=" + std::to_string(expectedCount);
            return false;
        }
        return true;
    }
    if (kind == "HRANGE") {
        std::string name;
        long long seed;
        int ox, oy, oz, count;
        std::string posStr;
        in >> name >> seed >> ox >> oy >> oz >> count >> posStr;
        auto hp = heightProvider(name);
        if (!hp) { err = "unknown height provider " + name; return false; }
        HeightRangePlacement m(hp);
        LegacyRandomSource r(seed);
        const auto got = m.getPositions(&g_ctx, r, BlockPos{ ox, oy, oz });
        if (static_cast<int>(got.size()) != count || posList(got) != posStr) {
            err = "HRANGE " + name + " seed " + std::to_string(seed) + " got " + posList(got) + " expected " + posStr;
            return false;
        }
        return true;
    }
    return true;
}

const std::vector<std::string> kHardcoded = {
    "HMAP\t0\t0\t64\t0\t0\t-",          // height == minY -> empty
    "HMAP\t0\t-5\t64\t7\t1\t-5:28:7",
    "HMAP\t0\t-13\t64\t-29\t0\t-",      // empty
    "HMAP\t4\t0\t64\t0\t1\t0:-36:0",
    "HRANGE\tconst50\t0\t100\t64\t-50\t1\t100:50:-50",
    "HRANGE\tuni_full\t42\t0\t64\t0\t1\t0:-38:0",
    "HRANGE\ttrap\t42\t0\t64\t0\t1\t0:10:0",
    "NCOUNT\tntc_flower\t0\t0\t4",
    "NCOUNT\tnbc\t0\t0\t48",
    "NCOUNT\tnbc\t123\t-456\t79",
    "NCOUNT\tnbc\t1000\t2000\t97",
};

} // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[1]) == "--cases") {
        std::ifstream f(argv[2]);
        if (!f) { std::cerr << "cannot open " << argv[2] << '\n'; return 2; }
        std::string line;
        long n = 0, bad = 0;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::string err;
            ++n;
            if (!verifyLine(line, err)) {
                ++bad;
                if (bad <= 20) std::cerr << "MISMATCH: " << err << '\n';
            }
        }
        std::cout << "world placement cases=" << n << " mismatches=" << bad << '\n';
        return bad == 0 ? 0 : 1;
    }

    bool ok = true;
    for (const auto& line : kHardcoded) {
        std::string err;
        if (!verifyLine(line, err)) { ok = false; std::cerr << "FAIL: " << err << '\n'; }
    }
    if (!ok) { std::cerr << "World placement parity checks FAILED\n"; return 1; }
    std::cout << "World placement parity checks passed\n";
    return 0;
}
