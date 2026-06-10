// AABB parity vs the real net.minecraft.world.phys.AABB (tools/AabbParity.java).
// Inputs and expectations are raw IEEE-754 bit hex; comparison is BIT-exact.
//
//   aabb_parity [--cases mcpp/build/aabb_cases.tsv]
#include "AABB.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using mc::AABB;

namespace {
double fromBits(const std::string& hexTok) {
    uint64_t bits = std::stoull(hexTok, nullptr, 16);
    double d;
    std::memcpy(&d, &bits, 8);
    return d;
}
std::string toBits(double d) {
    uint64_t bits;
    std::memcpy(&bits, &d, 8);
    char buf[20];
    snprintf(buf, sizeof buf, "%016llx", (unsigned long long)bits);
    return buf;
}
std::vector<std::string> tokens(const std::string& s) {
    std::istringstream ss(s);
    std::vector<std::string> v;
    std::string t;
    while (ss >> t) v.push_back(t);
    return v;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/aabb_cases.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];
    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    struct Case { std::vector<double> in; std::map<std::string, std::vector<std::string>> expect; };
    std::map<std::string, Case> cases;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);
        std::string kind, name, rest;
        if (!std::getline(ss, kind, '\t') || !std::getline(ss, name, '\t')) continue;
        std::getline(ss, rest);
        if (kind == "IN") {
            for (auto& t : tokens(rest)) cases[name].in.push_back(fromBits(t));
        } else {
            // CLIP rest = "<0|1>\t<hexes>" — re-split on the tab
            std::istringstream rs(rest);
            std::string part;
            std::vector<std::string> parts;
            while (std::getline(rs, part, '\t')) for (auto& t : tokens(part)) parts.push_back(t);
            cases[name].expect[kind] = parts;
        }
    }

    int n = 0, failures = 0;
    for (auto& [name, c] : cases) {
        if (c.in.size() != 12) { std::cerr << "BAD-CASE " << name << "\n"; ++failures; continue; }
        ++n;
        AABB box(c.in[0], c.in[1], c.in[2], c.in[3], c.in[4], c.in[5]);
        glm::dvec3 from(c.in[6], c.in[7], c.in[8]), to(c.in[9], c.in[10], c.in[11]);

        // CLIP
        auto& ce = c.expect["CLIP"];
        auto hit = box.clip(from, to);
        bool expHit = !ce.empty() && ce[0] == "1";
        if (hit.has_value() != expHit) {
            std::cerr << "CLIP-HIT-MISMATCH " << name << " got=" << hit.has_value() << " want=" << expHit << "\n";
            ++failures;
        } else if (hit) {
            if (toBits(hit->x) != ce[1] || toBits(hit->y) != ce[2] || toBits(hit->z) != ce[3]) {
                std::cerr << "CLIP-POINT-MISMATCH " << name << "\n";
                ++failures;
            }
        }

        // EXP / CON / DST
        glm::dvec3 d = to - from;
        AABB exp = box.expandTowards(d.x, d.y, d.z);
        AABB con = box.contract(d.x, d.y, d.z);
        double expV[6] = { exp.minCorner.x, exp.minCorner.y, exp.minCorner.z, exp.maxCorner.x, exp.maxCorner.y, exp.maxCorner.z };
        double conV[6] = { con.minCorner.x, con.minCorner.y, con.minCorner.z, con.maxCorner.x, con.maxCorner.y, con.maxCorner.z };
        for (int i = 0; i < 6; ++i) {
            if (toBits(expV[i]) != c.expect["EXP"][i]) { std::cerr << "EXP-MISMATCH " << name << " [" << i << "]\n"; ++failures; break; }
        }
        for (int i = 0; i < 6; ++i) {
            if (toBits(conV[i]) != c.expect["CON"][i]) { std::cerr << "CON-MISMATCH " << name << " [" << i << "]\n"; ++failures; break; }
        }
        if (toBits(box.distanceToSqr(from)) != c.expect["DST"][0]) {
            std::cerr << "DST-MISMATCH " << name << "\n";
            ++failures;
        }
    }

    std::cout << "AabbParity cases=" << n << " failures=" << failures << "\n";
    return failures == 0 ? 0 : 1;
}
