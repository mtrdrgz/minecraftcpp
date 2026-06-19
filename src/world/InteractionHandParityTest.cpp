// Bit-exact parity gate for net.minecraft.world.InteractionHand (Minecraft 26.1.2),
// ported in world/InteractionHand.h. Reads the TSV emitted by
// mcpp/tools/InteractionHandParity.java and compares values exactly.
//
// Tags:
//   CONST <ordinal> <name> <id> <asEquipmentSlot ordinal>
//   BYID  <id>      <resulting ordinal>
//
//   interaction_hand_parity --cases mcpp/build/interaction_hand.tsv

#include "world/InteractionHand.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

const char* expectedName(mc::InteractionHand h) {
    return h == mc::InteractionHand::MAIN_HAND ? "MAIN_HAND" : "OFF_HAND";
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: interaction_hand_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0;
    long mism = 0;
    int shown = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF safety
        if (line.empty()) continue;
        std::vector<std::string> p = split_tabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "CONST") {
            // CONST <ordinal> <name> <id> <asEquipmentSlot ordinal>
            ++cases;
            int ord = std::stoi(p[1]);
            const std::string& name = p[2];
            int expId = std::stoi(p[3]);
            int expSlot = std::stoi(p[4]);

            auto h = static_cast<mc::InteractionHand>(ord);
            bool ok = true;
            if (name != std::string(expectedName(h))) ok = false;
            if (mc::interactionHandId(h) != expId) ok = false;
            if (mc::interactionHandAsEquipmentSlotOrdinal(h) != expSlot) ok = false;

            if (!ok) {
                ++mism;
                if (shown++ < 40)
                    std::cerr << "CONST mismatch ord=" << ord << " name=" << name
                              << " id=" << expId << " slot=" << expSlot << "\n";
            }
        } else if (tag == "BYID") {
            // BYID <id> <resulting ordinal>
            ++cases;
            int id = static_cast<int>(std::stoll(p[1]));
            int exp = std::stoi(p[2]);
            int got = static_cast<int>(mc::interactionHandById(id));
            if (got != exp) {
                ++mism;
                if (shown++ < 40)
                    std::cerr << "BYID mismatch id=" << id << " exp=" << exp
                              << " got=" << got << "\n";
            }
        } else {
            // Unknown tag — do not silently pass; count as a mismatch.
            ++cases;
            ++mism;
            if (shown++ < 40) std::cerr << "unknown tag: " << tag << "\n";
        }
    }

    std::cout << "InteractionHand cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
