// Parity test for the equine Variant / Markings enums (Minecraft 26.1.2).
// Ground truth: tools/EquineVariantsParity.java (drives the REAL classes).
//
//   equine_variants_parity --cases <equine_variants.tsv>
//
// Verifies, bit-exact against the real net.minecraft classes:
//   * Variant.getId() and Variant.getSerializedName() per value
//   * Variant.byId(int) over a wide int sweep (the ByIdMap WRAP / floorMod trap)
//   * Markings.getId() per value
//   * Markings.byId(int) over the same sweep
//
// Row tags (tab-separated):
//   VID    <id>       <resolvedOrdinal>
//   VGET   <ordinal>  <id> <nameB64>
//   MID    <id>       <resolvedOrdinal>
//   MGET   <ordinal>  <id>

#include "world/entity/animal/equine/EquineVariants.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace eq = mc::world::entity::animal::equine;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// Parse a 32-bit id that may be as large as INT_MAX or as small as INT_MIN.
// stoll avoids the INT_MIN parse pitfall, then narrow to int32 (well-defined here
// since the TSV only ever carries valid int32 values).
int32_t parseId(const std::string& s) {
    return static_cast<int32_t>(std::stoll(s));
}

std::string b64decode(const std::string& in) {
    static const std::string T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int val = 0, bits = -8;
    std::string out;
    for (unsigned char c : in) {
        if (c == '=') break;
        auto pos = T.find(static_cast<char>(c));
        if (pos == std::string::npos) continue;
        val = (val << 6) + static_cast<int>(pos);
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: equine_variants_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];
        ++total;

        if (tag == "VID") {
            if (p.size() != 3) { fail("BADROW " + line); continue; }
            int32_t id = parseId(p[1]);
            int want = std::stoi(p[2]);
            int got = static_cast<int>(eq::variantById(id));  // enum value == ordinal == id
            if (got != want) fail(line);
        } else if (tag == "MID") {
            if (p.size() != 3) { fail("BADROW " + line); continue; }
            int32_t id = parseId(p[1]);
            int want = std::stoi(p[2]);
            int got = static_cast<int>(eq::markingsById(id));
            if (got != want) fail(line);
        } else if (tag == "VGET") {
            if (p.size() != 4) { fail("BADROW " + line); continue; }
            int ordinal = std::stoi(p[1]);
            int wantId = std::stoi(p[2]);
            std::string wantName = b64decode(p[3]);
            auto v = eq::VARIANT_BY_ORDINAL[ordinal];
            if (eq::variantGetId(v) != wantId) fail(line);
            if (std::string(eq::variantSerializedName(v)) != wantName) fail(line);
        } else if (tag == "MGET") {
            if (p.size() != 3) { fail("BADROW " + line); continue; }
            int ordinal = std::stoi(p[1]);
            int wantId = std::stoi(p[2]);
            auto m = eq::MARKINGS_BY_ORDINAL[ordinal];
            if (eq::markingsGetId(m) != wantId) fail(line);
        } else {
            fail("UNKNOWN_TAG " + tag);
        }
    }

    std::cout << "EquineVariants checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
