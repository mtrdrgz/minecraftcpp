// 1:1 C++ port of net.minecraft.world.entity.animal.axolotl.Axolotl.Variant
// (Minecraft 26.1.2).
//
// Java source: 26.1.2/src/net/minecraft/world/entity/animal/axolotl/Axolotl.java
//   (lines 626-678, the `public enum Variant`).
//
// This is a small, fully self-contained gameplay/sim helper: pure integer + RNG
// math with no world/level access, no entity tick state, no registry/datapack
// lookups, and no GL. It comprises:
//
//   * the 5 variants with their (id, name, common) tuples
//   * getId() / getName()
//   * byId(int)  -> ByIdMap.continuous(getId, values(), OutOfBoundsStrategy.ZERO)
//   * getCommonSpawnVariant(random) -> Util.getRandom(commonVariants, random)
//   * getRareSpawnVariant(random)   -> Util.getRandom(rareVariants,   random)
//
// where getSpawnVariant(random, common) is:
//   Variant[] valid = Arrays.stream(values()).filter(v -> v.common == common)
//                          .toArray(Variant[]::new);
//   return Util.getRandom(valid, random);   // valid[random.nextInt(valid.length)]
//
// 1:1 traps reproduced verbatim:
//   * The `common` filter preserves *enum declaration order*. Common variants are
//     {LUCY(0), WILD(1), GOLD(2), CYAN(3)} -> a length-4 array; the rare set is
//     {BLUE(4)} -> a length-1 array. The chosen index is random.nextInt(len), so
//     getRareSpawnVariant always burns exactly one nextInt(1) call (which, in the
//     LegacyRandomSource power-of-two-bound path, still advances the RNG) and
//     always returns BLUE.
//   * byId uses the ZERO out-of-bounds strategy: in range -> values[id]; else
//     values[0] (== LUCY). NOT clamp, NOT wrap. Negative ids and ids >= 5 both
//     fall back to LUCY.
//   * getId()/getName()/the (id,name,common) table are reproduced verbatim.
//
// Nothing here is invented; every value/formula comes from the Java above plus
// the certified ByIdMap (mcpp/src/util/ByIdMap.h) index arithmetic and the
// certified LegacyRandomSource (mcpp/src/world/level/levelgen/RandomSource.h).

#ifndef MCPP_WORLD_ENTITY_ANIMAL_AXOLOTL_AXOLOTLVARIANT_H
#define MCPP_WORLD_ENTITY_ANIMAL_AXOLOTL_AXOLOTLVARIANT_H

#include "util/ByIdMap.h"
#include "world/level/levelgen/RandomSource.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mc::world::entity::animal::axolotl {

// The enum values, in declaration order. The ordinal is the array index here and
// happens to equal the id for every variant (LUCY=0 .. BLUE=4), but the code
// below never assumes id == ordinal: byId() goes through the ByIdMap continuous
// index arithmetic keyed on getId(), exactly as Java does.
enum class Variant : int32_t {
    LUCY = 0,
    WILD = 1,
    GOLD = 2,
    CYAN = 3,
    BLUE = 4,
};

namespace detail {

struct VariantInfo {
    Variant variant;
    int32_t id;
    const char* name;
    bool common;
};

// Verbatim from the Java enum declaration (id, name, common):
//   LUCY(0, "lucy", true), WILD(1, "wild", true), GOLD(2, "gold", true),
//   CYAN(3, "cyan", true), BLUE(4, "blue", false);
inline constexpr std::array<VariantInfo, 5> kVariants = {{
    {Variant::LUCY, 0, "lucy", true},
    {Variant::WILD, 1, "wild", true},
    {Variant::GOLD, 2, "gold", true},
    {Variant::CYAN, 3, "cyan", true},
    {Variant::BLUE, 4, "blue", false},
}};

inline const VariantInfo& info(Variant v) {
    // values() declaration order == kVariants order; ordinal indexes directly.
    return kVariants[static_cast<size_t>(static_cast<int32_t>(v))];
}

} // namespace detail

// Axolotl.Variant.getId()
inline int32_t getId(Variant v) { return detail::info(v).id; }

// Axolotl.Variant.getName() / getSerializedName() (identical in Java).
inline std::string getName(Variant v) { return std::string(detail::info(v).name); }

// Axolotl.Variant.byId(int):
//   BY_ID = ByIdMap.continuous(Variant::getId, values(), OutOfBoundsStrategy.ZERO)
// The continuous map builds a dense array indexed by id (ids 0..4 contiguous),
// and resolves out-of-range ids via the ZERO strategy: in range -> values[id];
// else values[0]. We compute the resolved index with the certified ByIdMap
// integer arithmetic, then map it back to the variant at that array slot.
inline Variant byId(int32_t id) {
    constexpr int32_t length = static_cast<int32_t>(detail::kVariants.size());
    const int32_t idx =
        mc::util::continuousIndex(id, length, mc::util::OutOfBoundsStrategy::ZERO);
    // The continuous array is sorted by id; for this enum id == array slot, but
    // resolve through the table to mirror Java exactly (values()[resolvedIndex]).
    return detail::kVariants[static_cast<size_t>(idx)].variant;
}

// The arrays produced by getSpawnVariant's filter, in enum declaration order.
// common == true  -> {LUCY, WILD, GOLD, CYAN}
// common == false -> {BLUE}
inline std::vector<Variant> filteredVariants(bool common) {
    std::vector<Variant> out;
    for (const auto& vi : detail::kVariants) {
        if (vi.common == common) {
            out.push_back(vi.variant);
        }
    }
    return out;
}

// Axolotl.Variant.getSpawnVariant(random, common):
//   Variant[] valid = ...filter(v -> v.common == common)...;
//   return Util.getRandom(valid, random);   // valid[random.nextInt(valid.length)]
inline Variant getSpawnVariant(mc::levelgen::RandomSource& random, bool common) {
    std::vector<Variant> valid = filteredVariants(common);
    const int32_t length = static_cast<int32_t>(valid.size());
    const int32_t index = random.nextInt(length);  // Util.getRandom
    return valid[static_cast<size_t>(index)];
}

// Axolotl.Variant.getCommonSpawnVariant(random)
inline Variant getCommonSpawnVariant(mc::levelgen::RandomSource& random) {
    return getSpawnVariant(random, true);
}

// Axolotl.Variant.getRareSpawnVariant(random)
inline Variant getRareSpawnVariant(mc::levelgen::RandomSource& random) {
    return getSpawnVariant(random, false);
}

} // namespace mc::world::entity::animal::axolotl

#endif  // MCPP_WORLD_ENTITY_ANIMAL_AXOLOTL_AXOLOTLVARIANT_H
