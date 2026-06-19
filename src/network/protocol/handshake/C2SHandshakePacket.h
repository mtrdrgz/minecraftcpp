#pragma once
#include "../../protocol/Packet.h"

namespace mc::net {

// C2S 0x00 in Handshake state
// Reference: net.minecraft.network.protocol.handshake.ClientIntentionPacket
struct C2SHandshakePacket : Packet {
    int32_t     protocolVersion = 770; // Minecraft 26.1.2 protocol version
    std::string serverAddress;
    uint16_t    serverPort   = 25565;
    int32_t     nextState    = 2;    // 1=status, 2=login

    int32_t packetId() const override { return 0x00; }
    void write(PacketBuffer& buf) const override {
        buf.writeVarInt(protocolVersion);
        buf.writeString(serverAddress, 255);
        // Port as unsigned short (big-endian)
        uint16_t port = serverPort;
        buf.writeByte((uint8_t)(port >> 8));
        buf.writeByte((uint8_t)(port & 0xFF));
        buf.writeVarInt(nextState);
    }
};

} // namespace mc::net
