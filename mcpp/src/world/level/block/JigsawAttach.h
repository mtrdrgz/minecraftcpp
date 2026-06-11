#pragma once
// ---------------------------------------------------------------------------
// 1:1 port of the jigsaw ATTACHMENT predicate and Rotation.getShuffled used by
// the jigsaw structure-assembly placer (Minecraft Java Edition 26.1.2).
//
// Sources (translated verbatim, never invented):
//   net/minecraft/world/level/block/JigsawBlock.java
//       canAttach(StructureTemplate.JigsawBlockInfo source,
//                 StructureTemplate.JigsawBlockInfo target)  (JigsawBlock.java:81-89)
//       getFrontFacing(state) = state.getValue(ORIENTATION).front()  (:91-93)
//       getTopFacing(state)   = state.getValue(ORIENTATION).top()    (:95-97)
//   net/minecraft/core/FrontAndTop.java — front()/top() are the two Directions
//       carried per ORIENTATION constant (FrontAndTop.java:7-18,49-55).
//   net/minecraft/world/level/block/entity/JigsawBlockEntity.java — JointType
//       enum: ROLLABLE("rollable")=ordinal 0, ALIGNED("aligned")=ordinal 1
//       (JigsawBlockEntity.java:147-149).  NOTE: ROLLABLE is ordinal 0, ALIGNED 1.
//   net/minecraft/world/level/block/Rotation.java
//       getShuffled(random) = Util.shuffledCopy(values(), random)  (Rotation.java:116-118)
//       Rotation enum ordinals: NONE=0, CLOCKWISE_90=1, CLOCKWISE_180=2,
//       COUNTERCLOCKWISE_90=3 (Rotation.java:18-21).
//   net/minecraft/util/Util.java
//       shuffledCopy(T[] array, random) -> copy of array, then shuffle (Util.java:1049-1053)
//       shuffle(List, random): for (i = size; i > 1; i--) { swapTo = random.nextInt(i);
//           list.set(i-1, list.set(swapTo, list.get(i-1))); }  (Util.java:1061-1068)
//       i.e. Fisher-Yates from the top: for size 4 -> nextInt(4), nextInt(3), nextInt(2).
//
// REUSES (does not edit): world/phys/Direction.h (Direction + directionOpposite)
// and world/level/levelgen/RandomSource.h (LegacyRandomSource for the shuffle).
// ---------------------------------------------------------------------------

#include <array>
#include <string>

#include "world/level/levelgen/RandomSource.h"
#include "world/phys/Direction.h"

namespace mc::block {

// Java: net.minecraft.world.level.block.entity.JigsawBlockEntity.JointType.
// Enum declaration order is ROLLABLE, ALIGNED -> ordinals 0, 1
// (JigsawBlockEntity.java:147-149). Ordinals MUST match Java for parity TSVs.
enum class JointType : int32_t { ROLLABLE = 0, ALIGNED = 1 };

// Java: net.minecraft.world.level.block.Rotation — ordinal order
// NONE, CLOCKWISE_90, CLOCKWISE_180, COUNTERCLOCKWISE_90 (Rotation.java:18-21).
enum class Rotation : int32_t {
    NONE = 0,
    CLOCKWISE_90 = 1,
    CLOCKWISE_180 = 2,
    COUNTERCLOCKWISE_90 = 3,
};

// Rotation.values() order — the array Util.shuffledCopy copies from.
inline constexpr std::array<Rotation, 4> ROTATION_VALUES = {
    Rotation::NONE,
    Rotation::CLOCKWISE_90,
    Rotation::CLOCKWISE_180,
    Rotation::COUNTERCLOCKWISE_90,
};

// Java: JigsawBlock.canAttach(JigsawBlockInfo source, JigsawBlockInfo target)
// (JigsawBlock.java:81-89). The two JigsawBlockInfo records are modelled here by
// their attach-relevant projection:
//   sourceFront  = getFrontFacing(source.info().state())
//   sourceTop    = getTopFacing(source.info().state())
//   sourceTarget = source.target()   (Identifier)
//   sourceJoint  = source.jointType()
//   targetFront  = getFrontFacing(target.info().state())
//   targetTop    = getTopFacing(target.info().state())
//   targetName   = target.name()     (Identifier)
// Identifiers compare by equals() == byte-equal "namespace:path" strings.
inline bool canAttach(mc::Direction sourceFront,
                      mc::Direction sourceTop,
                      const std::string& sourceTarget,
                      JointType sourceJoint,
                      mc::Direction targetFront,
                      mc::Direction targetTop,
                      const std::string& targetName) {
    bool rollable = sourceJoint == JointType::ROLLABLE; // JigsawBlock.java:87
    return sourceFront == mc::directionOpposite(targetFront)        // JigsawBlock.java:88
           && (rollable || sourceTop == targetTop)
           && sourceTarget == targetName;
}

// Java: Rotation.getShuffled(random) = Util.shuffledCopy(values(), random)
// (Rotation.java:116-118 -> Util.java:1049-1068). Returns a List<Rotation>;
// we return the 4-element shuffle in order. The nextInt sequence is exactly
// nextInt(4), nextInt(3), nextInt(2) (Fisher-Yates from the top).
inline std::array<Rotation, 4> rotationGetShuffled(mc::levelgen::RandomSource& random) {
    // Util.shuffledCopy: ObjectArrayList copy = new ObjectArrayList(array)
    std::array<Rotation, 4> copy = ROTATION_VALUES;
    // Util.shuffle: for (int i = size; i > 1; i--) { swapTo = random.nextInt(i);
    //   list.set(i-1, list.set(swapTo, list.get(i-1))); }  (Util.java:1064-1067)
    const int size = static_cast<int>(copy.size());
    for (int i = size; i > 1; --i) {
        int swapTo = random.nextInt(i);
        // list.set(swapTo, list.get(i-1)) returns the OLD value at swapTo, which
        // is then stored at i-1 — i.e. a swap of indices (i-1) and swapTo.
        Rotation tmp = copy[swapTo];
        copy[swapTo] = copy[i - 1];
        copy[i - 1] = tmp;
    }
    return copy;
}

} // namespace mc::block
