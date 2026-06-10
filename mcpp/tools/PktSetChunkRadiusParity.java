// Ground truth for net.minecraft.network.protocol.game.ClientboundSetChunkCacheRadiusPacket.
//
// The packet holds a single `private final int radius`. Its STREAM_CODEC is
// Packet.codec(ClientboundSetChunkCacheRadiusPacket::write, ::new), where write/read are
// exactly:
//   write : FriendlyByteBuf.writeVarInt(this.radius)
//   read  : input.readVarInt()
// (net.minecraft.network.protocol.game.ClientboundSetChunkCacheRadiusPacket lines 9-24).
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body, so the whole
// wire payload is a single VarInt (LEB128) of the signed int `radius` (no zig-zag).
//
// Row format (tab separated):
//   ENC <radius-dec> <readableBytes-dec> <hex>     encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert radius equality as a
// sanity check before emitting. The C++ pkt_set_chunk_radius_parity rebuilds the packet
// from <radius>, re-encodes via PacketBuffer.writeVarInt, and must match <hex>
// byte-for-byte (+ readableBytes); it also decodes <hex> via readVarInt and checks the
// recovered radius.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetChunkCacheRadiusPacket;
import net.minecraft.server.Bootstrap;

public class PktSetChunkRadiusParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundSetChunkCacheRadiusPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundSetChunkCacheRadiusPacket>)
                ClientboundSetChunkCacheRadiusPacket.STREAM_CODEC;

        // Finite/physical input battery for the VarInt `radius`.
        // In vanilla the radius is the server view-distance + 1 (a small positive value,
        // typically 3..33), but the codec is a bare signed-int VarInt, so we pin every
        // LEB128 byte boundary (1->2->3->4->5 bytes) and the int extremes. Negatives encode
        // as 5 bytes in LEB128 since writeVarInt does not zig-zag.
        int[] radii = {
            0, 1, 2, 3, 8, 12, 32, 33, 42, 100,
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

        for (int radius : radii) {
            // ENC: construct the real packet, encode through the real codec, dump bytes.
            ClientboundSetChunkCacheRadiusPacket pkt =
                new ClientboundSetChunkCacheRadiusPacket(radius);
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(buf, pkt);

            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec and assert equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ClientboundSetChunkCacheRadiusPacket dec = CODEC.decode(rbuf);
            if (dec.getRadius() != radius) {
                throw new IllegalStateException(
                    "round-trip mismatch: in=" + radius + " out=" + dec.getRadius());
            }

            O.print("ENC\t");
            O.print(radius);
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
