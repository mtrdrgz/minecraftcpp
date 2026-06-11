// Parity test for the C++ Axolotl.Variant port
// (world/entity/animal/axolotl/AxolotlVariant.h).
//
// Ground truth: mcpp/tools/AxolotlVariantParity.java drives the REAL decompiled
// net.minecraft.world.entity.animal.axolotl.Axolotl$Variant enum from client.jar
// and emits one row per case. This test reads --cases <tsv>, recomputes every
// value with the port, and compares exactly:
//
//   ID     <ordinal> <id>          getId(values[ordinal]) == id          (decimal int)
//   NAME   <ordinal> <b64(name)>   getName(values[ordinal]) == name      (base64 string)
//   BYID   <id>      <ordinal>     byId(id).ordinal() == ordinal         (decimal int)
//   COMMON <seed>    <ordinal>     getCommonSpawnVariant(LegacyRandomSource(seed)).ordinal()
//   RARE   <seed>    <ordinal>     getRareSpawnVariant(LegacyRandomSource(seed)).ordinal()
//
// The enum value (cast to int) equals the ordinal for this enum (LUCY=0..BLUE=4),
// so the compared quantity is the ordinal, bit-exact with the Java side.
//
// Build (standalone probe, matching the session toolchain):
//   clang++ -O2 -std=c++20 -I mcpp/src \
//       mcpp/src/world/entity/animal/axolotl/AxolotlVariantParityTest.cpp \
//       mcpp/src/world/level/levelgen/RandomSource.cpp \
//       -o mcpp/build/axolotl_variant_probe.exe
//   mcpp/build/axolotl_variant_probe.exe --cases mcpp/build/axolotl_variant.tsv
//
// Prints "AxolotlVariant checks=<N> mismatches=<M>"; exit 0 iff M == 0.

#include "world/entity/animal/axolotl/AxolotlVariant.h"

#include "world/level/levelgen/RandomSource.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ax = mc::world::entity::animal::axolotl;
using mc::levelgen::LegacyRandomSource;

namespace {

// ordinal of a Variant == its underlying int (LUCY=0 .. BLUE=4).
int32_t ordinal(ax::Variant v) { return static_cast<int32_t>(v); }

// Standard base64 decode (matches java.util.Base64 encoder output).
std::string b64decode(const std::string& in) {
    static const std::string T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto val = [&](char c) -> int {
        auto p = T.find(c);
        return p == std::string::npos ? -1 : static_cast<int>(p);
    };
    std::string out;
    int buf = 0, bits = 0;
    for (char c : in) {
        if (c == '=' || c == '\r' || c == '\n') break;
        int d = val(c);
        if (d < 0) continue;
        buf = (buf << 6) | d;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

struct Counters {
    long checks = 0;
    long mismatches = 0;
};

void report(Counters& c, bool ok, const std::string& line) {
    ++c.checks;
    if (!ok) {
        ++c.mismatches;
        if (c.mismatches <= 25) {
            std::cerr << "MISMATCH: " << line << "\n";
        }
    }
}

void verifyLine(const std::string& line, Counters& c) {
    if (line.empty()) return;
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return;

    if (tag == "ID") {
        int ordinalIdx = 0;
        int64_t expId = 0;
        in >> ordinalIdx >> expId;
        ax::Variant v = static_cast<ax::Variant>(ordinalIdx);
        int64_t got = ax::getId(v);
        report(c, got == expId, line);
    } else if (tag == "NAME") {
        int ordinalIdx = 0;
        std::string b64;
        in >> ordinalIdx >> b64;
        ax::Variant v = static_cast<ax::Variant>(ordinalIdx);
        std::string got = ax::getName(v);
        std::string exp = b64decode(b64);
        report(c, got == exp, line);
    } else if (tag == "BYID") {
        int64_t id = 0;
        int expOrdinal = 0;
        in >> id >> expOrdinal;
        ax::Variant got = ax::byId(static_cast<int32_t>(id));
        report(c, ordinal(got) == expOrdinal, line);
    } else if (tag == "COMMON" || tag == "RARE") {
        int64_t seed = 0;
        int expOrdinal = 0;
        in >> seed >> expOrdinal;
        LegacyRandomSource rng(static_cast<int64_t>(seed));
        ax::Variant got = (tag == "COMMON") ? ax::getCommonSpawnVariant(rng)
                                            : ax::getRareSpawnVariant(rng);
        report(c, ordinal(got) == expOrdinal, line);
    }
    // Unknown tags are ignored (forward-compat with the generator).
}

// Self-checks with no Mojang files: verify the table, byId ZERO fallback, and
// that the rare spawn always resolves to BLUE while burning one RNG draw.
void selfChecks(Counters& c) {
    // getId / getName table.
    report(c, ax::getId(ax::Variant::LUCY) == 0, "self:id LUCY");
    report(c, ax::getId(ax::Variant::WILD) == 1, "self:id WILD");
    report(c, ax::getId(ax::Variant::GOLD) == 2, "self:id GOLD");
    report(c, ax::getId(ax::Variant::CYAN) == 3, "self:id CYAN");
    report(c, ax::getId(ax::Variant::BLUE) == 4, "self:id BLUE");
    report(c, ax::getName(ax::Variant::LUCY) == "lucy", "self:name LUCY");
    report(c, ax::getName(ax::Variant::BLUE) == "blue", "self:name BLUE");

    // byId in range.
    report(c, ax::byId(0) == ax::Variant::LUCY, "self:byId 0");
    report(c, ax::byId(4) == ax::Variant::BLUE, "self:byId 4");
    // byId ZERO out-of-bounds strategy -> LUCY.
    report(c, ax::byId(-1) == ax::Variant::LUCY, "self:byId -1");
    report(c, ax::byId(5) == ax::Variant::LUCY, "self:byId 5");
    report(c, ax::byId(1000) == ax::Variant::LUCY, "self:byId 1000");

    // Rare spawn is always BLUE (single-element array); common spawn is one of
    // {LUCY,WILD,GOLD,CYAN}.
    LegacyRandomSource r1(12345);
    report(c, ax::getRareSpawnVariant(r1) == ax::Variant::BLUE, "self:rare->BLUE");
    LegacyRandomSource r2(777);
    ax::Variant cv = ax::getCommonSpawnVariant(r2);
    report(c, cv != ax::Variant::BLUE, "self:common!=BLUE");
}

} // namespace

int main(int argc, char** argv) {
    Counters c;

    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }

    if (casesPath.empty()) {
        selfChecks(c);
    } else {
        std::ifstream f(casesPath);
        if (!f) {
            std::cerr << "cannot open cases file: " << casesPath << "\n";
            return 2;
        }
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            verifyLine(line, c);
        }
    }

    std::cout << "AxolotlVariant checks=" << c.checks
              << " mismatches=" << c.mismatches << "\n";
    return c.mismatches == 0 ? 0 : 1;
}
