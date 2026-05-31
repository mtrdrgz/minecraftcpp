#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace mc::levelgen {

class PositionalRandomFactory;

class RandomSource {
public:
    virtual ~RandomSource() = default;

    virtual std::shared_ptr<RandomSource> fork() = 0;
    virtual std::shared_ptr<PositionalRandomFactory> forkPositional() = 0;
    virtual void setSeed(int64_t seed) = 0;
    virtual int32_t nextInt() = 0;
    virtual int32_t nextInt(int32_t bound) = 0;
    virtual int64_t nextLong() = 0;
    virtual bool nextBoolean() = 0;
    virtual float nextFloat() = 0;
    virtual double nextDouble() = 0;
    virtual double nextGaussian() = 0;

    virtual void consumeCount(int32_t rounds);

    static std::shared_ptr<RandomSource> create(int64_t seed);
    static std::shared_ptr<RandomSource> createThreadLocalInstance(int64_t seed);
};

class PositionalRandomFactory {
public:
    virtual ~PositionalRandomFactory() = default;

    virtual std::shared_ptr<RandomSource> at(int32_t x, int32_t y, int32_t z) const = 0;
    virtual std::shared_ptr<RandomSource> fromHashOf(const std::string& name) const = 0;
    virtual std::shared_ptr<RandomSource> fromSeed(int64_t seed) const = 0;
    virtual std::string parityConfigString() const = 0;
};

class LegacyRandomSource final : public RandomSource {
public:
    explicit LegacyRandomSource(int64_t seed);

    std::shared_ptr<RandomSource> fork() override;
    std::shared_ptr<PositionalRandomFactory> forkPositional() override;
    void setSeed(int64_t seed) override;
    int32_t nextInt() override;
    int32_t nextInt(int32_t bound) override;
    int64_t nextLong() override;
    bool nextBoolean() override;
    float nextFloat() override;
    double nextDouble() override;
    double nextGaussian() override;
    int32_t next(int32_t bits);

private:
    uint64_t m_seed = 0;
    double m_nextNextGaussian = 0.0;
    bool m_haveNextNextGaussian = false;

    void resetGaussian();
};

class SingleThreadedRandomSource final : public RandomSource {
public:
    explicit SingleThreadedRandomSource(int64_t seed);

    std::shared_ptr<RandomSource> fork() override;
    std::shared_ptr<PositionalRandomFactory> forkPositional() override;
    void setSeed(int64_t seed) override;
    int32_t nextInt() override;
    int32_t nextInt(int32_t bound) override;
    int64_t nextLong() override;
    bool nextBoolean() override;
    float nextFloat() override;
    double nextDouble() override;
    double nextGaussian() override;
    int32_t next(int32_t bits);

private:
    uint64_t m_seed = 0;
    double m_nextNextGaussian = 0.0;
    bool m_haveNextNextGaussian = false;

    void resetGaussian();
};

struct Seed128bit {
    int64_t seedLo = 0;
    int64_t seedHi = 0;

    Seed128bit xorWith(int64_t lo, int64_t hi) const;
    Seed128bit xorWith(const Seed128bit& other) const;
    Seed128bit mixed() const;
};

namespace RandomSupport {
    static constexpr int64_t GOLDEN_RATIO_64 = -7046029254386353131LL;
    static constexpr int64_t SILVER_RATIO_64 = 7640891576956012809LL;

    int64_t mixStafford13(int64_t z);
    Seed128bit upgradeSeedTo128bitUnmixed(int64_t legacySeed);
    Seed128bit upgradeSeedTo128bit(int64_t legacySeed);
    Seed128bit seedFromHashOf(const std::string& input);
}

class Xoroshiro128PlusPlus final {
public:
    explicit Xoroshiro128PlusPlus(const Seed128bit& seed);
    Xoroshiro128PlusPlus(int64_t seedLo, int64_t seedHi);

    int64_t nextLong();

private:
    uint64_t m_seedLo = 0;
    uint64_t m_seedHi = 0;
};

class XoroshiroRandomSource final : public RandomSource {
public:
    explicit XoroshiroRandomSource(int64_t seed);
    explicit XoroshiroRandomSource(const Seed128bit& seed);
    XoroshiroRandomSource(int64_t seedLo, int64_t seedHi);

    std::shared_ptr<RandomSource> fork() override;
    std::shared_ptr<PositionalRandomFactory> forkPositional() override;
    void setSeed(int64_t seed) override;
    int32_t nextInt() override;
    int32_t nextInt(int32_t bound) override;
    int64_t nextLong() override;
    bool nextBoolean() override;
    float nextFloat() override;
    double nextDouble() override;
    double nextGaussian() override;
    void consumeCount(int32_t rounds) override;

private:
    Xoroshiro128PlusPlus m_randomNumberGenerator;
    double m_nextNextGaussian = 0.0;
    bool m_haveNextNextGaussian = false;

    uint64_t nextBits(int32_t bits);
    void resetGaussian();
};

int64_t getMthSeed(int32_t x, int32_t y, int32_t z);
int32_t javaStringHashCode(const std::string& value);

} // namespace mc::levelgen
