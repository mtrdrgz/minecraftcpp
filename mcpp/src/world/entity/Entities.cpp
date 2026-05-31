#include "Entities.h"
#include "player/Player.h"
#include "Mob.h"
#include "ai/RandomStrollGoal.h"

namespace mc {

std::unordered_map<int32_t, std::unique_ptr<Entity>> g_entities;

Entity* spawnEntity(int32_t id, EntityType type) {
    std::unique_ptr<Entity> e;
    if (type == EntityType::PLAYER) {
        e = std::make_unique<Player>(id);
    } else {
        // Most common mobs for now
        auto mob = std::make_unique<Mob>(id, type);
        // Add default AI (Random stroll is common for most mobs)
        mob->goalSelector().addGoal(8, std::make_unique<RandomStrollGoal>(mob.get(), 1.0));
        e = std::move(mob);
    }
    Entity* p = e.get();
    g_entities[id] = std::move(e);
    return p;
}

Entity* getEntity(int32_t id) {
    auto it = g_entities.find(id);
    return (it == g_entities.end()) ? nullptr : it->second.get();
}

void removeEntity(int32_t id) {
    g_entities.erase(id);
}

void clearEntities() {
    g_entities.clear();
}

} // namespace mc
