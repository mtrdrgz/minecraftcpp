// Parity test for the per-constant ordinal + getSerializedName() tables of a
// second family of StringRepresentable block-state-property enums. Ground truth:
//   tools/BlockStateEnums2Parity.java.
//
// For every enum, the GT emits a COUNT row and one NAME row per constant. We
// rebuild the C++ table from BlockStateEnums2.h and compare:
//   * COUNT  : C++ table size == GT constant count (catches a missing/extra const)
//   * NAME   : C++ serializedNames[ordinal] == GT getSerializedName(), exact string
// Additionally we assert every C++ constant is covered by a GT NAME row, so a
// C++ table that is longer than Java's cannot pass silently.
//
//   bsenums2_parity --cases mcpp/build/bsenums2.tsv

#include "BlockStateEnums2.h"

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace bsp = mc::block::state::properties;

namespace {
// Tab-split preserving empty fields (a serialized name is never empty here, but
// we keep the convention used across the parity suite).
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

const bsp::BlockStateEnum2Desc* findDesc(const std::string& javaName) {
    for (const bsp::BlockStateEnum2Desc* d : bsp::allBlockStateEnums2())
        if (d->javaName == javaName) return d;
    return nullptr;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: bsenums2_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& line, const std::string& why) {
        ++mism; if (shown++ < 40) std::cerr << "MISMATCH(" << why << ") " << line << "\n";
    };

    // Track which (enum, ordinal) pairs the GT covered, so a C++ table that is
    // strictly longer than Java's gets flagged (every C++ constant must appear).
    std::map<std::string, std::set<int>> covered;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF safety
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];

        if (tag == "COUNT") {
            ++total;
            const std::string& javaName = p[1];
            int count = std::stoi(p[2]);
            const bsp::BlockStateEnum2Desc* d = findDesc(javaName);
            if (!d) { fail(line, "COUNT unknown_enum=" + javaName); continue; }
            if (static_cast<int>(d->serializedNames.size()) != count)
                fail(line, "COUNT size_mismatch cpp=" +
                               std::to_string(d->serializedNames.size()));
        } else if (tag == "NAME") {
            ++total;
            const std::string& javaName = p[1];
            int ordinal = std::stoi(p[2]);
            const std::string& expected = p[3];
            const bsp::BlockStateEnum2Desc* d = findDesc(javaName);
            if (!d) { fail(line, "NAME unknown_enum=" + javaName); continue; }
            if (ordinal < 0 ||
                static_cast<std::size_t>(ordinal) >= d->serializedNames.size()) {
                fail(line, "NAME ordinal_oob"); continue;
            }
            covered[javaName].insert(ordinal);
            const std::string& got = d->serializedNames[static_cast<std::size_t>(ordinal)];
            if (got != expected) fail(line, "NAME got=" + got + " want=" + expected);
            // Also exercise the convenience lookup helper.
            if (bsp::getSerializedName2(javaName, ordinal) != expected)
                fail(line, "getSerializedName2 helper");
        } else {
            ++total;
            fail(line, "UNKNOWN_TAG");
        }
    }

    // Every C++ constant of every gated enum must have been covered by a GT row.
    for (const bsp::BlockStateEnum2Desc* d : bsp::allBlockStateEnums2()) {
        const auto& seen = covered[d->javaName];
        for (std::size_t ord = 0; ord < d->serializedNames.size(); ++ord) {
            ++total;
            if (seen.find(static_cast<int>(ord)) == seen.end())
                fail(d->javaName + "[" + std::to_string(ord) + "]",
                     "cpp_constant_not_in_gt");
        }
    }

    std::cout << "BlockStateEnums2 cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
