// Byte-exact parity gate for NetherFortressChildOffset.h against ground truth from
// the REAL net.minecraft...NetherFortressPieces.NetherBridgePiece
// .generateChild{Forward,Left,Right} (driven by NetherFortressChildOffsetParity.java).
//
// For each TSV row we recompute the child foot position + facing from the parent
// box/orientation/offsets via the header, then reproduce what the REAL method
// observably produces: the chosen child is always the RNG-free BridgeEndFiller,
// whose box == BoundingBox.orientBox(footX,footY,footZ, -1,-3,0, 5,10,8, childDir)
// and which is null iff NetherBridgePiece.isOkBox(box) (box.minY() > 10) fails.
// We compare present-ness, and (when present) the full child box + child Direction
// ordinal, bit-for-bit against the Java-observed values.
//
//   <name>_test.exe --cases <tsv>
// Prints "NetherFortressChildOffset checks=N mismatches=M"; exit nonzero iff M>0.

#include "NetherFortressChildOffset.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Direction;
using namespace mc::levelgen::structure::structures;

namespace {

// NetherFortressPieces.BridgeEndFiller.createPiece orientBox constants
// (NetherFortressPieces.java:211): orientBox(foot, -1,-3,0, 5,10,8, dir).
constexpr int32_t BEF_OFF_X = -1, BEF_OFF_Y = -3, BEF_OFF_Z = 0;
constexpr int32_t BEF_W = 5, BEF_H = 10, BEF_D = 8;

// NetherFortressPieces.NetherBridgePiece.isOkBox(box) == box.minY() > 10.
constexpr bool isOkBox(const BoundingBox& b) noexcept { return b.minY > 10; }

Direction dirFromOrdinal(int ord) {
    switch (ord) {
        case 0: return Direction::DOWN;
        case 1: return Direction::UP;
        case 2: return Direction::NORTH;
        case 3: return Direction::SOUTH;
        case 4: return Direction::WEST;
        case 5: return Direction::EAST;
        default: return Direction::NORTH;
    }
}
int ordinalOf(Direction d) { return static_cast<int>(d); }

struct Row {
    std::string tag;
    int32_t pMinX, pMinY, pMinZ, pMaxX, pMaxY, pMaxZ;
    int pDirOrd;
    int32_t a, b;          // (xOff,yOff) for FWD; (yOff,zOff) for LFT/RGT
    int present;
    int32_t cMinX, cMinY, cMinZ, cMaxX, cMaxY, cMaxZ;
    int cDirOrd;
};

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]);
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", casesPath.c_str());
        return 2;
    }

    long checks = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        Row r;
        if (!(ss >> r.tag)) continue;
        if (r.tag != "FWD" && r.tag != "LFT" && r.tag != "RGT") continue; // skip diagnostics
        ss >> r.pMinX >> r.pMinY >> r.pMinZ >> r.pMaxX >> r.pMaxY >> r.pMaxZ
           >> r.pDirOrd >> r.a >> r.b >> r.present
           >> r.cMinX >> r.cMinY >> r.cMinZ >> r.cMaxX >> r.cMaxY >> r.cMaxZ >> r.cDirOrd;

        BoundingBox parentBox(r.pMinX, r.pMinY, r.pMinZ, r.pMaxX, r.pMaxY, r.pMaxZ);
        Direction pDir = dirFromOrdinal(r.pDirOrd);

        // Recompute the child foot position + facing via the header.
        NetherFortressChildOffset off;
        if (r.tag == "FWD") {
            off = netherFortressChildForward(parentBox, /*hasOrientation=*/true, pDir, r.a, r.b);
        } else if (r.tag == "LFT") {
            off = netherFortressChildLeft(parentBox, /*hasOrientation=*/true, pDir, r.a, r.b);
        } else {
            off = netherFortressChildRight(parentBox, /*hasOrientation=*/true, pDir, r.a, r.b);
        }

        // Reproduce the observable BridgeEndFiller result: box + okBox-driven null.
        int predPresent = 0;
        BoundingBox predBox;
        int predDirOrd = -1;
        if (off.present) {
            BoundingBox box = BoundingBox::orientBox(off.footX, off.footY, off.footZ,
                                                     BEF_OFF_X, BEF_OFF_Y, BEF_OFF_Z,
                                                     BEF_W, BEF_H, BEF_D, off.direction);
            if (isOkBox(box)) {
                predPresent = 1;
                predBox = box;
                predDirOrd = ordinalOf(off.direction);
            }
        }

        bool ok;
        if (predPresent != r.present) {
            ok = false;
        } else if (predPresent == 0) {
            ok = true; // both null — nothing more to compare
        } else {
            ok = predBox.minX == r.cMinX && predBox.minY == r.cMinY && predBox.minZ == r.cMinZ
              && predBox.maxX == r.cMaxX && predBox.maxY == r.cMaxY && predBox.maxZ == r.cMaxZ
              && predDirOrd == r.cDirOrd;
        }

        ++checks;
        if (!ok) {
            ++mismatches;
            if (mismatches <= 20) {
                std::fprintf(stderr,
                    "MISMATCH %s p=(%d,%d,%d/%d,%d,%d) pDir=%d off=(%d,%d) "
                    "pred[present=%d box=(%d,%d,%d/%d,%d,%d) dir=%d] "
                    "java[present=%d box=(%d,%d,%d/%d,%d,%d) dir=%d]\n",
                    r.tag.c_str(), r.pMinX, r.pMinY, r.pMinZ, r.pMaxX, r.pMaxY, r.pMaxZ,
                    r.pDirOrd, r.a, r.b,
                    predPresent, predBox.minX, predBox.minY, predBox.minZ,
                    predBox.maxX, predBox.maxY, predBox.maxZ, predDirOrd,
                    r.present, r.cMinX, r.cMinY, r.cMinZ, r.cMaxX, r.cMaxY, r.cMaxZ, r.cDirOrd);
            }
        }
    }

    std::printf("NetherFortressChildOffset checks=%ld mismatches=%ld\n", checks, mismatches);
    return mismatches > 0 ? 1 : 0;
}
