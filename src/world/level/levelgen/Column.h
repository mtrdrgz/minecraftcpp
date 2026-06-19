// 1:1 port of net.minecraft.world.level.levelgen.Column (Minecraft 26.1.2).
//
// Pure integer column/ray range algebra used by worldgen scans. Only the PURE
// factory + accessor surface is ported here:
//
//   around(lowest, highest)        -> Range(lowest-1, highest+1)
//   inside(floor, ceiling)         -> Range(floor, ceiling)
//   below(ceiling)                 -> Ray(ceiling, false)
//   fromHighest(highest)           -> Ray(highest+1, false)
//   above(floor)                   -> Ray(floor, true)
//   fromLowest(lowest)             -> Ray(lowest-1, true)
//   line()                         -> Line.INSTANCE
//   create(floor?, ceiling?)       -> inside / above / below / line
//   withFloor / withCeiling        -> create(...)
//   getCeiling() / getFloor() / getHeight()  (java.util.OptionalInt)
//   Range::ceiling()/floor()/height(), Ray::edge()/pointingUp(), toString()
//
// The Java Range ctor throws IllegalArgumentException("Column of negative
// height: ...") when height() < 0; that exact guard is reproduced.
//
// NOT ported (world-coupled): scan(LevelSimulatedReader, ...) and the private
// scanDirection — they require a live level reader / block-state predicates.
// See tools/ColumnParity.java for the byte-exact ground truth.

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace mc::levelgen {

// Minimal stand-in for java.util.OptionalInt — present flag + int value, with
// the same semantics the Column accessors rely on (empty() vs of(int)).
struct OptionalInt {
    bool present = false;
    int  value   = 0;

    static OptionalInt empty() { return OptionalInt{false, 0}; }
    static OptionalInt of(int v) { return OptionalInt{true, v}; }

    bool isPresent() const { return present; }
    int  getAsInt()  const { return value; }

    bool operator==(const OptionalInt& o) const {
        // OptionalInt.equals: both empty, or both present with equal values.
        if (present != o.present) return false;
        return !present || value == o.value;
    }
    bool operator!=(const OptionalInt& o) const { return !(*this == o); }
};

// Column is an abstract base in Java with three final subclasses (Line, Range,
// Ray). We model the polymorphic accessors via a tagged union; the three
// factories below build each concrete kind exactly as the Java statics do.
class Column {
public:
    enum class Kind { Line, Range, Ray };

    // ---- pure factories (Column.java lines 12-48) -------------------------
    static Column around(int lowest, int highest) {
        return makeRange(addJ(lowest, -1), addJ(highest, 1));
    }
    static Column inside(int floor, int ceiling) {
        return makeRange(floor, ceiling);
    }
    static Column below(int ceiling) {
        return makeRay(ceiling, false);
    }
    static Column fromHighest(int highest) {
        return makeRay(addJ(highest, 1), false);
    }
    static Column above(int floor) {
        return makeRay(floor, true);
    }
    static Column fromLowest(int lowest) {
        return makeRay(addJ(lowest, -1), true);
    }
    static Column line() {
        Column c;
        c.kind_ = Kind::Line;
        return c;
    }

    // create(OptionalInt floor, OptionalInt ceiling) — Column.java 40-48.
    static Column create(const OptionalInt& floor, const OptionalInt& ceiling) {
        if (floor.isPresent() && ceiling.isPresent()) {
            return inside(floor.getAsInt(), ceiling.getAsInt());
        } else if (floor.isPresent()) {
            return above(floor.getAsInt());
        } else {
            return ceiling.isPresent() ? below(ceiling.getAsInt()) : line();
        }
    }

    // ---- polymorphic accessors -------------------------------------------
    OptionalInt getCeiling() const {
        switch (kind_) {
            case Kind::Line:  return OptionalInt::empty();
            case Kind::Range: return OptionalInt::of(ceiling_);
            case Kind::Ray:   return pointingUp_ ? OptionalInt::empty() : OptionalInt::of(edge_);
        }
        return OptionalInt::empty();
    }

    OptionalInt getFloor() const {
        switch (kind_) {
            case Kind::Line:  return OptionalInt::empty();
            case Kind::Range: return OptionalInt::of(floor_);
            case Kind::Ray:   return pointingUp_ ? OptionalInt::of(edge_) : OptionalInt::empty();
        }
        return OptionalInt::empty();
    }

    OptionalInt getHeight() const {
        switch (kind_) {
            case Kind::Line: return OptionalInt::empty();
            case Kind::Range: return OptionalInt::of(rangeHeight());
            case Kind::Ray:  return OptionalInt::empty();
        }
        return OptionalInt::empty();
    }

    // withFloor / withCeiling — Column.java 56-62.
    Column withFloor(const OptionalInt& floor) const {
        return create(floor, getCeiling());
    }
    Column withCeiling(const OptionalInt& ceiling) const {
        return create(getFloor(), ceiling);
    }

    // ---- Range-only accessors (only valid on Kind::Range) -----------------
    int ceiling() const { return ceiling_; }
    int floor() const { return floor_; }
    int height() const { return rangeHeight(); }

    // ---- Ray-only accessors (only valid on Kind::Ray) ---------------------
    int edge() const { return edge_; }
    bool pointingUp() const { return pointingUp_; }

    Kind kind() const { return kind_; }

    // toString() — Line.java 122-124, Range.java 166-169, Ray.java 196-199.
    std::string toString() const {
        switch (kind_) {
            case Kind::Line:
                return "C(-)";
            case Kind::Range:
                return "C(" + std::to_string(ceiling_) + "-" + std::to_string(floor_) + ")";
            case Kind::Ray:
                return pointingUp_ ? ("C(" + std::to_string(edge_) + "-)")
                                   : ("C(-" + std::to_string(edge_) + ")");
        }
        return "";
    }

private:
    Column() = default;

    // Java int addition with two's-complement wraparound (avoids signed-overflow
    // UB in C++ while reproducing Java's exact +1/-1 boundary arithmetic).
    static int addJ(int a, int b) {
        return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
    }

    // Range height(): ceiling - floor - 1 (two's-complement int wrap matches
    // Java) — Range.java line 162-164.
    int rangeHeight() const {
        return static_cast<int32_t>(static_cast<uint32_t>(ceiling_) -
                                    static_cast<uint32_t>(floor_) - 1u);
    }

    static Column makeRange(int floor, int ceiling) {
        Column c;
        c.kind_    = Kind::Range;
        c.floor_   = floor;
        c.ceiling_ = ceiling;
        // Range ctor guard — Column.java 134-136.
        if (c.rangeHeight() < 0) {
            throw std::invalid_argument("Column of negative height: " + c.toString());
        }
        return c;
    }

    static Column makeRay(int edge, bool pointingUp) {
        Column c;
        c.kind_       = Kind::Ray;
        c.edge_       = edge;
        c.pointingUp_ = pointingUp;
        return c;
    }

    Kind kind_ = Kind::Line;
    // Range fields.
    int floor_   = 0;
    int ceiling_ = 0;
    // Ray fields.
    int  edge_       = 0;
    bool pointingUp_ = false;
};

} // namespace mc::levelgen
