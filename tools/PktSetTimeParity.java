// Ground truth for net.minecraft.network.protocol.game.ClientboundSetTimePacket.
//
// Encodes the packet through the REAL ClientboundSetTimePacket.STREAM_CODEC into a
// RegistryFriendlyByteBuf and dumps the exact wire bytes. The WORLD_CLOCK registry
// (overworld id 0, the_end id 1 — registration order) is built exactly like
// net.minecraft.world.clock.WorldClocks.bootstrap so the holderRegistry key codec
// resolves real ids.
//
// Row format (tab-separated), TAG = PKT:
//   PKT <gameTime-dec> <nEntries> <entries> <hex>
// where <entries> is a space-separated list (one per clock update, in WIRE order)
//   <regId>:<totalTicks-dec>:<partialTickBits-08x>:<rateBits-08x>
// and "-" when nEntries == 0. <hex> is the full packet payload, lowercase hex.
//
// The C++ pkt_set_time_parity rebuilds the packet from these fields, re-encodes via
// PacketBuffer, and must match <hex> byte-for-byte; it also decodes <hex> and checks
// the recovered fields.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import net.minecraft.SharedConstants;
import net.minecraft.core.Holder;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundSetTimePacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.clock.ClockNetworkState;
import net.minecraft.world.clock.WorldClock;
import net.minecraft.world.clock.WorldClocks;

import com.mojang.serialization.Lifecycle;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktSetTimeParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Build the WORLD_CLOCK registry exactly as WorldClocks.bootstrap:
        //   register(OVERWORLD) -> id 0, register(THE_END) -> id 1.
        MappedRegistry<WorldClock> reg =
            new MappedRegistry<>(net.minecraft.core.registries.Registries.WORLD_CLOCK, Lifecycle.stable());
        Holder.Reference<WorldClock> overworld = Registry.registerForHolder(reg, WorldClocks.OVERWORLD, new WorldClock());
        Holder.Reference<WorldClock> theEnd    = Registry.registerForHolder(reg, WorldClocks.THE_END,   new WorldClock());
        reg.freeze();

        RegistryAccess registryAccess = new RegistryAccess.ImmutableRegistryAccess(List.of(reg)).freeze();

        Holder<WorldClock>[] holders = new Holder[] { overworld, theEnd };

        // Finite/physical input battery.
        // gameTime values to exercise (incl. negatives, varlong boundaries downstream).
        long[] gameTimes = {
            0L, 1L, 24000L, 13000L, -1L, 6000L, 123456789L,
            Long.MAX_VALUE, Long.MIN_VALUE, -123456789012345L
        };

        // totalTicks values (VAR_LONG) — boundary coverage for LEB128.
        long[] ticks = { 0L, 1L, 127L, 128L, 16383L, 16384L, 2147483647L, -1L, Long.MAX_VALUE, Long.MIN_VALUE };
        // partialTick / rate float values.
        float[] partials = { 0.0f, 0.5f, 1.0f, 0.25f, -0.5f, 0.123456f };
        float[] rates     = { 1.0f, 0.0f, 0.5f, 2.0f, -1.0f, 60.0f };

        // (A) empty map across all gameTimes (the common wire case).
        for (long gt : gameTimes) {
            emit(registryAccess, gt, new LinkedHashMap<>());
        }

        // (B) single-entry map: each holder, sweeping state values.
        for (int h = 0; h < holders.length; h++) {
            for (int i = 0; i < ticks.length; i++) {
                Map<Holder<WorldClock>, ClockNetworkState> m = new LinkedHashMap<>();
                m.put(holders[h], new ClockNetworkState(ticks[i], partials[i % partials.length], rates[i % rates.length]));
                emit(registryAccess, 24000L * (i + 1), m);
            }
        }

        // (C) two-entry map (both holders) — exercises map-size VarInt + ordering.
        {
            Map<Holder<WorldClock>, ClockNetworkState> m = new LinkedHashMap<>();
            m.put(overworld, new ClockNetworkState(123L, 0.75f, 1.0f));
            m.put(theEnd,    new ClockNetworkState(456L, 0.25f, 0.5f));
            emit(registryAccess, 13000L, m);
        }
        {
            Map<Holder<WorldClock>, ClockNetworkState> m = new LinkedHashMap<>();
            m.put(theEnd,    new ClockNetworkState(Long.MAX_VALUE, -0.5f, 60.0f));
            m.put(overworld, new ClockNetworkState(Long.MIN_VALUE,  0.0f, -1.0f));
            emit(registryAccess, -1L, m);
        }
    }

    static void emit(RegistryAccess registryAccess, long gameTime,
                     Map<Holder<WorldClock>, ClockNetworkState> clockUpdates) {
        ClientboundSetTimePacket pkt = new ClientboundSetTimePacket(gameTime, clockUpdates);
        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), registryAccess);
        ClientboundSetTimePacket.STREAM_CODEC.encode(buf, pkt);

        // full hex
        StringBuilder hex = new StringBuilder();
        int n = buf.readableBytes();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Recover the EXACT wire order of clock-update entries by parsing the raw
        // bytes (HashMap iteration order at encode time is internal and not otherwise
        // observable).
        String orderedEntries = parseWireEntries(buf);

        O.print("PKT\t");
        O.print(gameTime);
        O.print('\t');
        O.print(clockUpdates.size());
        O.print('\t');
        O.print(orderedEntries.isEmpty() ? "-" : orderedEntries);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }

    // Parse the encoded buffer's bytes directly to recover the EXACT wire order of
    // clock-update entries (independent of HashMap iteration order).
    static String parseWireEntries(ByteBuf src) {
        ByteBuf b = Unpooled.copiedBuffer(src); // fresh reader index
        b.readLong(); // gameTime
        int count = net.minecraft.network.VarInt.read(b);
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < count; i++) {
            int id = net.minecraft.network.VarInt.read(b);          // holderRegistry key id
            long totalTicks = net.minecraft.network.VarLong.read(b); // VAR_LONG
            int partialBits = b.readInt();                           // FLOAT (raw bits, big-endian)
            int rateBits = b.readInt();                              // FLOAT
            if (i > 0) sb.append(' ');
            sb.append(id).append(':')
              .append(totalTicks).append(':')
              .append(String.format("%08x", partialBits)).append(':')
              .append(String.format("%08x", rateBits));
        }
        return sb.toString();
    }
}
