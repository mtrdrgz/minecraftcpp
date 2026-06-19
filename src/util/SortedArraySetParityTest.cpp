// Parity test for the C++ port of net.minecraft.util.SortedArraySet
// (mc::util::SortedArraySetInt in util/SortedArraySet.h) and the
// net.minecraft.util.Util.growByHalf growth formula it relies on.
//
// Ground truth: tools/SortedArraySetParity.java driving the REAL SortedArraySet<Integer>
// (create()/natural order) and Util.growByHalf.
//
//   sorted_array_set_parity --cases mcpp/build/sorted_array_set.tsv
//
// Row TAGs (see SortedArraySetParity.java):
//   GROW  <cur> <min> <result>
//         Direct probe of growByHalf(cur, min). Recomputed and compared bit-exact.
//
//   OP    <scen> <step> <opName> <arg> <ret> <size> <capacity> <orderedCSV>
//         One executed operation in a scripted scenario. We replay the SAME op stream
//         per scenario: an `init` row (re)creates a fresh set whose initial capacity
//         equals that row's <capacity> column; every subsequent OP row executes the
//         named op on the running set and we compare ret/size/capacity/orderedCSV.
//
// `ret` semantics by op:
//   add/remove/contains -> "0"/"1"
//   addOrGet/first/last -> the returned int (decimal)
//   get                 -> the int when present, else literal "ABSENT"
//   clear/init          -> "-" (no return)
//
// Comparisons are exact: integers/strings compared verbatim (ints are decimal text;
// every value round-trips through int32_t so no precision is lost).

#include "util/SortedArraySet.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using mc::util::growByHalf;
using mc::util::SortedArraySetInt;

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int32_t toi(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

// Build the orderedCSV string the GT emits from a set's active ordered contents.
std::string orderedCsv(const SortedArraySetInt& s) {
    std::string out;
    bool first = true;
    for (int32_t v : s.ordered()) {
        if (!first) out += ',';
        out += std::to_string(v);
        first = false;
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) {
        std::cerr << "usage: sorted_array_set_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;

    auto fail = [&](const std::string& msg) {
        ++mism;
        if (shown++ < 60) std::cerr << "MISMATCH " << msg << "\n";
    };

    // Running set per scenario; the `init` row (re)creates it.
    std::unique_ptr<SortedArraySetInt> set;
    std::string curScen;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;

        if (p[0] == "GROW") {
            // GROW <cur> <min> <result>
            if (p.size() < 4) continue;
            int32_t cur = toi(p[1]);
            int32_t mn = toi(p[2]);
            int32_t expected = toi(p[3]);
            ++total;
            int32_t got = growByHalf(cur, mn);
            if (got != expected)
                fail("GROW cur=" + p[1] + " min=" + p[2] + " got=" +
                     std::to_string(got) + " exp=" + std::to_string(expected));
        } else if (p[0] == "OP") {
            // OP <scen> <step> <opName> <arg> <ret> <size> <capacity> <orderedCSV>
            // orderedCSV may be empty (trailing field absent after split).
            if (p.size() < 8) continue;  // orderedCSV (p[8]) may be missing when empty
            const std::string& scen = p[1];
            const std::string& op = p[3];
            const std::string& argRepr = p[4];
            const std::string& retExp = p[5];
            int32_t sizeExp = toi(p[6]);
            int32_t capExp = toi(p[7]);
            std::string orderedExp = (p.size() >= 9) ? p[8] : std::string();

            if (op == "init") {
                // Fresh set at the GT's initial capacity (== this row's capacity col).
                curScen = scen;
                set = std::make_unique<SortedArraySetInt>(capExp);
                // Verify the freshly-created set matches GT init snapshot.
                ++total;
                if (set->size() != sizeExp || set->capacity() != capExp ||
                    orderedCsv(*set) != orderedExp)
                    fail("OP init scen=" + scen + " size/cap/ordered mismatch");
                continue;
            }

            if (!set || scen != curScen) {
                fail("OP without init scen=" + scen + " op=" + op);
                continue;
            }

            std::string retGot;
            if (op == "add") {
                retGot = set->add(toi(argRepr)) ? "1" : "0";
            } else if (op == "addOrGet") {
                retGot = std::to_string(set->addOrGet(toi(argRepr)));
            } else if (op == "remove") {
                retGot = set->remove(toi(argRepr)) ? "1" : "0";
            } else if (op == "contains") {
                retGot = set->contains(toi(argRepr)) ? "1" : "0";
            } else if (op == "get") {
                int32_t v = 0;
                retGot = set->get(toi(argRepr), v) ? std::to_string(v) : "ABSENT";
            } else if (op == "first") {
                retGot = std::to_string(set->first());
            } else if (op == "last") {
                retGot = std::to_string(set->last());
            } else if (op == "clear") {
                set->clear();
                retGot = "-";
            } else {
                fail("OP unknown op=" + op);
                continue;
            }

            ++total;
            std::string orderedGot = orderedCsv(*set);
            bool ok = (retGot == retExp) && (set->size() == sizeExp) &&
                      (set->capacity() == capExp) && (orderedGot == orderedExp);
            if (!ok)
                fail("OP scen=" + scen + " step=" + p[2] + " op=" + op + " arg=" +
                     argRepr + " | ret got=" + retGot + " exp=" + retExp +
                     " size got=" + std::to_string(set->size()) +
                     " exp=" + std::to_string(sizeExp) +
                     " cap got=" + std::to_string(set->capacity()) +
                     " exp=" + std::to_string(capExp) + " ordered got=[" + orderedGot +
                     "] exp=[" + orderedExp + "]");
        }
        // unknown tags ignored
    }

    std::cout << "SortedArraySet checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
