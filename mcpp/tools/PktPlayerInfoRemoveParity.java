// Ground truth for ClientboundPlayerInfoRemovePacket's StreamCodec.
//
// The packet body is exactly FriendlyByteBuf.writeCollection(profileIds, UUIDUtil.STREAM_CODEC):
//   write : writeVarInt(profileIds.size()); for each UUID -> writeUUID(msb,lsb)
//   read  : readList(UUIDUtil.STREAM_CODEC) = readVarInt(count); loop count * readUUID
// (net.minecraft.network.protocol.game.ClientboundPlayerInfoRemovePacket lines 11-22,
//  FriendlyByteBuf.writeCollection/readList lines 134-144,
//  FriendlyByteBuf.writeUUID/readUUID lines 498-509 = writeLong(msb) then writeLong(lsb),
//  UUIDUtil.STREAM_CODEC lines 42-50 delegates to FriendlyByteBuf.{write,read}UUID.)
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body.
//
// Every field is a VarInt (LEB128, plain — no zig-zag) or a big-endian long, so the
// whole packet decomposes to primitives the C++ mc::net::PacketBuffer supports
// (writeVarInt + writeUUID(hi,lo) == two big-endian writeLong).
//
// Row format (tab separated):
//   ENC <name> <count> <msb0:lsb0;msb1:lsb1;...|-> <readableBytes> <hexBytes>
//     encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + bytes.
//     The uuid list column is a ';'-joined list of "msb:lsb" decimal-signed-long
//     pairs, or "-" when the list is empty.
// The C++ side reconstructs the UUID list, writes writeVarInt(count) + each
// writeUUID(msb,lsb) and must match byte-for-byte AND on readableBytes; it then
// decodes the expected bytes back and must round-trip the count + every uuid.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundPlayerInfoRemovePacket;

public class PktPlayerInfoRemoveParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundPlayerInfoRemovePacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundPlayerInfoRemovePacket>)
                ClientboundPlayerInfoRemovePacket.STREAM_CODEC;

        // Long boundary values for each 64-bit half of the UUID. writeLong is plain
        // big-endian (no zig-zag), so we pin sign, zero, all-ones, and the extremes.
        long[] L = {
            0L, 1L, -1L, 2L, -2L, 127L, 128L, 255L, 256L,
            0x00000000FFFFFFFFL, 0x0000000100000000L, 0x7FFFFFFFFFFFFFFFL,
            0x8000000000000000L, 0xFFFFFFFFFFFFFFFFL, 0x0123456789ABCDEFL,
            Long.MAX_VALUE, Long.MIN_VALUE
        };

        // --- empty list (count = 0) ---
        emit("empty", new UUID[] {}, CODEC);

        // --- singletons: nil + every (msb,lsb) boundary cross-product alone ---
        emit("one_nil", new UUID[] { new UUID(0L, 0L) }, CODEC);
        for (int a = 0; a < L.length; a++) {
            for (int b = 0; b < L.length; b++) {
                emit("one_" + a + "_" + b, new UUID[] { new UUID(L[a], L[b]) }, CODEC);
            }
        }

        // --- realistic random-looking UUID batches (fixed seeds for determinism) ---
        emit("run3", new UUID[] {
            new UUID(0x1111111111111111L, 0x2222222222222222L),
            new UUID(0x3333333333333333L, 0x4444444444444444L),
            new UUID(0x5555555555555555L, 0x6666666666666666L)
        }, CODEC);

        emit("run_named", new UUID[] {
            UUID.fromString("00000000-0000-0000-0000-000000000000"),
            UUID.fromString("ffffffff-ffff-ffff-ffff-ffffffffffff"),
            UUID.fromString("12345678-9abc-def0-1234-56789abcdef0"),
            UUID.fromString("deadbeef-cafe-babe-feed-0123456789ab")
        }, CODEC);

        // --- lists whose SIZE itself crosses VarInt byte boundaries: 127 / 128 / 200
        //     force the count prefix to be 1 then 2 bytes. ---
        emit("count127", seq(127), CODEC);
        emit("count128", seq(128), CODEC);
        emit("count200", seq(200), CODEC);
    }

    // Deterministic sequence of distinct UUIDs derived from the index.
    static UUID[] seq(int n) {
        UUID[] out = new UUID[n];
        for (int i = 0; i < n; i++) {
            long msb = ((long) i << 40) ^ (0x9E3779B97F4A7C15L * (i + 1));
            long lsb = (0xC2B2AE3D27D4EB4FL * (i + 1)) ^ ((long) i << 7);
            out[i] = new UUID(msb, lsb);
        }
        return out;
    }

    static void emit(String name, UUID[] uuids,
                     StreamCodec<FriendlyByteBuf, ClientboundPlayerInfoRemovePacket> codec) {
        List<UUID> list = new ArrayList<>(uuids.length);
        for (UUID u : uuids) list.add(u);
        ClientboundPlayerInfoRemovePacket pkt = new ClientboundPlayerInfoRemovePacket(list);

        // ENC: encode through the real codec, dump readableBytes + body bytes.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        codec.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Round-trip decode through the SAME codec (sanity).
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundPlayerInfoRemovePacket dec = codec.decode(rbuf);
        List<UUID> back = dec.profileIds();
        if (back.size() != uuids.length)
            throw new IllegalStateException(name + ": round-trip size " + back.size() + " != " + uuids.length);
        for (int i = 0; i < uuids.length; i++)
            if (!back.get(i).equals(uuids[i]))
                throw new IllegalStateException(name + ": round-trip uuid[" + i + "] " + back.get(i) + " != " + uuids[i]);

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(uuids.length);
        O.print('\t');
        O.print(joinUuids(uuids));
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');
    }

    static String joinUuids(UUID[] uuids) {
        if (uuids.length == 0) return "-";
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < uuids.length; i++) {
            if (i > 0) sb.append(';');
            sb.append(uuids[i].getMostSignificantBits());
            sb.append(':');
            sb.append(uuids[i].getLeastSignificantBits());
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
