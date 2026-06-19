// Ground truth for net.minecraft.network.protocol.game.ServerboundEntityTagQueryPacket.
//
// The packet carries two ints: `transactionId` and `entityId`. Its STREAM_CODEC is
// Packet.codec(write, new), where write/read are exactly (in this wire order)
// (net.minecraft.network.protocol.game.ServerboundEntityTagQueryPacket lines 20-28):
//   write : output.writeVarInt(this.transactionId)
//           output.writeVarInt(this.entityId)
//   read  : this.transactionId = input.readVarInt()
//           this.entityId       = input.readVarInt()
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body, so the whole
// wire payload is VarInt(transactionId) followed by VarInt(entityId), both LEB128 of the
// signed ints (writeVarInt does NOT zig-zag, so negatives encode as 5 bytes).
//
// Row format (tab separated):
//   ENC <transactionId-dec> <entityId-dec> <readableBytes-dec> <hex>
//        encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert field equality as a
// sanity check before emitting. The C++ pkt_entity_tag_query_sb_parity rebuilds the
// packet from <transactionId>/<entityId>, re-encodes via PacketBuffer.writeVarInt twice in
// order, and must match <hex> byte-for-byte (+ readableBytes); it also decodes <hex> via
// readVarInt twice and checks the recovered fields.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundEntityTagQueryPacket;
import net.minecraft.server.Bootstrap;

public class PktEntityTagQuerySbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundEntityTagQueryPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundEntityTagQueryPacket>)
                ServerboundEntityTagQueryPacket.STREAM_CODEC;

        // Finite/physical input battery for each VarInt. We pin every LEB128 byte boundary
        // (1->2->3->4->5 bytes), small physical counters/ids, and the int extremes
        // (negatives encode as 5 bytes since writeVarInt does not zig-zag). The two fields
        // are crossed so byte-boundary lengths combine in both positions.
        int[] vals = {
            0, 1, 2, 7, 42, 100,
            127, 128, 129,                   // 1->2 byte boundary
            255, 256,
            16383, 16384, 16385,             // 2->3 byte boundary
            2097151, 2097152, 2097153,       // 3->4 byte boundary
            268435455, 268435456, 268435457, // 4->5 byte boundary
            123456789,
            Integer.MAX_VALUE,               // 0x7fffffff -> 5 bytes
            -1, -2, -128, -16384, -2097152,
            Integer.MIN_VALUE                // 0x80000000 -> 5 bytes
        };

        // 1) Diagonal: same value in both fields (covers all boundaries paired with itself).
        for (int v : vals) {
            emit(CODEC, v, v);
        }
        // 2) Off-diagonal pairs that mix differing byte-lengths between the two fields, so
        //    the second VarInt starts at varying offsets. Each pair is a physical
        //    transactionId/entityId combination.
        int[][] pairs = {
            {0, 1}, {1, 0},
            {0, Integer.MAX_VALUE}, {Integer.MAX_VALUE, 0},
            {127, 128}, {128, 127},
            {16383, 16384}, {16384, 16383},
            {2097151, 2097152}, {2097152, 2097151},
            {268435455, 268435456}, {268435456, 268435455},
            {42, -1}, {-1, 42},
            {Integer.MIN_VALUE, Integer.MAX_VALUE}, {Integer.MAX_VALUE, Integer.MIN_VALUE},
            {-128, -16384}, {-2097152, -2},
            {123456789, 256}, {256, 123456789}
        };
        for (int[] p : pairs) {
            emit(CODEC, p[0], p[1]);
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundEntityTagQueryPacket> CODEC,
                     int transactionId, int entityId) throws Exception {
        // ENC: construct the real packet (public ctor), encode through the real codec,
        // dump bytes.
        ServerboundEntityTagQueryPacket pkt =
            new ServerboundEntityTagQueryPacket(transactionId, entityId);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec and assert equality.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundEntityTagQueryPacket dec = CODEC.decode(rbuf);
        if (dec.getTransactionId() != transactionId || dec.getEntityId() != entityId) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + transactionId + "," + entityId + ")"
                    + " out=(" + dec.getTransactionId() + "," + dec.getEntityId() + ")");
        }

        O.print("ENC\t");
        O.print(transactionId);
        O.print('\t');
        O.print(entityId);
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');
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
