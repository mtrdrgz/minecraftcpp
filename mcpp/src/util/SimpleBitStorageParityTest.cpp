// Parity test for net.minecraft.util.SimpleBitStorage. Ground truth:
// tools/SimpleBitStorageParity.java vs the real class. Verifies the embedded MAGIC
// table, cellIndex (magic division), constructor packing (data[] longs),
// get()/getAndSet(), all bit-for-bit (longs compared exactly).
//
//   bit_storage_parity --cases mcpp/build/bit_storage.tsv

#include "SimpleBitStorage.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using mc::util::SimpleBitStorage;
using mc::util::SIMPLE_BIT_STORAGE_MAGIC;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int       i(const std::string& s) { return std::stoi(s); }
long long ll(const std::string& s) { return std::stoll(s); }

// Same deterministic per-index value as the Java tool.
int val(int idx, uint64_t mask) { return static_cast<int>((static_cast<int64_t>(idx) * 2654435761LL + 12345LL) & static_cast<int64_t>(mask)); }

std::vector<int> makeValues(int bits, int size) {
    uint64_t mask = (1ULL << bits) - 1ULL;
    std::vector<int> v(size);
    for (int k = 0; k < size; ++k) v[k] = val(k, mask);
    return v;
}
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: bit_storage_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    std::map<std::pair<int,int>, std::unique_ptr<SimpleBitStorage>> ctorStore;
    std::map<int, std::unique_ptr<SimpleBitStorage>> gasStore; // keyed by bits (size fixed 64)

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "MAGIC") {
            if (SIMPLE_BIT_STORAGE_MAGIC[i(p[1])] != i(p[2])) fail(line + " embedded=" + std::to_string(SIMPLE_BIT_STORAGE_MAGIC[i(p[1])]));
        } else if (t == "MAGICLEN") {
            if (i(p[1]) != 192) fail(line + " embedded-len=192");
        } else if (t == "CTORLEN") {
            int bits = i(p[1]), size = i(p[2]);
            auto sbs = std::make_unique<SimpleBitStorage>(bits, size, makeValues(bits, size));
            if (static_cast<int>(sbs->getRaw().size()) != i(p[3])) fail(line + " got=" + std::to_string(sbs->getRaw().size()));
            ctorStore[{bits, size}] = std::move(sbs);
        } else if (t == "RAW") {
            auto& sbs = ctorStore[{i(p[1]), i(p[2])}];
            if (!sbs || sbs->getRaw()[i(p[3])] != ll(p[4])) fail(line);
        } else if (t == "CELL") {
            auto& sbs = ctorStore[{i(p[1]), i(p[2])}];
            if (!sbs || sbs->cellIndex(i(p[3])) != i(p[4])) fail(line);
        } else if (t == "GET") {
            auto& sbs = ctorStore[{i(p[1]), i(p[2])}];
            if (!sbs || sbs->get(i(p[3])) != i(p[4])) fail(line);
        } else if (t == "GAS") {
            int bits = i(p[1]), size = i(p[2]);
            auto& sbs = gasStore[bits];
            if (!sbs) sbs = std::make_unique<SimpleBitStorage>(bits, size, std::vector<int>(size, 0));
            int old = sbs->getAndSet(i(p[3]), i(p[4]));
            if (old != i(p[5])) fail(line + " got=" + std::to_string(old));
        } else if (t == "GASRAW") {
            auto& sbs = gasStore[i(p[1])];
            if (!sbs || sbs->getRaw()[i(p[3])] != ll(p[4])) fail(line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "SimpleBitStorage cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
