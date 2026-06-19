// Parity test for the per-constant ordinal + name() tables of the two pure
// nested enums of net.minecraft.world.level.ClipContext:
//   ClipContext.Block  (COLLIDER/OUTLINE/VISUAL/FALLDAMAGE_RESETTING)
//   ClipContext.Fluid  (NONE/SOURCE_ONLY/ANY/WATER)
// Ground truth: tools/ClipContextEnumsParity.java.
//
// For every enum the GT emits a COUNT row and one NAME row per constant. We
// rebuild the C++ tables from ClipContextEnums.h and compare:
//   * COUNT : C++ table size == GT constant count (catches a missing/extra const)
//   * NAME  : C++ name[ordinal] == GT name(), exact string, at the matching ordinal
// Additionally we assert every C++ constant is covered by a GT NAME row, so a
// C++ table that is longer than Java's cannot pass silently.
//
//   clip_context_enums_parity --cases mcpp/build/clip_context_enums.tsv

#include "ClipContextEnums.h"

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace cc = mc::world::level::clipcontext;

namespace {
// Tab-split preserving empty fields.
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

// Resolve the C++ name table for a GT tag; nullptr if unknown.
const std::vector<std::string>* namesFor(const std::string& tag) {
    if (tag == "Block") return &cc::blockNames();
    if (tag == "Fluid") return &cc::fluidNames();
    return nullptr;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: clip_context_enums_parity --cases <tsv>\n";
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
            const std::string& enumName = p[1];
            int count = std::stoi(p[2]);
            const std::vector<std::string>* names = namesFor(enumName);
            if (!names) { fail(line, "COUNT unknown_enum=" + enumName); continue; }
            if (static_cast<int>(names->size()) != count)
                fail(line, "COUNT size_mismatch cpp=" +
                               std::to_string(names->size()));
        } else if (tag == "NAME") {
            ++total;
            const std::string& enumName = p[1];
            int ordinal = std::stoi(p[2]);
            const std::string& expected = p[3];
            const std::vector<std::string>* names = namesFor(enumName);
            if (!names) { fail(line, "NAME unknown_enum=" + enumName); continue; }
            if (ordinal < 0 ||
                static_cast<std::size_t>(ordinal) >= names->size()) {
                fail(line, "NAME ordinal_oob"); continue;
            }
            covered[enumName].insert(ordinal);
            const std::string& got = (*names)[static_cast<std::size_t>(ordinal)];
            if (got != expected) fail(line, "NAME got=" + got + " want=" + expected);
            // Also exercise the convenience lookup helpers.
            const std::string& helper =
                (enumName == "Block") ? cc::blockName(ordinal) : cc::fluidName(ordinal);
            if (helper != expected) fail(line, "name helper got=" + helper);
        } else {
            ++total;
            fail(line, "UNKNOWN_TAG");
        }
    }

    // Every C++ constant of both enums must have been covered by a GT row.
    const std::pair<std::string, const std::vector<std::string>*> tables[] = {
        {"Block", &cc::blockNames()},
        {"Fluid", &cc::fluidNames()},
    };
    for (const auto& t : tables) {
        const auto& seen = covered[t.first];
        for (std::size_t ord = 0; ord < t.second->size(); ++ord) {
            ++total;
            if (seen.find(static_cast<int>(ord)) == seen.end())
                fail(t.first + "[" + std::to_string(ord) + "]",
                     "cpp_constant_not_in_gt");
        }
    }

    std::cout << "ClipContextEnums cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
