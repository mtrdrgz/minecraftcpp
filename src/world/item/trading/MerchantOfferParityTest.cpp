// Parity test for net.minecraft.world.item.trading.MerchantOffer (26.1.2).
//
// Ground truth: tools/MerchantOfferParity.java, which drives a REAL MerchantOffer
// (private all-args ctor + real ItemCost/ItemStack) and emits, per row, the raw
// inputs and the value the real method produced. The C++ port
// (mc::world::item::trading) must reproduce every value bit-for-bit.
//
// Row formats (tab-separated):
//   COST   <basePrice> <demand> <priceMultiplierBits> <specialPriceDiff> <maxStackSize> <result>
//   DEMAND <demand> <uses> <maxUses> <newDemand>
//   STOCK  <uses> <maxUses> <isOutOfStock> <needsRestock>
//
//   merchant_offer_parity --cases mcpp/build/merchant_offer.tsv

#include "MerchantOffer.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
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
float bf(const std::string& s) { return std::bit_cast<float>(u32(s)); }
std::int32_t i(const std::string& s) { return static_cast<std::int32_t>(std::stoll(s)); }
}  // namespace

int main(int argc, char** argv) {
    namespace mo = mc::world::item::trading;

    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: merchant_offer_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long checks = 0, mism = 0;
    int shown = 0;
    std::string line;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];

        if (tag == "COST") {
            ++checks;
            std::int32_t basePrice = i(p[1]);
            std::int32_t demand = i(p[2]);
            float priceMultiplier = bf(p[3]);
            std::int32_t special = i(p[4]);
            std::int32_t maxStackSize = i(p[5]);
            std::int32_t exp = i(p[6]);
            std::int32_t got = mo::getModifiedCostCount(basePrice, demand, priceMultiplier, special, maxStackSize);
            if (got != exp) {
                ++mism;
                if (shown++ < 40)
                    std::cerr << "MISMATCH COST got=" << got << " exp=" << exp << " | " << line << "\n";
            }
        } else if (tag == "DEMAND") {
            ++checks;
            std::int32_t demand = i(p[1]);
            std::int32_t uses = i(p[2]);
            std::int32_t maxUses = i(p[3]);
            std::int32_t exp = i(p[4]);
            std::int32_t got = mo::updateDemand(demand, uses, maxUses);
            if (got != exp) {
                ++mism;
                if (shown++ < 40)
                    std::cerr << "MISMATCH DEMAND got=" << got << " exp=" << exp << " | " << line << "\n";
            }
        } else if (tag == "STOCK") {
            ++checks;
            std::int32_t uses = i(p[1]);
            std::int32_t maxUses = i(p[2]);
            int expOos = i(p[3]);
            int expRestock = i(p[4]);
            bool gotOos = mo::isOutOfStock(uses, maxUses);
            bool gotRestock = mo::needsRestock(uses);
            if (gotOos != (expOos != 0) || gotRestock != (expRestock != 0)) {
                ++mism;
                if (shown++ < 40)
                    std::cerr << "MISMATCH STOCK gotOos=" << gotOos << " expOos=" << expOos
                              << " gotRestock=" << gotRestock << " expRestock=" << expRestock
                              << " | " << line << "\n";
            }
        } else {
            std::cerr << "unknown tag: " << tag << "\n";
            return 2;
        }
    }

    std::cout << "MerchantOffer checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
