// PropertyValues.h — 1:1 C++ port of the value<->name handling of
//   net.minecraft.world.level.block.state.properties.{IntegerProperty,
//   BooleanProperty, EnumProperty}.
//
// Strict reverse-engineering of Minecraft Java Edition 26.1.2. Every formula,
// constant and ordering below is taken VERBATIM from the decompiled Java:
//   26.1.2/src/net/minecraft/world/level/block/state/properties/IntegerProperty.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/BooleanProperty.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/EnumProperty.java
//
// These properties are pure string<->value maps; no registries / Bootstrap.
//
// Scope of this port (matches the parity gate):
//   IntegerProperty: create(name,min,max) with the exact ctor validation,
//     getPossibleValues() == [min..max], getName(int)==Integer.toString,
//     getValue(String) (Integer.parseInt + in-range), getInternalIndex(int).
//   BooleanProperty: create(name), getName(bool), getValue(String),
//     getInternalIndex(bool), getPossibleValues()=={true,false}.
//   EnumProperty: generic over a StringRepresentable enum descriptor (caller
//     supplies the enum's (ordinal -> serializedName) table and the selected
//     subset, exactly as Java's EnumProperty.create(name,clazz[,filter/list])).
//     getName(enum)==getSerializedName, getValue(String) (name->value map),
//     getInternalIndex(enum)==ordinalToIndex, getPossibleValues()==values.
//
// NOTE: The Property base's Codec / equals / hashCode / toString machinery is
// out of scope for this value<->name gate and is intentionally NOT ported here.

#ifndef MCPP_WORLD_LEVEL_BLOCK_STATE_PROPERTIES_PROPERTYVALUES_H
#define MCPP_WORLD_LEVEL_BLOCK_STATE_PROPERTIES_PROPERTYVALUES_H

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc::block::state::properties {

// ---------------------------------------------------------------------------
// java.lang.Integer.parseInt(String, 10) — bit/semantics-exact replica.
//
// From java.lang.Integer.parseInt(CharSequence,int,int,int) / parseInt(String):
//   * empty string  -> NumberFormatException
//   * an optional leading '+' or '-' sign; a lone sign -> NumberFormatException
//   * every remaining char must be an ASCII digit (Character.digit in radix 10);
//     any non-digit -> NumberFormatException
//   * accumulation uses `result = -(magnitude)` with overflow checks against
//     Integer.MIN_VALUE, so the largest negative is -2147483648 and overflow
//     past Integer.MAX_VALUE / MIN_VALUE -> NumberFormatException
//
// IntegerProperty.getValue only KEEPS the parsed value if it lands in [min,max]
// (a subrange of valid ints), so overflow handling here only matters for it to
// THROW (-> Optional.empty) rather than silently wrap. We faithfully reproduce
// the throw-on-overflow / throw-on-bad-char behaviour and surface it as a
// std::nullopt-style "parse failed" via the bool* ok flag.
inline bool javaParseInt(const std::string& s, int32_t& out) {
    const std::size_t len = s.size();
    if (len == 0) return false;  // NumberFormatException: empty

    bool negative = false;
    std::size_t i = 0;
    // Java's limit/multmin: works in the negative space to capture MIN_VALUE.
    int32_t limit = -2147483647;  // -Integer.MAX_VALUE
    const char first = s[0];
    if (first < '0') {  // possible leading sign
        if (first == '-') {
            negative = true;
            limit = -2147483647 - 1;  // Integer.MIN_VALUE
        } else if (first != '+') {
            return false;  // NumberFormatException: bad leading char
        }
        if (len == 1) return false;  // NumberFormatException: lone "+"/"-"
        i = 1;
    }

    const int32_t multmin = limit / 10;  // radix 10
    int32_t result = 0;
    while (i < len) {
        const char c = s[i++];
        if (c < '0' || c > '9') return false;  // Character.digit < 0 -> NFE
        const int32_t digit = c - '0';
        // Overflow check (operating in the negative space).
        if (result < multmin) return false;  // NumberFormatException
        result *= 10;
        if (result < limit + digit) return false;  // NumberFormatException
        result -= digit;
    }
    out = negative ? result : -result;
    return true;
}

// ---------------------------------------------------------------------------
// IntegerProperty
class IntegerProperty {
public:
    // IntegerProperty.create(name,min,max) -> the private ctor's validation.
    static IntegerProperty create(const std::string& name, int32_t min, int32_t max) {
        if (min < 0) {
            throw std::invalid_argument("Min value of " + name + " must be 0 or greater");
        }
        if (max <= min) {
            throw std::invalid_argument("Max value of " + name + " must be greater than min (" +
                                        std::to_string(min) + ")");
        }
        return IntegerProperty(name, min, max);
    }

    // this.values = IntImmutableList.toList(IntStream.range(min, max + 1)) => [min..max].
    const std::vector<int32_t>& getPossibleValues() const { return values_; }

    // getName(Integer value) => value.toString().
    std::string getName(int32_t value) const { return std::to_string(value); }

    // getValue(String): Integer.parseInt, accept only if value in [min,max].
    std::optional<int32_t> getValue(const std::string& name) const {
        int32_t value;
        if (!javaParseInt(name, value)) return std::nullopt;  // NumberFormatException
        return (value >= min_ && value <= max_) ? std::optional<int32_t>(value) : std::nullopt;
    }

    // getInternalIndex(Integer value) => value <= max ? value - min : -1.
    int32_t getInternalIndex(int32_t value) const {
        return value <= max_ ? value - min_ : -1;
    }

    const std::string& name() const { return name_; }
    int32_t min() const { return min_; }
    int32_t max() const { return max_; }

private:
    IntegerProperty(const std::string& name, int32_t min, int32_t max)
        : name_(name), min_(min), max_(max) {
        values_.reserve(static_cast<std::size_t>(max - min) + 1);
        for (int32_t v = min; v <= max; ++v) values_.push_back(v);
    }
    std::string name_;
    int32_t min_;
    int32_t max_;
    std::vector<int32_t> values_;
};

// ---------------------------------------------------------------------------
// BooleanProperty
//   VALUES = List.of(true, false); TRUE_INDEX=0, FALSE_INDEX=1.
class BooleanProperty {
public:
    static BooleanProperty create(const std::string& name) { return BooleanProperty(name); }

    // getPossibleValues() => {true, false} in that order.
    const std::vector<bool>& getPossibleValues() const { return values_; }

    // getName(Boolean value) => value.toString() => "true"/"false".
    std::string getName(bool value) const { return value ? "true" : "false"; }

    // getValue(String): switch("true"->true, "false"->false, default->empty).
    std::optional<bool> getValue(const std::string& name) const {
        if (name == "true") return std::optional<bool>(true);
        if (name == "false") return std::optional<bool>(false);
        return std::nullopt;
    }

    // getInternalIndex(Boolean value) => value ? 0 : 1.
    int32_t getInternalIndex(bool value) const { return value ? 0 : 1; }

    const std::string& name() const { return name_; }

private:
    explicit BooleanProperty(const std::string& name)
        : name_(name), values_{true, false} {}
    std::string name_;
    std::vector<bool> values_;
};

// ---------------------------------------------------------------------------
// EnumProperty<T extends Enum & StringRepresentable>
//
// Java derives everything from clazz.getEnumConstants() (all ordinals) plus the
// selected `values` subset. We model T as an int ordinal; the caller provides:
//   allSerializedNames : (ordinal -> getSerializedName()) for ALL enum constants
//                        (clazz.getEnumConstants(), in ordinal order)
//   selectedOrdinals   : the chosen subset, in selection order (== Java `values`)
// This mirrors EnumProperty.create(name,clazz)            (all constants),
//             EnumProperty.create(name,clazz,filter)      (filtered subset),
//             EnumProperty.create(name,clazz,T... / List) (explicit subset).
class EnumProperty {
public:
    static EnumProperty create(const std::string& name,
                               const std::vector<std::string>& allSerializedNames,
                               const std::vector<int32_t>& selectedOrdinals) {
        return EnumProperty(name, allSerializedNames, selectedOrdinals);
    }

    // getPossibleValues() => this.values (the selected subset, in order).
    const std::vector<int32_t>& getPossibleValues() const { return values_; }

    // getName(T value) => value.getSerializedName().
    // value is the enum ordinal; we look up its serialized name.
    std::string getName(int32_t ordinal) const { return allNames_.at(static_cast<std::size_t>(ordinal)); }

    // getValue(String) => Optional.ofNullable(this.names.get(name)) — returns the
    // selected enum constant (its ordinal) whose serialized name == `name`.
    std::optional<int32_t> getValue(const std::string& name) const {
        auto it = names_.find(name);
        if (it == names_.end()) return std::nullopt;
        return std::optional<int32_t>(it->second);
    }

    // getInternalIndex(T value) => this.ordinalToIndex[value.ordinal()].
    int32_t getInternalIndex(int32_t ordinal) const {
        return ordinalToIndex_.at(static_cast<std::size_t>(ordinal));
    }

    const std::string& name() const { return name_; }

private:
    EnumProperty(const std::string& name,
                 const std::vector<std::string>& allSerializedNames,
                 const std::vector<int32_t>& selectedOrdinals)
        : name_(name), allNames_(allSerializedNames), values_(selectedOrdinals) {
        if (values_.empty()) {
            throw std::invalid_argument("Trying to make empty EnumProperty '" + name + "'");
        }
        // ordinalToIndex[value.ordinal()] = values.indexOf(value); for every
        // enum constant (List.indexOf returns -1 if not in the subset).
        ordinalToIndex_.assign(allNames_.size(), -1);
        for (std::size_t ord = 0; ord < allNames_.size(); ++ord) {
            int32_t idx = -1;
            for (std::size_t k = 0; k < values_.size(); ++k) {
                if (values_[k] == static_cast<int32_t>(ord)) { idx = static_cast<int32_t>(k); break; }
            }
            ordinalToIndex_[ord] = idx;
        }
        // names: ImmutableMap.builder().put(getSerializedName(), value)... buildOrThrow()
        // (duplicate serialized names among the subset would throw in Java).
        for (int32_t ord : values_) {
            const std::string& key = allNames_.at(static_cast<std::size_t>(ord));
            auto res = names_.emplace(key, ord);
            if (!res.second) {
                throw std::invalid_argument("Multiple entries with same key: " + key);
            }
        }
    }
    std::string name_;
    std::vector<std::string> allNames_;        // ordinal -> serialized name (ALL constants)
    std::vector<int32_t> values_;              // selected subset (ordinals), in order
    std::vector<int32_t> ordinalToIndex_;      // ordinal -> index in `values_`, or -1
    std::unordered_map<std::string, int32_t> names_;  // serialized name -> ordinal
};

}  // namespace mc::block::state::properties

#endif  // MCPP_WORLD_LEVEL_BLOCK_STATE_PROPERTIES_PROPERTYVALUES_H
