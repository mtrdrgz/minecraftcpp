// Parity test for net.minecraft.world.level.saveddata.maps.MapId (single-int record).
// Ground truth: tools/MapIdParity.java vs the real class. Every exercised method is
// recomputed via world/level/saveddata/maps/MapId.h and compared exactly (integral /
// boolean outputs bit-for-bit; key() compared as a raw string).
//
//   map_id_parity --cases mcpp/build/map_id.tsv
//
// Row formats (TAG \t inputs... \t outputs...):
//   ID        id            | id()
//   HASH      id            | hashCode()
//   KEY       id            | key()  (raw string "maps/" + id)
//   EQUALS    idA idB       | equals(0/1)

#include "MapId.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::world::level::saveddata::maps::MapId;

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
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: map_id_parity --cases <tsv>\n";
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

        if (tag == "ID") {  // id | id()
            MapId m(i(p[1]));
            bad = m.id() != i(p[2]);
        } else if (tag == "HASH") {  // id | hashCode()
            MapId m(i(p[1]));
            bad = m.hashCode() != i(p[2]);
        } else if (tag == "KEY") {  // id | "maps/" + id
            MapId m(i(p[1]));
            bad = m.key() != p[2];
        } else if (tag == "EQUALS") {  // idA idB | equals
            MapId a(i(p[1]));
            MapId b(i(p[2]));
            bad = (a.equals(b) ? 1 : 0) != i(p[3]);
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

    std::cout << "MapId cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
