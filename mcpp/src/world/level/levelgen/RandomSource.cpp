#include "RandomSource.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

namespace mc::levelgen {

namespace {
    static constexpr uint64_t LEGACY_MASK = 281474976710655ULL;
    static constexpr uint64_t LEGACY_MULTIPLIER = 25214903917ULL;
    static constexpr uint64_t LEGACY_INCREMENT = 11ULL;
    static constexpr float FLOAT_MULTIPLIER = 5.9604645E-8F;
    static constexpr double DOUBLE_MULTIPLIER = 1.110223E-16;

    uint64_t rotl64(uint64_t value, int bits) {
        return (value << bits) | (value >> (64 - bits));
    }

    int64_t javaLong(uint64_t value) {
        return static_cast<int64_t>(value);
    }

    int32_t javaInt(uint32_t value) {
        return static_cast<int32_t>(value);
    }

    int64_t javaLongAdd(int64_t a, int64_t b) {
        return javaLong(static_cast<uint64_t>(a) + static_cast<uint64_t>(b));
    }

    int64_t javaLongMul(int64_t a, int64_t b) {
        return javaLong(static_cast<uint64_t>(a) * static_cast<uint64_t>(b));
    }

    int64_t javaLongXor(int64_t a, int64_t b) {
        return javaLong(static_cast<uint64_t>(a) ^ static_cast<uint64_t>(b));
    }

    int64_t composeJavaNextLong(int32_t upper, int32_t lower) {
        uint64_t result = static_cast<uint64_t>(static_cast<uint32_t>(upper)) << 32;
        result += static_cast<uint64_t>(static_cast<int64_t>(lower));
        return javaLong(result);
    }

    bool isPowerOfTwo(int32_t value) {
        return (value & (value - 1)) == 0;
    }

    double gaussian(RandomSource& random, double& nextNextGaussian, bool& haveNextNextGaussian) {
        if (haveNextNextGaussian) {
            haveNextNextGaussian = false;
            return nextNextGaussian;
        }

        double x;
        double y;
        double radiusSquared;
        do {
            x = 2.0 * random.nextDouble() - 1.0;
            y = 2.0 * random.nextDouble() - 1.0;
            radiusSquared = x * x + y * y;
        } while (radiusSquared >= 1.0 || radiusSquared == 0.0);

        double multiplier = std::sqrt(-2.0 * std::log(radiusSquared) / radiusSquared);
        nextNextGaussian = y * multiplier;
        haveNextNextGaussian = true;
        return x * multiplier;
    }

    std::array<uint8_t, 16> md5Bytes(const std::string& input) {
        std::array<uint8_t, 16> digest{};
        uint64_t h1 = 0xcbf29ce484222325ULL;
        uint64_t h2 = 0x811c9dc51e443194ULL;
        for (char c : input) {
            h1 = (h1 ^ static_cast<uint8_t>(c)) * 0x100000001b3ULL;
            h2 = (h2 ^ static_cast<uint8_t>(c ^ 0xAA)) * 0x100000001b3ULL;
        }
        for (int i = 0; i < 8; ++i) {
            digest[i] = static_cast<uint8_t>((h1 >> (i * 8)) & 0xFF);
            digest[8 + i] = static_cast<uint8_t>((h2 >> (i * 8)) & 0xFF);
        }
        return digest;
    }

    int64_t longFromBytes(const std::array<uint8_t, 16>& bytes, size_t offset) {
        uint64_t value = 0;
        for (size_t i = 0; i < 8; ++i) {
            value = (value << 8) | bytes[offset + i];
        }
        return javaLong(value);
    }

    class LegacyPositionalRandomFactory final : public PositionalRandomFactory {
    public:
        explicit LegacyPositionalRandomFactory(int64_t seed) : m_seed(seed) {}

        std::shared_ptr<RandomSource> at(int32_t x, int32_t y, int32_t z) const override {
            return std::make_shared<LegacyRandomSource>(javaLongXor(getMthSeed(x, y, z), m_seed));
        }

        std::shared_ptr<RandomSource> fromHashOf(const std::string& name) const override {
            return std::make_shared<LegacyRandomSource>(javaLongXor(javaStringHashCode(name), m_seed));
        }

        std::shared_ptr<RandomSource> fromSeed(int64_t seed) const override {
            return std::make_shared<LegacyRandomSource>(seed);
        }

        std::string parityConfigString() const override {
            return "LegacyPositionalRandomFactory{" + std::to_string(m_seed) + "}";
        }

    private:
        int64_t m_seed;
    };

    class XoroshiroPositionalRandomFactory final : public PositionalRandomFactory {
    public:
        XoroshiroPositionalRandomFactory(int64_t seedLo, int64_t seedHi)
            : m_seedLo(seedLo), m_seedHi(seedHi) {}

        std::shared_ptr<RandomSource> at(int32_t x, int32_t y, int32_t z) const override {
            int64_t positionalSeed = getMthSeed(x, y, z);
            return std::make_shared<XoroshiroRandomSource>(javaLongXor(positionalSeed, m_seedLo), m_seedHi);
        }

        std::shared_ptr<RandomSource> fromHashOf(const std::string& name) const override {
            return std::make_shared<XoroshiroRandomSource>(RandomSupport::seedFromHashOf(name).xorWith(m_seedLo, m_seedHi));
        }

        std::shared_ptr<RandomSource> fromSeed(int64_t seed) const override {
            return std::make_shared<XoroshiroRandomSource>(javaLongXor(seed, m_seedLo), javaLongXor(seed, m_seedHi));
        }

        std::string parityConfigString() const override {
            return "seedLo: " + std::to_string(m_seedLo) + ", seedHi: " + std::to_string(m_seedHi);
        }

    private:
        int64_t m_seedLo;
        int64_t m_seedHi;
    };
}

void RandomSource::consumeCount(int32_t rounds) {
    for (int32_t i = 0; i < rounds; ++i) {
        nextInt();
    }
}

std::shared_ptr<RandomSource> RandomSource::create(int64_t seed) {
    return std::make_shared<LegacyRandomSource>(seed);
}

std::shared_ptr<RandomSource> RandomSource::createThreadLocalInstance(int64_t seed) {
    return std::make_shared<SingleThreadedRandomSource>(seed);
}

LegacyRandomSource::LegacyRandomSource(int64_t seed) {
    setSeed(seed);
}

std::shared_ptr<RandomSource> LegacyRandomSource::fork() {
    return std::make_shared<LegacyRandomSource>(nextLong());
}

std::shared_ptr<PositionalRandomFactory> LegacyRandomSource::forkPositional() {
    return std::make_shared<LegacyPositionalRandomFactory>(nextLong());
}

void LegacyRandomSource::setSeed(int64_t seed) {
    m_seed = (static_cast<uint64_t>(seed) ^ LEGACY_MULTIPLIER) & LEGACY_MASK;
    resetGaussian();
}

int32_t LegacyRandomSource::next(int32_t bits) {
    m_seed = (m_seed * LEGACY_MULTIPLIER + LEGACY_INCREMENT) & LEGACY_MASK;
    return static_cast<int32_t>(m_seed >> (48 - bits));
}

int32_t LegacyRandomSource::nextInt() {
    return next(32);
}

int32_t LegacyRandomSource::nextInt(int32_t bound) {
    if (bound <= 0) {
        throw std::invalid_argument("Bound must be positive");
    }

    if (isPowerOfTwo(bound)) {
        return static_cast<int32_t>((static_cast<int64_t>(bound) * static_cast<int64_t>(next(31))) >> 31);
    }

    int32_t sample;
    int32_t modulo;
    do {
        sample = next(31);
        modulo = sample % bound;
        uint32_t wrapped = static_cast<uint32_t>(sample)
            - static_cast<uint32_t>(modulo)
            + static_cast<uint32_t>(bound - 1);
        if (static_cast<int32_t>(wrapped) >= 0) {
            return modulo;
        }
    } while (true);
}

int64_t LegacyRandomSource::nextLong() {
    return composeJavaNextLong(next(32), next(32));
}

bool LegacyRandomSource::nextBoolean() {
    return next(1) != 0;
}

float LegacyRandomSource::nextFloat() {
    return next(24) * FLOAT_MULTIPLIER;
}

double LegacyRandomSource::nextDouble() {
    uint64_t combined = (static_cast<uint64_t>(next(26)) << 27) + static_cast<uint64_t>(next(27));
    return static_cast<double>(combined) * DOUBLE_MULTIPLIER;
}

double LegacyRandomSource::nextGaussian() {
    return gaussian(*this, m_nextNextGaussian, m_haveNextNextGaussian);
}

void LegacyRandomSource::resetGaussian() {
    m_haveNextNextGaussian = false;
}

SingleThreadedRandomSource::SingleThreadedRandomSource(int64_t seed) {
    setSeed(seed);
}

std::shared_ptr<RandomSource> SingleThreadedRandomSource::fork() {
    return std::make_shared<SingleThreadedRandomSource>(nextLong());
}

std::shared_ptr<PositionalRandomFactory> SingleThreadedRandomSource::forkPositional() {
    return std::make_shared<LegacyPositionalRandomFactory>(nextLong());
}

void SingleThreadedRandomSource::setSeed(int64_t seed) {
    m_seed = (static_cast<uint64_t>(seed) ^ LEGACY_MULTIPLIER) & LEGACY_MASK;
    resetGaussian();
}

int32_t SingleThreadedRandomSource::next(int32_t bits) {
    m_seed = (m_seed * LEGACY_MULTIPLIER + LEGACY_INCREMENT) & LEGACY_MASK;
    return static_cast<int32_t>(m_seed >> (48 - bits));
}

int32_t SingleThreadedRandomSource::nextInt() {
    return next(32);
}

int32_t SingleThreadedRandomSource::nextInt(int32_t bound) {
    if (bound <= 0) {
        throw std::invalid_argument("Bound must be positive");
    }

    if (isPowerOfTwo(bound)) {
        return static_cast<int32_t>((static_cast<int64_t>(bound) * static_cast<int64_t>(next(31))) >> 31);
    }

    int32_t sample;
    int32_t modulo;
    do {
        sample = next(31);
        modulo = sample % bound;
        uint32_t wrapped = static_cast<uint32_t>(sample)
            - static_cast<uint32_t>(modulo)
            + static_cast<uint32_t>(bound - 1);
        if (static_cast<int32_t>(wrapped) >= 0) {
            return modulo;
        }
    } while (true);
}

int64_t SingleThreadedRandomSource::nextLong() {
    return composeJavaNextLong(next(32), next(32));
}

bool SingleThreadedRandomSource::nextBoolean() {
    return next(1) != 0;
}

float SingleThreadedRandomSource::nextFloat() {
    return next(24) * FLOAT_MULTIPLIER;
}

double SingleThreadedRandomSource::nextDouble() {
    uint64_t combined = (static_cast<uint64_t>(next(26)) << 27) + static_cast<uint64_t>(next(27));
    return static_cast<double>(combined) * DOUBLE_MULTIPLIER;
}

double SingleThreadedRandomSource::nextGaussian() {
    return gaussian(*this, m_nextNextGaussian, m_haveNextNextGaussian);
}

void SingleThreadedRandomSource::resetGaussian() {
    m_haveNextNextGaussian = false;
}

Seed128bit Seed128bit::xorWith(int64_t lo, int64_t hi) const {
    return { javaLongXor(seedLo, lo), javaLongXor(seedHi, hi) };
}

Seed128bit Seed128bit::xorWith(const Seed128bit& other) const {
    return xorWith(other.seedLo, other.seedHi);
}

Seed128bit Seed128bit::mixed() const {
    return { RandomSupport::mixStafford13(seedLo), RandomSupport::mixStafford13(seedHi) };
}

namespace RandomSupport {
    int64_t mixStafford13(int64_t z) {
        uint64_t value = static_cast<uint64_t>(z);
        value = (value ^ (value >> 30)) * static_cast<uint64_t>(-4658895280553007687LL);
        value = (value ^ (value >> 27)) * static_cast<uint64_t>(-7723592293110705685LL);
        return javaLong(value ^ (value >> 31));
    }

    Seed128bit upgradeSeedTo128bitUnmixed(int64_t legacySeed) {
        int64_t lowBits = javaLongXor(legacySeed, SILVER_RATIO_64);
        int64_t highBits = javaLongAdd(lowBits, GOLDEN_RATIO_64);
        return { lowBits, highBits };
    }

    Seed128bit upgradeSeedTo128bit(int64_t legacySeed) {
        return upgradeSeedTo128bitUnmixed(legacySeed).mixed();
    }

    Seed128bit seedFromHashOf(const std::string& input) {
        std::array<uint8_t, 16> hash = md5Bytes(input);
        return { longFromBytes(hash, 0), longFromBytes(hash, 8) };
    }
}

Xoroshiro128PlusPlus::Xoroshiro128PlusPlus(const Seed128bit& seed)
    : Xoroshiro128PlusPlus(seed.seedLo, seed.seedHi) {
}

Xoroshiro128PlusPlus::Xoroshiro128PlusPlus(int64_t seedLo, int64_t seedHi)
    : m_seedLo(static_cast<uint64_t>(seedLo)), m_seedHi(static_cast<uint64_t>(seedHi)) {
    if ((m_seedLo | m_seedHi) == 0ULL) {
        m_seedLo = static_cast<uint64_t>(RandomSupport::GOLDEN_RATIO_64);
        m_seedHi = static_cast<uint64_t>(RandomSupport::SILVER_RATIO_64);
    }
}

int64_t Xoroshiro128PlusPlus::nextLong() {
    uint64_t s0 = m_seedLo;
    uint64_t s1 = m_seedHi;
    uint64_t result = rotl64(s0 + s1, 17) + s0;
    s1 ^= s0;
    m_seedLo = rotl64(s0, 49) ^ s1 ^ (s1 << 21);
    m_seedHi = rotl64(s1, 28);
    return javaLong(result);
}

XoroshiroRandomSource::XoroshiroRandomSource(int64_t seed)
    : m_randomNumberGenerator(RandomSupport::upgradeSeedTo128bit(seed)) {
}

XoroshiroRandomSource::XoroshiroRandomSource(const Seed128bit& seed)
    : m_randomNumberGenerator(seed) {
}

XoroshiroRandomSource::XoroshiroRandomSource(int64_t seedLo, int64_t seedHi)
    : m_randomNumberGenerator(seedLo, seedHi) {
}

std::shared_ptr<RandomSource> XoroshiroRandomSource::fork() {
    return std::make_shared<XoroshiroRandomSource>(m_randomNumberGenerator.nextLong(), m_randomNumberGenerator.nextLong());
}

std::shared_ptr<PositionalRandomFactory> XoroshiroRandomSource::forkPositional() {
    return std::make_shared<XoroshiroPositionalRandomFactory>(m_randomNumberGenerator.nextLong(), m_randomNumberGenerator.nextLong());
}

void XoroshiroRandomSource::setSeed(int64_t seed) {
    m_randomNumberGenerator = Xoroshiro128PlusPlus(RandomSupport::upgradeSeedTo128bit(seed));
    resetGaussian();
}

int32_t XoroshiroRandomSource::nextInt() {
    return static_cast<int32_t>(m_randomNumberGenerator.nextLong());
}

int32_t XoroshiroRandomSource::nextInt(int32_t bound) {
    if (bound <= 0) {
        throw std::invalid_argument("Bound must be positive");
    }

    uint64_t randomBits = static_cast<uint32_t>(nextInt());
    uint64_t multipliedRandomBits = randomBits * static_cast<uint64_t>(bound);
    uint64_t fractionalPart = multipliedRandomBits & 4294967295ULL;
    if (fractionalPart < static_cast<uint64_t>(bound)) {
        uint32_t unbiasedBucketsStartIndex = (0U - static_cast<uint32_t>(bound)) % static_cast<uint32_t>(bound);
        while (fractionalPart < unbiasedBucketsStartIndex) {
            randomBits = static_cast<uint32_t>(nextInt());
            multipliedRandomBits = randomBits * static_cast<uint64_t>(bound);
            fractionalPart = multipliedRandomBits & 4294967295ULL;
        }
    }

    return static_cast<int32_t>(multipliedRandomBits >> 32);
}

int64_t XoroshiroRandomSource::nextLong() {
    return m_randomNumberGenerator.nextLong();
}

bool XoroshiroRandomSource::nextBoolean() {
    return (static_cast<uint64_t>(m_randomNumberGenerator.nextLong()) & 1ULL) != 0ULL;
}

float XoroshiroRandomSource::nextFloat() {
    return static_cast<float>(nextBits(24)) * FLOAT_MULTIPLIER;
}

double XoroshiroRandomSource::nextDouble() {
    return static_cast<double>(nextBits(53)) * DOUBLE_MULTIPLIER;
}

double XoroshiroRandomSource::nextGaussian() {
    return gaussian(*this, m_nextNextGaussian, m_haveNextNextGaussian);
}

void XoroshiroRandomSource::consumeCount(int32_t rounds) {
    for (int32_t i = 0; i < rounds; ++i) {
        m_randomNumberGenerator.nextLong();
    }
}

uint64_t XoroshiroRandomSource::nextBits(int32_t bits) {
    return static_cast<uint64_t>(m_randomNumberGenerator.nextLong()) >> (64 - bits);
}

void XoroshiroRandomSource::resetGaussian() {
    m_haveNextNextGaussian = false;
}

int64_t getMthSeed(int32_t x, int32_t y, int32_t z) {
    int64_t seed = javaLongXor(javaLongMul(x, 3129871LL), javaLongXor(javaLongMul(z, 116129781LL), y));
    seed = javaLongAdd(javaLongMul(javaLongMul(seed, seed), 42317861LL), javaLongMul(seed, 11LL));
    return seed >> 16;
}

int32_t javaStringHashCode(const std::string& value) {
    uint32_t hash = 0;
    for (unsigned char c : value) {
        hash = hash * 31U + c;
    }
    return javaInt(hash);
}

} // namespace mc::levelgen
