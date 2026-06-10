// Parity test for net.minecraft.world.entity.InterpolationHandler (26.1.2).
// Ground truth: tools/InterpolationHandlerParity.java.
//
// Bit-exact: doubles/floats compared as raw IEEE-754 bits via std::bit_cast.
//   interpolation_handler_parity --cases mcpp/build/interpolation_handler.tsv
//
// TAGs (see the Java tool header for the full column layout):
//   DATA   — InterpolationData.addDelta + addRotation + decrease
//   STEP   — one interpolate() tick (interpolateStep)
//   REPLAY — a full NT-tick interpolation sequence (interpolateStep replayed)

#include "InterpolationHandler.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::InterpolateStepResult;
using mc::InterpolationData;
using mc::Vec2;
using mc::Vec3;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
float  bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: interpolation_handler_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << l << "\n"; };
    auto eD = [&](double got, const std::string& exp, const std::string& l) {
        if (db(got) != std::stoull(exp, nullptr, 16)) fail(l);
    };
    auto eF = [&](float got, const std::string& exp, const std::string& l) {
        if (fb(got) != static_cast<uint32_t>(std::stoul(exp, nullptr, 16))) fail(l);
    };
    auto eI = [&](int got, const std::string& exp, const std::string& l) {
        if (got != std::stoi(exp)) fail(l);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "DATA") {
            int i = 1;
            int steps0 = std::stoi(p[i]); ++i;
            Vec3 pos0(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
            float yRot0 = bf(p[i]); ++i;
            float xRot0 = bf(p[i]); ++i;
            Vec3 delta(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
            float addY = bf(p[i]); ++i;
            float addX = bf(p[i]); ++i;

            InterpolationData data(steps0, pos0, yRot0, xRot0);
            data.addDelta(delta);
            data.addRotation(addY, addX);
            data.decrease();

            eI(data.steps, p[i], line); ++i;
            eD(data.position.x, p[i], line); eD(data.position.y, p[i + 1], line); eD(data.position.z, p[i + 2], line); i += 3;
            eF(data.yRot, p[i], line); ++i;
            eF(data.xRot, p[i], line); ++i;
        } else if (t == "STEP") {
            int i = 1;
            int steps = std::stoi(p[i]); ++i;
            Vec3 tgt(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
            float tgtY = bf(p[i]); ++i;
            float tgtX = bf(p[i]); ++i;
            Vec3 ePos(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
            float eY = bf(p[i]); ++i;
            float eX = bf(p[i]); ++i;
            bool hasPrevPos = std::stoi(p[i]) != 0; ++i;
            Vec3 prevPos(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
            bool hasPrevRot = std::stoi(p[i]) != 0; ++i;
            float prevRotX = bf(p[i]); ++i;   // Vec2(xRot, yRot)
            float prevRotY = bf(p[i]); ++i;
            bool noColl = std::stoi(p[i]) != 0; ++i;

            InterpolationData data(steps, tgt, tgtY, tgtX);
            InterpolateStepResult r = mc::interpolateStep(
                data, ePos, eY, eX,
                hasPrevPos, prevPos,
                hasPrevRot, Vec2(prevRotX, prevRotY),
                noColl);

            // outputs
            eD(r.newPosition.x, p[i], line); eD(r.newPosition.y, p[i + 1], line); eD(r.newPosition.z, p[i + 2], line); i += 3;
            eF(r.newYRot, p[i], line); ++i;
            eF(r.newXRot, p[i], line); ++i;
            eD(r.data.position.x, p[i], line); eD(r.data.position.y, p[i + 1], line); eD(r.data.position.z, p[i + 2], line); i += 3;
            eF(r.data.yRot, p[i], line); ++i;
            eF(r.data.xRot, p[i], line); ++i;
            eI(r.data.steps, p[i], line); ++i;
            eD(r.previousTickPosition.x, p[i], line); eD(r.previousTickPosition.y, p[i + 1], line); eD(r.previousTickPosition.z, p[i + 2], line); i += 3;
            eF(r.previousTickRot.x, p[i], line); ++i;   // prevTickRotX (xRot)
            eF(r.previousTickRot.y, p[i], line); ++i;   // prevTickRotY (yRot)
        } else if (t == "REPLAY") {
            int i = 1;
            int NT = std::stoi(p[i]); ++i;
            int stepsInit = std::stoi(p[i]); ++i;
            Vec3 tgt(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
            float tgtY = bf(p[i]); ++i;
            float tgtX = bf(p[i]); ++i;
            Vec3 startPos(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
            float startY = bf(p[i]); ++i;
            float startX = bf(p[i]); ++i;

            // read the NT-tick script: move(3 double), yaw, pitch (float), coll(int)
            struct Tick { Vec3 move; float yaw; float pitch; bool coll; };
            std::vector<Tick> ticks;
            ticks.reserve(NT);
            for (int t2 = 0; t2 < NT; ++t2) {
                Vec3 move(bd(p[i]), bd(p[i + 1]), bd(p[i + 2])); i += 3;
                float yaw = bf(p[i]); ++i;
                float pitch = bf(p[i]); ++i;
                bool coll = std::stoi(p[i]) != 0; ++i;
                ticks.push_back({move, yaw, pitch, coll});
            }

            // replay
            InterpolationData data(stepsInit, tgt, tgtY, tgtX);
            bool hasPrevPos = true, hasPrevRot = true;
            Vec3 prevPos = startPos;
            Vec2 prevRot(startX, startY);   // Vec2(xRot, yRot)
            Vec3 cur = startPos;

            for (int t2 = 0; t2 < NT; ++t2) {
                const Tick& tk = ticks[t2];
                cur = cur.add(tk.move);
                float eYRot = tk.yaw;
                float eXRot = tk.pitch;

                if (data.steps <= 0) {
                    // interpolation finished — entity pose unchanged.
                    eD(cur.x, p[i], line); eD(cur.y, p[i + 1], line); eD(cur.z, p[i + 2], line); i += 3;
                    eF(eYRot, p[i], line); ++i;
                    eF(eXRot, p[i], line); ++i;
                    continue;
                }

                InterpolateStepResult r = mc::interpolateStep(
                    data, cur, eYRot, eXRot,
                    hasPrevPos, prevPos,
                    hasPrevRot, prevRot,
                    tk.coll);

                cur = r.newPosition;
                prevPos = r.previousTickPosition;
                prevRot = r.previousTickRot;
                data = r.data;

                eD(r.newPosition.x, p[i], line); eD(r.newPosition.y, p[i + 1], line); eD(r.newPosition.z, p[i + 2], line); i += 3;
                eF(r.newYRot, p[i], line); ++i;
                eF(r.newXRot, p[i], line); ++i;
            }
        } else {
            std::cerr << "unknown TAG " << t << "\n";
            return 2;
        }
    }

    std::cout << "InterpolationHandler cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
