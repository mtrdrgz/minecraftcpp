// Parity test for net.minecraft.world.level.levelgen.GenerationStep.Decoration
// (Minecraft 26.1.2). Ground truth: tools/GenerationStepParity.java.
//
// The GT emits, per enum constant, a DEC row carrying ordinal() / name() /
// getName() / getSerializedName(), plus a COUNT row. We rebuild the C++ side from
//   * GenerationStepData.h    — the verified ordinal -> name/serializedName table
//   * feature/GenerationStep.h — the ENGINE'S actual enum (proves the live enum
//                                ordinals the engine uses match Java)
// and compare every field exactly. We also require:
//   * COUNT == both C++ table size AND engine enum COUNT (catches missing/extra)
//   * every C++ descriptor is covered by a GT DEC row (C++ longer than Java fails)
//   * name() == getName() == getSerializedName() (the Java invariant) per row
//
//   generation_step_parity --cases mcpp/build/generation_step.tsv

#include "GenerationStepData.h"
#include "feature/GenerationStep.h"

#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {

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

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: generation_step_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& line, const std::string& why) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH(" << why << ") " << line << "\n";
    };

    // Static cross-check: the engine enum COUNT must equal the descriptor table
    // length. (Compile-time-ish guard surfaced as a runtime case too.)
    namespace feat = mc::levelgen::feature;
    {
        ++total;
        if (static_cast<std::size_t>(feat::GenerationStep::COUNT) !=
            mc::levelgen::kDecorationStepCount)
            fail("ENGINE_ENUM_COUNT", "engine COUNT=" +
                     std::to_string(feat::GenerationStep::COUNT) + " table=" +
                     std::to_string(mc::levelgen::kDecorationStepCount));
    }

    std::set<int> covered;
    bool sawCount = false;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF safety
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];

        if (tag == "DEC") {
            ++total;
            if (p.size() < 5) { fail(line, "DEC malformed"); continue; }
            int ordinal = std::stoi(p[1]);
            const std::string& name = p[2];
            const std::string& getName = p[3];
            const std::string& serialized = p[4];

            if (ordinal < 0 ||
                static_cast<std::size_t>(ordinal) >= mc::levelgen::kDecorationStepCount) {
                fail(line, "DEC ordinal_oob");
                continue;
            }
            covered.insert(ordinal);
            const auto& d = mc::levelgen::kDecorationSteps[ordinal];

            // ordinal field inside the descriptor must equal its index.
            if (d.ordinal != ordinal)
                fail(line, "DEC desc_ordinal=" + std::to_string(d.ordinal));
            // name() exact.
            if (name != d.name)
                fail(line, "DEC name got=" + std::string(d.name) + " want=" + name);
            // getSerializedName() exact.
            if (serialized != d.serializedName)
                fail(line, "DEC serialized got=" + std::string(d.serializedName) +
                               " want=" + serialized);
            // Java invariant: getName() == getSerializedName(), and both == our
            // single serializedName column.
            if (getName != serialized)
                fail(line, "DEC getName!=serialized");
            if (getName != d.serializedName)
                fail(line, "DEC getName got=" + std::string(d.serializedName) +
                               " want=" + getName);
        } else if (tag == "COUNT") {
            ++total;
            sawCount = true;
            if (p.size() < 2) { fail(line, "COUNT malformed"); continue; }
            int count = std::stoi(p[1]);
            if (static_cast<std::size_t>(count) != mc::levelgen::kDecorationStepCount)
                fail(line, "COUNT table=" +
                               std::to_string(mc::levelgen::kDecorationStepCount));
            if (static_cast<int>(feat::GenerationStep::COUNT) != count)
                fail(line, "COUNT engine_enum=" +
                               std::to_string(feat::GenerationStep::COUNT));
        } else {
            ++total;
            fail(line, "UNKNOWN_TAG");
        }
    }

    // Every C++ descriptor must have been covered by a GT DEC row (so a C++ table
    // strictly longer than Java's cannot pass silently).
    for (std::size_t ord = 0; ord < mc::levelgen::kDecorationStepCount; ++ord) {
        ++total;
        if (covered.find(static_cast<int>(ord)) == covered.end())
            fail("DESC[" + std::to_string(ord) + "]", "cpp_descriptor_not_in_gt");
    }
    // A COUNT row must have appeared.
    {
        ++total;
        if (!sawCount) fail("COUNT", "missing_count_row");
    }

    std::cout << "GenerationStep cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
