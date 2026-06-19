// Parity test for the pure analog-power computation of
// net.minecraft.world.level.block.DaylightDetectorBlock.updateSignalStrength
// (26.1.2). Ground truth: tools/DaylightDetectorPowerParity.java.
//
//   daylight_detector_power_parity --cases <daylight_detector_power.tsv>
//
// Each row carries the int effective sky brightness, the SUN_ANGLE in DEGREES as
// 8-hex raw int bits (Float.floatToRawIntBits), the inverted flag (0/1) and the
// expected redstone POWER as a decimal int. The C++ side recomputes
// computePower(sky, sunAngleDeg, inverted) and compares the EXACT integer — no
// tolerance. The float angle is reconstructed bit-for-bit from its raw bits so
// the LUT index and rounding input match the JVM exactly.
//
// Row tag:
//   PWR  <skyBrightness>  <sunAngleDegBits8>  <inverted0/1>  <power>

#include "world/level/block/DaylightDetectorPower.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float floatFromHex(const std::string& s) {
    uint32_t bits = static_cast<uint32_t>(std::stoul(s, nullptr, 16));
    return std::bit_cast<float>(bits);
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: daylight_detector_power_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];
        ++total;

        if (tag == "PWR") {
            // PWR <skyBrightness> <sunAngleDegBits8> <inverted0/1> <power>
            if (p.size() != 5) { fail("BADROW " + line); continue; }
            int sky = std::stoi(p[1]);
            float deg = floatFromHex(p[2]);
            bool inverted = std::stoi(p[3]) != 0;
            int want = std::stoi(p[4]);
            int got = mc::block_daylight::computePower(sky, deg, inverted);
            if (got != want) {
                std::ostringstream os;
                os << line << "  (got=" << got << ")";
                fail(os.str());
            }
        } else {
            fail("UNKNOWN_TAG " + tag);
        }
    }

    std::cout << "DaylightDetectorPower checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
