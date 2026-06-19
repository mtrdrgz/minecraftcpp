// VERIFY parity gate for the ENGINE port of net.minecraft.util.CubicSpline
// (mcpp/src/world/level/levelgen/CubicSpline.cpp/.h — the DensityFunction-coupled
// implementation that the live worldgen build links). We do NOT duplicate the spline
// math here: we #include the real engine header, build the SAME fixed splines via the
// engine's CubicSpline::builder / CubicSpline::constant, and drive the engine's
// CubicSpline::apply(DensityFunctionContext) over a float sweep, comparing every
// result bit-for-bit against ground truth from the real net.minecraft.util.CubicSpline
// (tools/CubicSplineVerifyParity.java).
//
// Driving apply() with a raw float: the engine's apply(context) first evaluates
//   float input = coordinate->apply(context);
// We supply a coordinate that is a settable IDENTITY BoundedFloatFunction — its
// apply() returns a value we stash before each call, and its minValue()/maxValue()
// are -inf/+inf, exactly mirroring BoundedFloatFunction.IDENTITY used by the GT tool
// (so the construction-time bounds computation matches too). The block coordinates in
// DensityFunctionContext are irrelevant — the coordinate ignores them.
//
//   cubic_spline_verify_parity --cases mcpp/build/cubic_spline_verify.tsv

#include "CubicSpline.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::BoundedFloatFunction;
using mc::levelgen::BoundedFloatFunctionPtr;
using mc::levelgen::CubicSpline;
using mc::levelgen::CubicSplinePtr;
using mc::levelgen::DensityFunctionContext;

namespace {

// A settable IDENTITY coordinate: apply() returns the stored input (set via setInput
// before each evaluation), min/max = -inf/+inf. Equivalent to BoundedFloatFunction
// .createUnlimited(x -> x) (BoundedFloatFunction.java:7,15-31).
class IdentityCoordinate final : public BoundedFloatFunction {
public:
    float apply(const DensityFunctionContext&) const override { return m_input; }
    float minValue() const override { return -std::numeric_limits<float>::infinity(); }
    float maxValue() const override { return std::numeric_limits<float>::infinity(); }

    void setInput(float v) const { m_input = v; }

private:
    mutable float m_input = 0.0f;
};

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
uint32_t u32(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }
float    bf(const std::string& s) { return std::bit_cast<float>(u32(s)); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }

// ---- Fixed splines, all sharing the one IDENTITY coordinate `coord` ----------
// These mirror CubicSplineVerifyParity.java EXACTLY (locations, values, derivatives,
// and which addPoint overload is used).

CubicSplinePtr splineConst() { return CubicSpline::constant(3.25f); }

CubicSplinePtr splineFlat(const BoundedFloatFunctionPtr& coord) {
    return CubicSpline::builder(coord)
        .addPoint(-4.0f, 7.0f)
        .addPoint(-1.0f, -2.0f)
        .addPoint(1.5f, 3.0f)
        .addPoint(5.0f, -5.0f)
        .build();
}

CubicSplinePtr splineDeriv(const BoundedFloatFunctionPtr& coord) {
    return CubicSpline::builder(coord)
        .addPoint(-3.0f, 4.0f, -1.25f)
        .addPoint(-0.5f, -2.0f, 0.5f)
        .addPoint(2.0f, 6.0f, -2.0f)
        .addPoint(4.5f, -1.0f, 1.75f)
        .build();
}

CubicSplinePtr splineMixed(const BoundedFloatFunctionPtr& coord) {
    return CubicSpline::builder(coord)
        .addPoint(-2.0f, 1.0f, 0.0f)
        .addPoint(0.0f, 5.0f, 2.5f)
        .addPoint(3.0f, -3.0f, 0.0f)
        .addPoint(6.0f, 2.0f, -1.0f)
        .build();
}

CubicSplinePtr splineSingle(const BoundedFloatFunctionPtr& coord) {
    return CubicSpline::builder(coord).addPoint(0.0f, 1.0f, 2.0f).build();
}

CubicSplinePtr splineSingle0(const BoundedFloatFunctionPtr& coord) {
    return CubicSpline::builder(coord).addPoint(2.5f, -4.0f, 0.0f).build();
}

CubicSplinePtr splineNested(const BoundedFloatFunctionPtr& coord) {
    // Inner multipoint (child value of the middle point of the outer spline).
    CubicSplinePtr inner = CubicSpline::builder(coord)
                               .addPoint(-2.0f, 10.0f, 1.0f)
                               .addPoint(3.0f, -4.0f, -2.0f)
                               .build();
    // addPoint(location, sampler) fixes the child point's derivative to 0.0f — same
    // overload the GT tool uses for the nested point.
    return CubicSpline::builder(coord)
        .addPoint(-5.0f, 0.5f, 0.5f)
        .addPoint(0.0f, inner)
        .addPoint(6.0f, -3.0f, 0.0f)
        .build();
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: cubic_spline_verify_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    // One shared IDENTITY coordinate; we set its input before each apply().
    auto coordImpl = std::make_shared<IdentityCoordinate>();
    BoundedFloatFunctionPtr coord = coordImpl;

    const CubicSplinePtr k = splineConst();
    const CubicSplinePtr flat = splineFlat(coord);
    const CubicSplinePtr deriv = splineDeriv(coord);
    const CubicSplinePtr mixed = splineMixed(coord);
    const CubicSplinePtr single = splineSingle(coord);
    const CubicSplinePtr single0 = splineSingle0(coord);
    const CubicSplinePtr nested = splineNested(coord);

    DensityFunctionContext ctx;  // block coords irrelevant (coordinate ignores them)

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.size() < 3) continue;
        const std::string& tag = p[0];
        const float coordVal = bf(p[1]);
        const uint32_t exp = u32(p[2]);

        const CubicSpline* s = nullptr;
        if (tag == "K") s = k.get();
        else if (tag == "FLAT") s = flat.get();
        else if (tag == "DERIV") s = deriv.get();
        else if (tag == "MIXED") s = mixed.get();
        else if (tag == "SINGLE") s = single.get();
        else if (tag == "SINGLE0") s = single0.get();
        else if (tag == "NESTED") s = nested.get();
        else continue;

        ++total;
        coordImpl->setInput(coordVal);
        const uint32_t got = fb(s->apply(ctx));
        if (got != exp) {
            ++mism;
            if (shown++ < 40)
                std::cerr << "MISMATCH " << tag << " got=" << std::hex << got << " exp=" << exp
                          << std::dec << " | " << line << "\n";
        }
    }

    std::cout << "CubicSplineVerify cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
