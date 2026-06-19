#pragma once
// Port of net.minecraft.network.protocol.common.ClientboundPingPacket
//
// public class ClientboundPingPacket implements Packet<ClientCommonPacketListener> {
//    public static final StreamCodec<...> STREAM_CODEC =
//        Packet.codec(ClientboundPingPacket::write, ClientboundPingPacket::new);
//    private final int id;
//    private ClientboundPingPacket(FriendlyByteBuf input) { this.id = input.readInt(); }
//    private void write(FriendlyByteBuf output) { output.writeInt(this.id); }
//    public int getId() { return this.id; }
// }
//
// The body codec is exactly one fixed-width big-endian int (FriendlyByteBuf.writeInt
// delegates to netty ByteBuf.writeInt). Packet.codec adds no framing of its own.
#include <cstdint>
#include "../../PacketBuffer.h"

namespace mc::net::protocol::common {

struct ClientboundPingPacket {
    int32_t id;

    explicit ClientboundPingPacket(int32_t id_) : id(id_) {}

    int32_t getId() const { return id; }

    // STREAM_CODEC encode: output.writeInt(this.id)
    void write(PacketBuffer& output) const { output.writeInt(id); }

    // STREAM_CODEC decode: this.id = input.readInt()
    static ClientboundPingPacket read(PacketBuffer& input) {
        return ClientboundPingPacket(input.readInt());
    }
};

} // namespace mc::net::protocol::common
