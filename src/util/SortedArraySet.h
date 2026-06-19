// 1:1 C++ port of net.minecraft.util.SortedArraySet (Minecraft 26.1.2).
//
// Java source: 26.1.2/src/net/minecraft/util/SortedArraySet.java
//              26.1.2/src/net/minecraft/util/Util.java   (growByHalf)
//
// SortedArraySet<T> is an AbstractSet backed by a *sorted* T[] (`contents`) plus a
// logical `size`. Membership/insertion uses java.util.Arrays.binarySearch over the
// active prefix [0,size); the array is grown via Util.growByHalf. Every observable
// behaviour reduces to:
//
//   * EXACT java.util.Arrays.binarySearch insertion-point semantics:
//       found    -> returns the match index (>= 0)
//       not found-> returns -(insertionPoint) - 1   (a negative number)
//     getInsertionPosition(position) = -position - 1 recovers the slot.
//
//   * EXACT capacity growth via Util.growByHalf(currentSize, minimalNewSize):
//       (int) Math.max(Math.min((long)currentSize + (currentSize >> 1), 2147483639L),
//                      minimalNewSize)
//     Traps: signed right shift `currentSize >> 1`, the whole computation done in
//     64-bit (long) so `currentSize + currentSize/2` never overflows mid-formula,
//     the hard clamp to 2147483639 (= Integer.MAX_VALUE - 8), then a narrowing
//     (int) cast of the long result.
//
//   * grow(capacity): only reallocates when capacity > contents.length. The
//     `contents != ObjectArrays.DEFAULT_EMPTY_ARRAY` branch is the live path for all
//     public factories (create(...) always allocates a fresh `new Object[n]`, never
//     the fastutil sentinel), so growth ALWAYS goes through growByHalf. The
//     `else if (capacity < 10) capacity = 10` branch is dead for these factories; it
//     is reproduced here behind an `isDefaultEmpty_` flag that is always false for a
//     set built via the modeled factories, so the port stays literally 1:1 with the
//     source without ever silently changing behaviour.
//
//   * add/addOrGet/remove/contains/get/first/last/size and exact ordered contents.
//
// We instantiate over T=int with natural (signed) ordering, which is the
// `create()` / Comparator.naturalOrder() flavour. The element type is irrelevant to
// the arithmetic; only the comparator's sign matters, and for int natural order it
// is plain signed compare — exactly what Integer.compareTo does. Nothing here is
// invented: constants and formulas are verbatim from the Java.

#ifndef MCPP_UTIL_SORTED_ARRAY_SET_H
#define MCPP_UTIL_SORTED_ARRAY_SET_H

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

namespace mc::util {

// ---------------------------------------------------------------------------
// net.minecraft.util.Util.growByHalf(int currentSize, int minimalNewSize):
//   return (int) Math.max(Math.min((long)currentSize + (currentSize >> 1),
//                                  2147483639L), minimalNewSize);
//
// All intermediate arithmetic is done in 64-bit (Java `long`) exactly as the
// source: (long)currentSize + (currentSize >> 1). `currentSize >> 1` is a signed
// 32-bit arithmetic shift performed in int, then widened. The result is min'd with
// 2147483639L, max'd with minimalNewSize (widened to long), then cast back to int.
// ---------------------------------------------------------------------------
inline int32_t growByHalf(int32_t currentSize, int32_t minimalNewSize) {
    // (currentSize >> 1): signed (arithmetic) shift in 32-bit int, per Java `>>`.
    int32_t halfInt = currentSize >> 1;
    int64_t sum = static_cast<int64_t>(currentSize) + static_cast<int64_t>(halfInt);
    int64_t capped = sum < 2147483639LL ? sum : 2147483639LL;   // Math.min(.., 2147483639L)
    int64_t minNew = static_cast<int64_t>(minimalNewSize);
    int64_t maxed = capped > minNew ? capped : minNew;           // Math.max(.., minimalNewSize)
    return static_cast<int32_t>(maxed);                          // (int) narrowing cast
}

// ---------------------------------------------------------------------------
// SortedArraySet<int> with natural (signed) ordering.
//
// `comparator` defaults to signed int compare returning sign of (a-b) like
// Integer.compareTo, but the constructor accepts an arbitrary std::function so the
// gate can exercise the comparator-driven binary search exactly as Java's
// Comparator overload does.
// ---------------------------------------------------------------------------
class SortedArraySetInt {
public:
    // Mirrors `new SortedArraySet<>(initialCapacity, comparator)` (private ctor) as
    // reached via create(initialCapacity)/create(comparator,initialCapacity). The
    // public no-arg create() uses initialCapacity = DEFAULT_INITIAL_CAPACITY (10).
    static constexpr int32_t DEFAULT_INITIAL_CAPACITY = 10;

    explicit SortedArraySetInt(int32_t initialCapacity = DEFAULT_INITIAL_CAPACITY,
                               std::function<int32_t(int32_t, int32_t)> comparator =
                                   naturalOrder())
        : comparator_(std::move(comparator)), size_(0) {
        if (initialCapacity < 0) {
            throw std::invalid_argument("Initial capacity (" +
                                        std::to_string(initialCapacity) +
                                        ") is negative");
        }
        // this.contents = castRawArray(new Object[initialCapacity]);
        contents_.assign(static_cast<size_t>(initialCapacity), 0);
        // A `new Object[n]` is never ObjectArrays.DEFAULT_EMPTY_ARRAY (the fastutil
        // sentinel) for any public factory, so the dead `< 10` grow branch is off.
        isDefaultEmpty_ = false;
    }

    // Comparator.naturalOrder() for Integer == Integer.compareTo == sign of (a-b),
    // computed without overflow.
    static std::function<int32_t(int32_t, int32_t)> naturalOrder() {
        return [](int32_t a, int32_t b) -> int32_t {
            return (a < b) ? -1 : (a > b) ? 1 : 0;
        };
    }

    // ----- private int findIndex(T t): Arrays.binarySearch(contents,0,size,t,cmp)
    // Returns the match index (>=0) if found, else -(insertionPoint)-1.
    int32_t findIndex(int32_t t) const {
        return binarySearch(0, size_, t);
    }

    // private static int getInsertionPosition(int position) { return -position - 1; }
    static int32_t getInsertionPosition(int32_t position) { return -position - 1; }

    // public boolean add(T t)
    bool add(int32_t t) {
        int32_t position = findIndex(t);
        if (position >= 0) {
            return false;
        }
        int32_t pos = getInsertionPosition(position);
        addInternal(t, pos);
        return true;
    }

    // public T addOrGet(T t)
    int32_t addOrGet(int32_t t) {
        int32_t position = findIndex(t);
        if (position >= 0) {
            return getInternal(position);
        }
        addInternal(t, getInsertionPosition(position));
        return t;
    }

    // public boolean remove(Object o)
    bool remove(int32_t o) {
        int32_t position = findIndex(o);
        if (position >= 0) {
            removeInternal(position);
            return true;
        }
        return false;
    }

    // public @Nullable T get(T t) -> models presence with a bool out-param.
    // Returns true and writes the stored value when found, false otherwise.
    bool get(int32_t t, int32_t& out) const {
        int32_t position = findIndex(t);
        if (position >= 0) {
            out = getInternal(position);
            return true;
        }
        return false;
    }

    // public boolean contains(Object o)
    bool contains(int32_t o) const { return findIndex(o) >= 0; }

    // public T first()  (UB / throws like Java getInternal(0) on empty)
    int32_t first() const { return getInternal(0); }

    // public T last()
    int32_t last() const { return getInternal(size_ - 1); }

    // public int size()
    int32_t size() const { return size_; }

    // public void clear()
    void clear() {
        // Arrays.fill(contents,0,size,null); size = 0;  (capacity unchanged)
        for (int32_t i = 0; i < size_; ++i) contents_[static_cast<size_t>(i)] = 0;
        size_ = 0;
    }

    // Current backing-array length (contents.length) — the capacity. Observable in
    // the gate via reflection on the Java side; here it is the vector's size.
    int32_t capacity() const { return static_cast<int32_t>(contents_.size()); }

    // Ordered active contents [0,size) as a snapshot (iteration order == array order).
    std::vector<int32_t> ordered() const {
        return std::vector<int32_t>(contents_.begin(), contents_.begin() + size_);
    }

private:
    // java.util.Arrays.binarySearch(a, fromIndex, toIndex, key, comparator).
    // VERBATIM JDK algorithm. Returns the match index, or -(low) - 1 when absent,
    // where `low` is the insertion point. Uses the unsigned-midpoint trick
    // (low + high) >>> 1 so it never overflows for in-range indices.
    int32_t binarySearch(int32_t fromIndex, int32_t toIndex, int32_t key) const {
        int32_t low = fromIndex;
        int32_t high = toIndex - 1;
        while (low <= high) {
            // (low + high) >>> 1  — logical right shift of the 32-bit sum.
            int32_t mid = static_cast<int32_t>(
                (static_cast<uint32_t>(low) + static_cast<uint32_t>(high)) >> 1);
            int32_t midVal = contents_[static_cast<size_t>(mid)];
            int32_t cmp = comparator_(midVal, key);
            if (cmp < 0) {
                low = mid + 1;
            } else if (cmp > 0) {
                high = mid - 1;
            } else {
                return mid;  // key found
            }
        }
        return -(low + 1);  // key not found.
    }

    // private void grow(int capacity)
    void grow(int32_t capacity) {
        if (capacity > static_cast<int32_t>(contents_.size())) {
            if (!isDefaultEmpty_) {
                capacity = growByHalf(static_cast<int32_t>(contents_.size()), capacity);
            } else if (capacity < 10) {
                capacity = 10;
            }
            // Object[] t = new Object[capacity]; arraycopy(contents,0,t,0,size);
            std::vector<int32_t> t(static_cast<size_t>(capacity), 0);
            for (int32_t i = 0; i < size_; ++i)
                t[static_cast<size_t>(i)] = contents_[static_cast<size_t>(i)];
            contents_ = std::move(t);
            isDefaultEmpty_ = false;  // contents is now a fresh allocation.
        }
    }

    // private void addInternal(T t, int pos)
    void addInternal(int32_t t, int32_t pos) {
        grow(size_ + 1);
        if (pos != size_) {
            // System.arraycopy(contents, pos, contents, pos+1, size - pos);
            for (int32_t i = size_; i > pos; --i)
                contents_[static_cast<size_t>(i)] = contents_[static_cast<size_t>(i - 1)];
        }
        contents_[static_cast<size_t>(pos)] = t;
        size_++;
    }

    // private void removeInternal(int position)
    void removeInternal(int32_t position) {
        size_--;
        if (position != size_) {
            // System.arraycopy(contents, position+1, contents, position, size - position);
            for (int32_t i = position; i < size_; ++i)
                contents_[static_cast<size_t>(i)] = contents_[static_cast<size_t>(i + 1)];
        }
        contents_[static_cast<size_t>(size_)] = 0;  // contents[size] = null
    }

    // private T getInternal(int position)
    int32_t getInternal(int32_t position) const {
        if (position < 0 || position >= size_)
            throw std::out_of_range("SortedArraySet getInternal index " +
                                    std::to_string(position));
        return contents_[static_cast<size_t>(position)];
    }

    std::function<int32_t(int32_t, int32_t)> comparator_;
    std::vector<int32_t> contents_;
    int32_t size_;
    bool isDefaultEmpty_;
};

}  // namespace mc::util

#endif  // MCPP_UTIL_SORTED_ARRAY_SET_H
