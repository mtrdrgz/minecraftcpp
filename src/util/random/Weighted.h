#pragma once

// 1:1 port of the net.minecraft.util.random.Weighted<T> record (26.1.2).
//
// Source (Weighted.java, 26.1.2/src/net/minecraft/util/random/Weighted.java):
//   public record Weighted<T>(T value, int weight) {
//      public Weighted {                       // compact constructor
//         if (weight < 0) {
//            throw (IllegalArgumentException)Util.pauseInIde(
//                new IllegalArgumentException("Weight should be >= 0"));
//         }
//         if (weight == 0 && SharedConstants.IS_RUNNING_IN_IDE) {
//            LOGGER.warn("Found 0 weight, make sure this is intentional!");
//         }
//      }
//      public <U> Weighted<U> map(final Function<T,U> function) {
//         return new Weighted<>(function.apply(this.value()), this.weight);
//      }
//   }
//
// Notes on the port:
//   * Util.pauseInIde(t) (Util.java:747) just returns t (it only pauses when
//     IS_RUNNING_IN_IDE); so the observable behaviour is exactly "throw
//     IllegalArgumentException when weight < 0". We replicate that.
//   * The weight==0 branch only logs (and only in-IDE) — no behavioural change,
//     so it is intentionally not modelled here.
//   * value() / weight() are the record accessors. weight() returns the int as
//     stored (no normalisation).
//   * map() preserves the weight verbatim and transforms the value with the
//     supplied function — Weighted<U>(f(value), weight).
//   * No prior Weighted<T> port existed under mcpp/src (verified by Grep). The
//     selection algorithms (WeightedRandom / WeightedList Flat&Compact) are
//     ported separately and gated in util/WeightedRandom.h; this header is the
//     record itself, kept minimal and dependency-free.

#include <functional>
#include <stdexcept>
#include <utility>

namespace mc::util {

template <typename T>
class Weighted {
public:
    // Compact-constructor semantics: weight < 0 throws IllegalArgumentException.
    Weighted(T value, int weight)
        : m_value(std::move(value)), m_weight(weight) {
        if (weight < 0) {
            // Util.pauseInIde just returns the throwable; the net effect is a
            // thrown IllegalArgumentException("Weight should be >= 0").
            throw std::invalid_argument("Weight should be >= 0");
        }
    }

    const T& value() const { return m_value; }
    int weight() const { return m_weight; }

    // Weighted::map — transform value, preserve weight.
    template <typename U>
    Weighted<U> map(const std::function<U(const T&)>& function) const {
        return Weighted<U>(function(m_value), m_weight);
    }

    bool operator==(const Weighted& other) const {
        return m_weight == other.m_weight && m_value == other.m_value;
    }
    bool operator!=(const Weighted& other) const { return !(*this == other); }

private:
    T m_value;
    int m_weight;
};

}  // namespace mc::util
