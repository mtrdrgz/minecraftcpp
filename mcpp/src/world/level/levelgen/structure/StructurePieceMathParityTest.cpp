// Parity test for the PURE orientation/offset math in
//   net.minecraft.world.level.levelgen.structure.StructurePiece (26.1.2).
//
// Ground truth: tools/StructurePieceMathParity.java drives the REAL class
// (makeBoundingBox / setOrientation / getWorldX/Y/Z / getLocatorPosition /
// isCloseToChunk) and emits a TSV. We recompute each row from StructurePieceMath.h
// and compare. All values are decimal ints (this math is pure integer), so the
// gate is exact.
//
//   structure_piece_math_parity --cases mcpp/build/structure_piece_math.tsv

#include "StructurePieceMath.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen::structure::piece;

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

// Build a piece with orientation from an ordinal; -1 == setOrientation(null).
StructurePieceMath makePiece(const BoundingBox& bb, int dirOrd) {
    StructurePieceMath p;
    p.boundingBox = bb;
    if (dirOrd < 0) p.setOrientation(false, Direction::NORTH);
    else            p.setOrientation(true, dir(dirOrd));
    return p;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) { std::cerr << "usage: structure_piece_math_parity --cases <tsv>\n"; return 2; }

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

        if (tag == "MAKEBB") {
            // x y z dirOrd w h dp | minX minY minZ maxX maxY maxZ
            BoundingBox bb = makeBoundingBox(toi(p[1]), toi(p[2]), toi(p[3]), dir(toi(p[4])),
                                             toi(p[5]), toi(p[6]), toi(p[7]));
            if (bb.minX != toi(p[8]) || bb.minY != toi(p[9]) || bb.minZ != toi(p[10]) ||
                bb.maxX != toi(p[11]) || bb.maxY != toi(p[12]) || bb.maxZ != toi(p[13]))
                fail(tag, line + " got=" + std::to_string(bb.minX) + "," + std::to_string(bb.minY) + "," +
                          std::to_string(bb.minZ) + "," + std::to_string(bb.maxX) + "," +
                          std::to_string(bb.maxY) + "," + std::to_string(bb.maxZ));
        } else if (tag == "SETORIENT") {
            // dirOrd(-1=null) | mirrorOrd rotationOrd
            StructurePieceMath piece = makePiece(BoundingBox(0, 0, 0, 1, 1, 1), toi(p[1]));
            int gotM = static_cast<int>(piece.mirror);
            int gotR = static_cast<int>(piece.rotation);
            if (gotM != toi(p[2]) || gotR != toi(p[3]))
                fail(tag, line + " got=" + std::to_string(gotM) + "," + std::to_string(gotR));
        } else if (tag == "WORLD") {
            // dirOrd boxIdx x z | wx wy wz   (boxes rebuilt below to match the GT)
            static const BoundingBox boxes[] = {
                BoundingBox(0, 0, 0, 15, 15, 15),
                BoundingBox(-7, -3, -11, 5, 9, 20),
                BoundingBox(-100, 60, -200, -50, 120, -120),
                BoundingBox(1000000, 0, 1000000, 1000050, 30, 1000050),
                BoundingBox(-2147483640, -64, -2147483640, -2147483600, 320, -2147483600),
            };
            int dirOrd = toi(p[1]);
            int boxIdx = toi(p[2]);
            int x = toi(p[3]), z = toi(p[4]);
            const BoundingBox& bb = boxes[boxIdx];
            StructurePieceMath piece = makePiece(bb, dirOrd);
            int wx = piece.getWorldX(x, z);
            int wy = piece.getWorldY(x);
            int wz = piece.getWorldZ(x, z);
            if (wx != toi(p[5]) || wy != toi(p[6]) || wz != toi(p[7]))
                fail(tag, line + " got=" + std::to_string(wx) + "," + std::to_string(wy) + "," + std::to_string(wz));
        } else if (tag == "LOCATOR") {
            // minX minY minZ maxX maxY maxZ | cx cy cz
            BoundingBox bb(toi(p[1]), toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]), toi(p[6]));
            StructurePieceMath piece;
            piece.boundingBox = bb;
            Vec3i c = piece.getLocatorPosition();
            if (c.x != toi(p[7]) || c.y != toi(p[8]) || c.z != toi(p[9]))
                fail(tag, line + " got=" + std::to_string(c.x) + "," + std::to_string(c.y) + "," + std::to_string(c.z));
        } else if (tag == "CLOSE") {
            // bbMinX bbMinZ bbMaxX bbMaxZ cx cz dist | close(0/1)
            // Y span is irrelevant to isCloseToChunk; supply a trivial valid Y range.
            BoundingBox bb(toi(p[1]), 0, toi(p[2]), toi(p[3]), 0, toi(p[4]));
            StructurePieceMath piece;
            piece.boundingBox = bb;
            bool got = piece.isCloseToChunk(toi(p[5]), toi(p[6]), toi(p[7]));
            if ((got ? 1 : 0) != toi(p[8]))
                fail(tag, line + " got=" + std::to_string(got ? 1 : 0));
        } else {
            fail("UNKNOWN_TAG", tag);
        }
    }

    std::cout << "StructurePieceMath checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
