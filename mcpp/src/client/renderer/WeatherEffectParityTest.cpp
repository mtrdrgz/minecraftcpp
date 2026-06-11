// Parity test for WeatherEffectMath.h (the pure geometry math ported from
// net.minecraft.client.renderer.WeatherEffectRenderer). Ground truth:
// tools/WeatherEffectParity.java driving the REAL constructor tables and the
// REAL private renderInstances() (vertices captured via a proxy VertexConsumer).
// Bit-exact: floats as raw IEEE-754 bits, ints decimal, doubles as raw long bits.
//
//   weather_effect_parity --cases mcpp/build/weather_effect.tsv

#include "WeatherEffectMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace w = mc::client::renderer::weather;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) {
    return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16)));
}
double bd(const std::string& s) {
    return std::bit_cast<double>(static_cast<uint64_t>(std::stoull(s, nullptr, 16)));
}
uint32_t fbits(float v) { return std::bit_cast<uint32_t>(v); }
uint32_t hx(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: weather_effect_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    // Build the column-size tables once; the TBL rows verify them entry-by-entry,
    // and the RC rows consume them through renderColumnInstance.
    float columnSizeX[1024];
    float columnSizeZ[1024];
    w::buildColumnSizeTables(columnSizeX, columnSizeZ);

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "TBL") {
            int i = std::stoi(p[1]);
            if (fbits(columnSizeX[i]) != hx(p[2]) || fbits(columnSizeZ[i]) != hx(p[3])) fail(line);
        } else if (t == "RC") {
            int columnX = std::stoi(p[1]);
            int columnZ = std::stoi(p[2]);
            int bottomY = std::stoi(p[3]);
            int topY = std::stoi(p[4]);
            float uOff = bf(p[5]);
            float vOff = bf(p[6]);
            int lightCoords = std::stoi(p[7]);
            double camX = bd(p[8]);
            double camY = bd(p[9]);
            double camZ = bd(p[10]);
            float maxAlpha = bf(p[11]);
            int radius = std::stoi(p[12]);
            float intensity = bf(p[13]);
            int32_t expColor = std::stoi(p[14]);
            int32_t expLight = std::stoi(p[15]);

            w::ColumnGeometry g = w::renderColumnInstance(
                columnSizeX, columnSizeZ, columnX, columnZ, bottomY, topY, uOff, vOff,
                lightCoords, camX, camY, camZ, maxAlpha, radius, intensity);

            bool ok = (g.color == expColor) && (g.light == expLight);
            // 4 vertices, each 5 floats, starting at column 16.
            for (int vi = 0; vi < 4 && ok; ++vi) {
                int base = 16 + vi * 5;
                if (fbits(g.vx[vi]) != hx(p[base]) ||
                    fbits(g.vy[vi]) != hx(p[base + 1]) ||
                    fbits(g.vz[vi]) != hx(p[base + 2]) ||
                    fbits(g.vu[vi]) != hx(p[base + 3]) ||
                    fbits(g.vv[vi]) != hx(p[base + 4]))
                    ok = false;
            }
            if (!ok) fail(line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "WeatherEffect checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
