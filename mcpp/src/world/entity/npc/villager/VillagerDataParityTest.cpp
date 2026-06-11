// Parity test for the C++ VillagerData port
// (world/entity/npc/villager/VillagerData.h).
//
// Ground truth: mcpp/tools/VillagerDataParity.java drives the REAL decompiled
// net.minecraft.world.entity.npc.villager.VillagerData from client.jar and emits
// one row per case. This test reads --cases <tsv>, recomputes every value with
// the port, and compares exactly:
//
//   MINXP  <level>     <int>    getMinXpPerLevel(level) == int
//   MAXXP  <level>     <int>    getMaxXpPerLevel(level) == int
//   CANLVL <level>     <0|1>    canLevelUp(level)       == bool
//   CLAMP  <inLevel>   <int>    clampLevel(inLevel)     == int   (Math.max(1,level))
//   CONST  <name(b64)> <int>    named constant          == int
//
// All quantities are pure integer functions of the input level — no world, no
// RNG, no registry — so the comparison is bit-exact and host-independent.
//
// Build (standalone probe, matching the session toolchain):
//   clang++ -std=c++23 -O2 -ffp-contract=off -I mcpp/src \
//       mcpp/src/world/entity/npc/villager/VillagerDataParityTest.cpp \
//       -o mcpp/build/villager_data_probe.exe
//   mcpp/build/villager_data_probe.exe --cases mcpp/build/villager_data.tsv
//
// Prints "VillagerData checks=<N> mismatches=<M>"; exit 0 iff M == 0.

#include "world/entity/npc/villager/VillagerData.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace vd = mc::world::entity::npc::villager;

namespace {

// Standard base64 decode (matches java.util.Base64 encoder output).
std::string b64decode(const std::string& in) {
    static const std::string T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto val = [&](char c) -> int {
        auto p = T.find(c);
        return p == std::string::npos ? -1 : static_cast<int>(p);
    };
    std::string out;
    int buf = 0, bits = 0;
    for (char c : in) {
        if (c == '=' || c == '\r' || c == '\n') break;
        int d = val(c);
        if (d < 0) continue;
        buf = (buf << 6) | d;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

struct Counters {
    long checks = 0;
    long mismatches = 0;
};

void report(Counters& c, bool ok, const std::string& line) {
    ++c.checks;
    if (!ok) {
        ++c.mismatches;
        if (c.mismatches <= 25) {
            std::cerr << "MISMATCH: " << line << "\n";
        }
    }
}

void verifyLine(const std::string& line, Counters& c) {
    if (line.empty()) return;
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return;

    if (tag == "MINXP") {
        int64_t level = 0;
        int64_t exp = 0;
        in >> level >> exp;
        int32_t got = vd::getMinXpPerLevel(static_cast<int32_t>(level));
        report(c, got == exp, line);
    } else if (tag == "MAXXP") {
        int64_t level = 0;
        int64_t exp = 0;
        in >> level >> exp;
        int32_t got = vd::getMaxXpPerLevel(static_cast<int32_t>(level));
        report(c, got == exp, line);
    } else if (tag == "CANLVL") {
        int64_t level = 0;
        int exp = 0;
        in >> level >> exp;
        bool got = vd::canLevelUp(static_cast<int32_t>(level));
        report(c, (got ? 1 : 0) == exp, line);
    } else if (tag == "CLAMP") {
        int64_t inLevel = 0;
        int64_t exp = 0;
        in >> inLevel >> exp;
        int32_t got = vd::clampLevel(static_cast<int32_t>(inLevel));
        report(c, got == exp, line);
    } else if (tag == "CONST") {
        std::string b64;
        int64_t exp = 0;
        in >> b64 >> exp;
        std::string name = b64decode(b64);
        int32_t got = 0;
        bool known = true;
        if (name == "MIN_VILLAGER_LEVEL") {
            got = vd::MIN_VILLAGER_LEVEL;
        } else if (name == "MAX_VILLAGER_LEVEL") {
            got = vd::MAX_VILLAGER_LEVEL;
        } else {
            known = false;  // unknown constant name -> fail loudly
        }
        report(c, known && got == exp, line);
    }
    // Unknown tags are ignored (forward-compat with the generator).
}

// Self-checks with no Mojang files: pin the table, the canLevelUp gate, and the
// short-circuit that keeps the array accesses in bounds.
void selfChecks(Counters& c) {
    // canLevelUp gate: true only for level in {1,2,3,4}.
    report(c, !vd::canLevelUp(0), "self:canLevelUp 0");
    report(c, vd::canLevelUp(1), "self:canLevelUp 1");
    report(c, vd::canLevelUp(4), "self:canLevelUp 4");
    report(c, !vd::canLevelUp(5), "self:canLevelUp 5");
    report(c, !vd::canLevelUp(-1), "self:canLevelUp -1");

    // getMaxXpPerLevel: thresholds {0,10,70,150,250}, indexed by [level].
    report(c, vd::getMaxXpPerLevel(1) == 10, "self:max 1");
    report(c, vd::getMaxXpPerLevel(2) == 70, "self:max 2");
    report(c, vd::getMaxXpPerLevel(3) == 150, "self:max 3");
    report(c, vd::getMaxXpPerLevel(4) == 250, "self:max 4");
    // Out of gate -> 0 (no array index, no clamp, no wrap).
    report(c, vd::getMaxXpPerLevel(0) == 0, "self:max 0");
    report(c, vd::getMaxXpPerLevel(5) == 0, "self:max 5");
    report(c, vd::getMaxXpPerLevel(-100) == 0, "self:max -100");

    // getMinXpPerLevel: indexed by [level-1].
    report(c, vd::getMinXpPerLevel(1) == 0, "self:min 1");
    report(c, vd::getMinXpPerLevel(2) == 10, "self:min 2");
    report(c, vd::getMinXpPerLevel(3) == 70, "self:min 3");
    report(c, vd::getMinXpPerLevel(4) == 150, "self:min 4");
    report(c, vd::getMinXpPerLevel(0) == 0, "self:min 0");
    report(c, vd::getMinXpPerLevel(5) == 0, "self:min 5");

    // Constructor clamp: Math.max(1, level) — lower bound only, no upper clamp.
    report(c, vd::clampLevel(-5) == 1, "self:clamp -5");
    report(c, vd::clampLevel(0) == 1, "self:clamp 0");
    report(c, vd::clampLevel(1) == 1, "self:clamp 1");
    report(c, vd::clampLevel(3) == 3, "self:clamp 3");
    report(c, vd::clampLevel(99) == 99, "self:clamp 99 (no upper clamp)");

    // Constants.
    report(c, vd::MIN_VILLAGER_LEVEL == 1, "self:MIN_VILLAGER_LEVEL");
    report(c, vd::MAX_VILLAGER_LEVEL == 5, "self:MAX_VILLAGER_LEVEL");
}

} // namespace

int main(int argc, char** argv) {
    Counters c;

    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }

    if (casesPath.empty()) {
        selfChecks(c);
    } else {
        std::ifstream f(casesPath);
        if (!f) {
            std::cerr << "cannot open cases file: " << casesPath << "\n";
            return 2;
        }
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            verifyLine(line, c);
        }
    }

    std::cout << "VillagerData checks=" << c.checks
              << " mismatches=" << c.mismatches << "\n";
    return c.mismatches == 0 ? 0 : 1;
}
