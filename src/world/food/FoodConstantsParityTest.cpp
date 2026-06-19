// Parity test for net.minecraft.world.food.FoodConstants. Ground truth:
// tools/FoodConstantsParity.java (reads the real class reflectively). Bit-exact:
// int constants by decimal, float constants and saturationByModifier results by raw
// IEEE-754 bits (std::bit_cast).
//
//   food_constants_parity --cases mcpp/build/food_constants.tsv
//
// Rows:
//   ICONST <NAME> <int>
//   FCONST <NAME> <floatbits %08x>
//   SATMOD <nutrition int> <modifier floatbits> <result floatbits>

#include "FoodConstants.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using mc::food::FoodConstants;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int      i(const std::string& s) { return std::stoi(s); }
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }

// NAME -> C++ constant. The map is the explicit mirror of FoodConstants.h; a NAME present
// in the GT but absent here is a HARD failure (unknown constant), never silently passed.
const std::map<std::string, int>& intConsts() {
    static const std::map<std::string, int> m = {
        {"MAX_FOOD", FoodConstants::MAX_FOOD},
        {"HEALTH_TICK_COUNT", FoodConstants::HEALTH_TICK_COUNT},
        {"HEALTH_TICK_COUNT_SATURATED", FoodConstants::HEALTH_TICK_COUNT_SATURATED},
        {"HEAL_LEVEL", FoodConstants::HEAL_LEVEL},
        {"SPRINT_LEVEL", FoodConstants::SPRINT_LEVEL},
        {"STARVE_LEVEL", FoodConstants::STARVE_LEVEL},
    };
    return m;
}
const std::map<std::string, float>& floatConsts() {
    static const std::map<std::string, float> m = {
        {"MAX_SATURATION", FoodConstants::MAX_SATURATION},
        {"START_SATURATION", FoodConstants::START_SATURATION},
        {"SATURATION_FLOOR", FoodConstants::SATURATION_FLOOR},
        {"EXHAUSTION_DROP", FoodConstants::EXHAUSTION_DROP},
        {"FOOD_SATURATION_POOR", FoodConstants::FOOD_SATURATION_POOR},
        {"FOOD_SATURATION_LOW", FoodConstants::FOOD_SATURATION_LOW},
        {"FOOD_SATURATION_NORMAL", FoodConstants::FOOD_SATURATION_NORMAL},
        {"FOOD_SATURATION_GOOD", FoodConstants::FOOD_SATURATION_GOOD},
        {"FOOD_SATURATION_MAX", FoodConstants::FOOD_SATURATION_MAX},
        {"FOOD_SATURATION_SUPERNATURAL", FoodConstants::FOOD_SATURATION_SUPERNATURAL},
        {"EXHAUSTION_HEAL", FoodConstants::EXHAUSTION_HEAL},
        {"EXHAUSTION_JUMP", FoodConstants::EXHAUSTION_JUMP},
        {"EXHAUSTION_SPRINT_JUMP", FoodConstants::EXHAUSTION_SPRINT_JUMP},
        {"EXHAUSTION_MINE", FoodConstants::EXHAUSTION_MINE},
        {"EXHAUSTION_ATTACK", FoodConstants::EXHAUSTION_ATTACK},
        {"EXHAUSTION_WALK", FoodConstants::EXHAUSTION_WALK},
        {"EXHAUSTION_CROUCH", FoodConstants::EXHAUSTION_CROUCH},
        {"EXHAUSTION_SPRINT", FoodConstants::EXHAUSTION_SPRINT},
        {"EXHAUSTION_SWIM", FoodConstants::EXHAUSTION_SWIM},
    };
    return m;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: food_constants_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "ICONST") {
            const auto& m = intConsts();
            auto it = m.find(p[1]);
            if (it == m.end()) { fail(line + " [unknown int constant]"); continue; }
            if (it->second != i(p[2])) fail(line + " got=" + std::to_string(it->second));
        } else if (t == "FCONST") {
            const auto& m = floatConsts();
            auto it = m.find(p[1]);
            if (it == m.end()) { fail(line + " [unknown float constant]"); continue; }
            if (fb(it->second) != static_cast<uint32_t>(std::stoul(p[2], nullptr, 16)))
                fail(line + " gotbits=" + std::to_string(fb(it->second)));
        } else if (t == "SATMOD") {
            float got = FoodConstants::saturationByModifier(i(p[1]), bf(p[2]));
            if (fb(got) != static_cast<uint32_t>(std::stoul(p[3], nullptr, 16)))
                fail(line + " gotbits=" + std::to_string(fb(got)));
        } else {
            fail(line + " [unknown TAG]");
        }
    }

    std::cout << "FoodConstants cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
