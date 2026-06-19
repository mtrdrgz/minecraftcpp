// Parity test for net.minecraft.world.food.FoodData (26.1.2) — the STATEFUL hunger model.
// Ground truth: tools/FoodDataParity.java vs the real class. The TSV is a stream of ops
// (NEW / ADD / EXH / TICK) tagged by a sequence id; we keep one mc::world::food::FoodData
// per seqId and replay the IDENTICAL ops in order, comparing the full post-state
//   (foodLevel, saturationLevel, exhaustionLevel, tickTimer)
// bit-for-bit (raw IEEE-754 bits for the two floats; decimal for the two ints).
//
// Covers: add() eat formula + clamp(int) + clamp(float,..,foodLevel widened to float),
//         addExhaustion() Math.min cap-at-40, and the world-independent tick block
//         (exhaustion>4 -> saturation/foodLevel decrement, NORMAL vs PEACEFUL).
//
//   food_data_parity --cases mcpp/build/food_data.tsv
//
// Row schema (see FoodDataParity.java):
//   <OP>\t<seqId>\t<argA>\t<argB>\t<foodLevel>\t<satBits>\t<exhBits>\t<tickTimer>

#include "world/food/FoodData.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: food_data_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    std::map<int, mc::world::food::FoodData> instances;  // one persistent instance per seqId

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l, const std::string& what) {
        ++mism; if (shown++ < 40) std::cerr << "MISMATCH [" << what << "] " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.size() < 8) { fail(line, "ncols"); continue; }
        const std::string& op = p[0];
        int seqId = std::stoi(p[1]);
        ++total;

        auto& d = instances[seqId];  // default-constructed (defaults match) on first touch

        if (op == "NEW") {
            d = mc::world::food::FoodData{};  // reset to fresh defaults
        } else if (op == "ADD") {
            int   f = std::stoi(p[2]);
            float s = bf(p[3]);
            d.add(f, s);
        } else if (op == "EXH") {
            float e = bf(p[2]);
            d.addExhaustion(e);
        } else if (op == "TICK") {
            bool peaceful = (p[2] == "1");
            d.tickWorldIndependent(peaceful);
        } else {
            fail(line, "UNKNOWN_OP"); continue;
        }

        // Compare post-state: foodLevel(int) satBits(hex) exhBits(hex) tickTimer(int)
        int      expFood  = std::stoi(p[4]);
        uint32_t expSat   = static_cast<uint32_t>(std::stoul(p[5], nullptr, 16));
        uint32_t expExh   = static_cast<uint32_t>(std::stoul(p[6], nullptr, 16));
        int      expTimer = std::stoi(p[7]);

        if (d.foodLevel != expFood)        fail(line, "foodLevel got=" + std::to_string(d.foodLevel));
        if (fb(d.saturationLevel) != expSat) fail(line, "saturationLevel gotbits=" + std::to_string(fb(d.saturationLevel)));
        if (fb(d.exhaustionLevel) != expExh) fail(line, "exhaustionLevel gotbits=" + std::to_string(fb(d.exhaustionLevel)));
        if (d.tickTimer != expTimer)       fail(line, "tickTimer got=" + std::to_string(d.tickTimer));
    }

    std::cout << "FoodData cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
