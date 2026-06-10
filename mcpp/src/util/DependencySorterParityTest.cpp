// Bit-exact parity gate for net.minecraft.util.DependencySorter (Integer keys).
//
// Reads the TSV produced by DependencySorterParity.java, reconstructs each case's
// sorter via the C++ port (mc::util::IntDependencySorter), runs orderByDependencies,
// and compares the visitation order to the ground-truth order character-for-character.
//
//   ORDER \t <caseId> \t <entriesSpec> \t <resultOrder>
//
// entriesSpec = entry(';'entry)*,  entry = <id>':'reqList'|'optList
// reqList/optList = comma-separated ints (possibly empty)
// resultOrder = comma-separated ids (the BiConsumer accept() order)
//
// Pass: every recomputed order equals the GT order. Exit 0 iff zero mismatches.

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "util/DependencySorter.h"

using mc::util::IntDependencySorter;
using mc::util::IntDependencyEntry;

static std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    // std::getline drops trailing empty fields; pad so a trailing empty resultOrder
    // (an empty sorter) still yields the expected column count.
    return out;
}

// Split a comma-separated int list (empty string -> empty vector).
static std::vector<std::int32_t> parseInts(const std::string& s) {
    std::vector<std::int32_t> out;
    if (s.empty()) return out;
    std::string cur;
    std::istringstream ss(s);
    while (std::getline(ss, cur, ',')) {
        if (!cur.empty()) out.push_back(static_cast<std::int32_t>(std::stoll(cur)));
    }
    return out;
}

static std::vector<std::string> splitChar(const std::string& s, char c) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(s);
    while (std::getline(ss, cur, c)) out.push_back(cur);
    if (s.empty()) out.clear();
    return out;
}

int main(int argc, char** argv) {
    std::string tsv;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) tsv = argv[++i];
    }
    if (tsv.empty()) { std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]); return 2; }

    std::ifstream in(tsv);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", tsv.c_str()); return 2; }

    long cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto t = splitTabs(line);
        if (t.empty() || t[0] != "ORDER") continue;

        // Columns: ORDER, caseId, entriesSpec, [resultOrder]. resultOrder may be a
        // missing/empty trailing field (empty sorter).
        const std::string caseId   = t.size() > 1 ? t[1] : "";
        const std::string specStr  = t.size() > 2 ? t[2] : "";
        const std::string expected = t.size() > 3 ? t[3] : "";

        // Build the sorter in the same addEntry order as the GT.
        IntDependencySorter sorter;
        for (const std::string& entryStr : splitChar(specStr, ';')) {
            // <id>':'reqList'|'optList
            std::size_t colon = entryStr.find(':');
            std::size_t bar   = entryStr.find('|');
            if (colon == std::string::npos || bar == std::string::npos || bar < colon) {
                std::fprintf(stderr, "malformed entry '%s' in case %s\n", entryStr.c_str(), caseId.c_str());
                ++mismatches;
                continue;
            }
            std::int32_t id = static_cast<std::int32_t>(std::stoll(entryStr.substr(0, colon)));
            std::string reqStr = entryStr.substr(colon + 1, bar - colon - 1);
            std::string optStr = entryStr.substr(bar + 1);
            IntDependencyEntry e;
            e.requiredDeps = parseInts(reqStr);
            e.optionalDeps = parseInts(optStr);
            sorter.addEntry(id, e);
        }

        // Run orderByDependencies, capturing the visitation order as a CSV string.
        std::string got;
        sorter.orderByDependencies([&](std::int32_t id, const IntDependencyEntry&) {
            if (!got.empty()) got.push_back(',');
            got += std::to_string(id);
        });

        ++cases;
        if (got != expected) {
            ++mismatches;
            if (mismatches <= 25) {
                std::fprintf(stderr, "ORDER mismatch case=%s\n  exp=[%s]\n  got=[%s]\n",
                             caseId.c_str(), expected.c_str(), got.c_str());
            }
        }
    }

    std::printf("DependencySorter cases=%ld mismatches=%ld\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
