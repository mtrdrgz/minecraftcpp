#pragma once
#include "../PacketBuffer.h"

namespace mc::net {

struct Packet {
    virtual ~Packet() = default;
    virtual void write(PacketBuffer& buf) const = 0;
    virtual int32_t packetId() const = 0;
};

// Helper: write a full sendable packet (id + payload) into buf
inline void encodePacket(const Packet& p, PacketBuffer& out) {
    out.writeVarInt(p.packetId());
    p.write(out);
}

} // namespace mc::net
