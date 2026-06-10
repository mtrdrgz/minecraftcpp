// VERIFY gate for the existing engine header mcpp/src/util/Brightness.h.
//
// Ground truth: tools/BrightnessVerifyParity.java vs the REAL net.minecraft.util.Brightness
// record. This pins the Brightness record's own surface — pack(block, sky), unpack() ->
// block()/sky(), and the FULL_BRIGHT constant — over the full physical 0..15 light domain,
// recomputed via util/Brightness.h and compared bit-for-bit (all values are plain int32).
//
// The deeper LightCoordsUtil bit math that pack()/unpack() delegate to is already certified
// by brightness_parity; this gate is a focused re-verification of the Brightness wrapper.
//
//   brightness_verify_parity --cases mcpp/build/brightness_verify.tsv

#include "../../../util/Brightness.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::util::Brightness;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Java ints are signed 32-bit; some print as negative (e.g. 0x80000000 -> -2147483648).
int32_t i(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: brightness_verify_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++n;
        bool bad = false;

        if (tag == "PACK") {  // block sky | packed
            Brightness br{i(p[1]), i(p[2])};
            bad = br.pack() != i(p[3]);
        } else if (tag == "UNPACK") {  // packed | block sky
            Brightness u = Brightness::unpack(i(p[1]));
            bad = u.block != i(p[2]) || u.sky != i(p[3]);
        } else if (tag == "FULL") {  // block sky packed
            bad = mc::util::BRIGHTNESS_FULL_BRIGHT.block != i(p[1]) ||
                  mc::util::BRIGHTNESS_FULL_BRIGHT.sky != i(p[2]) ||
                  mc::util::BRIGHTNESS_FULL_BRIGHT.pack() != i(p[3]);
        } else {
            std::cerr << "unknown tag: " << tag << "\n";
            ++mism;
            continue;
        }

        if (bad) {
            ++mism;
            if (mism <= 20) std::cerr << "MISMATCH [" << tag << "] line: " << line << "\n";
        }
    }

    std::cout << "BrightnessVerify cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
