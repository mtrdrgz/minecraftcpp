// Parity test for the PURE per-room geometry in
//   net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces
//     .OceanMonumentPiece.{getRoomIndex, makeBoundingBox}  (26.1.2).
//
// Ground truth: tools/OceanMonumentRoomParity.java drives the REAL nested classes
// via reflection and emits a TSV. We recompute each row from
// OceanMonumentRoomGeometry.h and compare. All values are pure-integer, so the
// gate is exact (decimal compare).
//
//   ocean_monument_room_parity --cases mcpp/build/ocean_monument_room.tsv

#include "OceanMonumentRoomGeometry.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen::structure::oceanmonument;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int toi(const std::string& s) { return static_cast<int>(std::stoll(s)); }

Direction dir(int i) { return static_cast<Direction>(i); }

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) {
        std::cerr << "usage: ocean_monument_room_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;

    auto fail = [&](const std::string& tag, const std::string& detail) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << tag << " " << detail << "\n";
    };

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = splitTabs(line);
        const std::string& tag = p[0];
        ++total;

        if (tag == "IDX") {
            // roomX roomY roomZ | index
            int got = getRoomIndex(toi(p[1]), toi(p[2]), toi(p[3]));
            if (got != toi(p[4]))
                fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "BOX") {
            // dirOrd index roomW roomH roomD | minX minY minZ maxX maxY maxZ
            BoundingBox bb = makeRoomBoundingBox(dir(toi(p[1])), toi(p[2]),
                                                 toi(p[3]), toi(p[4]), toi(p[5]));
            if (bb.minX != toi(p[6]) || bb.minY != toi(p[7]) || bb.minZ != toi(p[8]) ||
                bb.maxX != toi(p[9]) || bb.maxY != toi(p[10]) || bb.maxZ != toi(p[11]))
                fail(tag, line + " got=" + std::to_string(bb.minX) + "," + std::to_string(bb.minY) +
                          "," + std::to_string(bb.minZ) + "," + std::to_string(bb.maxX) + "," +
                          std::to_string(bb.maxY) + "," + std::to_string(bb.maxZ));
        } else {
            fail("UNKNOWN_TAG", tag);
        }
    }

    std::cout << "OceanMonumentRoom checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
