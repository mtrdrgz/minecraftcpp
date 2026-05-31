#include "Mob.h"

namespace mc {

Mob::Mob(int32_t id, EntityType type) : Entity(id, type) {
}

void Mob::tick() {
    Entity::tick();
    
    // AI ticks
    m_goalSelector.tick();
    m_targetSelector.tick();
}

} // namespace mc
