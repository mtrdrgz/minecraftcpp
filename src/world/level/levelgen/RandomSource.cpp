#include "RandomSource.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include "platform/Platform.h"
#endif
#include <bcrypt.h>
#endif

namespace mc::levelgen {

namespace {
    static constexpr uint64_t LEGACY_MASK = 281474976710655ULL;
    static constexpr uint64_t LEGACY_MULTIPLIER = 25214903917ULL;
    static constexpr uint64_t LEGACY_INCREMENT = 11ULL;
    static constexpr float FLOAT_MULTIPLIER = 5.9604645E-8F;
    // Java's BitRandomSource/Xoroshiro multiply by the field
    // `double DOUBLE_MULTIPLIER = 1.110223E-16F;` — a double initialised from a
    // FLOAT literal, i.e. (double)(1.110223e-16f), which differs from the double
    // literal 1.110223E-16. The multiply itself is double precision. Initialise
    // from the float literal here to reproduce Java's exact bits.
    static constexpr double DOUBLE_MULTIPLIER = 1.110223E-16F;

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

    // Real RFC 1321 MD5. Java seeds every named worldgen noise via
    // XoroshiroPositionalRandomFactory.fromHashOf(name) =
    // Hashing.md5().hashString(name, UTF_8).asBytes(), so this MUST be the genuine
    // MD5 digest (the previous FNV placeholder made ALL overworld noise — climate
    // AND terrain — diverge from Java, collapsing biome variety). The 16 output
    // bytes are in standard MD5 order (each A/B/C/D word little-endian), matching
    // Guava's HashCode.asBytes(); seedFromHashOf then reads them big-endian via
    // longFromBytes, exactly like com.google.common.primitives.Longs.fromBytes.
    std::array<uint8_t, 16> md5Bytes(const std::string& input) {
        auto rotl = [](uint32_t x, uint32_t c) -> uint32_t { return (x << c) | (x >> (32 - c)); };
        static const uint32_t S[64] = {
            7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
            5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };
        static const uint32_t K[64] = {
            0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
            0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
            0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
            0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
            0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
            0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
            0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
            0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, 0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u };

        std::vector<uint8_t> msg(input.begin(), input.end());
        const uint64_t bitLen = static_cast<uint64_t>(msg.size()) * 8u;
        msg.push_back(0x80);
        while (msg.size() % 64 != 56) msg.push_back(0x00);
        for (int i = 0; i < 8; ++i) msg.push_back(static_cast<uint8_t>((bitLen >> (8 * i)) & 0xFF)); // length, little-endian

        uint32_t a0 = 0x67452301u, b0 = 0xefcdab89u, c0 = 0x98badcfeu, d0 = 0x10325476u;
        for (std::size_t off = 0; off < msg.size(); off += 64) {
            uint32_t M[16];
            for (int i = 0; i < 16; ++i)
                M[i] = static_cast<uint32_t>(msg[off + i * 4])
                     | (static_cast<uint32_t>(msg[off + i * 4 + 1]) << 8)
                     | (static_cast<uint32_t>(msg[off + i * 4 + 2]) << 16)
                     | (static_cast<uint32_t>(msg[off + i * 4 + 3]) << 24);
            uint32_t A = a0, B = b0, C = c0, D = d0;
            for (int i = 0; i < 64; ++i) {
                uint32_t F;
                int g;
                if (i < 16)      { F = (B & C) | (~B & D);        g = i; }
                else if (i < 32) { F = (D & B) | (~D & C);        g = (5 * i + 1) % 16; }
                else if (i < 48) { F = B ^ C ^ D;                 g = (3 * i + 5) % 16; }
                else             { F = C ^ (B | ~D);              g = (7 * i) % 16; }
                F = F + A + K[i] + M[g];
                A = D; D = C; C = B;
                B = B + rotl(F, S[i]);
            }
            a0 += A; b0 += B; c0 += C; d0 += D;
        }

        std::array<uint8_t, 16> digest{};
        auto put = [&](int base, uint32_t v) { for (int i = 0; i < 4; ++i) digest[base + i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF); };
        put(0, a0); put(4, b0); put(8, c0); put(12, d0);
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
    // Evaluate the two next(32) calls in order: C++ leaves function-argument
    // evaluation order unspecified, but java.util.Random is upper-then-lower.
    const int32_t upper = next(32);
    const int32_t lower = next(32);
    return composeJavaNextLong(upper, lower);
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
    // Evaluate the two next(32) calls in order: C++ leaves function-argument
    // evaluation order unspecified, but java.util.Random is upper-then-lower.
    const int32_t upper = next(32);
    const int32_t lower = next(32);
    return composeJavaNextLong(upper, lower);
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
    // Java evaluates the two nextLong() args left-to-right (seedLo then seedHi).
    // C++ argument evaluation order is unspecified (clang/x64 is right-to-left),
    // which would swap seedLo/seedHi and desync EVERY positional-derived noise.
    const int64_t seedLo = m_randomNumberGenerator.nextLong();
    const int64_t seedHi = m_randomNumberGenerator.nextLong();
    return std::make_shared<XoroshiroRandomSource>(seedLo, seedHi);
}

std::shared_ptr<PositionalRandomFactory> XoroshiroRandomSource::forkPositional() {
    // See fork(): force Java's left-to-right seedLo/seedHi evaluation order.
    const int64_t seedLo = m_randomNumberGenerator.nextLong();
    const int64_t seedHi = m_randomNumberGenerator.nextLong();
    return std::make_shared<XoroshiroPositionalRandomFactory>(seedLo, seedHi);
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

// ── WorldgenRandom ────────────────────────────────────────────────────────────
WorldgenRandom::WorldgenRandom(std::shared_ptr<RandomSource> source) : m_source(std::move(source)) {}

std::shared_ptr<RandomSource> WorldgenRandom::fork() { return m_source->fork(); }
std::shared_ptr<PositionalRandomFactory> WorldgenRandom::forkPositional() { return m_source->forkPositional(); }

void WorldgenRandom::setSeed(int64_t seed) {
    if (m_source) {
        m_source->setSeed(seed);
    }
}

int32_t WorldgenRandom::next(int32_t bits) {
    ++m_count;
    if (auto* legacy = dynamic_cast<LegacyRandomSource*>(m_source.get())) {
        return legacy->next(bits);
    }
    return static_cast<int32_t>(static_cast<uint64_t>(m_source->nextLong()) >> (64 - bits));
}

int32_t WorldgenRandom::nextInt() { return next(32); }

int32_t WorldgenRandom::nextInt(int32_t bound) {
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
        uint32_t wrapped = static_cast<uint32_t>(sample) - static_cast<uint32_t>(modulo) + static_cast<uint32_t>(bound - 1);
        if (static_cast<int32_t>(wrapped) >= 0) {
            return modulo;
        }
    } while (true);
}

int64_t WorldgenRandom::nextLong() {
    // java.util.Random: ((long)next(32) << 32) + next(32), evaluated left to right.
    const int32_t upper = next(32);
    const int32_t lower = next(32);
    return composeJavaNextLong(upper, lower);
}

bool WorldgenRandom::nextBoolean() { return next(1) != 0; }
float WorldgenRandom::nextFloat() { return next(24) * FLOAT_MULTIPLIER; }

double WorldgenRandom::nextDouble() {
    uint64_t combined = (static_cast<uint64_t>(next(26)) << 27) + static_cast<uint64_t>(next(27));
    return static_cast<double>(combined) * DOUBLE_MULTIPLIER;
}

double WorldgenRandom::nextGaussian() { return gaussian(*this, m_nextNextGaussian, m_haveNextNextGaussian); }

int64_t WorldgenRandom::setDecorationSeed(int64_t seed, int32_t blockX, int32_t blockZ) {
    setSeed(seed);
    const int64_t xScale = nextLong() | 1LL;
    const int64_t zScale = nextLong() | 1LL;
    const int64_t result = javaLongXor(
        javaLongAdd(javaLongMul(static_cast<int64_t>(blockX), xScale), javaLongMul(static_cast<int64_t>(blockZ), zScale)),
        seed);
    setSeed(result);
    return result;
}

void WorldgenRandom::setFeatureSeed(int64_t seed, int32_t index, int32_t step) {
    const int64_t result = javaLongAdd(javaLongAdd(seed, static_cast<int64_t>(index)),
                                       static_cast<int64_t>(10000 * step));
    setSeed(result);
}

void WorldgenRandom::setLargeFeatureSeed(int64_t seed, int32_t chunkX, int32_t chunkZ) {
    setSeed(seed);
    const int64_t xScale = nextLong();
    const int64_t zScale = nextLong();
    const int64_t result = javaLongXor(
        javaLongXor(javaLongMul(static_cast<int64_t>(chunkX), xScale), javaLongMul(static_cast<int64_t>(chunkZ), zScale)),
        seed);
    setSeed(result);
}

void WorldgenRandom::setLargeFeatureWithSalt(int64_t seed, int32_t x, int32_t z, int32_t salt) {
    const int64_t result = javaLongAdd(
        javaLongAdd(javaLongAdd(javaLongMul(static_cast<int64_t>(x), 341873128712LL),
                                javaLongMul(static_cast<int64_t>(z), 132897987541LL)),
                    seed),
        static_cast<int64_t>(salt));
    setSeed(result);
}

int64_t getMthSeed(int32_t x, int32_t y, int32_t z) {
    // Java: long seed = x * 3129871 ^ z * 116129781L ^ y;
    // The x term is int * int, so it overflows to 32 bits before widening.
    const int64_t xTerm = static_cast<int64_t>(javaInt(static_cast<uint32_t>(x) * 3129871U));
    const int64_t zTerm = javaLongMul(static_cast<int64_t>(z), 116129781LL);
    int64_t seed = javaLongXor(javaLongXor(xTerm, zTerm), static_cast<int64_t>(y));
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
