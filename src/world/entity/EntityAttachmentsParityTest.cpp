// Parity test for net.minecraft.world.entity.EntityAttachments (26.1.2).
// Ground truth: tools/EntityAttachmentsParity.java. Bit-exact: every double is
// compared by its raw 64-bit IEEE pattern (never by value) via std::bit_cast.
//
//   entity_attachments_parity --cases mcpp/build/entity_attachments.tsv
//
// Each TSV row carries a <cfg> key naming the EntityAttachments the GT built; the
// harness rebuilds the identical object with the C++ port and re-runs the same
// accessor (getClamped / get / getNullable / getAverage), diffing the recorded bits.

#include "world/entity/EntityAttachmentsFull.h"
#include "world/phys/Vec3.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using mc::Vec3;
using mc::entity_attachments::EntityAttachment;
using mc::entity_attachments::EntityAttachments;

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
uint64_t hx64(const std::string& s) { return std::stoull(s, nullptr, 16); }

// Build the EntityAttachments named by <cfg>, identical to the GT.
// Returns false if the key is not understood.
bool buildConfig(const std::string& cfg, EntityAttachments& out) {
    // def_<wbits>_<hbits>
    if (cfg.rfind("def_", 0) == 0 && cfg != "def_scale") {
        std::string r = cfg.substr(4);
        auto u = r.find('_');
        if (u == std::string::npos) return false;
        float w = bf(r.substr(0, u));
        float h = bf(r.substr(u + 1));
        out = EntityAttachments::createDefault(w, h);
        return true;
    }
    if (cfg == "b_pass3") {
        out = EntityAttachments::builder()
                  .attach(EntityAttachment::PASSENGER, 0.0f, 1.0f, 0.0f)
                  .attach(EntityAttachment::PASSENGER, 0.5f, 1.2f, -0.25f)
                  .attach(EntityAttachment::PASSENGER, -0.5f, 0.8f, 0.25f)
                  .build(0.6f, 1.8f);
        return true;
    }
    if (cfg == "b_all") {
        out = EntityAttachments::builder()
                  .attach(EntityAttachment::PASSENGER, 0.1f, 0.2f, 0.3f)
                  .attach(EntityAttachment::PASSENGER, 1.1f, 1.2f, 1.3f)
                  .attach(EntityAttachment::VEHICLE, -1.0f, 0.0f, 2.0f)
                  .attach(EntityAttachment::NAME_TAG, 0.0f, 2.5f, 0.0f)
                  .attach(EntityAttachment::NAME_TAG, 0.0f, 2.7f, 0.0f)
                  .attach(EntityAttachment::NAME_TAG, 0.0f, 2.9f, 0.0f)
                  .attach(EntityAttachment::NAME_TAG, 0.0f, 3.1f, 0.0f)
                  .attach(EntityAttachment::WARDEN_CHEST, 3.3f, -4.4f, 5.5f)
                  .build(1.0f, 2.0f);
        return true;
    }
    if (cfg == "b_warden1") {
        out = EntityAttachments::builder()
                  .attach(EntityAttachment::WARDEN_CHEST, Vec3(7.0, 8.0, 9.0))
                  .build(0.9f, 1.9f);
        return true;
    }
    if (cfg.rfind("b_scale", 0) == 0) {
        // base config used by GT for the scale battery.
        EntityAttachments base = EntityAttachments::builder()
                                     .attach(EntityAttachment::PASSENGER, 0.1f, 0.2f, 0.3f)
                                     .attach(EntityAttachment::PASSENGER, 1.1f, 1.2f, 1.3f)
                                     .attach(EntityAttachment::VEHICLE, -1.0f, 0.5f, 2.0f)
                                     .attach(EntityAttachment::WARDEN_CHEST, 3.3f, -4.4f, 5.5f)
                                     .build(1.0f, 2.0f);
        int si = std::stoi(cfg.substr(std::string("b_scale").size()));
        static const float scales[][3] = {
            {1.0f, 1.0f, 1.0f}, {2.0f, 0.5f, 3.0f}, {0.0f, 0.0f, 0.0f},
            {1.5f, 1.5f, 1.5f}, {-1.0f, 2.0f, -0.5f}, {0.3f, 0.7f, 1.1f}
        };
        if (si < 0 || si >= 6) return false;
        out = base.scale(scales[si][0], scales[si][1], scales[si][2]);
        return true;
    }
    if (cfg == "def_scale") {
        out = EntityAttachments::createDefault(0.6f, 1.8f).scale(2.0f, 0.5f, 2.0f);
        return true;
    }
    return false;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: entity_attachments_parity --cases <tsv>\n";
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

    // Cache rebuilt configs.
    std::map<std::string, EntityAttachments> cache;
    auto get = [&](const std::string& cfg, EntityAttachments*& p) -> bool {
        auto it = cache.find(cfg);
        if (it != cache.end()) { p = &it->second; return true; }
        EntityAttachments built;
        if (!buildConfig(cfg, built)) return false;
        auto ins = cache.emplace(cfg, std::move(built));
        p = &ins.first->second;
        return true;
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.size() < 3) { fail("SHORT " + line); continue; }
        const std::string& tag = p[0];
        const std::string& cfg = p[1];
        ++total;

        EntityAttachments* at = nullptr;
        if (!get(cfg, at)) { fail("UNRESOLVED_CFG " + cfg); continue; }

        int ao = std::stoi(p[2]);
        EntityAttachment a = static_cast<EntityAttachment>(ao);

        if (tag == "POINT") {
            // POINT cfg ao count [x y z]*
            int count = std::stoi(p[3]);
            // Verify our count matches (probe getNullable up to count).
            Vec3 tmp;
            int ourCount = 0;
            while (at->getNullable(a, ourCount, 0.0f, tmp)) {
                ++ourCount;
                if (ourCount > 1000) break;
            }
            if (ourCount != count) { fail(line); continue; }
            int off = 4;
            bool bad = false;
            for (int i = 0; i < count && !bad; ++i) {
                Vec3 v = at->getClamped(a, i, 0.0f);
                if (db(v.x) != hx64(p[off + 0]) ||
                    db(v.y) != hx64(p[off + 1]) ||
                    db(v.z) != hx64(p[off + 2])) bad = true;
                off += 3;
            }
            if (bad) fail(line);
        } else if (tag == "AVG") {
            // AVG cfg ao x y z
            Vec3 avg = at->getAverage(a);
            if (db(avg.x) != hx64(p[3]) || db(avg.y) != hx64(p[4]) || db(avg.z) != hx64(p[5]))
                fail(line);
        } else if (tag == "CLAMPED") {
            // CLAMPED cfg ao index rotY x y z
            int idx = std::stoi(p[3]);
            float rotY = bf(p[4]);
            Vec3 c = at->getClamped(a, idx, rotY);
            if (db(c.x) != hx64(p[5]) || db(c.y) != hx64(p[6]) || db(c.z) != hx64(p[7]))
                fail(line);
        } else if (tag == "GET") {
            // GET cfg ao index rotY x y z
            int idx = std::stoi(p[3]);
            float rotY = bf(p[4]);
            Vec3 g;
            try {
                g = at->get(a, idx, rotY);
            } catch (...) {
                fail("UNEXPECTED_THROW " + line);
                continue;
            }
            if (db(g.x) != hx64(p[5]) || db(g.y) != hx64(p[6]) || db(g.z) != hx64(p[7]))
                fail(line);
        } else if (tag == "NULL") {
            // NULL cfg ao index rotY isnull [x y z]
            int idx = std::stoi(p[3]);
            float rotY = bf(p[4]);
            int isNull = std::stoi(p[5]);
            Vec3 out;
            bool present = at->getNullable(a, idx, rotY, out);
            int ourNull = present ? 0 : 1;
            if (ourNull != isNull) { fail(line); continue; }
            if (!present) {
                // GT emits no coords; nothing else to check.
                if (p.size() != 6) fail("NULL_EXTRA " + line);
            } else {
                if (p.size() < 9) { fail("NULL_MISSING " + line); continue; }
                if (db(out.x) != hx64(p[6]) || db(out.y) != hx64(p[7]) || db(out.z) != hx64(p[8]))
                    fail(line);
            }
        } else {
            fail("UNKNOWN_TAG " + tag);
        }
    }

    std::cout << "EntityAttachments cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
