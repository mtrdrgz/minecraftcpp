// Ground truth for ClientboundDebugSamplePacket's StreamCodec.
//
// The packet is a record (long[] sample, RemoteDebugSampleType debugSampleType).
// Its body is exactly:
//   write : output.writeLongArray(this.sample);   output.writeEnum(this.debugSampleType);
//   read  : this(input.readLongArray(), input.readEnum(RemoteDebugSampleType.class));
// (net.minecraft.network.protocol.game.ClientboundDebugSamplePacket lines 14-21.)
//
// FriendlyByteBuf.writeLongArray(longs) = VarInt.write(longs.length) followed by
//   each long via output.writeLong(l)  -> a plain BIG-ENDIAN 8-byte long (NOT VarLong).
//   (FriendlyByteBuf.writeLongArray/writeFixedSizeLongArray lines 343-357.)
// FriendlyByteBuf.writeEnum(value) = writeVarInt(value.ordinal()).
//   (FriendlyByteBuf.writeEnum line 471-473.)
// readLongArray = VarInt count + count * readLong (BE8); readEnum = enumConstants[readVarInt()].
//
// RemoteDebugSampleType has exactly one constant: TICK_TIME (ordinal 0). We still
// enumerate RemoteDebugSampleType.values() so every real ordinal is exercised honestly.
//
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body.
// Everything decomposes to primitives mc::net::PacketBuffer supports
// (writeVarInt LEB128, writeLong BE8), so this is a faithful byte-exact gate.
//
// Row format (tab separated):
//   ENC <name> <count> <l0,l1,...|-> <enumOrdinal> <readableBytes> <hexBytes>
//     the long list column is a comma-joined DECIMAL list (signed 64-bit), or "-" empty.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundDebugSamplePacket;
import net.minecraft.util.debugchart.RemoteDebugSampleType;

public class PktDebugSampleParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundDebugSamplePacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundDebugSamplePacket>)
                ClientboundDebugSamplePacket.STREAM_CODEC;

        // Each long is written verbatim as a BE 8-byte value, so pin sign / zero /
        // every byte-pattern extreme. (No VarLong here, so these stress the 8 raw bytes.)
        long[] L = {
            0L, 1L, 2L, -1L, -2L, 127L, 128L, 255L, 256L,
            32767L, 32768L, 65535L, 65536L,
            2147483647L, 2147483648L, 4294967295L, 4294967296L,
            Long.MAX_VALUE, Long.MIN_VALUE,
            0x0123456789abcdefL, 0xfedcba9876543210L,
            0x00000000000000ffL, 0xff00000000000000L
        };

        // Iterate over EVERY real enum constant (only TICK_TIME today, ordinal 0).
        RemoteDebugSampleType[] TYPES = RemoteDebugSampleType.values();

        for (RemoteDebugSampleType t : TYPES) {
            // --- empty sample (count = 0) ---
            emit("empty_" + t.name(), new long[] {}, t, CODEC);

            // --- singletons: each boundary long alone (count = 1) ---
            for (long v : L) {
                emit("one_" + v + "_" + t.name(), new long[] { v }, t, CODEC);
            }

            // --- realistic tick-time style runs ---
            emit("run_small_" + t.name(), new long[] { 1L, 2L, 3L, 4L, 5L }, t, CODEC);
            emit("run_ticks_" + t.name(),
                new long[] { 50000L, 1000000L, 16666666L, 50000000L }, t, CODEC);

            // --- all boundary longs in one sample (count multi-byte too) ---
            emit("mixed_all_" + t.name(), L, t, CODEC);

            // --- sample SIZE crossing VarInt byte boundaries: 127 and 128 elements
            //     force the count prefix to be 1 then 2 bytes. ---
            long[] c127 = new long[127];
            for (int i = 0; i < c127.length; i++) c127[i] = (long) i;
            emit("count127_" + t.name(), c127, t, CODEC);

            long[] c128 = new long[128];
            for (int i = 0; i < c128.length; i++) c128[i] = (long) i;
            emit("count128_" + t.name(), c128, t, CODEC);

            long[] c200 = new long[200];
            for (int i = 0; i < c200.length; i++) c200[i] = (long) i * 0x0101010101010101L - 13L;
            emit("count200_" + t.name(), c200, t, CODEC);
        }
    }

    static void emit(String name, long[] sample, RemoteDebugSampleType type,
                     StreamCodec<FriendlyByteBuf, ClientboundDebugSamplePacket> codec) {
        ClientboundDebugSamplePacket pkt = new ClientboundDebugSamplePacket(sample, type);

        // ENC: encode through the real codec, dump readableBytes + body bytes.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        codec.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Round-trip decode through the SAME codec (sanity).
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundDebugSamplePacket dec = codec.decode(rbuf);
        long[] back = dec.sample();
        if (back.length != sample.length)
            throw new IllegalStateException(name + ": round-trip size " + back.length + " != " + sample.length);
        for (int i = 0; i < sample.length; i++)
            if (back[i] != sample[i])
                throw new IllegalStateException(name + ": round-trip long[" + i + "] " + back[i] + " != " + sample[i]);
        if (dec.debugSampleType() != type)
            throw new IllegalStateException(name + ": round-trip type " + dec.debugSampleType() + " != " + type);

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(sample.length);
        O.print('\t');
        O.print(joinLongs(sample));
        O.print('\t');
        O.print(type.ordinal());
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');
    }

    static String joinLongs(long[] longs) {
        if (longs.length == 0) return "-";
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < longs.length; i++) {
            if (i > 0) sb.append(',');
            sb.append(longs[i]); // signed decimal of a 64-bit long
        }
        return sb.toString();
    }

    static String toHex(FriendlyByteBuf b) {
        StringBuilder sb = new StringBuilder();
        ByteBuf dup = b.duplicate();
        while (dup.isReadable()) sb.append(String.format("%02x", dup.readByte() & 0xff));
        return sb.toString();
    }

    static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
