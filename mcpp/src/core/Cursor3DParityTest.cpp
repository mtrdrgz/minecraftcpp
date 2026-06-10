// Bit-exact parity gate for net.minecraft.core.Cursor3D (Minecraft 26.1.2),
// ported in core/Cursor3D.h. Reads the TSV emitted by tools/Cursor3DParity.java
// (ground truth from the REAL class) and compares iteration order + nextType
// classification cell-for-cell, plus the total step count per box.
//
//   cursor3d_parity --cases mcpp/build/cursor3d.tsv
//
// Cursor3D yields only integers, so comparisons are exact int equality (which
// is bit-for-bit). Each box is replayed: STEP rows for a box arrive in advance()
// order, so we hold one live mc::Cursor3D per (box bounds) key and step it once
// per STEP row. END rows are checked against a fresh full walk's step count.

#include "core/Cursor3D.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <optional>

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

int toi(const std::string& s) { return static_cast<int>(std::stoll(s)); }

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: cursor3d_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long cases = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };

    // The live iterator for the box whose STEP rows we are currently walking.
    std::array<int, 6> curKey{};
    bool haveCur = false;
    std::optional<mc::Cursor3D> cur;
    int curStep = 0; // how many advance() calls we've issued on `cur`

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++cases;

        if (tag == "STEP") {
            // STEP minX minY minZ maxX maxY maxZ step expX expY expZ expType
            std::array<int, 6> key{toi(p[1]), toi(p[2]), toi(p[3]),
                                   toi(p[4]), toi(p[5]), toi(p[6])};
            int step = toi(p[7]);
            int expX = toi(p[8]), expY = toi(p[9]), expZ = toi(p[10]);
            int expType = toi(p[11]);

            // (Re)build the iterator when the box changes or we see step 0.
            if (!haveCur || key != curKey || step == 0) {
                cur.emplace(key[0], key[1], key[2], key[3], key[4], key[5]);
                curKey = key;
                haveCur = true;
                curStep = 0;
            }

            bool ok = cur->advance();
            if (!ok) { fail(line + " (advance returned false)"); continue; }
            if (curStep != step) { fail(line + " (step index drift)"); }
            ++curStep;

            int gotX = cur->nextX(), gotY = cur->nextY(), gotZ = cur->nextZ();
            int gotType = cur->getNextType();
            if (gotX != expX || gotY != expY || gotZ != expZ || gotType != expType) {
                fail(line + " got=" + std::to_string(gotX) + "," + std::to_string(gotY)
                     + "," + std::to_string(gotZ) + " type=" + std::to_string(gotType));
            }
        }
        else if (tag == "END") {
            // END minX minY minZ maxX maxY maxZ totalSteps
            mc::Cursor3D c(toi(p[1]), toi(p[2]), toi(p[3]),
                           toi(p[4]), toi(p[5]), toi(p[6]));
            int expTotal = toi(p[7]);
            int count = 0;
            // Mirror the GT bound so a degenerate huge/negative end can't loop.
            while (count < 100000 && c.advance()) ++count;
            if (count != expTotal) {
                fail(line + " total=" + std::to_string(count));
            }
            // After a full walk, the next advance() must also be false.
            if (c.advance()) {
                fail(line + " (advance true past end)");
            }
            // Box ended; drop any partial STEP iterator.
            haveCur = false;
        }
        else {
            fail("UNKNOWN_TAG " + tag);
        }
    }

    std::cout << "Cursor3D cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
