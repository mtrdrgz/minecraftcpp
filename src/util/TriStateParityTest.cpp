// Parity test for net.minecraft.util.TriState.
// Ground truth: tools/TriStateParity.java vs the real enum. Every exercised method
// is recomputed via util/TriState.h and compared exactly (all integral/boolean
// outputs; serialized names compared as raw strings).
//
//   tristate_parity --cases mcpp/build/tristate.tsv
//
// Row formats (TAG \t inputs... \t outputs...):
//   FROM       value(0/1)            | ordinal(int)            triStateFrom
//   TOBOOL     ordinal default(0/1)  | result(0/1)            triStateToBoolean
//   NAME       ordinal               | serializedName(string) triStateGetSerializedName
//   ORDINAL    name(string)          | ordinal(int)           enum declaration order

#include "TriState.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::util::TriState;
using mc::util::triStateFrom;
using mc::util::triStateGetSerializedName;
using mc::util::triStateToBoolean;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int32_t i(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

// Map an enum ordinal (as emitted by Java's TriState.ordinal()) to the C++ enum.
// Declaration order: TRUE=0, FALSE=1, DEFAULT=2. Throws via -1 sentinel handling.
TriState fromOrdinal(int32_t ord, bool& ok) {
    ok = true;
    switch (ord) {
        case 0:
            return TriState::TRUE;
        case 1:
            return TriState::FALSE;
        case 2:
            return TriState::DEFAULT;
        default:
            ok = false;
            return TriState::DEFAULT;
    }
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: tristate_parity --cases <tsv>\n";
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

        if (tag == "FROM") {  // value | ordinal
            TriState r = triStateFrom(i(p[1]) != 0);
            bad = static_cast<int32_t>(r) != i(p[2]);
        } else if (tag == "TOBOOL") {  // ordinal default | result
            bool ok = false;
            TriState s = fromOrdinal(i(p[1]), ok);
            if (!ok) {
                std::cerr << "bad ordinal: " << line << "\n";
                bad = true;
            } else {
                bad = (triStateToBoolean(s, i(p[2]) != 0) ? 1 : 0) != i(p[3]);
            }
        } else if (tag == "NAME") {  // ordinal | serializedName
            bool ok = false;
            TriState s = fromOrdinal(i(p[1]), ok);
            if (!ok) {
                std::cerr << "bad ordinal: " << line << "\n";
                bad = true;
            } else {
                bad = triStateGetSerializedName(s) != p[2];
            }
        } else if (tag == "ORDINAL") {  // name | ordinal
            // Map the Java enum constant name to the expected C++ ordinal, then
            // confirm the serialized name + ordinal agree with our enum.
            const std::string& name = p[1];
            int32_t expectOrd = i(p[2]);
            TriState s;
            std::string expectSerialized;
            if (name == "TRUE") {
                s = TriState::TRUE;
                expectSerialized = "true";
            } else if (name == "FALSE") {
                s = TriState::FALSE;
                expectSerialized = "false";
            } else if (name == "DEFAULT") {
                s = TriState::DEFAULT;
                expectSerialized = "default";
            } else {
                std::cerr << "unknown enum name: " << line << "\n";
                ++mism;
                continue;
            }
            bad = static_cast<int32_t>(s) != expectOrd ||
                  triStateGetSerializedName(s) != expectSerialized;
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

    std::cout << "TriState cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
