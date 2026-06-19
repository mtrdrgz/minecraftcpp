// Parity test for the PURE integer geometry of
//   net.minecraft.world.level.levelgen.structure.BoundingBox (26.1.2).
//
// Ground truth: tools/BoundingBoxParity.java drives the REAL class (ctor incl.
// inverted-bounds fix, fromCorners, orientBox, intersectingChunks, intersects,
// encapsulating/encapsulate, moved, inflatedBy, isInside, getLength, getXSpan/
// getYSpan/getZSpan, getCenter) and emits a TSV. We recompute each row from
// BoundingBox.h and compare. All values are decimal ints / packed longs, so the
// gate is exact.
//
//   bounding_box_parity --cases mcpp/build/bounding_box.tsv

#include "BoundingBox.h"

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
int64_t tol(const std::string& s) { return std::stoll(s); }

Direction dir(int i) { return static_cast<Direction>(i); }

BoundingBox box6(const std::vector<std::string>& p, std::size_t i) {
    return BoundingBox(toi(p[i]), toi(p[i + 1]), toi(p[i + 2]),
                       toi(p[i + 3]), toi(p[i + 4]), toi(p[i + 5]));
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) { std::cerr << "usage: bounding_box_parity --cases <tsv>\n"; return 2; }

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

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = splitTabs(line);
        const std::string& tag = p[0];

        // The REAL BoundingBox ctor logs an ERROR (via Util.logAndPauseIfInIde)
        // when it is handed inverted bounds — which our CTOR/CORNER battery does
        // on purpose to exercise the swap fix. Logback writes those lines to
        // stdout, interleaving them into the TSV. They are not data rows; skip
        // any line whose first field is not one of our fixed tags.
        static const std::vector<std::string> kTags = {
            "CTOR", "CORNER", "ORIENT", "SPAN", "LEN", "CENTER", "MOVED", "INFL",
            "ISIN", "ENCAPB", "ENCPOS", "ISECT", "ISECT4", "CHUNKS"};
        bool known = false;
        for (const auto& t : kTags) if (tag == t) { known = true; break; }
        if (!known) continue;
        ++total;

        if (tag == "CTOR") {
            // x0 y0 z0 x1 y1 z1 | box6
            BoundingBox b(toi(p[1]), toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]), toi(p[6]));
            if (!eqBox(b, p, 7)) fail(tag, line + " got=" + boxStr(b));
        } else if (tag == "CORNER") {
            // ax ay az bx by bz | box6
            BoundingBox b = BoundingBox::fromCorners({toi(p[1]), toi(p[2]), toi(p[3])},
                                                     {toi(p[4]), toi(p[5]), toi(p[6])});
            if (!eqBox(b, p, 7)) fail(tag, line + " got=" + boxStr(b));
        } else if (tag == "ORIENT") {
            // fx fy fz ox oy oz w h d dirOrd | box6
            BoundingBox b = BoundingBox::orientBox(toi(p[1]), toi(p[2]), toi(p[3]),
                                                   toi(p[4]), toi(p[5]), toi(p[6]),
                                                   toi(p[7]), toi(p[8]), toi(p[9]), dir(toi(p[10])));
            if (!eqBox(b, p, 11)) fail(tag, line + " got=" + boxStr(b));
        } else if (tag == "SPAN") {
            // a6 | xSpan ySpan zSpan
            BoundingBox a = box6(p, 1);
            if (a.getXSpan() != toi(p[7]) || a.getYSpan() != toi(p[8]) || a.getZSpan() != toi(p[9]))
                fail(tag, line + " got=" + std::to_string(a.getXSpan()) + "," +
                          std::to_string(a.getYSpan()) + "," + std::to_string(a.getZSpan()));
        } else if (tag == "LEN") {
            // a6 | lx ly lz
            BoundingBox a = box6(p, 1);
            Vec3i l = a.getLength();
            if (l.x != toi(p[7]) || l.y != toi(p[8]) || l.z != toi(p[9]))
                fail(tag, line + " got=" + std::to_string(l.x) + "," + std::to_string(l.y) + "," + std::to_string(l.z));
        } else if (tag == "CENTER") {
            // a6 | cx cy cz
            BoundingBox a = box6(p, 1);
            Vec3i c = a.getCenter();
            if (c.x != toi(p[7]) || c.y != toi(p[8]) || c.z != toi(p[9]))
                fail(tag, line + " got=" + std::to_string(c.x) + "," + std::to_string(c.y) + "," + std::to_string(c.z));
        } else if (tag == "MOVED") {
            // a6 dx dy dz | box6
            BoundingBox b = box6(p, 1).moved(toi(p[7]), toi(p[8]), toi(p[9]));
            if (!eqBox(b, p, 10)) fail(tag, line + " got=" + boxStr(b));
        } else if (tag == "INFL") {
            // a6 ix iy iz | box6
            BoundingBox b = box6(p, 1).inflatedBy(toi(p[7]), toi(p[8]), toi(p[9]));
            if (!eqBox(b, p, 10)) fail(tag, line + " got=" + boxStr(b));
        } else if (tag == "ISIN") {
            // a6 x y z | 0|1
            BoundingBox a = box6(p, 1);
            int got = a.isInside(toi(p[7]), toi(p[8]), toi(p[9])) ? 1 : 0;
            if (got != toi(p[10])) fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "ENCAPB") {
            // a6 b6 | box6
            BoundingBox r = BoundingBox::encapsulating(box6(p, 1), box6(p, 7));
            if (!eqBox(r, p, 13)) fail(tag, line + " got=" + boxStr(r));
        } else if (tag == "ENCPOS") {
            // a6 px py pz | box6
            BoundingBox a = box6(p, 1);
            a.encapsulate(Vec3i{toi(p[7]), toi(p[8]), toi(p[9])});
            if (!eqBox(a, p, 10)) fail(tag, line + " got=" + boxStr(a));
        } else if (tag == "ISECT") {
            // a6 b6 | 0|1
            int got = box6(p, 1).intersects(box6(p, 7)) ? 1 : 0;
            if (got != toi(p[13])) fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "ISECT4") {
            // a6 minX minZ maxX maxZ | 0|1
            int got = box6(p, 1).intersects(toi(p[7]), toi(p[8]), toi(p[9]), toi(p[10])) ? 1 : 0;
            if (got != toi(p[11])) fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "CHUNKS") {
            // a6 | n packed*n  (order matters)
            BoundingBox a = box6(p, 1);
            std::vector<int64_t> got = a.intersectingChunks();
            int n = toi(p[7]);
            bool ok = static_cast<int>(got.size()) == n;
            for (int i = 0; ok && i < n; ++i) ok = got[i] == tol(p[8 + i]);
            if (!ok) {
                std::string g = std::to_string(got.size()) + "[";
                for (auto v : got) g += std::to_string(v) + ",";
                g += "]";
                fail(tag, line + " got=" + g);
            }
        }
    }

    std::cout << "BoundingBox checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
