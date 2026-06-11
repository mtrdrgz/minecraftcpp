// Parity gate for mc::render::octree (render/Octree.h) vs the REAL
// net.minecraft.client.renderer.Octree. Recomputes every ground-truth row from
// tools/OctreeParity.java and checks bit-for-bit:
//   ROOT   — the Octree(SectionPos,renderDistance,sectionsPerChunk,minBlockY) root BB
//   BRANCH — the root Branch's bbCenter*, AxisSorting ordinal, camera*DiffNegative
//   CHILD  — Branch.createChildBoundingBox for all 8 octants
//   SORT   — AxisSorting.getAxisSorting ordinal
//   NODE   — Branch.getNodeIndex child-octant index
//   CLOSE  — Octree.isClose boolean
//
// All values are exact integers / booleans (no float rounding), so equality is
// decimal/bit-exact.
//
//   octree_parity --cases mcpp/build/octree.tsv

#include "Octree.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace oc = mc::render::octree;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int i(const std::string& s) { return std::stoi(s); }
long long ll(const std::string& s) { return std::stoll(s); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: octree_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long checks = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& why) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << why << "\n";
    };
    auto eq = [&](long long got, long long exp, const std::string& what) {
        ++checks;
        if (got != exp) fail(what + " got=" + std::to_string(got) + " exp=" + std::to_string(exp));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];

        if (t == "ROOT") {
            // ROOT secX secY secZ rd spc minBlockY  minX minY minZ maxX maxY maxZ
            int secX = i(p[1]), secY = i(p[2]), secZ = i(p[3]), rd = i(p[4]), spc = i(p[5]), my = i(p[6]);
            oc::BoundingBox bb = oc::rootBoundingBox(secX, secY, secZ, rd, spc, my);
            std::string k = "ROOT(" + p[1] + "," + p[2] + "," + p[3] + ",rd" + p[4] + ",spc" + p[5] + ",my" + p[6] + ")";
            eq(bb.minX, i(p[7]), k + ".minX");
            eq(bb.minY, i(p[8]), k + ".minY");
            eq(bb.minZ, i(p[9]), k + ".minZ");
            eq(bb.maxX, i(p[10]), k + ".maxX");
            eq(bb.maxY, i(p[11]), k + ".maxY");
            eq(bb.maxZ, i(p[12]), k + ".maxZ");
        } else if (t == "BRANCH") {
            // BRANCH secX secY secZ rd spc minBlockY  cx cy cz sortingOrd xNeg yNeg zNeg
            int secX = i(p[1]), secY = i(p[2]), secZ = i(p[3]), rd = i(p[4]), spc = i(p[5]), my = i(p[6]);
            oc::BoundingBox bb = oc::rootBoundingBox(secX, secY, secZ, rd, spc, my);
            oc::Branch br = oc::Branch::make(bb, oc::cameraCenterX(secX), oc::cameraCenterY(secY),
                                             oc::cameraCenterZ(secZ));
            std::string k = "BRANCH(" + p[1] + "," + p[2] + "," + p[3] + ",rd" + p[4] + ")";
            eq(br.bbCenterX, i(p[7]), k + ".cx");
            eq(br.bbCenterY, i(p[8]), k + ".cy");
            eq(br.bbCenterZ, i(p[9]), k + ".cz");
            eq(static_cast<int>(br.sorting), i(p[10]), k + ".sortingOrd");
            eq(br.cameraXDiffNegative ? 1 : 0, i(p[11]), k + ".xNeg");
            eq(br.cameraYDiffNegative ? 1 : 0, i(p[12]), k + ".yNeg");
            eq(br.cameraZDiffNegative ? 1 : 0, i(p[13]), k + ".zNeg");
        } else if (t == "CHILD") {
            // CHILD secX secY secZ rd spc minBlockY  xNeg yNeg zNeg  cminX cminY cminZ cmaxX cmaxY cmaxZ
            int secX = i(p[1]), secY = i(p[2]), secZ = i(p[3]), rd = i(p[4]), spc = i(p[5]), my = i(p[6]);
            oc::BoundingBox bb = oc::rootBoundingBox(secX, secY, secZ, rd, spc, my);
            oc::Branch br = oc::Branch::make(bb, oc::cameraCenterX(secX), oc::cameraCenterY(secY),
                                             oc::cameraCenterZ(secZ));
            bool xn = i(p[7]) != 0, yn = i(p[8]) != 0, zn = i(p[9]) != 0;
            oc::BoundingBox cb = br.createChildBoundingBox(xn, yn, zn);
            std::string k = "CHILD(" + p[1] + "," + p[2] + "," + p[3] + ",rd" + p[4] + ",o" + p[7] + p[8] + p[9] + ")";
            eq(cb.minX, i(p[10]), k + ".minX");
            eq(cb.minY, i(p[11]), k + ".minY");
            eq(cb.minZ, i(p[12]), k + ".minZ");
            eq(cb.maxX, i(p[13]), k + ".maxX");
            eq(cb.maxY, i(p[14]), k + ".maxY");
            eq(cb.maxZ, i(p[15]), k + ".maxZ");
        } else if (t == "SORT") {
            // SORT absX absY absZ  ordinal
            oc::AxisSorting s = oc::getAxisSorting(i(p[1]), i(p[2]), i(p[3]));
            eq(static_cast<int>(s), i(p[4]),
               "SORT(" + p[1] + "," + p[2] + "," + p[3] + ")");
        } else if (t == "NODE") {
            // NODE sortingOrd xOpp yOpp zOpp  index
            oc::AxisSorting s = static_cast<oc::AxisSorting>(i(p[1]));
            int idx = oc::getNodeIndex(s, i(p[2]) != 0, i(p[3]) != 0, i(p[4]) != 0);
            eq(idx, i(p[5]),
               "NODE(s" + p[1] + ",o" + p[2] + p[3] + p[4] + ")");
        } else if (t == "CLOSE") {
            // CLOSE secX secY secZ  minX minY minZ maxX maxY maxZ closeDistance  result
            int secX = i(p[1]), secY = i(p[2]), secZ = i(p[3]);
            double minX = static_cast<double>(ll(p[4])), minY = static_cast<double>(ll(p[5])),
                   minZ = static_cast<double>(ll(p[6])), maxX = static_cast<double>(ll(p[7])),
                   maxY = static_cast<double>(ll(p[8])), maxZ = static_cast<double>(ll(p[9]));
            int dist = i(p[10]);
            bool r = oc::isClose(oc::cameraCenterX(secX), oc::cameraCenterY(secY), oc::cameraCenterZ(secZ),
                                 minX, minY, minZ, maxX, maxY, maxZ, dist);
            eq(r ? 1 : 0, i(p[11]),
               "CLOSE(c" + p[1] + "," + p[2] + "," + p[3] + ",d" + p[10] + ")");
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "Octree checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
