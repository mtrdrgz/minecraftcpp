// WorldBorderParityTest — bit-for-bit gate for WorldBorder.h vs the REAL
// net.minecraft.world.level.border.WorldBorder pure box/lerp math (ground truth:
// tools/WorldBorderParity.java, which drives the actual class through its public
// API and reads back the getters).
//
// The Java GT configures a real WorldBorder (setAbsoluteMaxSize, setCenter, then
// either setSize for STATIC or lerpSizeBetween + N tick() for MOVING) and emits the
// 14-field read-back payload. This test reconstructs the identical extent state from
// the row inputs (StaticBorderExtent / MovingBorderExtent in WorldBorder.h, replaying
// the same number of update() calls), recomputes every getter, and bit-compares.
//
// floats = %08x raw bits, doubles = %016x raw bits, longs = decimal, ints = decimal,
// status = enum name. Every float/double comparison is std::bit_cast (never by value).
//   world_border_parity --cases world_border.tsv

#include "WorldBorder.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace wb = mc::world::level::border;

static double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
static float  bf(const std::string& s) { return std::bit_cast<float>((uint32_t)std::stoul(s, nullptr, 16)); }
static uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }

static const char* statusName(wb::BorderStatus s) {
    switch (s) {
        case wb::BorderStatus::GROWING:    return "GROWING";
        case wb::BorderStatus::SHRINKING:  return "SHRINKING";
        case wb::BorderStatus::STATIONARY: return "STATIONARY";
    }
    return "?";
}

// Compare one double field (hex-bits expected vs computed). Returns true on match.
static bool cmpD(const char* tag, const char* field, const std::string& wantHex, double got,
                 long& mismatches) {
    uint64_t want = std::stoull(wantHex, nullptr, 16);
    if (db(got) != want) {
        mismatches++;
        std::fprintf(stderr, "%s %s mismatch want=%016llx got=%016llx\n",
                     tag, field, (unsigned long long)want, (unsigned long long)db(got));
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    const char* casesPath = nullptr;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (!casesPath) { std::fprintf(stderr, "usage: world_border_parity --cases <tsv>\n"); return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", casesPath); return 2; }

    long cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        std::vector<std::string> f;
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, '\t')) f.push_back(tok);
        if (f.empty()) continue;
        const std::string& tag = f[0];

        // payload field layout (shared) — offset within the 14-field tail:
        //   0 minX 1 maxX 2 minZ 3 maxZ 4 size 5 lerpSpeed 6 lerpTime(long)
        //   7 lerpTarget 8 statusName 9 dist 10 within(0/1) 11 clampX 12 clampZ
        // We compute minX/maxX/minZ/maxZ from the extent, then the top-level helpers.
        if (tag == "STATIC") {
            // STATIC cx cz absMax size delta x z margin <payload...>
            double cx = bd(f[1]);
            double cz = bd(f[2]);
            int absMax = std::stoi(f[3]);
            double size = bd(f[4]);
            // delta is ignored by StaticBorderExtent getters, but parsed for parity.
            float delta = bf(f[5]); (void)delta;
            double px = bd(f[6]);
            double pz = bd(f[7]);
            double margin = bd(f[8]);
            const std::string* p = &f[9]; // payload[0..12]

            wb::StaticBorderExtent ext(cx, cz, absMax, size);
            double minX = ext.getMinX(), maxX = ext.getMaxX();
            double minZ = ext.getMinZ(), maxZ = ext.getMaxZ();
            double dist = wb::getDistanceToBorder(px, pz, minX, maxX, minZ, maxZ);
            bool within = wb::isWithinBounds(px, pz, margin, minX, maxX, minZ, maxZ);
            double clampX = wb::clampVec3ToBoundX(px, minX, maxX);
            double clampZ = wb::clampVec3ToBoundZ(pz, minZ, maxZ);

            cases++;
            cmpD(tag.c_str(), "minX", p[0], minX, mismatches);
            cmpD(tag.c_str(), "maxX", p[1], maxX, mismatches);
            cmpD(tag.c_str(), "minZ", p[2], minZ, mismatches);
            cmpD(tag.c_str(), "maxZ", p[3], maxZ, mismatches);
            cmpD(tag.c_str(), "size", p[4], ext.getSize(), mismatches);
            cmpD(tag.c_str(), "lerpSpeed", p[5], ext.getLerpSpeed(), mismatches);
            if ((int64_t)std::stoll(p[6]) != ext.getLerpTime()) {
                mismatches++;
                std::fprintf(stderr, "%s lerpTime mismatch want=%s got=%lld\n",
                             tag.c_str(), p[6].c_str(), (long long)ext.getLerpTime());
            }
            cmpD(tag.c_str(), "lerpTarget", p[7], ext.getLerpTarget(), mismatches);
            if (p[8] != statusName(ext.getStatus())) {
                mismatches++;
                std::fprintf(stderr, "%s status mismatch want=%s got=%s\n",
                             tag.c_str(), p[8].c_str(), statusName(ext.getStatus()));
            }
            cmpD(tag.c_str(), "dist", p[9], dist, mismatches);
            if ((p[10] == "1") != within) {
                mismatches++;
                std::fprintf(stderr, "%s within mismatch want=%s got=%d\n",
                             tag.c_str(), p[10].c_str(), within ? 1 : 0);
            }
            cmpD(tag.c_str(), "clampX", p[11], clampX, mismatches);
            cmpD(tag.c_str(), "clampZ", p[12], clampZ, mismatches);
        } else if (tag == "MOVING") {
            // MOVING cx cz absMax from to duration gameTime ticks delta x z margin <payload...>
            double cx = bd(f[1]);
            double cz = bd(f[2]);
            int absMax = std::stoi(f[3]);
            double from = bd(f[4]);
            double to = bd(f[5]);
            int64_t duration = (int64_t)std::stoll(f[6]);
            int64_t gameTime = (int64_t)std::stoll(f[7]);
            int ticks = std::stoi(f[8]);
            float delta = bf(f[9]);
            double px = bd(f[10]);
            double pz = bd(f[11]);
            double margin = bd(f[12]);
            const std::string* p = &f[13]; // payload[0..12]

            wb::MovingBorderExtent ext(cx, cz, absMax, from, to, duration, gameTime);
            for (int i = 0; i < ticks; i++) ext.update();
            // The reported edge fields use the probe's partial-tick delta (the
            // renderer reads getMinX(delta)); the top-level WorldBorder helpers
            // (getDistanceToBorder/isWithinBounds/clampVec3ToBound) instead call
            // this.getMinX() == getMinX(0.0F), so they use the delta-0 edges.
            double minX = ext.getMinX(delta), maxX = ext.getMaxX(delta);
            double minZ = ext.getMinZ(delta), maxZ = ext.getMaxZ(delta);
            double minX0 = ext.getMinX(0.0F), maxX0 = ext.getMaxX(0.0F);
            double minZ0 = ext.getMinZ(0.0F), maxZ0 = ext.getMaxZ(0.0F);
            double dist = wb::getDistanceToBorder(px, pz, minX0, maxX0, minZ0, maxZ0);
            bool within = wb::isWithinBounds(px, pz, margin, minX0, maxX0, minZ0, maxZ0);
            double clampX = wb::clampVec3ToBoundX(px, minX0, maxX0);
            double clampZ = wb::clampVec3ToBoundZ(pz, minZ0, maxZ0);

            cases++;
            cmpD(tag.c_str(), "minX", p[0], minX, mismatches);
            cmpD(tag.c_str(), "maxX", p[1], maxX, mismatches);
            cmpD(tag.c_str(), "minZ", p[2], minZ, mismatches);
            cmpD(tag.c_str(), "maxZ", p[3], maxZ, mismatches);
            cmpD(tag.c_str(), "size", p[4], ext.getSize(), mismatches);
            cmpD(tag.c_str(), "lerpSpeed", p[5], ext.getLerpSpeed(), mismatches);
            if ((int64_t)std::stoll(p[6]) != ext.getLerpTime()) {
                mismatches++;
                std::fprintf(stderr, "%s lerpTime mismatch want=%s got=%lld\n",
                             tag.c_str(), p[6].c_str(), (long long)ext.getLerpTime());
            }
            cmpD(tag.c_str(), "lerpTarget", p[7], ext.getLerpTarget(), mismatches);
            if (p[8] != statusName(ext.getStatus())) {
                mismatches++;
                std::fprintf(stderr, "%s status mismatch want=%s got=%s\n",
                             tag.c_str(), p[8].c_str(), statusName(ext.getStatus()));
            }
            cmpD(tag.c_str(), "dist", p[9], dist, mismatches);
            if ((p[10] == "1") != within) {
                mismatches++;
                std::fprintf(stderr, "%s within mismatch want=%s got=%d\n",
                             tag.c_str(), p[10].c_str(), within ? 1 : 0);
            }
            cmpD(tag.c_str(), "clampX", p[11], clampX, mismatches);
            cmpD(tag.c_str(), "clampZ", p[12], clampZ, mismatches);
        }
        // unknown tags ignored
    }

    std::printf("WorldBorder checks=%ld mismatches=%ld\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
