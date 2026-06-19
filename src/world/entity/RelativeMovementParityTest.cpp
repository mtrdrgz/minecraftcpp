// Parity test for net.minecraft.world.entity.Relative (26.1.2).
//
// (Assignment names the class "RelativeMovement" — its older-version name; in
// 26.1.2 it is net.minecraft.world.entity.Relative. Same pack/unpack bitset.)
//
// Ground truth: tools/RelativeMovementParity.java, which calls the REAL
// net.minecraft.world.entity.Relative and emits:
//   CONST   <ordinal> <name> <bit> <mask>
//   COUNT   <values.length>
//   UNPACK  <value> <pack(unpack(value))>
//   PACKALL <value> <pack(unpack(value))>
//   ROT     <yRot> <xRot> <pack(rotation(...))>
//   POS     <x> <y> <z> <pack(position(...))>
//   DIR     <x> <y> <z> <pack(direction(...))>
//   STATIC  <pack(ALL)> <pack(ROTATION)> <pack(DELTA)>
//
// We replay each row against the C++ mc::Relative header and compare BIT-FOR-BIT.
// Everything here is integer-valued (a pure bitset), so equality is exact.
//
//   relative_movement_parity --cases mcpp/build/relative_movement.tsv

#include "world/entity/Relative.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::Relative;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// Map an ordinal to the C++ enum constant; false if out of [0,9).
bool relFromOrdinal(int ord, Relative& out) {
    if (ord < 0 || ord >= static_cast<int>(mc::RELATIVE_VALUES.size())) return false;
    out = mc::RELATIVE_VALUES[static_cast<size_t>(ord)];
    return true;
}
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: relative_movement_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& msg) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << msg << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto f = split(line);
        if (f.empty()) continue;
        const std::string& tag = f[0];

        if (tag == "CONST") {
            // CONST <ordinal> <name> <bit> <mask>
            if (f.size() != 5) { fail(line + " (bad arity)"); continue; }
            ++total;
            int ord = std::stoi(f[1]);
            const std::string& expectName = f[2];
            int expectBit = std::stoi(f[3]);
            int expectMask = std::stoi(f[4]);

            Relative r;
            if (!relFromOrdinal(ord, r)) { fail(line + " (ordinal out of range)"); continue; }
            if (mc::ordinal(r) != ord) { fail(line + " (ordinal mismatch)"); continue; }
            std::string gotName(mc::name(r));
            if (gotName != expectName) { fail(line + " (name got=" + gotName + ")"); continue; }
            if (mc::bit(r) != expectBit) {
                fail(line + " (bit got=" + std::to_string(mc::bit(r)) + ")");
                continue;
            }
            if (mc::getMask(r) != expectMask) {
                fail(line + " (mask got=" + std::to_string(mc::getMask(r)) + ")");
                continue;
            }
        } else if (tag == "COUNT") {
            if (f.size() != 2) { fail(line + " (bad arity)"); continue; }
            ++total;
            int count = std::stoi(f[1]);
            if (count != static_cast<int>(mc::RELATIVE_VALUES.size())) {
                fail(line + " (count got=" +
                     std::to_string(mc::RELATIVE_VALUES.size()) + ")");
                continue;
            }
        } else if (tag == "UNPACK" || tag == "PACKALL") {
            // <tag> <value> <pack(unpack(value))>
            if (f.size() != 3) { fail(line + " (bad arity)"); continue; }
            ++total;
            int value = static_cast<int>(std::stoll(f[1]));
            int expect = static_cast<int>(std::stoll(f[2]));
            mc::RelativeSet s = mc::unpack(value);
            int got = mc::pack(s);
            if (got != expect) {
                fail(line + " (got=" + std::to_string(got) + ")");
                continue;
            }
        } else if (tag == "ROT") {
            // ROT <yRot> <xRot> <packed>
            if (f.size() != 4) { fail(line + " (bad arity)"); continue; }
            ++total;
            bool yr = std::stoi(f[1]) != 0;
            bool xr = std::stoi(f[2]) != 0;
            int expect = static_cast<int>(std::stoll(f[3]));
            int got = mc::pack(mc::rotation(yr, xr));
            if (got != expect) { fail(line + " (got=" + std::to_string(got) + ")"); continue; }
        } else if (tag == "POS") {
            // POS <x> <y> <z> <packed>
            if (f.size() != 5) { fail(line + " (bad arity)"); continue; }
            ++total;
            bool x = std::stoi(f[1]) != 0;
            bool y = std::stoi(f[2]) != 0;
            bool z = std::stoi(f[3]) != 0;
            int expect = static_cast<int>(std::stoll(f[4]));
            int got = mc::pack(mc::position(x, y, z));
            if (got != expect) { fail(line + " (got=" + std::to_string(got) + ")"); continue; }
        } else if (tag == "DIR") {
            // DIR <x> <y> <z> <packed>
            if (f.size() != 5) { fail(line + " (bad arity)"); continue; }
            ++total;
            bool x = std::stoi(f[1]) != 0;
            bool y = std::stoi(f[2]) != 0;
            bool z = std::stoi(f[3]) != 0;
            int expect = static_cast<int>(std::stoll(f[4]));
            int got = mc::pack(mc::direction(x, y, z));
            if (got != expect) { fail(line + " (got=" + std::to_string(got) + ")"); continue; }
        } else if (tag == "STATIC") {
            // STATIC <pack(ALL)> <pack(ROTATION)> <pack(DELTA)>
            if (f.size() != 4) { fail(line + " (bad arity)"); continue; }
            ++total;
            int expectAll = static_cast<int>(std::stoll(f[1]));
            int expectRot = static_cast<int>(std::stoll(f[2]));
            int expectDelta = static_cast<int>(std::stoll(f[3]));
            int gotAll = mc::pack(mc::RELATIVE_ALL);
            int gotRot = mc::pack(mc::RELATIVE_ROTATION);
            int gotDelta = mc::pack(mc::RELATIVE_DELTA);
            if (gotAll != expectAll) { fail(line + " (ALL got=" + std::to_string(gotAll) + ")"); continue; }
            if (gotRot != expectRot) { fail(line + " (ROTATION got=" + std::to_string(gotRot) + ")"); continue; }
            if (gotDelta != expectDelta) { fail(line + " (DELTA got=" + std::to_string(gotDelta) + ")"); continue; }
        }
        // Unknown tags ignored (forward-compat).
    }

    std::cout << "RelativeMovement cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
