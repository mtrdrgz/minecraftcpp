// Bit-exact parity gate for net.minecraft.world.Difficulty (Minecraft 26.1.2),
// ported in world/Difficulty.h. Reads the TSV emitted by
// mcpp/tools/DifficultyParity.java and compares values exactly.
//
// Tags:
//   CONST <ordinal> <getId> <name> <getSerializedName>
//   BYID  <id> <byId(id).getId> <byId(id).getSerializedName>
#include "world/Difficulty.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

// Map a C++ Difficulty ordinal (PEACEFUL=0..HARD=3) to mc::Difficulty.
static mc::Difficulty diffFromOrd(int o) { return mc::DIFFICULTY_VALUES[o]; }

// The Java enum constant name() per ordinal — used to verify the CONST rows pin
// the right constant before comparing accessors.
static const char* expectedName(int o) {
    switch (o) {
        case 0: return "PEACEFUL";
        case 1: return "EASY";
        case 2: return "NORMAL";
        case 3: return "HARD";
        default: return "";
    }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++cases;

        if (tag == "CONST") {
            // <ordinal> <getId> <name> <getSerializedName>
            int ord = std::stoi(p[1]);
            int expId = std::stoi(p[2]);
            const std::string& expName = p[3];
            const std::string& expSer = p[4];
            mc::Difficulty d = diffFromOrd(ord);
            // Ordinal -> name must line up with the C++ enum ordering.
            if (expName != expectedName(ord)) { ++mism; continue; }
            if (mc::difficultyGetId(d) != expId) { ++mism; continue; }
            if (expSer != mc::difficultySerializedName(d)) { ++mism; continue; }
        } else if (tag == "BYID") {
            // <id> <byId(id).getId> <byId(id).getSerializedName>
            int id = std::stoi(p[1]);
            int expId = std::stoi(p[2]);
            const std::string& expSer = p[3];
            mc::Difficulty d = mc::difficultyById(id);
            if (mc::difficultyGetId(d) != expId) { ++mism; continue; }
            if (expSer != mc::difficultySerializedName(d)) { ++mism; continue; }
        } else {
            // Unknown tag — do not silently pass; count as a mismatch.
            ++mism;
        }
    }

    std::cout << "Difficulty cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
