// Parity test for the PURE aggregation / corner / identity helpers of
//   net.minecraft.world.level.levelgen.structure.BoundingBox (26.1.2)
// that the sibling bounding_box gate does NOT exercise (see BoundingBox.h:35-39).
//
// Ground truth: tools/BoundingBoxAggregateParity.java drives the REAL class
// (encapsulate(box), move(int,int,int), encapsulatingPositions(Iterable),
// forAllCorners, hashCode, equals) and emits a TSV. We recompute each row from
// BoundingBoxAggregate.h (+ BoundingBox.h) and compare. All values are decimal
// ints, so the gate is exact.
//
//   bounding_box_aggregate_parity --cases mcpp/build/bounding_box_aggregate.tsv

#include "BoundingBoxAggregate.h"

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen::structure;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int32_t toi(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

BoundingBox box6(const std::vector<std::string>& p, std::size_t i) {
    return BoundingBox(toi(p[i]), toi(p[i + 1]), toi(p[i + 2]),
                       toi(p[i + 3]), toi(p[i + 4]), toi(p[i + 5]));
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) { std::cerr << "usage: bounding_box_aggregate_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;

    auto fail = [&](const std::string& tag, const std::string& detail) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << tag << " " << detail << "\n";
    };

    auto eqBox = [](const BoundingBox& b, const std::vector<std::string>& p, std::size_t i) {
        return b.minX == toi(p[i]) && b.minY == toi(p[i + 1]) && b.minZ == toi(p[i + 2]) &&
               b.maxX == toi(p[i + 3]) && b.maxY == toi(p[i + 4]) && b.maxZ == toi(p[i + 5]);
    };
    auto boxStr = [](const BoundingBox& b) {
        return std::to_string(b.minX) + "," + std::to_string(b.minY) + "," + std::to_string(b.minZ) +
               "," + std::to_string(b.maxX) + "," + std::to_string(b.maxY) + "," + std::to_string(b.maxZ);
    };

    // Known data-row tags. The REAL BoundingBox ctor logs an ERROR via
    // Util.logAndPauseIfInIde for inverted bounds; logback may interleave such
    // lines into stdout. Our battery uses only well-ordered boxes, but we still
    // skip any line whose first field is not one of our fixed tags (harness rule).
    static const std::array<const char*, 6> kTags = {
        "ENCAP", "MOVE", "ENCPOSN", "CORNERS", "HASH", "EQ"};

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = splitTabs(line);
        const std::string& tag = p[0];

        bool known = false;
        for (const char* t : kTags) if (tag == t) { known = true; break; }
        if (!known) continue;
        ++total;

        if (tag == "ENCAP") {
            // a6 b6 | box6  (a after a.encapsulate(b))
            BoundingBox a = box6(p, 1);
            a.encapsulate(box6(p, 7));
            if (!eqBox(a, p, 13)) fail(tag, line + " got=" + boxStr(a));
        } else if (tag == "MOVE") {
            // a6 dx dy dz | box6  (a after a.move(...))
            BoundingBox a = box6(p, 1);
            a.move(toi(p[7]), toi(p[8]), toi(p[9]));
            if (!eqBox(a, p, 10)) fail(tag, line + " got=" + boxStr(a));
        } else if (tag == "ENCPOSN") {
            // n (px py pz)*n | present [box6]
            int n = toi(p[1]);
            std::vector<Vec3i> pts;
            pts.reserve(static_cast<std::size_t>(n));
            std::size_t idx = 2;
            for (int k = 0; k < n; ++k) {
                pts.push_back({toi(p[idx]), toi(p[idx + 1]), toi(p[idx + 2])});
                idx += 3;
            }
            EncapsulatingResult r = encapsulatingPositions(pts.data(), pts.size());
            int present = toi(p[idx]);
            if ((r.present ? 1 : 0) != present) {
                fail(tag, line + " got present=" + std::to_string(r.present ? 1 : 0));
            } else if (present == 1) {
                if (!eqBox(r.box, p, idx + 1)) fail(tag, line + " got=" + boxStr(r.box));
            }
        } else if (tag == "CORNERS") {
            // a6 | (cx cy cz)*8  in fixed visitation order
            BoundingBox a = box6(p, 1);
            std::array<Vec3i, 8> got = boundingBoxCorners(a);
            bool ok = true;
            std::size_t idx = 7;
            for (int c = 0; c < 8 && ok; ++c) {
                ok = got[static_cast<std::size_t>(c)].x == toi(p[idx]) &&
                     got[static_cast<std::size_t>(c)].y == toi(p[idx + 1]) &&
                     got[static_cast<std::size_t>(c)].z == toi(p[idx + 2]);
                idx += 3;
            }
            if (!ok) {
                std::string g;
                for (const auto& c : got)
                    g += "(" + std::to_string(c.x) + "," + std::to_string(c.y) + "," + std::to_string(c.z) + ")";
                fail(tag, line + " got=" + g);
            }
        } else if (tag == "HASH") {
            // a6 | int
            BoundingBox a = box6(p, 1);
            int32_t got = boundingBoxHashCode(a);
            if (got != toi(p[7])) fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "EQ") {
            // a6 b6 | 0|1
            int got = boundingBoxEquals(box6(p, 1), box6(p, 7)) ? 1 : 0;
            if (got != toi(p[13])) fail(tag, line + " got=" + std::to_string(got));
        }
    }

    std::cout << "BoundingBoxAggregate checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
