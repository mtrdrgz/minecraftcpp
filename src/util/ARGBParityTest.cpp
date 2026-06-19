// Parity test for net.minecraft.util.ARGB. Ground truth: tools/ARGBParity.java vs
// the real class. Verifies embedded sRGB LUTs + every method, bit-exact (floats as
// raw IEEE-754 bits, packed colors as ints).
//
//   argb_parity --cases mcpp/build/argb.tsv

#include "ARGB.h"

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
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int       i(const std::string& s) { return std::stoi(s); }
long long ll(const std::string& s) { return std::stoll(s); }
float     bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t  fb(float v) { return std::bit_cast<uint32_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: argb_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) { if (got != ll(exp)) fail(l + " got=" + std::to_string(got)); };
    auto eqF = [&](float got, const std::string& exp, const std::string& l) { if (fb(got) != static_cast<uint32_t>(std::stoul(exp, nullptr, 16))) fail(l + " gotbits=" + std::to_string(fb(got))); };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "SL")        eqI(argb::ARGB_SRGB_TO_LINEAR[i(p[1])], p[2], line);
        else if (t == "LS")   eqI(argb::ARGB_LINEAR_TO_SRGB[i(p[1])], p[2], line);
        else if (t == "ALPHA") eqI(argb::alpha(i(p[1])), p[2], line);
        else if (t == "RED")   eqI(argb::red(i(p[1])), p[2], line);
        else if (t == "GREEN") eqI(argb::green(i(p[1])), p[2], line);
        else if (t == "BLUE")  eqI(argb::blue(i(p[1])), p[2], line);
        else if (t == "ALPHAF") eqF(argb::alphaFloat(i(p[1])), p[2], line);
        else if (t == "REDF")   eqF(argb::redFloat(i(p[1])), p[2], line);
        else if (t == "GREENF") eqF(argb::greenFloat(i(p[1])), p[2], line);
        else if (t == "BLUEF")  eqF(argb::blueFloat(i(p[1])), p[2], line);
        else if (t == "OPAQUE")      eqI(argb::opaque(i(p[1])), p[2], line);
        else if (t == "TRANSPARENT") eqI(argb::transparent(i(p[1])), p[2], line);
        else if (t == "GREYSCALE")   eqI(argb::greyscale(i(p[1])), p[2], line);
        else if (t == "TOABGR")      eqI(argb::toABGR(i(p[1])), p[2], line);
        else if (t == "FROMABGR")    eqI(argb::fromABGR(i(p[1])), p[2], line);
        else if (t == "SETBRIGHT")   eqI(argb::setBrightness(i(p[1]), bf(p[2])), p[3], line);
        else if (t == "MULALPHA")    eqI(argb::multiplyAlpha(i(p[1]), bf(p[2])), p[3], line);
        else if (t == "SCALEF")      eqI(argb::scaleRGB(i(p[1]), bf(p[2])), p[3], line);
        else if (t == "SCALEI")      eqI(argb::scaleRGB(i(p[1]), i(p[2])), p[3], line);
        else if (t == "SRGB2LIN")    eqF(argb::srgbToLinearChannel(i(p[1])), p[2], line);
        else if (t == "LIN2SRGB")    eqI(argb::linearToSrgbChannel(bf(p[1])), p[2], line);
        else if (t == "MULTIPLY")    eqI(argb::multiply(i(p[1]), i(p[2])), p[3], line);
        else if (t == "ADDRGB")      eqI(argb::addRgb(i(p[1]), i(p[2])), p[3], line);
        else if (t == "SUBRGB")      eqI(argb::subtractRgb(i(p[1]), i(p[2])), p[3], line);
        else if (t == "ALPHABLEND")  eqI(argb::alphaBlend(i(p[1]), i(p[2])), p[3], line);
        else if (t == "AVERAGE")     eqI(argb::average(i(p[1]), i(p[2])), p[3], line);
        else if (t == "MEANLINEAR")  eqI(argb::meanLinear(i(p[1]), i(p[2]), i(p[1]), i(p[2])), p[3], line);
        else if (t == "SRGBLERP") {
            float al = bf(p[4]); int a = i(p[2]), b = i(p[3]);
            eqI(argb::srgbLerp(al, a, b), p[5], line);
            eqI(argb::linearLerp(al, a, b), p[6], line);
        }
        else if (t == "COLOR4") eqI(argb::color(i(p[1]), i(p[2]), i(p[3]), i(p[4])), p[5], line);
        else if (t == "COLOR3") eqI(argb::color(i(p[1]), i(p[2]), i(p[3])), p[4], line);
        else if (t == "WHITEF") eqI(argb::white(bf(p[1])), p[2], line);
        else if (t == "BLACKF") eqI(argb::black(bf(p[1])), p[2], line);
        else if (t == "GRAY")   eqI(argb::gray(bf(p[1])), p[2], line);
        else if (t == "AS8BIT") eqI(argb::as8BitChannel(bf(p[1])), p[2], line);
        else if (t == "COLORFROMFLOAT") eqI(argb::colorFromFloat(bf(p[1]), bf(p[1]), bf(p[1]), bf(p[1])), p[2], line);
        else if (t == "WHITEI") eqI(argb::white(i(p[1])), p[2], line);
        else if (t == "BLACKI") eqI(argb::black(i(p[1])), p[2], line);
        else fail("UNKNOWN_TAG " + t);
    }

    std::cout << "ARGB cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
