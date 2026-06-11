// Byte-exact parity gate for WoodlandMansionPieces$MansionGrid (RNG-driven mansion
// layout). Drives the C++ port WoodlandMansionGridLayout against ground-truth rows
// emitted by WoodlandMansionGridLayoutParity.java.
//
// TSV row: MANSION  <seed> <entranceX> <entranceY> <605 ints>
//   605 = 5 grids (base, third, floor0, floor1, floor2) * 11 * 11, x-major (x*11+y).
//
// Usage: WoodlandMansionGridLayoutParityTest --cases <tsv>
// Prints: WoodlandMansionGridLayout checks=N mismatches=M  (nonzero exit iff M>0).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "world/level/levelgen/RandomSource.h"
#include "world/level/levelgen/structure/structures/WoodlandMansionGridLayout.h"

using mc::levelgen::RandomSource;
using mc::levelgen::structure::structures::WoodlandMansionGridLayout;

namespace {

constexpr int kGridCells = 11 * 11;     // 121
constexpr int kGridCount = 5;            // base, third, f0, f1, f2
constexpr int kTotalCells = kGridCells * kGridCount; // 605

struct Row {
    int64_t seed = 0;
    int entranceX = 0;
    int entranceY = 0;
    int cells[kTotalCells] = {};
};

// Flatten a SimpleGrid the same way the Java driver does: x-major (gx*11+gy).
void flatten(const mc::levelgen::structure::structures::SimpleGrid& grid, int* out) {
    int w = grid.width();
    int h = grid.height();
    int idx = 0;
    for (int gx = 0; gx < w; gx++) {
        for (int gy = 0; gy < h; gy++) {
            out[idx++] = grid.get(gx, gy);
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    const char* casesPath = nullptr;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }
    if (!casesPath) {
        std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]);
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::fprintf(stderr, "cannot open cases file: %s\n", casesPath);
        return 2;
    }

    long long checks = 0;
    long long mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag != "MANSION") continue;

        Row row;
        ss >> row.seed >> row.entranceX >> row.entranceY;
        bool ok = true;
        for (int i = 0; i < kTotalCells; i++) {
            if (!(ss >> row.cells[i])) { ok = false; break; }
        }
        if (!ok) {
            std::fprintf(stderr, "malformed row (seed=%lld)\n", (long long)row.seed);
            return 2;
        }

        // Drive the real algorithm in C++ with the matching LegacyRandomSource.
        std::shared_ptr<RandomSource> rng = RandomSource::create(row.seed);
        WoodlandMansionGridLayout layout(*rng);

        int got[kTotalCells];
        flatten(layout.baseGrid, got + 0 * kGridCells);
        flatten(layout.thirdFloorGrid, got + 1 * kGridCells);
        flatten(layout.floorRooms[0], got + 2 * kGridCells);
        flatten(layout.floorRooms[1], got + 3 * kGridCells);
        flatten(layout.floorRooms[2], got + 4 * kGridCells);

        // entranceX / entranceY.
        checks++;
        if (layout.entranceX != row.entranceX || layout.entranceY != row.entranceY) {
            mismatches++;
            if (mismatches <= 10) {
                std::fprintf(stderr,
                             "entrance mismatch seed=%lld got=(%d,%d) want=(%d,%d)\n",
                             (long long)row.seed, layout.entranceX, layout.entranceY,
                             row.entranceX, row.entranceY);
            }
        }

        for (int i = 0; i < kTotalCells; i++) {
            checks++;
            if (got[i] != row.cells[i]) {
                mismatches++;
                if (mismatches <= 10) {
                    int g = i / kGridCells;
                    int c = i % kGridCells;
                    std::fprintf(stderr,
                                 "cell mismatch seed=%lld grid=%d x=%d y=%d got=%d want=%d\n",
                                 (long long)row.seed, g, c / 11, c % 11, got[i], row.cells[i]);
                }
            }
        }
    }

    std::printf("WoodlandMansionGridLayout checks=%lld mismatches=%lld\n", checks, mismatches);
    return mismatches != 0 ? 1 : 0;
}
