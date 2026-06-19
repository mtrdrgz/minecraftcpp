// Bit-exact parity gate for net.minecraft.core.AxisCycle (Minecraft 26.1.2).
// VERIFY-EXISTING: the AxisCycle port already lives (certified) in the engine
// header world/phys/Direction.h — this test #includes it and recomputes every
// row of axis_cycle.tsv (emitted by tools/AxisCycleParity.java), comparing
// BIT-FOR-BIT (ints decimal-exact, doubles via raw IEEE-754 bits).
//
// TSV rows (see AxisCycleParity.java header for the full schema):
//   ORD     <name>                                  <ordinal>
//   AXORD   <name>                                  <ordinal>
//   BETWEEN <fromOrd> <toOrd>                        <betweenOrd>
//   INVERSE <cycleOrd>                               <inverseOrd>
//   CYCLEAX <cycleOrd> <axisOrd>                     <resultAxisOrd>
//   CYCLEI  <cycleOrd> <x> <y> <z> <axisOrd>         <resultInt>
//   CYCLED  <cycleOrd> <xBits> <yBits> <zBits> <axisOrd>  <resultDoubleBits>
//
//   mcpp/build/axis_cycle_parity.exe --cases mcpp/build/axis_cycle.tsv

#include "world/phys/Direction.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::Axis;
using mc::AxisCycle;

static double db(const std::string& s) {
    return std::bit_cast<double>((uint64_t)std::stoull(s, nullptr, 16));
}
static uint64_t bd(double v) { return std::bit_cast<uint64_t>(v); }

static AxisCycle cyc(int ord) { return static_cast<AxisCycle>(ord); }
static Axis ax(int ord) { return static_cast<Axis>(ord); }

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: axis_cycle_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p;
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, '\t')) p.push_back(tok);
        if (p.empty()) continue;

        const std::string& tag = p[0];

        if (tag == "ORD") {
            // p[1]=name  p[2]=ordinal — verify our enum value matches by name.
            if (p.size() < 3) continue;
            const std::string& name = p[1];
            int exp = std::stoi(p[2]);
            int got;
            if (name == "NONE") got = static_cast<int>(AxisCycle::NONE);
            else if (name == "FORWARD") got = static_cast<int>(AxisCycle::FORWARD);
            else if (name == "BACKWARD") got = static_cast<int>(AxisCycle::BACKWARD);
            else { std::cerr << "unknown AxisCycle name " << name << "\n"; ++cases; ++mism; continue; }
            ++cases;
            if (got != exp) {
                ++mism;
                if (mism <= 20) std::cerr << "MISMATCH ORD " << name << " exp=" << exp << " got=" << got << "\n";
            }
        } else if (tag == "AXORD") {
            if (p.size() < 3) continue;
            const std::string& name = p[1];
            int exp = std::stoi(p[2]);
            int got;
            if (name == "X") got = static_cast<int>(Axis::X);
            else if (name == "Y") got = static_cast<int>(Axis::Y);
            else if (name == "Z") got = static_cast<int>(Axis::Z);
            else { std::cerr << "unknown Axis name " << name << "\n"; ++cases; ++mism; continue; }
            ++cases;
            if (got != exp) {
                ++mism;
                if (mism <= 20) std::cerr << "MISMATCH AXORD " << name << " exp=" << exp << " got=" << got << "\n";
            }
        } else if (tag == "BETWEEN") {
            // p[1]=fromOrd p[2]=toOrd p[3]=betweenOrd
            if (p.size() < 4) continue;
            int fromOrd = std::stoi(p[1]);
            int toOrd = std::stoi(p[2]);
            int exp = std::stoi(p[3]);
            int got = static_cast<int>(mc::axisCycleBetween(ax(fromOrd), ax(toOrd)));
            ++cases;
            if (got != exp) {
                ++mism;
                if (mism <= 20) std::cerr << "MISMATCH BETWEEN from=" << fromOrd << " to=" << toOrd
                                          << " exp=" << exp << " got=" << got << "\n";
            }
        } else if (tag == "INVERSE") {
            // p[1]=cycleOrd p[2]=inverseOrd
            if (p.size() < 3) continue;
            int cycleOrd = std::stoi(p[1]);
            int exp = std::stoi(p[2]);
            int got = static_cast<int>(mc::axisCycleInverse(cyc(cycleOrd)));
            ++cases;
            if (got != exp) {
                ++mism;
                if (mism <= 20) std::cerr << "MISMATCH INVERSE cycle=" << cycleOrd
                                          << " exp=" << exp << " got=" << got << "\n";
            }
        } else if (tag == "CYCLEAX") {
            // p[1]=cycleOrd p[2]=axisOrd p[3]=resultAxisOrd
            if (p.size() < 4) continue;
            int cycleOrd = std::stoi(p[1]);
            int axisOrd = std::stoi(p[2]);
            int exp = std::stoi(p[3]);
            int got = static_cast<int>(mc::axisCycleAxis(cyc(cycleOrd), ax(axisOrd)));
            ++cases;
            if (got != exp) {
                ++mism;
                if (mism <= 20) std::cerr << "MISMATCH CYCLEAX cycle=" << cycleOrd << " axis=" << axisOrd
                                          << " exp=" << exp << " got=" << got << "\n";
            }
        } else if (tag == "CYCLEI") {
            // p[1]=cycleOrd p[2]=x p[3]=y p[4]=z p[5]=axisOrd p[6]=resultInt
            if (p.size() < 7) continue;
            int cycleOrd = std::stoi(p[1]);
            int x = std::stoi(p[2]);
            int y = std::stoi(p[3]);
            int z = std::stoi(p[4]);
            int axisOrd = std::stoi(p[5]);
            int exp = std::stoi(p[6]);
            int got = mc::axisCycleChoose<int>(cyc(cycleOrd), x, y, z, ax(axisOrd));
            ++cases;
            if (got != exp) {
                ++mism;
                if (mism <= 20) std::cerr << "MISMATCH CYCLEI cycle=" << cycleOrd
                                          << " x=" << x << " y=" << y << " z=" << z
                                          << " axis=" << axisOrd << " exp=" << exp << " got=" << got << "\n";
            }
        } else if (tag == "CYCLED") {
            // p[1]=cycleOrd p[2]=xBits p[3]=yBits p[4]=zBits p[5]=axisOrd p[6]=resultDoubleBits
            if (p.size() < 7) continue;
            int cycleOrd = std::stoi(p[1]);
            double x = db(p[2]);
            double y = db(p[3]);
            double z = db(p[4]);
            int axisOrd = std::stoi(p[5]);
            uint64_t exp = (uint64_t)std::stoull(p[6], nullptr, 16);
            double got = mc::axisCycleChoose<double>(cyc(cycleOrd), x, y, z, ax(axisOrd));
            ++cases;
            if (bd(got) != exp) {
                ++mism;
                if (mism <= 20) std::cerr << "MISMATCH CYCLED cycle=" << cycleOrd
                                          << " axis=" << axisOrd << " exp=" << std::hex << exp
                                          << " got=" << bd(got) << std::dec << "\n";
            }
        }
        // Unknown tags ignored (forward-compatible).
    }

    std::cout << "AxisCycle cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
