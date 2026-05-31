#pragma once

#include "../item/Item.h"
#include <vector>

namespace mc {

class Player;

/**
 * Port of net.minecraft.world.Container
 */
class Container {
public:
    virtual ~Container() = default;

    virtual int getContainerSize() const = 0;
    virtual bool isEmpty() const = 0;
    virtual ItemStack getItem(int slot) const = 0;
    virtual ItemStack removeItem(int slot, int amount) = 0;
    virtual ItemStack removeItemNoUpdate(int slot) = 0;
    virtual void setItem(int slot, ItemStack stack) = 0;
    
    virtual int getMaxStackSize() const {
        return 64;
    }

    virtual void setChanged() = 0;
    virtual bool stillValid(Player& player) const = 0;
    virtual void clearContent() = 0;

    virtual bool canPlaceItem(int slot, ItemStack stack) {
        return true;
    }

    virtual int countItem(Item* item) const {
        int count = 0;
        for (int i = 0; i < getContainerSize(); ++i) {
            ItemStack stack = getItem(i);
            if (stack.item == item) {
                count += stack.count;
            }
        }
        return count;
    }

    virtual bool hasAnyOf(const std::vector<Item*>& items) const {
        for (int i = 0; i < getContainerSize(); ++i) {
            ItemStack stack = getItem(i);
            if (!stack.isEmpty()) {
                for (Item* item : items) {
                    if (stack.item == item) return true;
                }
            }
        }
        return false;
    }
};

} // namespace mc
