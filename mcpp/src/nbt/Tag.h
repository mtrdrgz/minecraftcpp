#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <memory>
#include <optional>
#include <span>

namespace mc::nbt {

enum class TagType : uint8_t {
    End       = 0,
    Byte      = 1,
    Short     = 2,
    Int       = 3,
    Long      = 4,
    Float     = 5,
    Double    = 6,
    ByteArray = 7,
    String    = 8,
    List      = 9,
    Compound  = 10,
    IntArray  = 11,
    LongArray = 12,
};

struct NbtTag;

using NbtByteArray = std::vector<int8_t>;
using NbtIntArray  = std::vector<int32_t>;
using NbtLongArray = std::vector<int64_t>;

struct NbtList {
    TagType              elementType = TagType::End;
    std::vector<NbtTag>  elements;
};

struct NbtCompound {
    // INSERTION-ORDERED storage (CompoundTag semantics that matter for 1:1 byte
    // round-trips: Java reads keys into its map in file order and writes them back
    // in that same order for an identical insertion sequence, so read->write is
    // byte-stable. put() of an existing key replaces the value IN PLACE, keeping
    // the original position, like java.util.HashMap.put).
    std::vector<std::pair<std::string, NbtTag>> entries;
    std::unordered_map<std::string, std::size_t> index;

    bool        has(std::string_view key) const;
    NbtTag*     get(std::string_view key);
    const NbtTag* get(std::string_view key) const;
    void        put(std::string key, NbtTag tag);

    // Typed getters (return default if missing/wrong type)
    int8_t      getByte(std::string_view key, int8_t def = 0) const;
    int16_t     getShort(std::string_view key, int16_t def = 0) const;
    int32_t     getInt(std::string_view key, int32_t def = 0) const;
    int64_t     getLong(std::string_view key, int64_t def = 0) const;
    float       getFloat(std::string_view key, float def = 0.f) const;
    double      getDouble(std::string_view key, double def = 0.0) const;
    std::string getString(std::string_view key, std::string_view def = "") const;
    const NbtCompound* getCompound(std::string_view key) const;
          NbtCompound* getCompound(std::string_view key);
    const NbtList*     getList(std::string_view key) const;
    const NbtIntArray* getIntArray(std::string_view key) const;
    const NbtByteArray* getByteArray(std::string_view key) const;
    const NbtLongArray* getLongArray(std::string_view key) const;
};

// The main tag type — variant index intentionally NOT used as type ID
struct NbtTag {
    using Value = std::variant<
        std::monostate,              // End
        int8_t,                      // Byte
        int16_t,                     // Short
        int32_t,                     // Int
        int64_t,                     // Long
        float,                       // Float
        double,                      // Double
        NbtByteArray,                // ByteArray
        std::string,                 // String
        std::shared_ptr<NbtList>,    // List
        std::shared_ptr<NbtCompound>,// Compound
        NbtIntArray,                 // IntArray
        NbtLongArray                 // LongArray
    >;

    Value value;

    // Factory helpers matching Java's *Tag.valueOf()
    static NbtTag byte_(int8_t v)        { NbtTag t; t.value = v; return t; }
    static NbtTag short_(int16_t v)      { NbtTag t; t.value = v; return t; }
    static NbtTag int_(int32_t v)        { NbtTag t; t.value = v; return t; }
    static NbtTag long_(int64_t v)       { NbtTag t; t.value = v; return t; }
    static NbtTag float_(float v)        { NbtTag t; t.value = v; return t; }
    static NbtTag double_(double v)      { NbtTag t; t.value = v; return t; }
    static NbtTag string_(std::string v) { NbtTag t; t.value = std::move(v); return t; }
    static NbtTag compound(NbtCompound c) {
        NbtTag t;
        t.value = std::make_shared<NbtCompound>(std::move(c));
        return t;
    }
    static NbtTag list(NbtList l) {
        NbtTag t;
        t.value = std::make_shared<NbtList>(std::move(l));
        return t;
    }

    TagType type() const {
        return static_cast<TagType>(
            std::visit([](auto&& v) -> uint8_t {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>)   return 0;
                if constexpr (std::is_same_v<T, int8_t>)           return 1;
                if constexpr (std::is_same_v<T, int16_t>)          return 2;
                if constexpr (std::is_same_v<T, int32_t>)          return 3;
                if constexpr (std::is_same_v<T, int64_t>)          return 4;
                if constexpr (std::is_same_v<T, float>)            return 5;
                if constexpr (std::is_same_v<T, double>)           return 6;
                if constexpr (std::is_same_v<T, NbtByteArray>)     return 7;
                if constexpr (std::is_same_v<T, std::string>)      return 8;
                if constexpr (std::is_same_v<T, std::shared_ptr<NbtList>>)     return 9;
                if constexpr (std::is_same_v<T, std::shared_ptr<NbtCompound>>) return 10;
                if constexpr (std::is_same_v<T, NbtIntArray>)      return 11;
                if constexpr (std::is_same_v<T, NbtLongArray>)     return 12;
                return 0;
            }, value)
        );
    }

    template<class T> bool is() const { return std::holds_alternative<T>(value); }
    template<class T> const T* as() const { return std::get_if<T>(&value); }
    template<class T> T*       as()       { return std::get_if<T>(&value); }
};

} // namespace mc::nbt
