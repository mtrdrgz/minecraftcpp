// Parity test for net.minecraft.world.phys.Vec2. Ground truth: tools/Vec2Parity.java.
// Bit-exact: every float is compared by its raw IEEE-754 bits (never by value), so
// NaN / -0.0 / Inf / subnormals are all checked faithfully.
//
//   vec2_parity --cases mcpp/build/vec2.tsv

#include "Vec2.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::Vec2;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
uint32_t hx(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: vec2_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << l << "\n"; };
    auto eF = [&](float got, const std::string& exp, const std::string& l) { if (fb(got) != hx(exp)) fail(l); };
    auto v2 = [&](const Vec2& v, const std::vector<std::string>& p, int o, const std::string& l) {
        if (fb(v.x) != hx(p[o]) || fb(v.y) != hx(p[o + 1])) fail(l);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "CONST") {
            const std::string& name = p[1];
            const Vec2* c = nullptr;
            if      (name == "ZERO")       c = &Vec2::ZERO;
            else if (name == "ONE")        c = &Vec2::ONE;
            else if (name == "UNIT_X")     c = &Vec2::UNIT_X;
            else if (name == "NEG_UNIT_X") c = &Vec2::NEG_UNIT_X;
            else if (name == "UNIT_Y")     c = &Vec2::UNIT_Y;
            else if (name == "NEG_UNIT_Y") c = &Vec2::NEG_UNIT_Y;
            else if (name == "MAX")        c = &Vec2::MAX;
            else if (name == "MIN")        c = &Vec2::MIN;
            if (!c) { fail("UNKNOWN_CONST " + name); continue; }
            if (fb(c->x) != hx(p[2]) || fb(c->y) != hx(p[3])) fail(line);
            continue;
        }

        Vec2 a{bf(p[1]), bf(p[2])};

        if      (t == "LENGTH")     eF(a.length(), p[3], line);
        else if (t == "LENGTHSQ")   eF(a.lengthSquared(), p[3], line);
        else if (t == "NORMALIZED") v2(a.normalized(), p, 3, line);
        else if (t == "NEGATED")    v2(a.negated(), p, 3, line);
        else if (t == "SCALE")      v2(a.scale(bf(p[3])), p, 4, line);
        else if (t == "ADDF")       v2(a.add(bf(p[3])), p, 4, line);
        else if (t == "ADD" || t == "DOT" || t == "DISTSQR" || t == "EQUALS") {
            Vec2 b{bf(p[3]), bf(p[4])};
            if      (t == "ADD")     v2(a.add(b), p, 5, line);
            else if (t == "DOT")     eF(a.dot(b), p[5], line);
            else if (t == "DISTSQR") eF(a.distanceToSqr(b), p[5], line);
            else if (t == "EQUALS")  { if ((a.equals(b) ? 1 : 0) != std::stoi(p[5])) fail(line); }
        }
        else fail("UNKNOWN_TAG " + t);
    }

    std::cout << "Vec2 cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
