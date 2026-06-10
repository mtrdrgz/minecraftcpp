// Second parity GATE for net.minecraft.util.ARGB — VERIFIES the existing certified
// port mcpp/src/util/ARGB.h against fresh ground truth (tools/ArgbVerifyParity.java).
//
// Focus: the methods named in the verification assignment — color(a,r,g,b),
// red/green/blue/alpha, srgbLerp/linearLerp, multiply, scaleRGB (all three overloads),
// opaque, colorFromFloat — PLUS the two-arg color() overloads color(int,int) /
// color(float,int) (header: colorAlphaRgb / colorAlphaFRgb) which the original
// argb_parity gate did not exercise. Bit-exact: floats compared as raw IEEE-754
// bits, packed colors as 32-bit ints.
//
//   argb_verify_parity --cases mcpp/build/argb_verify.tsv

#include "util/ARGB.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace argb = mc::argb;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Java ints are emitted as signed decimal; parse via long long then narrow so values
// like 0x89ABCDEF (printed as a negative int by Java) round-trip exactly.
int       i(const std::string& s)  { return static_cast<int>(std::stoll(s)); }
long long ll(const std::string& s) { return std::stoll(s); }
float     bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: argb_verify_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != ll(exp)) fail(l + " got=" + std::to_string(got));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& t = p[0];
        ++total;

        if (t == "ALPHA")            eqI(argb::alpha(i(p[1])), p[2], line);
        else if (t == "RED")         eqI(argb::red(i(p[1])), p[2], line);
        else if (t == "GREEN")       eqI(argb::green(i(p[1])), p[2], line);
        else if (t == "BLUE")        eqI(argb::blue(i(p[1])), p[2], line);
        else if (t == "OPAQUE")      eqI(argb::opaque(i(p[1])), p[2], line);
        else if (t == "TRANSPARENT") eqI(argb::transparent(i(p[1])), p[2], line);
        else if (t == "MULTIPLY")    eqI(argb::multiply(i(p[1]), i(p[2])), p[3], line);
        else if (t == "SCALEF")      eqI(argb::scaleRGB(i(p[1]), bf(p[2])), p[3], line);
        else if (t == "SCALE3")      eqI(argb::scaleRGB(i(p[1]), bf(p[2]), bf(p[3]), bf(p[4])), p[5], line);
        else if (t == "SCALEI")      eqI(argb::scaleRGB(i(p[1]), i(p[2])), p[3], line);
        else if (t == "COLOR_AI")    eqI(argb::colorAlphaRgb(i(p[1]), i(p[2])), p[3], line);
        else if (t == "COLOR_AF")    eqI(argb::colorAlphaFRgb(bf(p[1]), i(p[2])), p[3], line);
        else if (t == "COLOR4")      eqI(argb::color(i(p[1]), i(p[2]), i(p[3]), i(p[4])), p[5], line);
        else if (t == "COLOR3")      eqI(argb::color(i(p[1]), i(p[2]), i(p[3])), p[4], line);
        else if (t == "CFF")         eqI(argb::colorFromFloat(bf(p[1]), bf(p[2]), bf(p[3]), bf(p[4])), p[5], line);
        else if (t == "AS8BIT")      eqI(argb::as8BitChannel(bf(p[1])), p[2], line);
        else if (t == "SRGBLERP")    eqI(argb::srgbLerp(bf(p[1]), i(p[2]), i(p[3])), p[4], line);
        else if (t == "LINEARLERP")  eqI(argb::linearLerp(bf(p[1]), i(p[2]), i(p[3])), p[4], line);
        else fail("UNKNOWN_TAG " + t);
    }

    std::cout << "ArgbVerify cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
