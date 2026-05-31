#pragma once

#include "Container.h"
#include <vector>

namespace mc {

/**
 * Port of net.minecraft.world.SimpleContainer
 */
class SimpleContainer : public Container {
public:
    explicit SimpleContainer(int size);

    int getContainerSize() const override;
    bool isEmpty() const override;
    ItemStack getItem(int slot) const override;
    ItemStack removeItem(int slot, int amount) override;
    ItemStack removeItemNoUpdate(int slot) override;
    void setItem(int slot, ItemStack stack) override;
    
    void setChanged() override;
    bool stillValid(Player& player) const override;
    void clearContent() override;

private:
    int m_size;
    std::vector<ItemStack> m_items;
};

} // namespace mc
