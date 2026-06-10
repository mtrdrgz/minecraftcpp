// Parity test for the net.minecraft.util.random.Weighted record + the
// WeightedList.getRandom selection.
//
// VERIFIES two ports:
//   * mc::util::Weighted<T>   (NEW header util/random/Weighted.h) — the record
//     accessors (value/weight), map(), and the weight < 0 validation.
//   * mc::util::WeightedList  (EXISTING certified header util/WeightedRandom.h,
//     reused read-only) — getRandom(RandomSource): random.nextInt(totalWeight)
//     then the Flat/Compact selector, returning the chosen list index.
//
// Ground truth: mcpp/tools/WeightedParity.java, running the REAL decompiled
// net.minecraft.util.random.Weighted / WeightedList from client.jar.
//
//   default        -> small self-checks (no Mojang files)
//   --cases <tsv>  -> verify every row of the generated reference
//
// All compared quantities are integers, so plain == is bit-exact; we still
// route the WMAP value compare through std::bit_cast<int32_t> on the wrapped
// 32-bit addition to make the two's-complement wrap explicit and bit-for-bit.
//
// TAGS (tab-separated):
//   WVAL  <value> <weight>            <gotValue> <gotWeight>
//   WMAP  <value> <weight> <addend>   <mappedValue> <mappedWeight>
//   WNEG  <weight>                    <threw>
//   WL    <seed> <n> w0..w(n-1)       <result>   (-1 == empty)

#include "util/random/Weighted.h"

#include "util/WeightedRandom.h"
#include "world/level/levelgen/RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::LegacyRandomSource;
using mc::util::Weighted;
using mc::util::WeightedList;

namespace {

int optIndex(const std::optional<int>& o) { return o.has_value() ? *o : -1; }

std::vector<int> readWeights(std::istringstream& in) {
    int n = 0;
    in >> n;
    std::vector<int> w;
    w.reserve(static_cast<size_t>(n < 0 ? 0 : n));
    for (int i = 0; i < n; ++i) {
        int x = 0;
        in >> x;
        w.push_back(x);
    }
    return w;
}

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;

    if (tag == "WVAL") {
        int value = 0, weight = 0, expVal = 0, expWeight = 0;
        in >> value >> weight >> expVal >> expWeight;
        // weight >= 0 in this battery, so construction always succeeds.
        Weighted<int> e(value, weight);
        if (e.value() != expVal || e.weight() != expWeight) {
            err = "WVAL value=" + std::to_string(value) +
                  " weight=" + std::to_string(weight) +
                  " expected=(" + std::to_string(expVal) + "," +
                  std::to_string(expWeight) + ") got=(" +
                  std::to_string(e.value()) + "," +
                  std::to_string(e.weight()) + ")";
            return false;
        }
        return true;
    }

    if (tag == "WMAP") {
        int value = 0, weight = 0, addend = 0, expVal = 0, expWeight = 0;
        in >> value >> weight >> addend >> expVal >> expWeight;
        Weighted<int> e(value, weight);
        // Java performed int + int (two's-complement wrap). Reproduce exactly via
        // 32-bit wrapping addition. map() preserves the weight verbatim.
        std::function<int(const int&)> f = [addend](const int& x) {
            int32_t r = static_cast<int32_t>(
                static_cast<uint32_t>(x) + static_cast<uint32_t>(addend));
            return std::bit_cast<int>(r);
        };
        Weighted<int> m = e.map<int>(f);
        if (m.value() != expVal || m.weight() != expWeight) {
            err = "WMAP value=" + std::to_string(value) +
                  " weight=" + std::to_string(weight) +
                  " addend=" + std::to_string(addend) +
                  " expected=(" + std::to_string(expVal) + "," +
                  std::to_string(expWeight) + ") got=(" +
                  std::to_string(m.value()) + "," +
                  std::to_string(m.weight()) + ")";
            return false;
        }
        return true;
    }

    if (tag == "WNEG") {
        int weight = 0, expThrew = 0;
        in >> weight >> expThrew;
        int threw = 0;
        try {
            Weighted<int> e(0, weight);
            (void)e;
        } catch (const std::invalid_argument&) {
            threw = 1;
        }
        if (threw != expThrew) {
            err = "WNEG weight=" + std::to_string(weight) +
                  " expected=" + std::to_string(expThrew) +
                  " got=" + std::to_string(threw);
            return false;
        }
        return true;
    }

    if (tag == "WL") {
        int64_t seed = 0;
        in >> seed;
        std::vector<int> w = readWeights(in);
        int expected = 0;
        in >> expected;
        WeightedList wl(w);
        LegacyRandomSource rng(seed);
        int got = optIndex(wl.getRandom(rng));
        if (got != expected) {
            err = "WL seed=" + std::to_string(seed) +
                  " expected=" + std::to_string(expected) +
                  " got=" + std::to_string(got);
            return false;
        }
        return true;
    }

    err = "unknown tag: " + tag;
    return false;
}

int runCases(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "Weighted: cannot open " << path << "\n";
        return 2;
    }
    std::string line;
    int n = 0, mism = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        ++n;
        std::string err;
        if (!verifyLine(line, err)) {
            if (mism < 20) std::cerr << "MISMATCH: " << err << " | " << line << "\n";
            ++mism;
        }
    }
    std::cout << "Weighted cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

int runSelfChecks() {
    int mism = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            std::cerr << "SELFCHECK FAIL: " << what << "\n";
            ++mism;
        }
    };

    // Record accessors.
    Weighted<int> e(7, 5);
    check(e.value() == 7 && e.weight() == 5, "value/weight 7,5");

    // map preserves weight, transforms value.
    std::function<int(const int&)> add3 = [](const int& x) { return x + 3; };
    Weighted<int> m = e.map<int>(add3);
    check(m.value() == 10 && m.weight() == 5, "map +3 -> 10,weight 5");

    // weight==0 is legal (constructs, no throw).
    bool zeroOk = true;
    try { Weighted<int> z(1, 0); (void)z; } catch (...) { zeroOk = false; }
    check(zeroOk, "weight 0 constructs");

    // weight<0 throws.
    bool threw = false;
    try { Weighted<int> n(1, -1); (void)n; } catch (const std::invalid_argument&) { threw = true; }
    check(threw, "weight -1 throws");

    // WL selection sanity (reused existing port): {3,1,2} total 6.
    std::vector<int> w = {3, 1, 2};
    WeightedList wl(w);
    check(!wl.isEmpty() && wl.totalWeight() == 6, "wl total 6");

    std::cout << "Weighted self-checks mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (!casesPath.empty()) return runCases(casesPath);
    return runSelfChecks();
}
