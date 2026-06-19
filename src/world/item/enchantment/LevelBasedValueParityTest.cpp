// Parity test for net.minecraft.world.item.enchantment.LevelBasedValue.
// Ground truth: tools/LevelBasedValueParity.java vs the real records.
//
// For each TAG, we rebuild the SAME LevelBasedValue node tree the Java GT built,
// run calculate(level) for the level in each row, and compare the result's raw
// IEEE-754 bits against the Java float bits. Any mismatch is a real 1:1 port bug
// in the math (int-square overflow, (level-1) wrap, pow narrowing, clamp/min
// semantics, fraction zero-guard).
//
//   level_based_value_parity --cases mcpp/build/level_based_value.tsv

#include "LevelBasedValue.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace lbv = mc::world::item::enchantment;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// Java int level printed as signed decimal; parse via long long then narrow to
// the exact 32-bit two's-complement value (Integer.MIN_VALUE..MAX_VALUE).
std::int32_t parseLevel(const std::string& s) {
    return static_cast<std::int32_t>(std::stoll(s));
}

std::uint32_t floatBits(float v) {
    std::uint32_t b;
    std::memcpy(&b, &v, 4);
    return b;
}

using lbv::LevelBasedValue;
using lbv::LBVPtr;

// Build the registry of named trees, mirroring LevelBasedValueParity.main() 1:1.
std::map<std::string, LBVPtr> buildTrees() {
    std::map<std::string, LBVPtr> m;

    // Constant
    m["CONST_0"] = LevelBasedValue::constant(0.0f);
    m["CONST_2_5"] = LevelBasedValue::constant(2.5f);
    m["CONST_NEG"] = LevelBasedValue::constant(-7.25f);
    m["CONST_FRAC"] = LevelBasedValue::constant(0.1f);

    // Linear (perLevel(p) == perLevel(p,p))
    m["LIN_1_1"] = LevelBasedValue::linear(1.0f, 1.0f);
    m["LIN_2_3"] = LevelBasedValue::linear(2.0f, 3.0f);
    m["LIN_5_05"] = LevelBasedValue::linear(5.0f, 0.5f);
    m["LIN_NEG"] = LevelBasedValue::linear(-3.0f, -1.5f);
    m["LIN_BIG"] = LevelBasedValue::linear(0.0f, 100000.0f);

    // LevelsSquared
    m["SQ_0"] = LevelBasedValue::levelsSquared(0.0f);
    m["SQ_1_5"] = LevelBasedValue::levelsSquared(1.5f);
    m["SQ_NEG"] = LevelBasedValue::levelsSquared(-2.0f);

    // Clamped
    m["CLAMP_LIN"] = LevelBasedValue::clamped(LevelBasedValue::linear(2.0f, 3.0f), 1.0f, 20.0f);
    m["CLAMP_SQ"] = LevelBasedValue::clamped(LevelBasedValue::levelsSquared(0.0f), -5.0f, 50.0f);
    m["CLAMP_CONST"] = LevelBasedValue::clamped(LevelBasedValue::constant(100.0f), -10.0f, 10.0f);
    m["CLAMP_INVERT"] = LevelBasedValue::clamped(LevelBasedValue::linear(1.0f, 1.0f), 10.0f, 0.0f);

    // Fraction
    m["FRAC_LIN"] = LevelBasedValue::fraction(LevelBasedValue::linear(10.0f, 1.0f), LevelBasedValue::linear(1.0f, 1.0f));
    m["FRAC_ZERODEN"] = LevelBasedValue::fraction(LevelBasedValue::constant(5.0f), LevelBasedValue::levelsSquared(0.0f));
    m["FRAC_CONST"] = LevelBasedValue::fraction(LevelBasedValue::constant(1.0f), LevelBasedValue::constant(3.0f));
    m["FRAC_NEG"] = LevelBasedValue::fraction(LevelBasedValue::constant(-1.0f), LevelBasedValue::linear(2.0f, -1.0f));

    // Exponent
    m["EXP_2_LIN"] = LevelBasedValue::exponent(LevelBasedValue::constant(2.0f), LevelBasedValue::linear(0.0f, 1.0f));
    m["EXP_LIN_2"] = LevelBasedValue::exponent(LevelBasedValue::linear(1.0f, 1.0f), LevelBasedValue::constant(2.0f));
    m["EXP_HALF"] = LevelBasedValue::exponent(LevelBasedValue::levelsSquared(0.0f), LevelBasedValue::constant(0.5f));
    m["EXP_NEGBASE"] = LevelBasedValue::exponent(LevelBasedValue::constant(-2.0f), LevelBasedValue::linear(0.0f, 1.0f));

    // Lookup
    m["LOOK_3"] = LevelBasedValue::lookup({1.0f, 4.0f, 9.0f}, LevelBasedValue::constant(99.0f));
    m["LOOK_FALLLIN"] = LevelBasedValue::lookup({0.5f, 1.5f}, LevelBasedValue::linear(2.0f, 1.0f));
    m["LOOK_1"] = LevelBasedValue::lookup({7.0f}, LevelBasedValue::constant(0.0f));

    // Nested: clamp( fraction( squared(2), linear(1,1) ), 0, 100 )
    m["NEST_CFSL"] = LevelBasedValue::clamped(
        LevelBasedValue::fraction(
            LevelBasedValue::levelsSquared(2.0f),
            LevelBasedValue::linear(1.0f, 1.0f)),
        0.0f, 100.0f);

    return m;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: level_based_value_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    const auto trees = buildTrees();

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& msg) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << msg << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF tolerant
        if (line.empty()) continue;
        auto p = split(line);
        if (p.size() != 3) {
            fail("BAD_ROW " + line);
            continue;
        }
        ++total;

        const std::string& tag = p[0];
        const std::int32_t level = parseLevel(p[1]);

        // Parse the expected float as raw 32-bit hex bits.
        std::uint32_t expBits = static_cast<std::uint32_t>(std::stoul(p[2], nullptr, 16));

        auto it = trees.find(tag);
        if (it == trees.end()) {
            fail("UNKNOWN_TAG " + tag);
            continue;
        }

        const float got = it->second->calculate(level);
        const std::uint32_t gotBits = floatBits(got);

        // NaN payload note: Java's String.format prints whatever floatToRawIntBits
        // yields; both sides produce canonical NaNs from the same ops, so exact
        // bit-compare is the correct (and strictest) gate.
        if (gotBits != expBits) {
            std::ostringstream os;
            os << tag << " level=" << level << " got=" << std::hex << gotBits
               << " exp=" << expBits;
            fail(os.str());
        }
    }

    std::cout << "LevelBasedValue checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
