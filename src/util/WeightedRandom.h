#pragma once

// 1:1 port of net.minecraft.util.random.WeightedRandom (26.1.2) and the two
// selector strategies inside net.minecraft.util.random.WeightedList
// (Flat for totalWeight < 64, Compact otherwise).
//
// This header is a fresh, thin port: no prior C++ port of WeightedRandom /
// WeightedList existed under mcpp/src (verified by Grep). It deliberately stays
// generic over the element type so it can later be reused by loot tables,
// trial spawners, etc. The RandomSource-driven entry points take the certified
// mc::levelgen::RandomSource (world/level/levelgen/RandomSource.h) so seeding is
// byte-identical to net.minecraft.util.RandomSource.
//
// Source (WeightedRandom.java):
//   getTotalWeight:  sum weights as long; throw if > 2147483647; return (int).
//   getRandomItem:   if totalWeight<0 throw; if ==0 empty;
//                    selection = random.nextInt(totalWeight);
//                    return getWeightedItem(items, selection).
//   getWeightedItem: for each item: index -= weight; if (index < 0) return item.
//                    else Optional.empty.
// Source (WeightedList.java):
//   selector chosen at build time:
//     totalWeight == 0 -> no selector (empty)
//     totalWeight < 64 -> Flat: expand entries[totalWeight] by value, get =
//                         entries[selection].
//     else             -> Compact: same subtract loop as getWeightedItem, but
//                         throws IllegalStateException if it runs off the end.
//   getRandom / getRandomOrThrow: selection = random.nextInt(totalWeight);
//                         selector.get(selection).
//   FLAT_THRESHOLD = 64.

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "world/level/levelgen/RandomSource.h"

namespace mc::util {

// FLAT_THRESHOLD from WeightedList.java.
inline constexpr int FLAT_THRESHOLD = 64;

// WeightedRandom.getTotalWeight: long accumulation, overflow guard at INT_MAX,
// narrow to int. weights may individually be >= 0 (Weighted enforces weight>=0).
inline int getTotalWeight(const std::vector<int>& weights) {
    int64_t totalWeight = 0;
    for (int w : weights) {
        totalWeight += static_cast<int64_t>(w);
    }
    if (totalWeight > 2147483647LL) {
        throw std::invalid_argument("Sum of weights must be <= 2147483647");
    }
    return static_cast<int>(totalWeight);
}

// WeightedRandom.getWeightedItem: returns the selected index, or empty.
// `index` is the rolled weight in [0, totalWeight). Java returns Optional<T>;
// here we return the matching list index.
inline std::optional<int> getWeightedItem(const std::vector<int>& weights, int index) {
    for (int i = 0; i < static_cast<int>(weights.size()); ++i) {
        index -= weights[i];
        if (index < 0) {
            return i;
        }
    }
    return std::nullopt;
}

// WeightedRandom.getRandomItem(random, items, totalWeight, weightGetter).
inline std::optional<int> getRandomItem(mc::levelgen::RandomSource& random,
                                        const std::vector<int>& weights,
                                        int totalWeight) {
    if (totalWeight < 0) {
        throw std::invalid_argument("Negative total weight in getRandomItem");
    }
    if (totalWeight == 0) {
        return std::nullopt;
    }
    int selection = random.nextInt(totalWeight);
    return getWeightedItem(weights, selection);
}

// WeightedRandom.getRandomItem(random, items, weightGetter): computes total.
inline std::optional<int> getRandomItem(mc::levelgen::RandomSource& random,
                                        const std::vector<int>& weights) {
    return getRandomItem(random, weights, getTotalWeight(weights));
}

// WeightedList: picks the selector strategy by totalWeight, then resolves an
// in-range selection to a list index. Mirrors WeightedList.Selector::get.
//
// build()-time decision: totalWeight==0 -> empty (no selector); <64 -> Flat;
// else Compact. get(selection) is only ever called with selection in
// [0, totalWeight) (random.nextInt(totalWeight)), so both paths are total.
class WeightedList {
public:
    explicit WeightedList(std::vector<int> weights)
        : m_weights(std::move(weights)),
          m_totalWeight(getTotalWeight(m_weights)) {
        if (m_totalWeight == 0) {
            m_kind = Kind::Empty;
        } else if (m_totalWeight < FLAT_THRESHOLD) {
            m_kind = Kind::Flat;
            // WeightedList.Flat: entries[totalWeight], fill [i, i+weight) with
            // this entry's index (Arrays.fill of the value).
            m_flat.resize(static_cast<size_t>(m_totalWeight));
            int i = 0;
            for (int idx = 0; idx < static_cast<int>(m_weights.size()); ++idx) {
                int weight = m_weights[idx];
                for (int j = 0; j < weight; ++j) {
                    m_flat[static_cast<size_t>(i + j)] = idx;
                }
                i += weight;
            }
        } else {
            m_kind = Kind::Compact;
        }
    }

    bool isEmpty() const { return m_kind == Kind::Empty; }
    int totalWeight() const { return m_totalWeight; }

    // WeightedList.Selector.get(selection) for the chosen strategy. Returns the
    // selected entry index. Caller must pass selection in [0, totalWeight).
    int get(int selection) const {
        switch (m_kind) {
            case Kind::Flat:
                // Flat.get: direct array index.
                return m_flat[static_cast<size_t>(selection)];
            case Kind::Compact: {
                // Compact.get: subtract weights until < 0, else throw.
                for (int idx = 0; idx < static_cast<int>(m_weights.size()); ++idx) {
                    selection -= m_weights[idx];
                    if (selection < 0) {
                        return idx;
                    }
                }
                throw std::logic_error("selection exceeded total weight");
            }
            case Kind::Empty:
            default:
                throw std::logic_error("Weighted list has no elements");
        }
    }

    // WeightedList.getRandom: empty -> nullopt; else nextInt(totalWeight).
    std::optional<int> getRandom(mc::levelgen::RandomSource& random) const {
        if (m_kind == Kind::Empty) {
            return std::nullopt;
        }
        int selection = random.nextInt(m_totalWeight);
        return get(selection);
    }

    // WeightedList.getRandomOrThrow.
    int getRandomOrThrow(mc::levelgen::RandomSource& random) const {
        if (m_kind == Kind::Empty) {
            throw std::logic_error("Weighted list has no elements");
        }
        int selection = random.nextInt(m_totalWeight);
        return get(selection);
    }

private:
    enum class Kind { Empty, Flat, Compact };

    std::vector<int> m_weights;
    int m_totalWeight = 0;
    Kind m_kind = Kind::Empty;
    std::vector<int> m_flat;  // only populated for Kind::Flat
};

}  // namespace mc::util
