// Parity test for the pure color math of
// net.minecraft.world.item.alchemy.PotionContents (getColorOptional / getColorOr /
// getColor / BASE_POTION_COLOR). Ground truth: tools/PotionColorParity.java vs the
// REAL class. Bit-exact on the packed 32-bit ARGB ints.
//
//   potion_color_parity --cases mcpp/build/potion_color.tsv

#include "PotionContents.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace pc = mc::alchemy;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Parse a Java decimal int that may exceed INT32 range (e.g. 0x80000000 printed as
// 2147483648 or -2147483648): read as long long then truncate to 32-bit two's-complement.
std::int32_t i32(const std::string& s) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(std::stoll(s)));
}

// Read `n` effect triples starting at field index `idx`; advances idx past them.
std::vector<pc::EffectColor> readEffects(const std::vector<std::string>& p, std::size_t& idx, int n) {
    std::vector<pc::EffectColor> effs;
    effs.reserve(static_cast<std::size_t>(n));
    for (int k = 0; k < n; ++k) {
        pc::EffectColor e;
        e.color = i32(p[idx++]);
        e.amplifier = i32(p[idx++]);
        e.visible = i32(p[idx++]) != 0;
        effs.push_back(e);
    }
    return effs;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: potion_color_parity --cases <tsv>\n";
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
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& t = p[0];
        ++total;

        if (t == "CONST") {
            // CONST  BASE_POTION_COLOR  <value>
            if (i32(p[2]) != pc::BASE_POTION_COLOR) fail(line + " got=" + std::to_string(pc::BASE_POTION_COLOR));
        } else if (t == "OPT") {
            // OPT  n  [color amp vis]*n  present  argb
            std::size_t idx = 1;
            int n = static_cast<int>(i32(p[idx++]));
            auto effs = readEffects(p, idx, n);
            int expPresent = static_cast<int>(i32(p[idx++]));
            std::int32_t expArgb = i32(p[idx++]);
            std::optional<std::int32_t> got = pc::getColorOptional(effs);
            int gotPresent = got.has_value() ? 1 : 0;
            std::int32_t gotArgb = got.has_value() ? *got : 0;
            if (gotPresent != expPresent || gotArgb != expArgb)
                fail(line + " gotPresent=" + std::to_string(gotPresent) + " gotArgb=" + std::to_string(gotArgb));
        } else if (t == "OR") {
            // OR  hasCustom  custom  default  n  [...]*n  result
            std::size_t idx = 1;
            int hasCustom = static_cast<int>(i32(p[idx++]));
            std::int32_t custom = i32(p[idx++]);
            std::int32_t dflt = i32(p[idx++]);
            int n = static_cast<int>(i32(p[idx++]));
            auto effs = readEffects(p, idx, n);
            std::int32_t expRes = i32(p[idx++]);
            std::optional<std::int32_t> cc = hasCustom ? std::optional<std::int32_t>(custom) : std::nullopt;
            std::int32_t gotRes = pc::getColorOr(cc, effs, dflt);
            if (gotRes != expRes) fail(line + " got=" + std::to_string(gotRes));
        } else if (t == "COLOR") {
            // COLOR  hasCustom  custom  n  [...]*n  result
            std::size_t idx = 1;
            int hasCustom = static_cast<int>(i32(p[idx++]));
            std::int32_t custom = i32(p[idx++]);
            int n = static_cast<int>(i32(p[idx++]));
            auto effs = readEffects(p, idx, n);
            std::int32_t expRes = i32(p[idx++]);
            std::optional<std::int32_t> cc = hasCustom ? std::optional<std::int32_t>(custom) : std::nullopt;
            std::int32_t gotRes = pc::getColor(cc, effs);
            if (gotRes != expRes) fail(line + " got=" + std::to_string(gotRes));
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "PotionContents checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
