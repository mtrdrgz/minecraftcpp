// Parity test for VerticalAnchor / HeightProvider / FloatProvider. Ground truth:
// tools/HeightFloatProviderParity.java (the real decompiled classes).
//
//   default        -> hardcoded self-checks (no Mojang files)
//   --cases <tsv>  -> verify every line of the generated reference

#include "../FloatProvider.h"
#include "../RandomSource.h"
#include "../VerticalAnchor.h"
#include "../WorldGenerationContext.h"
#include "HeightProvider.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen;
using namespace mc::levelgen::heightproviders;
using mc::valueproviders::ClampedNormalFloat;
using mc::valueproviders::ConstantFloat;
using mc::valueproviders::FloatProviderPtr;
using mc::valueproviders::TrapezoidFloat;
using mc::valueproviders::UniformFloat;

namespace {

const WorldGenerationContext g_ctx(-64, 384);

std::map<std::string, VerticalAnchorPtr> buildAnchors() {
    using namespace VerticalAnchors;
    return {
        { "abs50", absolute(50) }, { "ab10", aboveBottom(10) }, { "bt20", belowTop(20) },
        { "bottom", bottom() }, { "top", top() },
    };
}

std::map<std::string, HeightProviderPtr> buildHeights() {
    using namespace VerticalAnchors;
    std::map<std::string, HeightProviderPtr> m;
    m["const50"] = std::make_shared<ConstantHeight>(absolute(50));
    m["uni_full"] = std::make_shared<UniformHeight>(aboveBottom(0), belowTop(0));
    m["uni_abs"] = std::make_shared<UniformHeight>(absolute(60), absolute(70));
    m["bias"] = std::make_shared<BiasedToBottomHeight>(absolute(-20), absolute(40), 1);
    m["verybias"] = std::make_shared<VeryBiasedToBottomHeight>(absolute(0), absolute(50), 2);
    m["trap"] = std::make_shared<TrapezoidHeight>(absolute(0), absolute(100), 20);
    return m;
}

std::map<std::string, FloatProviderPtr> buildFloats() {
    return {
        { "cf", ConstantFloat::of(0.5F) },
        { "uf01", UniformFloat::of(0.0F, 1.0F) },
        { "uf_neg", UniformFloat::of(-2.0F, 3.0F) },
        { "cnf", ClampedNormalFloat::of(0.0F, 1.0F, -2.0F, 2.0F) },
        { "tf", TrapezoidFloat::of(0.0F, 10.0F, 4.0F) },
    };
}

const auto g_anchors = buildAnchors();
const auto g_heights = buildHeights();
const auto g_floats = buildFloats();

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string kind;
    in >> kind;
    if (kind.empty()) return true;

    if (kind == "ANCHOR") {
        std::string name;
        int expected;
        in >> name >> expected;
        auto it = g_anchors.find(name);
        if (it == g_anchors.end()) { err = "unknown anchor " + name; return false; }
        const int got = it->second->resolveY(g_ctx);
        if (got != expected) { err = "ANCHOR " + name + " " + std::to_string(got) + "!=" + std::to_string(expected); return false; }
        return true;
    }
    if (kind == "HEIGHT") {
        std::string name;
        long long seed;
        in >> name >> seed;
        auto it = g_heights.find(name);
        if (it == g_heights.end()) { err = "unknown height " + name; return false; }
        LegacyRandomSource r(seed);
        for (int i = 0; i < 8; ++i) {
            int expected;
            in >> expected;
            const int got = it->second->sample(r, g_ctx);
            if (got != expected) {
                err = "HEIGHT " + name + " seed " + std::to_string(seed) + "[" + std::to_string(i) + "] " +
                      std::to_string(got) + "!=" + std::to_string(expected);
                return false;
            }
        }
        return true;
    }
    if (kind == "FLOAT") {
        std::string name;
        long long seed;
        in >> name >> seed;
        auto it = g_floats.find(name);
        if (it == g_floats.end()) { err = "unknown float " + name; return false; }
        LegacyRandomSource r(seed);
        for (int i = 0; i < 8; ++i) {
            int32_t expected;
            in >> expected;
            const int32_t got = std::bit_cast<int32_t>(it->second->sample(r));
            if (got != expected) {
                err = "FLOAT " + name + " seed " + std::to_string(seed) + "[" + std::to_string(i) + "] bits " +
                      std::to_string(got) + "!=" + std::to_string(expected);
                return false;
            }
        }
        return true;
    }
    return true;
}

const std::vector<std::string> kHardcoded = {
    "ANCHOR\tab10\t-54",
    "ANCHOR\tbt20\t299",
    "ANCHOR\ttop\t319",
    "HEIGHT\tuni_full\t0\t-16\t-36\t-3\t31\t187\t-35\t283\t65",
    "HEIGHT\tbias\t0\t-20\t27\t9\t-11\t-6\t-3\t-10\t12",
    "HEIGHT\tverybias\t0\t45\t1\t1\t17\t2\t3\t1\t17",
    "HEIGHT\ttrap\t0\t79\t59\t69\t60\t54\t36\t33\t62",
    "FLOAT\tuf01\t0\t1060839604\t1062525265\t1047940908\t1058748784\t1059270089\t1050557408\t1057810800\t1039114536",
    "FLOAT\tcnf\t0\t1062040271\t-1083782215\t1073741824\t1061389947\t1065094420\t-1076397554\t-1126199340\t1038878192",
    "FLOAT\ttf\t0\t1089703452\t1080044983\t1085043530\t1082558416\t1087430818\t1078317211\t1083253314\t1092450883",
};

} // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[1]) == "--cases") {
        std::ifstream f(argv[2]);
        if (!f) { std::cerr << "cannot open " << argv[2] << '\n'; return 2; }
        std::string line;
        long n = 0, bad = 0;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::string err;
            ++n;
            if (!verifyLine(line, err)) {
                ++bad;
                if (bad <= 20) std::cerr << "MISMATCH: " << err << '\n';
            }
        }
        std::cout << "height/float cases=" << n << " mismatches=" << bad << '\n';
        return bad == 0 ? 0 : 1;
    }

    bool ok = true;
    for (const auto& line : kHardcoded) {
        std::string err;
        if (!verifyLine(line, err)) { ok = false; std::cerr << "FAIL: " << err << '\n'; }
    }
    if (!ok) { std::cerr << "Height/Float provider parity checks FAILED\n"; return 1; }
    std::cout << "Height/Float provider parity checks passed\n";
    return 0;
}
