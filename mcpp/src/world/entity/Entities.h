#pragma once
#include "Entity.h"
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace mc {

// Global client-side entity table, keyed by network entity id (the int32_t
// sent in ADD_ENTITY / MOVE_ENTITY / REMOVE_ENTITIES packets).
//
// In Java this lives inside ClientLevel.entitiesById (an Int2ObjectMap). We
// keep it as a free-standing singleton until ClientLevel exists — matches how
// LevelChunk storage currently lives in Minecraft.cpp.
extern std::unordered_map<int32_t, std::unique_ptr<Entity>> g_entities;

// Create + register an entity. If an entity with this id already exists it is
// replaced (mirroring ClientPacketListener.handleAddEntity which removes the
// stale one first). Returns a non-owning pointer to the stored entity.
Entity* spawnEntity(int32_t id, EntityType type);

// Look up by id; nullptr if not present.
Entity* getEntity(int32_t id);

// Remove and destroy an entity by id. No-op if not present.
void    removeEntity(int32_t id);

// Drop every entity (called on dimension change / disconnect).
void    clearEntities();

} // namespace mc
