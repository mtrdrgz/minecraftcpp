// Parity test for mc::util WeightedRandom / WeightedList (util/WeightedRandom.h).
//
// Ground truth: mcpp/tools/WeightedRandomParity.java, which runs the REAL
// decompiled net.minecraft.util.random.WeightedRandom and WeightedList from
// client.jar. Each row is dispatched by TAG and the C++ port recomputes the
// selected index, compared exactly (integers, so plain == is bit-exact).
//
//   default        -> small self-checks (no Mojang files)
//   --cases <tsv>  -> verify every line of the generated reference
//
// TAG layout (tab-separated):
//   TOT  <n> w0..w(n-1)            <total>
//   WI   <n> w0..w(n-1)  <index>   <result>
//   RI   <seed> <n> w0..w(n-1)     <result>
//   WL   <seed> <n> w0..w(n-1)     <result>
// result == -1 means Optional.empty / empty list.

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

using namespace mc::util;
using mc::levelgen::LegacyRandomSource;

namespace {

// -1 sentinel for Optional.empty, matching the Java tool.
int optIndex(const std::optional<int>& o) { return o.has_value() ? *o : -1; }

// Read n then n weights from the stream.
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
    if (tag.empty()) return true;  // blank

    if (tag == "TOT") {
        std::vector<int> w = readWeights(in);
        int expected = 0;
        in >> expected;
        int got = getTotalWeight(w);
        if (got != expected) {
            err = "TOT total expected=" + std::to_string(expected) +
                  " got=" + std::to_string(got);
            return false;
        }
        return true;
    }

    if (tag == "WI") {
        std::vector<int> w = readWeights(in);
        int index = 0, expected = 0;
        in >> index >> expected;
        int got = optIndex(getWeightedItem(w, index));
        if (got != expected) {
            err = "WI index=" + std::to_string(index) +
                  " expected=" + std::to_string(expected) +
                  " got=" + std::to_string(got);
            return false;
        }
        return true;
    }

    if (tag == "RI") {
        int64_t seed = 0;
        in >> seed;
        std::vector<int> w = readWeights(in);
        int expected = 0;
        in >> expected;
        LegacyRandomSource rng(seed);
        int got = optIndex(getRandomItem(rng, w));
        if (got != expected) {
            err = "RI seed=" + std::to_string(seed) +
                  " expected=" + std::to_string(expected) +
                  " got=" + std::to_string(got);
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
        std::cerr << "WeightedRandom: cannot open " << path << "\n";
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
    std::cout << "WeightedRandom cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

// Minimal self-checks exercising the subtract loop, the Flat/Compact threshold,
// and the empty branch without any Mojang files.
int runSelfChecks() {
    int mism = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            std::cerr << "SELFCHECK FAIL: " << what << "\n";
            ++mism;
        }
    };

    // getWeightedItem subtract loop over {3,1,2} (total 6).
    std::vector<int> w = {3, 1, 2};
    check(getTotalWeight(w) == 6, "total {3,1,2}==6");
    check(optIndex(getWeightedItem(w, 0)) == 0, "wi 0->0");
    check(optIndex(getWeightedItem(w, 2)) == 0, "wi 2->0");
    check(optIndex(getWeightedItem(w, 3)) == 1, "wi 3->1");
    check(optIndex(getWeightedItem(w, 4)) == 2, "wi 4->2");
    check(optIndex(getWeightedItem(w, 5)) == 2, "wi 5->2");
    check(optIndex(getWeightedItem(w, 6)) == -1, "wi 6->empty");
    check(optIndex(getWeightedItem(w, -1)) == 0, "wi -1->0");

    // Empty / all-zero lists -> total 0, empty.
    std::vector<int> z = {0, 0, 0};
    check(getTotalWeight(z) == 0, "total zeros==0");
    WeightedList wlz(z);
    check(wlz.isEmpty(), "wl zeros empty");

    // Flat (total 62 < 64) vs Compact (total 64) selectors agree with the loop.
    std::vector<int> flat = {2, 4, 8, 16, 32};   // 62
    std::vector<int> comp = {2, 4, 8, 16, 34};   // 64
    WeightedList wlf(flat), wlc(comp);
    check(!wlf.isEmpty() && wlf.totalWeight() == 62, "flat total 62");
    check(!wlc.isEmpty() && wlc.totalWeight() == 64, "compact total 64");
    for (int sel = 0; sel < 62; ++sel) {
        check(wlf.get(sel) == optIndex(getWeightedItem(flat, sel)), "flat==loop");
    }
    for (int sel = 0; sel < 64; ++sel) {
        check(wlc.get(sel) == optIndex(getWeightedItem(comp, sel)), "compact==loop");
    }

    std::cout << "WeightedRandom self-checks mismatches=" << mism << "\n";
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
