#pragma once

#include "Container.h"

namespace mc {

/**
 * Port of net.minecraft.world.inventory.Slot
 */
class Slot {
public:
    Slot(Container& container, int slotIndex, int x, int y);
    virtual ~Slot() = default;

    virtual void onQuickCraft(ItemStack oldStack, ItemStack newStack);
    virtual void onTake(Player& player, ItemStack stack);
    
    virtual bool mayPlace(ItemStack stack) const;
    virtual ItemStack getItem() const;
    virtual bool hasItem() const;
    virtual void set(ItemStack stack);
    virtual void setChanged();
    virtual int getMaxStackSize() const;
    virtual int getMaxStackSize(ItemStack stack) const;
    
    virtual ItemStack remove(int amount);
    
    virtual bool isActive() const;
    virtual bool mayPickup(Player& player) const;

    int getSlotIndex() const { return m_slotIndex; }
    int getX() const { return m_x; }
    int getY() const { return m_y; }

protected:
    Container& m_container;
    int m_slotIndex;
    int m_x;
    int m_y;
};

} // namespace mc
