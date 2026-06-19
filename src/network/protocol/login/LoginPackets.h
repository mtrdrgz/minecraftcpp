#pragma once
#include "../../protocol/Packet.h"

namespace mc::net {

// ── C2S ──────────────────────────────────────────────────────────────────────

// C2S 0x00 Login Start
// Reference: net.minecraft.network.protocol.login.ServerboundHelloPacket
struct C2SLoginStartPacket : Packet {
    std::string name;
    uint64_t    uuidHi = 0, uuidLo = 0;

    int32_t packetId() const override { return 0x00; }
    void write(PacketBuffer& buf) const override {
        buf.writeString(name, 16);
        buf.writeUUID(uuidHi, uuidLo);
    }
};

// C2S 0x01 Encryption Response
// Reference: net.minecraft.network.protocol.login.ServerboundKeyPacket
struct C2SEncryptionResponsePacket : Packet {
    std::vector<uint8_t> sharedSecret;
    std::vector<uint8_t> verifyToken;

    int32_t packetId() const override { return 0x01; }
    void write(PacketBuffer& buf) const override {
        buf.writeVarInt((int32_t)sharedSecret.size());
        buf.writeBytes(sharedSecret);
        buf.writeVarInt((int32_t)verifyToken.size());
        buf.writeBytes(verifyToken);
    }
};

// C2S 0x03 Login Acknowledged
struct C2SLoginAcknowledgedPacket : Packet {
    int32_t packetId() const override { return 0x03; }
    void write(PacketBuffer&) const override {}
};

// ── S2C ──────────────────────────────────────────────────────────────────────

// S2C 0x00 Disconnect (Login)
struct S2CLoginDisconnectPacket {
    static constexpr int32_t ID = 0x00;
    std::string reason;
    static S2CLoginDisconnectPacket read(PacketBuffer& buf) {
        S2CLoginDisconnectPacket p;
        p.reason = buf.readString();
        return p;
    }
};

// S2C 0x01 Encryption Request
struct S2CEncryptionRequestPacket {
    static constexpr int32_t ID = 0x01;
    std::string serverId;
    std::vector<uint8_t> publicKey;
    std::vector<uint8_t> verifyToken;
    bool shouldAuthenticate = true;

    static S2CEncryptionRequestPacket read(PacketBuffer& buf) {
        S2CEncryptionRequestPacket p;
        p.serverId = buf.readString(20);
        int32_t pkLen = buf.readVarInt();
        p.publicKey = buf.readBytes(pkLen);
        int32_t vtLen = buf.readVarInt();
        p.verifyToken = buf.readBytes(vtLen);
        p.shouldAuthenticate = buf.readBool();
        return p;
    }
};

// S2C 0x02 Login Success
struct S2CLoginSuccessPacket {
    static constexpr int32_t ID = 0x02;
    uint64_t    uuidHi = 0, uuidLo = 0;
    std::string username;

    static S2CLoginSuccessPacket read(PacketBuffer& buf) {
        S2CLoginSuccessPacket p;
        buf.readUUID(p.uuidHi, p.uuidLo);
        p.username = buf.readString(16);
        int32_t propCount = buf.readVarInt();
        for (int i = 0; i < propCount; ++i) {
            buf.readString(); // name
            buf.readString(); // value
            if (buf.readBool()) buf.readString(); // signature
        }
        return p;
    }
};

// S2C 0x03 Set Compression
struct S2CSetCompressionPacket {
    static constexpr int32_t ID = 0x03;
    int32_t threshold;
    static S2CSetCompressionPacket read(PacketBuffer& buf) {
        return { buf.readVarInt() };
    }
};

} // namespace mc::net
