// Parity test for the C++ CrossbowItem shot helpers (world/item/CrossbowItem.h).
//
// Ground truth: mcpp/tools/CrossbowItemParity.java, which runs the REAL
// decompiled net.minecraft.world.item.CrossbowItem from client.jar. Each row is
// dispatched by TAG; the C++ port recomputes the value and compares it bit-exact
// against the Java-emitted float bits.
//
//   default        -> small self-checks (no Mojang files)
//   --cases <tsv>  -> verify every line of the generated reference
//
// TAG layout (tab-separated):
//   SP     <containsFirework 0|1>   <powerBits>   getShootingPower
//   PITCH  <seed> <index>           <pitchBits>   getShotPitch(LegacyRng(seed), index)
//
// Floats are compared via raw IEEE-754 bits, so equality is byte-exact.

#include "world/item/CrossbowItem.h"

#include "world/level/levelgen/RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using mc::levelgen::LegacyRandomSource;
namespace cb = mc::world::item;

namespace {

std::int32_t floatBits(float f) { return std::bit_cast<std::int32_t>(f); }

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;  // blank

    if (tag == "SP") {
        int containsFirework = 0;
        std::int32_t expected = 0;
        in >> containsFirework >> expected;
        std::int32_t got = floatBits(cb::getShootingPower(containsFirework != 0));
        if (got != expected) {
            err = "SP containsFirework=" + std::to_string(containsFirework) +
                  " expectedBits=" + std::to_string(expected) +
                  " gotBits=" + std::to_string(got);
            return false;
        }
        return true;
    }

    if (tag == "PITCH") {
        int64_t seed = 0;
        std::int32_t index = 0;
        std::int32_t expected = 0;
        in >> seed >> index >> expected;
        LegacyRandomSource rng(seed);
        std::int32_t got = floatBits(cb::getShotPitch(rng, index));
        if (got != expected) {
            err = "PITCH seed=" + std::to_string(seed) +
                  " index=" + std::to_string(index) +
                  " expectedBits=" + std::to_string(expected) +
                  " gotBits=" + std::to_string(got);
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
        std::cerr << "CrossbowItem: cannot open " << path << "\n";
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
    std::cout << "CrossbowItem checks=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

// Self-checks that exercise both shooting-power constants, the index-0 constant
// pitch (no RNG draw), and the high/low pitch branches, without any Mojang files.
int runSelfChecks() {
    int mism = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            std::cerr << "SELFCHECK FAIL: " << what << "\n";
            ++mism;
        }
    };

    check(cb::getShootingPower(true) == 1.6F, "power firework==1.6F");
    check(cb::getShootingPower(false) == 3.15F, "power arrow==3.15F");

    // index 0 is a pure 1.0F constant and must NOT advance the RNG.
    {
        LegacyRandomSource rng(12345);
        float p0 = cb::getShotPitch(rng, 0);
        check(p0 == 1.0F, "index0==1.0F");
        // Next draw should equal a fresh stream's first nextFloat (no draw spent).
        LegacyRandomSource fresh(12345);
        check(rng.nextFloat() == fresh.nextFloat(), "index0 consumed no RNG");
    }

    // index 1 (odd) -> high pitch (0.63F bias); index 2 (even) -> low (0.43F).
    // Recompute the formula directly to confirm the branch wiring.
    {
        LegacyRandomSource a(777), b(777);
        float odd = cb::getShotPitch(a, 1);
        float oddRef = 1.0F / (b.nextFloat() * 0.5F + 1.8F) + 0.63F;
        check(odd == oddRef, "index1 high-pitch formula");
    }
    {
        LegacyRandomSource a(777), b(777);
        float even = cb::getShotPitch(a, 2);
        float evenRef = 1.0F / (b.nextFloat() * 0.5F + 1.8F) + 0.43F;
        check(even == evenRef, "index2 low-pitch formula");
    }

    std::cout << "CrossbowItem self-checks mismatches=" << mism << "\n";
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
