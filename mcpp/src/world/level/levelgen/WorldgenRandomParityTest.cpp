// Parity test for mc::levelgen::WorldgenRandom (the population/decoration RNG
// that seeds every feature placed by ChunkGenerator.applyBiomeDecoration).
//
// Ground truth: mcpp/tools/WorldgenRandomParity.java, which runs the real
// decompiled WorldgenRandom from client.jar. Each case applies a seed method
// (setDecorationSeed / setFeatureSeed / setLargeFeatureSeed /
// setLargeFeatureWithSalt) and then a fixed probe sequence; floats/doubles are
// compared by raw IEEE bits.
//
//   default        -> hardcoded self-checks (no Mojang files)
//   --cases <tsv>  -> verify every line of the generated reference

#include "RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen;

namespace {

struct Probe {
    int64_t l1 = 0;
    int32_t i1 = 0, i2 = 0, i3 = 0, b1 = 0, fbits = 0;
    int64_t dbits = 0;
    int64_t l2 = 0;
};

Probe runProbe(WorldgenRandom& r) {
    Probe p;
    p.l1 = r.nextLong();
    p.i1 = r.nextInt();
    p.i2 = r.nextInt(16);
    p.i3 = r.nextInt(100);
    p.b1 = r.nextBoolean() ? 1 : 0;
    p.fbits = std::bit_cast<int32_t>(r.nextFloat());
    p.dbits = std::bit_cast<int64_t>(r.nextDouble());
    p.l2 = r.nextLong();
    return p;
}

WorldgenRandom xoro() { return WorldgenRandom(std::make_shared<XoroshiroRandomSource>(0)); }
WorldgenRandom legacy() { return WorldgenRandom(std::make_shared<LegacyRandomSource>(0)); }

bool probeEq(const Probe& a, const Probe& b) {
    return a.l1 == b.l1 && a.i1 == b.i1 && a.i2 == b.i2 && a.i3 == b.i3 && a.b1 == b.b1 &&
           a.fbits == b.fbits && a.dbits == b.dbits && a.l2 == b.l2;
}

// Verify one reference line. Returns true on match.
bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string kind;
    in >> kind;
    if (kind.empty()) return true; // blank

    int64_t seed = 0;
    int32_t x = 0, z = 0;
    Probe expected;
    WorldgenRandom r = xoro();

    auto readProbe = [&](Probe& p) {
        in >> p.l1 >> p.i1 >> p.i2 >> p.i3 >> p.b1 >> p.fbits >> p.dbits >> p.l2;
    };

    if (kind == "DEC") {
        int64_t ds = 0;
        in >> seed >> x >> z >> ds;
        readProbe(expected);
        const int64_t got = r.setDecorationSeed(seed, x, z);
        if (got != ds) { err = "decorationSeed " + std::to_string(got) + " != " + std::to_string(ds); return false; }
    } else if (kind == "FEAT") {
        int32_t idx = 0, step = 0;
        in >> seed >> x >> z >> idx >> step;
        readProbe(expected);
        const int64_t ds = r.setDecorationSeed(seed, x, z);
        r.setFeatureSeed(ds, idx, step);
    } else if (kind == "LARGE") {
        in >> seed >> x >> z;
        readProbe(expected);
        r = legacy();
        r.setLargeFeatureSeed(seed, x, z);
    } else if (kind == "SALT") {
        int32_t salt = 0;
        in >> seed >> x >> z >> salt;
        readProbe(expected);
        r = legacy();
        r.setLargeFeatureWithSalt(seed, x, z, salt);
    } else {
        return true; // unknown line, ignore
    }

    const Probe got = runProbe(r);
    if (!probeEq(got, expected)) {
        auto fld = [&](const char* n, int64_t a, int64_t b) -> std::string {
            return a == b ? "" : (std::string(n) + " " + std::to_string(a) + "!=" + std::to_string(b) + " ");
        };
        err = kind + " probe: " + fld("l1", got.l1, expected.l1) + fld("i1", got.i1, expected.i1) +
              fld("i2", got.i2, expected.i2) + fld("i3", got.i3, expected.i3) + fld("b1", got.b1, expected.b1) +
              fld("fbits", got.fbits, expected.fbits) + fld("dbits", got.dbits, expected.dbits) +
              fld("l2", got.l2, expected.l2);
        return false;
    }
    return true;
}

// Hardcoded reference lines from WorldgenRandomParity.java (jar-free self-test).
const std::vector<std::string> kHardcoded = {
    "DEC\t0\t0\t0\t0\t3038984751730664151\t1078879416\t1\t17\t1\t1061533523\t4606660424108617959\t-7194800961640013583",
    "FEAT\t0\t0\t0\t0\t7\t6384546642282394621\t1174684990\t0\t19\t0\t1048325480\t4603254929617983754\t8560926524807195611",
    "FEAT\t0\t0\t0\t0\t10\t-3048182983525289168\t-618630920\t3\t66\t1\t1063378727\t4601934054878679478\t6178507307442309173",
    "DEC\t123456789\t-32\t48\t2890539012205766469\t3882730286061511446\t-1782281532\t14\t30\t0\t1064192763\t4605030339251477622\t-4697388744634618551",
    "LARGE\t123456789\t16\t16\t-6369690808738342160\t-1437207604\t3\t18\t0\t1055446758\t4605576886000635122\t2588625024334543459",
    "SALT\t123456789\t16\t16\t12345\t-1843320493436674358\t189617649\t2\t75\t0\t1058242066\t4599394966554717850\t208770450485051233",
};

} // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[1]) == "--cases") {
        std::ifstream f(argv[2]);
        if (!f) { std::cerr << "cannot open " << argv[2] << '\n'; return 2; }
        std::string line;
        long n = 0, bad = 0;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::string err;
            ++n;
            if (!verifyLine(line, err)) {
                ++bad;
                if (bad <= 20) std::cerr << "MISMATCH: " << err << "  | " << line.substr(0, 60) << "...\n";
            }
        }
        std::cout << "WorldgenRandom cases=" << n << " mismatches=" << bad << '\n';
        return bad == 0 ? 0 : 1;
    }

    bool ok = true;
    for (const auto& line : kHardcoded) {
        std::string err;
        if (!verifyLine(line, err)) {
            ok = false;
            std::cerr << "FAIL: " << err << '\n';
        }
    }
    if (!ok) {
        std::cerr << "WorldgenRandom parity checks FAILED\n";
        return 1;
    }
    std::cout << "WorldgenRandom parity checks passed\n";
    return 0;
}
