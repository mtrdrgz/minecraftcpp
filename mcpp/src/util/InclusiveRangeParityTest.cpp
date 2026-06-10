// Parity test for net.minecraft.util.InclusiveRange (int specialization).
// Ground truth: tools/InclusiveRangeParity.java vs the real class. Every exercised
// method is recomputed via util/InclusiveRange.h and compared exactly (all integral /
// boolean outputs; toString compared as a raw string).
//
//   inclusive_range_parity --cases mcpp/build/inclusive_range.tsv
//
// Row formats (TAG \t inputs... \t outputs...):
//   INRANGE     min max value | inRange(0/1)
//   CONTAINS    aMin aMax bMin bMax | aContainsB(0/1)
//   CTOR        min max | constructible(0/1)
//   CREATE      min max | ok(0/1)
//   TOSTRING    min max | "[min, max]"

#include "InclusiveRange.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using mc::util::InclusiveRange;
using mc::util::inclusiveRangeCreate;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Java ints are signed 32-bit (MIN_VALUE prints as -2147483648).
int32_t i(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

// Mirror InclusiveRange's canonical ctor: constructible iff it does not throw.
bool constructible(int32_t min, int32_t max) {
    try {
        InclusiveRange<int32_t> r(min, max);
        (void)r;
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: inclusive_range_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long n = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // tolerate CRLF
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++n;
        bool bad = false;

        if (tag == "INRANGE") {  // min max value | inRange
            InclusiveRange<int32_t> r(i(p[1]), i(p[2]));
            bad = (r.isValueInRange(i(p[3])) ? 1 : 0) != i(p[4]);
        } else if (tag == "CONTAINS") {  // aMin aMax bMin bMax | contains
            InclusiveRange<int32_t> a(i(p[1]), i(p[2]));
            InclusiveRange<int32_t> b(i(p[3]), i(p[4]));
            bad = (a.contains(b) ? 1 : 0) != i(p[5]);
        } else if (tag == "CTOR") {  // min max | constructible
            bad = (constructible(i(p[1]), i(p[2])) ? 1 : 0) != i(p[3]);
        } else if (tag == "CREATE") {  // min max | ok
            auto res = inclusiveRangeCreate<int32_t>(i(p[1]), i(p[2]));
            bad = (res.ok ? 1 : 0) != i(p[3]);
        } else if (tag == "TOSTRING") {  // min max | "[min, max]"
            InclusiveRange<int32_t> r(i(p[1]), i(p[2]));
            bad = r.toString() != p[3];
        } else {
            std::cerr << "unknown tag: " << tag << "\n";
            ++mism;
            continue;
        }

        if (bad) {
            ++mism;
            if (mism <= 20) std::cerr << "MISMATCH [" << tag << "] line: " << line << "\n";
        }
    }

    std::cout << "InclusiveRange cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
