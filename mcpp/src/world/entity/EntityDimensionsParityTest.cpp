// Parity test for net.minecraft.world.entity.EntityDimensions (26.1.2).
// Ground truth: tools/EntityDimensionsParity.java. Bit-exact: every float is
// compared by its raw 32-bit IEEE pattern and every double by its raw 64-bit
// pattern (never by value), so -0.0 / Inf / subnormals / NaN all check faithfully.
//
//   entity_dimensions_parity --cases mcpp/build/entity_dimensions.tsv
//
// The harness REPLAYS the same scalable/fixed/scale/withEyeHeight/makeBoundingBox
// calls the GT performed, keyed by the row name, and diffs the recomputed record
// state + AABB corners against the recorded bits.

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
using mc::EntityAttachment;
using mc::EntityDimensions;
using mc::Vec3;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
double   bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
uint32_t fb(float v)  { return std::bit_cast<uint32_t>(v); }
uint64_t hx64(const std::string& s) { return std::stoull(s, nullptr, 16); }
uint32_t hx32(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }

// Parse the float-bits suffix embedded in a row name (the part after the final
// recognizable token). We instead reconstruct the EntityDimensions by replaying
// the GT's calls keyed off the name prefix; the trailing tokens are the
// raw-int-bits float hex (Integer.toHexString of floatToRawIntBits), so:
float nameFloat(const std::string& hex) {
    return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(hex, nullptr, 16)));
}
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: entity_dimensions_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    // Rebuild the EntityDimensions that the GT row refers to, purely from its name.
    // Returns false if the name is not understood.
    auto rebuild = [&](const std::string& name, EntityDimensions& out) -> bool {
        // name encodes float widths/heights/scales as Integer.toHexString of the
        // raw float bits, separated by 'x' or '_'. Split off the leading keyword.
        auto us = name.find('_');
        std::string key = (us == std::string::npos) ? name : name.substr(0, us);
        std::string rest = (us == std::string::npos) ? "" : name.substr(us + 1);

        auto splitTok = [](const std::string& s, char d) {
            std::vector<std::string> v; std::string it; std::istringstream ss(s);
            while (std::getline(ss, it, d)) v.push_back(it);
            return v;
        };

        if (key == "scalable" || key == "fixed") {
            // rest = "<wbits>x<hbits>"
            auto p = splitTok(rest, 'x');
            if (p.size() != 2) return false;
            float w = nameFloat(p[0]), h = nameFloat(p[1]);
            out = (key == "scalable") ? EntityDimensions::scalable(w, h)
                                      : EntityDimensions::makeFixed(w, h);
            return true;
        }
        if (key == "scalable" ) return false;
        // names like "scalable_eye_<w>x<h>" -> rest = "eye_<w>x<h>"
        if (rest.rfind("eye_", 0) == 0) {
            std::string r2 = rest.substr(4);
            auto p = splitTok(r2, 'x');
            if (p.size() != 2) return false;
            float w = nameFloat(p[0]), h = nameFloat(p[1]);
            EntityDimensions base = (key == "scalable") ? EntityDimensions::scalable(w, h)
                                                        : EntityDimensions::makeFixed(w, h);
            // GT used 0.123456f for scalable_eye, h for fixed_eye
            if (key == "scalable") out = base.withEyeHeight(0.123456f);
            else out = base.withEyeHeight(h);
            return true;
        }
        return false;
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        const std::string& name = p[1];
        ++total;

        // ---- Construct the EntityDimensions for this row by name keyword. ----
        EntityDimensions d;
        bool ok = false;

        if (name.rfind("scalable_eye_", 0) == 0) {
            std::string r = name.substr(std::string("scalable_eye_").size());
            auto x = r.find('x');
            if (x != std::string::npos) {
                float w = nameFloat(r.substr(0, x)), h = nameFloat(r.substr(x + 1));
                d = EntityDimensions::scalable(w, h).withEyeHeight(0.123456f); ok = true;
            }
        } else if (name.rfind("fixed_eye_", 0) == 0) {
            std::string r = name.substr(std::string("fixed_eye_").size());
            auto x = r.find('x');
            if (x != std::string::npos) {
                float w = nameFloat(r.substr(0, x)), h = nameFloat(r.substr(x + 1));
                d = EntityDimensions::makeFixed(w, h).withEyeHeight(h); ok = true;
            }
        } else if (name.rfind("scalable_", 0) == 0 && name.rfind("scalable_eye_", 0) != 0) {
            std::string r = name.substr(std::string("scalable_").size());
            auto x = r.find('x');
            if (x != std::string::npos) {
                float w = nameFloat(r.substr(0, x)), h = nameFloat(r.substr(x + 1));
                d = EntityDimensions::scalable(w, h); ok = true;
            }
        } else if (name.rfind("fixed_", 0) == 0 && name.rfind("fixed_eye_", 0) != 0) {
            std::string r = name.substr(std::string("fixed_").size());
            auto x = r.find('x');
            if (x != std::string::npos) {
                float w = nameFloat(r.substr(0, x)), h = nameFloat(r.substr(x + 1));
                d = EntityDimensions::makeFixed(w, h); ok = true;
            }
        } else if (name.rfind("box_", 0) == 0) {
            std::string r = name.substr(std::string("box_").size());
            auto x = r.find('x');
            float w = nameFloat(r.substr(0, x)), h = nameFloat(r.substr(x + 1));
            d = EntityDimensions::scalable(w, h); ok = true;
        } else if (name.rfind("boxF_", 0) == 0) {
            std::string r = name.substr(std::string("boxF_").size());
            auto x = r.find('x');
            float w = nameFloat(r.substr(0, x)), h = nameFloat(r.substr(x + 1));
            d = EntityDimensions::makeFixed(w, h); ok = true;
        } else if (name.rfind("scale1F_", 0) == 0) {
            float s = nameFloat(name.substr(std::string("scale1F_").size()));
            d = EntityDimensions::makeFixed(0.6f, 1.8f).scale(s); ok = true;
        } else if (name.rfind("scale1_", 0) == 0) {
            float s = nameFloat(name.substr(std::string("scale1_").size()));
            d = EntityDimensions::scalable(0.6f, 1.8f).scale(s); ok = true;
        } else if (name.rfind("scale2F_", 0) == 0) {
            std::string r = name.substr(std::string("scale2F_").size());
            auto u = r.find('_');
            float a = nameFloat(r.substr(0, u)), b = nameFloat(r.substr(u + 1));
            d = EntityDimensions::makeFixed(0.6f, 1.8f).scale(a, b); ok = true;
        } else if (name.rfind("scale2_", 0) == 0) {
            std::string r = name.substr(std::string("scale2_").size());
            auto u = r.find('_');
            float a = nameFloat(r.substr(0, u)), b = nameFloat(r.substr(u + 1));
            d = EntityDimensions::scalable(0.6f, 1.8f).scale(a, b); ok = true;
        } else if (name == "chain" || name == "chainBox") {
            d = EntityDimensions::scalable(1.0f, 2.0f).scale(2.0f).scale(0.5f, 3.0f); ok = true;
        } else if (name == "eyed") {
            d = EntityDimensions::scalable(0.9f, 1.9f).withEyeHeight(1.62f); ok = true;
        } else if (name == "eyed_scaled") {
            d = EntityDimensions::scalable(0.9f, 1.9f).withEyeHeight(1.62f).scale(2.0f, 0.5f); ok = true;
        }

        if (!ok) { fail("UNRESOLVED_NAME " + name); continue; }
        (void)rebuild;

        if (t == "DIM") {
            // p: DIM name w8 h8 eye8 fixed | (3 doubles)*4 = 12 doubles
            if (fb(d.width) != hx32(p[2])) { fail(line); continue; }
            if (fb(d.height) != hx32(p[3])) { fail(line); continue; }
            if (fb(d.eyeHeight) != hx32(p[4])) { fail(line); continue; }
            if ((d.fixed ? 1 : 0) != std::stoi(p[5])) { fail(line); continue; }
            // default attachment points, enum order PASSENGER,VEHICLE,NAME_TAG,WARDEN_CHEST
            int off = 6;
            bool bad = false;
            for (int i = 0; i < 4 && !bad; ++i) {
                const Vec3& v = d.attachments.points[i];
                if (db(v.x) != hx64(p[off + 0])) bad = true;
                if (db(v.y) != hx64(p[off + 1])) bad = true;
                if (db(v.z) != hx64(p[off + 2])) bad = true;
                off += 3;
            }
            if (bad) fail(line);
        } else if (t == "BOX" || t == "BOXV") {
            // p: TAG name x16 y16 z16 minx miny minz maxx maxy maxz
            double x = bd(p[2]), y = bd(p[3]), z = bd(p[4]);
            AABB b = (t == "BOX") ? d.makeBoundingBox(x, y, z)
                                  : d.makeBoundingBox(Vec3(x, y, z));
            if (db(b.minCorner.x) != hx64(p[5]) || db(b.minCorner.y) != hx64(p[6]) ||
                db(b.minCorner.z) != hx64(p[7]) || db(b.maxCorner.x) != hx64(p[8]) ||
                db(b.maxCorner.y) != hx64(p[9]) || db(b.maxCorner.z) != hx64(p[10])) {
                fail(line);
            }
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "EntityDimensions cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
