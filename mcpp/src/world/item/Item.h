#pragma once
#include "../../core/ResourceLocation.h"
#include <cstdint>
#include <string>

namespace mc {

class Block; // forward-decl — BlockItem will hold a Block* once ported

// Port of net.minecraft.world.item.Item
//
// The Java Item class is enormous (DataComponents, FeatureFlags, behavior hooks,
// etc.). For Phase 10/11 we only need enough to feed the hotbar / inventory
// rendering: a registry-name, a max stack size, a max damage value, and the
// fire-resistant flag. Everything else lives in the Properties struct and
// will grow as we port the matching Java systems.
class Item {
public:
    static constexpr int32_t DEFAULT_MAX_STACK_SIZE  = 64;   // Item.DEFAULT_MAX_STACK_SIZE
    static constexpr int32_t ABSOLUTE_MAX_STACK_SIZE = 99;   // Item.ABSOLUTE_MAX_STACK_SIZE

    struct Properties {
        int32_t     maxStackSize  = DEFAULT_MAX_STACK_SIZE;
        int32_t     maxDamage     = 0;       // 0 = unbreakable (Java: no DataComponents.MAX_DAMAGE)
        bool        fireResistant = false;   // Java: DataComponents.FIRE_RESISTANT marker
        std::string registryName;            // set by Items.cpp during registration
    };

    explicit Item(const Properties& props) : m_props(props) {}
    virtual ~Item() = default;

    const Properties&  properties()    const { return m_props; }
    const std::string& registryName()  const { return m_props.registryName; }
    int32_t            maxStackSize()  const { return m_props.maxStackSize; }
    int32_t            maxDamage()     const { return m_props.maxDamage; }
    bool               isFireResistant() const { return m_props.fireResistant; }
    bool               isDamageable()  const { return m_props.maxDamage > 0; }
    bool               isStackable()   const { return m_props.maxStackSize > 1 && !isDamageable(); }

    // Set by Registry<Item>::register_
    uint32_t registryId = 0;

protected:
    Properties m_props;
};

// Port of net.minecraft.world.item.ItemStack — bare-bones version.
//
// In Java, ItemStack is a heavy object with PatchedDataComponentMap, capability
// system, NBT tag, and a holder reference. For hotbar/inventory rendering we
// only need the item pointer, count, and damage. NBT/components are deferred
// until inventory packets are wired up.
struct ItemStack {
    Item*   item   = nullptr;   // nullptr or AIR = empty
    int32_t count  = 0;
    int32_t damage = 0;

    bool isEmpty() const { return !item || count <= 0; }

    static ItemStack empty() { return {nullptr, 0, 0}; }
};

} // namespace mc
