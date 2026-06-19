// Ground truth for net.minecraft.network.protocol.game.ServerboundSelectTradePacket.
//
// The packet holds a single `private final int item;`. Its STREAM_CODEC is
// Packet.codec(ServerboundSelectTradePacket::write, ServerboundSelectTradePacket::new),
// where write/read are exactly:
//   write : output.writeVarInt(this.item);
//   read  : this.item = input.readVarInt();
// (net.minecraft.network.protocol.game.ServerboundSelectTradePacket lines 9-24).
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body, so the whole
// wire payload is a single VarInt (LEB128, signed, no zig-zag): item.
//
// `item` is the selected merchant recipe index. There is no validation in the decode
// ctor, so every int value is legal; we still pin every LEB128 byte boundary and the
// signed extremes (negatives encode as 5 bytes since writeVarInt does not zig-zag).
//
// Row format (tab separated):
//   ENC <item-dec> <readableBytes-dec> <hex>
//        encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert the int equals as a
// sanity check before emitting. The C++ pkt_select_trade_sb_parity rebuilds the packet
// from <item>, re-encodes via PacketBuffer.writeVarInt, and must match <hex> byte-for-byte
// (+ readableBytes); it also decodes <hex> via readVarInt and checks the recovered int.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundSelectTradePacket;
import net.minecraft.server.Bootstrap;

public class PktSelectTradeSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundSelectTradePacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundSelectTradePacket>)
                ServerboundSelectTradePacket.STREAM_CODEC;

        // `item` is a merchant recipe index; pin every LEB128 byte boundary
        // (1->2->3->4->5 bytes) and the int extremes (negatives encode as 5 bytes in
        // LEB128 since writeVarInt does not zig-zag).
        int[] items = {
            0, 1, 2, 7, 63, 100,
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

        for (int item : items) {
            emit(CODEC, item);
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundSelectTradePacket> CODEC,
                     int item) throws Exception {
        // ENC: construct the real packet, encode through the real codec, dump bytes.
        ServerboundSelectTradePacket pkt = new ServerboundSelectTradePacket(item);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec and assert equality.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundSelectTradePacket dec = CODEC.decode(rbuf);
        if (dec.getItem() != item) {
            throw new IllegalStateException(
                "round-trip mismatch: in=" + item + " out=" + dec.getItem());
        }

        O.print("ENC\t");
        O.print(item);
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
