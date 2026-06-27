#include "Noise.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace mc::levelgen {

namespace {
    double lerp(double alpha, double p0, double p1) {
        return p0 + alpha * (p1 - p0);
    }

    double lerp2(double alpha1, double alpha2, double x00, double x10, double x01, double x11) {
        return lerp(alpha2, lerp(alpha1, x00, x10), lerp(alpha1, x01, x11));
    }

    double lerp3(
        double alpha1,
        double alpha2,
        double alpha3,
        double x000,
        double x100,
        double x010,
        double x110,
        double x001,
        double x101,
        double x011,
        double x111
    ) {
        return lerp(alpha3, lerp2(alpha1, alpha2, x000, x100, x010, x110), lerp2(alpha1, alpha2, x001, x101, x011, x111));
    }

    double smoothstep(double x) {
        return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
    }

    double smoothstepDerivative(double x) {
        return 30.0 * x * x * (x - 1.0) * (x - 1.0);
    }

    double clampedLerp(double factor, double min, double max) {
        if (factor < 0.0) return min;
        return factor > 1.0 ? max : lerp(factor, min, max);
    }

    int32_t floorInt(double v) {
        return static_cast<int32_t>(std::floor(v));
    }

    int64_t floorLong(double v) {
        return static_cast<int64_t>(std::floor(v));
    }

    std::vector<int32_t> rangeClosed(int32_t from, int32_t to) {
        std::vector<int32_t> values;
        values.reserve(static_cast<size_t>(to - from + 1));
        for (int32_t i = from; i <= to; ++i) {
            values.push_back(i);
        }
        return values;
    }

    int64_t doubleToJavaLong(double value) {
        if (std::isnan(value)) {
            return 0;
        }
        if (value >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
            return std::numeric_limits<int64_t>::max();
        }
        if (value <= static_cast<double>(std::numeric_limits<int64_t>::min())) {
            return std::numeric_limits<int64_t>::min();
        }
        return static_cast<int64_t>(value);
    }
}

SimplexNoise::SimplexNoise(RandomSource& random) {
    xo = random.nextDouble() * 256.0;
    yo = random.nextDouble() * 256.0;
    zo = random.nextDouble() * 256.0;

    for (int32_t i = 0; i < 256; ++i) {
        m_p[i] = i;
    }

    for (int32_t ix = 0; ix < 256; ++ix) {
        int32_t offset = random.nextInt(256 - ix);
        std::swap(m_p[ix], m_p[offset + ix]);
    }
}

int32_t SimplexNoise::p(int32_t x) const {
    return m_p[x & 0xFF];
}

double SimplexNoise::dot(const std::array<int32_t, 3>& g, double x, double y, double z) {
    return g[0] * x + g[1] * y + g[2] * z;
}

double SimplexNoise::getCornerNoise3D(int32_t index, double x, double y, double z, double base) const {
    double t0 = base - x * x - y * y - z * z;
    if (t0 < 0.0) {
        return 0.0;
    }

    t0 *= t0;
    return t0 * t0 * dot(GRADIENT[static_cast<size_t>(index)], x, y, z);
}

double SimplexNoise::getValue(double xin, double yin) const {
    const double sqrt3 = std::sqrt(3.0);
    const double f2 = 0.5 * (sqrt3 - 1.0);
    const double g2 = (3.0 - sqrt3) / 6.0;
    double s = (xin + yin) * f2;
    int32_t i = floorInt(xin + s);
    int32_t j = floorInt(yin + s);
    double t = (i + j) * g2;
    double x0 = xin - (i - t);
    double y0 = yin - (j - t);
    int32_t i1 = x0 > y0 ? 1 : 0;
    int32_t j1 = x0 > y0 ? 0 : 1;
    double x1 = x0 - i1 + g2;
    double y1 = y0 - j1 + g2;
    double x2 = x0 - 1.0 + 2.0 * g2;
    double y2 = y0 - 1.0 + 2.0 * g2;
    int32_t ii = i & 0xFF;
    int32_t jj = j & 0xFF;
    int32_t gi0 = p(ii + p(jj)) % 12;
    int32_t gi1 = p(ii + i1 + p(jj + j1)) % 12;
    int32_t gi2 = p(ii + 1 + p(jj + 1)) % 12;
    double n0 = getCornerNoise3D(gi0, x0, y0, 0.0, 0.5);
    double n1 = getCornerNoise3D(gi1, x1, y1, 0.0, 0.5);
    double n2 = getCornerNoise3D(gi2, x2, y2, 0.0, 0.5);
    return 70.0 * (n0 + n1 + n2);
}

double SimplexNoise::getValue(double xin, double yin, double zin) const {
    double s = (xin + yin + zin) * 0.3333333333333333;
    int32_t i = floorInt(xin + s);
    int32_t j = floorInt(yin + s);
    int32_t k = floorInt(zin + s);
    double t = (i + j + k) * 0.16666666666666666;
    double x0 = xin - (i - t);
    double y0 = yin - (j - t);
    double z0 = zin - (k - t);

    int32_t i1;
    int32_t j1;
    int32_t k1;
    int32_t i2;
    int32_t j2;
    int32_t k2;
    if (x0 >= y0) {
        if (y0 >= z0) {
            i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0;
        } else if (x0 >= z0) {
            i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1;
        } else {
            i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1;
        }
    } else if (y0 < z0) {
        i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1;
    } else if (x0 < z0) {
        i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1;
    } else {
        i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0;
    }

    double x1 = x0 - i1 + 0.16666666666666666;
    double y1 = y0 - j1 + 0.16666666666666666;
    double z1 = z0 - k1 + 0.16666666666666666;
    double x2 = x0 - i2 + 0.3333333333333333;
    double y2 = y0 - j2 + 0.3333333333333333;
    double z2 = z0 - k2 + 0.3333333333333333;
    double x3 = x0 - 1.0 + 0.5;
    double y3 = y0 - 1.0 + 0.5;
    double z3 = z0 - 1.0 + 0.5;
    int32_t ii = i & 0xFF;
    int32_t jj = j & 0xFF;
    int32_t kk = k & 0xFF;
    int32_t gi0 = p(ii + p(jj + p(kk))) % 12;
    int32_t gi1 = p(ii + i1 + p(jj + j1 + p(kk + k1))) % 12;
    int32_t gi2 = p(ii + i2 + p(jj + j2 + p(kk + k2))) % 12;
    int32_t gi3 = p(ii + 1 + p(jj + 1 + p(kk + 1))) % 12;
    double n0 = getCornerNoise3D(gi0, x0, y0, z0, 0.6);
    double n1 = getCornerNoise3D(gi1, x1, y1, z1, 0.6);
    double n2 = getCornerNoise3D(gi2, x2, y2, z2, 0.6);
    double n3 = getCornerNoise3D(gi3, x3, y3, z3, 0.6);
    return 32.0 * (n0 + n1 + n2 + n3);
}

PerlinSimplexNoise::PerlinSimplexNoise(RandomSource& random, std::vector<int32_t> octaveSet) {
    if (octaveSet.empty()) {
        throw std::invalid_argument("Need some octaves!");
    }

    std::sort(octaveSet.begin(), octaveSet.end());
    octaveSet.erase(std::unique(octaveSet.begin(), octaveSet.end()), octaveSet.end());

    const int32_t lowFreqOctaves = -octaveSet.front();
    const int32_t highFreqOctaves = octaveSet.back();
    const int32_t octaves = lowFreqOctaves + highFreqOctaves + 1;
    if (octaves < 1) {
        throw std::invalid_argument("Total number of octaves needs to be >= 1");
    }

    auto containsOctave = [&octaveSet](int32_t octave) {
        return std::binary_search(octaveSet.begin(), octaveSet.end(), octave);
    };

    SimplexNoise zeroOctave(random);
    const int32_t zeroOctaveIndex = highFreqOctaves;
    m_noiseLevels.resize(static_cast<size_t>(octaves));
    if (zeroOctaveIndex >= 0 && zeroOctaveIndex < octaves && containsOctave(0)) {
        m_noiseLevels[static_cast<size_t>(zeroOctaveIndex)] = std::make_unique<SimplexNoise>(zeroOctave);
    }

    for (int32_t i = zeroOctaveIndex + 1; i < octaves; ++i) {
        if (i >= 0 && containsOctave(zeroOctaveIndex - i)) {
            m_noiseLevels[static_cast<size_t>(i)] = std::make_unique<SimplexNoise>(random);
        } else {
            random.consumeCount(262);
        }
    }

    if (highFreqOctaves > 0) {
        const double seedValue = zeroOctave.getValue(zeroOctave.xo, zeroOctave.yo, zeroOctave.zo) *
            static_cast<double>(9.223372e18f);
        LegacyRandomSource highFreqRandom(doubleToJavaLong(seedValue));

        for (int32_t i = zeroOctaveIndex - 1; i >= 0; --i) {
            if (i < octaves && containsOctave(zeroOctaveIndex - i)) {
                m_noiseLevels[static_cast<size_t>(i)] = std::make_unique<SimplexNoise>(highFreqRandom);
            } else {
                highFreqRandom.consumeCount(262);
            }
        }
    }

    m_highestFreqInputFactor = std::pow(2.0, highFreqOctaves);
    m_highestFreqValueFactor = 1.0 / (std::pow(2.0, octaves) - 1.0);
}

double PerlinSimplexNoise::getValue(double x, double y, bool useNoiseStart) const {
    double value = 0.0;
    double factor = m_highestFreqInputFactor;
    double valueFactor = m_highestFreqValueFactor;

    for (const auto& noiseLevel : m_noiseLevels) {
        if (noiseLevel) {
            value += noiseLevel->getValue(
                x * factor + (useNoiseStart ? noiseLevel->xo : 0.0),
                y * factor + (useNoiseStart ? noiseLevel->yo : 0.0)) * valueFactor;
        }

        factor /= 2.0;
        valueFactor *= 2.0;
    }

    return value;
}

ImprovedNoise::ImprovedNoise(RandomSource& random) {
    xo = random.nextDouble() * 256.0;
    yo = random.nextDouble() * 256.0;
    zo = random.nextDouble() * 256.0;

    for (int32_t i = 0; i < 256; ++i) {
        m_p[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
    }

    for (int32_t i = 0; i < 256; ++i) {
        int32_t offset = random.nextInt(256 - i);
        std::swap(m_p[static_cast<size_t>(i)], m_p[static_cast<size_t>(i + offset)]);
    }
}

double ImprovedNoise::noise(double x, double y, double z) const {
    return noise(x, y, z, 0.0, 0.0);
}

double ImprovedNoise::noise(double x, double y, double z, double yScale, double yFudge) const {
    x += xo;
    y += yo;
    z += zo;
    int32_t xf = floorInt(x);
    int32_t yf = floorInt(y);
    int32_t zf = floorInt(z);
    double xr = x - xf;
    double yr = y - yf;
    double zr = z - zf;
    double yrFudge;
    if (yScale != 0.0) {
        double fudgeLimit = (yFudge >= 0.0 && yFudge < yr) ? yFudge : yr;
        // Java source: `Mth.floor(fudgeLimit / yScale + 1.0E-7F) * yScale` — the `F`
        // suffix makes the literal a float, which Java widens to double for the add.
        // C++ `1.0E-7` would be a true double literal (different value), so we must
        // cast through float to reproduce Java's widening semantics bit-for-bit.
        yrFudge = std::floor(fudgeLimit / yScale + static_cast<double>(1.0E-7F)) * yScale;
    } else {
        yrFudge = 0.0;
    }

    return sampleAndLerp(xf, yf, zf, xr, yr - yrFudge, zr, yr);
}

double ImprovedNoise::noiseWithDerivative(double x, double y, double z, std::array<double, 3>& derivativeOut) const {
    x += xo;
    y += yo;
    z += zo;
    int32_t xf = floorInt(x);
    int32_t yf = floorInt(y);
    int32_t zf = floorInt(z);
    double xr = x - xf;
    double yr = y - yf;
    double zr = z - zf;
    return sampleWithDerivative(xf, yf, zf, xr, yr, zr, derivativeOut);
}

int32_t ImprovedNoise::p(int32_t x) const {
    return m_p[static_cast<size_t>(x & 0xFF)] & 0xFF;
}

double ImprovedNoise::gradDot(int32_t hash, double x, double y, double z) {
    return SimplexNoise::dot(SimplexNoise::GRADIENT[static_cast<size_t>(hash & 15)], x, y, z);
}

double ImprovedNoise::sampleAndLerp(int32_t x, int32_t y, int32_t z, double xr, double yr, double zr, double yrOriginal) const {
    int32_t x0 = p(x);
    int32_t x1 = p(x + 1);
    int32_t xy00 = p(x0 + y);
    int32_t xy01 = p(x0 + y + 1);
    int32_t xy10 = p(x1 + y);
    int32_t xy11 = p(x1 + y + 1);
    double d000 = gradDot(p(xy00 + z), xr, yr, zr);
    double d100 = gradDot(p(xy10 + z), xr - 1.0, yr, zr);
    double d010 = gradDot(p(xy01 + z), xr, yr - 1.0, zr);
    double d110 = gradDot(p(xy11 + z), xr - 1.0, yr - 1.0, zr);
    double d001 = gradDot(p(xy00 + z + 1), xr, yr, zr - 1.0);
    double d101 = gradDot(p(xy10 + z + 1), xr - 1.0, yr, zr - 1.0);
    double d011 = gradDot(p(xy01 + z + 1), xr, yr - 1.0, zr - 1.0);
    double d111 = gradDot(p(xy11 + z + 1), xr - 1.0, yr - 1.0, zr - 1.0);
    double xAlpha = smoothstep(xr);
    double yAlpha = smoothstep(yrOriginal);
    double zAlpha = smoothstep(zr);
    return lerp3(xAlpha, yAlpha, zAlpha, d000, d100, d010, d110, d001, d101, d011, d111);
}

double ImprovedNoise::sampleWithDerivative(int32_t x, int32_t y, int32_t z, double xr, double yr, double zr, std::array<double, 3>& derivativeOut) const {
    int32_t x0 = p(x);
    int32_t x1 = p(x + 1);
    int32_t xy00 = p(x0 + y);
    int32_t xy01 = p(x0 + y + 1);
    int32_t xy10 = p(x1 + y);
    int32_t xy11 = p(x1 + y + 1);
    int32_t p000 = p(xy00 + z);
    int32_t p100 = p(xy10 + z);
    int32_t p010 = p(xy01 + z);
    int32_t p110 = p(xy11 + z);
    int32_t p001 = p(xy00 + z + 1);
    int32_t p101 = p(xy10 + z + 1);
    int32_t p011 = p(xy01 + z + 1);
    int32_t p111 = p(xy11 + z + 1);
    const auto& g000 = SimplexNoise::GRADIENT[static_cast<size_t>(p000 & 15)];
    const auto& g100 = SimplexNoise::GRADIENT[static_cast<size_t>(p100 & 15)];
    const auto& g010 = SimplexNoise::GRADIENT[static_cast<size_t>(p010 & 15)];
    const auto& g110 = SimplexNoise::GRADIENT[static_cast<size_t>(p110 & 15)];
    const auto& g001 = SimplexNoise::GRADIENT[static_cast<size_t>(p001 & 15)];
    const auto& g101 = SimplexNoise::GRADIENT[static_cast<size_t>(p101 & 15)];
    const auto& g011 = SimplexNoise::GRADIENT[static_cast<size_t>(p011 & 15)];
    const auto& g111 = SimplexNoise::GRADIENT[static_cast<size_t>(p111 & 15)];
    double d000 = SimplexNoise::dot(g000, xr, yr, zr);
    double d100 = SimplexNoise::dot(g100, xr - 1.0, yr, zr);
    double d010 = SimplexNoise::dot(g010, xr, yr - 1.0, zr);
    double d110 = SimplexNoise::dot(g110, xr - 1.0, yr - 1.0, zr);
    double d001 = SimplexNoise::dot(g001, xr, yr, zr - 1.0);
    double d101 = SimplexNoise::dot(g101, xr - 1.0, yr, zr - 1.0);
    double d011 = SimplexNoise::dot(g011, xr, yr - 1.0, zr - 1.0);
    double d111 = SimplexNoise::dot(g111, xr - 1.0, yr - 1.0, zr - 1.0);
    double xAlpha = smoothstep(xr);
    double yAlpha = smoothstep(yr);
    double zAlpha = smoothstep(zr);
    double d1x = lerp3(xAlpha, yAlpha, zAlpha, g000[0], g100[0], g010[0], g110[0], g001[0], g101[0], g011[0], g111[0]);
    double d1y = lerp3(xAlpha, yAlpha, zAlpha, g000[1], g100[1], g010[1], g110[1], g001[1], g101[1], g011[1], g111[1]);
    double d1z = lerp3(xAlpha, yAlpha, zAlpha, g000[2], g100[2], g010[2], g110[2], g001[2], g101[2], g011[2], g111[2]);
    double d2x = lerp2(yAlpha, zAlpha, d100 - d000, d110 - d010, d101 - d001, d111 - d011);
    double d2y = lerp2(zAlpha, xAlpha, d010 - d000, d011 - d001, d110 - d100, d111 - d101);
    double d2z = lerp2(xAlpha, yAlpha, d001 - d000, d101 - d100, d011 - d010, d111 - d110);
    derivativeOut[0] += d1x + smoothstepDerivative(xr) * d2x;
    derivativeOut[1] += d1y + smoothstepDerivative(yr) * d2y;
    derivativeOut[2] += d1z + smoothstepDerivative(zr) * d2z;
    return lerp3(xAlpha, yAlpha, zAlpha, d000, d100, d010, d110, d001, d101, d011, d111);
}

PerlinNoise PerlinNoise::createLegacyForBlendedNoise(RandomSource& random, const std::vector<int32_t>& octaves) {
    auto [firstOctave, amplitudes] = makeAmplitudes(octaves);
    return PerlinNoise(random, firstOctave, std::move(amplitudes), false);
}

PerlinNoise PerlinNoise::createLegacyForLegacyNetherBiome(RandomSource& random, int32_t firstOctave, const std::vector<double>& amplitudes) {
    return PerlinNoise(random, firstOctave, amplitudes, false);
}

PerlinNoise PerlinNoise::create(RandomSource& random, const std::vector<int32_t>& octaveSet) {
    auto [firstOctave, amplitudes] = makeAmplitudes(octaveSet);
    return PerlinNoise(random, firstOctave, std::move(amplitudes), true);
}

PerlinNoise PerlinNoise::create(RandomSource& random, int32_t firstOctave, const std::vector<double>& amplitudes) {
    return PerlinNoise(random, firstOctave, amplitudes, true);
}

PerlinNoise::PerlinNoise(RandomSource& random, int32_t firstOctave, std::vector<double> amplitudes, bool useNewInitialization)
    : m_firstOctave(firstOctave), m_amplitudes(std::move(amplitudes)) {
    int32_t octaves = static_cast<int32_t>(m_amplitudes.size());
    int32_t zeroOctaveIndex = -m_firstOctave;
    m_noiseLevels.resize(static_cast<size_t>(octaves));

    if (useNewInitialization) {
        std::shared_ptr<PositionalRandomFactory> positional = random.forkPositional();
        for (int32_t i = 0; i < octaves; ++i) {
            if (m_amplitudes[static_cast<size_t>(i)] != 0.0) {
                int32_t octave = m_firstOctave + i;
                std::shared_ptr<RandomSource> octaveRandom = positional->fromHashOf("octave_" + std::to_string(octave));
                m_noiseLevels[static_cast<size_t>(i)] = std::make_unique<ImprovedNoise>(*octaveRandom);
            }
        }
    } else {
        auto zeroOctave = std::make_unique<ImprovedNoise>(random);
        if (zeroOctaveIndex >= 0 && zeroOctaveIndex < octaves) {
            double zeroOctaveAmplitude = m_amplitudes[static_cast<size_t>(zeroOctaveIndex)];
            if (zeroOctaveAmplitude != 0.0) {
                m_noiseLevels[static_cast<size_t>(zeroOctaveIndex)] = std::move(zeroOctave);
            }
        }

        for (int32_t i = zeroOctaveIndex - 1; i >= 0; --i) {
            if (i < octaves) {
                double amplitude = m_amplitudes[static_cast<size_t>(i)];
                if (amplitude != 0.0) {
                    m_noiseLevels[static_cast<size_t>(i)] = std::make_unique<ImprovedNoise>(random);
                } else {
                    skipOctave(random);
                }
            } else {
                skipOctave(random);
            }
        }

        if (zeroOctaveIndex < octaves - 1) {
            throw std::invalid_argument("Positive octaves are temporarily disabled");
        }
    }

    m_lowestFreqInputFactor = std::pow(2.0, -zeroOctaveIndex);
    m_lowestFreqValueFactor = std::pow(2.0, octaves - 1) / (std::pow(2.0, octaves) - 1.0);
    m_maxValue = edgeValue(2.0);
}

std::pair<int32_t, std::vector<double>> PerlinNoise::makeAmplitudes(std::vector<int32_t> octaveSet) {
    if (octaveSet.empty()) {
        throw std::invalid_argument("Need some octaves!");
    }

    std::sort(octaveSet.begin(), octaveSet.end());
    octaveSet.erase(std::unique(octaveSet.begin(), octaveSet.end()), octaveSet.end());
    int32_t lowFreqOctaves = -octaveSet.front();
    int32_t highFreqOctaves = octaveSet.back();
    int32_t octaves = lowFreqOctaves + highFreqOctaves + 1;
    if (octaves < 1) {
        throw std::invalid_argument("Total number of octaves needs to be >= 1");
    }

    std::vector<double> amplitudes(static_cast<size_t>(octaves), 0.0);
    for (int32_t octave : octaveSet) {
        amplitudes[static_cast<size_t>(octave + lowFreqOctaves)] = 1.0;
    }

    return { -lowFreqOctaves, std::move(amplitudes) };
}

void PerlinNoise::skipOctave(RandomSource& random) {
    random.consumeCount(262);
}

double PerlinNoise::getValue(double x, double y, double z) const {
    return getValue(x, y, z, 0.0, 0.0);
}

double PerlinNoise::getValue(double x, double y, double z, double yScale, double yFudge) const {
    double value = 0.0;
    double factor = m_lowestFreqInputFactor;
    double valueFactor = m_lowestFreqValueFactor;

    for (size_t i = 0; i < m_noiseLevels.size(); ++i) {
        const std::unique_ptr<ImprovedNoise>& noise = m_noiseLevels[i];
        if (noise) {
            double noiseVal = noise->noise(wrap(x * factor), wrap(y * factor), wrap(z * factor), yScale * factor, yFudge * factor);
            value += m_amplitudes[i] * noiseVal * valueFactor;
        }

        factor *= 2.0;
        valueFactor /= 2.0;
    }

    return value;
}

double PerlinNoise::maxBrokenValue(double yScale) const {
    return edgeValue(yScale + 2.0);
}

const ImprovedNoise* PerlinNoise::getOctaveNoise(int32_t i) const {
    int32_t index = static_cast<int32_t>(m_noiseLevels.size()) - 1 - i;
    if (index < 0 || index >= static_cast<int32_t>(m_noiseLevels.size())) {
        return nullptr;
    }
    return m_noiseLevels[static_cast<size_t>(index)].get();
}

double PerlinNoise::wrap(double x) {
    return x - floorLong(x / 3.3554432E7 + 0.5) * 3.3554432E7;
}

double PerlinNoise::edgeValue(double noiseValue) const {
    double value = 0.0;
    double valueFactor = m_lowestFreqValueFactor;

    for (size_t i = 0; i < m_noiseLevels.size(); ++i) {
        if (m_noiseLevels[i]) {
            value += m_amplitudes[i] * noiseValue * valueFactor;
        }
        valueFactor /= 2.0;
    }

    return value;
}

NormalNoise NormalNoise::create(RandomSource& random, const NoiseParameters& parameters) {
    return NormalNoise(random, parameters, true);
}

NormalNoise NormalNoise::createLegacyNetherBiome(RandomSource& random, const NoiseParameters& parameters) {
    return NormalNoise(random, parameters, false);
}

NormalNoise::NormalNoise(RandomSource& random, NoiseParameters parameters, bool useNewInitialization)
    : m_parameters(std::move(parameters)) {
    if (useNewInitialization) {
        m_first = PerlinNoise::create(random, m_parameters.firstOctave, m_parameters.amplitudes);
        m_second = PerlinNoise::create(random, m_parameters.firstOctave, m_parameters.amplitudes);
    } else {
        m_first = PerlinNoise::createLegacyForLegacyNetherBiome(random, m_parameters.firstOctave, m_parameters.amplitudes);
        m_second = PerlinNoise::createLegacyForLegacyNetherBiome(random, m_parameters.firstOctave, m_parameters.amplitudes);
    }

    int32_t minOctave = std::numeric_limits<int32_t>::max();
    int32_t maxOctave = std::numeric_limits<int32_t>::min();
    for (int32_t i = 0; i < static_cast<int32_t>(m_parameters.amplitudes.size()); ++i) {
        if (m_parameters.amplitudes[static_cast<size_t>(i)] != 0.0) {
            minOctave = std::min(minOctave, i);
            maxOctave = std::max(maxOctave, i);
        }
    }

    int32_t octaveSpan = maxOctave - minOctave;
    m_valueFactor = 0.16666666666666666 / expectedDeviation(octaveSpan);
    m_maxValue = (m_first.maxValue() + m_second.maxValue()) * m_valueFactor;
}

double NormalNoise::expectedDeviation(int32_t octaveSpan) {
    return 0.1 * (1.0 + 1.0 / (octaveSpan + 1));
}

double NormalNoise::getValue(double x, double y, double z) const {
    double x2 = x * 1.0181268882175227;
    double y2 = y * 1.0181268882175227;
    double z2 = z * 1.0181268882175227;
    return (m_first.getValue(x, y, z) + m_second.getValue(x2, y2, z2)) * m_valueFactor;
}

BlendedNoise BlendedNoise::createUnseeded(double xzScale, double yScale, double xzFactor, double yFactor, double smearScaleMultiplier) {
    return BlendedNoise(std::make_shared<XoroshiroRandomSource>(0LL), xzScale, yScale, xzFactor, yFactor, smearScaleMultiplier);
}

BlendedNoise::BlendedNoise(std::shared_ptr<RandomSource> random, double xzScale, double yScale, double xzFactor, double yFactor, double smearScaleMultiplier)
    : m_minLimitNoise(PerlinNoise::createLegacyForBlendedNoise(*random, rangeClosed(-15, 0))),
      m_maxLimitNoise(PerlinNoise::createLegacyForBlendedNoise(*random, rangeClosed(-15, 0))),
      m_mainNoise(PerlinNoise::createLegacyForBlendedNoise(*random, rangeClosed(-7, 0))),
      m_xzFactor(xzFactor),
      m_yFactor(yFactor),
      m_smearScaleMultiplier(smearScaleMultiplier),
      m_xzScale(xzScale),
      m_yScale(yScale) {
    // NOTE: members are initialized in DECLARATION order (minLimit, maxLimit, main),
    // NOT the textual order in the initializer list. This guarantees the three
    // PerlinNoise instances consume `random` in the same order as Java's BlendedNoise
    // constructor (min first, max second, main third). The previous delegating-ctor
    // form relied on argument-evaluation order which is UNSPECIFIED in C++ and broke
    // parity on GCC (which evaluated right-to-left, putting main first).
    m_xzMultiplier = 684.412 * m_xzScale;
    m_yMultiplier = 684.412 * m_yScale;
    m_maxValue = m_minLimitNoise.maxBrokenValue(m_yMultiplier);
}

BlendedNoise::BlendedNoise(
    PerlinNoise minLimitNoise,
    PerlinNoise maxLimitNoise,
    PerlinNoise mainNoise,
    double xzScale,
    double yScale,
    double xzFactor,
    double yFactor,
    double smearScaleMultiplier
) : m_minLimitNoise(std::move(minLimitNoise)),
    m_maxLimitNoise(std::move(maxLimitNoise)),
    m_mainNoise(std::move(mainNoise)),
    m_xzFactor(xzFactor),
    m_yFactor(yFactor),
    m_smearScaleMultiplier(smearScaleMultiplier),
    m_xzScale(xzScale),
    m_yScale(yScale) {
    m_xzMultiplier = 684.412 * m_xzScale;
    m_yMultiplier = 684.412 * m_yScale;
    m_maxValue = m_minLimitNoise.maxBrokenValue(m_yMultiplier);
}

double BlendedNoise::compute(int32_t blockX, int32_t blockY, int32_t blockZ) const {
    double limitX = blockX * m_xzMultiplier;
    double limitY = blockY * m_yMultiplier;
    double limitZ = blockZ * m_xzMultiplier;
    double mainX = limitX / m_xzFactor;
    double mainY = limitY / m_yFactor;
    double mainZ = limitZ / m_xzFactor;
    double limitSmear = m_yMultiplier * m_smearScaleMultiplier;
    double mainSmear = limitSmear / m_yFactor;
    double blendMin = 0.0;
    double blendMax = 0.0;
    double mainNoiseValue = 0.0;
    double pow = 1.0;

    // Cache octave pointers to avoid repeated getOctaveNoise() lookups (which
    // do a vector index calculation + bounds check each call). With 8 + 16 + 16
    // = 40 octave lookups per compute(), this saves ~40 bounds checks.
    const ImprovedNoise* mainOctaves[8] = {};
    const ImprovedNoise* minOctaves[16] = {};
    const ImprovedNoise* maxOctaves[16] = {};
    for (int i = 0; i < 8; ++i) mainOctaves[i] = m_mainNoise.getOctaveNoise(i);
    for (int i = 0; i < 16; ++i) minOctaves[i] = m_minLimitNoise.getOctaveNoise(i);
    for (int i = 0; i < 16; ++i) maxOctaves[i] = m_maxLimitNoise.getOctaveNoise(i);

    for (int32_t i = 0; i < 8; ++i) {
        const ImprovedNoise* noise = mainOctaves[i];
        if (noise) {
            mainNoiseValue += noise->noise(
                PerlinNoise::wrap(mainX * pow),
                PerlinNoise::wrap(mainY * pow),
                PerlinNoise::wrap(mainZ * pow),
                mainSmear * pow,
                mainY * pow) / pow;
        }

        pow /= 2.0;
    }

    double factor = (mainNoiseValue / 10.0 + 1.0) / 2.0;
    bool isMax = factor >= 1.0;
    bool isMin = factor <= 0.0;
    pow = 1.0;

    for (int32_t i = 0; i < 16; ++i) {
        double wx = PerlinNoise::wrap(limitX * pow);
        double wy = PerlinNoise::wrap(limitY * pow);
        double wz = PerlinNoise::wrap(limitZ * pow);
        double yScalePow = limitSmear * pow;
        if (!isMax) {
            if (minOctaves[i]) {
                blendMin += minOctaves[i]->noise(wx, wy, wz, yScalePow, limitY * pow) / pow;
            }
        }

        if (!isMin) {
            if (maxOctaves[i]) {
                blendMax += maxOctaves[i]->noise(wx, wy, wz, yScalePow, limitY * pow) / pow;
            }
        }

        pow /= 2.0;
    }

    return clampedLerp(factor, blendMin / 512.0, blendMax / 512.0) / 128.0;
}

BlendedNoiseFunction::BlendedNoiseFunction(BlendedNoise noise)
    : m_noise(std::move(noise)) {
}

double BlendedNoiseFunction::compute(const DensityFunctionContext& context) const {
    return m_noise.compute(context.blockX, context.blockY, context.blockZ);
}

double BlendedNoiseFunction::minValue() const {
    return m_noise.minValue();
}

double BlendedNoiseFunction::maxValue() const {
    return m_noise.maxValue();
}

namespace DensityFunctions {
    DensityFunctionPtr blendedNoise(BlendedNoise noise) {
        return std::make_shared<BlendedNoiseFunction>(std::move(noise));
    }
}

void HeightmapGenerator::generateChunkHeightmap(int chunkX, int chunkZ, std::vector<int>& heightmap, const BlendedNoise& noise) {
    heightmap.resize(256);
    for (int32_t z = 0; z < 16; ++z) {
        for (int32_t x = 0; x < 16; ++x) {
            int32_t blockX = chunkX * 16 + x;
            int32_t blockZ = chunkZ * 16 + z;
            int32_t top = -64;
            for (int32_t y = 319; y >= -64; --y) {
                if (noise.compute(blockX, y, blockZ) > 0.0) {
                    top = y;
                    break;
                }
            }
            heightmap[static_cast<size_t>(z * 16 + x)] = top;
        }
    }
}

} // namespace mc::levelgen
