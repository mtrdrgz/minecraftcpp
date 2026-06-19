// Parity test for the pure structure-transform math (Rotation / Mirror /
// BoundingBox / StructureTemplate static transforms). Ground truth is generated
// by tools/StructureTransformsParity.java against the real 26.1.2 classes.
//
// Each TSV row carries its inputs and the expected output(s); we recompute from
// the inputs via StructureTransforms.h and compare. Doubles are compared as raw
// IEEE-754 bits (the TSV carries 16-hex bit patterns), so the gate is bit-exact.
//
//   structure_transform_parity --cases mcpp/build/structure_transforms.tsv

#include "StructureTransforms.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen::structure;
using mc::Direction;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int toi(const std::string& s) { return std::stoi(s); }
uint64_t tobits(const std::string& s) { return std::stoull(s, nullptr, 16); }
double bitsToDouble(uint64_t b) { double d; std::memcpy(&d, &b, sizeof(d)); return d; }

Rotation rot(int i) { return static_cast<Rotation>(i); }
Mirror mir(int i) { return static_cast<Mirror>(i); }
Direction dir(int i) { return static_cast<Direction>(i); }

int rotIdx(Rotation r) { return static_cast<int>(r); }
int dirIdx(Direction d) { return static_cast<int>(d); }

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) { std::cerr << "usage: structure_transform_parity --cases <tsv>\n"; return 2; }

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
        if (line.empty()) continue;
        auto p = splitTabs(line);
        const std::string& tag = p[0];
        ++total;

        if (tag == "ROT_GETROTATED") {
            int got = rotIdx(rotationGetRotated(rot(toi(p[1])), rot(toi(p[2]))));
            if (got != toi(p[3])) fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "ROT_DIR") {
            int got = dirIdx(rotationRotate(rot(toi(p[1])), dir(toi(p[2]))));
            if (got != toi(p[3])) fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "ROT_INT") {
            int got = rotationRotateInt(rot(toi(p[1])), toi(p[2]), toi(p[3]));
            if (got != toi(p[4])) fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "MIR_INT") {
            int got = mirrorMirrorInt(mir(toi(p[1])), toi(p[2]), toi(p[3]));
            if (got != toi(p[4])) fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "MIR_GETROT") {
            int got = rotIdx(mirrorGetRotation(mir(toi(p[1])), dir(toi(p[2]))));
            if (got != toi(p[3])) fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "MIR_DIR") {
            int got = dirIdx(mirrorMirror(mir(toi(p[1])), dir(toi(p[2]))));
            if (got != toi(p[3])) fail(tag, line + " got=" + std::to_string(got));
        } else if (tag == "XFORM") {
            BlockPos o = structureTransform(BlockPos{toi(p[1]), toi(p[2]), toi(p[3])}, mir(toi(p[4])), rot(toi(p[5])),
                                            BlockPos{toi(p[6]), toi(p[7]), toi(p[8])});
            if (o.x != toi(p[9]) || o.y != toi(p[10]) || o.z != toi(p[11]))
                fail(tag, line + " got=" + std::to_string(o.x) + "," + std::to_string(o.y) + "," + std::to_string(o.z));
        } else if (tag == "XFORMV") {
            Vec3d o = structureTransform(Vec3d{bitsToDouble(tobits(p[1])), bitsToDouble(tobits(p[2])), bitsToDouble(tobits(p[3]))},
                                         mir(toi(p[4])), rot(toi(p[5])), BlockPos{toi(p[6]), toi(p[7]), toi(p[8])});
            uint64_t gx, gy, gz; std::memcpy(&gx, &o.x, 8); std::memcpy(&gy, &o.y, 8); std::memcpy(&gz, &o.z, 8);
            if (gx != tobits(p[9]) || gy != tobits(p[10]) || gz != tobits(p[11]))
                fail(tag, line);
        } else if (tag == "ZEROPOS") {
            BlockPos o = getZeroPositionWithTransform({toi(p[1]), toi(p[2]), toi(p[3])}, mir(toi(p[4])), rot(toi(p[5])),
                                                      toi(p[6]), toi(p[7]));
            if (o.x != toi(p[8]) || o.y != toi(p[9]) || o.z != toi(p[10]))
                fail(tag, line + " got=" + std::to_string(o.x) + "," + std::to_string(o.y) + "," + std::to_string(o.z));
        } else if (tag == "BBOX") {
            BoundingBox bb = structureGetBoundingBox({toi(p[1]), toi(p[2]), toi(p[3])}, rot(toi(p[4])),
                                                     {toi(p[5]), toi(p[6]), toi(p[7])}, mir(toi(p[8])),
                                                     {toi(p[9]), toi(p[10]), toi(p[11])});
            if (bb.minX != toi(p[12]) || bb.minY != toi(p[13]) || bb.minZ != toi(p[14]) ||
                bb.maxX != toi(p[15]) || bb.maxY != toi(p[16]) || bb.maxZ != toi(p[17]))
                fail(tag, line);
        } else if (tag == "ORIENT") {
            BoundingBox bb = BoundingBox::orientBox(toi(p[1]), toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]), toi(p[6]),
                                                    toi(p[7]), toi(p[8]), toi(p[9]), dir(toi(p[10])));
            if (bb.minX != toi(p[11]) || bb.minY != toi(p[12]) || bb.minZ != toi(p[13]) ||
                bb.maxX != toi(p[14]) || bb.maxY != toi(p[15]) || bb.maxZ != toi(p[16]))
                fail(tag, line);
        } else if (tag == "BB_CTOR") {
            BoundingBox bb(toi(p[1]), toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]), toi(p[6]));
            if (bb.minX != toi(p[7]) || bb.minY != toi(p[8]) || bb.minZ != toi(p[9]) ||
                bb.maxX != toi(p[10]) || bb.maxY != toi(p[11]) || bb.maxZ != toi(p[12]))
                fail(tag, line);
        } else if (tag == "BB_CENTER") {
            BoundingBox bb(toi(p[1]), toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]), toi(p[6]));
            Vec3i c = bb.getCenter();
            if (c.x != toi(p[7]) || c.y != toi(p[8]) || c.z != toi(p[9])) fail(tag, line);
        } else if (tag == "BB_SPAN") {
            BoundingBox bb(toi(p[1]), toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]), toi(p[6]));
            if (bb.getXSpan() != toi(p[7]) || bb.getYSpan() != toi(p[8]) || bb.getZSpan() != toi(p[9])) fail(tag, line);
        } else if (tag == "BB_MOVED") {
            BoundingBox bb(toi(p[1]), toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]), toi(p[6]));
            BoundingBox m = bb.moved(toi(p[7]), toi(p[8]), toi(p[9]));
            if (m.minX != toi(p[10]) || m.minY != toi(p[11]) || m.minZ != toi(p[12]) ||
                m.maxX != toi(p[13]) || m.maxY != toi(p[14]) || m.maxZ != toi(p[15]))
                fail(tag, line);
        } else if (tag == "BB_INFLATE") {
            BoundingBox bb(toi(p[1]), toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]), toi(p[6]));
            BoundingBox m = bb.inflatedBy(toi(p[7]), toi(p[8]), toi(p[9]));
            if (m.minX != toi(p[10]) || m.minY != toi(p[11]) || m.minZ != toi(p[12]) ||
                m.maxX != toi(p[13]) || m.maxY != toi(p[14]) || m.maxZ != toi(p[15]))
                fail(tag, line);
        } else if (tag == "BB_FROMCORNERS") {
            BoundingBox bb = BoundingBox::fromCorners({toi(p[1]), toi(p[2]), toi(p[3])}, {toi(p[4]), toi(p[5]), toi(p[6])});
            if (bb.minX != toi(p[7]) || bb.minY != toi(p[8]) || bb.minZ != toi(p[9]) ||
                bb.maxX != toi(p[10]) || bb.maxY != toi(p[11]) || bb.maxZ != toi(p[12]))
                fail(tag, line);
        } else if (tag == "BB_INTERSECT") {
            BoundingBox a(toi(p[1]), toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]), toi(p[6]));
            BoundingBox b(toi(p[7]), toi(p[8]), toi(p[9]), toi(p[10]), toi(p[11]), toi(p[12]));
            if ((a.intersects(b) ? 1 : 0) != toi(p[13])) fail(tag, line);
        } else if (tag == "BB_ENCAPS") {
            BoundingBox a(toi(p[1]), toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]), toi(p[6]));
            BoundingBox b(toi(p[7]), toi(p[8]), toi(p[9]), toi(p[10]), toi(p[11]), toi(p[12]));
            BoundingBox e = BoundingBox::encapsulating(a, b);
            if (e.minX != toi(p[13]) || e.minY != toi(p[14]) || e.minZ != toi(p[15]) ||
                e.maxX != toi(p[16]) || e.maxY != toi(p[17]) || e.maxZ != toi(p[18]))
                fail(tag, line);
        } else if (tag == "BB_ISINSIDE") {
            BoundingBox a(toi(p[1]), toi(p[2]), toi(p[3]), toi(p[4]), toi(p[5]), toi(p[6]));
            if ((a.isInside(toi(p[7]), toi(p[8]), toi(p[9])) ? 1 : 0) != toi(p[10])) fail(tag, line);
        } else {
            fail("UNKNOWN_TAG", tag);
        }
    }

    std::cout << "StructureTransforms cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
