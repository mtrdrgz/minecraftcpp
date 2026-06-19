// VERIFY parity test for net.minecraft.world.entity.EntityDimensions (26.1.2).
//
// Independent re-verification of the already-certified C++ port
// (mcpp/src/world/entity/EntityDimensions.h). Ground truth:
// tools/EntityDimensionsVerifyParity.java.
//
//   entity_dimensions_verify_parity --cases mcpp/build/entity_dimensions_verify.tsv
//
// Every float is compared by its raw 32-bit IEEE pattern and every double by its
// raw 64-bit pattern (never by value) via std::bit_cast, so -0.0 / Inf /
// subnormals / NaN all check faithfully.
//
// Unlike the original gate (which decodes inputs from the row NAME), each row here
// carries its construction inputs as explicit hex columns. The C++ side rebuilds
// the EntityDimensions from those inputs using the SAME public API the Java GT used
// (scalable/fixed/scale/withEyeHeight/makeBoundingBox), then diffs the recomputed
// record state + AABB corners against the recorded bits.
//
// Row layout (tab-separated):
//   DIM  <op> <floats...> <width8> <height8> <eyeHeight8> <fixed> <12 doubles attach>
//   BOX  <op> <a8> <b8> <c8> <x16> <y16> <z16> <min{x,y,z}16> <max{x,y,z}16>
//   BOXV <op> <a8> <b8> <c8> <x16> <y16> <z16> <min{x,y,z}16> <max{x,y,z}16>
//
// <op> selects the construction recipe (SCAL/FIX/SCAL_EYE/FIX_EYE/SCAL_S1/FIX_S1/
// SCAL_S2/FIX_S2/CHAIN/EYED/EYED_SCALED). For SCAL_S2/FIX_S2 the DIM input block is
// 4 floats (w,h,fw,fh); for every other DIM op it is 3 floats (a,b,c). BOX/BOXV
// rows always carry 3 input floats (a=w, b=h, c unused unless the op needs it).

#include "world/entity/EntityDimensions.h"
#include "world/phys/AABB.h"
#include "world/phys/Vec3.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::AABB;
using mc::EntityDimensions;
using mc::Vec3;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
double   bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
uint32_t fb(float v)  { return std::bit_cast<uint32_t>(v); }
uint64_t hx64(const std::string& s) { return std::stoull(s, nullptr, 16); }
uint32_t hx32(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }

// Rebuild the EntityDimensions described by op + its (already-parsed) input floats.
// inA/inB/inC are the first three input floats; inD is the optional fourth (for the
// two-factor scale ops). Returns false for an unknown op.
bool rebuild(const std::string& op, float inA, float inB, float inC, float inD,
             EntityDimensions& out) {
    if (op == "SCAL") {
        out = EntityDimensions::scalable(inA, inB);
    } else if (op == "FIX") {
        out = EntityDimensions::makeFixed(inA, inB);
    } else if (op == "SCAL_EYE") {
        out = EntityDimensions::scalable(inA, inB).withEyeHeight(inC);
    } else if (op == "FIX_EYE") {
        out = EntityDimensions::makeFixed(inA, inB).withEyeHeight(inC);
    } else if (op == "SCAL_S1") {
        out = EntityDimensions::scalable(inA, inB).scale(inC);
    } else if (op == "FIX_S1") {
        out = EntityDimensions::makeFixed(inA, inB).scale(inC);
    } else if (op == "SCAL_S2") {
        out = EntityDimensions::scalable(inA, inB).scale(inC, inD);
    } else if (op == "FIX_S2") {
        out = EntityDimensions::makeFixed(inA, inB).scale(inC, inD);
    } else if (op == "CHAIN") {
        out = EntityDimensions::scalable(1.0f, 2.0f).scale(2.0f).scale(0.5f, 3.0f);
    } else if (op == "EYED") {
        out = EntityDimensions::scalable(0.9f, 1.9f).withEyeHeight(1.62f);
    } else if (op == "EYED_SCALED") {
        out = EntityDimensions::scalable(0.9f, 1.9f).withEyeHeight(1.62f).scale(2.0f, 0.5f);
    } else {
        return false;
    }
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: entity_dimensions_verify_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.size() < 2) { fail("SHORT " + line); continue; }
        const std::string& tag = p[0];
        const std::string& op = p[1];
        ++total;

        // Two-factor scale DIM rows carry 4 input floats; all others carry 3.
        bool four = (tag == "DIM" && (op == "SCAL_S2" || op == "FIX_S2"));
        size_t inN = four ? 4 : 3;
        if (p.size() < 2 + inN) { fail("SHORT " + line); continue; }

        float inA = bf(p[2]);
        float inB = bf(p[3]);
        float inC = bf(p[4]);
        float inD = four ? bf(p[5]) : 0.0f;

        EntityDimensions d;
        if (!rebuild(op, inA, inB, inC, inD, d)) {
            fail("UNKNOWN_OP " + op);
            continue;
        }

        if (tag == "DIM") {
            // After the input block: width8 height8 eye8 fixed | 12 attach doubles.
            size_t off = 2 + inN;
            // need 4 state ints/floats + 12 doubles
            if (p.size() < off + 4 + 12) { fail("SHORT " + line); continue; }
            if (fb(d.width) != hx32(p[off + 0])) { fail(line); continue; }
            if (fb(d.height) != hx32(p[off + 1])) { fail(line); continue; }
            if (fb(d.eyeHeight) != hx32(p[off + 2])) { fail(line); continue; }
            if ((d.fixed ? 1 : 0) != std::stoi(p[off + 3])) { fail(line); continue; }
            // default attachment points, enum order PASSENGER,VEHICLE,NAME_TAG,WARDEN_CHEST
            size_t doff = off + 4;
            bool bad = false;
            for (int i = 0; i < 4 && !bad; ++i) {
                const Vec3& v = d.attachments.points[i];
                if (db(v.x) != hx64(p[doff + 0])) bad = true;
                if (db(v.y) != hx64(p[doff + 1])) bad = true;
                if (db(v.z) != hx64(p[doff + 2])) bad = true;
                doff += 3;
            }
            if (bad) fail(line);
        } else if (tag == "BOX" || tag == "BOXV") {
            // After 3 input floats: x16 y16 z16 min{x,y,z}16 max{x,y,z}16.
            size_t off = 2 + 3;
            if (p.size() < off + 9) { fail("SHORT " + line); continue; }
            double x = bd(p[off + 0]), y = bd(p[off + 1]), z = bd(p[off + 2]);
            AABB b = (tag == "BOX") ? d.makeBoundingBox(x, y, z)
                                    : d.makeBoundingBox(Vec3(x, y, z));
            if (db(b.minCorner.x) != hx64(p[off + 3]) ||
                db(b.minCorner.y) != hx64(p[off + 4]) ||
                db(b.minCorner.z) != hx64(p[off + 5]) ||
                db(b.maxCorner.x) != hx64(p[off + 6]) ||
                db(b.maxCorner.y) != hx64(p[off + 7]) ||
                db(b.maxCorner.z) != hx64(p[off + 8])) {
                fail(line);
            }
        } else {
            fail("UNKNOWN_TAG " + tag);
        }
    }

    std::cout << "EntityDimensionsVerify cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
