// 1:1 C++ port of net.minecraft.world.entity.animal.panda.Panda.Gene
// (Minecraft 26.1.2). Pure, ungated, self-contained gameplay math: the panda
// gene enum and its three static helpers. No world/level access, no entity tick
// state, no registry/datapack lookups, no GL.
//
// Java source: 26.1.2/src/net/minecraft/world/entity/animal/panda/Panda.java
//   (enum Panda.Gene, lines ~702-765)
//
// The Gene enum (id, name, isRecessive):
//   NORMAL(0,"normal",false) LAZY(1,"lazy",false) WORRIED(2,"worried",false)
//   PLAYFUL(3,"playful",false) BROWN(4,"brown",true) WEAK(5,"weak",true)
//   AGGRESSIVE(6,"aggressive",false)
//
// Three static helpers ported verbatim:
//
//   byId(int id):
//     BY_ID = ByIdMap.continuous(Gene::getId, values(), OutOfBoundsStrategy.ZERO).
//     Ids are declared continuous 0..6 in enum order, so sortedValues[id] == the
//     gene with that id. ZERO strategy: in-range [0,7) -> values[id]; else NORMAL
//     (sortedValues[0]). Trap: negatives and >=7 fold to NORMAL, NOT clamp/wrap.
//
//   getVariantFromGenes(main, hidden):
//     if main.isRecessive(): return main == hidden ? main : NORMAL;
//     else:                  return main;
//     (Only BROWN and WEAK are recessive; a single recessive copy shows NORMAL.)
//
//   getRandom(int nextInt) where nextInt == random.nextInt(16):
//     0 -> LAZY; 1 -> WORRIED; 2 -> PLAYFUL; 4 -> AGGRESSIVE;
//     else if nextInt < 9  -> WEAK     (so 3,5,6,7,8 -> WEAK; note 3 falls through!)
//     else if nextInt < 11 -> BROWN    (9,10)
//     else                 -> NORMAL   (11..15)
//   The branch order matters: nextInt==3 is NOT special-cased, it reaches the
//   `< 9` arm and yields WEAK. This is the load-bearing 1:1 trap.
//
// Reuses the certified ZERO-strategy index arithmetic from mc::util (ByIdMap.h)
// so byId() shares one source of truth with the rest of the rebuild.

#ifndef MCPP_WORLD_ENTITY_ANIMAL_PANDA_PANDAGENE_H
#define MCPP_WORLD_ENTITY_ANIMAL_PANDA_PANDAGENE_H

#include <cstdint>
#include <string_view>

#include "util/ByIdMap.h"

namespace mc::world::entity::animal::panda {

// net.minecraft.world.entity.animal.panda.Panda.Gene — ordinal == id here
// (continuous 0..6, declared in id order).
enum class Gene : int32_t {
    NORMAL = 0,
    LAZY = 1,
    WORRIED = 2,
    PLAYFUL = 3,
    BROWN = 4,
    WEAK = 5,
    AGGRESSIVE = 6,
};

inline constexpr int32_t GENE_COUNT = 7;  // values().length; also MAX_GENE+1.

// Panda.Gene.getId()
inline constexpr int32_t getId(Gene g) { return static_cast<int32_t>(g); }

// Panda.Gene.getSerializedName()
inline constexpr std::string_view getSerializedName(Gene g) {
    switch (g) {
        case Gene::NORMAL: return "normal";
        case Gene::LAZY: return "lazy";
        case Gene::WORRIED: return "worried";
        case Gene::PLAYFUL: return "playful";
        case Gene::BROWN: return "brown";
        case Gene::WEAK: return "weak";
        case Gene::AGGRESSIVE: return "aggressive";
    }
    return "normal";  // unreachable
}

// Panda.Gene.isRecessive() — only BROWN and WEAK are recessive.
inline constexpr bool isRecessive(Gene g) {
    return g == Gene::BROWN || g == Gene::WEAK;
}

// Panda.Gene.byId(int) — ByIdMap.continuous(getId, values(), ZERO).
// sortedValues[index] is the gene whose id == index; for the continuous 0..6
// layout that is exactly Gene(index). continuousIndex() returns 0 (NORMAL) for
// any out-of-range id under the ZERO strategy.
inline Gene byId(int32_t id) {
    const int32_t index =
        mc::util::continuousIndex(id, GENE_COUNT, mc::util::OutOfBoundsStrategy::ZERO);
    return static_cast<Gene>(index);
}

// Panda.Gene.getVariantFromGenes(Gene main, Gene hidden)
inline constexpr Gene getVariantFromGenes(Gene mainGene, Gene hiddenGene) {
    if (isRecessive(mainGene)) {
        return mainGene == hiddenGene ? mainGene : Gene::NORMAL;
    }
    return mainGene;
}

// Panda.Gene.getRandom(RandomSource) with the single draw decoupled:
// `nextInt` is the value of random.nextInt(16). Branch order is verbatim.
inline constexpr Gene getRandomFromDraw(int32_t nextInt) {
    if (nextInt == 0) {
        return Gene::LAZY;
    } else if (nextInt == 1) {
        return Gene::WORRIED;
    } else if (nextInt == 2) {
        return Gene::PLAYFUL;
    } else if (nextInt == 4) {
        return Gene::AGGRESSIVE;
    } else if (nextInt < 9) {
        return Gene::WEAK;
    } else {
        return nextInt < 11 ? Gene::BROWN : Gene::NORMAL;
    }
}

}  // namespace mc::world::entity::animal::panda

#endif  // MCPP_WORLD_ENTITY_ANIMAL_PANDA_PANDAGENE_H
