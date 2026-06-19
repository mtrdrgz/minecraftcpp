// Parity test for net.minecraft.world.item.ItemCooldowns.
//
// Ground truth: tools/ItemCooldownsParity.java, which drives a REAL ItemCooldowns
// through deterministic op programs and records the cooldown percentage +
// on-cooldown flag at sampled points. We replay the SAME op stream against the C++
// port (mc::world::item::ItemCooldowns) and compare every QUERY bit-for-bit (the
// percent is exchanged as a raw IEEE-754 float; the boolean is decimal 0/1).
//
// Row formats (tab-separated, in stream order):
//   OP    <prog> <ADD|REMOVE|TICK> <group> <time>
//   QUERY <prog> <group> <partialTickBits> <percentBits> <onCooldown>
//
//   item_cooldowns_parity --cases mcpp/build/item_cooldowns.tsv

#include "ItemCooldowns.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
std::uint32_t u32(const std::string& s) { return static_cast<std::uint32_t>(std::stoul(s, nullptr, 16)); }
float    bf(const std::string& s) { return std::bit_cast<float>(u32(s)); }
std::uint32_t fb(float v) { return std::bit_cast<std::uint32_t>(v); }
int      i(const std::string& s) { return std::stoi(s); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: item_cooldowns_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    // One live cooldown machine per program id, evolved in stream order.
    std::unordered_map<int, mc::world::item::ItemCooldowns> machines;

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];

        if (tag == "OP") {
            int prog = i(p[1]);
            const std::string& kind = p[2];
            int group = i(p[3]);
            int time = i(p[4]);
            auto& ic = machines[prog];
            if (kind == "ADD")          ic.addCooldown(group, time);
            else if (kind == "REMOVE")  ic.removeCooldown(group);
            else if (kind == "TICK")    ic.tick();
            else { std::cerr << "unknown OP kind: " << kind << "\n"; return 2; }
        } else if (tag == "QUERY") {
            ++total;
            int prog = i(p[1]);
            int group = i(p[2]);
            float partialTick = bf(p[3]);
            const auto& ic = machines[prog];
            float gotPct = ic.getCooldownPercent(group, partialTick);
            bool gotOn = ic.isOnCooldown(group);
            std::uint32_t expPct = u32(p[4]);
            int expOn = i(p[5]);
            bool ok = (fb(gotPct) == expPct) && (gotOn == (expOn != 0));
            if (!ok) {
                ++mism;
                if (shown++ < 40) {
                    std::cerr << "MISMATCH prog=" << prog << " group=" << group
                              << " gotPct=" << std::hex << fb(gotPct)
                              << " expPct=" << expPct << std::dec
                              << " gotOn=" << gotOn << " expOn=" << expOn
                              << " | " << line << "\n";
                }
            }
        } else {
            std::cerr << "unknown tag: " << tag << "\n";
            return 2;
        }
    }

    std::cout << "ItemCooldowns cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
