// Bit-exact parity gate for com.mojang.blaze3d.vertex.PoseStack (Minecraft 26.1.2).
// Replays the exact op sequences emitted by tools/PoseStackParity.java and compares
// the top Pose's 16 pose + 9 normal floats (and trustedNormals) bit-for-bit against
// the REAL PoseStack/org.joml ground truth. Verifies render/PoseStack.h (and the
// PoseStack-specific methods added to render/model/Joml.h).
//
//   pose_stack_parity --cases mcpp/build/pose_stack.tsv

#include "PoseStack.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace r = mc::render;
namespace j = mc::render::model::joml;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
uint32_t hx(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }

// Same exact-float tables as the GT tool.
const float QS[10][4] = {
    {0,0,0,1}, {0.5f,0.5f,0.5f,0.5f}, {0.5f,0,0,0.5f}, {0,0.25f,0,0.75f},
    {-0.5f,0.5f,-0.5f,0.5f}, {0.125f,0.375f,-0.625f,0.75f}, {1,0,0,0},
    {0,1,0,0}, {0,0,1,0}, {-0.5f,-0.5f,0.5f,0.5f}
};
const float TS[5][3] = { {1,2,3}, {-5,0.5f,7}, {0.25f,-0.75f,1.5f}, {0,0,0}, {-1.5f,2.5f,-3.5f} };
const float SS[10][3] = { {2,3,4}, {-1,0.5f,2}, {0.25f,0.25f,0.25f}, {-2,-2,-2}, {2,2,2},
                          {-1,-1,-1}, {1,1,1}, {0.5f,-0.5f,0.5f}, {-3,3,3}, {-2,2,-2} };
constexpr int NQ = 10, NT = 5, NS = 10;

j::Quaternionf q(int i) { j::Quaternionf x; x.set(QS[i][0], QS[i][1], QS[i][2], QS[i][3]); return x; }

long long g_total = 0, g_mism = 0; int g_shown = 0;

// Compare the live PoseStack top against one expected TSV row. Row layout after the
// TAG/seq/step prefix (3 cols): 16 pose + 9 normal + 1 trustedNormals = 26 cols at o.
void check(r::PoseStack& ps, const std::vector<std::string>& p, const std::string& line) {
    ++g_total;
    r::Pose& pose = ps.last();
    const j::Matrix4f& m = pose.pose;
    const j::Matrix3f& n = pose.normal;
    const float gotP[16] = { m.m00,m.m01,m.m02,m.m03, m.m10,m.m11,m.m12,m.m13,
                             m.m20,m.m21,m.m22,m.m23, m.m30,m.m31,m.m32,m.m33 };
    const float gotN[9]  = { n.m00,n.m01,n.m02, n.m10,n.m11,n.m12, n.m20,n.m21,n.m22 };
    int o = 3; // TAG, seq, step
    bool ok = true;
    for (int k = 0; k < 16 && ok; ++k) if (fb(gotP[k]) != hx(p[o + k])) ok = false;
    for (int k = 0; k < 9 && ok; ++k)  if (fb(gotN[k]) != hx(p[o + 16 + k])) ok = false;
    int gotTrusted = pose.trustedNormals ? 1 : 0;
    if (ok && gotTrusted != std::stoi(p[o + 25])) ok = false;
    if (!ok) {
        ++g_mism;
        if (g_shown++ < 40) std::cerr << "MISMATCH " << line << "\n";
    }
}

// Build the argument Matrix4f for the mulPose(Matrix4f) batteries (mirrors the GT).
j::Matrix4f matTranslation(float x, float y, float z) { j::Matrix4f m; m.translation(x, y, z); return m; }
j::Matrix4f matRotation(int i) { j::Matrix4f m; m.rotation(q(i)); return m; }
j::Matrix4f matScaling(float x, float y, float z) { j::Matrix4f m; m.scaling(x, y, z); return m; }
j::Matrix4f matRotScale(int i, float sx, float sy, float sz) { j::Matrix4f m; m.rotation(q(i)); m.scale(sx, sy, sz); return m; }
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: pose_stack_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    // Read all rows, group by (tag, seq); each group is one op sequence whose steps
    // we replay deterministically while validating against the recorded rows.
    struct Row { std::string line; std::vector<std::string> p; };
    std::vector<Row> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.size() < 3) continue;
        rows.push_back({line, std::move(p)});
    }

    // Battery seq bases (must match the GT tool's single global `seq` counter order).
    const int B1 = 0;                       // ROTID : NQ seqs
    const int B2 = B1 + NQ;                 // TRROT : NQ*NT
    const int B3 = B2 + NQ * NT;            // SCROT : NQ*NS
    const int B4 = B3 + NQ * NS;            // SCALE : NS
    const int B5 = B4 + NS;                 // CHAIN : NQ
    const int B6 = B5 + NQ;                 // PUSH  : NQ
    const int B7 = B6 + NQ;                 // SETID : NQ
    const int B8 = B7 + NQ;                 // MULT  : NT
    const int B9 = B8 + NT;                 // MULR  : NQ
    const int B10 = B9 + NQ;                // MULS  : NS
    const int B11 = B10 + NS;               // MULA  : NQ

    // Walk rows; each (tag,seq) group is one op sequence we re-run from scratch.
    size_t idx = 0;
    auto stepRow = [&](r::PoseStack& ps) { check(ps, rows[idx].p, rows[idx].line); ++idx; };

    while (idx < rows.size()) {
        const std::string& tag = rows[idx].p[0];
        const int seq = std::stoi(rows[idx].p[1]);

        if (tag == "ROTID") {
            int i = seq - B1;
            r::PoseStack ps;
            stepRow(ps);                 // step0: identity
            ps.mulPose(q(i)); stepRow(ps);
        } else if (tag == "TRROT") {
            int local = seq - B2;
            int i = local / NT, t = local % NT;
            r::PoseStack ps;
            ps.translate(TS[t][0], TS[t][1], TS[t][2]); stepRow(ps);
            ps.mulPose(q(i));                            stepRow(ps);
        } else if (tag == "SCROT") {
            int local = seq - B3;
            int i = local / NS, sc = local % NS;
            r::PoseStack ps;
            ps.scale(SS[sc][0], SS[sc][1], SS[sc][2]); stepRow(ps);
            ps.mulPose(q(i));                          stepRow(ps);
        } else if (tag == "SCALE") {
            int sc = seq - B4;
            r::PoseStack ps;
            ps.scale(SS[sc][0], SS[sc][1], SS[sc][2]); stepRow(ps);
            ps.scale(SS[(sc + 1) % NS][0], SS[(sc + 1) % NS][1], SS[(sc + 1) % NS][2]); stepRow(ps);
        } else if (tag == "CHAIN") {
            int i = seq - B5;
            r::PoseStack ps;
            ps.translate(1.5f, -2.0f, 3.25f);      stepRow(ps);
            ps.mulPose(q(i));                       stepRow(ps);
            ps.scale(2.0f, 2.0f, 2.0f);             stepRow(ps);
            ps.mulPose(q((i + 1) % NQ));            stepRow(ps);
            ps.scale(-1.0f, -1.0f, -1.0f);          stepRow(ps);
            ps.translate(0.5f, 0.5f, 0.5f);         stepRow(ps);
            ps.mulPose(q((i + 2) % NQ));            stepRow(ps);
        } else if (tag == "PUSH") {
            int i = seq - B6;
            r::PoseStack ps;
            ps.translate(1, 2, 3);   stepRow(ps);
            ps.pushPose();           stepRow(ps);
            ps.mulPose(q(i));        stepRow(ps);
            ps.scale(2, -2, 2);      stepRow(ps);
            ps.pushPose();           stepRow(ps);
            ps.translate(-1, -1, -1); stepRow(ps);
            ps.popPose();            stepRow(ps);
            ps.popPose();            stepRow(ps);
            ps.mulPose(q((i + 1) % NQ)); stepRow(ps);
        } else if (tag == "SETID") {
            int i = seq - B7;
            r::PoseStack ps;
            ps.translate(4, 5, 6);   stepRow(ps);
            ps.scale(2, 3, 4);       stepRow(ps);
            ps.setIdentity();        stepRow(ps);
            ps.mulPose(q(i));        stepRow(ps);
        } else if (tag == "MULT") {
            int t = seq - B8;
            r::PoseStack ps;
            ps.scale(2, 3, 4);                          stepRow(ps);
            ps.mulPose(matTranslation(TS[t][0], TS[t][1], TS[t][2])); stepRow(ps);
        } else if (tag == "MULR") {
            int i = seq - B9;
            r::PoseStack ps;
            ps.translate(1, 1, 1);          stepRow(ps);
            ps.mulPose(matRotation(i));     stepRow(ps);
        } else if (tag == "MULS") {
            int sc = seq - B10;
            r::PoseStack ps;
            ps.translate(2, 0, -1);                            stepRow(ps);
            ps.mulPose(q(1));                                  stepRow(ps);
            ps.mulPose(matScaling(SS[sc][0], SS[sc][1], SS[sc][2])); stepRow(ps);
        } else if (tag == "MULA") {
            int i = seq - B11;
            r::PoseStack ps;
            ps.mulPose(q(i));                                  stepRow(ps);
            ps.mulPose(matRotScale((i + 3) % NQ, 2.0f, 0.5f, 1.5f)); stepRow(ps);
        } else {
            // Unknown tag: count as mismatch and advance to avoid an infinite loop.
            ++g_total; ++g_mism;
            if (g_shown++ < 40) std::cerr << "UNKNOWN_TAG " << rows[idx].line << "\n";
            ++idx;
        }
    }

    std::cout << "PoseStack cases=" << g_total << " mismatches=" << g_mism << "\n";
    return g_mism == 0 ? 0 : 1;
}
