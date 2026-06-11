#pragma once

// 1:1 port of the PURE static slot/redstone math from
// net.minecraft.world.inventory.AbstractContainerMenu (26.1.2).
//
// Every function here is a self-contained static helper on AbstractContainerMenu
// that touches NO menu/world/registry state — only int/float arithmetic on its
// arguments. We deliberately port ONLY this pure surface; the stateful menu
// (slots[], quickcraftSlots, carried, ContainerLevelAccess, ContainerSynchronizer,
// clicked(), doClick(), moveItemStackTo(), ...) is NOT ported here.
//
// Ported helpers (AbstractContainerMenu.java line numbers in 26.1.2):
//   getQuickcraftType(int mask)              :702-704   mask >> 2 & 3
//   getQuickcraftHeader(int mask)            :706-708   mask & 3
//   getQuickcraftMask(int header, int type)  :710-712   header & 3 | (type & 3) << 2
//   getQuickCraftPlaceCount(size,type,stack) :734-741   switch over type; case 0 = Mth.floor((float)count/size)
//   getRedstoneSignalFromContainer(...)      :751-768   float % accumulation -> Mth.lerpDiscrete(p, 0, 15)
//
// The two skipped sibling statics depend on un-ported types and are NOT ported:
//   isValidQuickcraftType(int, Player)       — needs Player.hasInfiniteMaterials()
//   canItemQuickReplace(Slot, ItemStack, b)  — needs Slot / ItemStack
//   getRedstoneSignalFromBlockEntity(BE)     — needs BlockEntity / instanceof Container
//
// getRedstoneSignalFromContainer is modeled abstractly: a real Container's loop reads,
// per slot, itemStack.getCount() and container.getMaxStackSize(itemStack) =
// min(container.getMaxStackSize(), itemStack.getMaxStackSize())  (Container.java:38-40).
// Empty stacks (count 0) are skipped exactly as `!itemStack.isEmpty()`. We pass that
// already-resolved per-slot (count, effectiveMax) pair plus the container size so the
// IEEE-754 float accumulation order is reproduced bit-for-bit. Container default
// getMaxStackSize() is 99 (Container.java:34-36); a stack's getMaxStackSize() is its
// MAX_STACK_SIZE component (typically 1, 16, or 64).
//
// All arithmetic composes the CERTIFIED engine primitives mc::levelgen::mth::floor and
// mc::levelgen::mth::lerpDiscrete (bit-exact ports of net.minecraft.util.Mth). The
// float division and accumulation are kept in `float` precision exactly as Java does.
//
// Verified bit-for-bit against the REAL class by abstract_container_math_parity
// (tools/AbstractContainerMenuMathParity.java).

#include <cstdint>
#include <vector>

#include "world/level/levelgen/Mth.h"

namespace mc::world::inventory {

// AbstractContainerMenu.java:702-704 — public static int getQuickcraftType(int mask)
//   return mask >> 2 & 3;
// Java `>>` is arithmetic (sign-propagating) shift on a 32-bit int; we operate on
// int32_t so a negative mask sign-extends exactly as in Java before the &3 mask.
inline int getQuickcraftType(int32_t mask) {
    return (mask >> 2) & 3;
}

// AbstractContainerMenu.java:706-708 — public static int getQuickcraftHeader(int mask)
//   return mask & 3;
inline int getQuickcraftHeader(int32_t mask) {
    return mask & 3;
}

// AbstractContainerMenu.java:710-712 — public static int getQuickcraftMask(int header, int type)
//   return header & 3 | (type & 3) << 2;
// Java operator precedence: << is higher than &, which is higher than |, so this
// parses as  (header & 3) | ((type & 3) << 2). Reproduced explicitly.
inline int getQuickcraftMask(int32_t header, int32_t type) {
    return (header & 3) | ((type & 3) << 2);
}

// AbstractContainerMenu.java:734-741 — public static int getQuickCraftPlaceCount(
//     int quickCraftSlotsSize, int quickCraftingType, ItemStack itemStack)
//   return switch (quickCraftingType) {
//      case 0 -> Mth.floor((float)itemStack.getCount() / quickCraftSlotsSize);
//      case 1 -> 1;
//      case 2 -> itemStack.getMaxStackSize();
//      default -> itemStack.getCount();
//   };
// itemStack is reduced to its two read fields: getCount() and getMaxStackSize().
// case 0 is the trap: integer count is widened to float, divided by the int size
// (which widens to float too), then Mth.floor = (int)Math.floor(double). We keep the
// division in float precision exactly as Java's `(float)count / size` expression does.
inline int getQuickCraftPlaceCount(int32_t quickCraftSlotsSize, int32_t quickCraftingType,
                                   int32_t itemCount, int32_t itemMaxStackSize) {
    switch (quickCraftingType) {
        case 0:
            return mc::levelgen::mth::floor(static_cast<float>(itemCount) /
                                            static_cast<float>(quickCraftSlotsSize));
        case 1:
            return 1;
        case 2:
            return itemMaxStackSize;
        default:
            return itemCount;
    }
}

// One resolved container slot for the redstone-signal computation:
//   count          = itemStack.getCount()                       (0 means empty -> skipped)
//   effectiveMax   = container.getMaxStackSize(itemStack)
//                  = min(container.getMaxStackSize(), itemStack.getMaxStackSize())
struct RedstoneSlot {
    int32_t count;
    int32_t effectiveMax;
};

// AbstractContainerMenu.java:751-768 — public static int getRedstoneSignalFromContainer(Container)
//   float totalPercent = 0.0F;
//   for (int i = 0; i < container.getContainerSize(); i++) {
//      ItemStack itemStack = container.getItem(i);
//      if (!itemStack.isEmpty()) {
//         totalPercent += (float)itemStack.getCount() / container.getMaxStackSize(itemStack);
//      }
//   }
//   totalPercent /= container.getContainerSize();
//   return Mth.lerpDiscrete(totalPercent, 0, 15);
//
// `slots` holds one RedstoneSlot per container index (size == container size). A null
// container returns 0 in Java; the caller models that as containerSize 0 -> handled below
// (an all-empty container divides 0.0F by size, yielding 0.0F -> lerpDiscrete -> 0).
// Every operation is float; the accumulation order is preserved by iterating in slot order.
inline int getRedstoneSignalFromContainer(const std::vector<RedstoneSlot>& slots) {
    const int32_t containerSize = static_cast<int32_t>(slots.size());
    if (containerSize == 0) {
        // Mirrors the `container == null -> return 0` guard; also avoids the /0 that
        // a genuinely zero-size Container can never reach (real containers are size>=1).
        return 0;
    }
    float totalPercent = 0.0F;
    for (const RedstoneSlot& s : slots) {
        if (s.count != 0) {  // !itemStack.isEmpty()
            totalPercent += static_cast<float>(s.count) / static_cast<float>(s.effectiveMax);
        }
    }
    totalPercent /= static_cast<float>(containerSize);
    return mc::levelgen::mth::lerpDiscrete(totalPercent, 0, 15);
}

}  // namespace mc::world::inventory
