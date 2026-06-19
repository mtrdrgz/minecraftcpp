// Parity test for net.minecraft.world.item.component.DyedItemColor.applyDyes
// (26.1.2). Ground truth: tools/DyedItemColorParity.java.
//
//   dyed_item_color_parity --cases <dyed_item_color.tsv>
//
// Each row carries: whether a current colour is present, the current rgb (8 hex,
// raw 32-bit), the dye count, the comma-separated DyeColor ids, and the expected
// result rgb (8 hex). The C++ side recomputes applyDyes(...) and compares the
// EXACT 32-bit result — no tolerance.
//
// Row tag:
//   DYE  <hasCurrent:0|1>  <currentRgb8>  <ndyes>  <dyeId0,dyeId1,...>  <resultRgb8>

#include "world/item/component/DyedItemColor.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Parse 8-hex as a raw 32-bit pattern, reinterpreted as a signed int (Java int).
int parseHex32(const std::string& s) {
    return static_cast<int>(static_cast<uint32_t>(std::stoul(s, nullptr, 16)));
}
std::vector<int> parseIds(const std::string& s, int ndyes) {
    std::vector<int> ids;
    if (ndyes == 0 || s == "-") return ids;
    std::string it;
    std::istringstream ss(s);
    while (std::getline(ss, it, ',')) ids.push_back(std::stoi(it));
    return ids;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: dyed_item_color_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];
        ++total;

        if (tag == "DYE") {
            // DYE <hasCurrent> <currentRgb8> <ndyes> <idList> <resultRgb8>
            if (p.size() != 6) { fail("BADROW " + line); continue; }
            int hasCurrent = std::stoi(p[1]);
            int currentRgb = parseHex32(p[2]);
            int ndyes = std::stoi(p[3]);
            std::vector<int> ids = parseIds(p[4], ndyes);
            int want = parseHex32(p[5]);

            if (static_cast<int>(ids.size()) != ndyes) { fail("BADIDS " + line); continue; }

            std::optional<int> current;
            if (hasCurrent == 1) current = currentRgb;

            int got = mc::item_component::dyed_item_color::applyDyes(current, ids);
            if (got != want) {
                std::ostringstream os;
                os << line << "  (got=" << std::hex
                   << static_cast<uint32_t>(got) << ")";
                fail(os.str());
            }
        } else {
            fail("UNKNOWN_TAG " + tag);
        }
    }

    std::cout << "DyedItemColor checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
