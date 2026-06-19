// Ground truth for net.minecraft.network.protocol.game.ClientboundTakeItemEntityPacket.
//
// The packet carries three ints (itemId, playerId, amount). Its STREAM_CODEC is
// Packet.codec(ClientboundTakeItemEntityPacket::write, ClientboundTakeItemEntityPacket::new),
// where write/read are exactly (net.minecraft...ClientboundTakeItemEntityPacket lines 22-32):
//   write : output.writeVarInt(this.itemId)
//           output.writeVarInt(this.playerId)
//           output.writeVarInt(this.amount)
//   read  : input.readVarInt(); input.readVarInt(); input.readVarInt();
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body, so the whole
// wire payload is exactly three VarInts (LEB128) of the signed ints, in that order.
//
// Row format (tab separated):
//   ENC <itemId-dec> <playerId-dec> <amount-dec> <readableBytes-dec> <hex>
//
// We round-trip-decode every case through the SAME codec and assert all three fields are
// equal as a sanity check before emitting. The C++ pkt_take_item_entity_parity rebuilds the
// fields from the columns, re-encodes via PacketBuffer.writeVarInt (in this order), and must
// match <hex> byte-for-byte (+ readableBytes); it also decodes <hex> via readVarInt thrice
// and checks the recovered fields.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundTakeItemEntityPacket;
import net.minecraft.server.Bootstrap;

public class PktTakeItemEntityParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundTakeItemEntityPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundTakeItemEntityPacket>)
                ClientboundTakeItemEntityPacket.STREAM_CODEC;

        // Finite/physical input battery for the three VarInt fields.
        // itemId/playerId are entity network ids (small non-negatives in practice); amount is
        // the pickup count. We still pin every LEB128 byte boundary (1->2->3->4->5 bytes) and
        // the int extremes per field independently, since writeVarInt does NOT zig-zag
        // (negatives encode as 5 bytes). Each row varies fields independently around the same
        // boundaries so per-field ordering is exercised distinctly.
        int[][] cases = {
            // {itemId, playerId, amount}
            {0, 0, 0},
            {1, 2, 3},
            {7, 42, 1},
            {100, 200, 64},
            {127, 128, 129},                 // 1->2 byte boundary, each field different
            {128, 127, 255},
            {255, 256, 257},
            {16383, 16384, 16385},           // 2->3 byte boundary
            {16384, 16383, 2},
            {2097151, 2097152, 2097153},     // 3->4 byte boundary
            {2097152, 1, 2097151},
            {268435455, 268435456, 268435457}, // 4->5 byte boundary
            {268435456, 5, 268435455},
            {123456789, 987654321, 55},
            {Integer.MAX_VALUE, Integer.MAX_VALUE, Integer.MAX_VALUE}, // 0x7fffffff -> 5 bytes
            {-1, -2, -128},                  // negatives -> 5 bytes (no zig-zag)
            {-16384, -2097152, -1},
            {Integer.MIN_VALUE, 0, Integer.MAX_VALUE}, // 0x80000000 -> 5 bytes
            {0, Integer.MIN_VALUE, 1},
            {Integer.MAX_VALUE, -1, Integer.MIN_VALUE},
        };

        for (int[] c : cases) {
            int itemId = c[0], playerId = c[1], amount = c[2];

            // ENC: construct the real packet, encode through the real codec, dump bytes.
            ClientboundTakeItemEntityPacket pkt =
                new ClientboundTakeItemEntityPacket(itemId, playerId, amount);
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(buf, pkt);

            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec and assert equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ClientboundTakeItemEntityPacket dec = CODEC.decode(rbuf);
            if (dec.getItemId() != itemId || dec.getPlayerId() != playerId
                || dec.getAmount() != amount) {
                throw new IllegalStateException(
                    "round-trip mismatch: in=(" + itemId + "," + playerId + "," + amount
                    + ") out=(" + dec.getItemId() + "," + dec.getPlayerId() + ","
                    + dec.getAmount() + ")");
            }

            O.print("ENC\t");
            O.print(itemId);
            O.print('\t');
            O.print(playerId);
            O.print('\t');
            O.print(amount);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
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
