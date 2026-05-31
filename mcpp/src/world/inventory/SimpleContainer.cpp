#include "SimpleContainer.h"
#include <algorithm>

namespace mc {

SimpleContainer::SimpleContainer(int size) 
    : m_size(size), m_items(size, ItemStack::empty()) {}

int SimpleContainer::getContainerSize() const {
    return m_size;
}

bool SimpleContainer::isEmpty() const {
    for (const auto& stack : m_items) {
        if (!stack.isEmpty()) return false;
    }
    return true;
}

ItemStack SimpleContainer::getItem(int slot) const {
    if (slot >= 0 && slot < m_size) {
        return m_items[slot];
    }
    return ItemStack::empty();
}

ItemStack SimpleContainer::removeItem(int slot, int amount) {
    if (slot < 0 || slot >= m_size || m_items[slot].isEmpty() || amount <= 0) {
        return ItemStack::empty();
    }
    
    ItemStack& stack = m_items[slot];
    int toRemove = std::min(amount, stack.count);
    ItemStack result = {stack.item, toRemove, stack.damage};
    
    stack.count -= toRemove;
    if (stack.count <= 0) {
        stack = ItemStack::empty();
    }
    
    setChanged();
    return result;
}

ItemStack SimpleContainer::removeItemNoUpdate(int slot) {
    if (slot < 0 || slot >= m_size) {
        return ItemStack::empty();
    }
    
    ItemStack result = m_items[slot];
    m_items[slot] = ItemStack::empty();
    setChanged();
    return result;
}

void SimpleContainer::setItem(int slot, ItemStack stack) {
    if (slot >= 0 && slot < m_size) {
        m_items[slot] = stack;
        if (!stack.isEmpty() && stack.count > getMaxStackSize()) {
            m_items[slot].count = getMaxStackSize();
        }
        setChanged();
    }
}

void SimpleContainer::setChanged() {
    // Basic implementation doesn't have listeners yet.
}

bool SimpleContainer::stillValid(Player& player) const {
    return true;
}

void SimpleContainer::clearContent() {
    m_items.assign(m_size, ItemStack::empty());
    setChanged();
}

} // namespace mc
