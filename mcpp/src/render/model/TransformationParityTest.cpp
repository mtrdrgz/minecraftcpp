// Bit-exact parity gate for com.mojang.math.Transformation (matrix-level subset).
// Reconstructs each input Matrix4f from 16 hex-float bits + its `properties` int
// (transported from the JOML ground truth so mul-dispatch matches), runs the C++
// port, and compares all 16 output floats + the output properties bit-for-bit.
#include "render/model/Transformation.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>

using mc::render::model::Transformation;
using mc::render::model::joml::Matrix4f;

static double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
static float  bf(const std::string& s) { return std::bit_cast<float>((uint32_t)std::stoul(s, nullptr, 16)); }
static uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
static uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }

// Build a Matrix4f from 16 hex-float fields starting at p[i] (column-major:
// m00,m01,m02,m03, m10,m11,m12,m13, m20,m21,m22,m23, m30,m31,m32,m33), then the
// `properties` int at p[i+16].
static Matrix4f readMat(const std::vector<std::string>& p, size_t i) {
    Matrix4f m;
    m.m00 = bf(p[i + 0]);  m.m01 = bf(p[i + 1]);  m.m02 = bf(p[i + 2]);  m.m03 = bf(p[i + 3]);
    m.m10 = bf(p[i + 4]);  m.m11 = bf(p[i + 5]);  m.m12 = bf(p[i + 6]);  m.m13 = bf(p[i + 7]);
    m.m20 = bf(p[i + 8]);  m.m21 = bf(p[i + 9]);  m.m22 = bf(p[i + 10]); m.m23 = bf(p[i + 11]);
    m.m30 = bf(p[i + 12]); m.m31 = bf(p[i + 13]); m.m32 = bf(p[i + 14]); m.m33 = bf(p[i + 15]);
    m.properties = std::stoi(p[i + 16]);
    return m;
}

// Compare a computed Matrix4f against the 16 float bits + properties at p[i].
// Returns true if ALL 17 fields match bit-for-bit.
static bool cmpMat(const Matrix4f& m, const std::vector<std::string>& p, size_t i) {
    const float vals[16] = {
        m.m00, m.m01, m.m02, m.m03,
        m.m10, m.m11, m.m12, m.m13,
        m.m20, m.m21, m.m22, m.m23,
        m.m30, m.m31, m.m32, m.m33,
    };
    for (int k = 0; k < 16; ++k) {
        if (fb(vals[k]) != (uint32_t)std::stoul(p[i + k], nullptr, 16)) return false;
    }
    if (m.properties != std::stoi(p[i + 16])) return false;
    return true;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) { std::cerr << "usage: --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long n = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> p;
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, '\t')) p.push_back(tok);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++n;

        if (tag == "GETMATRIX") {
            // [1..17] src(16+props)  [18..34] expected copy(16+props)
            Matrix4f src = readMat(p, 1);
            Transformation t(src);
            Matrix4f copy = t.getMatrixCopy();
            if (!cmpMat(copy, p, 18)) ++mism;
        } else if (tag == "COMPOSE") {
            // [1..17] A   [18..34] B   [35..51] expected
            Matrix4f a = readMat(p, 1);
            Matrix4f b = readMat(p, 18);
            Transformation ta(a), tb(b);
            Transformation r = ta.compose(tb);
            if (!cmpMat(r.getMatrix(), p, 35)) ++mism;
        } else if (tag == "INVERSE") {
            // [1..17] src   [18] finite   [19..35] expected inverse(16+props)
            Matrix4f src = readMat(p, 1);
            Transformation t(src);
            bool ok = false;
            Matrix4f inv = t.inverseMatrix(ok);
            int finite = std::stoi(p[18]);
            if ((ok ? 1 : 0) != finite) ++mism;
            else if (!cmpMat(inv, p, 19)) ++mism;
        } else if (tag == "IDENTITY") {
            // [1..17] expected identity matrix(16+props)
            Transformation t = Transformation::identity();
            if (!cmpMat(t.getMatrix(), p, 1)) ++mism;
        } else {
            std::cerr << "unknown tag: " << tag << "\n";
            ++mism;
        }
    }

    std::cout << "Transformation cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
