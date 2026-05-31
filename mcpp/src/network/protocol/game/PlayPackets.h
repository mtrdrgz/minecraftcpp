#pragma once
#include "../../protocol/Packet.h"
#include "../../../nbt/Tag.h"
#include "../../../world/level/chunk/LevelChunk.h"
#include "../../../world/entity/Entity.h"
#include "audio/SoundSource.h"
#include <optional>
#include <array>
#include <vector>
#include <map>

namespace mc::net {

// S2C 0x30 Login (Play)
struct S2CLoginPlayPacket {
    static constexpr int32_t ID = 0x30;
    int32_t entityId;
    bool    hardcore;
    std::vector<std::string> dimensionNames;
    int32_t maxPlayers;
    int32_t viewDistance;
    int32_t simulationDistance;
    bool    reducedDebugInfo;
    bool    enableRespawnScreen;
    bool    doLimitedCrafting;
    int32_t dimensionType;
    std::string dimensionName;
    int64_t hashedSeed;
    uint8_t gameMode;
    int8_t  previousGameMode;
    bool    isDebug;
    bool    isFlat;

    static S2CLoginPlayPacket read(PacketBuffer& buf);
};

// S2C 0x2C Chunk Data
struct S2CChunkDataPacket {
    static constexpr int32_t ID = 0x2C;
    int32_t chunkX, chunkZ;
    mc::nbt::NbtCompound heightmaps;
    std::vector<uint8_t> data;

    static S2CChunkDataPacket read(PacketBuffer& buf);
    void populateChunk(mc::LevelChunk& chunk) const;
};

// S2C 0x1F Disconnect
struct S2CDisconnectPlayPacket {
    static constexpr int32_t ID = 0x1F;
    std::string reason;
    static S2CDisconnectPlayPacket read(PacketBuffer& buf) {
        return { buf.readString() };
    }
};

// S2C 0x2F Update Light
struct S2CUpdateLightPacket {
    static constexpr int32_t ID = 0x2F;
    static void skip(PacketBuffer& buf) {
        buf.readVarInt();
        buf.readVarInt();
        buf.readVarInt();
    }
};

// S2C 0x47 Set Player Position
struct S2CPlayerPositionPacket {
    static constexpr int32_t ID = 0x47;
    int32_t teleportId;
    double x, y, z;
    double velX, velY, velZ;
    float  yaw, pitch;
    int32_t relatives;

    bool relativeX() const { return (relatives & (1 << 0)) != 0; }
    bool relativeY() const { return (relatives & (1 << 1)) != 0; }
    bool relativeZ() const { return (relatives & (1 << 2)) != 0; }
    bool relativeYaw() const { return (relatives & (1 << 3)) != 0; }
    bool relativePitch() const { return (relatives & (1 << 4)) != 0; }

    static S2CPlayerPositionPacket read(PacketBuffer& buf) {
        S2CPlayerPositionPacket p;
        p.teleportId = buf.readVarInt();
        p.x    = buf.readDouble();
        p.y    = buf.readDouble();
        p.z    = buf.readDouble();
        p.velX = buf.readDouble();
        p.velY = buf.readDouble();
        p.velZ = buf.readDouble();
        p.yaw  = buf.readFloat();
        p.pitch= buf.readFloat();
        p.relatives = buf.readInt();
        return p;
    }
};

// S2C 0x2B Keep Alive
struct S2CKeepAlivePacket {
    static constexpr int32_t ID = 0x2B;
    int64_t id;
    static S2CKeepAlivePacket read(PacketBuffer& buf) { return { buf.readLong() }; }
};

// S2C 0x00 Add Entity
struct S2CAddEntityPacket {
    static constexpr int32_t ID = 0x00;
    int32_t entityId = 0;
    mc::UUID uuid{};
    int32_t networkTypeId = 0;
    mc::EntityType type = mc::EntityType::UNKNOWN;
    double x = 0.0, y = 0.0, z = 0.0;
    double velX = 0.0, velY = 0.0, velZ = 0.0;
    float yaw = 0.0f, pitch = 0.0f, headYaw = 0.0f;
    int32_t data = 0;

    static S2CAddEntityPacket read(PacketBuffer& buf);
};

// S2C 0x03 Animate
struct S2CAnimatePacket {
    static constexpr int32_t ID = 0x03;
    int32_t entityId;
    uint8_t action;
    static S2CAnimatePacket read(PacketBuffer& buf);
};

// S2C 0x22 Entity Position Sync
struct S2CEntityPositionSyncPacket {
    static constexpr int32_t ID = 0x22;
    int32_t entityId = 0;
    double x = 0.0, y = 0.0, z = 0.0;
    double velX = 0.0, velY = 0.0, velZ = 0.0;
    float yaw = 0.0f, pitch = 0.0f;
    bool onGround = false;

    static S2CEntityPositionSyncPacket read(PacketBuffer& buf);
};

// S2C 0x34 Move Entity Pos
struct S2CMoveEntityPosPacket {
    static constexpr int32_t ID = 0x34;
    int32_t entityId = 0;
    int16_t xa = 0, ya = 0, za = 0;
    bool onGround = false;

    static S2CMoveEntityPosPacket read(PacketBuffer& buf);
};

// S2C 0x35 Move Entity Pos Rot
struct S2CMoveEntityPosRotPacket {
    static constexpr int32_t ID = 0x35;
    int32_t entityId = 0;
    int16_t xa = 0, ya = 0, za = 0;
    float yaw = 0.0f, pitch = 0.0f;
    bool onGround = false;

    static S2CMoveEntityPosRotPacket read(PacketBuffer& buf);
};

// S2C 0x37 Move Entity Rot
struct S2CMoveEntityRotPacket {
    static constexpr int32_t ID = 0x37;
    int32_t entityId = 0;
    float yaw = 0.0f, pitch = 0.0f;
    bool onGround = false;

    static S2CMoveEntityRotPacket read(PacketBuffer& buf);
};

// S2C 0x4C Remove Entities
struct S2CRemoveEntitiesPacket {
    static constexpr int32_t ID = 0x4C;
    std::vector<int32_t> entityIds;

    static S2CRemoveEntitiesPacket read(PacketBuffer& buf);
};

// S2C 0x52 Rotate Head
struct S2CRotateHeadPacket {
    static constexpr int32_t ID = 0x52;
    int32_t entityId = 0;
    float headYaw = 0.0f;

    static S2CRotateHeadPacket read(PacketBuffer& buf);
};

// S2C 0x64 Set Entity Motion
struct S2CSetEntityMotionPacket {
    static constexpr int32_t ID = 0x64;
    int32_t entityId = 0;
    double velX = 0.0, velY = 0.0, velZ = 0.0;

    static S2CSetEntityMotionPacket read(PacketBuffer& buf);
};

// S2C 0x62 Set Entity Data
struct S2CSetEntityDataPacket {
    static constexpr int32_t ID = 0x62;
    int32_t entityId;
    std::map<uint8_t, mc::EntityMetadata> packedItems;
    static S2CSetEntityDataPacket read(PacketBuffer& buf);
};

// S2C 0x65 Set Equipment
struct S2CSetEquipmentPacket {
    static constexpr int32_t ID = 0x65;
    int32_t entityId;
    std::vector<std::pair<mc::EquipmentSlot, mc::ItemStack>> slots;
    static S2CSetEquipmentPacket read(PacketBuffer& buf);
};

// S2C 0x73 Sound Entity
struct S2CSoundEntityPacket {
    static constexpr int32_t ID = 0x73;
    int32_t soundId;
    mc::audio::SoundSource source;
    int32_t entityId;
    float volume;
    float pitch;
    int64_t seed;
    static S2CSoundEntityPacket read(PacketBuffer& buf);
};

// S2C 0x74 Sound
struct S2CSoundPacket {
    static constexpr int32_t ID = 0x74;
    int32_t soundId;
    mc::audio::SoundSource source;
    double x, y, z;
    float volume;
    float pitch;
    int64_t seed;
    static S2CSoundPacket read(PacketBuffer& buf);
};

// S2C 0x76 Stop Sound
struct S2CStopSoundPacket {
    static constexpr int32_t ID = 0x76;
    uint8_t flags;
    std::optional<mc::audio::SoundSource> source;
    std::optional<mc::ResourceLocation> soundName;
    static S2CStopSoundPacket read(PacketBuffer& buf);
};

// S2C 0x7C Teleport Entity
struct S2CTeleportEntityPacket {
    static constexpr int32_t ID = 0x7C;
    int32_t entityId = 0;
    double x = 0.0, y = 0.0, z = 0.0;
    double velX = 0.0, velY = 0.0, velZ = 0.0;
    float yaw = 0.0f, pitch = 0.0f;
    int32_t relatives = 0;
    bool onGround = false;

    bool relativeX() const { return (relatives & (1 << 0)) != 0; }
    bool relativeY() const { return (relatives & (1 << 1)) != 0; }
    bool relativeZ() const { return (relatives & (1 << 2)) != 0; }
    bool relativeYaw() const { return (relatives & (1 << 3)) != 0; }
    bool relativePitch() const { return (relatives & (1 << 4)) != 0; }
    bool relativeVelX() const { return (relatives & (1 << 5)) != 0; }
    bool relativeVelY() const { return (relatives & (1 << 6)) != 0; }
    bool relativeVelZ() const { return (relatives & (1 << 7)) != 0; }

    static S2CTeleportEntityPacket read(PacketBuffer& buf);
};

// S2C 0x24 Forget Level Chunk
struct S2CForgetLevelChunkPacket {
    static constexpr int32_t ID = 0x24;
    int32_t chunkX = 0;
    int32_t chunkZ = 0;

    static S2CForgetLevelChunkPacket read(PacketBuffer& buf);
};

// S2C 0x07 Block Update
struct S2CBlockUpdatePacket {
    static constexpr int32_t ID = 0x07;
    int32_t x, y, z;
    int32_t blockStateId;

    static S2CBlockUpdatePacket read(PacketBuffer& buf);
};

// S2C 0x53 Section Blocks Update
struct S2CSectionBlocksUpdatePacket {
    static constexpr int32_t ID = 0x53;
    int32_t sectionX, sectionY, sectionZ;
    struct Change {
        int32_t x, y, z;
        int32_t blockStateId;
    };
    std::vector<Change> changes;

    static S2CSectionBlocksUpdatePacket read(PacketBuffer& buf);
};

// S2C 0x45 Player Info Update
struct S2CPlayerInfoUpdatePacket {
    static constexpr int32_t ID = 0x45;

    enum Action : uint8_t {
        ADD_PLAYER          = 1 << 0,
        INITIALIZE_CHAT     = 1 << 1,
        UPDATE_GAME_MODE    = 1 << 2,
        UPDATE_LISTED       = 1 << 3,
        UPDATE_LATENCY      = 1 << 4,
        UPDATE_DISPLAY_NAME = 1 << 5,
        UPDATE_LIST_ORDER   = 1 << 6,
        UPDATE_HAT          = 1 << 7
    };

    struct Entry {
        mc::UUID profileId{};
        std::string profileName;
        bool hasProfile = false;
        bool listed = false;
        int32_t latency = 0;
        int32_t gameMode = 0;
        bool showHat = true;
        int32_t listOrder = 0;
    };

    uint8_t actions = 0;
    std::vector<Entry> entries;

    bool hasAction(Action action) const { return (actions & action) != 0; }

    static S2CPlayerInfoUpdatePacket read(PacketBuffer& buf);
};

// S2C 0x44 Player Info Remove
struct S2CPlayerInfoRemovePacket {
    static constexpr int32_t ID = 0x44;
    std::vector<mc::UUID> profileIds;

    static S2CPlayerInfoRemovePacket read(PacketBuffer& buf);
};

// ── Key C2S Play packets ──────────────────────────────────────────────────────

struct C2SKeepAlivePacket : Packet {
    int64_t id;
    int32_t packetId() const override { return 0x1C; }
    void write(PacketBuffer& buf) const override { buf.writeLong(id); }
};

struct C2SConfirmTeleportPacket : Packet {
    int32_t teleportId;
    int32_t packetId() const override { return 0x00; }
    void write(PacketBuffer& buf) const override { buf.writeVarInt(teleportId); }
};

struct C2SPlayerMovePositionPacket : Packet {
    double x, y, z;
    bool   onGround;
    bool   horizontalCollision = false;
    int32_t packetId() const override { return 0x1E; }
    void write(PacketBuffer& buf) const override {
        buf.writeDouble(x); buf.writeDouble(y); buf.writeDouble(z);
        uint8_t flags = 0;
        if (onGround) flags |= 1;
        if (horizontalCollision) flags |= 2;
        buf.writeByte(flags);
    }
};

struct C2SPlayerMovePositionRotationPacket : Packet {
    double x, y, z;
    float yaw, pitch;
    bool  onGround;
    bool  horizontalCollision = false;
    int32_t packetId() const override { return 0x1F; }
    void write(PacketBuffer& buf) const override {
        buf.writeDouble(x); buf.writeDouble(y); buf.writeDouble(z);
        buf.writeFloat(yaw); buf.writeFloat(pitch);
        uint8_t flags = 0;
        if (onGround) flags |= 1;
        if (horizontalCollision) flags |= 2;
        buf.writeByte(flags);
    }
};

struct C2SPlayerMoveRotationPacket : Packet {
    float yaw, pitch;
    bool  onGround;
    bool  horizontalCollision = false;
    int32_t packetId() const override { return 0x20; }
    void write(PacketBuffer& buf) const override {
        buf.writeFloat(yaw); buf.writeFloat(pitch);
        uint8_t flags = 0;
        if (onGround) flags |= 1;
        if (horizontalCollision) flags |= 2;
        buf.writeByte(flags);
    }
};

struct C2SPlayerMoveStatusOnlyPacket : Packet {
    bool onGround;
    bool horizontalCollision = false;
    int32_t packetId() const override { return 0x21; }
    void write(PacketBuffer& buf) const override {
        uint8_t flags = 0;
        if (onGround) flags |= 1;
        if (horizontalCollision) flags |= 2;
        buf.writeByte(flags);
    }
};

struct C2SPlayerCommandPacket : Packet {
    enum Action : int32_t {
        STOP_SLEEPING = 0,
        START_SPRINTING = 1,
        STOP_SPRINTING = 2,
        START_RIDING_JUMP = 3,
        STOP_RIDING_JUMP = 4,
        OPEN_INVENTORY = 5,
        START_FALL_FLYING = 6
    };

    int32_t entityId = 0;
    Action action = STOP_SLEEPING;
    int32_t data = 0;

    int32_t packetId() const override { return 0x2A; }
    void write(PacketBuffer& buf) const override {
        buf.writeVarInt(entityId);
        buf.writeVarInt(static_cast<int32_t>(action));
        buf.writeVarInt(data);
    }
};

struct C2SPlayerInputPacket : Packet {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool jump = false;
    bool shift = false;
    bool sprint = false;

    int32_t packetId() const override { return 0x2B; }
    void write(PacketBuffer& buf) const override {
        uint8_t flags = 0;
        if (forward)  flags |= 1;
        if (backward) flags |= 2;
        if (left)     flags |= 4;
        if (right)    flags |= 8;
        if (jump)     flags |= 16;
        if (shift)    flags |= 32;
        if (sprint)   flags |= 64;
        buf.writeByte(flags);
    }
};

struct C2SClientInformationPacket : Packet {
    std::string locale   = "en_us";
    int8_t  viewDistance = 8;
    int32_t chatMode     = 0;
    bool    chatColors   = true;
    uint8_t skinParts    = 0x7F;
    int32_t mainHand     = 1; // right
    bool    textFiltering= false;
    bool    serverListing= true;
    int32_t particleStatus = 0;

    int32_t packetId() const override { return 0x0E; }
    void write(PacketBuffer& buf) const override {
        buf.writeString(locale, 16);
        buf.writeByte((uint8_t)viewDistance);
        buf.writeVarInt(chatMode);
        buf.writeBool(chatColors);
        buf.writeByte(skinParts);
        buf.writeVarInt(mainHand);
        buf.writeBool(textFiltering);
        buf.writeBool(serverListing);
        buf.writeVarInt(particleStatus);
    }
};

struct C2SConfigAcknowledgedPacket : Packet {
    int32_t packetId() const override { return 0x03; }
    void write(PacketBuffer&) const override {}
};

struct S2CFinishConfigurationPacket {
    static constexpr int32_t ID = 0x03;
};

struct C2SFinishConfigurationPacket : Packet {
    int32_t packetId() const override { return 0x03; }
    void write(PacketBuffer&) const override {}
};

} // namespace mc::net
