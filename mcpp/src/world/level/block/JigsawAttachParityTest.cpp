// Parity test for the jigsaw ATTACHMENT predicate
//   net.minecraft.world.level.block.JigsawBlock.canAttach(JigsawBlockInfo, JigsawBlockInfo)
// and the rotation shuffle
//   net.minecraft.world.level.block.Rotation.getShuffled(RandomSource)
// (Minecraft Java Edition 26.1.2).
//
// Ground truth: tools/JigsawAttachParity.java drives the REAL JigsawBlock.canAttach
// over a battery of (sourceFront x sourceTop x sourceJoint x targetFront x targetTop x
// name/target match) combos, and the REAL Rotation.getShuffled over ~200 seeds.
// We reproduce both with the ported JigsawAttach.h and compare bit-exact:
//   * ROTORD / JOINTORD / FAT lines lock the enum ordinals the TSV exchanges depend on.
//   * ATTACH lines: re-run canAttach with the same projected inputs, compare the boolean.
//   * SHUF lines: re-run rotationGetShuffled on a LegacyRandomSource(seed), compare the
//     4-rotation order (which also proves the nextInt(4),nextInt(3),nextInt(2) stream).
//
//   jigsaw_attach_parity --cases mcpp/build/jigsaw_attach.tsv

#include "JigsawAttach.h"

#include "world/level/levelgen/RandomSource.h"
#include "world/phys/Direction.h"

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::Direction;
using mc::block::canAttach;
using mc::block::JointType;
using mc::block::Rotation;
using mc::block::rotationGetShuffled;
using mc::block::ROTATION_VALUES;
using mc::levelgen::LegacyRandomSource;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int toi(const std::string& s) { return static_cast<int>(std::stoll(s)); }
long long tol(const std::string& s) { return std::stoll(s); }

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) {
        std::cerr << "usage: jigsaw_attach_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;

    auto fail = [&](const std::string& detail) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << detail << "\n";
    };

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = splitTabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "ROTORD") {
            // ROTORD <name> <ordinal> — verify the ported Rotation ordinal matches.
            ++total;
            int ord = toi(p[2]);
            const std::string& name = p[1];
            int got = -1;
            if (name == "NONE") got = static_cast<int>(Rotation::NONE);
            else if (name == "CLOCKWISE_90") got = static_cast<int>(Rotation::CLOCKWISE_90);
            else if (name == "CLOCKWISE_180") got = static_cast<int>(Rotation::CLOCKWISE_180);
            else if (name == "COUNTERCLOCKWISE_90") got = static_cast<int>(Rotation::COUNTERCLOCKWISE_90);
            if (got != ord) fail(line + " gotOrd=" + std::to_string(got));
        } else if (tag == "JOINTORD") {
            // JOINTORD <name> <ordinal> — verify the ported JointType ordinal matches.
            ++total;
            int ord = toi(p[2]);
            const std::string& name = p[1];
            int got = -1;
            if (name == "ROLLABLE") got = static_cast<int>(JointType::ROLLABLE);
            else if (name == "ALIGNED") got = static_cast<int>(JointType::ALIGNED);
            if (got != ord) fail(line + " gotOrd=" + std::to_string(got));
        } else if (tag == "FAT") {
            // FAT <name> <frontOrd> <topOrd> — informational lock of FrontAndTop
            // front()/top(); we just confirm the ordinals are valid Directions [0,5].
            ++total;
            int frontOrd = toi(p[2]);
            int topOrd = toi(p[3]);
            if (frontOrd < 0 || frontOrd > 5 || topOrd < 0 || topOrd > 5)
                fail(line + " out-of-range direction ordinal");
        } else if (tag == "ATTACH") {
            // ATTACH sFrontOrd sTopOrd sJointOrd sTarget tFrontOrd tTopOrd tName result01
            ++total;
            Direction sFront = static_cast<Direction>(toi(p[1]));
            Direction sTop = static_cast<Direction>(toi(p[2]));
            JointType sJoint = static_cast<JointType>(toi(p[3]));
            const std::string& sTarget = p[4];
            Direction tFront = static_cast<Direction>(toi(p[5]));
            Direction tTop = static_cast<Direction>(toi(p[6]));
            const std::string& tName = p[7];
            int exp = toi(p[8]);
            bool got = canAttach(sFront, sTop, sTarget, sJoint, tFront, tTop, tName);
            if ((got ? 1 : 0) != exp)
                fail(line + " got=" + std::to_string(got ? 1 : 0));
        } else if (tag == "SHUF") {
            // SHUF seed ord0 ord1 ord2 ord3
            ++total;
            int64_t seed = tol(p[1]);
            LegacyRandomSource random(seed);
            std::array<Rotation, 4> order = rotationGetShuffled(random);
            bool ok = true;
            for (int k = 0; k < 4; ++k) {
                int exp = toi(p[2 + k]);
                if (static_cast<int>(order[k]) != exp) {
                    ok = false;
                    break;
                }
            }
            if (!ok) {
                std::string gotStr;
                for (int k = 0; k < 4; ++k)
                    gotStr += " " + std::to_string(static_cast<int>(order[k]));
                fail(line + " got" + gotStr);
            }
        }
        // unknown tags (e.g. stray JVM bootstrap log lines) are ignored.
    }

    std::cout << "JigsawAttach checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
