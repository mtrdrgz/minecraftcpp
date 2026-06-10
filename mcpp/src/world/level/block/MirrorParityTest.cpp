// Parity test for net.minecraft.world.level.block.Mirror.
//
// The Mirror enum + its three pure methods (mirror(int,int),
// getRotation(Direction), mirror(Direction)) are already certified byte-exact in
// world/level/levelgen/structure/templatesystem/StructureTransforms.h; this gate
// re-verifies them as a standalone Mirror gate AND additionally certifies the
// StringRepresentable surface (ordinal/name/getSerializedName) via the companion
// MirrorSerializedName.h. Ground truth: tools/MirrorParity.java against the real
// 26.1.2 class.
//
//   mirror_parity --cases mcpp/build/mirror.tsv

#include "MirrorSerializedName.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::Direction;
using mc::levelgen::structure::Mirror;
using mc::levelgen::structure::Rotation;
using mc::levelgen::structure::mirrorGetRotation;
using mc::levelgen::structure::mirrorMirror;
using mc::levelgen::structure::mirrorMirrorInt;
using mc::block::mirrorGetSerializedName;
using mc::block::mirrorName;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int toi(const std::string& s) { return std::stoi(s); }

Mirror mir(int i) { return static_cast<Mirror>(i); }
Direction dir(int i) { return static_cast<Direction>(i); }
int mirIdx(Mirror m) { return static_cast<int>(m); }
int rotIdx(Rotation r) { return static_cast<int>(r); }
int dirIdx(Direction d) { return static_cast<int>(d); }

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) { std::cerr << "usage: mirror_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;

    auto fail = [&](const std::string& tag, const std::string& detail) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << tag << " " << detail << "\n";
    };

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = splitTabs(line);
        const std::string& tag = p[0];
        ++total;

        if (tag == "MIR_META") {
            // p[1]=mirror index, p[2]=ordinal, p[3]=name(), p[4]=getSerializedName()
            Mirror m = mir(toi(p[1]));
            int gotOrdinal = mirIdx(m); // C++ enum value == Java ordinal by construction
            std::string gotName = mirrorName(m);
            std::string gotSerial = mirrorGetSerializedName(m);
            if (gotOrdinal != toi(p[2]) || gotName != p[3] || gotSerial != p[4])
                fail(tag, line + " got=" + std::to_string(gotOrdinal) + "," + gotName + "," + gotSerial);
        } else if (tag == "MIR_INT") {
            int got = mirrorMirrorInt(mir(toi(p[1])), toi(p[2]), toi(p[3]));
            if (got != toi(p[4])) fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "MIR_GETROT") {
            int got = rotIdx(mirrorGetRotation(mir(toi(p[1])), dir(toi(p[2]))));
            if (got != toi(p[3])) fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "MIR_DIR") {
            int got = dirIdx(mirrorMirror(mir(toi(p[1])), dir(toi(p[2]))));
            if (got != toi(p[3])) fail(tag, line + " got=" + std::to_string(got));
        } else {
            fail("UNKNOWN_TAG", tag);
        }
    }

    std::cout << "Mirror cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
