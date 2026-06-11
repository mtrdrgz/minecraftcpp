// Parity test for the pure color math of
// net.minecraft.client.renderer.texture.MipmapGenerator. Ground truth:
// tools/MipmapGeneratorParity.java vs the real class (private statics via
// reflection, real NativeImage buffers). Verifies darkenedAlphaBlend,
// alphaTestCoverage and scaleAlphaToCoverage bit-exact (floats as raw IEEE-754
// bits, packed colors / coverage as ints/float-bits).
//
//   mipmap_generator_parity --cases mcpp/build/mipmap_generator.tsv

#include "MipmapGenerator.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace mm = mc::render::mipmap;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int      i(const std::string& s) { return static_cast<int>(std::stoll(s)); }
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: mipmap_generator_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != std::stoll(exp)) fail(l + " got=" + std::to_string(got));
    };
    auto eqF = [&](float got, const std::string& exp, const std::string& l) {
        if (fb(got) != static_cast<uint32_t>(std::stoul(exp, nullptr, 16)))
            fail(l + " gotbits=" + std::to_string(fb(got)));
    };

    // Image buffers keyed by tag, rebuilt from streamed PIX rows.
    std::map<std::string, mm::Image> images;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];

        // Image rebuild rows (no compare — they reconstruct the GT buffers).
        if (t.size() > 4 && t.compare(t.size() - 4, 4, "_DIM") == 0) {
            std::string tag = t.substr(0, t.size() - 4);
            images[tag] = mm::Image(i(p[1]), i(p[2]));
            continue;
        }
        if (t.size() > 4 && t.compare(t.size() - 4, 4, "_PIX") == 0) {
            std::string tag = t.substr(0, t.size() - 4);
            auto it = images.find(tag);
            if (it == images.end()) { std::cerr << "PIX before DIM for " << tag << "\n"; return 3; }
            size_t idx = static_cast<size_t>(i(p[1]));
            if (idx >= it->second.pixels.size()) { std::cerr << "PIX idx oob " << tag << " " << idx << "/" << it->second.pixels.size() << "\n"; return 3; }
            it->second.pixels[idx] = i(p[2]);
            continue;
        }

        ++total;
        if (t == "BLEND") {
            eqI(mm::darkenedAlphaBlend(i(p[1]), i(p[2]), i(p[3]), i(p[4])), p[5], line);
        } else if (t == "COVERAGE") {
            if (!images.count(p[1])) { fail(line + " MISSING_IMG " + p[1]); continue; }
            const mm::Image& img = images.at(p[1]);
            eqF(mm::alphaTestCoverage(img, bf(p[2]), bf(p[3])), p[4], line);
        } else if (t == "SCALE_PARAMS") {
            // p[1]=tag, expects <tag>_IN already streamed; apply, compare to <tag>_OUT.
            std::string tag = p[1];
            if (!images.count(tag + "_IN") || !images.count(tag + "_OUT")) { fail(line + " MISSING_INOUT " + tag); continue; }
            mm::Image img = images.at(tag + "_IN");  // copy
            mm::scaleAlphaToCoverage(img, bf(p[2]), bf(p[3]), bf(p[4]));
            const mm::Image& want = images.at(tag + "_OUT");
            if (img.width != want.width || img.height != want.height || img.pixels.size() != want.pixels.size()) {
                fail(line + " dim-mismatch");
            } else {
                for (size_t k = 0; k < img.pixels.size(); ++k) {
                    if (img.pixels[k] != want.pixels[k]) {
                        fail(line + " pixel[" + std::to_string(k) + "] got=" + std::to_string(img.pixels[k]) +
                             " want=" + std::to_string(want.pixels[k]));
                        break;
                    }
                }
            }
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "MipmapGenerator checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
