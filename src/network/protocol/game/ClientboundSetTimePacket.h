#pragma once
// Port of net.minecraft.network.protocol.game.ClientboundSetTimePacket (MC 26.1.2)
//
//   public record ClientboundSetTimePacket(long gameTime,
//                                          Map<Holder<WorldClock>, ClockNetworkState> clockUpdates)
//
//   STREAM_CODEC = StreamCodec.composite(
//      ByteBufCodecs.LONG,                                                   -> gameTime
//      ClientboundSetTimePacket::gameTime,
//      ByteBufCodecs.map(HashMap::new, WorldClock.STREAM_CODEC,             -> clockUpdates
//                                      ClockNetworkState.STREAM_CODEC),
//      ClientboundSetTimePacket::clockUpdates,
//      ClientboundSetTimePacket::new)
//
// Wire layout (RegistryFriendlyByteBuf), exactly per ByteBufCodecs:
//   ByteBufCodecs.LONG.encode      -> output.writeLong(gameTime)        (8 bytes big-endian)
//   ByteBufCodecs.map.encode       -> writeCount(map.size())            (VarInt)
//     for each (key,value):
//       WorldClock.STREAM_CODEC    = ByteBufCodecs.holderRegistry(Registries.WORLD_CLOCK)
//                                    -> registry(...).encode:
//                                       int id = registry.asHolderIdMap().getIdOrThrow(holder);
//                                       VarInt.write(output, id)         (VarInt, NO +1 — that is `holder()`, not `holderRegistry()`)
//       ClockNetworkState.STREAM_CODEC = StreamCodec.composite(
//                                       ByteBufCodecs.VAR_LONG, ::totalTicks,   -> VarLong totalTicks
//                                       ByteBufCodecs.FLOAT,    ::partialTick,  -> float  partialTick (4 bytes BE)
//                                       ByteBufCodecs.FLOAT,    ::rate,         -> float  rate        (4 bytes BE)
//                                       ClockNetworkState::new)
//
// The only registry-coupled value is the holder id (a VarInt). Because the
// WORLD_CLOCK registry is not yet ported, the map KEY is represented here as the
// already-resolved registry id (int32). The ground-truth tool resolves the id via
// the real net.minecraft registry; this port reproduces the byte layout 1:1.
#include "../../PacketBuffer.h"
#include <cstdint>
#include <vector>

namespace mc::net {

// Port of net.minecraft.world.clock.ClockNetworkState
//   record ClockNetworkState(long totalTicks, float partialTick, float rate)
//   STREAM_CODEC: VAR_LONG totalTicks, FLOAT partialTick, FLOAT rate
struct ClockNetworkState {
    int64_t totalTicks = 0;
    float   partialTick = 0.0f;
    float   rate = 0.0f;

    void encode(PacketBuffer& buf) const {
        buf.writeVarLong(totalTicks);   // ByteBufCodecs.VAR_LONG
        buf.writeFloat(partialTick);    // ByteBufCodecs.FLOAT
        buf.writeFloat(rate);           // ByteBufCodecs.FLOAT
    }

    static ClockNetworkState decode(PacketBuffer& buf) {
        ClockNetworkState s;
        s.totalTicks  = buf.readVarLong();
        s.partialTick = buf.readFloat();
        s.rate        = buf.readFloat();
        return s;
    }
};

// One resolved clock-update entry: the holder's registry id (VarInt key) + its state.
struct ClockUpdateEntry {
    int32_t           registryId = 0; // holderRegistry id (no +1)
    ClockNetworkState state{};
};

// S2C 0x?? Set Time
struct ClientboundSetTimePacket {
    int64_t gameTime = 0;
    // clockUpdates, in encode iteration order (matches the Java map's iteration order
    // for the ground-truth bytes). Decode preserves wire order.
    std::vector<ClockUpdateEntry> clockUpdates;

    // STREAM_CODEC.encode — byte-exact mirror of the composite codec.
    void encode(PacketBuffer& buf) const {
        // ByteBufCodecs.LONG
        buf.writeLong(gameTime);
        // ByteBufCodecs.map: writeCount(map.size(), Integer.MAX_VALUE) -> VarInt size
        buf.writeVarInt(static_cast<int32_t>(clockUpdates.size()));
        for (const ClockUpdateEntry& e : clockUpdates) {
            // WorldClock.STREAM_CODEC = holderRegistry: VarInt of the registry id
            buf.writeVarInt(e.registryId);
            // ClockNetworkState.STREAM_CODEC
            e.state.encode(buf);
        }
    }

    // STREAM_CODEC.decode — reads the same layout back.
    static ClientboundSetTimePacket decode(PacketBuffer& buf) {
        ClientboundSetTimePacket p;
        p.gameTime = buf.readLong();
        int32_t count = buf.readVarInt(); // readCount (Integer.MAX_VALUE max)
        // ByteBufCodecs.map decode caps the initial allocation at min(count, 65536).
        p.clockUpdates.reserve(static_cast<size_t>(count < 0 ? 0 : (count < 65536 ? count : 65536)));
        for (int32_t i = 0; i < count; ++i) {
            ClockUpdateEntry e;
            e.registryId = buf.readVarInt();
            e.state = ClockNetworkState::decode(buf);
            p.clockUpdates.push_back(e);
        }
        return p;
    }
};

} // namespace mc::net
