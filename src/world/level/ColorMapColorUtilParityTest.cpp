// Parity test for world/level/ColorMapColorUtil.h — the biome colormap sampler of
// net.minecraft.world.level.{ColorMapColorUtil,GrassColor,FoliageColor} (MC 26.1.2).
// Ground truth: tools/ColorMapColorUtilParity.java (drives the REAL static methods
// over the shipped client.jar, with a deterministic shared colormap).
//
// Row types (tab-separated, leading TAG):
//   PIX     <i_dec> <pixels[i]_dec>                         // colormap regen check
//   CONST   <name>  <value_dec>                             // published constants
//   GET     <tempBits16> <rainBits16> <default_dec> <color_dec>
//   GRASS   <tempBits16> <rainBits16> <color_dec>
//   FOLIAGE <tempBits16> <rainBits16> <color_dec>
//
// Colours / constants compared as exact 32-bit ints; temp/rain decoded from raw
// IEEE-754 double bits. The colormap is regenerated from the same integer formula
// both sides use, so it is a shared input rather than 65536 emitted rows.
//
//   colormapcolorutil_parity --cases mcpp/build/colormap_color_util.tsv

#include "ColorMapColorUtil.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cm = mc::level::colormap;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
double bd(const std::string& s) {
    return std::bit_cast<double>(static_cast<std::uint64_t>(std::stoull(s, nullptr, 16)));
}
std::int32_t di(const std::string& s) {
    // Decimal int that may be negative or up to 2147483647 / -2147483648.
    return static_cast<std::int32_t>(std::stoll(s));
}

// Same formula as ColorMapColorUtilParity.buildColormap(), in well-defined
// unsigned 32-bit arithmetic (Java int overflow == uint32 wrap, reinterpreted).
std::vector<std::int32_t> buildColormap() {
    std::vector<std::int32_t> px(65536);
    for (std::uint32_t i = 0; i < 65536u; ++i) {
        std::uint32_t v = i * 0x9E3779B1u + 0x7F4A7C15u;
        px[i] = static_cast<std::int32_t>(v);
    }
    return px;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: colormapcolorutil_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    const std::vector<std::int32_t> pixels = buildColormap();

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l, const char* tag) {
        ++mism; if (shown++ < 40) std::cerr << "MISMATCH @" << tag << "  " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "PIX") {
            std::size_t i = static_cast<std::size_t>(std::stoul(p[1]));
            std::int32_t want = di(p[2]);
            if (i >= pixels.size() || pixels[i] != want) fail(line, "PIX");
        } else if (t == "CONST") {
            const std::string& name = p[1];
            std::int32_t want = di(p[2]);
            std::int32_t got = 0;
            if (name == "FOLIAGE_EVERGREEN") got = cm::FOLIAGE_EVERGREEN;
            else if (name == "FOLIAGE_BIRCH") got = cm::FOLIAGE_BIRCH;
            else if (name == "FOLIAGE_DEFAULT") got = cm::FOLIAGE_DEFAULT;
            else if (name == "FOLIAGE_MANGROVE") got = cm::FOLIAGE_MANGROVE;
            else if (name == "GRASS_DEFAULT") got = cm::grassGet(0.5, 1.0, pixels);  // GrassColor.getDefaultColor()
            else { fail(line, "CONST_NAME"); continue; }
            if (got != want) fail(line, "CONST");
        } else if (t == "GET") {
            double temp = bd(p[1]), rain = bd(p[2]);
            std::int32_t def = di(p[3]), want = di(p[4]);
            std::int32_t got = cm::get(temp, rain, pixels, def);
            if (got != want) fail(line, "GET");
        } else if (t == "GRASS") {
            double temp = bd(p[1]), rain = bd(p[2]);
            std::int32_t want = di(p[3]);
            if (cm::grassGet(temp, rain, pixels) != want) fail(line, "GRASS");
        } else if (t == "FOLIAGE") {
            double temp = bd(p[1]), rain = bd(p[2]);
            std::int32_t want = di(p[3]);
            if (cm::foliageGet(temp, rain, pixels) != want) fail(line, "FOLIAGE");
        } else {
            fail(line, "UNKNOWN_TAG");
        }
    }

    std::cout << "ColorMapColorUtil checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
