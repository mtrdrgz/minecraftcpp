// Bit-exact 1:1 C++ port of com.mojang.math.Divisor (Minecraft 26.1.2).
//
// Source: 26.1.2/src/com/mojang/math/Divisor.java
//
// Divisor is an int iterator (implements it.unimi.dsi.fastutil.ints.IntIterator)
// that splits a `numerator` total across `denominator` segments, yielding the
// per-segment size from each nextInt() so that the sizes sum to `numerator` and
// stay as evenly distributed as possible. Used by the model/face-bakery code to
// distribute pixels/quads across a span.
//
// Pure integer arithmetic — no registries/world/components/network. Java int
// division and remainder truncate toward zero, which matches C++ for the
// signed-int operands used here (denominator>0 path), so `/` and `%` translate
// verbatim.
//
// Verbatim Java reference:
//
//   public Divisor(final int numerator, final int denominator) {
//      this.denominator = denominator;
//      if (denominator > 0) {
//         this.quotient = numerator / denominator;
//         this.mod = numerator % denominator;
//      } else {
//         this.quotient = 0;
//         this.mod = 0;
//      }
//   }
//   public boolean hasNext() { return this.returnedParts < this.denominator; }
//   public int nextInt() {
//      if (!this.hasNext()) throw new NoSuchElementException();
//      int next = this.quotient;
//      this.remainder = this.remainder + this.mod;
//      if (this.remainder >= this.denominator) {
//         this.remainder = this.remainder - this.denominator;
//         next++;
//      }
//      this.returnedParts++;
//      return next;
//   }

#pragma once

#include <cstdint>
#include <stdexcept>

namespace mc::render::model {

class Divisor {
public:
    // Mirrors `new Divisor(numerator, denominator)`. All fields are 32-bit int
    // to match Java `int` two's-complement semantics exactly.
    Divisor(int32_t numerator, int32_t denominator)
        : denominator_(denominator) {
        if (denominator > 0) {
            quotient_ = numerator / denominator;
            mod_ = numerator % denominator;
        } else {
            quotient_ = 0;
            mod_ = 0;
        }
    }

    // boolean hasNext()
    bool hasNext() const {
        return returnedParts_ < denominator_;
    }

    // int nextInt() — throws std::out_of_range to mirror NoSuchElementException
    // (the Java method throws when exhausted; callers gate on hasNext()).
    int32_t nextInt() {
        if (!hasNext()) {
            throw std::out_of_range("Divisor::nextInt: no such element");
        }
        int32_t next = quotient_;
        remainder_ = remainder_ + mod_;
        if (remainder_ >= denominator_) {
            remainder_ = remainder_ - denominator_;
            next++;
        }
        returnedParts_++;
        return next;
    }

private:
    const int32_t denominator_;
    int32_t quotient_ = 0;   // set in constructor
    int32_t mod_ = 0;        // set in constructor
    int32_t returnedParts_ = 0;
    int32_t remainder_ = 0;
};

} // namespace mc::render::model
