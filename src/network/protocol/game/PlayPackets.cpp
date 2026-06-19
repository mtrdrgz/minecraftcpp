#include "PlayPackets.h"
#include "../../../core/Log.h"
#include "../../../world/level/block/BlockState.h"
#include "../../../world/item/Items.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mc::net {

namespace {
    float unpackDegrees(uint8_t rot) {
        return static_cast<int8_t>(rot) * 360.0f / 256.0f;
    }

    double unpackLpComponent(uint64_t value) {
        constexpr double MAX_QUANTIZED = 32766.0;
        double v = static_cast<double>(std::min<uint64_t>(value & 32767ULL, 32766ULL));
        return v * 2.0 / MAX_QUANTIZED - 1.0;
    }

    glm::dvec3 readLpVec3(PacketBuffer& buf) {
        uint8_t lowest = buf.readByte();
        if (lowest == 0) return {0.0, 0.0, 0.0};

        uint8_t middle = buf.readByte();
        uint32_t highest = static_cast<uint32_t>(buf.readInt());
        uint64_t packed = (static_cast<uint64_t>(highest) << 16)
                        | (static_cast<uint64_t>(middle) << 8)
                        | static_cast<uint64_t>(lowest);

        uint64_t scale = lowest & 3U;
        if ((lowest & 4U) == 4U) {
            scale |= (static_cast<uint64_t>(static_cast<uint32_t>(buf.readVarInt())) << 2);
        }

        return {
            unpackLpComponent(packed >> 3) * static_cast<double>(scale),
            unpackLpComponent(packed >> 18) * static_cast<double>(scale),
            unpackLpComponent(packed >> 33) * static_cast<double>(scale)
        };
    }

    void readPositionMoveRotation(PacketBuffer& buf,
                                  double& x, double& y, double& z,
                                  double& velX, double& velY, double& velZ,
                                  float& yaw, float& pitch) {
        x = buf.readDouble();
        y = buf.readDouble();
        z = buf.readDouble();
        velX = buf.readDouble();
        velY = buf.readDouble();
        velZ = buf.readDouble();
        yaw = buf.readFloat();
        pitch = buf.readFloat();
    }

    void skipByteArray(PacketBuffer& buf, int32_t maxSize) {
        int32_t size = buf.readVarInt();
        if (size < 0 || size > maxSize) {
            throw std::runtime_error("Packet byte array size out of bounds");
        }
        buf.skipBytes(static_cast<size_t>(size));
    }

    void skipNbtString(PacketBuffer& buf) {
        uint16_t len = static_cast<uint16_t>(buf.readShort());
        buf.skipBytes(len);
    }

    void skipAnyNbt(PacketBuffer& buf);

    void skipNbtPayload(PacketBuffer& buf, uint8_t type) {
        switch (type) {
        case 0:  return;
        case 1:  buf.skipBytes(1); return;
        case 2:  buf.skipBytes(2); return;
        case 3:  buf.skipBytes(4); return;
        case 4:  buf.skipBytes(8); return;
        case 5:  buf.skipBytes(4); return;
        case 6:  buf.skipBytes(8); return;
        case 7: {
            int32_t size = buf.readInt();
            if (size < 0) throw std::runtime_error("NBT byte array has negative size");
            buf.skipBytes(static_cast<size_t>(size));
            return;
        }
        case 8:
            skipNbtString(buf);
            return;
        case 9: {
            uint8_t elementType = buf.readByte();
            int32_t size = buf.readInt();
            if (size < 0) throw std::runtime_error("NBT list has negative size");
            for (int32_t i = 0; i < size; ++i) {
                skipNbtPayload(buf, elementType);
            }
            return;
        }
        case 10:
            for (;;) {
                uint8_t childType = buf.readByte();
                if (childType == 0) return;
                skipNbtString(buf);
                skipNbtPayload(buf, childType);
            }
        case 11: {
            int32_t size = buf.readInt();
            if (size < 0) throw std::runtime_error("NBT int array has negative size");
            buf.skipBytes(static_cast<size_t>(size) * 4);
            return;
        }
        case 12: {
            int32_t size = buf.readInt();
            if (size < 0) throw std::runtime_error("NBT long array has negative size");
            buf.skipBytes(static_cast<size_t>(size) * 8);
            return;
        }
        default:
            throw std::runtime_error("Unknown NBT tag type");
        }
    }

    void skipAnyNbt(PacketBuffer& buf) {
        uint8_t type = buf.readByte();
        skipNbtPayload(buf, type);
    }

    void skipGameProfileProperties(PacketBuffer& buf) {
        int32_t propertyCount = buf.readVarInt();
        if (propertyCount < 0 || propertyCount > 16) {
            throw std::runtime_error("Game profile property count out of bounds");
        }

        for (int32_t i = 0; i < propertyCount; ++i) {
            buf.readString(64);
            buf.readString(32767);
            if (buf.readBool()) {
                buf.readString(1024);
            }
        }
    }

    void skipRemoteChatSession(PacketBuffer& buf) {
        if (!buf.readBool()) return;

        uint64_t hi, lo;
        buf.readUUID(hi, lo);
        buf.readLong();          // ProfilePublicKey.Data.expiresAt
        skipByteArray(buf, 512); // public key
        skipByteArray(buf, 4096);
    }

    void skipNullableComponent(PacketBuffer& buf) {
        if (buf.readBool()) {
            skipAnyNbt(buf);
        }
    }

    mc::ItemStack readItemStack(PacketBuffer& buf) {
        int32_t count = buf.readVarInt();
        if (count <= 0) return mc::ItemStack::empty();

        int32_t itemId = buf.readVarInt();
        mc::ItemStack stack;
        stack.item = mc::g_itemRegistry.getById(itemId);
        stack.count = count;

        int32_t positiveCount = buf.readVarInt();
        int32_t negativeCount = buf.readVarInt();
        if (positiveCount > 0) {
            MC_LOG_WARN("ItemStack has {} positive components. Desync risk!", positiveCount);
        }
        for (int i = 0; i < negativeCount; ++i) {
            buf.readVarInt(); // typeId
        }

        return stack;
    }
}

S2CLoginPlayPacket S2CLoginPlayPacket::read(PacketBuffer& buf) {
    S2CLoginPlayPacket p;
    p.entityId   = buf.readInt();
    p.hardcore   = buf.readBool();
    int32_t dimCount = buf.readVarInt();
    for (int i = 0; i < dimCount; ++i)
        p.dimensionNames.push_back(buf.readString());
    p.maxPlayers        = buf.readVarInt();
    p.viewDistance      = buf.readVarInt();
    p.simulationDistance= buf.readVarInt();
    p.reducedDebugInfo  = buf.readBool();
    p.enableRespawnScreen = buf.readBool();
    p.doLimitedCrafting = buf.readBool();
    p.dimensionType     = buf.readVarInt();
    p.dimensionName     = buf.readString();
    p.hashedSeed        = buf.readLong();
    p.gameMode          = buf.readByte();
    p.previousGameMode  = (int8_t)buf.readByte();
    p.isDebug           = buf.readBool();
    p.isFlat            = buf.readBool();
    if (buf.readBool()) {
        buf.readString(); 
        buf.readLong();   
    }
    buf.readVarInt();
    buf.readVarInt();
    return p;
}

S2CChunkDataPacket S2CChunkDataPacket::read(PacketBuffer& buf) {
    S2CChunkDataPacket p;
    p.chunkX      = buf.readInt();
    p.chunkZ      = buf.readInt();
    p.heightmaps  = buf.readNbt();

    int32_t dataSize = buf.readVarInt();
    p.data = buf.readBytes(dataSize);

    int32_t beCount = buf.readVarInt();
    for (int i = 0; i < beCount; ++i) {
        buf.readByte();    
        buf.readShort();   
        buf.readVarInt();  
        buf.readNbt();     
    }
    return p;
}

S2CAddEntityPacket S2CAddEntityPacket::read(PacketBuffer& buf) {
    S2CAddEntityPacket p;
    p.entityId = buf.readVarInt();
    buf.readUUID(p.uuid.hi, p.uuid.lo);
    p.networkTypeId = buf.readVarInt();
    p.type = mc::Entity::typeFromNetworkId(p.networkTypeId);
    p.x = buf.readDouble();
    p.y = buf.readDouble();
    p.z = buf.readDouble();
    glm::dvec3 movement = readLpVec3(buf);
    p.velX = movement.x;
    p.velY = movement.y;
    p.velZ = movement.z;
    p.pitch = unpackDegrees(buf.readByte());
    p.yaw = unpackDegrees(buf.readByte());
    p.headYaw = unpackDegrees(buf.readByte());
    p.data = buf.readVarInt();
    return p;
}

S2CAnimatePacket S2CAnimatePacket::read(PacketBuffer& buf) {
    S2CAnimatePacket p;
    p.entityId = buf.readVarInt();
    p.action = buf.readByte();
    return p;
}

S2CEntityPositionSyncPacket S2CEntityPositionSyncPacket::read(PacketBuffer& buf) {
    S2CEntityPositionSyncPacket p;
    p.entityId = buf.readVarInt();
    readPositionMoveRotation(buf, p.x, p.y, p.z, p.velX, p.velY, p.velZ, p.yaw, p.pitch);
    p.onGround = buf.readBool();
    return p;
}

S2CMoveEntityPosPacket S2CMoveEntityPosPacket::read(PacketBuffer& buf) {
    S2CMoveEntityPosPacket p;
    p.entityId = buf.readVarInt();
    p.xa = buf.readShort();
    p.ya = buf.readShort();
    p.za = buf.readShort();
    p.onGround = buf.readBool();
    return p;
}

S2CMoveEntityPosRotPacket S2CMoveEntityPosRotPacket::read(PacketBuffer& buf) {
    S2CMoveEntityPosRotPacket p;
    p.entityId = buf.readVarInt();
    p.xa = buf.readShort();
    p.ya = buf.readShort();
    p.za = buf.readShort();
    p.yaw = unpackDegrees(buf.readByte());
    p.pitch = unpackDegrees(buf.readByte());
    p.onGround = buf.readBool();
    return p;
}

S2CMoveEntityRotPacket S2CMoveEntityRotPacket::read(PacketBuffer& buf) {
    S2CMoveEntityRotPacket p;
    p.entityId = buf.readVarInt();
    p.yaw = unpackDegrees(buf.readByte());
    p.pitch = unpackDegrees(buf.readByte());
    p.onGround = buf.readBool();
    return p;
}

S2CRemoveEntitiesPacket S2CRemoveEntitiesPacket::read(PacketBuffer& buf) {
    S2CRemoveEntitiesPacket p;
    int32_t count = buf.readVarInt();
    p.entityIds.reserve(count > 0 ? static_cast<size_t>(count) : 0);
    for (int32_t i = 0; i < count; ++i) {
        p.entityIds.push_back(buf.readVarInt());
    }
    return p;
}

S2CRotateHeadPacket S2CRotateHeadPacket::read(PacketBuffer& buf) {
    S2CRotateHeadPacket p;
    p.entityId = buf.readVarInt();
    p.headYaw = unpackDegrees(buf.readByte());
    return p;
}

S2CSetEntityMotionPacket S2CSetEntityMotionPacket::read(PacketBuffer& buf) {
    S2CSetEntityMotionPacket p;
    p.entityId = buf.readVarInt();
    glm::dvec3 movement = readLpVec3(buf);
    p.velX = movement.x;
    p.velY = movement.y;
    p.velZ = movement.z;
    return p;
}

S2CSetEntityDataPacket S2CSetEntityDataPacket::read(PacketBuffer& buf) {
    S2CSetEntityDataPacket p;
    p.entityId = buf.readVarInt();

    for (;;) {
        uint8_t id = buf.readByte();
        if (id == 0xFF) break;

        int32_t typeId = buf.readVarInt();
        mc::EntityMetadata meta;
        meta.type = static_cast<mc::EntityMetadata::Type>(typeId);

        switch (meta.type) {
            case mc::EntityMetadata::Type::BYTE:
                meta.value = buf.readByte();
                break;
            case mc::EntityMetadata::Type::INT:
            case mc::EntityMetadata::Type::OPT_VARINT:
            case mc::EntityMetadata::Type::POSE:
                meta.value = (int32_t)buf.readVarInt();
                break;
            case mc::EntityMetadata::Type::LONG:
                meta.value = (int64_t)buf.readLong();
                break;
            case mc::EntityMetadata::Type::FLOAT:
                meta.value = buf.readFloat();
                break;
            case mc::EntityMetadata::Type::STRING:
            case mc::EntityMetadata::Type::COMPONENT:
                meta.value = buf.readString();
                break;
            case mc::EntityMetadata::Type::BOOLEAN:
                meta.value = (uint8_t)(buf.readBool() ? 1 : 0);
                break;
            case mc::EntityMetadata::Type::OPT_COMPONENT:
            case mc::EntityMetadata::Type::OPT_BLOCK_POS:
            case mc::EntityMetadata::Type::OPT_UUID:
            case mc::EntityMetadata::Type::OPT_GLOBAL_POS:
                if (buf.readBool()) {
                    if (meta.type == mc::EntityMetadata::Type::OPT_COMPONENT) skipAnyNbt(buf);
                    else if (meta.type == mc::EntityMetadata::Type::OPT_BLOCK_POS) buf.readLong();
                    else if (meta.type == mc::EntityMetadata::Type::OPT_UUID) { uint64_t hi, lo; buf.readUUID(hi, lo); }
                    else if (meta.type == mc::EntityMetadata::Type::OPT_GLOBAL_POS) { buf.readString(); buf.readLong(); }
                }
                break;
            case mc::EntityMetadata::Type::ITEM_STACK:
                readItemStack(buf); 
                break;
            case mc::EntityMetadata::Type::ROTATION:
            case mc::EntityMetadata::Type::VECTOR3:
                buf.readFloat(); buf.readFloat(); buf.readFloat();
                break;
            case mc::EntityMetadata::Type::QUATERNION:
                buf.readFloat(); buf.readFloat(); buf.readFloat(); buf.readFloat();
                break;
            case mc::EntityMetadata::Type::BLOCK_POS:
                buf.readLong();
                break;
            case mc::EntityMetadata::Type::DIRECTION:
            case mc::EntityMetadata::Type::BLOCK_STATE:
            case mc::EntityMetadata::Type::OPT_BLOCK_STATE:
            case mc::EntityMetadata::Type::PARTICLE:
                buf.readVarInt();
                break;
            case mc::EntityMetadata::Type::VILLAGER_DATA:
                buf.readVarInt(); buf.readVarInt(); buf.readVarInt();
                break;
            case mc::EntityMetadata::Type::COMPOUND_TAG:
                skipAnyNbt(buf);
                break;
            default:
                break;
        }
        p.packedItems[id] = std::move(meta);
    }
    return p;
}

S2CSetEquipmentPacket S2CSetEquipmentPacket::read(PacketBuffer& buf) {
    S2CSetEquipmentPacket p;
    p.entityId = buf.readVarInt();

    uint8_t slotId;
    do {
        slotId = buf.readByte();
        mc::EquipmentSlot slot;
        switch (slotId & 127) {
            case 0: slot = mc::EquipmentSlot::MAINHAND; break;
            case 1: slot = mc::EquipmentSlot::FEET;     break;
            case 2: slot = mc::EquipmentSlot::LEGS;     break;
            case 3: slot = mc::EquipmentSlot::CHEST;    break;
            case 4: slot = mc::EquipmentSlot::HEAD;     break;
            case 5: slot = mc::EquipmentSlot::OFFHAND;  break;
            case 6: slot = mc::EquipmentSlot::BODY;     break;
            case 7: slot = mc::EquipmentSlot::SADDLE;   break;
            default: slot = mc::EquipmentSlot::MAINHAND; break;
        }
        p.slots.push_back({slot, readItemStack(buf)});
    } while ((slotId & 128) != 0);

    return p;
}

S2CSoundEntityPacket S2CSoundEntityPacket::read(PacketBuffer& buf) {
    S2CSoundEntityPacket p;
    int32_t id = buf.readVarInt();
    if (id == 0) {
        buf.readString(); 
        if (buf.readBool()) buf.readFloat(); 
        p.soundId = -1;
    } else {
        p.soundId = id - 1;
    }
    p.source = static_cast<mc::audio::SoundSource>(buf.readVarInt());
    p.entityId = buf.readVarInt();
    p.volume = buf.readFloat();
    p.pitch = buf.readFloat();
    p.seed = buf.readLong();
    return p;
}

S2CSoundPacket S2CSoundPacket::read(PacketBuffer& buf) {
    S2CSoundPacket p;
    int32_t id = buf.readVarInt();
    if (id == 0) {
        buf.readString();
        if (buf.readBool()) buf.readFloat();
        p.soundId = -1;
    } else {
        p.soundId = id - 1;
    }
    p.source = static_cast<mc::audio::SoundSource>(buf.readVarInt());
    p.x = (double)buf.readInt() / 8.0;
    p.y = (double)buf.readInt() / 8.0;
    p.z = (double)buf.readInt() / 8.0;
    p.volume = buf.readFloat();
    p.pitch = buf.readFloat();
    p.seed = buf.readLong();
    return p;
}

S2CStopSoundPacket S2CStopSoundPacket::read(PacketBuffer& buf) {
    S2CStopSoundPacket p;
    p.flags = buf.readByte();
    if (p.flags & 1) {
        p.source = static_cast<mc::audio::SoundSource>(buf.readVarInt());
    }
    if (p.flags & 2) {
        p.soundName = mc::ResourceLocation::parse(buf.readString());
    }
    return p;
}

S2CTeleportEntityPacket S2CTeleportEntityPacket::read(PacketBuffer& buf) {
    S2CTeleportEntityPacket p;
    p.entityId = buf.readVarInt();
    readPositionMoveRotation(buf, p.x, p.y, p.z, p.velX, p.velY, p.velZ, p.yaw, p.pitch);
    p.relatives = buf.readInt();
    p.onGround = buf.readBool();
    return p;
}

S2CForgetLevelChunkPacket S2CForgetLevelChunkPacket::read(PacketBuffer& buf) {
    S2CForgetLevelChunkPacket p;
    int64_t packed = buf.readLong();
    p.chunkX = static_cast<int32_t>(packed);
    p.chunkZ = static_cast<int32_t>(packed >> 32);
    return p;
}

void S2CChunkDataPacket::populateChunk(mc::LevelChunk& chunk) const {
    if (data.empty()) return;
    mc::net::PacketBuffer sectionBuf(data);
    for (int s = 0; s < mc::CHUNK_SECTION_COUNT; ++s) {
        if (sectionBuf.eof()) break;
        int16_t blockCount = sectionBuf.readShort(); 
        mc::ChunkSection* section = chunk.getSection(s);
        if (!section) continue;
        uint8_t bitsPerEntry = sectionBuf.readByte();
        if (bitsPerEntry == 0) {
            uint32_t singleStateId = (uint32_t)sectionBuf.readVarInt();
            sectionBuf.readVarInt(); 
            if (singleStateId != 0) {
                for (int y = 0; y < 16; ++y)
                    for (int z = 0; z < 16; ++z)
                        for (int x = 0; x < 16; ++x)
                            section->setBlock(x, y, z, singleStateId);
            }
        } else if (bitsPerEntry <= 8) {
            int32_t palSize = sectionBuf.readVarInt();
            std::vector<uint32_t> palette(palSize);
            for (auto& v : palette) v = (uint32_t)sectionBuf.readVarInt();
            int32_t dataLen = sectionBuf.readVarInt();
            std::vector<uint64_t> longs(dataLen);
            for (auto& l : longs) {
                int64_t v = sectionBuf.readLong();
                memcpy(&l, &v, 8);
            }
            int valuesPerLong = 64 / bitsPerEntry;
            uint64_t mask = (1ULL << bitsPerEntry) - 1;
            for (int i = 0; i < 4096; ++i) {
                int longIdx = i / valuesPerLong;
                int bitOff  = (i % valuesPerLong) * bitsPerEntry;
                if (longIdx >= dataLen) break;
                uint32_t palIdx = (uint32_t)((longs[longIdx] >> bitOff) & mask);
                uint32_t stateId = palIdx < palette.size() ? palette[palIdx] : 0;
                int x = i & 15, y = (i >> 8) & 15, z = (i >> 4) & 15;
                section->setBlock(x, y, z, stateId);
            }
        } else {
            sectionBuf.readVarInt(); 
            int32_t dataLen = sectionBuf.readVarInt();
            std::vector<uint64_t> longs(dataLen);
            for (auto& l : longs) {
                int64_t v = sectionBuf.readLong();
                memcpy(&l, &v, 8);
            }
            uint64_t mask = (1ULL << bitsPerEntry) - 1;
            for (int i = 0; i < 4096; ++i) {
                int longIdx = (i * bitsPerEntry) / 64;
                int bitOff  = (i * bitsPerEntry) % 64;
                if (longIdx >= dataLen) break;
                uint32_t stateId = (uint32_t)((longs[longIdx] >> bitOff) & mask);
                if (bitOff + bitsPerEntry > 64 && longIdx + 1 < dataLen) {
                    stateId |= (uint32_t)((longs[longIdx + 1] << (64 - bitOff)) & mask);
                }
                int x = i & 15, y = (i >> 8) & 15, z = (i >> 4) & 15;
                section->setBlock(x, y, z, stateId);
            }
        }
        uint8_t biomeBits = sectionBuf.readByte();
        if (biomeBits == 0) {
            sectionBuf.readVarInt(); 
            sectionBuf.readVarInt(); 
        } else {
            int32_t bpalSize = sectionBuf.readVarInt();
            for (int i = 0; i < bpalSize; ++i) sectionBuf.readVarInt();
            int32_t bdataLen = sectionBuf.readVarInt();
            for (int i = 0; i < bdataLen; ++i) sectionBuf.readLong();
        }
    }
    chunk.computeHeightmap();
    chunk.meshDirty = true;
}

S2CBlockUpdatePacket S2CBlockUpdatePacket::read(PacketBuffer& buf) {
    S2CBlockUpdatePacket p;
    int64_t packed = buf.readLong();
    p.x = (int32_t)(packed >> 38);
    p.y = (int32_t)((packed << 52) >> 52); 
    p.z = (int32_t)((packed << 26) >> 38); 
    p.blockStateId = buf.readVarInt();
    return p;
}

S2CSectionBlocksUpdatePacket S2CSectionBlocksUpdatePacket::read(PacketBuffer& buf) {
    S2CSectionBlocksUpdatePacket p;
    int64_t packed = buf.readLong();
    p.sectionX = (int32_t)(packed >> 42);
    p.sectionZ = (int32_t)((packed << 22) >> 42);
    p.sectionY = (int32_t)((packed << 44) >> 44);
    int32_t count = buf.readVarInt();
    p.changes.reserve(count > 0 ? (size_t)count : 0);
    for (int32_t i = 0; i < count; ++i) {
        int64_t v = buf.readVarLong();
        int32_t localPos = (int32_t)(v & 0xFFFLL);
        int32_t stateId  = (int32_t)((uint64_t)v >> 12);
        int32_t lx = (localPos >> 8) & 0xF;
        int32_t lz = (localPos >> 4) & 0xF;
        int32_t ly = localPos & 0xF;
        Change c;
        c.x = (p.sectionX << 4) + lx;
        c.y = (p.sectionY << 4) + ly;
        c.z = (p.sectionZ << 4) + lz;
        c.blockStateId = stateId;
        p.changes.push_back(c);
    }
    return p;
}

S2CPlayerInfoUpdatePacket S2CPlayerInfoUpdatePacket::read(PacketBuffer& buf) {
    S2CPlayerInfoUpdatePacket p;
    p.actions = buf.readByte();
    int32_t count = buf.readVarInt();
    if (count < 0) throw std::runtime_error("PlayerInfoUpdate entry count is negative");
    p.entries.reserve(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        Entry entry;
        buf.readUUID(entry.profileId.hi, entry.profileId.lo);
        if (p.hasAction(ADD_PLAYER)) {
            entry.profileName = buf.readString(16);
            entry.hasProfile = true;
            skipGameProfileProperties(buf);
        }
        if (p.hasAction(INITIALIZE_CHAT)) {
            skipRemoteChatSession(buf);
        }
        if (p.hasAction(UPDATE_GAME_MODE)) {
            entry.gameMode = buf.readVarInt();
        }
        if (p.hasAction(UPDATE_LISTED)) {
            entry.listed = buf.readBool();
        }
        if (p.hasAction(UPDATE_LATENCY)) {
            entry.latency = buf.readVarInt();
        }
        if (p.hasAction(UPDATE_DISPLAY_NAME)) {
            skipNullableComponent(buf);
        }
        if (p.hasAction(UPDATE_LIST_ORDER)) {
            entry.listOrder = buf.readVarInt();
        }
        if (p.hasAction(UPDATE_HAT)) {
            entry.showHat = buf.readBool();
        }
        p.entries.push_back(std::move(entry));
    }
    return p;
}

S2CPlayerInfoRemovePacket S2CPlayerInfoRemovePacket::read(PacketBuffer& buf) {
    S2CPlayerInfoRemovePacket p;
    int32_t count = buf.readVarInt();
    if (count < 0) throw std::runtime_error("PlayerInfoRemove entry count is negative");
    p.profileIds.reserve(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        mc::UUID profileId;
        buf.readUUID(profileId.hi, profileId.lo);
        p.profileIds.push_back(profileId);
    }
    return p;
}

} // namespace mc::net
