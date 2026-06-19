// Parity test for net.minecraft.world.entity.MoverType (26.1.2).
//
// Ground truth: tools/MoverTypeParity.java, which iterates the REAL
// net.minecraft.world.entity.MoverType.values() and emits one MOVER row per
// constant (<ordinal>\t<name>) plus a COUNT row (values.length). This test
// replays each row against the C++ mc::MoverType enum and checks that the
// ordinal maps to the matching name verbatim, and that the value count agrees.
//
//   mover_type_parity --cases mcpp/build/mover_type.tsv
//
// There are no floats/doubles here (the enum has no fields), so comparisons are
// exact integer/string equality — which is as bit-exact as it gets.

#include "world/entity/MoverType.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::MoverType;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// Map an ordinal (Java declaration index) to the C++ enum constant. Returns
// false if the ordinal is out of the defined range [0,5).
bool moverFromOrdinal(int ord, MoverType& out) {
    if (ord < 0 || ord >= static_cast<int>(mc::MOVER_TYPE_VALUES.size())) return false;
    out = mc::MOVER_TYPE_VALUES[static_cast<size_t>(ord)];
    return true;
}
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: mover_type_parity --cases <tsv>\n";
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
        if (shown++ < 40) std::cerr << "MISMATCH " << msg << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto f = split(line);
        if (f.empty()) continue;
        const std::string& tag = f[0];

        if (tag == "MOVER") {
            // MOVER <ordinal> <name>
            if (f.size() != 3) { fail(line + " (bad arity)"); continue; }
            ++total;
            int ord = std::stoi(f[1]);
            const std::string& expectName = f[2];

            MoverType mt;
            if (!moverFromOrdinal(ord, mt)) {
                fail(line + " (ordinal out of C++ range)");
                continue;
            }
            // ordinal() round-trip and name() must both match.
            if (mc::ordinal(mt) != ord) { fail(line + " (ordinal mismatch)"); continue; }
            std::string gotName(mc::name(mt));
            if (gotName != expectName) {
                fail(line + " (name got=" + gotName + ")");
                continue;
            }
        } else if (tag == "COUNT") {
            // COUNT <values.length>
            if (f.size() != 2) { fail(line + " (bad arity)"); continue; }
            ++total;
            int count = std::stoi(f[1]);
            if (count != static_cast<int>(mc::MOVER_TYPE_VALUES.size())) {
                fail(line + " (count got=" +
                     std::to_string(mc::MOVER_TYPE_VALUES.size()) + ")");
                continue;
            }
        }
        // Unknown tags are ignored (forward-compat).
    }

    std::cout << "MoverType cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
