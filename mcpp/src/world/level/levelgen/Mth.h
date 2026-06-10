#pragma once

// 1:1 port of the parts of net.minecraft.util.Mth that worldgen depends on. These
// MUST match Java bit-for-bit: the sine table is the table-based approximation
// (NOT std::sin/cos), and the index/scale arithmetic reproduces Java's exact
// double-precision multiply + (long) truncation + 16-bit mask. Using a different
// scale (e.g. a truncated float 10430.378f) or table expression diverges by ULPs
// and corrupts feature/carver shapes at boundaries.

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace mc::levelgen::mth {

// ── Constants (each is a (float)Math.X cast in Java; Mth.java:20-25) ──────────
inline constexpr float PI         = static_cast<float>(3.141592653589793);
inline constexpr float HALF_PI    = static_cast<float>(3.141592653589793 / 2.0);
inline constexpr float TWO_PI     = static_cast<float>(3.141592653589793 * 2.0);
inline constexpr float DEG_TO_RAD = static_cast<float>(3.141592653589793 / 180.0);
inline constexpr float RAD_TO_DEG = 180.0f / static_cast<float>(3.141592653589793);
inline constexpr float EPSILON    = 1.0E-5F;

// SIN[i] = (float)Math.sin(i / 10430.378350470453), i in [0, 65536). Built once.
inline const std::array<float, 65536>& sinTable() {
    static const std::array<float, 65536> table = [] {
        std::array<float, 65536> t{};
        for (int i = 0; i < 65536; ++i) {
            t[static_cast<std::size_t>(i)] = static_cast<float>(std::sin(i / 10430.378350470453));
        }
        return t;
    }();
    return table;
}

// Mth.sin(double): SIN[(int)((long)(i * 10430.378350470453) & 65535L)].
inline float sin(double i) {
    const std::int64_t idx = static_cast<std::int64_t>(i * 10430.378350470453) & 65535LL;
    return sinTable()[static_cast<std::size_t>(idx)];
}

// Mth.cos(double): SIN[(int)((long)(i * 10430.378350470453 + 16384.0) & 65535L)].
inline float cos(double i) {
    const std::int64_t idx = static_cast<std::int64_t>(i * 10430.378350470453 + 16384.0) & 65535LL;
    return sinTable()[static_cast<std::size_t>(idx)];
}

// Mth.floor / Mth.ceil = (int)Math.floor/ceil (the float overloads widen to double).
inline int floor(double v) { return static_cast<int>(std::floor(v)); }
inline int floor(float v)  { return static_cast<int>(std::floor(static_cast<double>(v))); }
inline int ceil(double v)  { return static_cast<int>(std::ceil(v)); }
inline int ceil(float v)   { return static_cast<int>(std::ceil(static_cast<double>(v))); }

// Mth.lerp(double delta, double start, double end) = start + delta * (end - start).
inline double lerp(double delta, double start, double end) { return start + delta * (end - start); }
inline float lerpF(float delta, float start, float end) { return start + delta * (end - start); }   // Mth.java:532-534

// Mth.inverseLerp (Mth.java:326-332) + clampedLerp (:109-123) + clampedMap (:636-642),
// in BOTH precisions: callers mix them (e.g. DripstoneClusterFeature uses the float
// overload for the column chance and the double overload for the height bias).
inline double clampedLerpD(double factor, double min, double max) {
    if (factor < 0.0) return min;
    return factor > 1.0 ? max : lerp(factor, min, max);
}
inline float clampedLerpF(float factor, float min, float max) {
    if (factor < 0.0f) return min;
    return factor > 1.0f ? max : lerpF(factor, min, max);
}
inline double clampedMapD(double value, double fromMin, double fromMax, double toMin, double toMax) {
    return clampedLerpD((value - fromMin) / (fromMax - fromMin), toMin, toMax);
}
inline float clampedMapF(float value, float fromMin, float fromMax, float toMin, float toMax) {
    return clampedLerpF((value - fromMin) / (fromMax - fromMin), toMin, toMax);
}

// ─────────────────────────────────────────────────────────────────────────────
// Full net.minecraft.util.Mth surface (pure functions). Each line below mirrors
// the decompiled 26.1.2 source; integer/bit ops use unsigned for well-defined
// wraparound, signed for arithmetic shifts. Certified by mth_parity.
// ─────────────────────────────────────────────────────────────────────────────

// Mth.sqrt(float) = (float)Math.sqrt(x) — Mth.java:57-59.
inline float sqrt(float x) { return static_cast<float>(std::sqrt(static_cast<double>(x))); }

// Mth.lfloor(double) = (long)Math.floor(v) — Mth.java:69-71.
inline int64_t lfloor(double v) { return static_cast<int64_t>(std::floor(v)); }
// Mth.ceilLong(double) — Mth.java:89-91.
inline int64_t ceilLong(double v) { return static_cast<int64_t>(std::ceil(v)); }

// Mth.abs — Mth.java:73-79.
inline float abs(float v) { return std::fabs(v); }
inline int   abs(int v)   { return v < 0 ? -v : v; }

// Mth.clamp — Mth.java:93-107 (note: float/double use `value<min?min:min(value,max)`).
inline int     clamp(int value, int min, int max)             { return std::min(std::max(value, min), max); }
inline int64_t clamp(int64_t value, int64_t min, int64_t max) { return std::min(std::max(value, min), max); }
inline float   clamp(float value, float min, float max)       { return value < min ? min : std::min(value, max); }
inline double  clamp(double value, double min, double max)    { return value < min ? min : std::min(value, max); }

// Mth.absMax — Mth.java:125-135.
inline int    absMax(int a, int b)       { return std::max(abs(a), abs(b)); }
inline float  absMax(float a, float b)   { return std::max(std::fabs(a), std::fabs(b)); }
inline double absMax(double a, double b) { return std::max(std::fabs(a), std::fabs(b)); }

// Mth.chessboardDistance — Mth.java:137-139.
inline int chessboardDistance(int x0, int z0, int x1, int z1) { return absMax(x1 - x0, z1 - z0); }

// java.lang.Math.floorDiv(int,int) — used by Mth.floorDiv/positiveCeilDiv.
inline int floorDiv(int a, int b) {
    int q = a / b;
    if ((a ^ b) < 0 && q * b != a) --q;
    return q;
}
// java.lang.Math.floorMod(int,int).
inline int floorMod(int a, int b) {
    int r = a % b;
    if (r != 0 && ((r ^ b) < 0)) r += b;
    return r;
}

// Mth.equal — Mth.java:157-163 (both overloads use the 1e-5F threshold).
inline bool equal(float a, float b)   { return std::fabs(b - a) < 1.0E-5F; }
inline bool equal(double a, double b) { return std::fabs(b - a) < 1.0E-5F; }

// Mth.positiveModulo — Mth.java:165-175.
inline int    positiveModulo(int input, int mod)          { return floorMod(input, mod); }
inline float  positiveModulo(float input, float mod)      { return std::fmod(std::fmod(input, mod) + mod, mod); }
inline double positiveModulo(double input, double mod)    { return std::fmod(std::fmod(input, mod) + mod, mod); }

// Mth.isMultipleOf — Mth.java:177-179.
inline bool isMultipleOf(int dividend, int divisor) { return dividend % divisor == 0; }

// Mth.packDegrees/unpackDegrees — Mth.java:181-187.
inline int8_t packDegrees(float angle) { return static_cast<int8_t>(floor(angle * 256.0F / 360.0F)); }
inline float  unpackDegrees(int8_t rot) { return rot * 360 / 256.0F; }

// Mth.wrapDegrees — Mth.java:189-239 (int / long->float / float / double).
inline int wrapDegrees(int angle) {
    int n = angle % 360;
    if (n >= 180) n -= 360;
    if (n < -180) n += 360;
    return n;
}
inline float wrapDegrees(int64_t angle) {
    float n = static_cast<float>(angle % 360LL);
    if (n >= 180.0F) n -= 360.0F;
    if (n < -180.0F) n += 360.0F;
    return n;
}
inline float wrapDegrees(float angle) {
    float n = std::fmod(angle, 360.0F);
    if (n >= 180.0F) n -= 360.0F;
    if (n < -180.0F) n += 360.0F;
    return n;
}
inline double wrapDegrees(double angle) {
    double n = std::fmod(angle, 360.0);
    if (n >= 180.0) n -= 360.0;
    if (n < -180.0) n += 360.0;
    return n;
}

// Mth.degreesDifference / Abs — Mth.java:241-247.
inline float degreesDifference(float fromAngle, float toAngle) { return wrapDegrees(toAngle - fromAngle); }
inline float degreesDifferenceAbs(float a, float b) { return abs(degreesDifference(a, b)); }

// Mth.rotateIfNecessary / approach / approachDegrees — Mth.java:249-263.
inline float rotateIfNecessary(float baseAngle, float targetAngle, float maxAngleDiff) {
    float d = degreesDifference(baseAngle, targetAngle);
    float dc = clamp(d, -maxAngleDiff, maxAngleDiff);
    return targetAngle - dc;
}
inline float approach(float current, float target, float increment) {
    increment = abs(increment);
    return current < target ? clamp(current + increment, current, target) : clamp(current - increment, target, current);
}
inline float approachDegrees(float current, float target, float increment) {
    float difference = degreesDifference(current, target);
    return approach(current, current + difference, increment);
}

// Mth.smallestEncompassingPowerOfTwo / isPowerOfTwo / ceillog2 / log2 — Mth.java:269-298.
inline int smallestEncompassingPowerOfTwo(int input) {
    int r = input - 1;
    r |= r >> 1; r |= r >> 2; r |= r >> 4; r |= r >> 8; r |= r >> 16;
    return r + 1;
}
inline bool isPowerOfTwo(int input) { return input != 0 && (input & (input - 1)) == 0; }
inline constexpr int MULTIPLY_DE_BRUIJN_BIT_POSITION[32] = {
    0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
    31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9};
inline int ceillog2(int input) {
    input = isPowerOfTwo(input) ? input : smallestEncompassingPowerOfTwo(input);
    return MULTIPLY_DE_BRUIJN_BIT_POSITION[static_cast<int>(static_cast<int64_t>(input) * 125613361LL >> 27) & 31];
}
inline int log2(int input) { return ceillog2(input) - (isPowerOfTwo(input) ? 0 : 1); }
inline int smallestSquareSide(int itemCount) { return ceil(std::sqrt(static_cast<double>(itemCount))); }

// Mth.frac — Mth.java:300-306.
inline float  frac(float num)  { return num - static_cast<float>(floor(num)); }
inline double frac(double num) { return num - static_cast<double>(lfloor(num)); }

// Mth.getSeed — Mth.java:313-318.
inline int64_t getSeed(int x, int y, int z) {
    int32_t xa = static_cast<int32_t>(static_cast<uint32_t>(x) * 3129871u);
    int64_t seed = static_cast<int64_t>(xa) ^ (static_cast<int64_t>(z) * 116129781LL) ^ static_cast<int64_t>(y);
    seed = static_cast<int64_t>(static_cast<uint64_t>(seed) * static_cast<uint64_t>(seed) * 42317861ull
                                + static_cast<uint64_t>(seed) * 11ull);
    return seed >> 16;
}

// Mth.inverseLerp — Mth.java:326-332.
inline double inverseLerp(double value, double min, double max) { return (value - min) / (max - min); }
inline float  inverseLerp(float value, float min, float max)    { return (value - min) / (max - min); }

// Mth.fastInvSqrt(double) — Mth.java:431-437.
inline double fastInvSqrt(double x) {
    double xhalf = 0.5 * x;
    int64_t i = std::bit_cast<int64_t>(x);
    i = 6910469410427058090LL - (i >> 1);
    x = std::bit_cast<double>(i);
    return x * (1.5 - xhalf * x * x);
}
// Mth.fastInvCubeRoot(float) — Mth.java:439-445.
inline float fastInvCubeRoot(float x) {
    int32_t i = std::bit_cast<int32_t>(x);
    i = 1419967116 - i / 3;
    float y = std::bit_cast<float>(i);
    y = 0.6666667F * y + 1.0F / (3.0F * y * y * x);
    return 0.6666667F * y + 1.0F / (3.0F * y * y * x);
}
// Mth.invSqrt — Mth.java:422-428 (org.joml.Math.invsqrt; default no-FASTMATH).
// JOML: invsqrt(float) = 1.0f / sqrt(r), where sqrt(float)=(float)Math.sqrt(r) —
// the sqrt is narrowed to float FIRST, then the reciprocal is taken in float.
// invsqrt(double) = 1.0 / Math.sqrt(r).
inline float  invSqrt(float x)  { return 1.0f / static_cast<float>(std::sqrt(static_cast<double>(x))); }
inline double invSqrt(double x) { return 1.0 / std::sqrt(x); }

// ── Mth.atan2 fast-LUT approximation — Mth.java:373-420, 745-752 ──────────────
inline constexpr double FRAC_BIAS = std::bit_cast<double>(static_cast<int64_t>(4805340802404319232LL));
// Java builds ASIN_TAB/COS_TAB with Math.asin/Math.cos; the host libm differs by
// ULPs, so embed the EXACT Java values (reflection-dumped) — see MthAtanTables.inc.
#include "MthAtanTables.inc"
struct AtanTabs { double asin[257]; double cos[257]; };
inline const AtanTabs& atanTabs() {
    static const AtanTabs t = [] {
        AtanTabs a{};
        for (int ind = 0; ind < 257; ++ind) {
            a.asin[ind] = std::bit_cast<double>(MTH_ASIN_TAB_BITS[ind]);
            a.cos[ind]  = std::bit_cast<double>(MTH_COS_TAB_BITS[ind]);
        }
        return a;
    }();
    return t;
}
inline double atan2(double y, double x) {
    double d2 = x * x + y * y;
    if (std::isnan(d2)) return std::nan("");
    bool negY = y < 0.0; if (negY) y = -y;
    bool negX = x < 0.0; if (negX) x = -x;
    bool steep = y > x;  if (steep) { double t = x; x = y; y = t; }
    double rinv = fastInvSqrt(d2);
    x *= rinv; y *= rinv;
    double yp = FRAC_BIAS + y;
    int index = static_cast<int>(std::bit_cast<int64_t>(yp));
    const AtanTabs& tab = atanTabs();
    double phi = tab.asin[index];
    double cPhi = tab.cos[index];
    double sPhi = yp - FRAC_BIAS;
    double sd = y * cPhi - x * sPhi;
    double d = (6.0 + sd * sd) * sd * 0.16666666666666666;
    double theta = phi + d;
    if (steep) theta = (3.141592653589793 / 2) - theta;
    if (negX) theta = 3.141592653589793 - theta;
    if (negY) theta = -theta;
    return theta;
}

// net.minecraft.util.ARGB.color(a,r,g,b) — packed ARGB int.
inline int argbColor(int a, int r, int g, int b) { return (a << 24) | (r << 16) | (g << 8) | b; }
// Mth.hsvToArgb / hsvToRgb — Mth.java:447-496.
inline int hsvToArgb(float hue, float saturation, float value, int alpha) {
    int h = static_cast<int>(hue * 6.0F) % 6;
    float f = hue * 6.0F - h;
    float p = value * (1.0F - saturation);
    float q = value * (1.0F - f * saturation);
    float t = value * (1.0F - (1.0F - f) * saturation);
    float red, green, blue;
    switch (h) {
        case 0:  red = value; green = t;     blue = p;     break;
        case 1:  red = q;     green = value; blue = p;     break;
        case 2:  red = p;     green = value; blue = t;     break;
        case 3:  red = p;     green = q;     blue = value; break;
        case 4:  red = t;     green = p;     blue = value; break;
        default: red = value; green = p;     blue = q;     break; // case 5
    }
    return argbColor(alpha, clamp(static_cast<int>(red * 255.0F), 0, 255),
                     clamp(static_cast<int>(green * 255.0F), 0, 255),
                     clamp(static_cast<int>(blue * 255.0F), 0, 255));
}
inline int hsvToRgb(float hue, float saturation, float value) { return hsvToArgb(hue, saturation, value, 0); }

// Mth.murmurHash3Mixer — Mth.java:498-504.
inline int murmurHash3Mixer(int hash) {
    uint32_t h = static_cast<uint32_t>(hash);
    h ^= h >> 16;
    h *= static_cast<uint32_t>(-2048144789);
    h ^= h >> 13;
    h *= static_cast<uint32_t>(-1028477387);
    h ^= h >> 16;
    return static_cast<int>(h);
}

// Mth.lerpInt / lerpDiscrete — Mth.java:523-530.
inline int lerpInt(float alpha1, int p0, int p1) { return p0 + floor(alpha1 * (p1 - p0)); }
inline int lerpDiscrete(float alpha1, int p0, int p1) {
    int delta = p1 - p0;
    return p0 + floor(alpha1 * (delta - 1)) + (alpha1 > 0.0F ? 1 : 0);
}

// Mth.lerp2 / lerp3 — Mth.java:544-562.
inline double lerp2(double a1, double a2, double x00, double x10, double x01, double x11) {
    return lerp(a2, lerp(a1, x00, x10), lerp(a1, x01, x11));
}
inline double lerp3(double a1, double a2, double a3,
                    double x000, double x100, double x010, double x110,
                    double x001, double x101, double x011, double x111) {
    return lerp(a3, lerp2(a1, a2, x000, x100, x010, x110), lerp2(a1, a2, x001, x101, x011, x111));
}

// Mth.catmullrom — Mth.java:564-572.
inline float catmullrom(float alpha, float p0, float p1, float p2, float p3) {
    return 0.5F * (2.0F * p1 + (p2 - p0) * alpha
        + (2.0F * p0 - 5.0F * p1 + 4.0F * p2 - p3) * alpha * alpha
        + (3.0F * p1 - p0 - 3.0F * p2 + p3) * alpha * alpha * alpha);
}

// Mth.smoothstep / Derivative — Mth.java:574-580.
inline double smoothstep(double x) { return x * x * x * (x * (x * 6.0 - 15.0) + 10.0); }
inline double smoothstepDerivative(double x) { return 30.0 * x * x * (x - 1.0) * (x - 1.0); }

// Mth.sign(double) — Mth.java:582-588.
inline int sign(double n) { return n == 0.0 ? 0 : (n > 0.0 ? 1 : -1); }

// Mth.rotLerp / rotLerpRad — Mth.java:590-610.
inline float  rotLerp(float a, float from, float to)    { return from + a * wrapDegrees(to - from); }
inline double rotLerp(double a, double from, double to) { return from + a * wrapDegrees(to - from); }
inline float rotLerpRad(float a, float from, float to) {
    float diff = to - from;
    while (diff < -PI)            diff += TWO_PI;
    while (diff >= PI)            diff -= TWO_PI;
    return from + a * diff;
}

// Mth.triangleWave — Mth.java:612-614.
inline float triangleWave(float index, float period) {
    return (std::fabs(std::fmod(index, period) - period * 0.5F) - period * 0.25F) / (period * 0.25F);
}

// Mth.square / cube — Mth.java:616-634.
inline float   square(float x)  { return x * x; }
inline double  square(double x) { return x * x; }
inline int     square(int x)    { return static_cast<int>(static_cast<uint32_t>(x) * static_cast<uint32_t>(x)); }
inline int64_t square(int64_t x){ return static_cast<int64_t>(static_cast<uint64_t>(x) * static_cast<uint64_t>(x)); }
inline float   cube(float x)    { return x * x * x; }

// Mth.map — Mth.java:644-650.
inline double map(double value, double fromMin, double fromMax, double toMin, double toMax) {
    return lerp(inverseLerp(value, fromMin, fromMax), toMin, toMax);
}
inline float map(float value, float fromMin, float fromMax, float toMin, float toMax) {
    return lerpF(inverseLerp(value, fromMin, fromMax), toMin, toMax);
}

// Mth.quantize / roundToward / positiveCeilDiv — Mth.java:656-702.
inline int positiveCeilDiv(int input, int divisor) { return -floorDiv(-input, divisor); }
inline int roundToward(int input, int multiple) { return positiveCeilDiv(input, multiple) * multiple; }
inline int quantize(double value, int quantizeResolution) {
    return floor(value / quantizeResolution) * quantizeResolution;
}

// Mth.length / lengthSquared — Mth.java:676-698.
inline double lengthSquared(double x, double y) { return x * x + y * y; }
inline double length(double x, double y) { return std::sqrt(lengthSquared(x, y)); }
inline float  length(float x, float y) { return static_cast<float>(std::sqrt(static_cast<double>(x * x + y * y))); }
inline double lengthSquared(double x, double y, double z) { return x * x + y * y + z * z; }
inline double length(double x, double y, double z) { return std::sqrt(lengthSquared(x, y, z)); }
inline float  lengthSquared(float x, float y, float z) { return x * x + y * y + z * z; }

// Mth.binarySearch — Mth.java:506-521.
template <typename Pred>
inline int binarySearch(int from, int to, Pred condition) {
    int len = to - from;
    while (len > 0) {
        int half = len / 2;
        int middle = from + half;
        if (condition(middle)) { len = half; }
        else { from = middle + 1; len -= half + 1; }
    }
    return from;
}

} // namespace mc::levelgen::mth
