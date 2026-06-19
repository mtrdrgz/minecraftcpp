// Parity test for mc::enchantment::getEnchantmentCost
// (world/item/enchantment/EnchantmentCost.h).
//
// Ground truth: mcpp/tools/EnchantmentCostParity.java, which runs the REAL
// decompiled net.minecraft.world.item.enchantment.EnchantmentHelper
//   .getEnchantmentCost(RandomSource, int slot, int bookcases, ItemStack)
// from client.jar. Each row is the exact level requirement that vanilla shows
// on an enchanting-table slot. Results are ints, so plain == is bit-exact.
//
//   default        -> small self-checks (no Mojang files)
//   --cases <tsv>  -> verify every line of the generated reference
//
// TSV row layout (tab-separated):
//   CST  <seed>  <slot>  <bookcases>  <hasEnchantable:0|1>  <result>
//
// The C++ port reconstructs a fresh LegacyRandomSource(seed) per row so the RNG
// stream order (nextInt(8) then nextInt(bookcases+1)) is reproduced exactly,
// then compares getEnchantmentCost(rng, slot, bookcases, hasEnchantable).

#include "world/item/enchantment/EnchantmentCost.h"

#include "world/level/levelgen/RandomSource.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using mc::enchantment::getEnchantmentCost;
using mc::levelgen::LegacyRandomSource;

namespace {

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;  // blank

    if (tag == "CST") {
        int64_t seed = 0;
        int slot = 0, bookcases = 0, hasEnch = 0, expected = 0;
        in >> seed >> slot >> bookcases >> hasEnch >> expected;
        if (in.fail()) {
            err = "CST malformed row";
            return false;
        }
        LegacyRandomSource rng(seed);
        int got = getEnchantmentCost(rng, slot, bookcases, hasEnch != 0);
        if (got != expected) {
            err = "CST seed=" + std::to_string(seed) +
                  " slot=" + std::to_string(slot) +
                  " bookcases=" + std::to_string(bookcases) +
                  " hasEnch=" + std::to_string(hasEnch) +
                  " expected=" + std::to_string(expected) +
                  " got=" + std::to_string(got);
            return false;
        }
        return true;
    }

    err = "unknown tag: " + tag;
    return false;
}

int runCases(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "EnchantmentCost: cannot open " << path << "\n";
        return 2;
    }
    std::string line;
    int n = 0, mism = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        ++n;
        std::string err;
        if (!verifyLine(line, err)) {
            if (mism < 20) std::cerr << "MISMATCH: " << err << " | " << line << "\n";
            ++mism;
        }
    }
    std::cout << "EnchantmentCost checks=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

// Self-checks with no Mojang files: exercise the early-return guard and the
// three slot branches with hand-computed RNG draws. LegacyRandomSource(0) is a
// known stream; we don't hardcode its draws here (the --cases gate covers exact
// values), but we DO verify structural invariants that must hold for any seed.
int runSelfChecks() {
    int mism = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            std::cerr << "SELFCHECK FAIL: " << what << "\n";
            ++mism;
        }
    };

    // Non-enchantable -> hard 0 and RNG untouched. After the call the rng must
    // still yield its very first draw (proving no nextInt was consumed).
    {
        LegacyRandomSource a(12345), b(12345);
        check(getEnchantmentCost(a, 0, 10, /*hasEnchantable=*/false) == 0,
              "non-enchantable slot0 -> 0");
        check(getEnchantmentCost(a, 2, 16, /*hasEnchantable=*/false) == 0,
              "non-enchantable slot2 -> 0");
        // a was used twice with the guard; both returned before any nextInt, so
        // a's stream is identical to a pristine b's stream.
        check(a.nextInt(8) == b.nextInt(8), "guard does not consume RNG");
    }

    // Enchantable invariants that hold for every draw:
    //   slot 0 result is always >= 1 (Math.max(.., 1)).
    //   slot 2 result is always >= bookcases*2 (Math.max(selected, bc*2)) and
    //   slot>=2 / slot==-1 share the slot-2 branch, so they must match slot 2.
    for (int64_t seed : {0LL, 1LL, 42LL, -7LL, 999999LL}) {
        for (int bc : {0, 1, 8, 15, 16, 20}) {
            LegacyRandomSource r0(seed);
            int s0 = getEnchantmentCost(r0, 0, bc, true);
            check(s0 >= 1, "slot0 >= 1");

            // bookcases clamps to 15, so slot2 >= min(bc,15)*2.
            int clamped = bc > 15 ? 15 : bc;
            LegacyRandomSource r2(seed);
            int s2 = getEnchantmentCost(r2, 2, bc, true);
            check(s2 >= clamped * 2, "slot2 >= clampedBookcases*2");

            // slot 3 and slot -1 take the same else/else branch as slot 2.
            LegacyRandomSource r3(seed);
            int s3 = getEnchantmentCost(r3, 3, bc, true);
            check(s3 == s2, "slot3 == slot2");
            LegacyRandomSource rm(seed);
            int sm = getEnchantmentCost(rm, -1, bc, true);
            check(sm == s2, "slot-1 == slot2");

            // slot 1 result is selected*2/3 + 1, always >= 1.
            LegacyRandomSource r1(seed);
            int s1 = getEnchantmentCost(r1, 1, bc, true);
            check(s1 >= 1, "slot1 >= 1");
        }
    }

    std::cout << "EnchantmentCost self-checks mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (!casesPath.empty()) return runCases(casesPath);
    return runSelfChecks();
}
