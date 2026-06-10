// Parity test for net.minecraft.world.level.levelgen.Column (Minecraft 26.1.2).
// Ground truth: tools/ColumnParity.java vs the real class. Pure integer column/
// ray range algebra; OptionalInt accessors + toString + the Range negative-height
// throw boundary are all compared exactly.
//
//   column_parity --cases mcpp/build/column.tsv
//
// OptionalInt is encoded in the TSV as <present 1/0>\t<value>. The "valid" flag
// column lets us assert that inside()/around()/create() throw exactly when the
// real Range ctor does (height = ceiling - floor - 1 < 0).

#include "world/level/levelgen/Column.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using mc::levelgen::Column;
using mc::levelgen::OptionalInt;

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

int i(const std::string& s) { return static_cast<int>(std::stoll(s)); }

// Read an OptionalInt encoded as two TSV fields starting at index k; advances k.
OptionalInt readOpt(const std::vector<std::string>& p, size_t& k) {
    bool present = i(p[k]) != 0;
    int  value   = i(p[k + 1]);
    k += 2;
    return present ? OptionalInt::of(value) : OptionalInt::empty();
}

// Reconstruct the base column used by WITHFLOOR/WITHCEILING (mirrors the Java
// `bases` array in tools/ColumnParity.java).
Column baseColumn(const std::string& name) {
    if (name == "line")          return Column::line();
    if (name == "below100")      return Column::below(100);
    if (name == "above50")       return Column::above(50);
    if (name == "belowm32")      return Column::below(-32);
    if (name == "above0")        return Column::above(0);
    if (name == "inside0_320")   return Column::inside(0, 320);
    if (name == "insidem64_320") return Column::inside(-64, 320);
    if (name == "around0_100")   return Column::around(0, 100);
    throw std::runtime_error("unknown base column: " + name);
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: column_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    auto eqOpt = [&](const OptionalInt& got, const OptionalInt& exp) { return got == exp; };

    // Given a successfully-constructed Column and the trailing TSV fields
    // (starting at index k: ceil opt(2), floor opt(2), height opt(2), str),
    // verify every accessor + toString. Returns true on full match.
    auto checkAccessors = [&](const Column& c, const std::vector<std::string>& p,
                              size_t k, const std::string& line) {
        size_t kk = k;
        OptionalInt expCeil   = readOpt(p, kk);
        OptionalInt expFloor  = readOpt(p, kk);
        OptionalInt expHeight = readOpt(p, kk);
        const std::string& expStr = p[kk];
        bool ok = eqOpt(c.getCeiling(), expCeil)
               && eqOpt(c.getFloor(), expFloor)
               && eqOpt(c.getHeight(), expHeight)
               && c.toString() == expStr;
        if (!ok) fail(line + " got{ceil=" + std::to_string(c.getCeiling().present) + ":" +
                      std::to_string(c.getCeiling().value) + " floor=" +
                      std::to_string(c.getFloor().present) + ":" + std::to_string(c.getFloor().value) +
                      " height=" + std::to_string(c.getHeight().present) + ":" +
                      std::to_string(c.getHeight().value) + " str=" + c.toString() + "}");
        return ok;
    };

    // Try to construct via a factory that may throw; compare validity flag, then
    // accessors. `valid` is the expected validity (1/0). `make` builds the column.
    auto checkFactory = [&](bool expValid, const std::vector<std::string>& p, size_t accIdx,
                            const std::string& line, auto&& make) {
        bool threw = false;
        Column built = Column::line();
        try {
            built = make();
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        bool gotValid = !threw;
        if (gotValid != expValid) { fail(line + " valid mismatch got=" + std::to_string(gotValid)); return; }
        if (expValid) checkAccessors(built, p, accIdx, line);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "INSIDE") {
            // INSIDE floor ceiling valid [ceilOpt floorOpt heightOpt str]
            int floor = i(p[1]), ceiling = i(p[2]);
            bool valid = i(p[3]) != 0;
            checkFactory(valid, p, 4, line, [&] { return Column::inside(floor, ceiling); });
        }
        else if (t == "AROUND") {
            int lowest = i(p[1]), highest = i(p[2]);
            bool valid = i(p[3]) != 0;
            checkFactory(valid, p, 4, line, [&] { return Column::around(lowest, highest); });
        }
        else if (t == "BELOW") {
            checkAccessors(Column::below(i(p[1])), p, 2, line);
        }
        else if (t == "FROMHIGHEST") {
            checkAccessors(Column::fromHighest(i(p[1])), p, 2, line);
        }
        else if (t == "ABOVE") {
            checkAccessors(Column::above(i(p[1])), p, 2, line);
        }
        else if (t == "FROMLOWEST") {
            checkAccessors(Column::fromLowest(i(p[1])), p, 2, line);
        }
        else if (t == "LINE") {
            checkAccessors(Column::line(), p, 2, line);
        }
        else if (t == "CREATE") {
            // CREATE <floor opt(2)> <ceil opt(2)> valid [acc...]
            size_t k = 1;
            OptionalInt floor   = readOpt(p, k);
            OptionalInt ceiling = readOpt(p, k);
            bool valid = i(p[k]) != 0;
            size_t accIdx = k + 1;
            checkFactory(valid, p, accIdx, line, [&] { return Column::create(floor, ceiling); });
        }
        else if (t == "WITHFLOOR") {
            // WITHFLOOR <baseName> <floor opt(2)> valid [acc...]
            const std::string& baseName = p[1];
            size_t k = 2;
            OptionalInt floor = readOpt(p, k);
            bool valid = i(p[k]) != 0;
            size_t accIdx = k + 1;
            Column base = baseColumn(baseName);
            checkFactory(valid, p, accIdx, line, [&] { return base.withFloor(floor); });
        }
        else if (t == "WITHCEILING") {
            const std::string& baseName = p[1];
            size_t k = 2;
            OptionalInt ceiling = readOpt(p, k);
            bool valid = i(p[k]) != 0;
            size_t accIdx = k + 1;
            Column base = baseColumn(baseName);
            checkFactory(valid, p, accIdx, line, [&] { return base.withCeiling(ceiling); });
        }
        else { fail("UNKNOWN_TAG " + t); }
    }

    std::cout << "Column cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
