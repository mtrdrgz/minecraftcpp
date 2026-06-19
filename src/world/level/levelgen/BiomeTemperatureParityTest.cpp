// Parity test for mc::biome::BiomeTemperature (world/level/biome/BiomeTemperature.h)
// vs the REAL decompiled net.minecraft.world.level.biome.Biome temperature math.
//
// The header ports only the registry-free temperature curve:
//   modifyTemperature (NONE/FROZEN) -> getHeightAdjustedTemperature -> getTemperature,
// reusing the certified mc::levelgen::PerlinSimplexNoise (Noise.{h,cpp}) for the three
// noise statics, built exactly as Biome.java's static initializers do.
//
// Ground truth: mcpp/tools/BiomeTemperatureParity.java, which builds a REAL Biome and
// reflectively calls the private getHeightAdjustedTemperature / getTemperature. Floats
// are compared by raw IEEE bits via std::bit_cast (never by value).
//
//   --cases <tsv>  -> verify every HAT/TMP row of the generated reference
//   default        -> a small self-consistency smoke test (no jar needed)

#include "../biome/BiomeTemperature.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::biome::BiomeTemperature;
using mc::biome::TemperatureModifier;

namespace {

float fb(const std::string& s) {
    return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16)));
}
uint32_t bf(float v) { return std::bit_cast<uint32_t>(v); }

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream in(line);
    while (std::getline(in, cur, '\t')) out.push_back(cur);
    return out;
}

TemperatureModifier modOf(int i) {
    return i == 1 ? TemperatureModifier::FROZEN : TemperatureModifier::NONE;
}

} // namespace

int main(int argc, char** argv) {
    BiomeTemperature bt;

    if (argc <= 2 || std::string(argv[1]) != "--cases") {
        // Smoke test (no jar): values are finite; below snowLevel HAT==modifyTemperature;
        // NONE modifier leaves the base temp untouched below snowLevel.
        float none = bt.getHeightAdjustedTemperature(TemperatureModifier::NONE, 0.8F, 10, 60, 20, 63);
        bool ok = (none == 0.8F);                       // y=60 <= seaLevel+17=80 -> unchanged
        float frozen = bt.getTemperature(TemperatureModifier::FROZEN, 0.0F, 100, 64, 200, 63);
        ok = ok && (frozen == frozen);                  // not NaN
        std::cout << "BiomeTemperature self-test " << (ok ? "passed" : "FAILED") << '\n';
        return ok ? 0 : 1;
    }

    std::ifstream f(argv[2]);
    if (!f) { std::cerr << "cannot open " << argv[2] << '\n'; return 2; }

    std::string line;
    long n = 0, bad = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::vector<std::string> t = split(line);
        if (t.size() < 8) continue;
        const std::string& tag = t[0];
        if (tag != "HAT" && tag != "TMP") continue;

        // <tag> baseTempBits modifier x y z seaLevel valueBits
        float baseTemp = fb(t[1]);
        int mi = std::stoi(t[2]);
        int32_t x = std::stoi(t[3]);
        int32_t y = std::stoi(t[4]);
        int32_t z = std::stoi(t[5]);
        int32_t seaLevel = std::stoi(t[6]);
        uint32_t ev = static_cast<uint32_t>(std::stoul(t[7], nullptr, 16));

        float got = (tag == "HAT")
            ? bt.getHeightAdjustedTemperature(modOf(mi), baseTemp, x, y, z, seaLevel)
            : bt.getTemperature(modOf(mi), baseTemp, x, y, z, seaLevel);

        ++n;
        uint32_t gv = bf(got);
        if (gv != ev) {
            ++bad;
            if (bad <= 20)
                std::cerr << "MISMATCH " << tag << " base=" << baseTemp << " mod=" << mi
                          << " x=" << x << " y=" << y << " z=" << z << " sea=" << seaLevel
                          << " got " << std::hex << gv << " != " << ev << std::dec << '\n';
        }
    }

    std::cout << "BiomeTemperature cases=" << n << " mismatches=" << bad << '\n';
    return bad == 0 ? 0 : 1;
}
