// Bit-exact parity gate for the FULL recursive mineshaft assembly ported in
//   world/level/levelgen/structure/structures/MineshaftAssembly.h
// (createRandomShaftPiece + generateAndAddPiece + the four addChildren +
//  moveBelowSeaLevel), built on the already-certified per-box helpers and the
//  certified RandomSource (WorldgenRandom over LegacyRandomSource).
//
// Ground truth: tools/MineshaftAssemblyParity.java drives the REAL
//   net.minecraft...structures.MineshaftPieces.MineShaftRoom.addChildren(...)
// for each (seed, chunkX, chunkZ) and dumps every assembled piece (post
// sea-level adjust) as TSV, one "CASE\tseed\tcx\tcz" header per chunk. We
// re-run assembleMineshaftNormal(seed,cx,cz) here and compare every piece field
// bit-for-bit: kind, box (6 ints), orientation ordinal, genDepth, and the
// kind-specific flags (corridor hasRails/spider/numSections, crossing
// twoFloored/direction).
//
//   mineshaft_assembly_parity --cases mcpp/build/mineshaft_assembly.tsv

#include "world/level/levelgen/structure/structures/MineshaftAssembly.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ms = mc::levelgen::structure::structures;
using mc::levelgen::structure::Direction;

static int i32(const std::string& s) { return static_cast<int>(std::stoll(s)); }
static int64_t i64(const std::string& s) { return std::stoll(s); }

static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

static const char* kindName(ms::MsKind k) {
    switch (k) {
        case ms::MsKind::ROOM:     return "ROOM";
        case ms::MsKind::CORRIDOR: return "CORRIDOR";
        case ms::MsKind::CROSSING: return "CROSSING";
        case ms::MsKind::STAIRS:   return "STAIRS";
    }
    return "?";
}

// Render a C++ piece into the same 13-column TSV row the Java oracle emits.
static std::string rowFor(int idx, const ms::MsPiece& p) {
    int orientOrd = p.hasOrientation ? static_cast<int>(p.orientation) : -1;
    int fa = -1, fb = -1, fc = -1;
    if (p.kind == ms::MsKind::CORRIDOR) {
        fa = p.hasRails ? 1 : 0;
        fb = p.spiderCorridor ? 1 : 0;
        fc = p.numSections;
    } else if (p.kind == ms::MsKind::CROSSING) {
        fa = p.isTwoFloored ? 1 : 0;
        fb = static_cast<int>(p.crossingDir);
    }
    std::ostringstream o;
    o << idx << '\t' << kindName(p.kind) << '\t'
      << p.box.minX << '\t' << p.box.minY << '\t' << p.box.minZ << '\t'
      << p.box.maxX << '\t' << p.box.maxY << '\t' << p.box.maxZ << '\t'
      << orientOrd << '\t' << p.genDepth << '\t' << fa << '\t' << fb << '\t' << fc;
    return o.str();
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: mineshaft_assembly_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0, checks = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& ctx, const std::string& got, const std::string& want) {
        ++mism;
        if (shown++ < 40)
            std::cerr << "MISMATCH " << ctx << "\n   got:  " << got << "\n   want: " << want << "\n";
    };

    std::vector<ms::MsPiece> cur;   // current case's C++ assembly
    long curSeed = 0; int curCx = 0, curCz = 0; bool haveCase = false;
    long rowsThisCase = 0;
    auto closeCase = [&]() {
        if (!haveCase) return;
        // The oracle emits EVERY piece; if C++ assembled a different count, the
        // structure diverged even if every shared idx matched.
        if (rowsThisCase != static_cast<long>(cur.size())) {
            ++mism;
            if (shown++ < 40)
                std::cerr << "MISMATCH piece-count seed=" << curSeed << " chunk=(" << curCx
                          << "," << curCz << ") oracle=" << rowsThisCase
                          << " cpp=" << cur.size() << "\n";
        }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;

        if (p[0] == "CASE") {
            closeCase();
            curSeed = i64(p[1]); curCx = i32(p[2]); curCz = i32(p[3]);
            cur = ms::assembleMineshaftNormal(curSeed, curCx, curCz);
            haveCase = true;
            rowsThisCase = 0;
            ++cases;
            continue;
        }
        ++rowsThisCase;
        if (!haveCase) { fail("orphan-row(no CASE)", line, ""); continue; }

        int idx = i32(p[0]);
        std::string ctx = "seed=" + std::to_string(curSeed) + " chunk=(" +
                          std::to_string(curCx) + "," + std::to_string(curCz) + ") idx=" + std::to_string(idx);
        ++checks;
        if (idx < 0 || idx >= static_cast<int>(cur.size())) {
            fail(ctx + " [C++ has " + std::to_string(cur.size()) + " pieces]", "<missing>", line);
            continue;
        }
        std::string got = rowFor(idx, cur[static_cast<size_t>(idx)]);
        if (got != line) fail(ctx, got, line);
    }
    closeCase();

    std::cerr << "mineshaft_assembly_parity: cases=" << cases
              << " piece-checks=" << checks << " mismatches=" << mism << "\n";
    if (mism != 0) { std::cout << "FAIL\n"; return 1; }
    std::cout << "OK\n";
    return 0;
}
