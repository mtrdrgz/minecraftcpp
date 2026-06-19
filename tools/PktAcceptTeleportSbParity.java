// Ground truth for net.minecraft.network.protocol.game.ServerboundAcceptTeleportationPacket.
//
// The packet carries a single int `id`. Its STREAM_CODEC is Packet.codec(write, new),
// where write/read are exactly:
//   write : FriendlyByteBuf.writeVarInt(this.id)
//   read  : input.readVarInt()
// (net.minecraft.network.protocol.game.ServerboundAcceptTeleportationPacket lines 9-24).
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body, so the whole
// wire payload is a single VarInt (LEB128) of the signed int `id`.
//
// Row format (tab separated):
//   ENC <id-dec> <readableBytes-dec> <hex>     encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert id equality as a
// sanity check before emitting. The C++ pkt_accept_teleport_sb_parity rebuilds the
// packet from <id>, re-encodes via PacketBuffer.writeVarInt, and must match <hex>
// byte-for-byte (+ readableBytes); it also decodes <hex> via readVarInt and checks the
// recovered id.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundAcceptTeleportationPacket;
import net.minecraft.server.Bootstrap;

public class PktAcceptTeleportSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundAcceptTeleportationPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundAcceptTeleportationPacket>)
                ServerboundAcceptTeleportationPacket.STREAM_CODEC;

        // Finite/physical input battery for the VarInt `id` (the server-assigned
        // teleport sequence the client echoes back). It is a small non-negative counter
        // in practice; we also pin every LEB128 byte boundary (1->2->3->4->5 bytes) and
        // the int extremes (negatives encode as 5 bytes since writeVarInt does not
        // zig-zag).
        int[] ids = {
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

        for (int id : ids) {
            // ENC: construct the real packet (public ctor), encode through the real
            // codec, dump bytes.
            ServerboundAcceptTeleportationPacket pkt = new ServerboundAcceptTeleportationPacket(id);
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(buf, pkt);

            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec and assert equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundAcceptTeleportationPacket dec = CODEC.decode(rbuf);
            if (dec.getId() != id) {
                throw new IllegalStateException(
                    "round-trip mismatch: in=" + id + " out=" + dec.getId());
            }

            O.print("ENC\t");
            O.print(id);
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
