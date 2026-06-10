// Ground truth for net.minecraft.network.protocol.game.ClientboundBlockChangedAckPacket.
//
// The packet is a record(int sequence). Its STREAM_CODEC is Packet.codec(write, new),
// where write/read are exactly:
//   write : FriendlyByteBuf.writeVarInt(this.sequence)
//   read  : input.readVarInt()
// (net.minecraft.network.protocol.game.ClientboundBlockChangedAckPacket lines 9-19).
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body, so the
// whole wire payload is a single VarInt (LEB128) of the signed int `sequence`.
//
// Row format (tab separated):
//   ENC <sequence-dec> <readableBytes-dec> <hex>     encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert sequence equality
// as a sanity check before emitting. The C++ pkt_block_changed_ack_parity rebuilds the
// packet from <sequence>, re-encodes via PacketBuffer.writeVarInt, and must match <hex>
// byte-for-byte (+ readableBytes); it also decodes <hex> via readVarInt and checks the
// recovered sequence.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundBlockChangedAckPacket;
import net.minecraft.server.Bootstrap;

public class PktBlockChangedAckParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundBlockChangedAckPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundBlockChangedAckPacket>)
                ClientboundBlockChangedAckPacket.STREAM_CODEC;

        // Finite/physical input battery for the VarInt `sequence`.
        // The sequence is an ever-increasing server ack counter (ServerboundUseItem etc.),
        // so the common values are small non-negatives; we also pin every LEB128 byte
        // boundary (1->2->3->4->5 bytes) and the int extremes (negatives encode as 5
        // bytes in LEB128 since writeVarInt does not zig-zag).
        int[] sequences = {
            0, 1, 2, 7, 42, 100,
            127, 128, 129,                 // 1->2 byte boundary
            255, 256,
            16383, 16384, 16385,           // 2->3 byte boundary
            2097151, 2097152, 2097153,     // 3->4 byte boundary
            268435455, 268435456, 268435457, // 4->5 byte boundary
            123456789,
            Integer.MAX_VALUE,             // 0x7fffffff -> 5 bytes
            -1, -2, -128, -16384, -2097152,
            Integer.MIN_VALUE              // 0x80000000 -> 5 bytes
        };

        for (int sequence : sequences) {
            // ENC: construct the real packet, encode through the real codec, dump bytes.
            ClientboundBlockChangedAckPacket pkt = new ClientboundBlockChangedAckPacket(sequence);
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(buf, pkt);

            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec and assert equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ClientboundBlockChangedAckPacket dec = CODEC.decode(rbuf);
            if (dec.sequence() != sequence) {
                throw new IllegalStateException(
                    "round-trip mismatch: in=" + sequence + " out=" + dec.sequence());
            }

            O.print("ENC\t");
            O.print(sequence);
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
