#pragma once

// 1:1 port of net.minecraft.util.CrudeIncrementalIntIdentityHashBiMap (the
// registry id<->object bimap used in network id mapping). Java source:
// 26.1.2/src/net/minecraft/util/CrudeIncrementalIntIdentityHashBiMap.java.
//
// Open-addressing, linear-probing int-identity bimap. add(value)->id,
// getId(value), byId(id), grow-on-load-factor (0.8), nextId monotone growth.
//
// ── The identity-hash externalization ────────────────────────────────────────
// Java's hash(key) is:
//     (Mth.murmurHash3Mixer(System.identityHashCode(key)) & 2147483647) % len
// System.identityHashCode is non-deterministic per object/per JVM run, so to make
// this driveable + bit-exact we port the *int-key path*: the caller supplies, for
// each key, the int that Java would feed into hash() — i.e. its identityHashCode.
// The map's structure (probe order, slot placement, grow timing, assigned ids)
// depends ONLY on those identity-hashes and key-identity equality, never on the
// stored value. So a key here is a small struct { id, identityHash }, where `id`
// is a unique opaque token giving reference-identity (`keys[i] == key`) and
// `identityHash` is exactly Java's System.identityHashCode(thatObject).
//
// murmurHash3Mixer is reused VERBATIM from the certified Mth.h (do not duplicate).

#include "world/level/levelgen/Mth.h"

#include <cstdint>
#include <vector>

namespace mc::util {

// A driven key: `id` gives reference-identity (Java `==`), `identityHash` is the
// value Java's System.identityHashCode(obj) returned for that object. id == -1 is
// the empty/null sentinel (Java's null key / EMPTY_SLOT).
struct IdKey {
    int id = -1;            // -1 == null (EMPTY_SLOT). Any >=0 is a distinct object.
    int identityHash = 0;   // System.identityHashCode(obj)

    bool isNull() const { return id == -1; }
    // Reference identity: Java compares with `==` (object identity). Two IdKeys are
    // the same reference iff they carry the same opaque id.
    bool sameRef(const IdKey& o) const { return id == o.id; }
};

inline const IdKey NULL_KEY{-1, 0};

class CrudeIncrementalIntIdentityHashBiMap {
public:
    static constexpr int NOT_FOUND = -1;

    // create(initialCapacity): new map((int)(initialCapacity / 0.8F)).
    // (int)(float) truncates toward zero; initialCapacity/0.8F >= 0 here so plain cast.
    static CrudeIncrementalIntIdentityHashBiMap create(int initialCapacity) {
        return CrudeIncrementalIntIdentityHashBiMap(
            static_cast<int>(static_cast<float>(initialCapacity) / 0.8F));
    }

    explicit CrudeIncrementalIntIdentityHashBiMap(int capacity)
        : keys_(capacity, NULL_KEY), values_(capacity, 0), byId_(capacity, NULL_KEY),
          nextId_(0), size_(0) {}

    int getId(const IdKey& thing) const {
        return getValue(indexOf(thing, hash(thing)));
    }

    // byId(id): id>=0 && id<byId.length ? byId[id] : null
    const IdKey& byId(int id) const {
        if (id >= 0 && id < static_cast<int>(byId_.size())) return byId_[id];
        return NULL_KEY;
    }

    bool contains(const IdKey& key) const { return getId(key) != NOT_FOUND; }
    bool contains(int id) const { return !byId(id).isNull(); }

    // add(key): id = nextId(); addMapping(key, id); return id.
    int add(const IdKey& key) {
        int value = nextId();
        addMapping(key, value);
        return value;
    }

    void addMapping(const IdKey& key, int id) {
        int minSize = id > (size_ + 1) ? id : (size_ + 1);  // Math.max(id, size+1)
        // minSize >= keys.length * 0.8F  (float comparison, exactly as Java)
        if (static_cast<float>(minSize) >= static_cast<float>(keys_.size()) * 0.8F) {
            int newSize = static_cast<int>(keys_.size()) << 1;
            while (newSize < id) newSize <<= 1;
            grow(newSize);
        }
        int index = findEmpty(hash(key));
        keys_[index] = key;
        values_[index] = id;
        byId_[id] = key;
        size_++;
        if (id == nextId_) nextId_++;
    }

    int size() const { return size_; }
    int nextIdField() const { return nextId_; }   // expose for parity dumping
    int capacity() const { return static_cast<int>(keys_.size()); }

    void clear() {
        std::fill(keys_.begin(), keys_.end(), NULL_KEY);
        std::fill(byId_.begin(), byId_.end(), NULL_KEY);
        nextId_ = 0;
        size_ = 0;
    }

private:
    std::vector<IdKey> keys_;
    std::vector<int>   values_;
    std::vector<IdKey> byId_;
    int nextId_;
    int size_;

    int getValue(int index) const {
        return index == -1 ? -1 : values_[index];
    }

    // nextId(): advance over occupied slots in byId, return first free.
    int nextId() {
        while (nextId_ < static_cast<int>(byId_.size()) && !byId_[nextId_].isNull()) {
            nextId_++;
        }
        return nextId_;
    }

    void grow(int newSize) {
        std::vector<IdKey> oldKeys = keys_;
        std::vector<int>   oldValues = values_;
        CrudeIncrementalIntIdentityHashBiMap resized(newSize);
        for (std::size_t i = 0; i < oldKeys.size(); ++i) {
            if (!oldKeys[i].isNull()) {
                resized.addMapping(oldKeys[i], oldValues[i]);
            }
        }
        keys_   = resized.keys_;
        values_ = resized.values_;
        byId_   = resized.byId_;
        nextId_ = resized.nextId_;
        size_   = resized.size_;
    }

    // hash(key): (murmurHash3Mixer(identityHashCode(key)) & 2147483647) % keys.length
    int hash(const IdKey& key) const {
        int mixed = mc::levelgen::mth::murmurHash3Mixer(key.identityHash);
        return (mixed & 2147483647) % static_cast<int>(keys_.size());
    }

    int indexOf(const IdKey& key, int startFrom) const {
        int len = static_cast<int>(keys_.size());
        for (int i = startFrom; i < len; i++) {
            if (keys_[i].sameRef(key)) return i;
            if (keys_[i].isNull()) return -1;           // EMPTY_SLOT
        }
        for (int i = 0; i < startFrom; i++) {
            if (keys_[i].sameRef(key)) return i;
            if (keys_[i].isNull()) return -1;
        }
        return -1;
    }

    int findEmpty(int startFrom) const {
        int len = static_cast<int>(keys_.size());
        for (int i = startFrom; i < len; i++) {
            if (keys_[i].isNull()) return i;
        }
        for (int i = 0; i < startFrom; i++) {
            if (keys_[i].isNull()) return i;
        }
        // Java throws RuntimeException("Overflowed :("); mirror with a sentinel that
        // will crash loudly if ever hit (load factor guarantees it never is).
        return -1;
    }
};

}  // namespace mc::util
