// Parity gate for net.minecraft.world.level.levelgen.VerticalAnchor.
//
// Verifies the EXISTING engine port (VerticalAnchor.h) — Absolute / AboveBottom /
// BelowTop resolveY(WorldGenerationContext) — against ground truth produced by
// tools/VerticalAnchorParity.java (the real decompiled net.minecraft classes).
//
//   default        -> a few hardcoded self-checks (no Mojang files needed)
//   --cases <tsv>  -> verify every ANCHOR row of the generated reference
//
// TSV row: ANCHOR <kind> <param> <minGenY> <genDepth> <resolvedY>   (all decimal)
// resolveY is pure 32-bit int math, so comparison is exact bit-for-bit on int32.

#include "VerticalAnchor.h"
#include "WorldGenerationContext.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen;

namespace {

// Build the anchor for a given kind/param. bottom()==aboveBottom(0),
// top()==belowTop(0), exactly as in the Java interface.
VerticalAnchorPtr makeAnchor(const std::string& kind, int param) {
    using namespace VerticalAnchors;
    if (kind == "absolute") return absolute(param);
    if (kind == "aboveBottom") return aboveBottom(param);
    if (kind == "belowTop") return belowTop(param);
    if (kind == "bottom") return bottom();
    if (kind == "top") return top();
    return nullptr;
}

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;
    if (tag != "ANCHOR") { err = "unknown tag " + tag; return false; }

    std::string kind;
    int param = 0, minGenY = 0, genDepth = 0, expected = 0;
    in >> kind >> param >> minGenY >> genDepth >> expected;
    if (!in) { err = "malformed row: " + line; return false; }

    VerticalAnchorPtr a = makeAnchor(kind, param);
    if (!a) { err = "unknown anchor kind " + kind; return false; }

    const WorldGenerationContext ctx(minGenY, genDepth);
    const int32_t got = a->resolveY(ctx);

    if (std::bit_cast<int32_t>(got) != std::bit_cast<int32_t>(expected)) {
        err = "ANCHOR " + kind + " param=" + std::to_string(param) + " ctx(" +
              std::to_string(minGenY) + "," + std::to_string(genDepth) + ") " +
              std::to_string(got) + "!=" + std::to_string(expected);
        return false;
    }
    return true;
}

// Hand-computed against the verbatim Java formulas for ctx = (minGenY=-64, genDepth=384):
//   absolute(50)      -> 50
//   aboveBottom(10)   -> -64 + 10            = -54
//   belowTop(20)      -> 384 - 1 + (-64) - 20 = 299
//   bottom() == aboveBottom(0) -> -64
//   top()    == belowTop(0)    -> 384 - 1 - 64 = 319
const std::vector<std::string> kHardcoded = {
    "ANCHOR\tabsolute\t50\t-64\t384\t50",
    "ANCHOR\taboveBottom\t10\t-64\t384\t-54",
    "ANCHOR\tbelowTop\t20\t-64\t384\t299",
    "ANCHOR\tbottom\t0\t-64\t384\t-64",
    "ANCHOR\ttop\t0\t-64\t384\t319",
    // ctx = (0, 256): aboveBottom(64)=64, belowTop(0)=255, absolute(-1)=-1
    "ANCHOR\taboveBottom\t64\t0\t256\t64",
    "ANCHOR\tbelowTop\t0\t0\t256\t255",
    "ANCHOR\tabsolute\t-1\t0\t256\t-1",
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
        std::cout << "VerticalAnchor cases=" << n << " mismatches=" << bad << '\n';
        return bad == 0 ? 0 : 1;
    }

    long bad = 0;
    for (const auto& line : kHardcoded) {
        std::string err;
        if (!verifyLine(line, err)) { ++bad; std::cerr << "FAIL: " << err << '\n'; }
    }
    std::cout << "VerticalAnchor cases=" << kHardcoded.size() << " mismatches=" << bad << '\n';
    return bad == 0 ? 0 : 1;
}
