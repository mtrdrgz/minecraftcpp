#pragma once
// Port of net.minecraft.network.protocol.common.ClientboundKeepAlivePacket
//
// Java (26.1.2):
//   public static final StreamCodec<FriendlyByteBuf, ClientboundKeepAlivePacket> STREAM_CODEC =
//       Packet.codec(ClientboundKeepAlivePacket::write, ClientboundKeepAlivePacket::new);
//   private final long id;
//   private ClientboundKeepAlivePacket(FriendlyByteBuf input) { this.id = input.readLong(); }
//   private void write(FriendlyByteBuf output) { output.writeLong(this.id); }
//   public long getId() { return this.id; }
//
// The codec is a single 64-bit big-endian long (FriendlyByteBuf.writeLong ->
// netty ByteBuf.writeLong is big-endian). No VarLong — verified against the Java.
#include <cstdint>
#include "../../PacketBuffer.h"

namespace mc::net::protocol::common {

struct ClientboundKeepAlivePacket {
    int64_t id;

    // private void write(FriendlyByteBuf output) { output.writeLong(this.id); }
    void write(PacketBuffer& output) const { output.writeLong(this->id); }

    // private ClientboundKeepAlivePacket(FriendlyByteBuf input) { this.id = input.readLong(); }
    static ClientboundKeepAlivePacket read(PacketBuffer& input) {
        return ClientboundKeepAlivePacket{ input.readLong() };
    }

    // public long getId() { return this.id; }
    int64_t getId() const { return id; }
};

} // namespace mc::net::protocol::common
