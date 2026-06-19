#pragma once
// Port of net.minecraft.network.protocol.common.ServerboundKeepAlivePacket
//
// 1:1 with 26.1.2/src/net/minecraft/network/protocol/common/ServerboundKeepAlivePacket.java
//   public static final StreamCodec<FriendlyByteBuf, ServerboundKeepAlivePacket> STREAM_CODEC =
//       Packet.codec(ServerboundKeepAlivePacket::write, ServerboundKeepAlivePacket::new);
//   private final long id;
//   private ServerboundKeepAlivePacket(FriendlyByteBuf input) { this.id = input.readLong(); }
//   private void write(FriendlyByteBuf output) { output.writeLong(this.id); }
//
// The STREAM_CODEC body is exactly one writeLong/readLong (8 bytes, big-endian).
#include "../../PacketBuffer.h"
#include <cstdint>

namespace mc::net::protocol::common {

struct ServerboundKeepAlivePacket {
    // ServerboundKeepAlivePacket(final long id) { this.id = id; }
    long long id;

    // private void write(final FriendlyByteBuf output) { output.writeLong(this.id); }
    void write(PacketBuffer& output) const {
        output.writeLong((int64_t)id);
    }

    // private ServerboundKeepAlivePacket(final FriendlyByteBuf input) { this.id = input.readLong(); }
    static ServerboundKeepAlivePacket read(PacketBuffer& input) {
        return ServerboundKeepAlivePacket{ (long long)input.readLong() };
    }

    // public long getId() { return this.id; }
    long long getId() const { return id; }
};

} // namespace mc::net::protocol::common
