// Ground truth for net.minecraft.network.protocol.game.ServerboundAttackPacket.
//
// The packet is `public record ServerboundAttackPacket(int entityId)`. Its STREAM_CODEC is
//   StreamCodec.composite(ByteBufCodecs.VAR_INT, ServerboundAttackPacket::entityId,
//                         ServerboundAttackPacket::new)
// (net.minecraft.network.protocol.game.ServerboundAttackPacket lines 9-22).
// ByteBufCodecs.VAR_INT is exactly VarInt.write / VarInt.read (LEB128, signed, NO zig-zag),
// and composite over a single field writes only that field — so the whole wire payload is a
// single VarInt: entityId. There is NO packet-id prefix (the type id is framed elsewhere).
//
// `entityId` is the attacked entity's network id. The decode ctor performs no validation,
// so every int is legal; we still pin every LEB128 byte boundary (1->2->3->4->5 bytes) and
// the signed extremes (negatives encode as 5 bytes since VAR_INT does not zig-zag).
//
// NOTE the codec is over a plain ByteBuf (not RegistryFriendlyByteBuf), but we still encode
// through a FriendlyByteBuf instance (which IS a ByteBuf) to mirror the real send path and
// keep toHex/round-trip identical to the other pkt_* ground truths.
//
// Row format (tab separated):
//   ENC <entityId-dec> <readableBytes-dec> <hex>
//        encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert the int equals as a
// sanity check before emitting. The C++ pkt_attack_parity rebuilds the packet from
// <entityId>, re-encodes via PacketBuffer.writeVarInt, and must match <hex> byte-for-byte
// (+ readableBytes); it also decodes <hex> via readVarInt and checks the recovered int.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundAttackPacket;
import net.minecraft.server.Bootstrap;

public class PktAttackParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet. It is declared over ByteBuf; FriendlyByteBuf
        // is a ByteBuf, so this assignment / encode path is exactly the real send path.
        StreamCodec<ByteBuf, ServerboundAttackPacket> CODEC =
            (StreamCodec<ByteBuf, ServerboundAttackPacket>)
                (StreamCodec<?, ServerboundAttackPacket>) ServerboundAttackPacket.STREAM_CODEC;

        // `entityId` is the attacked entity's network id; pin every LEB128 byte boundary
        // (1->2->3->4->5 bytes) and the int extremes (negatives encode as 5 bytes in
        // LEB128 since VAR_INT does not zig-zag).
        int[] ids = {
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

        for (int id : ids) {
            emit(CODEC, id);
        }
    }

    static void emit(StreamCodec<ByteBuf, ServerboundAttackPacket> CODEC,
                     int entityId) throws Exception {
        // ENC: construct the real packet, encode through the real codec, dump bytes.
        ServerboundAttackPacket pkt = new ServerboundAttackPacket(entityId);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec and assert equality.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundAttackPacket dec = CODEC.decode(rbuf);
        if (dec.entityId() != entityId) {
            throw new IllegalStateException(
                "round-trip mismatch: in=" + entityId + " out=" + dec.entityId());
        }

        O.print("ENC\t");
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
