#pragma once
#include "../../core/Math.h"
#include "../item/Item.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <variant>
#include <vector>
#include <string>
#include <optional>

namespace mc {

// 128-bit UUID, ported from java.util.UUID. Two 64-bit halves; high bits first.
struct UUID {
    uint64_t hi = 0;
    uint64_t lo = 0;
    constexpr bool operator==(const UUID&) const = default;
};

struct UUIDHash {
    size_t operator()(const UUID& uuid) const noexcept {
        size_t h1 = std::hash<uint64_t>{}(uuid.hi);
        size_t h2 = std::hash<uint64_t>{}(uuid.lo);
        return h1 ^ (h2 + 0x9E3779B97F4A7C15ULL + (h1 << 6) + (h1 >> 2));
    }
};

// Port of net.minecraft.world.entity.EquipmentSlot
enum class EquipmentSlot : uint8_t {
    MAINHAND = 0,
    FEET     = 1,
    LEGS     = 2,
    CHEST    = 3,
    HEAD     = 4,
    OFFHAND  = 5,
    BODY     = 6,
    SADDLE   = 7,
    COUNT
};

// Port of subset of net.minecraft.world.entity.EntityType registrations.
// Numeric values are arbitrary (NOT protocol IDs — the protocol uses a registry
// remapping on join). These are used by the client to pick a default bounding
// box and renderer. Add more entries as renderers are ported.
enum class EntityType : int32_t {
    UNKNOWN = 0,
    PLAYER,
    ZOMBIE,
    SKELETON,
    CREEPER,
    SPIDER,
    ENDERMAN,
    WITCH,
    SLIME,
    BLAZE,
    GHAST,
    COW,
    PIG,
    SHEEP,
    CHICKEN,
    HORSE,
    WOLF,
    CAT,
    RABBIT,
    FOX,
    VILLAGER,
    VINDICATOR,
    PILLAGER,
    EVOKER,
    WANDERING_TRADER,
    GIANT,
    ILLUSIONER,
    WITHER_SKELETON,
    STRAY,
    HUSK,
    DROWNED,
    ZOMBIE_VILLAGER,
    ZOMBIFIED_PIGLIN,
    PIGLIN,
    IRON_GOLEM,
    SNOW_GOLEM,
    BAT,
    SQUID,
    ITEM,            // dropped item entity (ItemEntity.java)
    EXPERIENCE_ORB,
    ARROW,
    SNOWBALL,
    FIREBALL,
    TNT,
    FALLING_BLOCK,
    ARMOR_STAND,
    BOAT,
    MINECART,
    ENDER_DRAGON,
    WITHER,
    COUNT
};

// Port of net.minecraft.network.syncher.SynchedEntityData
// Stores key-value pairs of entity state that drive renderers and logic.
struct EntityMetadata {
    enum class Type {
        BYTE = 0,
        INT = 1,
        LONG = 2,
        FLOAT = 3,
        STRING = 4,
        COMPONENT = 5,
        OPT_COMPONENT = 6,
        ITEM_STACK = 7,
        BOOLEAN = 8,
        ROTATION = 9,
        BLOCK_POS = 10,
        OPT_BLOCK_POS = 11,
        DIRECTION = 12,
        OPT_UUID = 13,
        BLOCK_STATE = 14,
        OPT_BLOCK_STATE = 15,
        COMPOUND_TAG = 16,
        PARTICLE = 17,
        VILLAGER_DATA = 18,
        OPT_VARINT = 19,
        POSE = 20,
        CAT_VARIANT = 21,
        WOLF_VARIANT = 22,
        FROG_VARIANT = 23,
        OPT_GLOBAL_POS = 24,
        PAINTING_VARIANT = 25,
        SNIFFER_STATE = 26,
        ARMADILLO_STATE = 27,
        VECTOR3 = 28,
        QUATERNION = 29
    };

    using Value = std::variant<
        uint8_t,      // BYTE / BOOLEAN
        int32_t,      // INT / VARINT / POSE / VILLAGER_DATA (approx)
        int64_t,      // LONG
        float,        // FLOAT
        std::string,  // STRING / COMPONENT / etc (serialized)
        std::vector<uint8_t> // blob for complex types
    >;

    Type type;
    Value value;
};

// Port of net.minecraft.world.entity.Entity — minimal, client-side only.
// Stores the data carried by ADD_ENTITY / MOVE_ENTITY / TELEPORT_ENTITY /
// SET_ENTITY_MOTION packets so a renderer can draw a box where the entity is.
// Physics, AI, and most behavior live in subclasses (LivingEntity, Mob,
// Player, etc.) and are deferred until Phase 15.
class Entity {
public:
    Entity(int32_t id, EntityType type);
    virtual ~Entity() = default;

    int32_t       entityId = 0;
    UUID          uuid{};
    EntityType    type     = EntityType::UNKNOWN;

    glm::dvec3    position{0.0, 0.0, 0.0};
    glm::dvec3    positionCodecBase{0.0, 0.0, 0.0};
    glm::vec3     velocity{0.0f, 0.0f, 0.0f};

    float         yaw      = 0.0f;   // Entity.java yRot
    float         pitch    = 0.0f;   // Entity.java xRot
    float         headYaw  = 0.0f;   // LivingEntity.java yHeadRot

    // Bounding box dimensions: (width, height, width). Width applies to both
    // X and Z axes — matches EntityDimensions.java.
    glm::vec3     bbSize{0.6f, 1.8f, 0.6f};

    bool          onGround = false;
    bool          removed  = false;

    // Synced entity metadata (SynchedEntityData.java)
    std::map<uint8_t, EntityMetadata> metadata;
    
    // Entity equipment (LivingEntity.java armor + hands)
    std::map<EquipmentSlot, ItemStack> equipment;

    float         walkAnimPos   = 0.0f;
    float         walkAnimSpeed = 0.0f;
    int32_t       swingTime     = 0;
    int32_t       swingTimeMax  = 6;

    void setPosition(double x, double y, double z);
    void setRotation(float newYaw, float newPitch);

    virtual void tick();

    // Helpers to access metadata
    bool getMetadataFlag(uint8_t entryId, int flagIndex) const;
    uint8_t getMetadataByte(uint8_t entryId, uint8_t defaultValue = 0) const;

    // AABB derived from position + bbSize. Position is the entity's foot
    // origin: feet at position.y, head at position.y + bbSize.y.
    glm::dvec3 getBBMin() const;
    glm::dvec3 getBBMax() const;

    // Default bounding-box size for a given EntityType. Sourced from the
    // .sized() calls in EntityType.java.
    static glm::vec3 defaultBBSize(EntityType type);

    // BuiltInRegistries.ENTITY_TYPE network id -> local subset enum.
    static EntityType typeFromNetworkId(int32_t networkId);
};

} // namespace mc
