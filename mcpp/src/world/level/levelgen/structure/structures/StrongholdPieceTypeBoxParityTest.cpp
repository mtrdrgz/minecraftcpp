// Parity test for the PURE box-construction + validity layer of
//   net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces
//   (26.1.2).
//
// Every StrongholdPieces.<X>.createPiece(...) builds its candidate box via
// BoundingBox.orientBox(footX,footY,footZ, <type consts>, direction) and accepts
// it iff StrongholdPiece.isOkBox(box) (== box.minY() > 10). Ground truth:
// tools/StrongholdPieceTypeBoxParity.java drives the REAL BoundingBox.orientBox
// with each type's constants and the REAL (reflected) isOkBox, AND cross-checks
// every type's constants by invoking the REAL createPiece. We recompute the box +
// okBox from StrongholdPieceTypeBox.h and compare. All values are decimal ints /
// a 0|1 flag, so the gate is exact.
//
//   stronghold_piece_type_box_parity --cases mcpp/build/stronghold_piece_type_box.tsv
//
// Row: BOX <typeIdx> <footX> <footY> <footZ> <dirOrd>  <minX..maxZ> <okBox>
// dirOrd is net.minecraft.core.Direction.ordinal() (DOWN0 UP1 NORTH2 SOUTH3 WEST4
// EAST5). The Java Bootstrap and the createPiece cross-check may print unrelated
// lines (e.g. XCHK_FAIL diagnostics); rows whose first tab field is not "BOX" are
// skipped (not counted, not failed).

#include "StrongholdPieceTypeBox.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen::structure::structures;
using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Direction;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int toi(const std::string& s) { return static_cast<int>(std::stoll(s)); }

// net.minecraft.core.Direction ordinal -> our Direction enum.
Direction dirFromOrdinal(int ord) { return static_cast<Direction>(ord); }

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) { std::cerr << "usage: stronghold_piece_type_box_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = splitTabs(line);
        if (p.empty() || p[0] != "BOX") continue; // skip Bootstrap / XCHK diagnostics
        if (p.size() < 13) continue;

        int typeIdx = toi(p[1]);
        int footX = toi(p[2]), footY = toi(p[3]), footZ = toi(p[4]);
        int dirOrd = toi(p[5]);
        int eMinX = toi(p[6]), eMinY = toi(p[7]), eMinZ = toi(p[8]);
        int eMaxX = toi(p[9]), eMaxY = toi(p[10]), eMaxZ = toi(p[11]);
        int eOk = toi(p[12]);

        auto type = static_cast<StrongholdPiece>(typeIdx);
        StrongholdPieceResult r =
            strongholdCreatePieceGeometry(type, footX, footY, footZ, dirFromOrdinal(dirOrd));

        bool ok = r.box.minX == eMinX && r.box.minY == eMinY && r.box.minZ == eMinZ &&
                  r.box.maxX == eMaxX && r.box.maxY == eMaxY && r.box.maxZ == eMaxZ &&
                  (r.okBox ? 1 : 0) == eOk;
        ++total;
        if (!ok) {
            ++mism;
            if (shown++ < 20) {
                std::cerr << "MISMATCH BOX type=" << typeIdx << " foot=(" << footX << "," << footY << ","
                          << footZ << ") dir=" << dirOrd << "\n"
                          << "  want box=" << eMinX << "," << eMinY << "," << eMinZ << "/" << eMaxX << ","
                          << eMaxY << "," << eMaxZ << " ok=" << eOk << "\n"
                          << "  got  box=" << r.box.minX << "," << r.box.minY << "," << r.box.minZ << "/"
                          << r.box.maxX << "," << r.box.maxY << "," << r.box.maxZ << " ok=" << (r.okBox ? 1 : 0)
                          << "\n";
            }
        }
    }

    std::cout << "StrongholdPieceTypeBox checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
