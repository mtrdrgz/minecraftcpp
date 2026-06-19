// Parity GATE for the existing mc::levelgen::RandomSupport port (declared in
// RandomSource.h, implemented in RandomSource.cpp). RandomSupport is the seed
// math behind every 1.21 RNG: mixStafford13 (Stafford variant 13 of MurmurHash3
// finaliser), the legacy->128bit seed upgrade, the Seed128bit record's xor/mixed
// operations, and seedFromHashOf (real MD5_128). A single bit wrong here
// silently corrupts ALL worldgen noise and feature placement, so this gate
// pins every helper bit-for-bit against ground truth.
//
// Ground truth: mcpp/tools/RandomSupportParity.java, which calls the REAL
// decompiled net.minecraft.world.level.levelgen.RandomSupport from client.jar.
//
//   default        -> a few hardcoded self-checks (no Mojang files required)
//   --cases <tsv>  -> verify every row of the generated reference
//
// Outputs are 64-bit longs; comparison is exact integer equality (no float
// conversion needed, but std::bit_cast is used to print mismatches faithfully).

#include "RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen;

namespace {

// Split a line into tab-separated fields. Preserves empty fields (the HASH rows
// can carry an empty input string), which a >>-based parse would drop.
std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string field;
    std::istringstream in(line);
    while (std::getline(in, field, '\t')) {
        out.push_back(field);
    }
    return out;
}

int64_t toI64(const std::string& s) {
    return static_cast<int64_t>(std::stoll(s));
}

// Verify a single reference row. On mismatch fills `err` and returns false.
bool verifyLine(const std::string& rawLine, std::string& err) {
    // Strip a trailing CR (Windows-generated TSV) so the last field is clean.
    std::string line = rawLine;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) return true;

    std::vector<std::string> f = splitTabs(line);
    if (f.empty()) return true;
    const std::string& tag = f[0];

    auto fail = [&](const std::string& what, int64_t got, int64_t want) {
        std::ostringstream os;
        os << tag << " " << what << " got=" << got << " want=" << want
           << " (got bits=0x" << std::hex << static_cast<uint64_t>(got)
           << " want bits=0x" << static_cast<uint64_t>(want) << ")"
           << " line=[" << line << "]";
        err = os.str();
        return false;
    };

    if (tag == "MIX") {
        // MIX <z> <result>
        int64_t z = toI64(f[1]);
        int64_t want = toI64(f[2]);
        int64_t got = RandomSupport::mixStafford13(z);
        if (got != want) return fail("mixStafford13", got, want);
        return true;
    }

    if (tag == "UNMIX") {
        // UNMIX <seed> <lo> <hi>
        int64_t seed = toI64(f[1]);
        int64_t wantLo = toI64(f[2]);
        int64_t wantHi = toI64(f[3]);
        Seed128bit r = RandomSupport::upgradeSeedTo128bitUnmixed(seed);
        if (r.seedLo != wantLo) return fail("upgradeUnmixed.lo", r.seedLo, wantLo);
        if (r.seedHi != wantHi) return fail("upgradeUnmixed.hi", r.seedHi, wantHi);
        return true;
    }

    if (tag == "UP") {
        // UP <seed> <lo> <hi>
        int64_t seed = toI64(f[1]);
        int64_t wantLo = toI64(f[2]);
        int64_t wantHi = toI64(f[3]);
        Seed128bit r = RandomSupport::upgradeSeedTo128bit(seed);
        if (r.seedLo != wantLo) return fail("upgrade.lo", r.seedLo, wantLo);
        if (r.seedHi != wantHi) return fail("upgrade.hi", r.seedHi, wantHi);
        return true;
    }

    if (tag == "MIXED") {
        // MIXED <baseLo> <baseHi> <lo> <hi>
        Seed128bit base{ toI64(f[1]), toI64(f[2]) };
        int64_t wantLo = toI64(f[3]);
        int64_t wantHi = toI64(f[4]);
        Seed128bit r = base.mixed();
        if (r.seedLo != wantLo) return fail("mixed.lo", r.seedLo, wantLo);
        if (r.seedHi != wantHi) return fail("mixed.hi", r.seedHi, wantHi);
        return true;
    }

    if (tag == "XOR") {
        // XOR <baseLo> <baseHi> <xlo> <xhi> <lo> <hi>
        Seed128bit base{ toI64(f[1]), toI64(f[2]) };
        int64_t xlo = toI64(f[3]);
        int64_t xhi = toI64(f[4]);
        int64_t wantLo = toI64(f[5]);
        int64_t wantHi = toI64(f[6]);
        Seed128bit r = base.xorWith(xlo, xhi);
        if (r.seedLo != wantLo) return fail("xor.lo", r.seedLo, wantLo);
        if (r.seedHi != wantHi) return fail("xor.hi", r.seedHi, wantHi);
        return true;
    }

    if (tag == "XORS") {
        // XORS <baseLo> <baseHi> <xlo> <xhi> <lo> <hi>
        Seed128bit base{ toI64(f[1]), toI64(f[2]) };
        Seed128bit other{ toI64(f[3]), toI64(f[4]) };
        int64_t wantLo = toI64(f[5]);
        int64_t wantHi = toI64(f[6]);
        Seed128bit r = base.xorWith(other);
        if (r.seedLo != wantLo) return fail("xorS.lo", r.seedLo, wantLo);
        if (r.seedHi != wantHi) return fail("xorS.hi", r.seedHi, wantHi);
        return true;
    }

    if (tag == "HASH") {
        // HASH <string> <lo> <hi>   (string may be empty)
        // Last two fields are the expected lo/hi; everything between the tag and
        // them is the input string (it has no tabs by construction, so f[1] is it,
        // and the empty-string case yields f[1]=="").
        if (f.size() < 4) {
            // Empty string: getline keeps "" as a field only if a tab follows it,
            // which it does, so f.size()==4 normally. Guard regardless.
            err = "HASH row has too few fields: [" + line + "]";
            return false;
        }
        std::string input = f[1];
        int64_t wantLo = toI64(f[2]);
        int64_t wantHi = toI64(f[3]);
        Seed128bit r = RandomSupport::seedFromHashOf(input);
        if (r.seedLo != wantLo) return fail("hash.lo[" + input + "]", r.seedLo, wantLo);
        if (r.seedHi != wantHi) return fail("hash.hi[" + input + "]", r.seedHi, wantHi);
        return true;
    }

    // Unknown tag -> treat as a hard failure so a malformed TSV cannot pass silently.
    err = "unknown tag: " + tag + " line=[" + line + "]";
    return false;
}

// Built-in self-checks that need no Mojang files: confirm the documented
// constants and a couple of hand-computed reference values.
int runSelfChecks() {
    int mism = 0;

    // Constants must match the Java fields verbatim.
    if (RandomSupport::GOLDEN_RATIO_64 != static_cast<int64_t>(-7046029254386353131LL)) {
        std::cout << "self: GOLDEN_RATIO_64 wrong\n"; ++mism;
    }
    if (RandomSupport::SILVER_RATIO_64 != static_cast<int64_t>(7640891576956012809LL)) {
        std::cout << "self: SILVER_RATIO_64 wrong\n"; ++mism;
    }

    // mixStafford13(0) == 0 (every term multiplies/xors zero).
    if (RandomSupport::mixStafford13(0) != 0) {
        std::cout << "self: mixStafford13(0)!=0\n"; ++mism;
    }

    // upgradeSeedTo128bitUnmixed(0): lo = 0 ^ SILVER, hi = lo + GOLDEN.
    {
        Seed128bit r = RandomSupport::upgradeSeedTo128bitUnmixed(0);
        int64_t expectLo = RandomSupport::SILVER_RATIO_64;
        int64_t expectHi = static_cast<int64_t>(
            static_cast<uint64_t>(RandomSupport::SILVER_RATIO_64) +
            static_cast<uint64_t>(RandomSupport::GOLDEN_RATIO_64));
        if (r.seedLo != expectLo || r.seedHi != expectHi) {
            std::cout << "self: upgradeUnmixed(0) wrong\n"; ++mism;
        }
    }

    // xor is bitwise on both lanes.
    {
        Seed128bit base{ 0x0123456789ABCDEFLL, static_cast<int64_t>(0xFEDCBA9876543210ULL) };
        Seed128bit r = base.xorWith(static_cast<int64_t>(0xFFFFFFFFFFFFFFFFULL), 0);
        if (r.seedLo != static_cast<int64_t>(~static_cast<uint64_t>(0x0123456789ABCDEFLL)) ||
            r.seedHi != static_cast<int64_t>(0xFEDCBA9876543210ULL)) {
            std::cout << "self: xor wrong\n"; ++mism;
        }
    }

    std::cout << "RandomSupportParity(self) cases=5 mismatches=" << mism << "\n";
    return mism;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }

    if (casesPath.empty()) {
        return runSelfChecks() == 0 ? 0 : 1;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "RandomSupportParity: cannot open " << casesPath << "\n";
        return 2;
    }

    int64_t cases = 0;
    int64_t mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line == "\r") continue;
        ++cases;
        std::string err;
        if (!verifyLine(line, err)) {
            ++mismatches;
            if (mismatches <= 20) {
                std::cout << "MISMATCH: " << err << "\n";
            }
        }
    }

    std::cout << "RandomSupportParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
