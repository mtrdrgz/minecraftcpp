// Bit-exact parity gate for net.minecraft.world.DifficultyInstance (MC 26.1.2),
// ported in world/DifficultyInstance.h. Reads the TSV emitted by
// mcpp/tools/DifficultyInstanceParity.java, reconstructs each instance from the
// recorded constructor inputs, and compares the float results bit-for-bit.
//
// Tag (see DifficultyInstanceParity.java):
//   INST <ordinal> <totalGameTime> <localGameTime>
//        <moonBrightnessBits> <effectiveDifficultyBits> <specialMultiplierBits>
//        <isHard> <isHarderThan_eff> <isHarderThan_lo> <isHarderThan_hi>
#include "world/DifficultyInstance.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

// Float.floatToRawIntBits — reinterpret the float's storage as int32.
static int32_t floatToRawIntBits(float f) {
    int32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));
    return bits;
}

// int32 IEEE-754 bit pattern -> float (Float.intBitsToFloat), for decoding the
// moon-brightness input recorded by the Java side.
static float intBitsToFloat(int32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++cases;

        if (tag == "INST") {
            if (p.size() != 11) { ++mism; continue; }
            int ord = std::stoi(p[1]);
            long long total = std::stoll(p[2]);
            long long local = std::stoll(p[3]);
            int32_t moonBits = static_cast<int32_t>(std::stoll(p[4]));
            int32_t expEffBits = static_cast<int32_t>(std::stoll(p[5]));
            int32_t expSpecBits = static_cast<int32_t>(std::stoll(p[6]));
            int expHard = std::stoi(p[7]);
            int expHarderEff = std::stoi(p[8]);
            int expHarderLo = std::stoi(p[9]);
            int expHarderHi = std::stoi(p[10]);

            if (ord < 0 || ord >= mc::DIFFICULTY_COUNT) { ++mism; continue; }
            mc::Difficulty base = mc::DIFFICULTY_VALUES[ord];
            float moon = intBitsToFloat(moonBits);

            mc::DifficultyInstance di(base, static_cast<int64_t>(total),
                                      static_cast<int64_t>(local), moon);

            float eff = di.getEffectiveDifficulty();
            float spec = di.getSpecialMultiplier();

            bool ok = true;
            ok = ok && (floatToRawIntBits(eff) == expEffBits);
            ok = ok && (floatToRawIntBits(spec) == expSpecBits);
            ok = ok && (static_cast<int>(di.isHard()) == expHard);
            ok = ok && (static_cast<int>(di.isHarderThan(eff)) == expHarderEff);
            ok = ok && (static_cast<int>(di.isHarderThan(eff - 0.0001F)) == expHarderLo);
            ok = ok && (static_cast<int>(di.isHarderThan(eff + 0.0001F)) == expHarderHi);

            if (!ok) ++mism;
        } else {
            // Unknown tag — never silently pass.
            ++mism;
        }
    }

    std::cout << "DifficultyInstance checks=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
