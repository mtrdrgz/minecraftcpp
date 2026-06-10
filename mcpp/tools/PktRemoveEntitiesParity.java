// Ground truth for ClientboundRemoveEntitiesPacket's StreamCodec.
//
// The packet body is exactly FriendlyByteBuf.writeIntIdList(entityIds):
//   write : writeVarInt(ids.size()); ids.forEach(this::writeVarInt);
//   read  : int count = readVarInt(); loop count * readVarInt();
// (net.minecraft.network.protocol.game.ClientboundRemoveEntitiesPacket lines 24-30,
//  FriendlyByteBuf.readIntIdList/writeIntIdList lines 146-160.)
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body.
//
// Every field is a VarInt (LEB128), so the whole packet decomposes to primitives
// the C++ mc::net::PacketBuffer supports (writeVarInt / readVarInt).
//
// Row format (tab separated):
//   ENC <name> <count> <id0,id1,...|-> <readableBytes> <hexBytes>
//     encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + bytes.
//     The id list column is a comma-joined decimal list, or "-" when empty.
// The C++ side reconstructs the IntList, writes writeVarInt(count) + each
// writeVarInt(id) and must match byte-for-byte AND on readableBytes; it then
// decodes the expected bytes back and must round-trip the count + every id.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import it.unimi.dsi.fastutil.ints.IntArrayList;
import it.unimi.dsi.fastutil.ints.IntList;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundRemoveEntitiesPacket;

public class PktRemoveEntitiesParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundRemoveEntitiesPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundRemoveEntitiesPacket>)
                ClientboundRemoveEntitiesPacket.STREAM_CODEC;

        // VarInt byte-boundary values (zig-zag is NOT used here — plain VarInt of the
        // raw int, so negatives encode to 5 bytes). Real entity ids are small
        // non-negative ints, but the codec passes them verbatim through writeVarInt,
        // so we pin the full VarInt 1->2->3->4->5 byte boundaries + sign + extremes.
        int[] V = {
            0, 1, 2, 127, 128, 129, 255, 256, 16383, 16384, 16385,
            2097151, 2097152, 2097153, 268435455, 268435456, 268435457,
            -1, -2, -128, -2097152, Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        // --- empty list (count = 0) ---
        emit("empty", new int[] {}, CODEC);

        // --- singletons: each boundary value alone (count = 1) ---
        for (int v : V) {
            emit("one_" + v, new int[] { v }, CODEC);
        }

        // --- typical small id runs (realistic entity-id batches) ---
        emit("run_small", new int[] { 1, 2, 3, 4, 5 }, CODEC);
        emit("run_ids", new int[] { 42, 100, 1000, 32767, 65536 }, CODEC);
        emit("run_seq", new int[] { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 }, CODEC);

        // --- mixed boundary + sign + extremes in one list (count multi-byte too) ---
        emit("mixed_all", V, CODEC);

        // --- a list whose SIZE itself crosses VarInt byte boundaries: 127 and 128
        //     elements force the count prefix to be 1 then 2 bytes. ---
        int[] c127 = new int[127];
        for (int i = 0; i < c127.length; i++) c127[i] = i;
        emit("count127", c127, CODEC);

        int[] c128 = new int[128];
        for (int i = 0; i < c128.length; i++) c128[i] = i;
        emit("count128", c128, CODEC);

        int[] c200 = new int[200];
        for (int i = 0; i < c200.length; i++) c200[i] = i * 7 - 13; // spread of signs/sizes
        emit("count200", c200, CODEC);
    }

    static void emit(String name, int[] ids,
                     StreamCodec<FriendlyByteBuf, ClientboundRemoveEntitiesPacket> codec) {
        IntList list = new IntArrayList(ids);
        ClientboundRemoveEntitiesPacket pkt = new ClientboundRemoveEntitiesPacket(list);

        // ENC: encode through the real codec, dump readableBytes + body bytes.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        codec.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Round-trip decode through the SAME codec (sanity).
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundRemoveEntitiesPacket dec = codec.decode(rbuf);
        IntList back = dec.getEntityIds();
        if (back.size() != ids.length)
            throw new IllegalStateException(name + ": round-trip size " + back.size() + " != " + ids.length);
        for (int i = 0; i < ids.length; i++)
            if (back.getInt(i) != ids[i])
                throw new IllegalStateException(name + ": round-trip id[" + i + "] " + back.getInt(i) + " != " + ids[i]);

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(ids.length);
        O.print('\t');
        O.print(joinIds(ids));
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');
    }

    static String joinIds(int[] ids) {
        if (ids.length == 0) return "-";
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < ids.length; i++) {
            if (i > 0) sb.append(',');
            sb.append(ids[i]);
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
