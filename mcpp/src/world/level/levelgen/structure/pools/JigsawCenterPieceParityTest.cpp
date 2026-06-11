// Parity gate for the ENTRY of net.minecraft...pools.JigsawPlacement.addPieces
// (lines 63-122) — the CENTER (start) piece placement — vs the REAL generator.
// This is the first verifiable slice of the C++ jigsaw assembly placer: it certifies
//   * the RNG seeding (WorldgenRandom(LegacyRandomSource(0)).setLargeFeatureSeed(seed,0,0)),
//   * Rotation.getRandom(random) = (Rotation)nextInt(4) + getRandomTemplate (consumes nextInt(size)),
//   * centerElement.getBoundingBox(adjustedPos=(0,0,0), centerRotation)  [certified StructureTemplatePool],
//   * centerX/Z = (box.maxX+box.minX)/2 (Java int div), bottomY = position.getY()+getFirstFreeHeight=0+64,
//     oldAbsoluteGroundY = box.minY+groundLevelDelta(=1), centerPiece.move(0, bottomY-oldAbsoluteGroundY, 0).
// The full BFS placer (children) is the next increment; this gate compares only piece idx 0.
//
//   build:  add_executable(jigsaw_center_piece_parity ... StructureTemplatePool deps + nlohmann_json + RandomSource)
//   inputs (run from repo root):
//     mcpp/build/structure_template_loader.tsv   (TEMPLATE rows -> per-template size; committed GT)
//     26.1.2/data/minecraft/worldgen/template_pool/pillager_outpost/base_plates.json
//     mcpp/build/jigsaw_placement.tsv            (the committed oracle; PIECE idx 0 = center piece)

#include "StructureTemplatePool.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Vec3i;
using mc::levelgen::structure::Rotation;
using mc::levelgen::structure::Mirror;
using mc::levelgen::structure::kBlockPosZero;
namespace pools = mc::levelgen::structure::pools;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss; ss << f.rdbuf();
    return ss.str();
}

}  // namespace

int main(int argc, char** argv) {
    std::string loaderTsv = "mcpp/build/structure_template_loader.tsv";
    std::string poolJson = "26.1.2/data/minecraft/worldgen/template_pool/pillager_outpost/base_plates.json";
    std::string oracleTsv = "mcpp/build/jigsaw_placement.tsv";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--loader" && i + 1 < argc) loaderTsv = argv[++i];
        else if (a == "--pool" && i + 1 < argc) poolJson = argv[++i];
        else if (a == "--cases" && i + 1 < argc) oracleTsv = argv[++i];
    }

    // 1) per-template size from the committed StructureTemplateLoader GT.
    std::map<std::string, Vec3i> sizes;
    {
        std::ifstream f(loaderTsv, std::ios::binary);
        if (!f) { std::fprintf(stderr, "cannot open %s\n", loaderTsv.c_str()); return 2; }
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("TEMPLATE\t", 0) != 0) continue;
            auto c = splitTabs(line);            // TEMPLATE <shortname> base64 sx sy sz nblk
            if (c.size() < 6) continue;
            // loader TSV keys by short name; pool element location is the full id.
            sizes["minecraft:pillager_outpost/" + c[1]] = Vec3i{std::stoi(c[3]), std::stoi(c[4]), std::stoi(c[5])};
        }
    }
    pools::SizeResolver sizeOf = [&](const std::string& loc) -> Vec3i {
        auto it = sizes.find(loc);
        if (it == sizes.end()) throw std::runtime_error("no size for " + loc);
        return it->second;
    };

    // 2) base_plates pool.
    pools::StructureTemplatePool basePlates = pools::loadPool(nlohmann::json::parse(readFile(poolJson)));

    // 3) oracle PIECE idx-0 rows per seed.
    struct Expect { int rot; std::string loc; int bb[6]; };
    std::map<int64_t, Expect> expected;
    {
        std::ifstream f(oracleTsv, std::ios::binary);
        if (!f) { std::fprintf(stderr, "cannot open %s\n", oracleTsv.c_str()); return 2; }
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("PIECE\t", 0) != 0) continue;
            auto c = splitTabs(line);            // PIECE seed idx loc rot pX pY pZ bbMinXYZ bbMaxXYZ gld nj
            if (c.size() < 15) continue;
            if (std::stoi(c[2]) != 0) continue;  // center piece only
            Expect e;
            e.loc = c[3]; e.rot = std::stoi(c[4]);
            for (int k = 0; k < 6; ++k) e.bb[k] = std::stoi(c[8 + k]);
            expected[std::stoll(c[1])] = e;
        }
    }

    long checks = 0, mismatches = 0;
    for (const auto& [seed, e] : expected) {
        // RNG: WorldgenRandom(LegacyRandomSource(0)).setLargeFeatureSeed(seed, 0, 0).
        auto rnd = std::make_shared<mc::levelgen::WorldgenRandom>(
            std::make_shared<mc::levelgen::LegacyRandomSource>(0));
        rnd->setLargeFeatureSeed(seed, 0, 0);

        Rotation centerRotation = static_cast<Rotation>(rnd->nextInt(4));   // Rotation.getRandom
        int idx = basePlates.getRandomTemplateIndex(*rnd);                  // consumes nextInt(size)
        const pools::StructurePoolElement& element = basePlates.templates[static_cast<std::size_t>(idx)];

        ++checks;
        bool ok = false;
        try {
            if (element.isEmpty()) { mismatches++; continue; }
            BoundingBox box = element.getBoundingBox(sizeOf, mc::levelgen::structure::BlockPos{0, 0, 0}, centerRotation);
            int centerX = (box.maxX + box.minX) / 2;   // Java int division
            int centerZ = (box.maxZ + box.minZ) / 2;
            (void)centerX; (void)centerZ;
            int bottomY = 0 + 64;                       // position.getY() + getFirstFreeHeight(stub=64)
            int oldAbsoluteGroundY = box.minY + element.getGroundLevelDelta();
            box.move(0, bottomY - oldAbsoluteGroundY, 0);
            ok = (static_cast<int>(centerRotation) == e.rot) &&
                 (element.locationString() == e.loc) &&
                 box.minX == e.bb[0] && box.minY == e.bb[1] && box.minZ == e.bb[2] &&
                 box.maxX == e.bb[3] && box.maxY == e.bb[4] && box.maxZ == e.bb[5];
        } catch (...) { ok = false; }

        if (!ok) {
            ++mismatches;
            if (mismatches <= 10) std::fprintf(stderr, "MISMATCH seed=%lld\n", (long long)seed);
        }
    }

    std::printf("JigsawCenterPiece checks=%ld mismatches=%ld\n", checks, mismatches);
    return mismatches > 0 ? 1 : 0;
}
