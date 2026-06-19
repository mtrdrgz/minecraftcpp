#include "Slot.h"
#include <algorithm>

namespace mc {

Slot::Slot(Container& container, int slotIndex, int x, int y)
    : m_container(container), m_slotIndex(slotIndex), m_x(x), m_y(y) {}

void Slot::onQuickCraft(ItemStack oldStack, ItemStack newStack) {
    // Java: default no-op or basic tracking
}

void Slot::onTake(Player& player, ItemStack stack) {
    setChanged();
}

bool Slot::mayPlace(ItemStack stack) const {
    return m_container.canPlaceItem(m_slotIndex, stack);
}

ItemStack Slot::getItem() const {
    return m_container.getItem(m_slotIndex);
}

bool Slot::hasItem() const {
    return !getItem().isEmpty();
}

void Slot::set(ItemStack stack) {
    m_container.setItem(m_slotIndex, stack);
    setChanged();
}

void Slot::setChanged() {
    m_container.setChanged();
}

int Slot::getMaxStackSize() const {
    return m_container.getMaxStackSize();
}

int Slot::getMaxStackSize(ItemStack stack) const {
    return std::min(getMaxStackSize(), stack.item ? stack.item->maxStackSize() : 64);
}

ItemStack Slot::remove(int amount) {
    return m_container.removeItem(m_slotIndex, amount);
}

bool Slot::isActive() const {
    return true;
}

bool Slot::mayPickup(Player& player) const {
    return true;
}

} // namespace mc
