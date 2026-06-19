// Parity test for the C++ Rabbit.Variant port
// (world/entity/animal/rabbit/RabbitVariant.h).
//
// Ground truth: mcpp/tools/RabbitVariantParity.java drives the REAL decompiled
// net.minecraft.world.entity.animal.rabbit.Rabbit$Variant enum from client.jar
// and emits one row per case. This test reads --cases <tsv>, recomputes every
// value with the port, and compares exactly:
//
//   ID    <ordinal> <id>         id(values[ordinal]) == id              (decimal int)
//   NAME  <ordinal> <b64(name)>  getSerializedName(values[ordinal]) == name (base64)
//   BYID  <id>      <ordinal>    byId(id).ordinal() == ordinal          (decimal int)
//
// byId is backed by ByIdMap.sparse: the declared keys {0,1,2,3,4,5,99} hit their
// variant, every other id (negatives, holes 6..98, 100+, INT extremes) folds to
// DEFAULT = BROWN (ordinal 0). The enum's C++ underlying value equals its Java
// ordinal (BROWN=0..EVIL=6), so the compared quantity is the ordinal, bit-exact.
//
// Build (standalone probe, matching the session toolchain):
//   clang++ -O2 -std=c++23 -ffp-contract=off -I mcpp/src \
//       mcpp/src/world/entity/animal/rabbit/RabbitVariantParityTest.cpp \
//       -o mcpp/build/rabbit_variant_probe.exe
//   mcpp/build/rabbit_variant_probe.exe --cases mcpp/build/rabbit_variant.tsv
//
// Prints "RabbitVariant checks=<N> mismatches=<M>"; exit 0 iff M == 0.

#include "world/entity/animal/rabbit/RabbitVariant.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace rb = mc::world::entity::animal::rabbit;

namespace {

// ordinal of a RabbitVariant == its underlying int (BROWN=0 .. EVIL=6).
int32_t ordinal(rb::RabbitVariant v) { return static_cast<int32_t>(v); }

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
        rb::RabbitVariant v = static_cast<rb::RabbitVariant>(ordinalIdx);
        int64_t got = rb::rabbitVariantId(v);
        report(c, got == expId, line);
    } else if (tag == "NAME") {
        int ordinalIdx = 0;
        std::string b64;
        in >> ordinalIdx >> b64;
        rb::RabbitVariant v = static_cast<rb::RabbitVariant>(ordinalIdx);
        std::string got(rb::rabbitVariantSerializedName(v));
        std::string exp = b64decode(b64);
        report(c, got == exp, line);
    } else if (tag == "BYID") {
        int64_t id = 0;
        int expOrdinal = 0;
        in >> id >> expOrdinal;
        rb::RabbitVariant got = rb::rabbitVariantById(static_cast<int32_t>(id));
        report(c, ordinal(got) == expOrdinal, line);
    }
    // Unknown tags are ignored (forward-compat with the generator).
}

// Self-checks with no Mojang files: the id/name table and the sparse byId
// semantics (declared keys vs. holes/oob -> BROWN default).
void selfChecks(Counters& c) {
    // id() table -- note EVIL declares id 99, NOT its ordinal 6.
    report(c, rb::rabbitVariantId(rb::RabbitVariant::BROWN) == 0, "self:id BROWN");
    report(c, rb::rabbitVariantId(rb::RabbitVariant::WHITE) == 1, "self:id WHITE");
    report(c, rb::rabbitVariantId(rb::RabbitVariant::BLACK) == 2, "self:id BLACK");
    report(c, rb::rabbitVariantId(rb::RabbitVariant::WHITE_SPLOTCHED) == 3,
           "self:id WHITE_SPLOTCHED");
    report(c, rb::rabbitVariantId(rb::RabbitVariant::GOLD) == 4, "self:id GOLD");
    report(c, rb::rabbitVariantId(rb::RabbitVariant::SALT) == 5, "self:id SALT");
    report(c, rb::rabbitVariantId(rb::RabbitVariant::EVIL) == 99, "self:id EVIL");

    // getSerializedName() table.
    report(c, rb::rabbitVariantSerializedName(rb::RabbitVariant::BROWN) == "brown",
           "self:name BROWN");
    report(c,
           rb::rabbitVariantSerializedName(rb::RabbitVariant::WHITE_SPLOTCHED) ==
               "white_splotched",
           "self:name WHITE_SPLOTCHED");
    report(c, rb::rabbitVariantSerializedName(rb::RabbitVariant::EVIL) == "evil",
           "self:name EVIL");

    // byId: declared keys hit their variant.
    report(c, rb::rabbitVariantById(0) == rb::RabbitVariant::BROWN, "self:byId 0");
    report(c, rb::rabbitVariantById(5) == rb::RabbitVariant::SALT, "self:byId 5");
    report(c, rb::rabbitVariantById(99) == rb::RabbitVariant::EVIL, "self:byId 99");

    // byId: holes and out-of-bounds fold to DEFAULT = BROWN.
    report(c, rb::rabbitVariantById(-1) == rb::RabbitVariant::BROWN, "self:byId -1");
    report(c, rb::rabbitVariantById(6) == rb::RabbitVariant::BROWN, "self:byId 6");
    report(c, rb::rabbitVariantById(50) == rb::RabbitVariant::BROWN, "self:byId 50");
    report(c, rb::rabbitVariantById(98) == rb::RabbitVariant::BROWN, "self:byId 98");
    report(c, rb::rabbitVariantById(100) == rb::RabbitVariant::BROWN, "self:byId 100");
    report(c, rb::rabbitVariantById(1000000) == rb::RabbitVariant::BROWN,
           "self:byId 1000000");
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

    std::cout << "RabbitVariant checks=" << c.checks
              << " mismatches=" << c.mismatches << "\n";
    return c.mismatches == 0 ? 0 : 1;
}
