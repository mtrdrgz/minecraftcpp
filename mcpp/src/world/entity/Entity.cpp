#include "Entity.h"

namespace mc {

Entity::Entity(int32_t id, EntityType t)
    : entityId(id), type(t), bbSize(defaultBBSize(t)) {}

void Entity::setPosition(double x, double y, double z) {
    position.x = x;
    position.y = y;
    position.z = z;
    positionCodecBase = position;
}

void Entity::setRotation(float newYaw, float newPitch) {
    yaw   = newYaw;
    pitch = newPitch;
}

bool Entity::getMetadataFlag(uint8_t entryId, int flagIndex) const {
    auto it = metadata.find(entryId);
    if (it == metadata.end()) return false;
    if (it->second.type != EntityMetadata::Type::BYTE) return false;
    uint8_t val = std::get<uint8_t>(it->second.value);
    return (val & (1 << flagIndex)) != 0;
}

uint8_t Entity::getMetadataByte(uint8_t entryId, uint8_t defaultValue) const {
    auto it = metadata.find(entryId);
    if (it == metadata.end()) return defaultValue;
    if (it->second.type != EntityMetadata::Type::BYTE) return defaultValue;
    return std::get<uint8_t>(it->second.value);
}

void Entity::tick() {
    // Basic walk animation (simplified)
    // In Java, this is driven by walkAnimationSpeed and walkAnimationPos in LivingEntity
    float dist = (float)std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    walkAnimSpeed = glm::clamp(dist * 5.0f, 0.0f, 1.0f);
    walkAnimPos += walkAnimSpeed;

    if (swingTime > 0) {
        swingTime--;
    }
}

glm::dvec3 Entity::getBBMin() const {
    const double halfW = bbSize.x * 0.5;
    return { position.x - halfW, position.y, position.z - halfW };
}

glm::dvec3 Entity::getBBMax() const {
    const double halfW = bbSize.x * 0.5;
    return { position.x + halfW, position.y + bbSize.y, position.z + halfW };
}

// Defaults below mirror the .sized(width, height) calls in EntityType.java
// (26.1.2/src/net/minecraft/world/entity/EntityType.java).
glm::vec3 Entity::defaultBBSize(EntityType t) {
    switch (t) {
        case EntityType::PLAYER:         return {0.6f,  1.8f,  0.6f};
        case EntityType::ZOMBIE:         return {0.6f,  1.95f, 0.6f};
        case EntityType::SKELETON:       return {0.6f,  1.99f, 0.6f};
        case EntityType::CREEPER:        return {0.6f,  1.7f,  0.6f};
        case EntityType::SPIDER:         return {1.4f,  0.9f,  1.4f};
        case EntityType::ENDERMAN:       return {0.6f,  2.9f,  0.6f};
        case EntityType::WITCH:          return {0.6f,  1.95f, 0.6f};
        case EntityType::SLIME:          return {2.04f, 2.04f, 2.04f}; // size=4 default
        case EntityType::BLAZE:          return {0.6f,  1.8f,  0.6f};
        case EntityType::GHAST:          return {4.0f,  4.0f,  4.0f};
        case EntityType::COW:            return {0.9f,  1.4f,  0.9f};
        case EntityType::PIG:            return {0.9f,  0.9f,  0.9f};
        case EntityType::SHEEP:          return {0.9f,  1.3f,  0.9f};
        case EntityType::CHICKEN:        return {0.4f,  0.7f,  0.4f};
        case EntityType::HORSE:          return {1.3964844f, 1.6f, 1.3964844f};
        case EntityType::WOLF:           return {0.6f,  0.85f, 0.6f};
        case EntityType::CAT:            return {0.6f,  0.7f,  0.6f};
        case EntityType::RABBIT:         return {0.49f, 0.6f,  0.49f};
        case EntityType::FOX:            return {0.6f,  0.7f,  0.6f};
        case EntityType::VILLAGER:       return {0.6f,  1.95f, 0.6f};
        case EntityType::VINDICATOR:     return {0.6f,  1.95f, 0.6f};
        case EntityType::PILLAGER:       return {0.6f,  1.95f, 0.6f};
        case EntityType::EVOKER:         return {0.6f,  1.95f, 0.6f};
        case EntityType::WANDERING_TRADER: return {0.6f,  1.95f, 0.6f};
        case EntityType::GIANT:          return {3.6f,  12.0f, 3.6f};
        case EntityType::ILLUSIONER:     return {0.6f,  1.95f, 0.6f};
        case EntityType::WITHER_SKELETON: return {0.7f,  2.4f,  0.7f};
        case EntityType::STRAY:          return {0.6f,  1.99f, 0.6f};
        case EntityType::HUSK:           return {0.6f,  1.95f, 0.6f};
        case EntityType::DROWNED:        return {0.6f,  1.95f, 0.6f};
        case EntityType::ZOMBIE_VILLAGER: return {0.6f,  1.95f, 0.6f};
        case EntityType::ZOMBIFIED_PIGLIN: return {0.6f,  1.95f, 0.6f};
        case EntityType::PIGLIN:         return {0.6f,  1.95f, 0.6f};
        case EntityType::IRON_GOLEM:     return {1.4f,  2.7f,  1.4f};
        case EntityType::SNOW_GOLEM:     return {0.7f,  1.9f,  0.7f};
        case EntityType::BAT:            return {0.5f,  0.9f,  0.5f};
        case EntityType::SQUID:          return {0.8f,  0.8f,  0.8f};
        case EntityType::ITEM:           return {0.25f, 0.25f, 0.25f};
        case EntityType::EXPERIENCE_ORB: return {0.5f,  0.5f,  0.5f};
        case EntityType::ARROW:          return {0.5f,  0.5f,  0.5f};
        case EntityType::SNOWBALL:       return {0.25f, 0.25f, 0.25f};
        case EntityType::FIREBALL:       return {1.0f,  1.0f,  1.0f};
        case EntityType::TNT:            return {0.98f, 0.98f, 0.98f};
        case EntityType::FALLING_BLOCK:  return {0.98f, 0.98f, 0.98f};
        case EntityType::ARMOR_STAND:    return {0.5f,  1.975f, 0.5f};
        case EntityType::BOAT:           return {1.375f, 0.5625f, 1.375f};
        case EntityType::MINECART:       return {0.98f, 0.7f,  0.98f};
        case EntityType::ENDER_DRAGON:   return {16.0f, 8.0f,  16.0f};
        case EntityType::WITHER:         return {0.9f,  3.5f,  0.9f};
        case EntityType::UNKNOWN:
        case EntityType::COUNT:
        default:                         return {1.0f,  1.0f,  1.0f};
    }
}

EntityType Entity::typeFromNetworkId(int32_t networkId) {
    // Order matches BuiltInRegistries.ENTITY_TYPE registration order in
    // 26.1.2/src/net/minecraft/world/entity/EntityType.java.
    switch (networkId) {
        case 5:   return EntityType::ARMOR_STAND;
        case 6:   return EntityType::ARROW;
        case 10:  return EntityType::BAT;
        case 14:  return EntityType::BLAZE;
        case 21:  return EntityType::CAT;
        case 26:  return EntityType::CHICKEN;
        case 30:  return EntityType::COW;
        case 32:  return EntityType::CREEPER;
        case 38:  return EntityType::DROWNED;
        case 41:  return EntityType::ENDERMAN;
        case 43:  return EntityType::ENDER_DRAGON;
        case 46:  return EntityType::EVOKER;
        case 49:  return EntityType::EXPERIENCE_ORB;
        case 51:  return EntityType::FALLING_BLOCK;
        case 52:  return EntityType::FIREBALL;
        case 54:  return EntityType::FOX;
        case 57:  return EntityType::GHAST;
        case 59:  return EntityType::GIANT;
        case 66:  return EntityType::HORSE;
        case 67:  return EntityType::HUSK;
        case 68:  return EntityType::ILLUSIONER;
        case 70:  return EntityType::IRON_GOLEM;
        case 71:  return EntityType::ITEM;
        case 85:  return EntityType::MINECART;
        case 100: return EntityType::PIG;
        case 101: return EntityType::PIGLIN;
        case 103: return EntityType::PILLAGER;
        case 108: return EntityType::RABBIT;
        case 111: return EntityType::SHEEP;
        case 115: return EntityType::SKELETON;
        case 117: return EntityType::SLIME;
        case 120: return EntityType::SNOWBALL;
        case 121: return EntityType::SNOW_GOLEM;
        case 124: return EntityType::SPIDER;
        case 127: return EntityType::SQUID;
        case 128: return EntityType::STRAY;
        case 132: return EntityType::TNT;
        case 139: return EntityType::VILLAGER;
        case 140: return EntityType::VINDICATOR;
        case 141: return EntityType::WANDERING_TRADER;
        case 144: return EntityType::WITCH;
        case 145: return EntityType::WITHER;
        case 146: return EntityType::WITHER_SKELETON;
        case 148: return EntityType::WOLF;
        case 150: return EntityType::ZOMBIE;
        case 153: return EntityType::ZOMBIE_VILLAGER;
        case 154: return EntityType::ZOMBIFIED_PIGLIN;
        case 155: return EntityType::PLAYER;
        default:  return EntityType::UNKNOWN;
    }
}

} // namespace mc
